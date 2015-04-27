/*
 * Copyright (C) 2002,2009,2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

/* The interfaces in this file are subject to change at any time. */

#ifndef vte_ring_h_included
#define vte_ring_h_included

#include <gio/gio.h>
#include <vte/vte.h>

#include "vterowdata.h"
#include "vtestream.h"

G_BEGIN_DECLS


typedef struct _VteVisualPosition {
	long row, col;
} VteVisualPosition;

typedef struct _VteCellAttrChange {
	gsize text_end_offset;  /* offset of first character no longer using this attr */
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
	VteStream *attr_stream, *text_stream, *row_stream;
	gsize last_attr_text_start_offset;
	VteIntCellAttr last_attr;
	GString *utf8_buffer;

	VteRowData cached_row;
	gulong cached_row_num;

	gboolean has_streams;
        gulong visible_rows;  /* to keep at least a screenful of lines in memory, bug 646098 comment 12 */
};

#define _vte_ring_contains(__ring, __position) \
	(((gulong) (__position) >= (__ring)->start) && \
	 ((gulong) (__position) < (__ring)->end))
#define _vte_ring_delta(__ring) ((glong) (__ring)->start)
#define _vte_ring_length(__ring) ((glong) ((__ring)->end - (__ring)->start))
#define _vte_ring_next(__ring) ((glong) (__ring)->end)

const VteRowData *_vte_ring_index (VteRing *ring, gulong position);
VteRowData *_vte_ring_index_writable (VteRing *ring, gulong position);

void _vte_ring_init (VteRing *ring, gulong max_rows, gboolean has_streams);
void _vte_ring_fini (VteRing *ring);
long _vte_ring_reset (VteRing *ring);
void _vte_ring_resize (VteRing *ring, gulong max_rows);
void _vte_ring_shrink (VteRing *ring, gulong max_len);
VteRowData *_vte_ring_insert (VteRing *ring, gulong position);
VteRowData *_vte_ring_append (VteRing *ring);
void _vte_ring_remove (VteRing *ring, gulong position);
void _vte_ring_drop_scrollback (VteRing *ring, gulong position);
void _vte_ring_set_visible_rows (VteRing *ring, gulong rows);
void _vte_ring_rewrap (VteRing *ring, glong columns, VteVisualPosition **markers);
gboolean _vte_ring_write_contents (VteRing *ring,
				   GOutputStream *stream,
				   VteWriteFlags flags,
				   GCancellable *cancellable,
				   GError **error);

G_END_DECLS

#endif
