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


#include <config.h>

#include <sys/param.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include "debug.h"
#include "vtebg.h"
#include "vtedraw.h"
#include "vte-private.h"

#include <pango/pangocairo.h>


/* Overview:
 *
 *
 * This file implements vte rendering using pangocairo.  Note that this does
 * NOT implement any kind of complex text rendering.  That's not currently a
 * goal.
 *
 * The aim is to be super-fast and avoid unneeded work as much as possible.
 * Here is an overview of how that is accomplished:
 *
 *   - We attach a font_info to the draw.  A font_info has all the information
 *     to quickly draw text.
 *
 *   - A font_info keeps uses unistr_font_info structs that represent all
 *     information needed to quickly draw a single vteunistr.  The font_info
 *     creates those unistr_font_info structs on demand and caches them
 *     indefinitely.  It uses a direct array for the ASCII range and a hash
 *     table for the rest.
 *
 *
 * Fast rendering of unistrs:
 *
 * A unistr_font_info (uinfo) calls Pango to set text for the unistr upon
 * initialization and then caches information needed to draw the results
 * later.  It uses three different internal representations and respectively
 * three drawing paths:
 *
 *   - COVERAGE_USE_CAIRO_GLYPH:
 *     Keeping a single glyph index and a cairo scaled-font.  This is the
 *     fastest way to draw text as it bypasses Pango completely and allows
 *     for stuffing multiple glyphs into a single cairo_show_glyphs() request
 *     (if scaled-fonts match).  This method is used if the glyphs used for
 *     the vteunistr as determined by Pango consists of a single regular glyph
 *     positioned at 0,0 using a regular font.  This method is used for more
 *     than 99% of the cases.  Only exceptional cases fall through to the
 *     other two methods.
 *
 *   - COVERAGE_USE_PANGO_GLYPH_STRING:
 *     Keeping a pango glyphstring and a pango font.  This is slightly slower
 *     than the previous case as drawing each glyph goes through pango
 *     separately and causes a separate cairo_show_glyphs() call.  This method
 *     is used when the previous method cannot be used but the glyphs for the
 *     character all use a single font.  This is the method used for hexboxes
 *     and "empty" characters like U+200C ZERO WIDTH NON-JOINER for example.
 *
 *   - COVERAGE_USE_PANGO_LAYOUT_LINE:
 *     Keeping a pango layout line.  This method is used only in the very
 *     weird and exceptional case that a single vteunistr uses more than one
 *     font to be drawn.  This happens for example if some diacretics is not
 *     available in the font chosen for the base character.
 *
 *
 * Caching of font infos:
 *
 * To avoid recreating font info structs for the same font again and again we
 * do the following:
 *
 *   - Use a global cache to share font info structs across different widgets.
 *     We use pango language, cairo font options, resolution, and font description
 *     as the key for our hash table.
 *
 *   - When a font info struct is no longer used by any widget, we delay
 *     destroying it for a while (FONT_CACHE_TIMEOUT seconds).  This is
 *     supposed to serve two purposes:
 *
 *       * Destroying a terminal widget and creating it again right after will
 *         reuse the font info struct from the previous widget.
 *
 *       * Zooming in and out a terminal reuses the font info structs.
 *
 *     Since we use gdk timeout to schedule the delayed destruction, we also
 *     add a gtk quit handler which is run when the innermost main loop exits
 *     to cleanup any pending delayed destructions.
 *
 *
 * Pre-caching ASCII letters:
 *
 * When initializing a font info struct we measure a string consisting of all
 * ASCII letters and some other ASCII characters.  Since we have a shaped pango
 * layout at hand, we walk over it and cache unistr font info for the ASCII
 * letters if we can do that easily using COVERAGE_USE_CAIRO_GLYPH.  This
 * means that we precache all ASCII letters without any extra pango shaping
 * involved.
 */



#define FONT_CACHE_TIMEOUT (30) /* seconds */


/* All shared data structures are implicitly protected by GDK mutex, because
 * that's how vte.c works and we only get called from there. */


