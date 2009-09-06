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
#include <string.h>
#include <glib.h>
#include "debug.h"
#include "ring.h"


#define VTE_POOL_BYTES	(1024*1024 - 4 * sizeof (void *)) /* hopefully we get some nice mmapped region */


/*
 * VtePool: Global, alloc-only, allocator for VteCells
 */

typedef struct _VtePool VtePool;
struct _VtePool {
	VtePool *next_pool;
	unsigned int bytes_left;
	char *next_data;
	char data[1];
};

static VtePool *current_pool;

static void *
_vte_pool_alloc (unsigned int size)
{
	void *ret;

	if (G_UNLIKELY (!current_pool || current_pool->bytes_left < size)) {
		unsigned int alloc_size = MAX (VTE_POOL_BYTES, size + G_STRUCT_OFFSET (VtePool, data));
		VtePool *pool = g_malloc (alloc_size);

		_vte_debug_print(VTE_DEBUG_RING, "Allocating new pool of size %d \n", alloc_size);

		pool->next_pool = current_pool;
		pool->bytes_left = alloc_size - G_STRUCT_OFFSET (VtePool, data);
		pool->next_data = pool->data;

		current_pool = pool;
	}

	_vte_debug_print(VTE_DEBUG_RING, "Allocating %d bytes from pool\n", size);

	ret = current_pool->next_data;
	current_pool->bytes_left -= size;
	current_pool->next_data += size;

	return ret;
}

static void
_vte_pool_free_all (void)
{
	_vte_debug_print(VTE_DEBUG_RING, "Freeing all pools\n");

	/* Free all cells pools */
	while (current_pool) {
		VtePool *pool = current_pool;
		current_pool = pool->next_pool;
		g_free (pool);
	}
}


/*
 * Free all pools if all rings have been destructed.
 */

static unsigned int ring_count;

static void
_ring_created (void)
{
	ring_count++;
	_vte_debug_print(VTE_DEBUG_RING, "Rings++: %d\n", ring_count);
}

static void
_ring_destroyed (void)
{
	g_assert (ring_count > 0);
	ring_count--;
	_vte_debug_print(VTE_DEBUG_RING, "Rings--: %d\n", ring_count);

	if (!ring_count)
		_vte_pool_free_all ();
}



/*
 * VteCells: A row's cell array
 */

typedef struct _VteCells VteCells;
struct _VteCells {
	unsigned int rank;
	unsigned int alloc_len; /* 1 << rank */
	union {
		VteCells *next;
		VteCell cells[1];
	} p;
};

/* Cache of freed VteCells by rank */
static VteCells *free_cells[sizeof (unsigned int) * 8];

static inline VteCells *
_vte_cells_for_cell_array (VteCell *cells)
{
	if (!cells)
		return NULL;

	return (VteCells *) (((char *) cells) - G_STRUCT_OFFSET (VteCells, p.cells));
}

static VteCells *
_vte_cells_alloc (unsigned int len)
{
	VteCells *ret;
	unsigned int rank = g_bit_storage (MAX (len, 80) - 1);

	if (G_LIKELY (free_cells[rank])) {
		_vte_debug_print(VTE_DEBUG_RING, "Allocating array of %d cells (rank %d) from cache\n", len, rank);
		ret = free_cells[rank];
		free_cells[rank] = ret->p.next;

	} else {
		unsigned int alloc_len = (1 << rank);
		_vte_debug_print(VTE_DEBUG_RING, "Allocating new array of %d cells (rank %d)\n", len, rank);

		ret = _vte_pool_alloc (G_STRUCT_OFFSET (VteCells, p.cells) + alloc_len * sizeof (ret->p.cells[0]));

		ret->rank = rank;
		ret->alloc_len = alloc_len;
	}

	return ret;
}

static void
_vte_cells_free (VteCells *cells)
{
	_vte_debug_print(VTE_DEBUG_RING, "Freeing cells (rank %d) to cache\n", cells->rank);

	cells->p.next = free_cells[cells->rank];
	free_cells[cells->rank] = cells;
}

static inline VteCells *
_vte_cells_realloc (VteCells *cells, unsigned int len)
{
	if (G_UNLIKELY (!cells || len > cells->alloc_len)) {
		VteCells *new_cells = _vte_cells_alloc (len);

		if (cells) {
			_vte_debug_print(VTE_DEBUG_RING, "Moving cells (rank %d to %d)\n", cells->rank, new_cells->rank);

			memcpy (new_cells->p.cells, cells->p.cells, sizeof (cells->p.cells[0]) * cells->alloc_len);
			_vte_cells_free (cells);
		}

		cells = new_cells;
	}

	return cells;
}

/* Convenience */

static inline VteCell *
_vte_cell_array_realloc (VteCell *cells, unsigned int len)
{
	return _vte_cells_realloc (_vte_cells_for_cell_array (cells), len)->p.cells;
}

static void
_vte_cell_array_free (VteCell *cells)
{
	_vte_cells_free (_vte_cells_for_cell_array (cells));
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
		_vte_cell_array_free (row->cells);
	row->cells = NULL;
}

static inline void
_vte_row_data_ensure (VteRowData *row, unsigned int len)
{
	if (G_LIKELY (row->len < len))
		row->cells = _vte_cell_array_realloc (row->cells, len);
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

	_ring_created ();

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

	_ring_destroyed ();
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
