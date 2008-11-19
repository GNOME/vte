/*
 * Copyright (C) 2003,2008 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "../config.h"

#include <sys/param.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include "debug.h"
#include "vtebg.h"
#include "vtedraw.h"
#include "vtepangocairo.h"
#include "vte-private.h"

#include <pango/pangocairo.h>
#include <glib/gi18n-lib.h>

#undef PROFILE_COVERAGE

#ifdef PROFILE_COVERAGE
static int coverage_count[4];
#endif

/* cairo_show_glyphs accepts runs up to 102 glyphs before it allocates a
 * temporary array.
 *
 * Setting this to a large value can cause dramatic slow-downs for some
 * xservers (notably fglrx), see bug #410534.
 *
 * Moreover, setting it larger than VTE_DRAW_MAX_LENGTH is nonsensical,
 * as the higher layers will not submit runs longer than that value.
 */
#define MAX_RUN_LENGTH 100


enum unichar_coverage {
	/* in increasing order of speed */
	COVERAGE_UNKNOWN = 0,		/* we don't know about the character yet */
	COVERAGE_USE_PANGO_LAYOUT_LINE,	/* use a PangoLayoutLine for the character */
	COVERAGE_USE_PANGO_GLYPH_STRING,	/* use a PangoGlyphString for the character */
	COVERAGE_USE_CAIRO_GLYPH	/* use a cairo_glyph_t for the character */
};

union unichar_font_info {
	/* COVERAGE_USE_PANGO_LAYOUT_LINE */
	struct {
		PangoLayoutLine *line;
	} using_pango_layout_line;
	/* COVERAGE_USE_PANGO_GLYPH_STRING */
	struct {
		PangoFont *font;
		PangoGlyphString *glyph_string;
	} using_pango_glyph_string;
	/* COVERAGE_USE_CAIRO_GLYPH */
	struct {
		cairo_scaled_font_t *scaled_font;
		unsigned int glyph_index;
	} using_cairo_glyph;
};

struct unichar_info {
	guchar coverage;
	guchar has_unknown_chars;
	guint16 width;
	union unichar_font_info ufi;
};

static struct unichar_info *
unichar_info_create (void)
{
	return g_slice_new0 (struct unichar_info);
}

static void
unichar_info_finish (struct unichar_info *uinfo)
{
	union unichar_font_info *ufi = &uinfo->ufi;

	switch (uinfo->coverage) {
	default:
	case COVERAGE_UNKNOWN:
		g_assert_not_reached ();
		break;
	case COVERAGE_USE_PANGO_LAYOUT_LINE:
		/* we hold a manual reference on layout */
		g_object_unref (ufi->using_pango_layout_line.line->layout);
		ufi->using_pango_layout_line.line->layout = NULL;
		pango_layout_line_unref (ufi->using_pango_layout_line.line);
		ufi->using_pango_layout_line.line = NULL;
		break;
	case COVERAGE_USE_PANGO_GLYPH_STRING:
		if (ufi->using_pango_glyph_string.font)
			g_object_unref (ufi->using_pango_glyph_string.font);
		ufi->using_pango_glyph_string.font = NULL;
		pango_glyph_string_free (ufi->using_pango_glyph_string.glyph_string);
		ufi->using_pango_glyph_string.glyph_string = NULL;
		break;
	case COVERAGE_USE_CAIRO_GLYPH:
		cairo_scaled_font_destroy (ufi->using_cairo_glyph.scaled_font);
		ufi->using_cairo_glyph.scaled_font = NULL;
		break;
	}
}

static void
unichar_info_destroy (struct unichar_info *uinfo)
{
	unichar_info_finish (uinfo);
	g_slice_free (struct unichar_info, uinfo);
}

struct font_info {
	PangoLayout *layout;

	PangoCoverage *coverage;

	struct unichar_info ascii_unichar_info[128];
	GHashTable *other_unichar_info;

	gint width, height, ascent;
};

static struct unichar_info *
font_info_find_unichar_info (struct font_info    *info,
			     gunichar             c)
{
	struct unichar_info *uinfo;

	if (G_LIKELY (c < G_N_ELEMENTS (info->ascii_unichar_info)))
		return &info->ascii_unichar_info[c];

	if (G_UNLIKELY (info->other_unichar_info == NULL))
		info->other_unichar_info = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) unichar_info_destroy);

	uinfo = g_hash_table_lookup (info->other_unichar_info, GINT_TO_POINTER (c));
	if (G_LIKELY (uinfo))
		return uinfo;

	uinfo = unichar_info_create ();
	g_hash_table_insert (info->other_unichar_info, GINT_TO_POINTER (c), uinfo);
	return uinfo;
}