/* cairo_show_glyphs accepts runs up to 102 glyphs before it allocates a
 * temporary array.
 *
 * Setting this to a large value can cause dramatic slow-downs for some
 * xservers (notably fglrx), see bug #410534.
 *
 * Moreover, setting it larger than %VTE_DRAW_MAX_LENGTH is nonsensical,
 * as the higher layers will not submit runs longer than that value.
 */
#define MAX_RUN_LENGTH 100


enum unistr_coverage {
	/* in increasing order of speed */
	COVERAGE_UNKNOWN = 0,		/* we don't know about the character yet */
	COVERAGE_USE_PANGO_LAYOUT_LINE,	/* use a PangoLayoutLine for the character */
	COVERAGE_USE_PANGO_GLYPH_STRING,	/* use a PangoGlyphString for the character */
	COVERAGE_USE_CAIRO_GLYPH	/* use a cairo_glyph_t for the character */
};

union unistr_font_info {
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

struct unistr_info {
	guchar coverage;
	guchar has_unknown_chars;
	guint16 width;
	union unistr_font_info ufi;
};

static struct unistr_info *
unistr_info_create (void)
{
	return g_slice_new0 (struct unistr_info);
}

static void
unistr_info_finish (struct unistr_info *uinfo)
{
	union unistr_font_info *ufi = &uinfo->ufi;

	switch (uinfo->coverage) {
	default:
	case COVERAGE_UNKNOWN:
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
unistr_info_destroy (struct unistr_info *uinfo)
{
	unistr_info_finish (uinfo);
	g_slice_free (struct unistr_info, uinfo);
}

struct font_info {
	/* lifecycle */
	int ref_count;
	guint destroy_timeout; /* only used when ref_count == 0 */

	/* reusable layout set with font and everything set */
	PangoLayout *layout;

	/* cache of character info */
	struct unistr_info ascii_unistr_info[128];
	GHashTable *other_unistr_info;

	/* cell metrics */
	gint width, height, ascent;

	/* reusable string for UTF-8 conversion */
	GString *string;

#ifdef VTE_DEBUG
	/* profiling info */
	int coverage_count[4];
#endif
};


static struct unistr_info *
font_info_find_unistr_info (struct font_info    *info,
			    vteunistr            c)
{
	struct unistr_info *uinfo;

	if (G_LIKELY (c < G_N_ELEMENTS (info->ascii_unistr_info)))
		return &info->ascii_unistr_info[c];

	if (G_UNLIKELY (info->other_unistr_info == NULL))
		info->other_unistr_info = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) unistr_info_destroy);

	uinfo = g_hash_table_lookup (info->other_unistr_info, GINT_TO_POINTER (c));
	if (G_LIKELY (uinfo))
		return uinfo;

	uinfo = unistr_info_create ();
	g_hash_table_insert (info->other_unistr_info, GINT_TO_POINTER (c), uinfo);
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
	PangoLanguage *language;
	gboolean latin_uses_default_language;
	
	/* We have info->layout holding most ASCII characters.  We want to
	 * cache as much info as we can about the ASCII letters so we don't
	 * have to look them up again later */

	/* Don't cache if unknown glyphs found in layout */
	if (pango_layout_get_unknown_glyphs_count (info->layout) != 0)
		return;

	language = pango_context_get_language (pango_layout_get_context (info->layout));
	if (language == NULL)
		language = pango_language_get_default ();
	latin_uses_default_language = pango_language_includes_script (language, PANGO_SCRIPT_LATIN);

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
		struct unistr_info *uinfo;
		union unistr_font_info *ufi;
	 	PangoGlyphGeometry *geometry;
		PangoGlyph glyph;
		vteunistr c;

		/* Only cache simple clusters */
		if (iter.start_char +1 != iter.end_char  ||
		    iter.start_index+1 != iter.end_index ||
		    iter.start_glyph+1 != iter.end_glyph)
			continue;

		c = text[iter.start_index];
		glyph = glyph_string->glyphs[iter.start_glyph].glyph;
		geometry = &glyph_string->glyphs[iter.start_glyph].geometry;

		/* If not using the default locale language, only cache non-common
		 * characters as common characters get their font from their neighbors
		 * and we don't want to force Latin on them. */
		if (!latin_uses_default_language &&
		    pango_script_for_unichar (c) <= PANGO_SCRIPT_INHERITED)
			continue;

		/* Only cache simple glyphs */
		if (!(glyph <= 0xFFFF) || (geometry->x_offset | geometry->y_offset) != 0)
			continue;

		uinfo = font_info_find_unistr_info (info, c);
		if (G_UNLIKELY (uinfo->coverage != COVERAGE_UNKNOWN))
			continue;

		ufi = &uinfo->ufi;

		uinfo->width = PANGO_PIXELS_CEIL (geometry->width);
		uinfo->has_unknown_chars = FALSE;

		uinfo->coverage = COVERAGE_USE_CAIRO_GLYPH;

		ufi->using_cairo_glyph.scaled_font = cairo_scaled_font_reference (scaled_font);
		ufi->using_cairo_glyph.glyph_index = glyph;

#ifdef VTE_DEBUG
		info->coverage_count[0]++;
		info->coverage_count[uinfo->coverage]++;
#endif
	}

#ifdef VTE_DEBUG
	_vte_debug_print (VTE_DEBUG_PANGOCAIRO,
			  "vtepangocairo: %p cached %d ASCII letters\n",
			  info, info->coverage_count[0]);
#endif
}

