/*
 * Copyright (C) 2003 Red Hat, Inc.
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
#include <gdk/gdkx.h>
#include <glib.h>
#include <fontconfig/fontconfig.h>
#include <X11/Xft/Xft.h>
#include "debug.h"
#include "vtebg.h"
#include "vtedraw.h"
#include "vtefc.h"
#include "vtexft.h"
#include "vtetree.h"
#include "vte-private.h"

#include <glib/gi18n-lib.h>

#define FONT_INDEX_FUDGE 1
#define CHAR_WIDTH_FUDGE 1

/* libXft will accept runs up to 1024 glyphs before allocating a temporary
 * array. However, setting this to a large value can cause dramatic slow-downs
 * for some xservers (notably fglrx), see bug 410534.
 * Also setting it larger than VTE_DRAW_MAX_LENGTH is nonsensical, as the
 * higher layers will not submit runs longer than that value.
 */
#define MAX_RUN_LENGTH 80

struct _vte_xft_font {
	guint ref;
	Display *display;
	GPtrArray *patterns;
	GPtrArray *fonts;
	VteTree *fontmap;
	VteTree *widths;
	guint last_pattern;

	gint width, height, ascent;
	gboolean have_metrics;
};

struct _vte_xft_data {
	struct _vte_xft_font *font;
	Display *display;
	Drawable drawable;
	int x_offs, y_offs;
	Visual *visual;
	Colormap colormap;
	XftDraw *draw;
	GdkColor color;
	guint16 opacity;
	GdkPixmap *pixmap;
	Pixmap xpixmap;
	gint pixmapw, pixmaph;
	gint scrollx, scrolly;
	GPtrArray *locked_fonts[2];
	guint cur_locked_fonts;
	gboolean has_clip_mask;
};

/* protected by gdk_mutex */
static GHashTable *font_cache;

static int
_vte_xft_direct_compare (gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT (a) - GPOINTER_TO_INT (b);
}

static gboolean
_vte_xft_char_exists (struct _vte_xft_font *font, XftFont *ftfont, FcChar32 c)
{
	return XftCharExists (font->display, ftfont, c) == FcTrue;
}

static void
_vte_xft_text_extents (struct _vte_xft_font *font, XftFont *ftfont, FcChar32 c,
		      XGlyphInfo *extents)
{
	XftTextExtents32 (font->display, ftfont, &c, 1, extents);
}

static guint
_vte_xft_font_hash (struct _vte_xft_font *font)
{
	guint v, i;

	v = GPOINTER_TO_UINT (font->display);
	for (i = 0; i < font->patterns->len; i++) {
		v ^= FcPatternHash (g_ptr_array_index (font->patterns, i));
	}
	return v;
}

static gboolean
_vte_xft_font_equal (struct _vte_xft_font *a, struct _vte_xft_font *b)
{
	guint i;

	if (a->display != b->display) {
		return FALSE;
	}
	if (a->patterns->len != b->patterns->len) {
		return FALSE;
	}
	for (i = 0; i < a->patterns->len; i++) {
		if (!FcPatternEqual (g_ptr_array_index (a->patterns, i),
					g_ptr_array_index (b->patterns, i))) {
			return FALSE;
		}
	}
	return TRUE;
}