static void
font_info_cache_ascii (struct font_info *info)
{
	PangoLayoutLine *line;
	PangoGlyphItemIter iter;
	PangoGlyphItem *glyph_item;
	PangoGlyphString *glyph_string;
	PangoFont *pango_font;
	cairo_scaled_font_t *scaled_font;
	const char *text;
	gboolean more;
	
	/* We have info->layout holding most ASCII characters.  We want to
	 * cache as much info as we can about the ASCII letters so we don't
	 * have to look them up again later */

	/* Don't cache if unknown glyphs found in layout */
	if (pango_layout_get_unknown_glyphs_count (info->layout) != 0)
		return;

	text = pango_layout_get_text (info->layout);

	line = pango_layout_get_line_readonly (info->layout, 0);

	/* Don't cache if more than one font used for the line */
	if (G_UNLIKELY (!line || !line->runs || line->runs->next))
		return;

	glyph_item = line->runs->data;
	glyph_string = glyph_item->glyphs;
	pango_font = glyph_item->item->analysis.font;
	if (!pango_font)
		return;
	scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *) pango_font);
	if (!scaled_font)
		return;

	for (more = pango_glyph_item_iter_init_start (&iter, glyph_item, text);
	     more;
	     more = pango_glyph_item_iter_next_cluster (&iter))
	{
		struct unichar_info *uinfo;
		union unichar_font_info *ufi;
	 	PangoGlyphGeometry *geometry;
		PangoGlyph glyph;
		gunichar c;

		/* Only cache simple clusters */
		if (iter.start_char +1 != iter.end_char  ||
		    iter.start_index+1 != iter.end_index ||
		    iter.start_glyph+1 != iter.end_glyph)
			continue;

		c = text[iter.start_index];
		glyph = glyph_string->glyphs[iter.start_glyph].glyph;
		geometry = &glyph_string->glyphs[iter.start_glyph].geometry;

		/* Only cache non-common characters as common characters get
		 * their font from their neighbors */
		if (pango_script_for_unichar (c) <= PANGO_SCRIPT_INHERITED)
			continue;

		/* Only cache simple glyphs */
		if (!(glyph <= 0xFFFF) || (geometry->x_offset | geometry->y_offset) != 0)
			continue;

		uinfo = font_info_find_unichar_info (info, c);
		if (G_UNLIKELY (uinfo->coverage != COVERAGE_UNKNOWN))
			continue;

		ufi = &uinfo->ufi;

		uinfo->width = PANGO_PIXELS_CEIL (geometry->width);
		uinfo->has_unknown_chars = FALSE;

		uinfo->coverage = COVERAGE_USE_CAIRO_GLYPH;

		ufi->using_cairo_glyph.scaled_font = cairo_scaled_font_reference (scaled_font);
		ufi->using_cairo_glyph.glyph_index = glyph;
	}
}

static void
font_info_measure_font (struct font_info *info)
{
	PangoRectangle logical;
	gunichar full_codepoints[] = {VTE_DRAW_DOUBLE_WIDE_IDEOGRAPHS};
	GString *full_string;
	gint single_width, full_width;
	guint i;

	/* Estimate for ASCII characters. */
	pango_layout_set_text (info->layout, VTE_DRAW_SINGLE_WIDE_CHARACTERS, -1);
	pango_layout_get_extents (info->layout, NULL, &logical);
	single_width = howmany (logical.width, strlen(VTE_DRAW_SINGLE_WIDE_CHARACTERS));
	info->height = PANGO_PIXELS_CEIL (logical.height);
	info->ascent = PANGO_PIXELS_CEIL (pango_layout_get_baseline (info->layout));

	/* Now that we shaped the entire ASCII character string, cache glyph
	 * info for them */
	font_info_cache_ascii (info);

	/* Estimate for CJK characters. */
	full_width = single_width * 2;
	full_string = g_string_new(NULL);
	for (i = 0; i < G_N_ELEMENTS (full_codepoints); i++) {
		g_string_append_unichar (full_string, full_codepoints[i]);
	}
	pango_layout_set_text (info->layout, full_string->str, -1);
	g_string_free (full_string, TRUE);
	pango_layout_get_extents (info->layout, NULL, &logical);
	full_width = howmany (logical.width, G_N_ELEMENTS(full_codepoints));

	/* If they're the same, then we have a screwy font. */
	if (full_width == single_width) {
		single_width = (full_width + 1) / 2;
	}

	/* We don't do CEIL for width since we are averaging; rounding is
	 * more accurate */
	info->width = PANGO_PIXELS (single_width);

	if (info->height == 0) {
		info->height = PANGO_PIXELS_CEIL (logical.height);
	}
	if (info->ascent == 0) {
		info->ascent = PANGO_PIXELS_CEIL (pango_layout_get_baseline (info->layout));
	}

	_vte_debug_print (VTE_DEBUG_MISC,
			  "VtePangoCairo font metrics = %dx%d (%d).\n",
			  info->width, info->height, info->ascent);
}