static void
font_info_measure_font (struct font_info *info)
{
	PangoRectangle logical;

	/* Estimate for ASCII characters. */
	pango_layout_set_text (info->layout, VTE_DRAW_SINGLE_WIDE_CHARACTERS, -1);
	pango_layout_get_extents (info->layout, NULL, &logical);
	/* We don't do CEIL for width since we are averaging;
	 * rounding is more accurate */
	info->width  = PANGO_PIXELS (howmany (logical.width, strlen(VTE_DRAW_SINGLE_WIDE_CHARACTERS)));
	info->height = PANGO_PIXELS_CEIL (logical.height);
	info->ascent = PANGO_PIXELS_CEIL (pango_layout_get_baseline (info->layout));

	/* Now that we shaped the entire ASCII character string, cache glyph
	 * info for them */
	font_info_cache_ascii (info);


	if (info->height == 0) {
		info->height = PANGO_PIXELS_CEIL (logical.height);
	}
	if (info->ascent == 0) {
		info->ascent = PANGO_PIXELS_CEIL (pango_layout_get_baseline (info->layout));
	}

	_vte_debug_print (VTE_DEBUG_MISC,
			  "vtepangocairo: %p font metrics = %dx%d (%d)\n",
			  info, info->width, info->height, info->ascent);
}


static struct font_info *
font_info_allocate (PangoContext *context)
{
	struct font_info *info;

	info = g_slice_new0 (struct font_info);

	_vte_debug_print (VTE_DEBUG_PANGOCAIRO,
			  "vtepangocairo: %p allocating font_info\n",
			  info);

	info->layout = pango_layout_new (context);
	info->string = g_string_sized_new (VTE_UTF8_BPC+1);

	font_info_measure_font (info);

	return info;
}

static void
font_info_free (struct font_info *info)
{
	vteunistr i;

#ifdef VTE_DEBUG
	_vte_debug_print (VTE_DEBUG_PANGOCAIRO,
			  "vtepangocairo: %p freeing font_info.  coverages %d = %d + %d + %d\n",
			  info,
			  info->coverage_count[0],
			  info->coverage_count[1],
			  info->coverage_count[2],
			  info->coverage_count[3]);
#endif

	g_string_free (info->string, TRUE);
	g_object_unref (info->layout);

	for (i = 0; i < G_N_ELEMENTS (info->ascii_unistr_info); i++)
		unistr_info_finish (&info->ascii_unistr_info[i]);
		
	if (info->other_unistr_info) {
		g_hash_table_destroy (info->other_unistr_info);
	}

	g_slice_free (struct font_info, info);
}


static GHashTable *font_info_for_context;
static guint quit_id;

static gboolean
cleanup_delayed_font_info_destroys_predicate (PangoContext *context,
					      struct font_info *info)
{
	if (info->destroy_timeout) {
		g_source_remove (info->destroy_timeout);
		info->destroy_timeout = 0;

		font_info_free (info);
		return TRUE;
	}

