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
#include "debug.h"
#include "vtedraw.h"

struct _vte_skel_data
{
	GdkColor color;
	GdkPixmap *pixmap;
	gint pixmapw, pixmaph;
	gint scrollx, scrolly;
};

static gboolean
_vte_skel_check(struct _vte_draw *draw, GtkWidget *widget)
{
	/* Implement me first. */
	return FALSE;
}

static void
_vte_skel_create(struct _vte_draw *draw, GtkWidget *widget)
{
	struct _vte_skel_data *data;

	draw->impl_data = g_malloc(sizeof(struct _vte_skel_data));
	data = (struct _vte_skel_data*) draw->impl_data;

	data->color.red = 0;
	data->color.green = 0;
	data->color.blue = 0;
	data->pixmap = NULL;
	data->pixmapw = data->pixmaph = 0;
	data->scrollx = data->scrolly = 0;
}

static void
_vte_skel_destroy(struct _vte_draw *draw)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;

	data->scrollx = data->scrolly = 0;

	if (GDK_IS_PIXMAP(data->pixmap)) {
		g_object_unref(G_OBJECT(data->pixmap));
		data->pixmap = NULL;
		data->pixmapw = data->pixmaph = 0;
	}

	memset(&data->color, 0, sizeof(data->color));

	g_free(draw->impl_data);
}

static void
_vte_skel_start(struct _vte_draw *draw)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
	gdk_rgb_find_color(gdk_drawable_get_colormap(draw->widget->window),
			   &data->color);
}

static void
_vte_skel_end(struct _vte_draw *draw)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
}

static void
_vte_skel_set_background_color(struct _vte_draw *draw, GdkColor *color)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
	data->color = *color;
}

static void
_vte_skel_set_background_pixbuf(struct _vte_draw *draw, GdkPixbuf *pixbuf,
				 gboolean pan, gboolean scroll)
{
	GdkColormap *colormap;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	struct _vte_skel_data *data;

	data = (struct _vte_skel_data*) draw->impl_data;
}

static void
_vte_skel_clear(struct _vte_draw *draw,
		 gint x, gint y, gint width, gint height)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
}

static void
_vte_skel_set_text_font(struct _vte_draw *draw,
			 const PangoFontDescription *fontdesc)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
}

static int
_vte_skel_get_text_width(struct _vte_draw *draw)
{
	return draw->width;
}

static int
_vte_skel_get_text_height(struct _vte_draw *draw)
{
	return draw->height;
}

static int
_vte_skel_get_text_base(struct _vte_draw *draw)
{
	return draw->base;
}

static void
_vte_skel_draw_text(struct _vte_draw *draw,
		     struct _vte_draw_text_request *requests, gsize n_requests,
		     GdkColor *color, guchar alpha)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
}

static void
_vte_skel_draw_rectangle(struct _vte_draw *draw,
			  gint x, gint y, gint width, gint height,
			  GdkColor *color, guchar alpha)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
}

static void
_vte_skel_fill_rectangle(struct _vte_draw *draw,
			  gint x, gint y, gint width, gint height,
			  GdkColor *color, guchar alpha)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
}

static gboolean
_vte_skel_scroll(struct _vte_draw *draw, gint dx, gint dy)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
	return FALSE;
}

static void
_vte_skel_set_scroll(struct _vte_draw *draw, gint x, gint y)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
	data->scrollx = x;
	data->scrolly = y;
}

struct _vte_draw_impl _vte_draw_skel = {
	"VteSkel", "VTE_USE_SKEL",
	_vte_skel_check,
	_vte_skel_create,
	_vte_skel_destroy,
	_vte_skel_start,
	_vte_skel_end,
	_vte_skel_set_background_color,
	_vte_skel_set_background_pixbuf,
	_vte_skel_clear,
	_vte_skel_set_text_font,
	_vte_skel_get_text_width,
	_vte_skel_get_text_height,
	_vte_skel_get_text_base,
	_vte_skel_draw_text,
	_vte_skel_draw_rectangle,
	_vte_skel_fill_rectangle,
	_vte_skel_scroll,
	_vte_skel_set_scroll,
};
