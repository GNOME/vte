/*
 * Copyright (C) 2010 Saleem Abdulrasool <compnerd@compnerd.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef vte_gtk_compat_h_included
#define vte_gtk_compat_h_included

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GdkRegion cairo_region_t
#define VteRegionRectangle cairo_rectangle_int_t
#define gdk_region_new() cairo_region_create()
#define gdk_region_rectangle(r) cairo_region_create_rectangle(r)
#define gdk_region_copy(r) cairo_region_copy(r)
#define gdk_region_destroy cairo_region_destroy
#define gdk_region_union_with_rect(r, rect) cairo_region_union_rectangle(r, rect)
#define gdk_region_union(r, s) cairo_region_union(r, s)
#define gdk_region_get_clipbox(r, rect) cairo_region_get_extents(r, rect)
#define gdk_region_get_rectangles(r, rects, n_rects)			\
	do {								\
		int i;							\
									\
		*(n_rects) = cairo_region_num_rectangles(r);		\
		*(rects) = g_new(cairo_rectangle_int_t, *(n_rects));	\
		for (i = 0; i < *(n_rects); i++)			\
			cairo_region_get_rectangle ((r), i, &(*(rects))[i]); \
	} while (0)

G_END_DECLS

#endif