	return FALSE;
}

static gboolean
cleanup_delayed_font_info_destroys (void)
{
	g_hash_table_foreach_remove (font_info_for_context,
				     (GHRFunc) cleanup_delayed_font_info_destroys_predicate,
				     NULL);

	quit_id = 0;
	return 0;
}

static void
ensure_quit_handler (void)
{
	if (G_UNLIKELY (quit_id == 0))
		quit_id = gtk_quit_add (1,
					(GtkFunction) cleanup_delayed_font_info_destroys,
					NULL);
}

static struct font_info *
font_info_register (struct font_info *info)
{
	g_hash_table_insert (font_info_for_context,
			     pango_layout_get_context (info->layout),
			     info);

	return info;
}

static void
font_info_unregister (struct font_info *info)
{
	g_hash_table_remove (font_info_for_context,
			     pango_layout_get_context (info->layout));
}


static struct font_info *
font_info_reference (struct font_info *info)
{
	if (!info)
		return info;

	g_return_val_if_fail (info->ref_count >= 0, info);

	if (info->destroy_timeout) {
		g_source_remove (info->destroy_timeout);
		info->destroy_timeout = 0;
	}

	info->ref_count++;

	return info;
}

static gboolean
font_info_destroy_delayed (struct font_info *info)
{
	info->destroy_timeout = 0;

	font_info_unregister (info);
	font_info_free (info);

	return FALSE;
}

static void
font_info_destroy (struct font_info *info)
{
	if (!info)
		return;

	g_return_if_fail (info->ref_count > 0);

	info->ref_count--;
	if (info->ref_count)
		return;

	/* Delay destruction by a few seconds, in case we need it again */
	ensure_quit_handler ();
	info->destroy_timeout = gdk_threads_add_timeout_seconds (FONT_CACHE_TIMEOUT,
								 (GSourceFunc) font_info_destroy_delayed,
								 info);
}

static GQuark
fontconfig_timestamp_quark (void)
{
	static GQuark quark;

	if (G_UNLIKELY (!quark))
		quark = g_quark_from_static_string ("vte-fontconfig-timestamp");

	return quark;
}

static void
vte_pango_context_set_fontconfig_timestamp (PangoContext *context,
					    guint         fontconfig_timestamp)
{
	g_object_set_qdata ((GObject *) context,
			    fontconfig_timestamp_quark (),
			    GUINT_TO_POINTER (fontconfig_timestamp));
}

static guint
vte_pango_context_get_fontconfig_timestamp (PangoContext *context)
{
	return GPOINTER_TO_UINT (g_object_get_qdata ((GObject *) context,
						     fontconfig_timestamp_quark ()));
}

static guint
context_hash (PangoContext *context)
{
	return pango_units_from_double (pango_cairo_context_get_resolution (context))
	     ^ pango_font_description_hash (pango_context_get_font_description (context))
	     ^ cairo_font_options_hash (pango_cairo_context_get_font_options (context))
	     ^ GPOINTER_TO_UINT (pango_context_get_language (context))
	     ^ vte_pango_context_get_fontconfig_timestamp (context);
}

static gboolean
context_equal (PangoContext *a,
	       PangoContext *b)
{
	return pango_cairo_context_get_resolution (a) == pango_cairo_context_get_resolution (b)
	    && pango_font_description_equal (pango_context_get_font_description (a), pango_context_get_font_description (b))
	    && cairo_font_options_equal (pango_cairo_context_get_font_options (a), pango_cairo_context_get_font_options (b))
	    && pango_context_get_language (a) == pango_context_get_language (b)
	    && vte_pango_context_get_fontconfig_timestamp (a) == vte_pango_context_get_fontconfig_timestamp (b);
}