static struct font_info *
font_info_create_for_screen (GdkScreen                  *screen,
			     const PangoFontDescription *desc,
			     VteTerminalAntiAlias        antialias)
{
	struct font_info *info;
	PangoContext *context;
	PangoLayout *layout;

	/* XXX caching */

	info = g_slice_new0 (struct font_info);

	context = gdk_pango_context_get_for_screen (screen);
	if (!PANGO_IS_CAIRO_FONT_MAP (pango_context_get_font_map (context))) {
		/* Ouch, Gtk+ switched over to some drawing system?
		 * Lets just create one from the default font map.
		 */
		g_object_unref (context);
		context = pango_font_map_create_context (pango_cairo_font_map_get_default ());
	}

	pango_context_set_base_dir (context, PANGO_DIRECTION_LTR);
	if (desc)
		pango_context_set_font_description (context, desc);

	switch (antialias) {
		cairo_font_options_t *font_options;
		cairo_antialias_t cr_aa;

	case VTE_ANTI_ALIAS_FORCE_ENABLE:
	case VTE_ANTI_ALIAS_FORCE_DISABLE:

		if (antialias == VTE_ANTI_ALIAS_FORCE_ENABLE)
			cr_aa = CAIRO_ANTIALIAS_DEFAULT; /* let surface decide between gray and subpixel */
		else
			cr_aa = CAIRO_ANTIALIAS_NONE;

		font_options = cairo_font_options_copy (pango_cairo_context_get_font_options (context));
		cairo_font_options_set_antialias (font_options, cr_aa);
		pango_cairo_context_set_font_options (context, font_options);
		cairo_font_options_destroy (font_options);

		break;

	default:
	case VTE_ANTI_ALIAS_USE_DEFAULT:
		break;
	}

	info->layout = layout = pango_layout_new (context);
	g_object_unref (context);

	info->coverage = pango_coverage_new ();

	font_info_measure_font (info);

	return info;
}

static void
font_info_destroy (struct font_info *info)
{
	gunichar i;

	if (!info)
		return;

	g_object_unref (info->layout);
	info->layout = NULL;

	for (i = 0; i < G_N_ELEMENTS (info->ascii_unichar_info); i++)
		unichar_info_finish (&info->ascii_unichar_info[i]);
		
	if (info->other_unichar_info) {
		g_hash_table_destroy (info->other_unichar_info);
		info->other_unichar_info = NULL;
	}

	pango_coverage_unref (info->coverage);
	info->coverage = NULL;

	info->width = info->height = info->ascent = 1;

#ifdef PROFILE_COVERAGE
	g_message ("coverages %d = %d + %d + %d",
		   coverage_count[0],
		   coverage_count[1],
		   coverage_count[2],
		   coverage_count[3]);
#endif
}

static struct font_info *
font_info_create_for_widget (GtkWidget                  *widget,
			     const PangoFontDescription *desc,
			     VteTerminalAntiAlias        antialias)
{
	GdkScreen *screen = gtk_widget_get_screen (widget);

	if (!desc) {
		PangoContext *context = gtk_widget_get_pango_context (widget);
		desc = pango_context_get_font_description (context);
	}

	return font_info_create_for_screen (screen, desc, antialias);
}

