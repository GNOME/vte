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

#include "../config.h"
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "debug.h"
#include "ring.h"

#ifdef VTE_DEBUG
static void
_vte_ring_validate(VteRing * ring)
{
	long i, max;
	g_assert(ring != NULL);
	g_assert(ring->length <= ring->max);
	max = ring->delta + ring->length;
	for (i = ring->delta; i < max; i++) {
		g_assert(_vte_ring_contains(ring, i));
		g_assert(ring->array[i % ring->max] != NULL);
	}
}
#endif

/**
 * _vte_ring_new:
 * @max_elements: the maximum size the new ring will be allowed to reach
 * @free: a #VteRingFreeFunc
 * @data: user data for @free
 *
 * Allocates a new ring capable of holding up to @max_elements elements at a
 * time, using @free to free them when they are removed from the ring.  The
 * @data pointer is passed to the @free callback whenever it is called.
 *
 * Returns: a new ring
 */
VteRing *
_vte_ring_new(glong max_elements, VteRingFreeFunc free, gpointer data)
{
	VteRing *ret = g_slice_new0(VteRing);
	ret->user_data = data;
	ret->delta = ret->length = 0;
	ret->max = MAX(max_elements, 2);
	ret->array = g_malloc0(sizeof(gpointer) * ret->max);
	ret->free = free;
	return ret;
}

VteRing *
_vte_ring_new_with_delta(glong max_elements, glong delta,
			 VteRingFreeFunc free, gpointer data)
{
	VteRing *ret;
	ret = _vte_ring_new(max_elements, free, data);
	ret->delta = delta;
	return ret;
}

/**
 * _vte_ring_insert:
 * @ring: a #VteRing
 * @position: an index
 * @data: the new item
 *
 * Inserts a new item (@data) into @ring at the @position'th offset.  If @ring
 * already has an item stored at the desired location, it will be freed before
 * being replaced by the new @data.
 *
 */
void
_vte_ring_insert(VteRing * ring, long position, gpointer data)
{
	long point, i;
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_RING)) {
		fprintf(stderr, "Inserting at position %ld.\n", position);
		fprintf(stderr, " Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	}
	_vte_ring_validate(ring);
#endif
	g_return_if_fail(ring != NULL);
	g_return_if_fail(position >= ring->delta);
	g_return_if_fail(position <= ring->delta + ring->length);
	g_return_if_fail(data != NULL);

	/* Initial insertion, or append. */
	if (position == ring->length + ring->delta) {
		/* If there was something there before, free it. */
		if ((ring->free != NULL) &&
		    (ring->array[position % ring->max] != NULL)) {
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_RING)) {
				fprintf(stderr, "Freeing item at position "
					"%ld.\n", position);
			}
#endif
			ring->free(ring->array[position % ring->max],
				   ring->user_data);
		}
		/* Set the new item, and if the buffer wasn't "full", increase
		 * our idea of how big it is, otherwise increase the delta so
		 * that this becomes the "last" item and the previous item
		 * scrolls off the *top*. */
		ring->array[position % ring->max] = data;
		if (ring->length == ring->max) {
			ring->delta++;
		} else {
			ring->length++;
		}
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_RING)) {
			fprintf(stderr, " Delta = %ld, Length = %ld, "
				"Max = %ld.\n",
				ring->delta, ring->length, ring->max);
		}
		_vte_ring_validate(ring);
#endif
		return;
	}

	/* All other cases.  Calculate the location where the last "item" in the
	 * buffer is going to end up in the array. */
	point = ring->delta + ring->length - 1;
	while (point < 0) {
		point += ring->max;
	}

	if (ring->length == ring->max) {
		/* If the buffer's full, then the last item will have to be
		 * "lost" to make room for the new item so that the buffer
		 * doesn't grow (here we scroll off the *bottom*). */
		if (ring->free && ring->array[point % ring->max]) {
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_RING)) {
				fprintf(stderr, "Freeing item at position "
					"%ld.\n", point);
			}
#endif
			ring->free(ring->array[point % ring->max],
				   ring->user_data);
		}
	} else {
		/* We don't want to discard the last item. */
		point++;
	}

	/* We need to bubble the remaining valid elements down.  This isn't as
	 * slow as you probably think it is due to the pattern of usage. */
	for (i = point; i > position; i--) {
		ring->array[i % ring->max] = ring->array[(i - 1) % ring->max];
	}

	/* Store the new item and bump up the length, unless we've hit the
	 * maximum length already. */
	ring->array[position % ring->max] = data;
	ring->length = CLAMP(ring->length + 1, 0, ring->max);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_RING)) {
		fprintf(stderr, " Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	}
	_vte_ring_validate(ring);
#endif
}

/**
 * _vte_ring_insert_preserve:
 * @ring: a #VteRing
 * @position: an index
 * @data: the new item
 *
 * Inserts a new item (@data) into @ring at the @position'th offset.  If @ring
 * already has an item stored at the desired location, it (and any successive
 * items) will be moved down, items that need to be removed will be removed
 * from the *top*.
 *
 */
void
_vte_ring_insert_preserve(VteRing * ring, long position, gpointer data)
{
	long point, i;
	gpointer **tmp;

	g_return_if_fail(position <= _vte_ring_next(ring));

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_RING)) {
		fprintf(stderr, "Inserting+ at position %ld.\n", position);
		fprintf(stderr, " Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	}
	_vte_ring_validate(ring);
#endif

	/* Allocate space to save existing elements. */
	point = _vte_ring_next(ring);
	i = MAX(1, point - position);

	/* Save existing elements. */
	tmp = g_malloc0(sizeof(gpointer) * i);
	for (i = position; i < point; i++) {
		tmp[i - position] = _vte_ring_index(ring, gpointer, i);
	}

	/* Remove the existing elements. */
	for (i = point - 1; i >= position; i--) {
		_vte_ring_remove(ring, i, FALSE);
	}

	/* Append the new item. */
	_vte_ring_append(ring, data);

	/* Append the old items. */
	for (i = position; i < point; i++) {
		_vte_ring_append(ring, tmp[i - position]);
	}

	/* Clean up. */
	g_free(tmp);
}

/**
 * _vte_ring_remove:
 * @ring: a #VteRing
 * @position: an index
 * @free_element: %TRUE if the item should be freed
 *
 * Removes the @position'th item from @ring, freeing it only if @free_element is
 * %TRUE.
 *
 */
void
_vte_ring_remove(VteRing * ring, long position, gboolean free)
{
	long i;
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_RING)) {
		fprintf(stderr, "Removing item at position %ld.\n", position);
		fprintf(stderr, " Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	}
	_vte_ring_validate(ring);
#endif
	/* Remove the data at this position. */
	if (free && ring->array[position % ring->max] && ring->free) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_RING)) {
			fprintf(stderr, "Freeing item at position %ld.\n",
				position);
		}
