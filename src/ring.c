/*
 * Copyright (C) 2002,2009 Red Hat, Inc.
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
 * Red Hat Author(s): Nalin Dahyabhai, Behdad Esfahbod
 */

#include <config.h>

#include <stdio.h>
#include <glib.h>
#include "debug.h"
#include "ring.h"


/*
 * vtecells: A row's cell array
 */

typedef struct _vtecells vtecells;
struct _vtecells {
	unsigned int alloc_size;
	union {
		vtecells *next;
		VteCell cells[1];
	} p;
};

static inline vtecells *
vtecells_for_cells (VteCell *cells)
{
  return (vtecells *) (((char *) cells) - G_STRUCT_OFFSET (vtecells, p.cells));
}

static VteCell *
_vte_cells_realloc (VteCell *cells, unsigned int len)
{
	vtecells *vcells = cells ? vtecells_for_cells (cells) : NULL;
	unsigned int new_size = (1 << g_bit_storage (MAX (len, 80)));

	vcells = g_realloc (vcells, sizeof (vtecells) + len * sizeof (VteCell));
	vcells->alloc_size = new_size;

	return vcells->p.cells;
}

static void
_vte_cells_free (VteCell *cells)
{
	vtecells *vcells = vtecells_for_cells (cells);

	g_free (vcells);
}


/*
 * VteRowData: A row's data
 */

static VteRowData *
_vte_row_data_init (VteRowData *row)
{
	row->len = 0;
	row->soft_wrapped = 0;
	return row;
}

static void
_vte_row_data_fini (VteRowData *row)
{
	if (row->cells)
		_vte_cells_free (row->cells);
	row->cells = NULL;
}

static void
_vte_row_data_ensure (VteRowData *row, unsigned int len)
{
	if (row->len < len)
		row->cells = _vte_cells_realloc (row->cells, len);
}

void
_vte_row_data_insert (VteRowData *row, unsigned int col, const VteCell *cell)
{
	unsigned int i;

	_vte_row_data_ensure (row, row->len + 1);

	for (i = row->len; i > col; i--)
		row->cells[i] = row->cells[i - 1];

	row->cells[col] = *cell;
	row->len++;
}

void _vte_row_data_append (VteRowData *row, const VteCell *cell)
{
	_vte_row_data_ensure (row, row->len + 1);
	row->cells[row->len] = *cell;
	row->len++;
}

void _vte_row_data_remove (VteRowData *row, unsigned int col)
{
	unsigned int i;

	for (i = col + 1; i < row->len; i++)
		row->cells[i - 1] = row->cells[i];

	if (G_LIKELY (row->len))
		row->len--;
}

void _vte_row_data_fill (VteRowData *row, const VteCell *cell, unsigned int len)
{
	if (row->len < len) {
		unsigned int i = len - row->len;

		_vte_row_data_ensure (row, len);

		for (i = row->len; i < len; i++)
			row->cells[i] = *cell;

		row->len = len;
	}
}

void _vte_row_data_shrink (VteRowData *row, unsigned int max_len)
{
	if (max_len < row->len)
		row->len = max_len;
}



/*
 * VteRing: A buffer ring
 */

static void
_vte_ring_swap (VteRing *ring, unsigned int to, unsigned int from)
{
	VteRowData tmp;
	VteRowData *to_row = _vte_ring_index(ring, to);
	VteRowData *from_row = _vte_ring_index(ring, from);

	tmp = *to_row;
	*to_row = *from_row;
	*from_row = tmp;
}