static struct _vte_xft_font *
_vte_xft_font_open (GtkWidget *widget, const PangoFontDescription *fontdesc,
		   VteTerminalAntiAlias antialias)
{
	struct _vte_xft_font *font, *old;
	GPtrArray *patterns;

	patterns = g_ptr_array_new ();
	if (!_vte_fc_patterns_from_pango_font_desc (widget, fontdesc, antialias,
						   patterns, NULL, NULL)) {
		g_ptr_array_free (patterns, TRUE);
		return NULL;
	}

	font = g_slice_new (struct _vte_xft_font);
	font->ref = 1;
	font->display = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (widget));
	font->patterns = patterns;
	font->last_pattern = 0;
	font->have_metrics = FALSE;

	if (font_cache == NULL) {
		font_cache = g_hash_table_new (
				(GHashFunc) _vte_xft_font_hash,
				(GEqualFunc) _vte_xft_font_equal);
	}
	old = g_hash_table_lookup (font_cache, font);
	if (old) {
		guint i;
		for (i = 0; i < font->patterns->len; i++) {
			FcPatternDestroy (g_ptr_array_index (font->patterns,i));
		}
		g_ptr_array_free (font->patterns, TRUE);
		g_slice_free (struct _vte_xft_font, font);
		font = old;
		font->ref++;
	} else {
		g_hash_table_insert (font_cache, font, font);
		font->fonts = g_ptr_array_new ();
		g_ptr_array_add (font->fonts, NULL); /* 1 indexed array */
		font->fontmap = _vte_tree_new (_vte_xft_direct_compare);
		font->widths = _vte_tree_new (_vte_xft_direct_compare);
	}

	return font;
}

static void
_vte_xft_font_close (struct _vte_xft_font *font)
{
	XftFont *ftfont;
	guint i;

	if (--font->ref) {
		return;
	}
	g_hash_table_remove (font_cache, font);

	for (i = 0; i < font->patterns->len; i++) {
		FcPatternDestroy (g_ptr_array_index (font->patterns, i));
	}
	g_ptr_array_free (font->patterns, TRUE);

	for (i = 0; i < font->fonts->len; i++) {
		ftfont = g_ptr_array_index (font->fonts, i);
		if (ftfont != NULL) {
			XftFontClose (font->display, ftfont);
		}
	}
	g_ptr_array_free (font->fonts, TRUE);

	_vte_tree_destroy (font->fontmap);
	_vte_tree_destroy (font->widths);

	g_slice_free (struct _vte_xft_font, font);

	if (g_hash_table_size (font_cache) == 0) {
		g_hash_table_destroy (font_cache);
		font_cache = NULL;
	}
}

static XftFont *
_vte_xft_open_font_for_char (struct _vte_xft_font *font, gunichar c, GPtrArray *locked_fonts)
{
	gpointer p = GINT_TO_POINTER (c);
	guint i, j;
	XftFont *ftfont;

	/* Look the character up in the fonts we have. */
	for (i = 1; i < font->fonts->len; i++) {
		ftfont = g_ptr_array_index (font->fonts, i);
		if (_vte_xft_char_exists (font, ftfont, c)) {
			if (g_ptr_array_index (locked_fonts, i) == NULL) {
				XftLockFace (ftfont);
				g_ptr_array_index (locked_fonts, i) = ftfont;
			}
			_vte_tree_insert (font->fontmap,
					p, GINT_TO_POINTER (i));
			return ftfont;
		}
	}

	/* Look the character up in other fonts. */
	for (j = font->last_pattern; j < font->patterns->len; j++) {
		FcPattern *pattern = g_ptr_array_index (font->patterns, j);
		FcPatternReference (pattern);
		ftfont = XftFontOpenPattern (font->display, pattern);
		/* If the font was opened, it takes a ref to the pattern. */
		if (ftfont != NULL) {
			g_ptr_array_add (font->fonts, ftfont);
			if (_vte_xft_char_exists (font, ftfont, c)) {
				XftLockFace (ftfont);
				g_ptr_array_index (locked_fonts, i) = ftfont;
				_vte_tree_insert (font->fontmap,
						p, GINT_TO_POINTER (i));
				font->last_pattern = j;
				return ftfont;
			}
			i++;
		} else {
			FcPatternDestroy (pattern);
		}
	}
	font->last_pattern = j;

	/* No match? */
	_vte_tree_insert (font->fontmap,
			p, GINT_TO_POINTER (-FONT_INDEX_FUDGE));
	_vte_debug_print (VTE_DEBUG_MISC,
			"Can not find appropriate font for character U+%04x.\n",
			c);
	return NULL;
}
static inline XftFont *
_vte_xft_font_for_char (struct _vte_xft_font *font, gunichar c, GPtrArray *locked_fonts)
{
	guint i;
	XftFont *ftfont;

	/* Check if we have a char-to-font entry for it. */
	i = GPOINTER_TO_INT (_vte_tree_lookup (
				font->fontmap, GINT_TO_POINTER (c)));
	if (G_LIKELY (i != 0)) {
		switch (i) {
		/* Checked before, no luck. */
		case -FONT_INDEX_FUDGE:
			return NULL;
		/* Matched before. */
		default:
			ftfont = g_ptr_array_index (font->fonts, i);
			if (g_ptr_array_index (locked_fonts, i) == NULL) {
				XftLockFace (ftfont);
				g_ptr_array_index (locked_fonts, i) = ftfont;
			}
			return ftfont;
		}
	} else
		return _vte_xft_open_font_for_char (font, c, locked_fonts);
}

