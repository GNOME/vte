/*
 * Copyright (C) 2009,2010 Red Hat, Inc.
 * Copyright (C) 2013 Google, Inc.
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
 *
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#include <errno.h>

#include "vteutils.h"


#if 1

/*
 * File implementation using Unix syscalls.
 */

#include <unistd.h>

#ifndef HAVE_PREAD
#define pread _pread
static inline gsize
pread (int fd, char *data, gsize len, gsize offset)
{
  if (-1 == lseek (fd, offset, SEEK_SET))
    return -1;
  return read (fd, data, len);
}
#endif

#ifndef HAVE_PWRITE
#define pwrite _pwrite
static inline gsize
pwrite (int fd, char *data, gsize len, gsize offset)
{
  if (-1 == lseek (fd, offset, SEEK_SET))
    return -1;
  return write (fd, data, len);
}
#endif


typedef struct
{
  int fd;
} _file_t;

static inline void
_file_init (_file_t *f)
{
	f->fd = -1;
}

static inline void
_file_open (_file_t *f, int fd)
{
	f->fd = fd;
}

static inline gboolean
_file_isopen (_file_t *f)
{
	return f->fd != -1;
}

static inline void
_file_close (_file_t *f)
{
	if (G_UNLIKELY (!_file_isopen (f)))
		return;

	close (f->fd);
}


static void
_file_try_truncate (_file_t *f, gsize offset)
{
	if (G_UNLIKELY (!_file_isopen (f)))
		return;

	do { } while (-1 == ftruncate (f->fd, offset) && errno == EINTR);
}

static void
_file_reset (_file_t *f)
{
	/* Our try_truncate() actually works. */
	_file_try_truncate (f, 0);
}

static gsize
_file_read (_file_t *f, char *data, gsize len, gsize offset)
{
	gsize ret, total = 0;

	if (G_UNLIKELY (!_file_isopen (f)))
		return 0;

	while (len) {
		ret = pread (f->fd, data, len, offset);
		if (G_UNLIKELY (ret == (gsize) -1)) {
			if (errno == EINTR)
				continue;
			else
				break;
		}
		if (G_UNLIKELY (ret == 0))
			break;
		data += ret;
		len -= ret;
		offset += ret;
		total += ret;
	}
	return total;
}

static void
_file_write (_file_t *f, const char *data, gsize len, gsize offset)
{
	gsize ret;
	gboolean truncated = FALSE;

	g_assert (_file_isopen (f) || !len);

	while (len) {
		ret = pwrite (f->fd, data, len, offset);
		if (G_UNLIKELY (ret == (gsize) -1)) {
			if (errno == EINTR)
				continue;
			else if (errno == EINVAL && !truncated)
			{
				/* Perhaps previous writes failed and now we are
				 * seeking past end of file.  Try extending it
				 * and retry.  This allows recovering from a
				 * "/tmp is full" error.
				 */
				_file_try_truncate (f, offset);
				truncated = TRUE;
				continue;
			}
			else
				break;
		}
		if (G_UNLIKELY (ret == 0))
			break;
		data += ret;
		len -= ret;
		offset += ret;
	}
}

#endif


/*
 * VteFileStream: A file-based stream
 */

typedef struct _VteFileStream {
	VteStream parent;

	/* The first file/offset is for the write head, second is for last page */
	_file_t file[2];
	gsize offset[2];
	gsize head;
} VteFileStream;

typedef VteStreamClass VteFileStreamClass;

static GType _vte_file_stream_get_type (void);
#define VTE_TYPE_FILE_STREAM _vte_file_stream_get_type ()

G_DEFINE_TYPE (VteFileStream, _vte_file_stream, VTE_TYPE_STREAM)

static void
_vte_file_stream_init (VteFileStream *stream)
{
	_file_init (&stream->file[0]);
	_file_init (&stream->file[1]);
}

VteStream *
_vte_file_stream_new (void)
{
	return (VteStream *) g_object_new (VTE_TYPE_FILE_STREAM, NULL);
}

static void
_vte_file_stream_finalize (GObject *object)
{
	VteFileStream *stream = (VteFileStream *) object;

	_file_close (&stream->file[0]);
	_file_close (&stream->file[1]);

	G_OBJECT_CLASS (_vte_file_stream_parent_class)->finalize(object);
}

static inline void
_vte_file_stream_ensure_file0 (VteFileStream *stream)
{
	int fd;

	if (G_LIKELY (_file_isopen (&stream->file[0])))
		return;

        fd = _vte_mkstemp ();
        if (fd == -1)
                return;

        _file_open (&stream->file[0], fd);
}

static void
_vte_file_stream_reset (VteStream *astream, gsize offset)
{
	VteFileStream *stream = (VteFileStream *) astream;

	_file_reset (&stream->file[0]);
	_file_reset (&stream->file[1]);

	stream->head = stream->offset[0] = stream->offset[1] = offset;
}

static void
_vte_file_stream_append (VteStream *astream, const char *data, gsize len)
{
	VteFileStream *stream = (VteFileStream *) astream;

	_vte_file_stream_ensure_file0 (stream);

	_file_write (&stream->file[0], data, len, stream->head - stream->offset[0]);
	stream->head += len;
}

static gboolean
_vte_file_stream_read (VteStream *astream, gsize offset, char *data, gsize len)
{
	VteFileStream *stream = (VteFileStream *) astream;
	gsize l;

	if (G_UNLIKELY (offset < stream->offset[1]))
		return FALSE;

	if (offset < stream->offset[0]) {
		l = _file_read (&stream->file[1], data, len, offset - stream->offset[1]);
		offset += l; data += l; len -= l; if (!len) return TRUE;
	}

	l = _file_read (&stream->file[0], data, len, offset - stream->offset[0]);
	offset += l; data += l; len -= l; if (!len) return TRUE;

	return FALSE;
}

static void
_vte_file_stream_swap_fds (VteFileStream *stream)
{
	_file_t f;

	f = stream->file[0]; stream->file[0] = stream->file[1]; stream->file[1] = f;
}

static void
_vte_file_stream_truncate (VteStream *astream, gsize offset)
{
	VteFileStream *stream = (VteFileStream *) astream;

	if (G_UNLIKELY (offset < stream->offset[1])) {
		_file_reset (&stream->file[1]);
		stream->offset[1] = offset;
	}

	if (G_UNLIKELY (offset < stream->offset[0])) {
		_file_reset (&stream->file[0]);
		stream->offset[0] = stream->offset[1];
		_vte_file_stream_swap_fds (stream);
	} else {
		_file_try_truncate (&stream->file[0], offset - stream->offset[0]);
	}

	stream->head = offset;
}

static void
_vte_file_stream_new_page (VteStream *astream)
{
	VteFileStream *stream = (VteFileStream *) astream;

	stream->offset[1] = stream->offset[0];
	stream->offset[0] = stream->head;
	_vte_file_stream_swap_fds (stream);
	_file_reset (&stream->file[0]);
}

static gsize
_vte_file_stream_head (VteStream *astream, guint _index)
{
	VteFileStream *stream = (VteFileStream *) astream;

	return _index == 0 ? stream->head : stream->offset[_index - 1];
}

static void
_vte_file_stream_class_init (VteFileStreamClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = _vte_file_stream_finalize;

	klass->reset = _vte_file_stream_reset;
	klass->append = _vte_file_stream_append;
	klass->read = _vte_file_stream_read;
	klass->truncate = _vte_file_stream_truncate;
	klass->new_page = _vte_file_stream_new_page;
	klass->head = _vte_file_stream_head;
}
