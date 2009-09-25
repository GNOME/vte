/*
 * Copyright (C) 2009 Red Hat, Inc.
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
 */

#include <string.h>
#include <unistd.h>
#include <errno.h>

static gsize
_xread (int fd, char *data, gsize len)
{
	gsize ret, total = 0;

	if (G_UNLIKELY (len && !fd))
		return 0;

	while (len) {
		ret = read (fd, data, len);
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
		total += ret;
	}
	return total;
}

static void
_xwrite (int fd, const char *data, gsize len)
{
	gsize ret;

	g_assert (fd || !len);

	while (len) {
		ret = write (fd, data, len);
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
	}
}

static void
_xtruncate (gint fd, gsize offset)
{
	int ret;

	if (G_UNLIKELY (!fd))
		return;

	do {
		ret = ftruncate (fd, offset);
	} while (ret == -1 && errno == EINTR);
}


/*
 * VteFileStream: A POSIX file-based stream
 */

typedef struct _VteFileStream {
	VteStream parent;

	/* The first fd/offset is for the write head, second is for last page */
	gint fd[2];
	gsize offset[2];
} VteFileStream;

typedef VteStreamClass VteFileStreamClass;

static GType _vte_file_stream_get_type (void);
#define VTE_TYPE_FILE_STREAM _vte_file_stream_get_type ()

G_DEFINE_TYPE (VteFileStream, _vte_file_stream, VTE_TYPE_STREAM)

static void
_vte_file_stream_init (VteFileStream *stream G_GNUC_UNUSED)
{
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

	if (stream->fd[0]) close (stream->fd[0]);
	if (stream->fd[1]) close (stream->fd[1]);

	G_OBJECT_CLASS (_vte_file_stream_parent_class)->finalize(object);
}

static inline void
_vte_file_stream_ensure_fd0 (VteFileStream *stream)
{
	gint fd;
	gchar *file_name;
	if (G_LIKELY (stream->fd[0]))
		return;

	fd = g_file_open_tmp ("vteXXXXXX", &file_name, NULL);
	if (fd != -1) {
		unlink (file_name);
		g_free (file_name);
	}

	stream->fd[0] = dup (fd); /* we do the dup to make sure ->fd[0] is not 0 */

	close (fd);
}

static void
_vte_file_stream_reset (VteStream *astream, gsize offset)
{
	VteFileStream *stream = (VteFileStream *) astream;

	if (stream->fd[0]) _xtruncate (stream->fd[0], 0);
	if (stream->fd[1]) _xtruncate (stream->fd[1], 0);

	stream->offset[0] = stream->offset[1] = offset;
}

static gsize
_vte_file_stream_append (VteStream *astream, const char *data, gsize len)
{
	VteFileStream *stream = (VteFileStream *) astream;
	gsize ret;

	_vte_file_stream_ensure_fd0 (stream);

	ret = lseek (stream->fd[0], 0, SEEK_END);
	_xwrite (stream->fd[0], data, len);

	return stream->offset[0] + ret;
}

static gboolean
_vte_file_stream_read (VteStream *astream, gsize offset, char *data, gsize len)
{
	VteFileStream *stream = (VteFileStream *) astream;
	gsize l;

	if (G_UNLIKELY (offset < stream->offset[1]))
		return FALSE;

	if (offset < stream->offset[0]) {
		lseek (stream->fd[1], offset - stream->offset[1], SEEK_SET);
		l = _xread (stream->fd[1], data, len);
		offset += l; data += l; len -= l; if (!len) return TRUE;
	}

	lseek (stream->fd[0], offset - stream->offset[0], SEEK_SET);
	l = _xread (stream->fd[0], data, len);
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
		_xtruncate (stream->fd[1], 0);
		stream->offset[1] = offset;
	}

	if (G_UNLIKELY (offset < stream->offset[0])) {
		_xtruncate (stream->fd[0], 0);
		stream->offset[0] = stream->offset[1];
		_vte_file_stream_swap_fds (stream);
	} else {
		_xtruncate (stream->fd[0], offset - stream->offset[0]);
	}
}

static void
_vte_file_stream_new_page (VteStream *astream)
{
	VteFileStream *stream = (VteFileStream *) astream;

	stream->offset[1] = stream->offset[0];
	if (stream->fd[0])
		stream->offset[0] += lseek (stream->fd[0], 0, SEEK_END);
	_vte_file_stream_swap_fds (stream);
	_xtruncate (stream->fd[0], 0);
}

static gsize
_vte_file_stream_head (VteStream *astream)
{
	VteFileStream *stream = (VteFileStream *) astream;

	if (stream->fd[0])
		return stream->offset[0] + lseek (stream->fd[0], 0, SEEK_END);
	else
		return stream->offset[0];
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
