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
void _vte_buffer_free(struct _vte_buffer *buffer);
void _vte_buffer_prepend(struct _vte_buffer *buffer,
			 const unsigned char *bytes, size_t length);
void _vte_buffer_append(struct _vte_buffer *buffer,
			const unsigned char *bytes, size_t length);
size_t _vte_buffer_length(struct _vte_buffer *buffer);
void _vte_buffer_consume(struct _vte_buffer *buffer, size_t length);
void _vte_buffer_clear(struct _vte_buffer *buffer);
void _vte_buffer_set_minimum_size(struct _vte_buffer *buffer, size_t length);

G_END_DECLS

#endif