static gint _vte_xft_compute_char_width (struct _vte_xft_font *font, XftFont *ftfont, gunichar c, int columns)
{
	XGlyphInfo extents;
	gint width;

	/* Compute and store the width. */
	memset (&extents, 0, sizeof (extents));
	if (ftfont != NULL) {
		_vte_xft_text_extents (font, ftfont, c, &extents);
	}
	if (extents.xOff == 0 || extents.xOff == font->width * columns) {
		width = -CHAR_WIDTH_FUDGE;
	} else {
		width = extents.xOff;
	}
	_vte_tree_insert (font->widths,
			GINT_TO_POINTER (c), GINT_TO_POINTER (width));
	return extents.xOff;
}
static inline gint
_vte_xft_char_width (struct _vte_xft_font *font, XftFont *ftfont, gunichar c, int columns)
{
	gint width;

	/* Check if we have a char-to-width entry for it. */
	width = GPOINTER_TO_INT (_vte_tree_lookup (
				font->widths, GINT_TO_POINTER (c)));
	if (G_LIKELY (width != 0)) {
		switch (width) {
		case -CHAR_WIDTH_FUDGE:
			return 0;
		default:
			return width;
		}
	} else
		return _vte_xft_compute_char_width (font, ftfont, c, columns);

}

static gboolean
_vte_xft_check (struct _vte_draw *draw, GtkWidget *widget)
{
	/* We can draw onto any widget. */
	return TRUE;
}

static void
_vte_xft_create (struct _vte_draw *draw, GtkWidget *widget)
{
	struct _vte_xft_data *data;

	data = g_slice_new0 (struct _vte_xft_data);
	draw->impl_data = data;

	data->opacity = 0xffff;

	data->xpixmap = -1;
	data->pixmapw = data->pixmaph = -1;

	data->drawable = -1;
}

static void
_vte_xft_unlock_fonts (struct _vte_xft_data *data)
{
	guint i, j;
	for (i = 0; i < G_N_ELEMENTS (data->locked_fonts); i++) {
		for (j = 1; j < data->locked_fonts[i]->len; j++) {
			XftFont *ftfont = g_ptr_array_index (
					data->locked_fonts[i], j);
			if (ftfont != NULL) {
				XftUnlockFace (ftfont);
			}
		}
		g_ptr_array_free (data->locked_fonts[i], TRUE);
		data->locked_fonts[i] = NULL;
	}
}
static void
_vte_xft_destroy (struct _vte_draw *draw)
{
	struct _vte_xft_data *data;
	data = (struct _vte_xft_data*) draw->impl_data;
	if (data->font != NULL) {
		_vte_xft_unlock_fonts (data);
		_vte_xft_font_close (data->font);
	}
	if (data->draw != NULL) {
		XftDrawDestroy (data->draw);
	}
	g_slice_free (struct _vte_xft_data, data);
}

static GdkVisual *
_vte_xft_get_visual (struct _vte_draw *draw)
{
	return gtk_widget_get_visual (draw->widget);
}

static GdkColormap *
_vte_xft_get_colormap (struct _vte_draw *draw)
{
	return gtk_widget_get_colormap (draw->widget);
}

