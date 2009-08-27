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

typedef struct _vtecellattr {
	guint32 columns: 4;	/* Number of visible columns
				   (as determined by g_unicode_iswide(c)).
				   Also abused for tabs; bug 353610
				   Keep at least 4 for tabs to work
				   */
	guint32 fore: 9;	/* Index into color palette */
	guint32 back: 9;	/* Index into color palette. */

	guint32 fragment: 1;	/* A continuation cell. */
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

	/* 31 bits */
} vtecellattr;


typedef struct _vtecell {
	vteunistr c;
	vtecellattr attr;
} vtecell;

static const vtecell basic_cell = {
	0,
	{
	1, /* columns */
	VTE_DEF_FG, /* fore */
	VTE_DEF_BG, /* back */

	0, /* fragment */
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

typedef struct _VteRowData {
	GArray *_cells;
	guchar soft_wrapped: 1;
} VteRowData;


#define _vte_row_data_get(__row, __col)			((const vtecell *) _vte_row_data_get_writable (__row, __col))
#define _vte_row_data_length(__row)			((__row)->_cells->len + 0)
#define _vte_row_data_insert(__row, __pos, __cell)	g_array_insert_val ((__row)->_cells, __pos, *(__cell))
#define _vte_row_data_append(__row, __cell)		g_array_append_val ((__row)->_cells, *(__cell))
#define _vte_row_data_remove(__row, __col)		g_array_remove_index ((__row)->_cells, __col)
#define _vte_row_data_fill(__row, __cell, __len)	G_STMT_START { \
								int __i = (__len) - (__row)->_cells->len; \
								while (__i-- > 0)  \
									_vte_row_data_append (__row, __cell); \
							} G_STMT_END

#define _vte_row_data_shrink(__row, __max_len)		g_array_set_size ((__row)->_cells, MIN((__row)->_cells->len, (unsigned int)(__max_len)))

static inline vtecell *
_vte_row_data_get_writable (VteRowData *row, unsigned int col)
{
	if (G_UNLIKELY (row->_cells->len <= col))
		return NULL;

	return &g_array_index (row->_cells, vtecell, col);
}

#if 0
const vtecell *_vte_row_data_get (VteRowData *row, unsigned int col);
unsigned int _vte_row_data_length (VteRowData *row);
void _vte_row_data_insert (VteRowData *row, int pos, const vtecell *cell);
void _vte_row_data_append (VteRowData *row, const vtecell *cell);
void _vte_row_data_remove (VteRowData *row, unsigned int col);
void _vte_row_data_fill (VteRowData *row, const vtecell *cell, int len);
void _vte_row_data_shrink (VteRowData *row, int max_len);
#endif


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
#define _vte_ring_max(__ring) ((__ring)->max + 0)
#define _vte_ring_index(__ring, __position) (&(__ring)->array[(__position) % (__ring)->max])
#define _vte_ring_set_length(__ring, __length) ((__ring)->length = __length)

VteRing *_vte_ring_new(glong max_elements);
void _vte_ring_resize(VteRing *ring, glong max_elements);
VteRowData *_vte_ring_insert(VteRing *ring, glong position);
VteRowData *_vte_ring_append(VteRing *ring);
void _vte_ring_remove(VteRing *ring, glong position);
void _vte_ring_free(VteRing *ring);

G_END_DECLS

#endif