static struct unichar_info *
font_info_get_unichar_info (struct font_info *info,
			    gunichar c)
{
	struct unichar_info *uinfo;
	union unichar_font_info *ufi;
	char buf[VTE_UTF8_BPC+1];
	PangoRectangle logical;
	PangoLayoutLine *line;

	uinfo = font_info_find_unichar_info (info, c);
	if (G_LIKELY (uinfo->coverage != COVERAGE_UNKNOWN))
		return uinfo;

	ufi = &uinfo->ufi;

	buf[g_unichar_to_utf8 (c, buf)] = '\0';
	pango_layout_set_text (info->layout, buf, -1);
	pango_layout_get_extents (info->layout, NULL, &logical);

	uinfo->width = PANGO_PIXELS_CEIL (logical.width);

	line = pango_layout_get_line_readonly (info->layout, 0);

	uinfo->has_unknown_chars = pango_layout_get_unknown_glyphs_count (info->layout) != 0;
	/* we use PangoLayoutRun rendering unless there is exactly one run in the line. */
	if (G_UNLIKELY (!line || !line->runs || line->runs->next))
	{
		uinfo->coverage = COVERAGE_USE_PANGO_LAYOUT_LINE;

		ufi->using_pango_layout_line.line = pango_layout_line_ref (line);
		/* we hold a manual reference on layout.  pango currently
		 * doesn't work if line->layout is NULL.  ugh! */
		pango_layout_set_text (info->layout, "", -1); /* make layout disassociate from the line */
		ufi->using_pango_layout_line.line->layout = g_object_ref (info->layout);

	} else {
		PangoGlyphItem *glyph_item = line->runs->data;
		PangoFont *pango_font = glyph_item->item->analysis.font;
		PangoGlyphString *glyph_string = glyph_item->glyphs;

		/* we use fast cairo path if glyph string has only one real
		 * glyph and at origin */
		if (!uinfo->has_unknown_chars &&
		    glyph_string->num_glyphs == 1 && glyph_string->glyphs[0].glyph <= 0xFFFF &&
		    (glyph_string->glyphs[0].geometry.x_offset |
		     glyph_string->glyphs[0].geometry.y_offset) == 0)
		{
			cairo_scaled_font_t *scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *) pango_font);

			if (scaled_font) {
				uinfo->coverage = COVERAGE_USE_CAIRO_GLYPH;

				ufi->using_cairo_glyph.scaled_font = cairo_scaled_font_reference (scaled_font);
				ufi->using_cairo_glyph.glyph_index = glyph_string->glyphs[0].glyph;
			}
		}

		/* use pango fast path otherwise */
		if (G_UNLIKELY (uinfo->coverage == COVERAGE_UNKNOWN)) {
			uinfo->coverage = COVERAGE_USE_PANGO_GLYPH_STRING;

			ufi->using_pango_glyph_string.font = pango_font ? g_object_ref (pango_font) : NULL;
			ufi->using_pango_glyph_string.glyph_string = pango_glyph_string_copy (glyph_string);
		}
	}

#ifdef PROFILE_COVERAGE
	coverage_count[0]++;
	coverage_count[uinfo->coverage]++;
#endif
	return uinfo;
}


struct _vte_pangocairo_data {
	struct font_info *font;

	cairo_t *cr;

	GdkPixmap *pixmap;
	gint pixmapw, pixmaph;
};

static void
set_source_color_alpha (cairo_t        *cr,
			const GdkColor *color,
			guchar alpha)
{
	cairo_set_source_rgba (cr,
			      color->red / 65535.,
			      color->green / 65535.,
			      color->blue / 65535.,
			      alpha / 255.);
}


static void
_vte_pangocairo_create (struct _vte_draw *draw, GtkWidget *widget)
{
	struct _vte_pangocairo_data *data;

	data = g_slice_new0 (struct _vte_pangocairo_data);
	draw->impl_data = data;
}

static void
_vte_pangocairo_destroy (struct _vte_draw *draw)
{
	struct _vte_pangocairo_data *data = draw->impl_data;

	if (data->pixmap != NULL) {
		g_object_unref (data->pixmap);
		data->pixmap = NULL;
	}

	if (data->font != NULL) {
		font_info_destroy (data->font);
		data->font = NULL;
	}

	g_slice_free (struct _vte_pangocairo_data, draw->impl_data);
}

