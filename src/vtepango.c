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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include "debug.h"
#include "vtebg.h"
#include "vtedraw.h"
#include "vtepango.h"

struct _vte_pango_data
{
	GdkPixmap *pixmap;
	gint pixmapw, pixmaph;
	PangoContext *ctx;
	PangoFontDescription *font;
	PangoLayout *layout;
	GdkGC *gc;
};

static void
_vte_pango_create(struct _vte_draw *draw, GtkWidget *widget)
{
	struct _vte_pango_data *data;

	draw->impl_data = g_slice_new(struct _vte_pango_data);
	data = (struct _vte_pango_data*) draw->impl_data;

	data->pixmap = NULL;
	data->pixmapw = data->pixmaph = 0;
	data->font = NULL;
	data->layout = NULL;
	data->gc = NULL;
}

static void
_vte_pango_destroy(struct _vte_draw *draw)
{
	struct _vte_pango_data *data = (struct _vte_pango_data*) draw->impl_data;

	if (data->pixmap != NULL) {
		g_object_unref(data->pixmap);
	}
	if (data->font != NULL) {
		pango_font_description_free(data->font);
	}
	if (data->layout != NULL) {
		g_object_unref(data->layout);
	}
	if (data->gc != NULL) {
		g_object_unref(data->gc);
	}

	g_slice_free (struct _vte_pango_data, draw->impl_data);
}

static void
_vte_pango_start(struct _vte_draw *draw)
{
	struct _vte_pango_data *data = (struct _vte_pango_data*) draw->impl_data;
	PangoContext *ctx;

	ctx = gtk_widget_get_pango_context (draw->widget);

	if (data->layout != NULL) {
		g_object_unref(data->layout);
	}
	data->layout = pango_layout_new(ctx);
	if (data->font != NULL) {
		pango_layout_set_font_description(data->layout, data->font);
	}

	if (data->gc != NULL) {
		g_object_unref(data->gc);
	}
	data->gc = gdk_gc_new(draw->widget->window);

	gdk_rgb_find_color(gdk_drawable_get_colormap(draw->widget->window),
			   &draw->bg_color);
}

static void
_vte_pango_end(struct _vte_draw *draw)
{
	struct _vte_pango_data *data = (struct _vte_pango_data*) draw->impl_data;

	if (data->gc != NULL) {
		g_object_unref(data->gc);
	}
	data->gc = NULL;

	if (data->layout != NULL) {
		g_object_unref(data->layout);
	}
	data->layout = NULL;
}

static void
_vte_pango_set_background_image(struct _vte_draw *draw,
				enum VteBgSourceType type,
				GdkPixbuf *pixbuf,
				const char *file,
				const GdkColor *color,
				double saturation)
{
	struct _vte_pango_data *data = (struct _vte_pango_data*) draw->impl_data;
	GdkPixmap *pixmap;
	GdkScreen *screen;

	screen = gtk_widget_get_screen(draw->widget);

	pixmap = vte_bg_get_pixmap(vte_bg_get_for_screen(screen),
				   type, pixbuf, file,
				   color, saturation,
				   _vte_draw_get_colormap(draw, TRUE));
	if (data->pixmap != NULL) {
		g_object_unref(data->pixmap);
	}
	data->pixmap = NULL;
	data->pixmapw = data->pixmaph = 0;
	if (pixmap) {
		data->pixmap = pixmap;
		gdk_drawable_get_size(pixmap, &data->pixmapw, &data->pixmaph);
	}
}

static void
_vte_pango_clip(struct _vte_draw *draw, GdkRegion *region)
{
	struct _vte_pango_data *data = draw->impl_data;
	gdk_gc_set_clip_region(data->gc, region);
}

static void
_vte_pango_clear(struct _vte_draw *draw,
		 gint x, gint y, gint width, gint height)
{
	struct _vte_pango_data *data = (struct _vte_pango_data*) draw->impl_data;
	gint i, j, h, w, xstop, ystop;

	if ((data->pixmap == NULL) ||
	    (data->pixmapw == 0) ||
	    (data->pixmaph == 0)) {
		gdk_gc_set_foreground(data->gc, &draw->bg_color);
		gdk_draw_rectangle(draw->widget->window,
				   data->gc,
				   TRUE,
				   x, y, width, height);
		return;
	}

	/* Flood fill. */
	xstop = x + width;
	ystop = y + height;

	y = ystop - height;
	j = (draw->scrolly + y) % data->pixmaph;
	while (y < ystop) {
		x = xstop - width;
		i = (draw->scrollx + x) % data->pixmapw;
		h = MIN(data->pixmaph - (j % data->pixmaph), ystop - y);
		while (x < xstop) {
			w = MIN(data->pixmapw - (i % data->pixmapw), xstop - x);
			gdk_draw_drawable(draw->widget->window,
					  data->gc,
					  data->pixmap,
					  i, j,
					  x, y,
					  w, h);
			x += w;
			i = 0;
		}
		y += h;
		j = 0;
	}
}

