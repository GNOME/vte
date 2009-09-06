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

#define FRAGMENT			-2


/*
 * vtecellattr: A single cell style attributes
 */

typedef struct _vtecellattr {
	guint32 columns: 4;	/* Number of visible columns
				   (as determined by g_unicode_iswide(c)).
				   Also abused for tabs; bug 353610
				   Keep at least 4 for tabs to work
				   */
	guint32 fore: 9;	/* Index into color palette */
	guint32 back: 9;	/* Index into color palette. */

	guint32 standout: 1;	/* Single-bit attributes. */
	guint32 underline: 1;
	guint32 strikethrough: 1;

	guint32 reverse: 1;
	guint32 blink: 1;
	guint32 half: 1;
	guint32 bold: 1;

	guint32 invisible: 1;
	/* unused; bug 499893
	guint32 protect: 1;
	 */

	/* 30 bits */
} vtecellattr;


/*
 * VteCell: A single cell's data
 */

typedef struct _VteCell {
	vteunistr c;
	vtecellattr attr;
} VteCell;

static const VteCell basic_cell = {
	0,
	{
	1, /* columns */
	VTE_DEF_FG, /* fore */
	VTE_DEF_BG, /* back */

	0, /* standout */
	0, /* underline */
	0, /* strikethrough */

	0, /* reverse */
	0, /* blink */
	0, /* half */
	0, /* bold */

	0  /* invisible */
	}
};


/*
 * VteRowData: A single row's data
 */

typedef struct _VteRowData {
	VteCell *cells;
	unsigned int len;
	guchar soft_wrapped: 1;
} VteRowData;


#define _vte_row_data_get(__row, __col)			((const VteCell *) _vte_row_data_get_writable (__row, __col))
#define _vte_row_data_length(__row)			((__row)->len + 0)

static inline VteCell *
_vte_row_data_get_writable (VteRowData *row, unsigned int col)
{
	if (G_UNLIKELY (row->len <= col))
		return NULL;

	return &row->cells[col];
}

void _vte_row_data_insert (VteRowData *row, unsigned int col, const VteCell *cell);
void _vte_row_data_append (VteRowData *row, const VteCell *cell);
void _vte_row_data_remove (VteRowData *row, unsigned int col);
void _vte_row_data_fill (VteRowData *row, const VteCell *cell, unsigned int len);
void _vte_row_data_shrink (VteRowData *row, unsigned int max_len);


/*
 * VteRing: A buffer ring
 */

typedef struct _VteRing VteRing;

struct _VteRing {
	glong delta, length, max;
	VteRowData *array;
};

#define _vte_ring_contains(__ring, __position) \
	(((__position) >= (__ring)->delta) && \
	 ((__position) < (__ring)->delta + (__ring)->length))
#define _vte_ring_delta(__ring) ((__ring)->delta + 0)
#define _vte_ring_length(__ring) ((__ring)->length + 0)
#define _vte_ring_next(__ring) ((__ring)->delta + (__ring)->length)
#define _vte_ring_index(__ring, __position) (&(__ring)->array[(__position) % (__ring)->max])

VteRing *_vte_ring_new (glong max_rows);
void _vte_ring_free (VteRing *ring);
void _vte_ring_resize (VteRing *ring, glong max_rows);
void _vte_ring_shrink (VteRing *ring, unsigned int max_len);
VteRowData *_vte_ring_insert (VteRing *ring, glong position);
VteRowData *_vte_ring_append (VteRing *ring);
void _vte_ring_remove (VteRing *ring, glong position);

G_END_DECLS

#endif