static void
_vte_xft_start (struct _vte_draw *draw)
{
	GdkDrawable *drawable;
	GPtrArray *locked_fonts;
	guint i;

	struct _vte_xft_data *data;
	data = (struct _vte_xft_data*) draw->impl_data;

	gdk_error_trap_push ();

	gdk_window_get_internal_paint_info (draw->widget->window,
					   &drawable,
					   &data->x_offs,
					   &data->y_offs);
	if (data->drawable != gdk_x11_drawable_get_xid (drawable)) {
		GdkVisual *gvisual;
		GdkColormap *gcolormap;

		if (data->draw != NULL) {
			XftDrawDestroy (data->draw);
		}
		data->display = gdk_x11_drawable_get_xdisplay (drawable);
		data->drawable = gdk_x11_drawable_get_xid (drawable);
		gvisual = gdk_drawable_get_visual (drawable);
		data->visual = gdk_x11_visual_get_xvisual (gvisual);
		gcolormap = gdk_drawable_get_colormap (drawable);
		data->colormap = gdk_x11_colormap_get_xcolormap (gcolormap);
		data->draw = XftDrawCreate (data->display, data->drawable,
				data->visual, data->colormap);
		data->has_clip_mask = FALSE;
	}
	g_assert (data->display == data->font->display);

	locked_fonts = data->locked_fonts [++data->cur_locked_fonts&1];
	if (locked_fonts != NULL) {
		guint cnt=0;
		for (i = 1; i < locked_fonts->len; i++) {
			XftFont *ftfont = g_ptr_array_index (locked_fonts, i);
			if (ftfont != NULL) {
				XftUnlockFace (ftfont);
				g_ptr_array_index (locked_fonts, i) = NULL;
				cnt++;
			}
		}
	}
}

static void
_vte_xft_end (struct _vte_draw *draw)
{
	struct _vte_xft_data *data;

	data = (struct _vte_xft_data*) draw->impl_data;

	gdk_error_trap_pop ();
}

static void
_vte_xft_set_background_color (struct _vte_draw *draw, GdkColor *color,
			      guint16 opacity)
{
	struct _vte_xft_data *data;
	data = (struct _vte_xft_data*) draw->impl_data;
	data->color = *color;
	data->opacity = opacity;

	draw->requires_clear = opacity != 0xffff
		|| (data->pixmapw > 0 && data->pixmaph > 0);
}

static void
_vte_xft_set_background_image (struct _vte_draw *draw,
			      enum VteBgSourceType type,
			      GdkPixbuf *pixbuf,
			      const char *file,
			      const GdkColor *color,
			      double saturation)
{
	struct _vte_xft_data *data;
	GdkPixmap *pixmap;
	GdkScreen *screen;

	data = (struct _vte_xft_data*) draw->impl_data;

	screen = gtk_widget_get_screen (draw->widget);

	data->xpixmap = -1;
	data->pixmapw = data->pixmaph = 0;
	pixmap = vte_bg_get_pixmap (vte_bg_get_for_screen (screen), type,
				   pixbuf, file, color, saturation,
				   _vte_draw_get_colormap (draw, TRUE));
	if (data->pixmap != NULL) {
		g_object_unref (data->pixmap);
	}
	draw->requires_clear = data->opacity != 0xffff;
	data->pixmap = NULL;
	if (pixmap != NULL) {
		data->pixmap = pixmap;
		data->xpixmap = gdk_x11_drawable_get_xid (pixmap);
		gdk_drawable_get_size (pixmap, &data->pixmapw, &data->pixmaph);
		draw->requires_clear |=
			data->pixmapw > 0 && data->pixmaph > 0;
	}
}