#endif
		ring->free(ring->array[position % ring->max], ring->user_data);
	}
	ring->array[position % ring->max] = NULL;

	/* Bubble the rest of the buffer up one notch.  This is also less
	 * of a problem than it might appear, again due to usage patterns. */
	for (i = position; i < ring->delta + ring->length - 1; i++) {
		ring->array[i % ring->max] = ring->array[(i + 1) % ring->max];
	}

	/* Store a NULL in the position at the end of the buffer and decrement
	 * its length (got room for one more now). */
	ring->array[(ring->delta + ring->length - 1) % ring->max] = NULL;
	if (ring->length > 0) {
		ring->length--;
	}
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_RING)) {
		fprintf(stderr, " Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	}
	_vte_ring_validate(ring);
#endif
}

/**
 * _vte_ring_append:
 * @ring: a #VteRing
 * @data: the new item
 *
 * Appends a new item to the ring.  If an item must be removed to make room for
 * the new item, it is freed.
 *
 */
void
_vte_ring_append(VteRing * ring, gpointer data)
{
	g_assert(data != NULL);
	_vte_ring_insert(ring, ring->delta + ring->length, data);
}

/**
 * _vte_ring_free:
 * @ring: a #VteRing
 * @free_elements: %TRUE if items in the ring should be freed
 *
 * Frees the ring and, optionally, each of the items it contains.
 *
 */
void
_vte_ring_free(VteRing * ring, gboolean free_elements)
{
	long i;
	if (free_elements && ring->free) {
		for (i = 0; i < ring->max; i++) {
			/* Remove this item. */
			if (ring->array[i] != NULL) {
				ring->free(ring->array[i], ring->user_data);
				ring->array[i] = NULL;
			}
		}
	}
	g_free(ring->array);
	ring->free = NULL;
	ring->user_data = NULL;
	ring->array = NULL;
	ring->delta = ring->length = ring->max = 0;
	g_slice_free(VteRing, ring);
}

#ifdef RING_MAIN
static void
scrolled_off(gpointer freed, gpointer data)
{
	long *l = (long *)freed;
	char *fmt = data;
	fprintf(stderr, fmt, *l);
}

int
main(int argc, char **argv)
{
	long i, j, k, bias;
	const int size = 8;
	long values[40];
	long lone = 42;
	long *value;
	VteRing *ring;

	for (i = 0; i < G_N_ELEMENTS(values); i++) {
		values[i] = i;
	}

	ring = _vte_ring_new(size, scrolled_off, "Lost value %ld.\n");
	bias = 0;
	fprintf(stderr, "Initializing.\n");
	for (i = 0; i + bias <= G_N_ELEMENTS(values); i++) {
		k = 0;
		fprintf(stderr, "[%ld] ", i);
		for (j = 0; j < G_N_ELEMENTS(values); j++) {
			if (_vte_ring_contains(ring, j)) {
				value = _vte_ring_index(ring, long *, j);
			} else {
				value = NULL;
			}
			if (value) {
				fprintf(stderr, "%s%ld->%ld",
					(k > 0) ? ", " : "", j, *value);
				k++;
			}
		}
		fprintf(stderr, "\n");
		fprintf(stderr, "[%ld] max %ld, delta %ld, length %ld = {",
			i, ring->max, ring->delta, ring->length);
		for (j = 0; j < size; j++) {
			value = ring->array[j];
			if (j > 0) {
				fprintf(stderr, ", ");
			}
			if (value) {
				fprintf(stderr, "%ld", *value);
			}
		}
		fprintf(stderr, "}\n");
		if (i == 3) {
			fprintf(stderr, "Removing item at 4.\n");
			_vte_ring_remove(ring, 4, TRUE);
			bias--;
		} else if (i == 10) {
			fprintf(stderr, "Inserting item at 7.\n");
			_vte_ring_insert(ring, 7, &lone);
			bias--;
		} else if (i == 20) {
			fprintf(stderr, "Inserting item at 13.\n");
			_vte_ring_insert(ring, 13, &lone);
			bias--;
		} else if (i == 30) {
			fprintf(stderr, "Inserting item at 23.\n");
			_vte_ring_insert_preserve(ring, 23, &lone);
			bias--;
		} else if (i < G_N_ELEMENTS(values)) {
			fprintf(stderr, "Appending item.\n");
			_vte_ring_append(ring, &values[i + bias]);
		}
	}

	_vte_ring_free(ring, TRUE);

	return 0;
}
#endif
