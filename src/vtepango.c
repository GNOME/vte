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
	GdkColor color;
	GdkPixmap *pixmap;
	gint pixmapw, pixmaph;
	gint scrollx, scrolly;
	PangoContext *ctx;
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

	draw->impl_data = g_slice_new(struct _vte_pango_data);
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
	data->ctx = NULL;
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
	if (PANGO_IS_CONTEXT(data->ctx)) {
		g_object_unref(G_OBJECT(data->ctx));
		data->ctx = NULL;
	}
	if (GDK_IS_GC(data->gc)) {
		g_object_unref(G_OBJECT(data->gc));
		data->gc = NULL;
	}

	memset(&data->color, 0, sizeof(data->color));

	g_slice_free(struct _vte_pango_data, draw->impl_data);
}

static GdkVisual *
_vte_pango_get_visual(struct _vte_draw *draw)
{
	return gtk_widget_get_visual(draw->widget);
}

static GdkColormap *
_vte_pango_get_colormap(struct _vte_draw *draw)
{
	return gtk_widget_get_colormap(draw->widget);
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
	if (PANGO_IS_CONTEXT(data->ctx)) {
		g_object_unref(G_OBJECT(data->ctx));
	}
	data->ctx = ctx;

	if (PANGO_IS_LAYOUT(data->layout)) {
		g_object_unref(G_OBJECT(data->layout));
	}
	data->layout = pango_layout_new(data->ctx);
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

	if (GDK_IS_GC(data->gc)) {
		g_object_unref(G_OBJECT(data->gc));
	}
	data->gc = NULL;

	if (PANGO_IS_LAYOUT(data->layout)) {
		g_object_unref(G_OBJECT(data->layout));
	}
	data->layout = NULL;

	if (PANGO_IS_CONTEXT(data->ctx)) {
		g_object_unref(G_OBJECT(data->ctx));
	}
	data->ctx = NULL;
}

static void
_vte_pango_set_background_color(struct _vte_draw *draw, GdkColor *color)
{
	struct _vte_pango_data *data;
	data = (struct _vte_pango_data*) draw->impl_data;
	data->color = *color;
}

static void
_vte_pango_set_background_image(struct _vte_draw *draw,
				enum VteBgSourceType type,
				GdkPixbuf *pixbuf,
				const char *file,
				const GdkColor *color,
				double saturation)
{
	GdkPixmap *pixmap;
	struct _vte_pango_data *data;
	GdkScreen *screen;

	screen = gtk_widget_get_screen(draw->widget);

	data = (struct _vte_pango_data*) draw->impl_data;
	pixmap = vte_bg_get_pixmap(vte_bg_get_for_screen(screen),
				   type, pixbuf, file,
				   color, saturation,
				   _vte_draw_get_colormap(draw, TRUE));
	if (GDK_IS_PIXMAP(data->pixmap)) {
		g_object_unref(G_OBJECT(data->pixmap));
	}
	data->pixmap = NULL;
	data->pixmapw = data->pixmaph = 0;
	if (pixmap) {
		data->pixmap = pixmap;
		gdk_drawable_get_size(pixmap, &data->pixmapw, &data->pixmaph);
	}
}

static void
_vte_pango_clear(struct _vte_draw *draw,
		 gint x, gint y, gint width, gint height)
{
	struct _vte_pango_data *data;
	gint i, j, h, w, xstop, ystop;

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

	/* Flood fill. */
	xstop = x + width;
	ystop = y + height;

	y = ystop - height;
	j = (data->scrolly + y) % data->pixmaph;
	while (y < ystop) {
		x = xstop - width;
		i = (data->scrollx + x) % data->pixmapw;
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
	GdkScreen *screen;
	PangoContext *ctx;
	PangoLayout *layout;
	PangoLayoutIter *iter;
	PangoRectangle ink, logical;
	gunichar full_codepoints[] = {VTE_DRAW_DOUBLE_WIDE_CHARACTERS};
	GString *full_string;
	gint full_width;
	int i;
	struct _vte_pango_data *data;

	data = (struct _vte_pango_data*) draw->impl_data;

	if (gtk_widget_has_screen(draw->widget)) {
		screen = gtk_widget_get_screen(draw->widget);
	} else {
		screen = gdk_display_get_default_screen(gtk_widget_get_display(draw->widget));
	}
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
		draw->width /= 2;
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

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "VtePango font metrics = %dx%d (%d).\n",
			draw->width, draw->height, draw->ascent);
	}
#endif

	g_object_unref(G_OBJECT(layout));
	g_object_unref(G_OBJECT(ctx));
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
_vte_pango_get_text_ascent(struct _vte_draw *draw)
{
	return draw->ascent;
}

static int
_vte_pango_get_char_width(struct _vte_draw *draw, gunichar c, int columns)
{
	return _vte_pango_get_text_width(draw) * columns;
}

static gboolean
_vte_pango_get_using_fontconfig(struct _vte_draw *draw)
{
	if (getenv("GDK_USE_XFT") != NULL) {
		return atoi(getenv("GDK_USE_XFT")) != 0;
	}
	return TRUE;
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

static gboolean
_vte_pango_draw_char(struct _vte_draw *draw,
		     struct _vte_draw_text_request *request,
		     GdkColor *color, guchar alpha)
{
	_vte_pango_draw_text(draw, request, 1, color, alpha);
	return TRUE;
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
	_vte_pango_get_visual,
	_vte_pango_get_colormap,
	_vte_pango_start,
	_vte_pango_end,
	_vte_pango_set_background_color,
	_vte_pango_set_background_image,
	FALSE,
	_vte_pango_clear,
	_vte_pango_set_text_font,
	_vte_pango_get_text_width,
	_vte_pango_get_text_height,
	_vte_pango_get_text_ascent,
	_vte_pango_get_char_width,
	_vte_pango_get_using_fontconfig,
	_vte_pango_draw_text,
	_vte_pango_draw_char,
	_vte_pango_draw_rectangle,
	_vte_pango_fill_rectangle,
	_vte_pango_set_scroll,
};