static void
_vte_xft_clip (struct _vte_draw *draw,
		GdkRegion *region)
{
	struct _vte_xft_data *data = draw->impl_data;
	XRectangle stack_rect[16];
	XRectangle *xrect;
	GdkRectangle *rect;
	gint i, n;

	gdk_region_get_rectangles (region, &rect, &n);
	/* only enable clipping if we have to */
	if (n > 1 ||
			rect[0].width < draw->widget->allocation.width ||
			rect[0].height < draw->widget->allocation.height) {
		xrect = n > (gint) G_N_ELEMENTS (stack_rect) ?
			g_new (XRectangle, n) :
			stack_rect;
		for (i = 0; i < n; i++) {
			xrect[i].x = rect[i].x - data->x_offs;
			xrect[i].y = rect[i].y - data->y_offs;
			xrect[i].width = rect[i].width;
			xrect[i].height = rect[i].height;
		}
		XftDrawSetClipRectangles (data->draw, 0, 0, xrect, n);
		data->has_clip_mask = TRUE;
		if (xrect != stack_rect)
			g_free (xrect);
	} else {
		if (data->has_clip_mask) {
			XftDrawSetClip (data->draw, NULL);
			data->has_clip_mask = FALSE;
		}
	}

	g_free (rect);
}

static void
_vte_xft_clear (struct _vte_draw *draw,
	       gint x, gint y, gint width, gint height)
{
	struct _vte_xft_data *data;
	XRenderColor rcolor;
	XftColor ftcolor;
	gint h, w, txstop, tystop, sx, sy, tx, ty;
	GC gc;

	data = (struct _vte_xft_data*) draw->impl_data;

	if (data->pixmap == NULL ||
	    (data->pixmapw <= 0) ||
	    (data->pixmaph <= 0)) {
		rcolor.red = data->color.red * data->opacity / 0xffff;
		rcolor.green = data->color.green * data->opacity / 0xffff;
		rcolor.blue = data->color.blue * data->opacity / 0xffff;
		rcolor.alpha = data->opacity;
		if (XftColorAllocValue (data->display, data->visual,
				       data->colormap, &rcolor, &ftcolor)) {
			XftDrawRect (data->draw, &ftcolor,
				    x - data->x_offs,
				    y - data->y_offs,
				    width, height);
			XftColorFree (data->display, data->visual,
				     data->colormap, &ftcolor);
		}
		return;
	}

	/* Adjust the drawing offsets. */
	tx = x;
	ty = y;
	txstop = x + width;
	tystop = y + height;

	/* Flood fill. */
	gc = XCreateGC (data->display, data->drawable, 0, NULL);
	sy = (data->scrolly + y) % data->pixmaph;
	while (ty < tystop) {
		h = MIN (data->pixmaph - sy, tystop - ty);
		tx = x;
		sx = (data->scrollx + x) % data->pixmapw;
		while (tx < txstop) {
			w = MIN (data->pixmapw - sx, txstop - tx);
			XCopyArea (data->display,
				  data->xpixmap,
				  data->drawable,
				  gc,
				  sx, sy,
				  w, h,
				  tx - data->x_offs, ty - data->y_offs);
			tx += w;
			sx = 0;
		}
		ty += h;
		sy = 0;
	}
	XFreeGC (data->display, gc);
}

