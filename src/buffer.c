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

#ident "$Id$"
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

static void
_vte_buffer_check(struct _vte_buffer *buffer, size_t length)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	g_assert(buf->buf_length >= length);
	g_assert(buf->buf_length >= buf->buf_used);
}

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

struct _vte_buffer*
_vte_buffer_new_with_data(gconstpointer data, size_t length)
{
	struct _vte_buffer *buf;
	buf = _vte_buffer_new();
	_vte_buffer_append(buf, data, length);
	return buf;
}

void
_vte_buffer_set_minimum_size(struct _vte_buffer *buffer, size_t length)
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
_vte_buffer_prepend(struct _vte_buffer *buffer,
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
		memmove(buf->bytes + length, buf->bytes, buf->buf_used);
		memcpy(buf->bytes, bytes, length);
		buf->buf_used += length;
	}
}

void
_vte_buffer_append(struct _vte_buffer *buffer,
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
_vte_buffer_consume(struct _vte_buffer *buffer, size_t length)
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
_vte_buffer_clear(struct _vte_buffer *buffer)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	_vte_buffer_check(buffer, 0);
	buf->buf_used = 0;
}

void
_vte_buffer_free(struct _vte_buffer *buffer)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	_vte_buffer_check(buffer, 0);
	g_free(buf->bytes);
	g_free(buf);
}

size_t
_vte_buffer_length(struct _vte_buffer *buffer)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) buffer;
	_vte_buffer_check(buffer, 0);
	return buf->buf_used;
}

void
_vte_buffer_append_guint16(struct _vte_buffer *buffer, guint16 i)
{
	guint16 j;
	j = g_htons(i);
	_vte_buffer_append(buffer, (gpointer) &j, sizeof(j));
}

guint16
_vte_buffer_peek_guint16(struct _vte_buffer *buffer)
{
	guint16 i;
	_vte_buffer_check(buffer, sizeof(i));
	memcpy(&i, buffer->bytes, sizeof(i));
	return g_ntohs(i);
}

guint16
_vte_buffer_read_guint16(struct _vte_buffer *buffer)
{
	guint16 ret;
	ret = _vte_buffer_peek_guint16(buffer);
	_vte_buffer_consume(buffer, sizeof(ret));
	return ret;
}

void
_vte_buffer_append_guint32(struct _vte_buffer *buffer, guint32 i)
{
	guint32 j;
	j = g_htonl(i);
	_vte_buffer_append(buffer, (gpointer) &j, sizeof(j));
}

guint32
_vte_buffer_peek_guint32(struct _vte_buffer *buffer)
{
	guint32 i;
	_vte_buffer_check(buffer, sizeof(i));
	memcpy(&i, buffer->bytes, sizeof(i));
	return g_ntohl(i);
}

guint32
_vte_buffer_read_guint32(struct _vte_buffer *buffer)
{
	guint32 ret;
	ret = _vte_buffer_peek_guint32(buffer);
	_vte_buffer_consume(buffer, sizeof(ret));
	return ret;
}

void
_vte_buffer_append_gstring(struct _vte_buffer *buffer, const GString *s)
{
	_vte_buffer_append_guint32(buffer, s->len);
	_vte_buffer_append(buffer, s->str, s->len);
}

GString *
_vte_buffer_peek_gstring(struct _vte_buffer *buffer)
{
	GString *ret;
	guint32 i;
	i = _vte_buffer_peek_guint32(buffer);
	_vte_buffer_check(buffer, sizeof(i) + i);
	ret = g_string_new_len(buffer->bytes + sizeof(i), i);
	return ret;
}

GString *
_vte_buffer_read_gstring(struct _vte_buffer *buffer)
{
	GString *ret;
	ret = _vte_buffer_peek_gstring(buffer);
	_vte_buffer_consume(buffer, sizeof(guint32) + ret->len);
	return ret;
}

void
_vte_buffer_append_buffer(struct _vte_buffer *buffer, struct _vte_buffer *s)
{
	struct _vte_real_buffer *buf = (struct _vte_real_buffer*) s;
	_vte_buffer_append_guint32(buffer, buf->buf_used);
	_vte_buffer_append(buffer, buf->bytes, buf->buf_used);
}

struct _vte_buffer *
_vte_buffer_peek_buffer(struct _vte_buffer *buffer)
{
	struct _vte_buffer *ret;
	guint32 i;
	i = _vte_buffer_peek_guint32(buffer);
	_vte_buffer_check(buffer, sizeof(i) + i);
	ret = _vte_buffer_new_with_data(buffer->bytes + sizeof(i), i);
	return ret;
}

struct _vte_buffer *
_vte_buffer_read_buffer(struct _vte_buffer *buffer)
{
	struct _vte_buffer *ret;
	guint32 i;
	i = _vte_buffer_read_guint32(buffer);
	_vte_buffer_check(buffer, i);
	ret = _vte_buffer_new_with_data(buffer->bytes, i);
	_vte_buffer_consume(buffer, i);
	return ret;
}