static void
_vte_pangocairo_start (struct _vte_draw *draw)
{
	struct _vte_pangocairo_data *data = draw->impl_data;

	data->cr = gdk_cairo_create (draw->widget->window);
}

static void
_vte_pangocairo_end (struct _vte_draw *draw)
{
	struct _vte_pangocairo_data *data = draw->impl_data;

	if (data->cr != NULL) {
		cairo_destroy (data->cr);
		data->cr = NULL;
	}
}

static void
_vte_pangocairo_set_background_image (struct _vte_draw *draw,
				      enum VteBgSourceType type,
				      GdkPixbuf *pixbuf,
				      const char *file,
				      const GdkColor *color,
				      double saturation)
{
	struct _vte_pangocairo_data *data = draw->impl_data;
	GdkPixmap *pixmap;
	GdkScreen *screen;

	if (data->pixmap != NULL) {
		g_object_unref(data->pixmap);
	}

	screen = gtk_widget_get_screen(draw->widget);
	pixmap = vte_bg_get_pixmap(vte_bg_get_for_screen(screen),
				   type, pixbuf, file,
				   color, saturation,
				   _vte_draw_get_colormap(draw, TRUE));

	data->pixmap = NULL;
	data->pixmapw = data->pixmaph = 0;
	if (pixmap) {
		gdk_drawable_get_size(pixmap, &data->pixmapw, &data->pixmaph);
		data->pixmap = pixmap;
	}
}

static void
_vte_pangocairo_clip (struct _vte_draw *draw,
		      GdkRegion *region)
{
	struct _vte_pangocairo_data *data = draw->impl_data;

	gdk_cairo_region(data->cr, region);
	cairo_clip (data->cr);
}

static void
_vte_pangocairo_clear (struct _vte_draw *draw,
		       gint x, gint y, gint width, gint height)
{
	struct _vte_pangocairo_data *data = draw->impl_data;

	cairo_rectangle (data->cr, x, y, width, height);
	cairo_set_operator (data->cr, CAIRO_OPERATOR_SOURCE);

	if (data->pixmap == NULL) {
		set_source_color_alpha (data->cr, &draw->bg_color, draw->bg_opacity >> 8);
	} else {
		gdk_cairo_set_source_pixmap (data->cr, data->pixmap,
					     draw->scrollx, draw->scrolly);
		cairo_pattern_set_extend (cairo_get_source (data->cr), CAIRO_EXTEND_REPEAT);
	}

	cairo_fill (data->cr);
}

static void
_vte_pangocairo_set_text_font (struct _vte_draw *draw,
			       const PangoFontDescription *fontdesc,
			       VteTerminalAntiAlias antialias)
{
	struct _vte_pangocairo_data *data = draw->impl_data;

	font_info_destroy (data->font);
	data->font = font_info_create_for_widget (draw->widget, fontdesc, antialias);
}

static void
_vte_pangocairo_get_text_metrics(struct _vte_draw *draw,
				 gint *width, gint *height, gint *ascent)
{
	struct _vte_pangocairo_data *data = draw->impl_data;
	
	g_return_if_fail (data->font != NULL);

	*width  = data->font->width;
	*height = data->font->height;
	*ascent = data->font->ascent;
}


static int
_vte_pangocairo_get_char_width (struct _vte_draw *draw, gunichar c, int columns)
{
	struct _vte_pangocairo_data *data = draw->impl_data;
	struct unichar_info *uinfo;

	g_return_val_if_fail (data->font != NULL, 0);

	uinfo = font_info_get_unichar_info (data->font, c);
	return uinfo->width;
}

