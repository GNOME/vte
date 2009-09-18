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

#ifndef vterowdata_h_included
#define vterowdata_h_included

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
 * VteCellAttr: A single cell style attributes
 *
 * Ordered by most commonly changed attributes, to
 * optimize the compact representation.
 */

typedef struct _VteCellAttr {
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
} VteCellAttr;
ASSERT_STATIC (sizeof (VteCellAttr) == 4);

typedef union _VteIntCellAttr {
	VteCellAttr s;
	guint32 i;
} VteIntCellAttr;
ASSERT_STATIC (sizeof (VteCellAttr) == sizeof (VteIntCellAttr));

/*
 * VteCell: A single cell's data
 */

typedef struct _VteCell {
	vteunistr c;
	VteCellAttr attr;
} VteCell;
ASSERT_STATIC (sizeof (VteCell) == 8);

typedef union _VteIntCell {
	VteCell cell;
	struct {
		guint32 c;
		guint32 attr;
	} i;
} VteIntCell;
ASSERT_STATIC (sizeof (VteCell) == sizeof (VteIntCell));

static const VteIntCell basic_cell = {
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
 * VteRowAttr: A single row's attributes
 */

typedef struct _VteRowAttr {
	guint8 soft_wrapped: 1;
} VteRowAttr;
ASSERT_STATIC (sizeof (VteRowAttr) == 1);

/*
 * VteRowData: A single row's data
 */

typedef struct _VteRowData {
	VteCell *cells;
	guint16 len;
	VteRowAttr attr;
} VteRowData;


#define _vte_row_data_length(__row)			((__row)->len + 0)

static inline const VteCell *
_vte_row_data_get (const VteRowData *row, guint col)
{
	if (G_UNLIKELY (row->len <= col))
		return NULL;

	return &row->cells[col];
}

static inline VteCell *
_vte_row_data_get_writable (VteRowData *row, guint col)
{
	if (G_UNLIKELY (row->len <= col))
		return NULL;

	return &row->cells[col];
}

void _vte_row_data_init (VteRowData *row);
void _vte_row_data_clear (VteRowData *row);
void _vte_row_data_fini (VteRowData *row);
gboolean _vte_row_data_ensure (VteRowData *row, guint len);
void _vte_row_data_insert (VteRowData *row, guint col, const VteCell *cell);
void _vte_row_data_append (VteRowData *row, const VteCell *cell);
void _vte_row_data_remove (VteRowData *row, guint col);
void _vte_row_data_fill (VteRowData *row, const VteCell *cell, guint len);
void _vte_row_data_shrink (VteRowData *row, guint max_len);


G_END_DECLS

#endif