static GPtrArray *
ptr_array_zeroed_new (guint len)
{
	GPtrArray *ptr;
	ptr = g_ptr_array_sized_new (len);
	while (len--) {
		g_ptr_array_add (ptr, NULL);
	}
	return ptr;
}
static void
_vte_xft_set_text_font (struct _vte_draw *draw,
		       const PangoFontDescription *fontdesc,
		       VteTerminalAntiAlias antialias)
{
	struct _vte_xft_data *data;
	struct _vte_xft_font *ft;

	data = (struct _vte_xft_data*) draw->impl_data;

	ft = _vte_xft_font_open (draw->widget, fontdesc, antialias);
	if (ft != NULL) {
		if (data->font != NULL) {
			_vte_xft_unlock_fonts (data);
			_vte_xft_font_close (data->font);
		}
		data->font = ft;
	}
	if (data->font == NULL) {
		return;
	}

	data->locked_fonts[0] = ptr_array_zeroed_new (1 + data->font->patterns->len);
	data->locked_fonts[1] = ptr_array_zeroed_new (1 + data->font->patterns->len);

	if (data->font->have_metrics) {
		draw->width = data->font->width;
		draw->height = data->font->height;
		draw->ascent = data->font->ascent;
	} else {
		XftFont *font, *prev_font;
		XGlyphInfo extents;
		gunichar wide_chars[] = {VTE_DRAW_DOUBLE_WIDE_CHARACTERS};
		guint i;
		gint n, width, height, min = G_MAXINT, max = G_MININT;
		FcChar32 c;
		GPtrArray *locked_fonts;

		draw->width = 1;
		draw->height = 1;
		draw->ascent = 1;

		locked_fonts = data->locked_fonts [data->cur_locked_fonts&1];

		gdk_error_trap_push ();
		n = width = height = 0;
		/* Estimate a typical cell width by looking at single-width
		 * characters. */
		for (i = 0; i < sizeof (VTE_DRAW_SINGLE_WIDE_CHARACTERS) - 1; i++) {
			c = VTE_DRAW_SINGLE_WIDE_CHARACTERS[i];
			font = _vte_xft_font_for_char (data->font, c, locked_fonts);
			if (font != NULL) {
				memset (&extents, 0, sizeof (extents));
				_vte_xft_text_extents (data->font, font, c, &extents);
				n++;
				width += extents.xOff;
				if (extents.xOff < min) {
					min = extents.xOff;
				}
				if (extents.xOff > max) {
					max = extents.xOff;
				}
				if (extents.height > height) {
					height = extents.height;
				}
			}
		}
		if (n > 0) {
			draw->width = howmany (width, n);
			draw->height = (font != NULL) ?
				font->ascent + font->descent : height;
			draw->ascent = (font != NULL) ?
				font->ascent : height;
		}
		/* Estimate a typical cell width by looking at double-width
		 * characters, and if it's the same as the single width, assume the
		 * single-width stuff is broken. */
		n = width = 0;
		prev_font = NULL;
		for (i = 0; i < G_N_ELEMENTS (wide_chars); i++) {
			c = wide_chars[i];
			font = _vte_xft_font_for_char (data->font, c, locked_fonts);
			if (font != NULL) {
				if (n && prev_font != font) {/* font change */
					width = howmany (width, n);
					if (width >= draw->width -1 &&
							width <= draw->width + 1){
						/* add 1 to round up when dividing by 2 */
						draw->width = (draw->width + 1) / 2;
						break;
					}
					n = width = 0;
				}
				memset (&extents, 0, sizeof (extents));
				_vte_xft_text_extents (data->font, font, c, &extents);
				n++;
				width += extents.xOff;
				prev_font = font;
			}
		}
		if (n > 0) {
			width = howmany (width, n);
			if (width >= draw->width -1 &&
					width <= draw->width + 1){
				/* add 1 to round up when dividing by 2 */
				draw->width = (draw->width + 1) / 2;
			}
		}

		gdk_error_trap_pop ();

		data->font->width = draw->width;
		data->font->height = draw->height;
		data->font->ascent = draw->ascent;
		data->font->have_metrics = TRUE;

		_vte_debug_print (VTE_DEBUG_MISC,
				"VteXft font metrics = %dx%d (%d),"
				" width range [%d, %d].\n",
				draw->width, draw->height, draw->ascent,
				min, max);
	}
}

static inline int
_vte_xft_get_text_width (struct _vte_draw *draw)
{
	return draw->width;
}

static inline int
_vte_xft_get_text_height (struct _vte_draw *draw)
{
	return draw->height;
}

static inline int
_vte_xft_get_text_ascent (struct _vte_draw *draw)
{
	return draw->ascent;
}

static int
_vte_xft_get_char_width (struct _vte_draw *draw, gunichar c, int columns)
{
	struct _vte_xft_data *data;
	XftFont *ftfont;
	int width;

	data = (struct _vte_xft_data*) draw->impl_data;
	if (data->font != NULL) {
		ftfont = _vte_xft_font_for_char (data->font, c,
				data->locked_fonts[data->cur_locked_fonts&1]);
		if (ftfont != NULL) {
			width = _vte_xft_char_width (data->font, ftfont, c, columns);
			if (width != 0) {
				return width;
			}
		}
	}
	return _vte_xft_get_text_width (draw) * columns;
}

static gboolean
_vte_xft_get_using_fontconfig (struct _vte_draw *draw)
{
	return TRUE;
}

