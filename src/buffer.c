/*
 * Copyright 2001.2002 Red Hat, Inc.
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

#include "../config.h"
#include <sys/types.h>
#include <glib.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"
#define VTE_BUFFER_FUDGE_SIZE 0x1000

struct _vte_real_buffer {
	unsigned char *bytes;
	/* private */
	size_t buf_used, buf_length;
};

static size_t
_vte_buffer_calc_new_size(size_t minimum_length)
{
	return minimum_length + VTE_BUFFER_FUDGE_SIZE;
}

struct _vte_buffer*
_vte_buffer_new(void)
{
	struct _vte_real_buffer *buf;
	buf = g_malloc(sizeof(struct _vte_real_buffer));
	buf->buf_used = buf->buf_length = 0;
	buf->bytes = NULL;
	return (struct _vte_buffer*) buf;
}

void
_vte_buffer_set_minimum_size(struct _vte_buffer *buffer, size_t length)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	size_t size;
	unsigned char *tmp;
	g_assert(buf->buf_length >= buf->buf_used);
	if (length > buf->buf_length) {
		size = _vte_buffer_calc_new_size(length);
		tmp = g_malloc(size);
		if (buf->bytes != NULL) {
			if (buf->buf_used > 0) {
				memcpy(tmp, buf->bytes, buf->buf_used);
			}
			g_free(buf->bytes);
		}
		buf->bytes = tmp;
		buf->buf_length = size;
	}
	if (length > buf->buf_used) {
		buf->buf_used = length;
	}
}

void
_vte_buffer_prepend(struct _vte_buffer *buffer,
		    const unsigned char *bytes, size_t length)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	size_t size;
	unsigned char *tmp;
	g_assert(buf->buf_length >= buf->buf_used);
	if (length > 0) {
		if (buf->buf_used + length > buf->buf_length) {
			size = _vte_buffer_calc_new_size(buf->buf_used +
							 length);
			tmp = g_malloc(size);
			if (buf->bytes != NULL) {
				if (buf->buf_used > 0) {
					memcpy(tmp, buf->bytes, buf->buf_used);
				}
				g_free(buf->bytes);
			}
			buf->bytes = tmp;
			buf->buf_length = size;
		}
		memmove(buf->bytes + length, buf->bytes, buf->buf_used);
		memcpy(buf->bytes, bytes, length);
		buf->buf_used += length;
	}
}

void
_vte_buffer_append(struct _vte_buffer *buffer,
		   const unsigned char *bytes, size_t length)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	size_t size;
	unsigned char *tmp;
	g_assert(buf->buf_length >= buf->buf_used);
	if (length > 0) {
		if (buf->buf_used + length > buf->buf_length) {
			size = _vte_buffer_calc_new_size(buf->buf_used +
							 length);
			tmp = g_malloc(size);
			if (buf->bytes != NULL) {
				if (buf->buf_used > 0) {
					memcpy(tmp, buf->bytes, buf->buf_used);
				}
				g_free(buf->bytes);
			}
			buf->bytes = tmp;
			buf->buf_length = size;
		}
		memcpy(buf->bytes + buf->buf_used, bytes, length);
		buf->buf_used += length;
	}
}

void
_vte_buffer_consume(struct _vte_buffer *buffer, size_t length)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	g_assert(buf->buf_length >= buf->buf_used);
	g_assert(length <= buf->buf_used);
	if (length == buf->buf_used) {
		buf->buf_used = 0;
	} else
	if (length > 0) {
		memmove(buf->bytes, buf->bytes + length,
			buf->buf_used - length);
		buf->buf_used -= length;
	}
}

void
_vte_buffer_clear(struct _vte_buffer *buffer)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	g_assert(buf->buf_length >= buf->buf_used);
	buf->buf_used = 0;
}

void
_vte_buffer_free(struct _vte_buffer *buffer)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	g_assert(buf->buf_length >= buf->buf_used);
	g_free(buf->bytes);
	g_free(buf);
}

size_t
_vte_buffer_length(struct _vte_buffer *buffer)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	g_assert(buf->buf_length >= buf->buf_used);
	return buf->buf_used;
}
