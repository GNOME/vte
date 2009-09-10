/*
 * Copyright (C) 2002 Red Hat, Inc.
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
 */

/* The interfaces in this file are subject to change at any time. */

#ifndef vte_ring_h_included
#define vte_ring_h_included

#include "vterowdata.h"

G_BEGIN_DECLS

/*
 * VteRingChunk: A chunk of the scrollback buffer ring
 */

typedef enum _VteRingChunkType VteRingChunkType;
enum _VteRingChunkType {
	VTE_RING_CHUNK_TYPE_INVALID,
	VTE_RING_CHUNK_TYPE_WRITABLE,
	VTE_RING_CHUNK_TYPE_COMPACT

};

typedef struct _VteRingChunk VteRingChunk;
struct _VteRingChunk {
	VteRingChunkType type; /* Chunk implementation type */
	VteRingChunk *prev_chunk, *next_chunk;
	guint start, end;
};

typedef struct _VteRingChunkWritable {
	VteRingChunk base;

	guint mask;
	VteRowData *array;
} VteRingChunkWritable;

/*
 * VteRing: A scrollback buffer ring
 */

typedef struct _VteRing VteRing;
struct _VteRing {
	guint max;

	VteRowData cached_row;
	guint cached_row_num;

	VteRingChunk *tail, *cursor;
	VteRingChunkWritable head[1];
};

#define _vte_ring_contains(__ring, __position) \
	(((__position) >= (__ring)->tail->start) && \
	 ((__position) < (__ring)->head->base.end))
#define _vte_ring_delta(__ring) ((__ring)->tail->start + 0)
#define _vte_ring_length(__ring) ((__ring)->head->base.end - (__ring)->tail->start)
#define _vte_ring_next(__ring) ((__ring)->head->base.end + 0)

const VteRowData *_vte_ring_index (VteRing *ring, guint position);
VteRowData *_vte_ring_index_writable (VteRing *ring, guint position);

void _vte_ring_init (VteRing *ring, guint max_rows);
void _vte_ring_fini (VteRing *ring);
void _vte_ring_resize (VteRing *ring, guint max_rows);
void _vte_ring_shrink (VteRing *ring, guint max_len);
VteRowData *_vte_ring_insert (VteRing *ring, guint position);
VteRowData *_vte_ring_append (VteRing *ring);
void _vte_ring_remove (VteRing *ring, guint position);

G_END_DECLS

#endif
