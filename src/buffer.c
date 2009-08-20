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

#include <config.h>
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

#ifndef G_DISABLE_ASSERT
static void
_vte_buffer_check(VteBuffer *buffer, size_t length)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	if (length > 0) {
		g_assert(buf->bytes != NULL);
	}
	g_assert(buf->buf_length >= length);
	g_assert(buf->buf_length >= buf->buf_used);
}
#else
#define _vte_buffer_check(b, len)
#endif

static size_t
_vte_buffer_calc_new_size(size_t minimum_length)
{
	return minimum_length + VTE_BUFFER_FUDGE_SIZE;
}

VteBuffer*
_vte_buffer_new(void)
{
	struct _vte_real_buffer *buf;
	buf = g_slice_new(struct _vte_real_buffer);
	buf->buf_used = buf->buf_length = 0;
	buf->bytes = NULL;
	return (VteBuffer*) buf;
}

void
_vte_buffer_set_minimum_size(VteBuffer *buffer, size_t length)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	size_t size;
	unsigned char *tmp;
	_vte_buffer_check(buffer, 0);
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
_vte_buffer_append(VteBuffer *buffer,
		   gconstpointer bytes, size_t length)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	size_t size;
	unsigned char *tmp;
	_vte_buffer_check(buffer, 0);
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
_vte_buffer_consume(VteBuffer *buffer, size_t length)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	_vte_buffer_check(buffer, length);
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
_vte_buffer_clear(VteBuffer *buffer)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	_vte_buffer_check(buffer, 0);
	buf->buf_used = 0;
}

void
_vte_buffer_free(VteBuffer *buffer)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	_vte_buffer_check(buffer, 0);
	g_free(buf->bytes);
	g_slice_free(struct _vte_real_buffer, buf);
}

size_t
_vte_buffer_length(VteBuffer *buffer)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	_vte_buffer_check(buffer, 0);
	return buf->buf_used;
}
