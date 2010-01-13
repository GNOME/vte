/*
 * Copyright (C) 2002,2009,2010 Red Hat, Inc.
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

/* The interfaces in this file are subject to change at any time. */

#ifndef vte_ring_h_included
#define vte_ring_h_included

#include <gio/gio.h>
#include "vte.h"

#include "vterowdata.h"
#include "vtestream.h"

G_BEGIN_DECLS


typedef struct _VteCellAttrChange {
	gsize text_offset;
	VteIntCellAttr attr;
} VteCellAttrChange;


/*
 * VteRing: A scrollback buffer ring
 */

typedef struct _VteRing VteRing;
struct _VteRing {
	gulong max;

	gulong start, end;

	/* Writable */
	gulong writable, mask;
	VteRowData *array;

	/* Storage */
	gulong last_page;
	VteStream *attr_stream, *text_stream, *row_stream;
	VteCellAttrChange last_attr;
	GString *utf8_buffer;

	VteRowData cached_row;
	gulong cached_row_num;

};

#define _vte_ring_contains(__ring, __position) \
	(((gulong) (__position) >= (__ring)->start) && \
	 ((gulong) (__position) < (__ring)->end))
#define _vte_ring_delta(__ring) ((glong) (__ring)->start)
#define _vte_ring_length(__ring) ((glong) ((__ring)->end - (__ring)->start))
#define _vte_ring_next(__ring) ((glong) (__ring)->end)

const VteRowData *_vte_ring_index (VteRing *ring, gulong position);
VteRowData *_vte_ring_index_writable (VteRing *ring, gulong position);

void _vte_ring_init (VteRing *ring, gulong max_rows);
void _vte_ring_fini (VteRing *ring);
void _vte_ring_resize (VteRing *ring, gulong max_rows);
void _vte_ring_shrink (VteRing *ring, gulong max_len);
VteRowData *_vte_ring_insert (VteRing *ring, gulong position);
VteRowData *_vte_ring_append (VteRing *ring);
void _vte_ring_remove (VteRing *ring, gulong position);
gboolean _vte_ring_write_contents (VteRing *ring,
				   GOutputStream *stream,
				   VteTerminalWriteFlags flags,
				   GCancellable *cancellable,
				   GError **error);

G_END_DECLS

#endif
