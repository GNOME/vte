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

#ifndef vtestream_h_included
#define vtestream_h_included

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _VteStream VteStream;

void _vte_stream_reset (VteStream *stream, gsize offset);
gsize _vte_stream_append (VteStream *stream, const char *data, gsize len);
gboolean _vte_stream_read (VteStream *stream, gsize offset, char *data, gsize len);
void _vte_stream_truncate (VteStream *stream, gsize offset);
void _vte_stream_new_page (VteStream *stream);
gsize _vte_stream_head (VteStream *stream);
gboolean _vte_stream_write_contents (VteStream *stream, GOutputStream *output,
				     gsize start_offset,
				     GCancellable *cancellable, GError **error);


/* Various streams */

VteStream *
_vte_file_stream_new (void);

G_END_DECLS

#endif