static void
_vte_xft_draw_text (struct _vte_draw *draw,
		   struct _vte_draw_text_request *requests, gsize n_requests,
		   GdkColor *color, guchar alpha)
{
	XftGlyphSpec glyphs[MAX_RUN_LENGTH];
	XRenderColor rcolor;
	XftColor ftcolor;
	struct _vte_xft_data *data;
	gsize i, j;
	gint width, y_off, x_off, char_width;
	XftFont *font, *ft;
	GPtrArray *locked_fonts;

	data = (struct _vte_xft_data*) draw->impl_data;
	if (G_UNLIKELY (data->font == NULL)){
		return; /* cannot draw anything */
	}
	locked_fonts = data->locked_fonts[data->cur_locked_fonts&1];

	/* find the first displayable character ... */
	font = NULL;
	for (i = 0; i < n_requests; i++) {
		font = _vte_xft_font_for_char (data->font,
				requests[i].c, locked_fonts);
		if (G_UNLIKELY (font == NULL)) {
			continue;
		}
		break;
	}
	if (G_UNLIKELY (i == n_requests)) {
		return; /* nothing to see here, please move along */
	}

	rcolor.red = color->red;
	rcolor.green = color->green;
	rcolor.blue = color->blue;
	rcolor.alpha = (alpha == VTE_DRAW_OPAQUE) ?
		0xffff : (alpha << 8);
	if (!XftColorAllocValue (data->display, data->visual,
				data->colormap, &rcolor, &ftcolor)) {
		return;
	}

	/* split the text into runs of the same font, because
	 * "We need to break down the draw request into runs which use the same
	 * font, to work around a bug which appears to be in Xft and which I
	 * haven't pinned down yet." */
	x_off = -data->x_offs;
	y_off = draw->ascent - data->y_offs;
	char_width = draw->width;
	do {
		j = 0;
		do {
			gint next_x;

			glyphs[j].glyph = XftCharIndex (data->display,
					font, requests[i].c);
			glyphs[j].x = requests[i].x + x_off;
			next_x = requests[i].x + requests[i].columns*char_width;
			width = _vte_xft_char_width (data->font,
					font, requests[i].c, requests[i].columns);
			if (G_UNLIKELY (width != 0)) {
				width = requests[i].columns*char_width - width;
				width = CLAMP (width / 2, 0, char_width);
				glyphs[j].x += width;
			}
			glyphs[j].y = requests[i].y + y_off;
			j++;

			/* find the next displayable character ... */
			ft = NULL;
			while (++i < n_requests) {
				ft = _vte_xft_font_for_char (data->font,
						requests[i].c, locked_fonts);
				if (G_UNLIKELY (ft == NULL)) {
					continue;
				}
				break;
			}
			if (j == MAX_RUN_LENGTH || ft != font) {
				break;
			}

			/* check to see if we've skipped over any spaces...
			 * and reinsert them so as not to break the stream
			 * unnecessarily - the blank space is less overhead
			 * than starting a new sequence.
			 */
			if (requests[i].y + y_off == glyphs[j-1].y) {
				while (next_x < requests[i].x) {
					glyphs[j].glyph = XftCharIndex (
							data->display,
							font,
							' ');
					glyphs[j].x = next_x + x_off;
					glyphs[j].y = glyphs[j-1].y;
					if (++j == MAX_RUN_LENGTH) {
						goto draw;
					}
					next_x += char_width;
				}
			}
		} while (TRUE);
draw:
		XftDrawGlyphSpec (data->draw, &ftcolor, font, glyphs, j);
		font = ft;
	} while (i < n_requests);
	XftColorFree (data->display, data->visual,
			data->colormap, &ftcolor);
}

static gboolean
_vte_xft_draw_char (struct _vte_draw *draw,
		   struct _vte_draw_text_request *request,
		   GdkColor *color, guchar alpha)
{
	struct _vte_xft_data *data;

	data = (struct _vte_xft_data*) draw->impl_data;
	if (data->font != NULL &&
			_vte_xft_font_for_char (data->font, request->c,
				data->locked_fonts[data->cur_locked_fonts&1]) != NULL) {
		_vte_xft_draw_text (draw, request, 1, color, alpha);
		return TRUE;
	}
	return FALSE;
}

