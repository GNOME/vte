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


#include <glib.h>

#include "debug.h"
#include "vteunistr.h"

G_BEGIN_DECLS

#define VTE_DEF_FG			256
#define VTE_DEF_BG			257
#define VTE_BOLD_FG			258
#define VTE_DIM_FG			259
#define VTE_DEF_HL                      260
#define VTE_CUR_BG			261
#define VTE_PALETTE_SIZE		262


/*
 * vtecellattr: A single cell style attributes
 *
 * Ordered by most commonly changed attributes, to
 * optimize the compact representation.
 */

typedef struct _vtecellattr {
	guint32 fragment: 1;	/* A continuation cell. */
	guint32 columns: 4;	/* Number of visible columns
				   (as determined by g_unicode_iswide(c)).
				   Also abused for tabs; bug 353610
				   Keep at least 4 for tabs to work
				   */
	guint32 bold: 1;
	guint32 fore: 9;	/* Index into color palette */
	guint32 back: 9;	/* Index into color palette. */

	guint32 standout: 1;
	guint32 underline: 1;
	guint32 strikethrough: 1;

	guint32 reverse: 1;
	guint32 blink: 1;
	guint32 half: 1;

	guint32 invisible: 1;
	/* unused; bug 499893
	guint32 protect: 1;
	 */

	/* 30 bits */
} vtecellattr;
ASSERT_STATIC (sizeof (vtecellattr) == 4);


/*
 * VteCell: A single cell's data
 */

typedef struct _VteCell {
	vteunistr c;
	vtecellattr attr;
} VteCell;
ASSERT_STATIC (sizeof (VteCell) == 8);

typedef union _VteCellInt {
	VteCell cell;
	struct {
		guint32 c;
		guint32 attr;
	} i;
} VteCellInt;

static const VteCellInt basic_cell = {
	{
		0,
		{
			0, /* fragment */
			1, /* columns */
			0, /* bold */
			VTE_DEF_FG, /* fore */
			VTE_DEF_BG, /* back */

			0, /* standout */
			0, /* underline */
			0, /* strikethrough */

			0, /* reverse */
			0, /* blink */
			0, /* half */

			0  /* invisible */
		}
	}
};


/*
 * VteRowStorage: Storage layout flags for a row's cells
 */

typedef union _VteRowStorage {
	guint8 compact; /* For quick access */
	struct {
		guint8 compact   : 1;
		/* TODO these can be made faster using shifts instead of num bytes */
		guint8 charbytes : 3;
		guint8 attrbytes : 3;
	} flags;
} VteRowStorage;
ASSERT_STATIC (sizeof (VteRowStorage) == 1);

/*
 * VteRowData: A single row's data
 */

typedef struct _VteRowData {
	union {
		VteCell *cells; /* for non-compact storage */
		guchar *bytes;  /* for compact storage */
	} data;
	guint32 len;
	VteRowStorage storage;
	guint8 soft_wrapped: 1;
} VteRowData;


#define _vte_row_data_length(__row)			((__row)->len + 0)

static inline const VteCell *
_vte_row_data_get (const VteRowData *row, guint col)
{
	if (G_UNLIKELY (row->len <= col))
		return NULL;

	return &row->data.cells[col];
}

static inline VteCell *
_vte_row_data_get_writable (VteRowData *row, guint col)
{
	if (G_UNLIKELY (row->len <= col))
		return NULL;

	return &row->data.cells[col];
}

void _vte_row_data_insert (VteRowData *row, guint col, const VteCell *cell);
void _vte_row_data_append (VteRowData *row, const VteCell *cell);
void _vte_row_data_remove (VteRowData *row, guint col);
void _vte_row_data_fill (VteRowData *row, const VteCell *cell, guint len);
void _vte_row_data_shrink (VteRowData *row, guint max_len);


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
	guint mask; /* For WRITABLE chunks only */
	VteRowData *array;
};


/*
 * VteRing: A scrollback buffer ring
 */

typedef struct _VteRing VteRing;
struct _VteRing {
	guint max;

	VteRowData cached_row;
	guint cached_row_num;

	VteRingChunk *tail, *cursor;
	VteRingChunk head[1];
};

#define _vte_ring_contains(__ring, __position) \
	(((__position) >= (__ring)->tail->start) && \
	 ((__position) < (__ring)->head->end))
#define _vte_ring_delta(__ring) ((__ring)->tail->start + 0)
#define _vte_ring_length(__ring) ((__ring)->head->end - (__ring)->tail->start)
#define _vte_ring_next(__ring) ((__ring)->head->end + 0)

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
