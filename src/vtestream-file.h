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
static gsize
pread (int fd, char *data, gsize len, gsize offset)
{
  if (-1 == lseek (fd, offset, SEEK_SET))
    return -1;
  return read (fd, data, len);
}
#endif

#ifndef HAVE_PWRITE
#define pwrite _pwrite
static gsize
pwrite (int fd, char *data, gsize len, gsize offset)
{
  if (-1 == lseek (fd, offset, SEEK_SET))
    return -1;
  return write (fd, data, len);
}
#endif

static inline void
_xinit (gint *fd)
{
	*fd = -1;
}

static inline gboolean
_xisopen (gint fd)
{
	return fd != -1;
}

static inline void
_xclose (gint fd)
{
	if (G_UNLIKELY (!_xisopen (fd)))
		return;

	close (fd);
}


static void
_xtruncate (gint fd, gsize offset)
{
	if (G_UNLIKELY (!_xisopen (fd)))
		return;

	do { } while (-1 == ftruncate (fd, offset) && errno == EINTR);
}

static void
_xreset (gint fd)
{
	_xtruncate (fd, 0);
}

static gsize
_xpread (int fd, char *data, gsize len, gsize offset)
{
	gsize ret, total = 0;

	if (G_UNLIKELY (!_xisopen (fd)))
		return 0;

	while (len) {
		ret = pread (fd, data, len, offset);
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
_xpwrite (int fd, const char *data, gsize len, gsize offset)
{
	gsize ret;
	gboolean truncated = FALSE;

	g_assert (_xisopen (fd) || !len);

	while (len) {
		ret = pwrite (fd, data, len, offset);
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
				_xtruncate (fd, offset);
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

	/* The first fd/offset is for the write head, second is for last page */
	gint fd[2];
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
	stream->fd[0] = stream->fd[1] = -1;
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

	_xclose (stream->fd[0]);
	_xclose (stream->fd[1]);

	G_OBJECT_CLASS (_vte_file_stream_parent_class)->finalize(object);
}

static inline void
_vte_file_stream_ensure_fd0 (VteFileStream *stream)
{
	gint fd;

	if (G_LIKELY (stream->fd[0] != -1))
		return;

        fd = _vte_mkstemp ();
        if (fd == -1)
                return;

        stream->fd[0] = fd;
}

static void
_vte_file_stream_reset (VteStream *astream, gsize offset)
{
	VteFileStream *stream = (VteFileStream *) astream;

	_xreset (stream->fd[0]);
	_xreset (stream->fd[1]);

	stream->head = stream->offset[0] = stream->offset[1] = offset;
}

static void
_vte_file_stream_append (VteStream *astream, const char *data, gsize len)
{
	VteFileStream *stream = (VteFileStream *) astream;

	_vte_file_stream_ensure_fd0 (stream);

	_xpwrite (stream->fd[0], data, len, stream->head - stream->offset[0]);
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
		l = _xpread (stream->fd[1], data, len, offset - stream->offset[1]);
		offset += l; data += l; len -= l; if (!len) return TRUE;
	}

	l = _xpread (stream->fd[0], data, len, offset - stream->offset[0]);
	offset += l; data += l; len -= l; if (!len) return TRUE;

	return FALSE;
}

static void
_vte_file_stream_swap_fds (VteFileStream *stream)
{
	gint fd;

	fd = stream->fd[0]; stream->fd[0] = stream->fd[1]; stream->fd[1] = fd;
}

static void
_vte_file_stream_truncate (VteStream *astream, gsize offset)
{
	VteFileStream *stream = (VteFileStream *) astream;

	if (G_UNLIKELY (offset < stream->offset[1])) {
		_xreset (stream->fd[1]);
		stream->offset[1] = offset;
	}

	if (G_UNLIKELY (offset < stream->offset[0])) {
		_xreset (stream->fd[0]);
		stream->offset[0] = stream->offset[1];
		_vte_file_stream_swap_fds (stream);
	} else {
		_xtruncate (stream->fd[0], offset - stream->offset[0]);
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
	_xreset (stream->fd[0]);
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
