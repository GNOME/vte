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
#include <string.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include "debug.h"
#include "vtedraw.h"
#include "vtepango.h"

struct _vte_pango_data
{
	GdkColor color;
	GdkPixmap *pixmap;
	gint pixmapw, pixmaph;
	gint scrollx, scrolly;
	PangoFontDescription *font;
	PangoLayout *layout;
	GdkGC *gc;
};

static gboolean
_vte_pango_check(struct _vte_draw *draw, GtkWidget *widget)
{
	/* We can draw onto any widget. */
	return TRUE;
}

static void
_vte_pango_create(struct _vte_draw *draw, GtkWidget *widget)
{
	struct _vte_pango_data *data;

	draw->impl_data = g_malloc(sizeof(struct _vte_pango_data));
	data = (struct _vte_pango_data*) draw->impl_data;

	data->color.red = 0;
	data->color.green = 0;
	data->color.blue = 0;
	data->pixmap = NULL;
	data->pixmapw = data->pixmaph = 0;
	data->scrollx = data->scrolly = 0;
	data->font = NULL;
	data->layout = NULL;
	data->gc = NULL;
}

static void
_vte_pango_destroy(struct _vte_draw *draw)
{
	struct _vte_pango_data *data;
	data = (struct _vte_pango_data*) draw->impl_data;

	data->scrollx = data->scrolly = 0;

	if (GDK_IS_PIXMAP(data->pixmap)) {
		g_object_unref(G_OBJECT(data->pixmap));
		data->pixmap = NULL;
		data->pixmapw = data->pixmaph = 0;
	}
	if (data->font != NULL) {
		pango_font_description_free(data->font);
		data->font = NULL;
	}
	if (PANGO_IS_LAYOUT(data->layout)) {
		g_object_unref(G_OBJECT(data->layout));
		data->layout = NULL;
	}
	if (GDK_IS_GC(data->gc)) {
		g_object_unref(G_OBJECT(data->gc));
		data->gc = NULL;
	}

	memset(&data->color, 0, sizeof(data->color));

	g_free(draw->impl_data);
}

static void
_vte_pango_start(struct _vte_draw *draw)
{
	GdkScreen *screen;
	PangoContext *ctx;
	struct _vte_pango_data *data;
	data = (struct _vte_pango_data*) draw->impl_data;

	screen = gdk_drawable_get_screen(draw->widget->window);
	ctx = gdk_pango_context_get_for_screen(screen);

	if (PANGO_IS_LAYOUT(data->layout)) {
		g_object_unref(G_OBJECT(data->layout));
	}
	data->layout = pango_layout_new(ctx);
	if (data->font) {
		pango_layout_set_font_description(data->layout, data->font);
	}

	if (GDK_IS_GC(data->gc)) {
		g_object_unref(G_OBJECT(data->gc));
	}
	data->gc = gdk_gc_new(draw->widget->window);

	gdk_rgb_find_color(gdk_drawable_get_colormap(draw->widget->window),
			   &data->color);
}

static void
_vte_pango_end(struct _vte_draw *draw)
{
	struct _vte_pango_data *data;
	data = (struct _vte_pango_data*) draw->impl_data;

	if (PANGO_IS_LAYOUT(data->layout)) {
		g_object_unref(G_OBJECT(data->layout));
	}
	data->layout = NULL;

	if (GDK_IS_GC(data->gc)) {
		g_object_unref(G_OBJECT(data->gc));
	}
	data->gc = NULL;
}

static void
_vte_pango_set_background_color(struct _vte_draw *draw, GdkColor *color)
{
	struct _vte_pango_data *data;
	data = (struct _vte_pango_data*) draw->impl_data;
	data->color = *color;
}

static void
_vte_pango_set_background_pixbuf(struct _vte_draw *draw, GdkPixbuf *pixbuf,
				 gboolean pan, gboolean scroll)
{
	GdkColormap *colormap;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	struct _vte_pango_data *data;

	data = (struct _vte_pango_data*) draw->impl_data;
	if (data->pixmap) {
		g_object_unref(G_OBJECT(data->pixmap));
		data->pixmap = NULL;
		data->pixmapw = data->pixmaph = 0;
	}
	if ((pixbuf != NULL) &&
	    (gdk_pixbuf_get_width(pixbuf) > 0) &&
	    (gdk_pixbuf_get_height(pixbuf) > 0)) {
		colormap = gdk_drawable_get_colormap(draw->widget->window);
		gdk_pixbuf_render_pixmap_and_mask_for_colormap(pixbuf, colormap,
							       &pixmap, &bitmap,
							       0);
		if (bitmap) {
			g_object_unref(G_OBJECT(bitmap));
		}
		if (pixmap) {
			data->pixmap = pixmap;
			data->pixmapw = gdk_pixbuf_get_width(pixbuf);
			data->pixmaph = gdk_pixbuf_get_height(pixbuf);
		}
	}
}

