/*
 * Copyright 2009 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA
 *
 */

/* The interfaces in this file are subject to change at any time. */

#ifndef vte_buffer_h_included
#define vte_buffer_h_included


#include <glib.h>

G_BEGIN_DECLS

typedef GByteArray VteBuffer;

#define _vte_buffer_new				g_byte_array_new
#define _vte_buffer_free(B)			g_byte_array_free (B, TRUE)
#define _vte_buffer_append(B, data, length)	g_byte_array_append (B, (const guint8 *) (data), length)
#define _vte_buffer_length(B)			((B)->len)
#define _vte_buffer_consume(B, length)		g_byte_array_remove_range (B, 0, length)
#define _vte_buffer_clear(B)			g_byte_array_set_size (B, 0)
#define _vte_buffer_set_minimum_size(B, length)	g_byte_array_set_size (B, MAX ((length), (B)->len))

G_END_DECLS

#endif
