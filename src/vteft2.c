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
#include <glib.h>
#include <fontconfig/fontconfig.h>
#include "debug.h"
#include "vtebg.h"
#include "vtedraw.h"
#include "vtefc.h"
#include "vteglyph.h"
#include "vtergb.h"

#define FONT_INDEX_FUDGE 10
#define CHAR_WIDTH_FUDGE 10

struct _vte_ft2_data
{
	struct _vte_glyph_cache *cache;
	struct _vte_rgb_buffer *rgb;
	GdkColor color;
	GdkPixbuf *pixbuf;
	gint scrollx, scrolly;
	gint left, right, top, bottom;
};

static gboolean
_vte_ft2_check(struct _vte_draw *draw, GtkWidget *widget)
{
	/* We can draw onto any widget. */
	return TRUE;
}

static void
_vte_ft2_create(struct _vte_draw *draw, GtkWidget *widget)
{
	draw->impl_data = g_slice_new0(struct _vte_ft2_data);
}

static void
_vte_ft2_destroy(struct _vte_draw *draw)
{
	struct _vte_ft2_data *data;
	data = (struct _vte_ft2_data*) draw->impl_data;
	if (data->cache != NULL) {
		_vte_glyph_cache_free(data->cache);
	}
	if (data->rgb != NULL) {
		_vte_rgb_buffer_free(data->rgb);
	}
	if (data->pixbuf != NULL) {
		g_object_unref(data->pixbuf);
	}
	g_slice_free(struct _vte_ft2_data, data);
}

static GdkVisual *
_vte_ft2_get_visual(struct _vte_draw *draw)
{
	return gtk_widget_get_visual(draw->widget);
}

static GdkColormap *
_vte_ft2_get_colormap(struct _vte_draw *draw)
{
	return gtk_widget_get_colormap(draw->widget);
}

static void
_vte_ft2_start(struct _vte_draw *draw)
{
	struct _vte_ft2_data *data;
	guint width, height;
	data = (struct _vte_ft2_data*) draw->impl_data;

	width = draw->widget->allocation.width;
	height = draw->widget->allocation.height;
	if (data->rgb == NULL) {
		data->rgb = _vte_rgb_buffer_new(width, height);
	} else {
		_vte_rgb_buffer_resize(data->rgb, width, height);
	}
	data->left = data->top = G_MAXINT;
	data->right = data->bottom = -G_MAXINT;
}

static void
_vte_ft2_end(struct _vte_draw *draw)
{
	struct _vte_ft2_data *data;
	GtkWidget *widget;
	GtkStateType state;
	data = (struct _vte_ft2_data*) draw->impl_data;
	widget = draw->widget;
	state = GTK_WIDGET_STATE(widget);
	if (data->right < data->left) {
		_vte_rgb_draw_on_drawable(widget->window,
					  widget->style->fg_gc[state],
					  0, 0,
						widget->allocation.width,
						widget->allocation.height,
					  data->rgb,
					  0, 0);
	} else {
		_vte_rgb_draw_on_drawable(widget->window,
					  widget->style->fg_gc[state],
					  data->left, data->top,
					  data->right - data->left + 1,
					  data->bottom - data->top + 1,
					  data->rgb,
					  data->left, data->top);
	}
	gdk_gc_set_clip_region(widget->style->fg_gc[state], NULL);
}

static void
_vte_ft2_set_background_color(struct _vte_draw *draw, GdkColor *color, guint16 opacity)
{
	struct _vte_ft2_data *data;
	data = (struct _vte_ft2_data*) draw->impl_data;
	data->color = *color;
}

static void
_vte_ft2_set_background_image(struct _vte_draw *draw,
			      enum VteBgSourceType type,
			      GdkPixbuf *pixbuf,
			      const char *file,
			      const GdkColor *color,
			      double saturation)
{
	struct _vte_ft2_data *data;
	GdkPixbuf *bgpixbuf;
	GdkScreen *screen;

	screen = gtk_widget_get_screen(draw->widget);

	data = (struct _vte_ft2_data*) draw->impl_data;

	bgpixbuf = vte_bg_get_pixbuf(vte_bg_get_for_screen(screen),
				     type, pixbuf, file,
				     color, saturation);
	if (data->pixbuf != NULL) {
		g_object_unref(data->pixbuf);
	}
	data->pixbuf = bgpixbuf;
	draw->requires_clear = bgpixbuf != NULL;
}

