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

#ifndef vte_vte_rgbh_included
#define vte_vte_rgbh_included


#include <gdk/gdk.h>
#include <glib.h>

struct _vte_rgb_buffer {
	guchar *pixels;
	gint width, height, stride;
};

struct _vte_rgb_buffer *_vte_rgb_buffer_new(gint width, gint height);
void _vte_rgb_buffer_free(struct _vte_rgb_buffer *buffer);
void _vte_rgb_buffer_clear(struct _vte_rgb_buffer *buffer);
void _vte_rgb_buffer_resize(struct _vte_rgb_buffer *buffer,
			    gint minimum_width, gint minimum_height);

void _vte_rgb_draw_color_rgb(struct _vte_rgb_buffer *buffer,
			     gint x, gint y, gint width, gint height,
			     guchar r, guchar g, guchar b);
void _vte_rgb_draw_color(struct _vte_rgb_buffer *buffer,
			 gint x, gint y, gint width, gint height,
			 GdkColor *color);
void _vte_rgb_draw_pixbuf(struct _vte_rgb_buffer *buffer,
			  gint x, gint y, gint width, gint height,
			  GdkPixbuf *pixbuf, gint xbias, gint ybias);
void _vte_rgb_draw_on_drawable(GdkDrawable *drawable, GdkGC *gc,
			       gint x, gint y, gint width, gint height,
			       struct _vte_rgb_buffer *buffer,
			       gint xbias, gint ybias);

#endif
