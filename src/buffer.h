/*
 * Copyright 2001,2002 Red Hat, Inc.
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


#include <sys/types.h>

G_BEGIN_DECLS

struct _vte_buffer {
	/* public */
	unsigned char *bytes;
	/* private stuff is hidden */
};

struct _vte_buffer* _vte_buffer_new(void);
struct _vte_buffer* _vte_buffer_new_with_data(gconstpointer data,
					      size_t length);
void _vte_buffer_free(struct _vte_buffer *buffer);
void _vte_buffer_prepend(struct _vte_buffer *buffer,
			 gconstpointer bytes, size_t length);
void _vte_buffer_append(struct _vte_buffer *buffer,
			gconstpointer bytes, size_t length);
size_t _vte_buffer_length(struct _vte_buffer *buffer);
void _vte_buffer_consume(struct _vte_buffer *buffer, size_t length);
void _vte_buffer_clear(struct _vte_buffer *buffer);
void _vte_buffer_set_minimum_size(struct _vte_buffer *buffer, size_t length);

void _vte_buffer_append_guint16(struct _vte_buffer *buffer, guint16 i);
guint16 _vte_buffer_peek_guint16(struct _vte_buffer *buffer);
guint16 _vte_buffer_read_guint16(struct _vte_buffer *buffer);

void _vte_buffer_append_guint32(struct _vte_buffer *buffer, guint32 i);
guint32 _vte_buffer_peek_guint32(struct _vte_buffer *buffer);
guint32 _vte_buffer_read_guint32(struct _vte_buffer *buffer);

void _vte_buffer_append_gstring(struct _vte_buffer *buffer, const GString *s);
GString *_vte_buffer_peek_gstring(struct _vte_buffer *buffer);
GString * _vte_buffer_read_gstring(struct _vte_buffer *buffer);

void _vte_buffer_append_buffer(struct _vte_buffer *buffer,
			       struct _vte_buffer *s);
void _vte_buffer_append_buffer_contents(struct _vte_buffer *buffer,
					struct _vte_buffer *s);
struct _vte_buffer *_vte_buffer_peek_buffer(struct _vte_buffer *buffer);
struct _vte_buffer *_vte_buffer_read_buffer(struct _vte_buffer *buffer);

G_END_DECLS

#endif