static struct font_info *
font_info_find_for_context (PangoContext *context)
{
	struct font_info *info;

	if (G_UNLIKELY (font_info_for_context == NULL))
		font_info_for_context = g_hash_table_new ((GHashFunc) context_hash, (GEqualFunc) context_equal);

	info = g_hash_table_lookup (font_info_for_context, context);
	if (G_LIKELY (info)) {
		_vte_debug_print (VTE_DEBUG_PANGOCAIRO,
				  "vtepangocairo: %p found font_info in cache\n",
				  info);
		return font_info_reference (info);
	}

	info = font_info_allocate (context);
	info->ref_count = 1;
	font_info_register (info);

	g_object_unref (context);

	return info;
}

/* assumes ownership/reference of context */
static struct font_info *
font_info_create_for_context (PangoContext               *context,
			      const PangoFontDescription *desc,
			      VteTerminalAntiAlias        antialias,
			      PangoLanguage              *language,
			      guint                       fontconfig_timestamp)
{
	if (!PANGO_IS_CAIRO_FONT_MAP (pango_context_get_font_map (context))) {
		/* Ouch, Gtk+ switched over to some drawing system?
		 * Lets just create one from the default font map.
		 */
		g_object_unref (context);
		context = pango_font_map_create_context (pango_cairo_font_map_get_default ());
	}

	vte_pango_context_set_fontconfig_timestamp (context, fontconfig_timestamp);

	pango_context_set_base_dir (context, PANGO_DIRECTION_LTR);

	if (desc)
		pango_context_set_font_description (context, desc);

	pango_context_set_language (context, language);

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
		/* Make sure our contexts have a font_options set.  We use
		 * this invariant in our context hash and equal functions.
		 */
		if (!pango_cairo_context_get_font_options (context)) {
			font_options = cairo_font_options_create ();
			pango_cairo_context_set_font_options (context, font_options);
			cairo_font_options_destroy (font_options);
		}
		break;
	}

	return font_info_find_for_context (context);
}

static struct font_info *
font_info_create_for_screen (GdkScreen                  *screen,
			     const PangoFontDescription *desc,
			     VteTerminalAntiAlias        antialias,
			     PangoLanguage              *language)
{
	GtkSettings *settings = gtk_settings_get_for_screen (screen);
	int fontconfig_timestamp;
	g_object_get (settings, "gtk-fontconfig-timestamp", &fontconfig_timestamp, NULL);
	return font_info_create_for_context (gdk_pango_context_get_for_screen (screen),
					     desc, antialias, language, fontconfig_timestamp);
}

static struct font_info *
font_info_create_for_widget (GtkWidget                  *widget,
			     const PangoFontDescription *desc,
			     VteTerminalAntiAlias        antialias)
{
	GdkScreen *screen = gtk_widget_get_screen (widget);
	PangoLanguage *language = pango_context_get_language (gtk_widget_get_pango_context (widget));

	return font_info_create_for_screen (screen, desc, antialias, language);
}

static struct unistr_info *
font_info_get_unistr_info (struct font_info *info,
			   vteunistr c)
{
	struct unistr_info *uinfo;
	union unistr_font_info *ufi;
	PangoRectangle logical;
	PangoLayoutLine *line;

	uinfo = font_info_find_unistr_info (info, c);
	if (G_LIKELY (uinfo->coverage != COVERAGE_UNKNOWN))
		return uinfo;

	ufi = &uinfo->ufi;

	g_string_set_size (info->string, 0);
	_vte_unistr_append_to_string (c, info->string);
	pango_layout_set_text (info->layout, info->string->str, -1);
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

	/* release internal layout resources */
	pango_layout_set_text (info->layout, "", -1);

#ifdef VTE_DEBUG
	info->coverage_count[0]++;
	info->coverage_count[uinfo->coverage]++;
#endif

	return uinfo;
}

struct _vte_draw {
	GtkWidget *widget;

	gint started;

	gboolean requires_clear;

	struct font_info *font;
	struct font_info *font_bold;
	cairo_pattern_t *bg_pattern;

	cairo_t *cr;
};

struct _vte_draw *
_vte_draw_new (GtkWidget *widget)
{
	struct _vte_draw *draw;

	/* Create the structure. */
	draw = g_slice_new0 (struct _vte_draw);
	draw->widget = g_object_ref (widget);
	draw->requires_clear = FALSE;

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_new\n");

	return draw;
}