#ifdef VTE_DEBUG
static void
_vte_ring_validate (VteRing * ring)
{
	long i, max;
	g_assert(ring != NULL);
	_vte_debug_print(VTE_DEBUG_RING,
			" Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	g_assert(ring->length <= ring->max);
	max = ring->delta + ring->length;
	for (i = ring->delta; i < max; i++)
		g_assert(_vte_ring_contains(ring, i));
}
#else
#define _vte_ring_validate(ring) G_STMT_START {} G_STMT_END
#endif

/**
 * _vte_ring_new:
 * @max_elements: the maximum size the new ring will be allowed to reach
 *
 * Allocates a new ring capable of holding up to @max_elements rows at a time.
 *
 * Returns: the new ring
 */
VteRing *
_vte_ring_new (glong max_elements)
{
	VteRing *ring = g_slice_new0(VteRing);
	ring->max = MAX(max_elements, 2);
	ring->array = g_malloc0(sizeof(ring->array[0]) * ring->max);

	_vte_debug_print(VTE_DEBUG_RING, "New ring %p.\n", ring);
	_vte_ring_validate(ring);

	return ring;
}

/**
 * _vte_ring_free:
 * @ring: a #VteRing
 *
 * Frees the ring and each of the items it contains.
 */
void
_vte_ring_free (VteRing *ring)
{
	glong i;
	for (i = 0; i < ring->max; i++)
		_vte_row_data_fini (&ring->array[i]);
	g_free(ring->array);
	g_slice_free(VteRing, ring);
}

/**
 * _vte_ring_free:
 * @ring: a #VteRing
 * @max_elements: new maximum numbers of rows in the ring
 *
 * Changes the number of lines the ring can contain.
 */
void
_vte_ring_resize (VteRing *ring, glong max_elements)
{
	glong position, end, old_max;
	VteRowData *old_array;

	max_elements = MAX(max_elements, 2);

	if (ring->max == max_elements)
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Resizing ring.\n");
	_vte_ring_validate(ring);

	old_max = ring->max;
	old_array = ring->array;

	ring->max = max_elements;
	ring->array = g_malloc0(sizeof(ring->array[0]) * ring->max);

	end = ring->delta + ring->length;
	for (position = ring->delta; position < end; position++) {
		_vte_row_data_fini (_vte_ring_index(ring, position));
		*_vte_ring_index(ring, position) = old_array[position % old_max];
	}

	if (ring->length > ring->max) {
	  ring->length = ring->max;
	  ring->delta = end - ring->max;
	}

	g_free (old_array);

	_vte_ring_validate(ring);
}

/**
 * _vte_ring_insert_internal:
 * @ring: a #VteRing
 * @position: an index
 *
 * Inserts a new, empty, row into @ring at the @position'th offset.
 * The item at that position and any items after that are shifted down.
 *
 * Return: the newly added row.
 */
static VteRowData *
_vte_ring_insert_internal (VteRing * ring, long position)
{
	long i;
	VteRowData *row;

	g_return_val_if_fail(position >= ring->delta, NULL);
	g_return_val_if_fail(position <= ring->delta + ring->length, NULL);

	_vte_debug_print(VTE_DEBUG_RING, "Inserting at position %ld.\n", position);
	_vte_ring_validate(ring);

	for (i = ring->delta + ring->length; i > position; i--)
		_vte_ring_swap (ring, i % ring->max, (i - 1) % ring->max);

	row = _vte_row_data_init(_vte_ring_index(ring, position));
	if (ring->length < ring->max)
		ring->length++;
	else
		ring->delta++;

	_vte_ring_validate(ring);
	return row;
}

/**
 * _vte_ring_insert:
 * @ring: a #VteRing
 * @data: the new item
 *
 * Inserts a new, empty, row into @ring at the @position'th offset.
 * The item at that position and any items after that are shifted down.
 * It pads enough lines if @position is after the end of the ring.
 *
 * Return: the newly added row.
 */
VteRowData *
_vte_ring_insert (VteRing *ring, glong position)
{
	while (G_UNLIKELY (_vte_ring_next (ring) < position))
		_vte_ring_append (ring);
	return _vte_ring_insert_internal (ring, position);
}

/**
 * _vte_ring_append:
 * @ring: a #VteRing
 * @data: the new item
 *
 * Appends a new item to the ring.
 *
 * Return: the newly added row.
 */
VteRowData *
_vte_ring_append (VteRing * ring)
{
	return _vte_ring_insert_internal (ring, ring->delta + ring->length);
}

/**
 * _vte_ring_remove:
 * @ring: a #VteRing
 * @position: an index
 *
 * Removes the @position'th item from @ring.
 */
void
_vte_ring_remove (VteRing * ring, long position)
{
	long i;

	if (G_UNLIKELY (!_vte_ring_contains (ring, position)))
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Removing item at position %ld.\n", position);
	_vte_ring_validate(ring);

	for (i = position; i < ring->delta + ring->length - 1; i++)
		_vte_ring_swap (ring, i % ring->max, (i + 1) % ring->max);

	if (ring->length > 0)
		ring->length--;

	_vte_ring_validate(ring);
}
