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

#ident "$Id$"
#include "../config.h"
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "debug.h"
#include "ring.h"

#ifdef VTE_DEBUG
static void
vte_ring_validate(VteRing *ring)
{
	long i, max;
	max = ring->delta + ring->length;
	for (i = ring->delta; i < max; i++) {
		g_assert(vte_ring_contains(ring, i));
		g_assert(ring->array[i % ring->max] != NULL);
	}
}
#endif

VteRing *
vte_ring_new(long max_elements, VteRingFreeFunc free, gpointer data)
{
	VteRing *ret = g_malloc0(sizeof(VteRing));
	ret->user_data = data;
	ret->delta = ret->length = 0;
	ret->max = max_elements;
	ret->array = g_malloc0(sizeof(gpointer) * ret->max);
	ret->free = free;
	return ret;
}

void
vte_ring_insert(VteRing *ring, long position, gpointer data)
{
	long point, i;
#ifdef VTE_DEBUG
	if (vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Inserting at position %ld.\n", position);
		fprintf(stderr, " Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	}
#endif
	g_return_if_fail(position >= ring->delta);
	g_return_if_fail(position <= ring->delta + ring->length);
	g_return_if_fail(data != NULL);

	/* Initial insertion, or append. */
	if (position == ring->length + ring->delta) {
		/* If there was something there before, free it. */
		if (ring->array[position % ring->max] && ring->free) {
			ring->free(ring->array[position % ring->max],
				   ring->user_data);
		}
		ring->array[position % ring->max] = data;
		if (ring->length == ring->max) {
			ring->delta++;
		} else {
			ring->length++;
		}
#ifdef VTE_DEBUG
		if (vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, " Delta = %ld, Length = %ld, "
				"Max = %ld.\n",
				ring->delta, ring->length, ring->max);
		}
		vte_ring_validate(ring);
#endif
		return;
	}

	/* All other cases. */
	point = ring->delta + ring->length - 1;
	while (point < 0) {
		point += ring->max;
	}

	/* If the buffer's full, then the last item will be lost. */
	if (ring->length == ring->max) {
		if (ring->free && ring->array[point % ring->max]) {
			ring->free(ring->array[point % ring->max],
				   ring->user_data);
		}
	}

	/* Bubble the rest down.  This isn't as slow as you probably think
	 * it is due to the pattern of usage. */
	if (ring->length != ring->max) {
		/* We'll need to copy the last item, too. */
		point++;
	}
	for (i = point; i > position; i--) {
		ring->array[i % ring->max] = ring->array[(i - 1) % ring->max];
	}
	ring->array[position % ring->max] = data;
	ring->length = MIN(ring->length + 1, ring->max);
#ifdef VTE_DEBUG
	if (vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, " Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	}
	vte_ring_validate(ring);
#endif
}

void
vte_ring_remove(VteRing *ring, long position, gboolean free)
{
	long i;
#ifdef VTE_DEBUG
	if (vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Removing item at position %ld.\n", position);
		fprintf(stderr, " Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	}
#endif
	/* Remove the data at this position. */
	if (ring->array[position % ring->max] && ring->free) {
		ring->free(ring->array[position % ring->max],
			   ring->user_data);
	}
	ring->array[position % ring->max] = NULL;

	/* Bubble the rest of the buffer up one notch.  This is also less
	 * of a problem than it might appear, again due to usage patterns. */
	for (i = position; i < ring->delta + ring->length - 1; i++) {
		ring->array[i % ring->max] = ring->array[(i + 1) % ring->max];
	}
	if (ring->length > 0) {
		ring->array[(ring->delta + ring->length - 1) % ring->max] = NULL;
		ring->length--;
	}
#ifdef VTE_DEBUG
	if (vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, " Delta = %ld, Length = %ld, Max = %ld.\n",
			ring->delta, ring->length, ring->max);
	}
	vte_ring_validate(ring);
#endif
}

void
vte_ring_append(VteRing *ring, gpointer data)
{
	vte_ring_insert(ring, ring->delta + ring->length, data);
}

void
vte_ring_free(VteRing *ring, gboolean free)
{
	long i;
	if (free) {
		for (i = 0; i < ring->max; i++) {
			if (ring->array[i]) {
				ring->free(ring->array[i], ring->user_data);
				ring->array[i] = NULL;
			}
		}
	}
	g_free(ring->array);
	memset(ring, 0, sizeof(ring));
	g_free(ring);
}

#ifdef RING_MAIN
static void
scrolled_off(gpointer freed, gpointer data)
{
	long *l = (long *)freed;
	char *fmt = data;
	g_print(fmt, *l);
}

int
main(int argc, char **argv)
{
	long i, j, k, bias;
	const int size = 8;
	long values[24];
	long lone = 42;
	long *value;
	VteRing *ring;

	for (i = 0; i < G_N_ELEMENTS(values); i++) {
		values[i] = i;
	}

	ring = vte_ring_new(size, scrolled_off, "Lost value %ld.\n");
	bias = 0;
	fprintf(stderr, "Initializing.\n");
	for (i = 0; i + bias <= G_N_ELEMENTS(values); i++) {
		k = 0;
		fprintf(stderr, "[%ld] ", i);
		for (j = 0; j < G_N_ELEMENTS(values); j++) {
			value = vte_ring_index(ring, long *, j);
			if (value) {
				fprintf(stderr, "%s%ld->%ld",
					(k > 0) ? ", " : "",
					j, *value);
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
			fprintf(stderr, "Removing item 3.\n");
			vte_ring_remove(ring, 4, TRUE);
			bias--;
		} else
		if (i == 10) {
			fprintf(stderr, "Inserting item 7.\n");
			vte_ring_insert(ring, 7, &lone);
			bias--;
		} else
		if (i == 20) {
			fprintf(stderr, "Inserting item 13.\n");
			vte_ring_insert(ring, 13, &lone);
			bias--;
		} else
		if (i < G_N_ELEMENTS(values)) {
			fprintf(stderr, "Appending item.\n");
			vte_ring_append(ring, &values[i + bias]);
		}
	}

	vte_ring_free(ring, TRUE);

	return 0;
}
#endif
