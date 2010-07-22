/*
 * Copyright (C) 2010 Saleem Abdulrasool <compnerd@compnerd.org>
 *
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Library General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef vte_gtk_compat_h_included
#define vte_gtk_compat_h_included

#include <gtk/gtk.h>

G_BEGIN_DECLS

#if GTK_CHECK_VERSION (2, 90, 5)

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

#else

#define VteRegionRectangle GdkRectangle

#endif

#if !GTK_CHECK_VERSION (2, 22, 0)
#define gtk_accessible_get_widget(accessible)           ((accessible)->widget)
#endif

#if !GTK_CHECK_VERSION (2, 20, 0)
#define gtk_widget_get_mapped(widget)                   (GTK_WIDGET_MAPPED ((widget)))
#define gtk_widget_get_realized(widget)                 (GTK_WIDGET_REALIZED ((widget)))
#define gtk_widget_set_realized(widget, state)          ((state) ? GTK_WIDGET_SET_FLAGS ((widget), GTK_REALIZED) : GTK_WIDGET_UNSET_FLAGS ((widget), GTK_REALIZED))
#endif

#if !GTK_CHECK_VERSION (2, 18, 0)
#define gtk_widget_has_focus(widget)                    (GTK_WIDGET_HAS_FOCUS ((widget)))
#define gtk_widget_get_state(widget)                    ((widget)->state)
#define gtk_widget_set_window(widget, wndw)             ((widget)->window = (wndw))
#define gtk_widget_is_drawable(widget)                  (GTK_WIDGET_DRAWABLE ((widget)))
#define gtk_widget_get_allocation(widget, alloc)        (*(alloc) = (widget)->allocation)
#define gtk_widget_set_allocation(widget, alloc)        ((widget)->allocation = *(alloc))
#define gtk_widget_get_double_buffered(widget)          (GTK_WIDGET_DOUBLE_BUFFERED ((widget)))
#endif

G_END_DECLS

#endif

