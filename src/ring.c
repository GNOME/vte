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


static VteRowData *
_vte_row_data_init (VteRowData *row)
{
	if (row->cells)
		g_array_set_size (row->cells, 0);
	else
		row->cells = g_array_new(FALSE, TRUE, sizeof(struct vte_charcell));
	row->soft_wrapped = 0;
	return row;
}

static void
_vte_row_data_fini(VteRowData *row, gboolean free_cells)
{
	if (free_cells && row->cells)
		g_array_free(row->cells, TRUE);
	row->cells = NULL;
}


#ifdef VTE_DEBUG
static void
_vte_ring_validate(VteRing * ring)
{
	long i, max;
	g_assert(ring != NULL);
	_vte_debug_print(VTE_DEBUG_RING,
			" Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	g_assert(ring->length <= ring->max);
	max = ring->delta + ring->length;
	for (i = ring->delta; i < max; i++) {
		g_assert(_vte_ring_contains(ring, i));
		g_assert(_vte_ring_index(ring, i)->cells != NULL);
	}
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
_vte_ring_free(VteRing *ring)
{
	glong i;
	for (i = 0; i < ring->max; i++)
		_vte_row_data_fini (&ring->array[i], TRUE);
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
_vte_ring_resize(VteRing *ring, glong max_elements)
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
		_vte_row_data_fini (&ring->array[position % ring->max], TRUE);
		ring->array[position % ring->max] = old_array[position % old_max];
	}

	if (ring->length > ring->max) {
	  ring->length = ring->max;
	  ring->delta = end - ring->max;
	}

	g_free (old_array);

	_vte_ring_validate(ring);
}

/**
 * _vte_ring_insert:
 * @ring: a #VteRing
 * @position: an index
 *
 * Inserts a new, empty, row into @ring at the @position'th offset.
 * XXX document how exactly this thing is supposed to work.
 *
 * Return: the newly added row.
 */
VteRowData *
_vte_ring_insert(VteRing * ring, long position)
{
	long point, i;
	VteRowData *row;

	g_return_val_if_fail(position >= ring->delta, NULL);
	g_return_val_if_fail(position <= ring->delta + ring->length, NULL);

	_vte_debug_print(VTE_DEBUG_RING, "Inserting at position %ld.\n", position);
	_vte_ring_validate(ring);

	/* Initial insertion, or append. */
	if (position == ring->length + ring->delta) {
		/* If there was something there before, free it. */
		_vte_row_data_fini (&ring->array[position % ring->max], TRUE);
		/* Set the new item, and if the buffer wasn't "full", increase
		 * our idea of how big it is, otherwise increase the delta so
		 * that this becomes the "last" item and the previous item
		 * scrolls off the *top*. */
		row = _vte_row_data_init(&ring->array[position % ring->max]);
		if (ring->length == ring->max)
			ring->delta++;
		else
			ring->length++;

		_vte_ring_validate(ring);
		return row;
	}

	/* All other cases.  Calculate the location where the last "item" in the
	 * buffer is going to end up in the array. */
	point = ring->delta + ring->length - 1;
	while (point < 0)
		point += ring->max;

	if (ring->length == ring->max) {
		/* If the buffer's full, then the last item will have to be
		 * "lost" to make room for the new item so that the buffer
		 * doesn't grow (here we scroll off the *bottom*). */
		_vte_row_data_fini (&ring->array[point % ring->max], TRUE);
	} else {
		/* We don't want to discard the last item. */
		point++;
	}

	/* We need to bubble the remaining valid elements down.  This isn't as
	 * slow as you probably think it is due to the pattern of usage. */
	for (i = point; i > position; i--)
		ring->array[i % ring->max] = ring->array[(i - 1) % ring->max];
	_vte_row_data_fini (&ring->array[position % ring->max], FALSE);

	/* Store the new item and bump up the length, unless we've hit the
	 * maximum length already. */
	row = _vte_row_data_init(&ring->array[position % ring->max]);
	ring->length = CLAMP(ring->length + 1, 0, ring->max);

	_vte_ring_validate(ring);
	return row;
}

/**
 * _vte_ring_append:
 * @ring: a #VteRing
 * @data: the new item
 *
 * Appends a new item to the ring.  If an item must be removed to make room for
 * the new item, it is freed.
 *
 * Return: the newly added row.
 */
VteRowData *
_vte_ring_append(VteRing * ring)
{
	return _vte_ring_insert(ring, ring->delta + ring->length);
}

/**
 * _vte_ring_remove:
 * @ring: a #VteRing
 * @position: an index
 *
 * Removes the @position'th item from @ring.
 */
void
_vte_ring_remove(VteRing * ring, long position)
{
	long i;
	_vte_debug_print(VTE_DEBUG_RING, "Removing item at position %ld.\n", position);
	_vte_ring_validate(ring);

	/* Remove the data at this position. */
	_vte_row_data_fini (&ring->array[position % ring->max], TRUE);

	/* Bubble the rest of the buffer up one notch.  This is also less
	 * of a problem than it might appear, again due to usage patterns. */
	for (i = position; i < ring->delta + ring->length - 1; i++)
		ring->array[i % ring->max] = ring->array[(i + 1) % ring->max];
	_vte_row_data_fini (&ring->array[(ring->delta + ring->length - 1) % ring->max], FALSE);
	if (ring->length > 0) {
		ring->length--;
	}
	_vte_ring_validate(ring);
}
