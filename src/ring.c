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

#include "debug.h"
#include "ring.h"

#include <string.h>

/*
 * VteRing: A buffer ring
 */

#ifdef VTE_DEBUG
static void
_vte_ring_validate (VteRing * ring)
{
	g_assert(ring != NULL);
	_vte_debug_print(VTE_DEBUG_RING,
			" Delta = %u, Length = %u, Max = %u, Writable = %u.\n",
			ring->start, ring->end - ring->start,
			ring->max, ring->end - ring->writable);

	g_assert (ring->last_page <= ring->start);
	g_assert (ring->start <= ring->writable);
	g_assert (ring->writable <= ring->end);

	g_assert (ring->end - ring->start <= ring->max);
	g_assert (ring->end - ring->writable <= ring->mask);
}
#else
#define _vte_ring_validate(ring) G_STMT_START {} G_STMT_END
#endif


void
_vte_ring_init (VteRing *ring, guint max_rows)
{
	_vte_debug_print(VTE_DEBUG_RING, "New ring %p.\n", ring);

	ring->max = MAX (max_rows, 3);

	ring->mask = 31;
	ring->array = g_malloc0 (sizeof (ring->array[0]) * (ring->mask + 1));

	ring->cell_stream = _vte_file_stream_new ();
	ring->row_stream = _vte_file_stream_new ();

	_vte_row_data_init (&ring->cached_row);
	ring->cached_row_num = (guint) -1;

	_vte_ring_validate(ring);
}

void
_vte_ring_fini (VteRing *ring)
{
	guint i;

	for (i = 0; i <= ring->mask; i++)
		_vte_row_data_fini (&ring->array[i]);

	g_free (ring->array);

	g_object_unref (ring->cell_stream);
	g_object_unref (ring->row_stream);

	_vte_row_data_fini (&ring->cached_row);
}

static void
_vte_ring_freeze_row (VteRing *ring, guint position, const VteRowData *row)
{
	gsize cell_position;
	VteRowData tmp;

	_vte_debug_print (VTE_DEBUG_RING, "Freezing row %d.\n", position);

	cell_position = _vte_stream_append (ring->cell_stream, (const char *) row->cells, row->len * sizeof (row->cells[0]));

	tmp = *row;
	tmp.cells = GSIZE_TO_POINTER (cell_position);
	_vte_stream_append (ring->row_stream, (const char *) &tmp, sizeof (tmp));
}

static void
_vte_ring_thaw_row (VteRing *ring, guint position, VteRowData *row)
{
	VteCell *cells;
	gsize cell_position;

	_vte_debug_print (VTE_DEBUG_RING, "Thawing row %d.\n", position);

	cells = row->cells;
	_vte_stream_read (ring->row_stream, position * sizeof (*row), (char *) row, sizeof (*row));
	cell_position = GPOINTER_TO_SIZE (row->cells);
	row->cells = cells;

	if (G_UNLIKELY (!_vte_row_data_ensure (row, row->len))) {
		row->len = 0;
		return;
	}

	_vte_stream_read (ring->cell_stream, cell_position, (char *) row->cells, row->len * sizeof (row->cells[0]));
}

static void
_vte_ring_new_page (VteRing *ring)
{
	_vte_stream_new_page (ring->cell_stream);
	_vte_stream_new_page (ring->row_stream);

	ring->last_page = ring->writable;
}


static inline VteRowData *
_vte_ring_writable_index (VteRing *ring, guint position)
{
	return &ring->array[position & ring->mask];
}

const VteRowData *
_vte_ring_index (VteRing *ring, guint position)
{
	if (G_LIKELY (position >= ring->writable))
		return _vte_ring_writable_index (ring, position);

	if (ring->cached_row_num != position) {
		_vte_debug_print(VTE_DEBUG_RING, "Caching row %d.\n", position);
		_vte_ring_thaw_row (ring, position, &ring->cached_row);
		ring->cached_row_num = position;
	}

	return &ring->cached_row;
}

static void _vte_ring_ensure_writable (VteRing *ring, guint position);

VteRowData *
_vte_ring_index_writable (VteRing *ring, guint position)
{
	_vte_ring_ensure_writable (ring, position);
	return _vte_ring_writable_index (ring, position);
}

static void
_vte_ring_freeze_one_row (VteRing *ring)
{
	VteRowData *row;

	if (G_UNLIKELY (ring->start - ring->last_page >= ring->max))
		_vte_ring_new_page (ring);

	row = _vte_ring_writable_index (ring, ring->writable);
	_vte_ring_freeze_row (ring, ring->writable, row);

	ring->writable++;
}

static void
_vte_ring_ensure_writable_head (VteRing *ring)
{
	if (G_LIKELY (ring->writable + ring->mask == ring->end))
		_vte_ring_freeze_one_row (ring);
}

