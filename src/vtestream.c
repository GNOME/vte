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

#include <config.h>

#include "debug.h"
#include "vtestream.h"

#include <glib-object.h>

/*
 * VteStream: Abstract base stream class
 */

typedef GObject VteStream;

typedef struct _VteStreamClass {
	void (*add) (const char *data, gsize len);
	void (*read) (gsize offset, char *data, gsize len);
	void (*trunc) (gsize len);
	void (*newpage) (void);
} VteStreamClass;

static GType _vte_stream_get_type (void);
#define VTE_TYPE_STREAM _vte_stream_get_type ()

G_DEFINE_ABSTRACT_TYPE (VteStream, _vte_stream, G_TYPE_OBJECT)

static void
_vte_stream_class_init (VteStreamClass *klass)
{
}

static void
_vte_stream_init (VteStream *stream)
{
}

