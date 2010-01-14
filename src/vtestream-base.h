/*
 * Copyright (C) 2009,2010 Red Hat, Inc.
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

#include <glib-object.h>
#include <gio/gio.h>

/*
 * VteStream: Abstract base stream class
 */

struct _VteStream {
	GObject parent;
};

typedef struct _VteStreamClass {
	GObjectClass parent_class;

	void (*reset) (VteStream *stream, gsize offset);
	gsize (*append) (VteStream *stream, const char *data, gsize len);
	gboolean (*read) (VteStream *stream, gsize offset, char *data, gsize len);
	void (*truncate) (VteStream *stream, gsize offset);
	void (*new_page) (VteStream *stream);
	gsize (*head) (VteStream *stream);
	gboolean (*write_contents) (VteStream *stream, GOutputStream *output,
				    gsize start_offset,
				    GCancellable *cancellable, GError **error);
} VteStreamClass;

static GType _vte_stream_get_type (void);
#define VTE_TYPE_STREAM _vte_stream_get_type ()
#define VTE_STREAM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_STREAM, VteStreamClass))

G_DEFINE_ABSTRACT_TYPE (VteStream, _vte_stream, G_TYPE_OBJECT)

static void
_vte_stream_class_init (VteStreamClass *klass G_GNUC_UNUSED)
{
}

static void
_vte_stream_init (VteStream *stream G_GNUC_UNUSED)
{
}

void
_vte_stream_reset (VteStream *stream, gsize offset)
{
	VTE_STREAM_GET_CLASS (stream)->reset (stream, offset);
}

gsize
_vte_stream_append (VteStream *stream, const char *data, gsize len)
{
	return VTE_STREAM_GET_CLASS (stream)->append (stream, data, len);
}

gboolean
_vte_stream_read (VteStream *stream, gsize offset, char *data, gsize len)
{
	return VTE_STREAM_GET_CLASS (stream)->read (stream, offset, data, len);
}

void
_vte_stream_truncate (VteStream *stream, gsize offset)
{
	VTE_STREAM_GET_CLASS (stream)->truncate (stream, offset);
}

void
_vte_stream_new_page (VteStream *stream)
{
	VTE_STREAM_GET_CLASS (stream)->new_page (stream);
}

gsize
_vte_stream_head (VteStream *stream)
{
	return VTE_STREAM_GET_CLASS (stream)->head (stream);
}

gboolean
_vte_stream_write_contents (VteStream *stream, GOutputStream *output,
			    gsize start_offset,
			    GCancellable *cancellable, GError **error)
{
	return VTE_STREAM_GET_CLASS (stream)->write_contents (stream, output,
							      start_offset,
							      cancellable, error);
}