static void
_vte_ft2_clip(struct _vte_draw *draw, GdkRegion *region)
{
	gdk_gc_set_clip_region(
			draw->widget->style->fg_gc[GTK_WIDGET_STATE(draw->widget)],
			region);
}

static inline void
update_bbox(struct _vte_ft2_data *data, gint x, gint y, gint width, gint height)
{
	if (x < data->left) data->left = x;
	x += width - 1;
	if (x > data->right) data->right = x;

	if (y < data->top) data->top = y;
	y += height - 1;
	if (y > data->bottom) data->bottom = y;
}

static void
_vte_ft2_clear(struct _vte_draw *draw,
	       gint x, gint y, gint width, gint height)
{
	struct _vte_ft2_data *data;
	data = (struct _vte_ft2_data*) draw->impl_data;

	if (data->pixbuf != NULL) {
		/* Tile a pixbuf in. */
		_vte_rgb_draw_pixbuf(data->rgb, x, y, width, height,
				     data->pixbuf,
				     data->scrollx + x, data->scrolly + y);
	} else {
		/* The simple case is a solid color. */
		_vte_rgb_draw_color(data->rgb, x, y, width, height,
				    &data->color);
	}
	update_bbox(data, x, y, width, height);
}

static void
_vte_ft2_set_text_font(struct _vte_draw *draw,
		       const PangoFontDescription *fontdesc,
		       VteTerminalAntiAlias anti_alias)
{
	struct _vte_ft2_data *data;

	data = (struct _vte_ft2_data*) draw->impl_data;

	if (data->cache != NULL) {
		_vte_glyph_cache_free(data->cache);
	}
	data->cache = _vte_glyph_cache_new();
	_vte_glyph_cache_set_font_description(draw->widget, NULL,
					      data->cache, fontdesc, anti_alias,
					      NULL, NULL);
	_vte_debug_print(VTE_DEBUG_MISC,
			"VteFT2 font metrics = %ldx%ld (%ld).\n",
			data->cache->width,
			data->cache->height,
			data->cache->ascent);
}

static int
_vte_ft2_get_text_width(struct _vte_draw *draw)
{
	struct _vte_ft2_data *data;
	data = (struct _vte_ft2_data*) draw->impl_data;
	return data->cache->width;
}

static int
_vte_ft2_get_text_height(struct _vte_draw *draw)
{
	struct _vte_ft2_data *data;
	data = (struct _vte_ft2_data*) draw->impl_data;
	return data->cache->height;
}

static int
_vte_ft2_get_text_ascent(struct _vte_draw *draw)
{
	struct _vte_ft2_data *data;
	data = (struct _vte_ft2_data*) draw->impl_data;
	return data->cache->ascent;
}

static int
_vte_ft2_get_char_width(struct _vte_draw *draw, gunichar c, int columns)
{
	struct _vte_ft2_data *data;
	const struct _vte_glyph *glyph;

	data = (struct _vte_ft2_data*) draw->impl_data;

	glyph = _vte_glyph_get(data->cache, c);
	if (glyph != NULL) {
		return glyph->width;
	}

	return _vte_ft2_get_text_width(draw) * columns;
}

static gboolean
_vte_ft2_get_using_fontconfig(struct _vte_draw *draw)
{
	return TRUE;
}