static void
_vte_pangocairo_draw_text (struct _vte_draw *draw,
			   struct _vte_draw_text_request *requests, gsize n_requests,
			   GdkColor *color, guchar alpha)
{
	struct _vte_pangocairo_data *data = draw->impl_data;
	gsize i;
	cairo_scaled_font_t *last_scaled_font = NULL;
	int n_cr_glyphs = 0;
	cairo_glyph_t cr_glyphs[MAX_RUN_LENGTH];

	g_return_if_fail (data->font != NULL);

	set_source_color_alpha (data->cr, color, alpha);
	cairo_set_operator (data->cr, CAIRO_OPERATOR_OVER);

	for (i = 0; i < n_requests; i++) {
		gunichar c = requests[i].c;
		int x = requests[i].x;
		int y = requests[i].y + data->font->ascent;
		struct unichar_info *uinfo = font_info_get_unichar_info (data->font, c);
		union unichar_font_info *ufi = &uinfo->ufi;

		switch (uinfo->coverage) {
		default:
		case COVERAGE_UNKNOWN:
			g_assert_not_reached ();
			break;
		case COVERAGE_USE_PANGO_LAYOUT_LINE:
			cairo_move_to (data->cr, x, y);
			pango_cairo_show_layout_line (data->cr,
						      ufi->using_pango_layout_line.line);
			break;
		case COVERAGE_USE_PANGO_GLYPH_STRING:
			cairo_move_to (data->cr, x, y);
			pango_cairo_show_glyph_string (data->cr,
						       ufi->using_pango_glyph_string.font,
						       ufi->using_pango_glyph_string.glyph_string);
			break;
		case COVERAGE_USE_CAIRO_GLYPH:
			if (last_scaled_font != ufi->using_cairo_glyph.scaled_font || n_cr_glyphs == MAX_RUN_LENGTH) {
				if (n_cr_glyphs) {
					cairo_set_scaled_font (data->cr, last_scaled_font);
					cairo_show_glyphs (data->cr,
							   cr_glyphs,
							   n_cr_glyphs);
					n_cr_glyphs = 0;
				}
				last_scaled_font = ufi->using_cairo_glyph.scaled_font;
			}
			cr_glyphs[n_cr_glyphs].index = ufi->using_cairo_glyph.glyph_index;
			cr_glyphs[n_cr_glyphs].x = x;
			cr_glyphs[n_cr_glyphs].y = y;
			n_cr_glyphs++;
			break;
		}
	}
	if (n_cr_glyphs) {
		cairo_set_scaled_font (data->cr, last_scaled_font);
		cairo_show_glyphs (data->cr,
				   cr_glyphs,
				   n_cr_glyphs);
		n_cr_glyphs = 0;
	}
}

static gboolean
_vte_pangocairo_draw_has_char (struct _vte_draw *draw, gunichar c)
{
	struct _vte_pangocairo_data *data = draw->impl_data;
	struct unichar_info *uinfo;

	g_return_val_if_fail (data->font != NULL, FALSE);

	uinfo = font_info_get_unichar_info (data->font, c);
	return !uinfo->has_unknown_chars;
}

static void
_vte_pangocairo_draw_rectangle (struct _vte_draw *draw,
				gint x, gint y, gint width, gint height,
				GdkColor *color, guchar alpha)
{
	struct _vte_pangocairo_data *data = draw->impl_data;

	cairo_set_operator (data->cr, CAIRO_OPERATOR_OVER);
	cairo_rectangle (data->cr, x+.5, y+.5, width-1, height-1);
	set_source_color_alpha (data->cr, color, alpha);
	cairo_set_line_width (data->cr, 1);
	cairo_stroke (data->cr);
}

static void
_vte_pangocairo_fill_rectangle (struct _vte_draw *draw,
				gint x, gint y, gint width, gint height,
				GdkColor *color, guchar alpha)
{
	struct _vte_pangocairo_data *data = draw->impl_data;

	cairo_set_operator (data->cr, CAIRO_OPERATOR_OVER);
	cairo_rectangle (data->cr, x, y, width, height);
	set_source_color_alpha (data->cr, color, alpha);
	cairo_fill (data->cr);
}

const struct _vte_draw_impl _vte_draw_pangocairo = {
	"pangocairo",
	NULL, /* check */
	_vte_pangocairo_create,
	_vte_pangocairo_destroy,
	NULL, /* get_visual */
	NULL, /* get_colormap */
	_vte_pangocairo_start,
	_vte_pangocairo_end,
	_vte_pangocairo_set_background_image,
	_vte_pangocairo_clip,
	FALSE, /* always_requires_clear */
	_vte_pangocairo_clear,
	_vte_pangocairo_set_text_font,
	_vte_pangocairo_get_text_metrics,
	_vte_pangocairo_get_char_width,
	NULL, /* get_using_fontconfig */
	_vte_pangocairo_draw_text,
	_vte_pangocairo_draw_has_char,
	_vte_pangocairo_draw_rectangle,
	_vte_pangocairo_fill_rectangle
};