static void
_vte_pango_clear(struct _vte_draw *draw,
		 gint x, gint y, gint width, gint height)
{
	struct _vte_pango_data *data;
	gint i, j, istart, jstart, h, w, xstop, ystop;

	data = (struct _vte_pango_data*) draw->impl_data;

	if ((data->pixmap == NULL) ||
	    (data->pixmapw == 0) ||
	    (data->pixmaph == 0)) {
		gdk_gc_set_foreground(data->gc, &data->color);
		gdk_draw_rectangle(draw->widget->window,
				   data->gc,
				   TRUE,
				   x, y, width, height);
		return;
	}

	/* Determine the origin of the pixmap if x = y = 0. */
	i = data->scrollx % data->pixmapw;
	j = data->scrolly % data->pixmaph;

	/* Adjust the drawing offsets. */
	istart = (i + x) % data->pixmapw;
	jstart = (j + y) % data->pixmaph;

	/* Flood fill. */
	xstop = x + width;
	ystop = y + height;
	j = jstart;
	while (y < ystop) {
		h = MIN(data->pixmaph - (j % data->pixmaph),
			ystop - j);
		i = istart;
		while (x < xstop) {
			w = MIN(data->pixmapw - (i % data->pixmapw),
				xstop - i);
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
			 const PangoFontDescription *fontdesc)
{
	GdkScreen *screen;
	PangoContext *ctx;
	PangoLayout *layout;
	PangoRectangle ink, logical;
	gunichar full_codepoints[] = {VTE_DRAW_DOUBLE_WIDE_CHARACTERS};
	GString *full_string;
	gint full_width;
	int i;
	struct _vte_pango_data *data;

	data = (struct _vte_pango_data*) draw->impl_data;

	screen = gdk_screen_get_default();
	ctx = gdk_pango_context_get_for_screen(screen);
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

	/* Estimate for CJK characters. */
	full_width = draw->width * 2;
	full_string = g_string_new("");
	for (i = 0; i < G_N_ELEMENTS(full_codepoints); i++) {
		g_string_append_unichar(full_string, full_codepoints[i]);
	}
	pango_layout_set_text(layout, full_string->str, full_string->len);
	pango_layout_get_extents(layout, &ink, &logical);
	full_width = howmany(logical.width, G_N_ELEMENTS(full_codepoints));
	g_string_free(full_string, TRUE);

	/* If they're the same, then we have a screwy font. */
	if (full_width == draw->width) {
		draw->width /= 2;
	}

	g_object_unref(G_OBJECT(layout));

	draw->width = howmany(draw->width, PANGO_SCALE);
	draw->height = howmany(logical.height, PANGO_SCALE);
	draw->base = draw->height;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "VtePango font metrics = %dx%d (%d).\n",
			draw->width, draw->height, draw->base);
	}
#endif
}

static int
_vte_pango_get_text_width(struct _vte_draw *draw)
{
	return draw->width;
}

static int
_vte_pango_get_text_height(struct _vte_draw *draw)
{
	return draw->height;
}

static int
_vte_pango_get_text_base(struct _vte_draw *draw)
{
	return draw->base;
}

static void
_vte_pango_draw_text(struct _vte_draw *draw,
		     struct _vte_draw_text_request *requests, gsize n_requests,
		     GdkColor *color, guchar alpha)
{
	struct _vte_pango_data *data;
	char buf[VTE_UTF8_BPC];
	int i;
	gsize length;
	GdkColor wcolor;

	data = (struct _vte_pango_data*) draw->impl_data;

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

static void
_vte_pango_draw_rectangle(struct _vte_draw *draw,
			  gint x, gint y, gint width, gint height,
			  GdkColor *color, guchar alpha)
{
	struct _vte_pango_data *data;
	GdkColor wcolor;

	data = (struct _vte_pango_data*) draw->impl_data;

	wcolor = *color;
	gdk_rgb_find_color(gdk_drawable_get_colormap(draw->widget->window),
			   &wcolor);
	gdk_gc_set_foreground(data->gc, &wcolor);

	gdk_draw_rectangle(draw->widget->window, data->gc, FALSE,
			   x, y, width, height);
}

static void
_vte_pango_fill_rectangle(struct _vte_draw *draw,
			  gint x, gint y, gint width, gint height,
			  GdkColor *color, guchar alpha)
{
	struct _vte_pango_data *data;
	GdkColor wcolor;

	data = (struct _vte_pango_data*) draw->impl_data;
	wcolor = *color;
	gdk_rgb_find_color(gdk_drawable_get_colormap(draw->widget->window),
			   &wcolor);
	gdk_gc_set_foreground(data->gc, &wcolor);
	gdk_draw_rectangle(draw->widget->window, data->gc, TRUE,
			   x, y, width, height);
}

static gboolean
_vte_pango_scroll(struct _vte_draw *draw, gint dx, gint dy)
{
	struct _vte_pango_data *data;
	data = (struct _vte_pango_data*) draw->impl_data;
	if (GDK_IS_WINDOW(draw->widget->window)) {
		gdk_window_scroll(GDK_WINDOW(draw->widget->window), dx, dy);
		data->scrollx += dx;
		data->scrolly += dy;
		return TRUE;
	}
	return FALSE;
}

static void
_vte_pango_set_scroll(struct _vte_draw *draw, gint x, gint y)
{
	struct _vte_pango_data *data;
	data = (struct _vte_pango_data*) draw->impl_data;
	data->scrollx = x;
	data->scrolly = y;
}

struct _vte_draw_impl _vte_draw_pango = {
	"VtePango", "VTE_USE_PANGO",
	_vte_pango_check,
	_vte_pango_create,
	_vte_pango_destroy,
	_vte_pango_start,
	_vte_pango_end,
	_vte_pango_set_background_color,
	_vte_pango_set_background_pixbuf,
	_vte_pango_clear,
	_vte_pango_set_text_font,
	_vte_pango_get_text_width,
	_vte_pango_get_text_height,
	_vte_pango_get_text_base,
	_vte_pango_draw_text,
	_vte_pango_draw_rectangle,
	_vte_pango_fill_rectangle,
	_vte_pango_scroll,
	_vte_pango_set_scroll,
};