static gboolean
_vte_xft_draw_has_char (struct _vte_draw *draw, gunichar c)
{
	struct _vte_xft_data *data;

	data = (struct _vte_xft_data*) draw->impl_data;
	if (data->font != NULL &&
			_vte_xft_font_for_char (data->font, c,
				data->locked_fonts[data->cur_locked_fonts&1]) != NULL) {

		return TRUE;
	}
	return FALSE;
}

static void
_vte_xft_draw_rectangle (struct _vte_draw *draw,
			gint x, gint y, gint width, gint height,
			GdkColor *color, guchar alpha)
{
	struct _vte_xft_data *data;
	XRenderColor rcolor;
	XftColor ftcolor;

	data = (struct _vte_xft_data*) draw->impl_data;

	rcolor.red = color->red;
	rcolor.green = color->green;
	rcolor.blue = color->blue;
	rcolor.alpha = (alpha == VTE_DRAW_OPAQUE) ? 0xffff : alpha << 8;
	if (XftColorAllocValue (data->display, data->visual,
			       data->colormap, &rcolor, &ftcolor)) {
		XftDrawRect (data->draw, &ftcolor,
			    x - data->x_offs, y - data->y_offs,
			    width, 1);
		XftDrawRect (data->draw, &ftcolor,
			    x - data->x_offs, y - data->y_offs,
			    1, height);
		XftDrawRect (data->draw, &ftcolor,
			    x - data->x_offs, y + height - 1 - data->y_offs,
			    width, 1);
		XftDrawRect (data->draw, &ftcolor,
			    x + width - 1 - data->x_offs, y - data->y_offs,
			    1, height);
		XftColorFree (data->display, data->visual, data->colormap,
			     &ftcolor);
	}
}

static void
_vte_xft_fill_rectangle (struct _vte_draw *draw,
			gint x, gint y, gint width, gint height,
			GdkColor *color, guchar alpha)
{
	struct _vte_xft_data *data;
	XRenderColor rcolor;
	XftColor ftcolor;

	data = (struct _vte_xft_data*) draw->impl_data;

	rcolor.red = color->red;
	rcolor.green = color->green;
	rcolor.blue = color->blue;
	rcolor.alpha = (alpha == VTE_DRAW_OPAQUE) ? 0xffff : alpha << 8;
	if (XftColorAllocValue (data->display, data->visual,
			       data->colormap, &rcolor, &ftcolor)) {
		XftDrawRect (data->draw, &ftcolor,
			    x - data->x_offs, y - data->y_offs, width, height);
		XftColorFree (data->display, data->visual, data->colormap,
			     &ftcolor);
	}
}

static void
_vte_xft_set_scroll (struct _vte_draw *draw, gint x, gint y)
{
	struct _vte_xft_data *data;
	data = (struct _vte_xft_data*) draw->impl_data;
	data->scrollx = x;
	data->scrolly = y;
}

const struct _vte_draw_impl _vte_draw_xft = {
	"Xft",
	_vte_xft_check,
	_vte_xft_create,
	_vte_xft_destroy,
	_vte_xft_get_visual,
	_vte_xft_get_colormap,
	_vte_xft_start,
	_vte_xft_end,
	_vte_xft_set_background_color,
	_vte_xft_set_background_image,
	FALSE,
	_vte_xft_clip,
	_vte_xft_clear,
	_vte_xft_set_text_font,
	_vte_xft_get_text_width,
	_vte_xft_get_text_height,
	_vte_xft_get_text_ascent,
	_vte_xft_get_char_width,
	_vte_xft_get_using_fontconfig,
	_vte_xft_draw_text,
	_vte_xft_draw_char,
	_vte_xft_draw_has_char,
	_vte_xft_draw_rectangle,
	_vte_xft_fill_rectangle,
	_vte_xft_set_scroll,
};