void
_vte_draw_free (struct _vte_draw *draw)
{
	_vte_debug_print (VTE_DEBUG_DRAW, "draw_free\n");

	if (draw->bg_pattern != NULL) {
		cairo_pattern_destroy (draw->bg_pattern);
		draw->bg_pattern = NULL;
	}

	if (draw->font != NULL) {
		font_info_destroy (draw->font);
		draw->font = NULL;
	}

	if (draw->widget != NULL) {
		g_object_unref (draw->widget);
	}

	g_slice_free (struct _vte_draw, draw);
}

void
_vte_draw_start (struct _vte_draw *draw)
{
	g_return_if_fail (GTK_WIDGET_REALIZED (draw->widget));

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_start\n");

	if (draw->started == 0) {
		g_object_ref (draw->widget->window);
		draw->cr = gdk_cairo_create (draw->widget->window);
	}

	draw->started++;
}

void
_vte_draw_end (struct _vte_draw *draw)
{
	g_return_if_fail (draw->started);

	draw->started--;
	if (draw->started == 0) {
		cairo_destroy (draw->cr);
		draw->cr = NULL;
		g_object_unref (draw->widget->window);
 	}

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_end\n");
}

void
_vte_draw_set_background_solid(struct _vte_draw *draw,
			       double red,
			       double green,
			       double blue,
			       double opacity)
{
	draw->requires_clear = opacity != 0xFFFF;

	if (draw->bg_pattern)
		cairo_pattern_destroy (draw->bg_pattern);

	draw->bg_pattern = cairo_pattern_create_rgba (red,
						      green,
						      blue,
						      opacity);
}

void
_vte_draw_set_background_image (struct _vte_draw *draw,
			        VteBgSourceType type,
			        GdkPixbuf *pixbuf,
			        const char *filename,
			        const PangoColor *color,
			        double saturation)
{
	cairo_surface_t *surface;

	if (type != VTE_BG_SOURCE_NONE)
		draw->requires_clear = TRUE;

	/* Need a valid draw->cr for cairo_get_target () */
	_vte_draw_start (draw);

	surface = vte_bg_get_surface (vte_bg_get_for_screen (gtk_widget_get_screen (draw->widget)),
				     type, pixbuf, filename,
				     color, saturation,
				     cairo_get_target(draw->cr));

	_vte_draw_end (draw);

	if (!surface)
		return;

	if (draw->bg_pattern)
		cairo_pattern_destroy (draw->bg_pattern);

	draw->bg_pattern = cairo_pattern_create_for_surface (surface);
	cairo_surface_destroy (surface);
	cairo_pattern_set_extend (draw->bg_pattern, CAIRO_EXTEND_REPEAT);
}

void
_vte_draw_set_background_scroll (struct _vte_draw *draw,
				 gint x, gint y)
{
	cairo_matrix_t matrix;

	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_set_scroll (%d, %d)\n",
			x, y);

	g_return_if_fail (draw->bg_pattern != NULL);

	cairo_matrix_init_translate (&matrix, x, y);
	cairo_pattern_set_matrix (draw->bg_pattern, &matrix);
}

gboolean
_vte_draw_clip (struct _vte_draw *draw, GdkRegion *region)
{
	_vte_debug_print (VTE_DEBUG_DRAW, "draw_clip\n");
	gdk_cairo_region(draw->cr, region);
	cairo_clip (draw->cr);

	return TRUE;
}

void
_vte_draw_clear (struct _vte_draw *draw, gint x, gint y, gint width, gint height)
{
	g_return_if_fail (draw->bg_pattern != NULL);

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_clear (%d, %d, %d, %d)\n",
			  x,y,width, height);

	cairo_rectangle (draw->cr, x, y, width, height);
	cairo_set_operator (draw->cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source (draw->cr, draw->bg_pattern);
	cairo_fill (draw->cr);
}

