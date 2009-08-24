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

/* The structure we use to hold characters we're supposed to display -- this
 * includes any supported visible attributes. */
struct vte_charcell {
	vteunistr c;		/* The Unicode string for the cell. */

	struct vte_charcell_attr {
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
	} attr;
};

typedef struct _VteRowData {
	GArray *cells;
	guchar soft_wrapped: 1;
} VteRowData;

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
VteRowData *_vte_ring_insert_preserve(VteRing *ring, glong position);
VteRowData *_vte_ring_append(VteRing *ring);
void _vte_ring_remove(VteRing *ring, glong position);
void _vte_ring_free(VteRing *ring);

G_END_DECLS

#endif