static void
_vte_ring_ensure_writable_tail (VteRing *ring)
{
	guint new_mask, old_mask, i, end;
	VteRowData *old_array, *new_array;;

	if (G_LIKELY (ring->start + ring->mask > ring->end))
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Enlarging writable array.\n");

	old_mask = ring->mask;
	old_array = ring->array;

	ring->mask = (ring->mask << 1) + 1;
	ring->array = g_malloc0 (sizeof (ring->array[0]) * (ring->mask + 1));

	new_mask = ring->mask;
	new_array = ring->array;

	end = ring->writable + old_mask + 1;
	for (i = ring->writable; i < end; i++)
		new_array[i & new_mask] = old_array[i & old_mask];

	g_free (old_array);
}

static void
_vte_ring_thaw_one_row (VteRing *ring)
{
	VteRowData *row;

	_vte_ring_ensure_writable_tail (ring);

	ring->writable--;

	if (ring->writable == ring->cached_row_num)
		/* Invalidate cached row */
		ring->cached_row_num = (guint) -1;

	row = _vte_ring_writable_index (ring, ring->writable);

	if (ring->start >= ring->writable) {
		g_assert_not_reached ();
		_vte_row_data_clear (row);
		ring->start = ring->writable;
		return;
	}

	_vte_ring_thaw_row (ring, ring->writable, row);
}

static void
_vte_ring_ensure_writable (VteRing *ring, guint position)
{
	if (G_LIKELY (position >= ring->writable))
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Ensure writable %d.\n", position);

	while (position < ring->writable)
		_vte_ring_thaw_one_row (ring);
}


/**
 * _vte_ring_resize:
 * @ring: a #VteRing
 * @max_rows: new maximum numbers of rows in the ring
 *
 * Changes the number of lines the ring can contain.
 */
void
_vte_ring_resize (VteRing *ring, guint max_rows)
{
	_vte_debug_print(VTE_DEBUG_RING, "Resizing to %d.\n", max_rows);
	_vte_ring_validate(ring);

	/* Adjust the start of tail chunk now */
	if (_vte_ring_length (ring) > max_rows)
		ring->start = ring->end - max_rows;

	ring->max = max_rows;
}

void
_vte_ring_shrink (VteRing *ring, guint max_len)
{
	if (_vte_ring_length (ring) <= max_len)
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Shrinking to %d.\n", max_len);
	_vte_ring_validate(ring);

	if (ring->writable - ring->start <= max_len)
		ring->end = ring->start + max_len;
	else {
		while (ring->writable - ring->start > max_len) {
			_vte_ring_ensure_writable (ring, ring->writable - 1);
			ring->end = ring->writable;
		}
	}

	/* TODO May want to shrink down ring->array */

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
_vte_ring_insert_internal (VteRing *ring, guint position)
{
	guint i;
	VteRowData *row, tmp;

	_vte_debug_print(VTE_DEBUG_RING, "Inserting at position %u.\n", position);
	_vte_ring_validate(ring);

	if (_vte_ring_length (ring) == ring->max)
		ring->start++;

	g_assert (position >= ring->start && position <= ring->end);

	/* Make room */
	_vte_ring_ensure_writable (ring, position);
	if (position == ring->writable)
		_vte_ring_ensure_writable_tail (ring);
	_vte_ring_ensure_writable_head (ring);

	tmp = *_vte_ring_writable_index (ring, ring->end);
	for (i = ring->end; i > position; i--)
		*_vte_ring_writable_index (ring, i) = *_vte_ring_writable_index (ring, i - 1);
	*_vte_ring_writable_index (ring, position) = tmp;

	row = _vte_ring_writable_index (ring, position);
	_vte_row_data_clear (row);
	ring->end++;

	_vte_ring_validate(ring);
	return row;
}

/**
 * _vte_ring_remove:
 * @ring: a #VteRing
 * @position: an index
 *
 * Removes the @position'th item from @ring.
 */
void
_vte_ring_remove (VteRing * ring, guint position)
{
	guint i;
	VteRowData tmp;

	_vte_debug_print(VTE_DEBUG_RING, "Removing item at position %u.\n", position);
	_vte_ring_validate(ring);

	g_assert (_vte_ring_contains (ring, position));

	_vte_ring_ensure_writable (ring, position);

	tmp = *_vte_ring_writable_index (ring, position);
	for (i = position; i < ring->end - 1; i++)
		*_vte_ring_writable_index (ring, i) = *_vte_ring_writable_index (ring, i + 1);
	*_vte_ring_writable_index (ring, ring->end - 1) = tmp;

	if (ring->end > ring->writable)
		ring->end--;

	_vte_ring_validate(ring);
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
_vte_ring_insert (VteRing *ring, guint position)
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
	return _vte_ring_insert_internal (ring, _vte_ring_next (ring));
}