void
_vte_draw_set_text_font (struct _vte_draw *draw,
			const PangoFontDescription *fontdesc,
			VteTerminalAntiAlias antialias)
{
	PangoFontDescription *bolddesc = NULL;

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_set_text_font (aa=%d)\n",
			  antialias);

	if (draw->font_bold != draw->font)
		font_info_destroy (draw->font_bold);
	font_info_destroy (draw->font);
	draw->font = font_info_create_for_widget (draw->widget, fontdesc, antialias);

	/* calculate bold font desc */
	bolddesc = pango_font_description_copy (fontdesc);
	pango_font_description_set_weight (bolddesc, PANGO_WEIGHT_BOLD);

	draw->font_bold = font_info_create_for_widget (draw->widget, bolddesc, antialias);
	pango_font_description_free (bolddesc);

	/* Decide if we should keep this bold font face, per bug 54926:
	 *  - reject bold font if it is not within 10% of normal font width
	 */
	if ( abs((draw->font_bold->width * 100 / draw->font->width) - 100) > 10 ) {
		font_info_destroy (draw->font_bold);
		draw->font_bold = draw->font;
	}
}

void
_vte_draw_get_text_metrics(struct _vte_draw *draw,
			   gint *width, gint *height, gint *ascent)
{
	g_return_if_fail (draw->font != NULL);

	if (width)
		*width  = draw->font->width;
	if (height)
		*height = draw->font->height;
	if (ascent)
		*ascent = draw->font->ascent;
}


int
_vte_draw_get_char_width (struct _vte_draw *draw, vteunistr c, int columns,
			  gboolean bold)
{
	struct unistr_info *uinfo;

	g_return_val_if_fail (draw->font != NULL, 0);

	uinfo = font_info_get_unistr_info (bold ? draw->font_bold : draw->font, c);
	return uinfo->width;
}

static gboolean
_vte_draw_has_bold (struct _vte_draw *draw)
{
	return (draw->font != draw->font_bold);
}

static void
set_source_color_alpha (cairo_t        *cr,
			const PangoColor *color,
			guchar alpha)
{
	cairo_set_source_rgba (cr,
			      color->red / 65535.,
			      color->green / 65535.,
			      color->blue / 65535.,
			      alpha / 255.);
}

