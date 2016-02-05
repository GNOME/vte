/*
 * Copyright (C) 2002 Red Hat, Inc.
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
 */

/* The interfaces in this file are subject to change at any time. */

#ifndef vterowdata_h_included
#define vterowdata_h_included

#include "vteunistr.h"
#include "vtemacros.h"
#include "vtedefines.hh"

G_BEGIN_DECLS

#define VTE_TAB_WIDTH_BITS		4  /* Has to be able to store the value of 8. */
#define VTE_TAB_WIDTH_MAX		((1 << VTE_TAB_WIDTH_BITS) - 1)

/*
 * VteCellAttr: A single cell style attributes
 *
 * Ordered by most commonly changed attributes, to
 * optimize the compact representation.
 *
 * When adding new attributes, remember to update basic_cell below too.
 */

typedef struct _VteCellAttr {
	guint64 fragment: 1;	/* A continuation cell. */
	guint64 columns: VTE_TAB_WIDTH_BITS;	/* Number of visible columns
						   (as determined by g_unicode_iswide(c)).
						   Also abused for tabs; bug 353610
						   */
	guint64 bold: 1;
	guint64 italic: 1;
	guint64 fore: 25;	/* Index into color palette, or direct RGB, */
	/* 4-byte boundary */
	guint64 back: 25;	/* see vtedefines.hh */

	guint64 underline: 1;
	guint64 strikethrough: 1;

	guint64 reverse: 1;
	guint64 blink: 1;
	guint64 dim: 1;		/* also known as faint, half intensity etc. */

	guint64 invisible: 1;
        /* 1 bit unused */
} VteCellAttr;
G_STATIC_ASSERT (sizeof (VteCellAttr) == 8);

typedef union _VteIntCellAttr {
	VteCellAttr s;
	guint64 i;
} VteIntCellAttr;
G_STATIC_ASSERT (sizeof (VteCellAttr) == sizeof (VteIntCellAttr));

/*
 * VteCell: A single cell's data
 */

typedef struct _VTE_GNUC_PACKED _VteCell {
	vteunistr c;
	VteCellAttr attr;
} VteCell;
G_STATIC_ASSERT (sizeof (VteCell) == 12);

typedef union _VteIntCell {
	VteCell cell;
	struct _VTE_GNUC_PACKED {
		guint32 c;
		guint64 attr;
	} i;
} VteIntCell;
G_STATIC_ASSERT (sizeof (VteCell) == sizeof (VteIntCell));

static const VteIntCell basic_cell = {
	{
		0,
		{
			0, /* fragment */
			1, /* columns */
			0, /* bold */
			0, /* italic */
			VTE_DEFAULT_FG, /* fore */
			VTE_DEFAULT_BG, /* back */

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
G_STATIC_ASSERT (sizeof (VteRowAttr) == 1);

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
_vte_row_data_get (const VteRowData *row, gulong col)
{
	if (G_UNLIKELY (row->len <= col))
		return NULL;

	return &row->cells[col];
}

static inline VteCell *
_vte_row_data_get_writable (VteRowData *row, gulong col)
{
	if (G_UNLIKELY (row->len <= col))
		return NULL;

	return &row->cells[col];
}

void _vte_row_data_init (VteRowData *row);
void _vte_row_data_clear (VteRowData *row);
void _vte_row_data_fini (VteRowData *row);
void _vte_row_data_insert (VteRowData *row, gulong col, const VteCell *cell);
void _vte_row_data_append (VteRowData *row, const VteCell *cell);
void _vte_row_data_remove (VteRowData *row, gulong col);
void _vte_row_data_fill (VteRowData *row, const VteCell *cell, gulong len);
void _vte_row_data_shrink (VteRowData *row, gulong max_len);


G_END_DECLS

#endif
