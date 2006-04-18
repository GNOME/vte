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
#include "vtebg.h"
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

	draw->impl_data = g_slice_new(struct _vte_skel_data);
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

	g_slice_free(struct _vte_skel_data, draw->impl_data);
}

static GdkVisual *
_vte_skel_get_visual(struct _vte_draw *draw)
{
	return gtk_widget_get_visual(draw->widget);
}

static GdkColormap *
_vte_skel_get_colormap(struct _vte_draw *draw)
{
	return gtk_widget_get_colormap(draw->widget);
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
_vte_skel_set_background_image(struct _vte_draw *draw,
			       enum VteBgSourceType type,
			       GdkPixbuf *pixbuf,
			       const char *filename,
			       const GdkColor *tint,
			       double saturation)
{
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
			const PangoFontDescription *fontdesc,
			VteTerminalAntiAlias antialias)
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
_vte_skel_get_text_ascent(struct _vte_draw *draw)
{
	return draw->ascent;
}

static int
_vte_skel_get_char_width(struct _vte_draw *draw, gunichar c, int columns)
{
	return _vte_skel_get_text_width(draw) * columns;
}

static gboolean
_vte_skel_get_using_fontconfig(struct _vte_draw *draw)
{
	return FALSE;
}

static void
_vte_skel_draw_text(struct _vte_draw *draw,
		    struct _vte_draw_text_request *requests, gsize n_requests,
		    GdkColor *color, guchar alpha)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
}

static gboolean
_vte_skel_draw_char(struct _vte_draw *draw,
		    struct _vte_draw_text_request *request,
		    GdkColor *color, guchar alpha)
{
	struct _vte_skel_data *data;
	data = (struct _vte_skel_data*) draw->impl_data;
	return FALSE;
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
	_vte_skel_get_visual,
	_vte_skel_get_colormap,
	_vte_skel_start,
	_vte_skel_end,
	_vte_skel_set_background_color,
	_vte_skel_set_background_image,
	TRUE,
	_vte_skel_clear,
	_vte_skel_set_text_font,
	_vte_skel_get_text_width,
	_vte_skel_get_text_height,
	_vte_skel_get_text_ascent,
	_vte_skel_get_char_width,
	_vte_skel_get_using_fontconfig,
	_vte_skel_draw_text,
	_vte_skel_draw_char,
	_vte_skel_draw_rectangle,
	_vte_skel_fill_rectangle,
	_vte_skel_set_scroll,
};