static void
_vte_pango_set_text_font(struct _vte_draw *draw,
			 const PangoFontDescription *fontdesc,
			 VteTerminalAntiAlias antialias)
{
	struct _vte_pango_data *data = (struct _vte_pango_data*) draw->impl_data;
	PangoContext *ctx;
	PangoLayout *layout;
	PangoLayoutIter *iter;
	PangoRectangle ink, logical;
	gunichar full_codepoints[] = {VTE_DRAW_DOUBLE_WIDE_IDEOGRAPHS};
	GString *full_string;
	gint full_width;
	guint i;

	ctx = gtk_widget_get_pango_context(draw->widget);
	layout = pango_layout_new(ctx);
	if (data->font != NULL) {
		pango_font_description_free(data->font);
	}
	data->font = pango_font_description_copy(fontdesc);
	pango_layout_set_font_description(layout, data->font);

	/* Estimate for ASCII characters. */
	pango_layout_set_text(layout,
			      VTE_DRAW_SINGLE_WIDE_CHARACTERS,
			      strlen(VTE_DRAW_SINGLE_WIDE_CHARACTERS));
	pango_layout_get_extents(layout, &ink, &logical);
	draw->width = logical.width;
	draw->width = howmany(draw->width,
			      strlen(VTE_DRAW_SINGLE_WIDE_CHARACTERS));
	iter = pango_layout_get_iter(layout);
	draw->height = PANGO_PIXELS(logical.height);
	draw->ascent = PANGO_PIXELS(pango_layout_iter_get_baseline(iter));
	pango_layout_iter_free(iter);

	/* Estimate for CJK characters. */
	full_width = draw->width * 2;
	full_string = g_string_new(NULL);
	for (i = 0; i < G_N_ELEMENTS(full_codepoints); i++) {
		g_string_append_unichar(full_string, full_codepoints[i]);
	}
	pango_layout_set_text(layout, full_string->str, full_string->len);
	pango_layout_get_extents(layout, &ink, &logical);
	full_width = howmany(logical.width, G_N_ELEMENTS(full_codepoints));
	g_string_free(full_string, TRUE);

	/* If they're the same, then we have a screwy font. */
	if (full_width == draw->width) {
		/* add 1 to round up when dividing by 2 */
		draw->width = (draw->width + 1) / 2;
	}

	draw->width = PANGO_PIXELS(draw->width);
	iter = pango_layout_get_iter(layout);
	if (draw->height == 0) {
		draw->height = PANGO_PIXELS(logical.height);
	}
	if (draw->ascent == 0) {
		draw->ascent = PANGO_PIXELS(pango_layout_iter_get_baseline(iter));
	}
	pango_layout_iter_free(iter);

	_vte_debug_print(VTE_DEBUG_MISC,
			"VtePango font metrics = %dx%d (%d).\n",
			draw->width, draw->height, draw->ascent);

	g_object_unref(layout);
}

static void
_vte_pango_draw_text(struct _vte_draw *draw,
		     struct _vte_draw_text_request *requests, gsize n_requests,
		     GdkColor *color, guchar alpha)
{
	struct _vte_pango_data *data = (struct _vte_pango_data*) draw->impl_data;
	char buf[VTE_UTF8_BPC];
	guint i;
	gsize length;
	GdkColor wcolor;


	wcolor = *color;
	gdk_rgb_find_color(gdk_drawable_get_colormap(draw->widget->window),
			   &wcolor);
	gdk_gc_set_foreground(data->gc, &wcolor);

	for (i = 0; i < n_requests; i++) {
		length = g_unichar_to_utf8(requests[i].c, buf);
		pango_layout_set_text(data->layout, buf, length);
		gdk_draw_layout(draw->widget->window, data->gc,
				requests[i].x, requests[i].y,
				data->layout);
	}
}

static gboolean
_vte_pango_draw_has_char(struct _vte_draw *draw, gunichar c)
{
#if defined(PANGO_VERSION_CHECK) && PANGO_VERSION_CHECK(1,15,4)
	struct _vte_pango_data *data = (struct _vte_pango_data*) draw->impl_data;
	char buf[VTE_UTF8_BPC];
	gsize length;

	length = g_unichar_to_utf8(c, buf);
	pango_layout_set_text(data->layout, buf, length);

	return pango_layout_get_unknown_glyphs_count (data->layout) == 0;
#else
	return FALSE;
#endif
}

static void
_vte_pango_draw_rectangle(struct _vte_draw *draw,
			  gint x, gint y, gint width, gint height,
			  GdkColor *color, guchar alpha)
{
	struct _vte_pango_data *data = (struct _vte_pango_data*) draw->impl_data;
	GdkColor wcolor;

	wcolor = *color;
	gdk_rgb_find_color(gdk_drawable_get_colormap(draw->widget->window),
			   &wcolor);
	gdk_gc_set_foreground(data->gc, &wcolor);

	gdk_draw_rectangle(draw->widget->window, data->gc, FALSE,
			   x, y, width-1, height-1);
}

static void
_vte_pango_fill_rectangle(struct _vte_draw *draw,
			  gint x, gint y, gint width, gint height,
			  GdkColor *color, guchar alpha)
{
	struct _vte_pango_data *data = (struct _vte_pango_data*) draw->impl_data;
	GdkColor wcolor;

	wcolor = *color;
	gdk_rgb_find_color(gdk_drawable_get_colormap(draw->widget->window),
			   &wcolor);
	gdk_gc_set_foreground(data->gc, &wcolor);
	gdk_draw_rectangle(draw->widget->window, data->gc, TRUE,
			   x, y, width, height);
}

const struct _vte_draw_impl _vte_draw_pango = {
	"pango",
	NULL, /* check */
	_vte_pango_create,
	_vte_pango_destroy,
	NULL, /* get_visual */
	NULL, /* get_colormap */
	_vte_pango_start,
	_vte_pango_end,
	NULL, /* set_background_opacity */
	NULL, /* set_background_color */
	_vte_pango_set_background_image,
	FALSE, /* always_requires_clear */
	_vte_pango_clip,
	_vte_pango_clear,
	_vte_pango_set_text_font,
	NULL, /* get_text_width */
	NULL, /* get_text_height */
	NULL, /* get_text_ascent */
	NULL, /* get_char_width */
	NULL, /* get_using_fontconfig */
	_vte_pango_draw_text,
	NULL, /* draw_char */
	_vte_pango_draw_has_char,
	_vte_pango_draw_rectangle,
	_vte_pango_fill_rectangle,
	NULL /* set_scroll */
};