static void
_vte_draw_text_internal (struct _vte_draw *draw,
			 struct _vte_draw_text_request *requests, gsize n_requests,
			 const PangoColor *color, guchar alpha, gboolean bold)
{
	gsize i;
	cairo_scaled_font_t *last_scaled_font = NULL;
	int n_cr_glyphs = 0;
	cairo_glyph_t cr_glyphs[MAX_RUN_LENGTH];
	struct font_info *font = bold ? draw->font_bold : draw->font;

	g_return_if_fail (font != NULL);

	set_source_color_alpha (draw->cr, color, alpha);
	cairo_set_operator (draw->cr, CAIRO_OPERATOR_OVER);

	for (i = 0; i < n_requests; i++) {
		vteunistr c = requests[i].c;
		int x = requests[i].x;
		int y = requests[i].y + font->ascent;
		struct unistr_info *uinfo = font_info_get_unistr_info (font, c);
		union unistr_font_info *ufi = &uinfo->ufi;

		switch (uinfo->coverage) {
		default:
		case COVERAGE_UNKNOWN:
			g_assert_not_reached ();
			break;
		case COVERAGE_USE_PANGO_LAYOUT_LINE:
			cairo_move_to (draw->cr, x, y);
			pango_cairo_show_layout_line (draw->cr,
						      ufi->using_pango_layout_line.line);
			break;
		case COVERAGE_USE_PANGO_GLYPH_STRING:
			cairo_move_to (draw->cr, x, y);
			pango_cairo_show_glyph_string (draw->cr,
						       ufi->using_pango_glyph_string.font,
						       ufi->using_pango_glyph_string.glyph_string);
			break;
		case COVERAGE_USE_CAIRO_GLYPH:
			if (last_scaled_font != ufi->using_cairo_glyph.scaled_font || n_cr_glyphs == MAX_RUN_LENGTH) {
				if (n_cr_glyphs) {
					cairo_set_scaled_font (draw->cr, last_scaled_font);
					cairo_show_glyphs (draw->cr,
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
		cairo_set_scaled_font (draw->cr, last_scaled_font);
		cairo_show_glyphs (draw->cr,
				   cr_glyphs,
				   n_cr_glyphs);
		n_cr_glyphs = 0;
	}
}

void
_vte_draw_text (struct _vte_draw *draw,
	       struct _vte_draw_text_request *requests, gsize n_requests,
	       const PangoColor *color, guchar alpha, gboolean bold)
{
	g_return_if_fail (draw->started);

	if (_vte_debug_on (VTE_DEBUG_DRAW)) {
		GString *string = g_string_new ("");
		gchar *str;
		gsize n;
		for (n = 0; n < n_requests; n++) {
			g_string_append_unichar (string, requests[n].c);
		}
		str = g_string_free (string, FALSE);
		g_printerr ("draw_text (\"%s\", len=%"G_GSIZE_FORMAT", color=(%d,%d,%d,%d), %s)\n",
				str, n_requests, color->red, color->green, color->blue,
				alpha, bold ? "bold" : "normal");
		g_free (str);
	}

	_vte_draw_text_internal (draw, requests, n_requests, color, alpha, bold);

	/* handle fonts that lack a bold face by double-striking */
	if (bold && !_vte_draw_has_bold (draw)) {
		gsize i;

		/* Take a step to the right. */
		for (i = 0; i < n_requests; i++) {
			requests[i].x++;
		}
		_vte_draw_text_internal (draw, requests,
					   n_requests, color, alpha, FALSE);
		/* Now take a step back. */
		for (i = 0; i < n_requests; i++) {
			requests[i].x--;
		}
	}
}

gboolean
_vte_draw_has_char (struct _vte_draw *draw, vteunistr c, gboolean bold)
{
	struct unistr_info *uinfo;

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_has_char ('0x%04X', %s)\n", c,
			  bold ? "bold" : "normal");

	g_return_val_if_fail (draw->font != NULL, FALSE);

	uinfo = font_info_get_unistr_info (bold ? draw->font_bold : draw->font, c);
	return !uinfo->has_unknown_chars;
}

gboolean
_vte_draw_char (struct _vte_draw *draw,
	       struct _vte_draw_text_request *request,
	       const PangoColor *color, guchar alpha, gboolean bold)
{
	gboolean has_char;

	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_char ('%c', color=(%d,%d,%d,%d), %s)\n",
			request->c,
			color->red, color->green, color->blue,
			alpha, bold ? "bold" : "normal");

	has_char =_vte_draw_has_char (draw, request->c, bold);
	if (has_char)
		_vte_draw_text (draw, request, 1, color, alpha, bold);

	return has_char;
}

void
_vte_draw_draw_rectangle (struct _vte_draw *draw,
			 gint x, gint y, gint width, gint height,
			 const PangoColor *color, guchar alpha)
{
	g_return_if_fail (draw->started);

	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_rectangle (%d, %d, %d, %d, color=(%d,%d,%d,%d))\n",
			x,y,width,height,
			color->red, color->green, color->blue,
			alpha);

	cairo_set_operator (draw->cr, CAIRO_OPERATOR_OVER);
	cairo_rectangle (draw->cr, x+VTE_LINE_WIDTH/2., y+VTE_LINE_WIDTH/2., width-VTE_LINE_WIDTH, height-VTE_LINE_WIDTH);
	set_source_color_alpha (draw->cr, color, alpha);
	cairo_set_line_width (draw->cr, VTE_LINE_WIDTH);
	cairo_stroke (draw->cr);
}

void
_vte_draw_fill_rectangle (struct _vte_draw *draw,
			 gint x, gint y, gint width, gint height,
			 const PangoColor *color, guchar alpha)
{
	g_return_if_fail (draw->started);

	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_fill_rectangle (%d, %d, %d, %d, color=(%d,%d,%d,%d))\n",
			x,y,width,height,
			color->red, color->green, color->blue,
			alpha);

	cairo_set_operator (draw->cr, CAIRO_OPERATOR_OVER);
	cairo_rectangle (draw->cr, x, y, width, height);
	set_source_color_alpha (draw->cr, color, alpha);
	cairo_fill (draw->cr);
}

gboolean
_vte_draw_requires_clear (struct _vte_draw *draw)
{
	return draw->requires_clear;
}