static void
_vte_ft2_draw_text(struct _vte_draw *draw,
		   struct _vte_draw_text_request *requests, gsize n_requests,
		   GdkColor *color, guchar alpha)
{
	struct _vte_ft2_data *data;
	gsize i, j;

	data = (struct _vte_ft2_data*) draw->impl_data;

	for (i = 0; i < n_requests; i++) {
		if (requests[i].c == (gunichar)-1 ||
			       	requests[i].c == 32 /* space */)
			continue;
		_vte_glyph_draw(data->cache, requests[i].c, color,
				requests[i].x, requests[i].y,
				requests[i].columns,
				0,
				data->rgb);
		update_bbox(data, requests[i].x, requests[i].y,
			    data->cache->width * requests[i].columns,
			    data->cache->height);
		for (j = i + 1; j < n_requests; j++) {
			if (requests[j].c == requests[i].c) {
				_vte_rgb_copy(data->rgb,
						requests[i].x, requests[i].y,
						requests[i].columns * data->cache->width, data->cache->height,
						requests[j].x, requests[j].y);
				update_bbox(data, requests[j].x, requests[j].y,
						data->cache->width * requests[j].columns,
						data->cache->height);
				requests[j].c = -1;
			}
		}
	}
}

static gboolean
_vte_ft2_draw_char(struct _vte_draw *draw,
		   struct _vte_draw_text_request *request,
		   GdkColor *color, guchar alpha)
{
	struct _vte_ft2_data *data;

	data = (struct _vte_ft2_data*) draw->impl_data;

	if (data->cache != NULL) {
		if (_vte_glyph_get(data->cache, request->c) != NULL) {
			_vte_ft2_draw_text(draw, request, 1, color, alpha);
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean
_vte_ft2_draw_has_char(struct _vte_draw *draw, gunichar c)
{
	struct _vte_ft2_data *data;

	data = (struct _vte_ft2_data*) draw->impl_data;

	if (data->cache != NULL) {
		if (_vte_glyph_get(data->cache, c) != NULL) {
			return TRUE;
		}
	}
	return FALSE;
}

static void
_vte_ft2_draw_rectangle(struct _vte_draw *draw,
			gint x, gint y, gint width, gint height,
			GdkColor *color, guchar alpha)
{
	struct _vte_ft2_data *data;

	data = (struct _vte_ft2_data*) draw->impl_data;

	_vte_rgb_draw_color(data->rgb,
			    x, y,
			    width, 1,
			    color);
	_vte_rgb_draw_color(data->rgb,
			    x, y,
			    1, height,
			    color);
	_vte_rgb_draw_color(data->rgb,
			    x, y + height - 1,
			    width, 1,
			    color);
	_vte_rgb_draw_color(data->rgb,
			    x + width - 1, y,
			    1, height,
			    color);
	update_bbox(data, x, y, width, height);
}

static void
_vte_ft2_fill_rectangle(struct _vte_draw *draw,
			gint x, gint y, gint width, gint height,
			GdkColor *color, guchar alpha)
{
	struct _vte_ft2_data *data;

	data = (struct _vte_ft2_data*) draw->impl_data;

	_vte_rgb_draw_color(data->rgb, x, y, width, height, color);
	update_bbox(data, x, y, width, height);
}

static void
_vte_ft2_set_scroll(struct _vte_draw *draw, gint x, gint y)
{
	struct _vte_ft2_data *data;
	data = (struct _vte_ft2_data*) draw->impl_data;
	data->scrollx = x;
	data->scrolly = y;
}

const struct _vte_draw_impl _vte_draw_ft2 = {
	"FT2",
	_vte_ft2_check,
	_vte_ft2_create,
	_vte_ft2_destroy,
	_vte_ft2_get_visual,
	_vte_ft2_get_colormap,
	_vte_ft2_start,
	_vte_ft2_end,
	_vte_ft2_set_background_color,
	_vte_ft2_set_background_image,
	FALSE,
	_vte_ft2_clip,
	_vte_ft2_clear,
	_vte_ft2_set_text_font,
	_vte_ft2_get_text_width,
	_vte_ft2_get_text_height,
	_vte_ft2_get_text_ascent,
	_vte_ft2_get_char_width,
	_vte_ft2_get_using_fontconfig,
	_vte_ft2_draw_text,
	_vte_ft2_draw_char,
	_vte_ft2_draw_has_char,
	_vte_ft2_draw_rectangle,
	_vte_ft2_fill_rectangle,
	_vte_ft2_set_scroll,
};
