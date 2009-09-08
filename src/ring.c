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
#define VTE_RING_CHUNK_COMPACT_MAX_FREE		4


/*
 * VtePool: Global, alloc-only, allocator for VteCells
 */

typedef struct _VtePool VtePool;
struct _VtePool {
	VtePool *next_pool;
	guint bytes_left;
	guchar *cursor;
	guchar data[1];
};

static VtePool *current_pool;

static void *
_vte_pool_alloc (guint size)
{
	void *ret;

	if (G_UNLIKELY (!current_pool || current_pool->bytes_left < size)) {
		guint alloc_size = MAX (VTE_POOL_BYTES, size + G_STRUCT_OFFSET (VtePool, data));
		VtePool *pool = g_malloc (alloc_size);

		_vte_debug_print(VTE_DEBUG_RING, "Allocating new pool of size %d \n", alloc_size);

		pool->next_pool = current_pool;
		pool->bytes_left = alloc_size - G_STRUCT_OFFSET (VtePool, data);
		pool->cursor = pool->data;

		current_pool = pool;
	}

	_vte_debug_print(VTE_DEBUG_RING, "Allocating %d bytes from pool\n", size);

	ret = current_pool->cursor;
	current_pool->bytes_left -= size;
	current_pool->cursor += size;

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
 * VteCells: A row's cell array
 */

typedef struct _VteCells VteCells;
struct _VteCells {
	guint32 rank;
	guint32 alloc_len; /* (1 << rank) - 1 */
	union {
		VteCells *next;
		VteCell cells[1];
	} p;
};

/* Cache of freed VteCells by rank */
static VteCells *free_cells[32];

static inline VteCells *
_vte_cells_for_cell_array (VteCell *cells)
{
	if (!cells)
		return NULL;

	return (VteCells *) (((guchar *) cells) - G_STRUCT_OFFSET (VteCells, p));
}

static VteCells *
_vte_cells_alloc (guint len)
{
	VteCells *ret;
	guint rank = g_bit_storage (MAX (len, 80));

	g_assert (rank < 32);

	if (G_LIKELY (free_cells[rank])) {
		_vte_debug_print(VTE_DEBUG_RING, "Allocating array of %d cells (rank %d) from cache\n", len, rank);
		ret = free_cells[rank];
		free_cells[rank] = ret->p.next;

	} else {
		guint alloc_len = (1 << rank) - 1;
		_vte_debug_print(VTE_DEBUG_RING, "Allocating new array of %d cells (rank %d)\n", len, rank);

		ret = _vte_pool_alloc (G_STRUCT_OFFSET (VteCells, p) + alloc_len * sizeof (ret->p.cells[0]));

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
_vte_cells_realloc (VteCells *cells, guint len)
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
_vte_cell_array_realloc (VteCell *cells, guint len)
{
	return _vte_cells_realloc (_vte_cells_for_cell_array (cells), len)->p.cells;
}

static void
_vte_cell_array_free (VteCell *cells)
{
	_vte_cells_free (_vte_cells_for_cell_array (cells));
}


/*
 * VteRowStorage: Storage layout flags for a row's cells
 */

static guint
_width (guint32 x)
{
	if (!x)
		return 0;
	if (G_LIKELY (x < 0x100))
		return 1;
	if (x < 0x10000)
		return 2;
	return 4;
}

static VteRowStorage
_vte_row_storage_compute (const VteCell *cells, guint len)
{
	guint i;
	const guint32 *c = (const guint32 *) cells;
	guint32 basic_attrs = basic_cell.i.attr;
	guint32 chars = 0, attrs = 0;
	VteRowStorage storage;

	for (i = 0; i < len; i++) {
		chars |= *c;
		c++;
		attrs |= *c ^ basic_attrs;
		c++;
	}

	storage.compact = 0;
	storage.flags.compact = 1;
	storage.flags.charbytes = _width (chars);
	storage.flags.attrbytes = _width (attrs);

	return storage;
}

static guint
_vte_row_storage_get_size (VteRowStorage storage, guint len)
{
	if (!storage.compact)
		return len * sizeof (VteCell);

	return len * (storage.flags.charbytes + storage.flags.attrbytes);
}

static guchar *
_store (guchar *to, const guint32 *from, guint32 xor, guint width, guint len)
{
	guint i;

	switch (width) {
        default: break;
	case 1:
		for (i = 0; i < len; i++) {
			guint8 c = *from ^ xor;
			*to++ = c;
			from += 2;
		}
		break;
	case 2:
		for (i = 0; i < len; i++) {
			guint16 c = *from ^ xor;
			*to++ = c >> 8;
			*to++ = c;
			from += 2;
		}
		break;
	case 4:
		for (i = 0; i < len; i++) {
			guint8 c = *from ^ xor;
			*to++ = c >> 24;
			*to++ = c >> 16;
			*to++ = c >> 8;
			*to++ = c;
			from += 2;
		}
		break;
	}

	return to;
}

static const guchar *
_fetch (const guchar *from, guint32 *to, guint32 xor, guint width, guint len)
{
	guint i;

	switch (width) {
        default:
		for (i = 0; i < len; i++) {
			guint32 c = 0;
			*to = c ^ xor;
			to += 2;
		}
		break;
	case 1:
		for (i = 0; i < len; i++) {
			guint32 c = 0;
			c += *from++;
			*to = c ^ xor;
			to += 2;
		}
		break;
	case 2:
		for (i = 0; i < len; i++) {
			guint32 c = 0;
			c += *from++ << 8;
			c += *from++;
			*to = c ^ xor;
			to += 2;
		}
		break;
	case 4:
		for (i = 0; i < len; i++) {
			guint8 c = 0;
			c += *from++ << 24;
			c += *from++ << 16;
			c += *from++ << 8;
			c += *from++;
			*to = c ^ xor;
			to += 2;
		}
		break;
	}

	return from;
}

static void
_vte_row_storage_compact (VteRowStorage storage, guchar *to, const VteCell *cells, guint len)
{
	guint32 basic_attrs = basic_cell.i.attr;

	_vte_debug_print(VTE_DEBUG_RING, "Compacting row: %d %d %d.\n",
			 storage.flags.compact, storage.flags.charbytes, storage.flags.attrbytes);

	if (!storage.compact) {
		memcpy (to, cells, len * sizeof (VteCell));
		return;
	}

	to = _store (to,     (const guint32 *) cells, 0,           storage.flags.charbytes, len);
	to = _store (to, 1 + (const guint32 *) cells, basic_attrs, storage.flags.attrbytes, len);
}

static void
_vte_row_storage_uncompact (VteRowStorage storage, const guchar *from, VteCell *cells, guint len)
{
	guint32 basic_attrs = basic_cell.i.attr;

	_vte_debug_print(VTE_DEBUG_RING, "Uncompacting row: %d %d %d.\n",
			 storage.flags.compact, storage.flags.charbytes, storage.flags.attrbytes);

	if (!storage.compact) {
		memcpy (cells, from, len * sizeof (VteCell));
		return;
	}

	from = _fetch (from,     (guint32 *) cells, 0,           storage.flags.charbytes, len);
	from = _fetch (from, 1 + (guint32 *) cells, basic_attrs, storage.flags.attrbytes, len);
}


/*
 * VteRowData: A row's data
 */

static void
_vte_row_data_init (VteRowData *row)
{
	memset (row, 0, sizeof (*row));
}

static void
_vte_row_data_clear (VteRowData *row)
{
	VteCell *cells = row->data.cells;
	_vte_row_data_init (row);
	row->data.cells = cells;
}

static void
_vte_row_data_fini (VteRowData *row)
{
	g_assert (!row->storage.compact);

	if (row->data.cells)
		_vte_cell_array_free (row->data.cells);
	row->data.cells = NULL;
}

static inline void
_vte_row_data_ensure (VteRowData *row, guint len)
{
	if (G_LIKELY (row->len < len))
		row->data.cells = _vte_cell_array_realloc (row->data.cells, len);
}

void
_vte_row_data_insert (VteRowData *row, guint col, const VteCell *cell)
{
	guint i;

	_vte_row_data_ensure (row, row->len + 1);

	for (i = row->len; i > col; i--)
		row->data.cells[i] = row->data.cells[i - 1];

	row->data.cells[col] = *cell;
	row->len++;
}

void _vte_row_data_append (VteRowData *row, const VteCell *cell)
{
	_vte_row_data_ensure (row, row->len + 1);
	row->data.cells[row->len] = *cell;
	row->len++;
}

void _vte_row_data_remove (VteRowData *row, guint col)
{
	guint i;

	for (i = col + 1; i < row->len; i++)
		row->data.cells[i - 1] = row->data.cells[i];

	if (G_LIKELY (row->len))
		row->len--;
}

void _vte_row_data_fill (VteRowData *row, const VteCell *cell, guint len)
{
	if (row->len < len) {
		guint i = len - row->len;

		_vte_row_data_ensure (row, len);

		for (i = row->len; i < len; i++)
			row->data.cells[i] = *cell;

		row->len = len;
	}
}

void _vte_row_data_shrink (VteRowData *row, guint max_len)
{
	if (max_len < row->len)
		row->len = max_len;
}

static void
_vte_row_data_uncompact_row (VteRowData *row, const VteRowData *old_row)
{
	VteRowStorage storage;
	VteCell *cells;

	g_assert (!row->storage.compact);

	storage = old_row->storage;

	/* Store cell data */
	_vte_row_data_ensure (row, old_row->len);
	_vte_row_storage_uncompact (storage, old_row->data.bytes, row->data.cells, old_row->len);

	/* Store row data */
	cells = row->data.cells;
	*row = *old_row;
	row->storage.compact = 0;
	row->data.cells = cells;
}



/*
 * VteRingChunk: A chunk of the scrollback buffer ring
 */

static void
_vte_ring_chunk_init (VteRingChunk *chunk)
{
	memset (chunk, 0, sizeof (*chunk));
}

static void
_vte_ring_chunk_insert_chunk_before (VteRingChunk *chunk, VteRingChunk *new)
{
	new->prev_chunk = chunk->prev_chunk;
	new->next_chunk = chunk;

	if (chunk->prev_chunk)
		chunk->prev_chunk->next_chunk = new;
	chunk->prev_chunk = new;
}


/* Compact chunk type */

typedef struct _VteRingChunkCompact {
	VteRingChunk base;

	guint offset;
	guint total_bytes;
	guint bytes_left;
	guchar *cursor; /* move backward */
	union {
		VteRowData rows[1];
		guchar data[1];
	} p;
} VteRingChunkCompact;

static VteRingChunkCompact *free_chunk_compact;
static guint num_free_chunk_compact;

static VteRingChunk *
_vte_ring_chunk_new_compact (guint start)
{
	VteRingChunkCompact *chunk;

	if (G_LIKELY (free_chunk_compact)) {
		chunk = free_chunk_compact;
		free_chunk_compact = (VteRingChunkCompact *) chunk->base.next_chunk;
		num_free_chunk_compact--;
	} else {
		chunk = g_malloc (VTE_POOL_BYTES);
		chunk->total_bytes = VTE_POOL_BYTES - G_STRUCT_OFFSET (VteRingChunkCompact, p);
	}

	_vte_ring_chunk_init (&chunk->base);
	chunk->base.type = VTE_RING_CHUNK_TYPE_COMPACT;
	chunk->offset = chunk->base.start = chunk->base.end = start;
	chunk->base.array = chunk->p.rows;

	chunk->bytes_left = chunk->total_bytes;
	chunk->cursor = chunk->p.data + chunk->bytes_left;

	return &chunk->base;
}

static void
_vte_ring_chunk_free_compact (VteRingChunk *bchunk)
{
	VteRingChunkCompact *chunk = (VteRingChunkCompact *) bchunk;
	g_assert (bchunk->type == VTE_RING_CHUNK_TYPE_COMPACT);

	if (num_free_chunk_compact >= VTE_RING_CHUNK_COMPACT_MAX_FREE) {
		g_free (bchunk);
		return;
	}

	chunk->base.next_chunk = (VteRingChunk *) free_chunk_compact;
	free_chunk_compact = chunk;
	num_free_chunk_compact++;
}

static void
_vte_ring_chunk_free_compact_spares (void)
{
	VteRingChunk *chunk;

	chunk = (VteRingChunk *) free_chunk_compact;
	while (chunk) {
		VteRingChunk *next_chunk = chunk->next_chunk;
		g_free (chunk);
		chunk = next_chunk;
		num_free_chunk_compact--;
	}

	g_assert (num_free_chunk_compact == 0);
}

/* Optimized version of _vte_ring_index() for writable chunks */
static inline VteRowData *
_vte_ring_chunk_compact_index (VteRingChunkCompact *chunk, guint position)
{
	return &chunk->p.rows[position - chunk->offset];
}

static gboolean
_vte_ring_chunk_compact_push_head_row (VteRingChunk *bchunk, VteRowData *row)
{
	VteRingChunkCompact *chunk = (VteRingChunkCompact *) bchunk;
	VteRowStorage storage;
	VteRowData *new_row;
	guint compact_size, total_size;

	g_assert (!row->storage.compact);

	storage = _vte_row_storage_compute (row->data.cells, row->len);
	compact_size = _vte_row_storage_get_size (storage, row->len);
	total_size = compact_size + sizeof (chunk->p.rows[0]);

	if (chunk->bytes_left < total_size)
		return FALSE;

	/* Store cell data */
	chunk->cursor -= compact_size;
	chunk->bytes_left -= total_size;
	_vte_row_storage_compact (storage, chunk->cursor, row->data.cells, row->len);

	/* Store row data */
	new_row = _vte_ring_chunk_compact_index (chunk, chunk->base.end);
	*new_row = *row;
	new_row->storage = storage;
	new_row->data.bytes = chunk->cursor;
	/* Truncate rows of no information */
	if (!compact_size)
		new_row->len = 0;

	chunk->base.end++;
	return TRUE;
}

static void
_vte_ring_chunk_compact_pop_head_row (VteRingChunk *bchunk, VteRowData *row)
{
	VteRingChunkCompact *chunk = (VteRingChunkCompact *) bchunk;
	const VteRowData *compact_row;
	guint compact_size, total_size;

	compact_row = _vte_ring_chunk_compact_index (chunk, chunk->base.end - 1);

	_vte_row_data_uncompact_row (row, compact_row);

	compact_size = _vte_row_storage_get_size (compact_row->storage, row->len);
	total_size = compact_size + sizeof (chunk->p.rows[0]);

	chunk->base.end--;
	chunk->cursor += compact_size;
	chunk->bytes_left += total_size;
}


/* Writable chunk type */

static void
_vte_ring_chunk_init_writable (VteRingChunk *chunk)
{
	_vte_ring_chunk_init (chunk);

	chunk->type = VTE_RING_CHUNK_TYPE_WRITABLE;
	chunk->mask = 31;
	chunk->array = g_malloc0 (sizeof (chunk->array[0]) * (chunk->mask + 1));
}

static void
_vte_ring_chunk_fini_writable (VteRingChunk *chunk)
{
	guint i;
	g_assert (chunk->type == VTE_RING_CHUNK_TYPE_WRITABLE);

	for (i = 0; i <= chunk->mask; i++)
		_vte_row_data_fini (&chunk->array[i]);

	g_free (chunk->array);
	chunk->array = NULL;
}

/* Optimized version of _vte_ring_index() for writable chunks */
static inline VteRowData *
_vte_ring_chunk_writable_index (VteRingChunk *chunk, guint position)
{
	return &chunk->array[position & chunk->mask];
}

static void
_vte_ring_chunk_writable_ensure_tail (VteRingChunk *chunk)
{
	guint new_mask, old_mask, i, end;
	VteRowData *old_array, *new_array;;

	if (G_LIKELY (chunk->start + chunk->mask > chunk->end))
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Enlarging writable array.\n");

	old_mask = chunk->mask;
	old_array = chunk->array;

	chunk->mask = (chunk->mask << 1) + 1;
	chunk->array = g_malloc0 (sizeof (chunk->array[0]) * (chunk->mask + 1));

	new_mask = chunk->mask;
	new_array = chunk->array;

	end = chunk->start + old_mask + 1;
	for (i = chunk->start; i < end; i++)
		new_array[i & new_mask] = old_array[i & old_mask];

	g_free (old_array);
}

static VteRowData *
_vte_ring_chunk_writable_insert (VteRingChunk *chunk, guint position)
{
	guint i;
	VteRowData *row, tmp;

	tmp = *_vte_ring_chunk_writable_index (chunk, chunk->end);
	for (i = chunk->end; i > position; i--)
		*_vte_ring_chunk_writable_index (chunk, i) = *_vte_ring_chunk_writable_index (chunk, i - 1);
	*_vte_ring_chunk_writable_index (chunk, position) = tmp;

	row = _vte_ring_chunk_writable_index(chunk, position);
	_vte_row_data_clear (row);
	chunk->end++;

	return row;
}

static void
_vte_ring_chunk_writable_remove (VteRingChunk *chunk, guint position)
{
	guint i;
	VteRowData tmp;

	tmp = *_vte_ring_chunk_writable_index (chunk, position);
	for (i = position; i < chunk->end - 1; i++)
		*_vte_ring_chunk_writable_index (chunk, i) = *_vte_ring_chunk_writable_index (chunk, i + 1);
	*_vte_ring_chunk_writable_index (chunk, chunk->end - 1) = tmp;

	if (chunk->end > chunk->start)
		chunk->end--;
}


/* Generic chunks */

static void
_vte_ring_chunk_free (VteRingChunk *chunk)
{
	g_assert (chunk->type == VTE_RING_CHUNK_TYPE_COMPACT);

	_vte_ring_chunk_free_compact (chunk);
}


/*
 * Free all pools if all rings have been destructed.
 */

static guint ring_count;

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

	if (ring_count)
		return;

	_vte_pool_free_all ();
	_vte_ring_chunk_free_compact_spares ();
}


/*
 * VteRing: A buffer ring
 */

#ifdef VTE_DEBUG
static void
_vte_ring_validate (VteRing * ring)
{
	VteRingChunk *chunk;

	g_assert(ring != NULL);
	_vte_debug_print(VTE_DEBUG_RING,
			" Delta = %u, Length = %u, Max = %u, Writable = %u.\n",
			ring->tail->start, ring->head->end - ring->tail->start, ring->max, ring->head->end - ring->head->start);

	g_assert(ring->head->end - ring->tail->start <= ring->max);

	g_assert(ring->head->start <= ring->head->end);
	chunk = ring->head->prev_chunk;
	while (chunk) {
		g_assert(chunk->start < chunk->end);
		g_assert(chunk->end == chunk->next_chunk->start);
		chunk = chunk->prev_chunk;
	}
}
#else
#define _vte_ring_validate(ring) G_STMT_START {} G_STMT_END
#endif


void
_vte_ring_init (VteRing *ring, guint max_rows)
{
	ring->max = MAX (max_rows, 2);

	_vte_row_data_init (&ring->cached_row);
	ring->cached_row_num = (guint) -1;

	ring->tail = ring->cursor = ring->head;

	_vte_ring_chunk_init_writable (ring->head);

	_vte_debug_print(VTE_DEBUG_RING, "New ring %p.\n", ring);
	_vte_ring_validate(ring);

	_ring_created ();
}

void
_vte_ring_fini (VteRing *ring)
{
	VteRingChunk *chunk;

	_vte_row_data_fini (&ring->cached_row);

	chunk = ring->head->prev_chunk;
	while (chunk) {
		VteRingChunk *prev_chunk = chunk->prev_chunk;
		_vte_ring_chunk_free_compact (chunk);
		chunk = prev_chunk;
	}

	_vte_ring_chunk_fini_writable (ring->head);

	_ring_destroyed ();
}

static const VteRingChunk *
_vte_ring_find_chunk (VteRing *ring, guint position)
{
	g_assert (_vte_ring_contains (ring, position));

	while (position < ring->cursor->start)
		ring->cursor = ring->cursor->prev_chunk;
	while (position >= ring->cursor->end)
		ring->cursor = ring->cursor->next_chunk;

	return ring->cursor;
}

const VteRowData *
_vte_ring_index (VteRing *ring, guint position)
{
	if (G_LIKELY (position >= ring->head->start))
		return _vte_ring_chunk_writable_index (ring->head, position);

	if (ring->cached_row_num != position) {
		VteRingChunkCompact *chunk = (VteRingChunkCompact *) _vte_ring_find_chunk (ring, position);
		VteRowData *compact_row = _vte_ring_chunk_compact_index (chunk, position);

		_vte_debug_print(VTE_DEBUG_RING, "Caching row %d.\n", position);

		_vte_row_data_uncompact_row (&ring->cached_row, compact_row);
		ring->cached_row_num = position;
	}

	return &ring->cached_row;
}

static void _vte_ring_ensure_writable (VteRing *ring, guint position);

VteRowData *
_vte_ring_index_writable (VteRing *ring, guint position)
{
	_vte_ring_ensure_writable (ring, position);
	return _vte_ring_chunk_writable_index (ring->head, position);
}

static void
_vte_ring_free_chunk (VteRing *ring, VteRingChunk *chunk)
{
	_vte_debug_print(VTE_DEBUG_RING, "Freeing chunk.\n");

	if (chunk == ring->head)
		return;

	if (ring->tail == chunk)
		ring->tail = chunk->next_chunk;
	if (ring->cursor == chunk)
		ring->cursor = chunk->next_chunk;

	chunk->next_chunk->prev_chunk = chunk->prev_chunk;
	if (chunk->prev_chunk)
		chunk->prev_chunk->next_chunk = chunk->next_chunk;

	_vte_ring_chunk_free (chunk);
}

static void
_vte_ring_pop_tail_row (VteRing *ring)
{
	ring->tail->start++;
	if (ring->tail->start == ring->tail->end)
		_vte_ring_free_chunk (ring, ring->tail);
}

static void
_vte_ring_compact_one_row (VteRing *ring)
{
	VteRowData *row;
	VteRingChunk *head = ring->head;

	_vte_debug_print(VTE_DEBUG_RING, "Compacting row %d.\n", head->start);

	row = _vte_ring_chunk_writable_index (head, head->start);

	if (!head->prev_chunk ||
	    !_vte_ring_chunk_compact_push_head_row (head->prev_chunk, row))
	{
		/* Previous head doesn't have enough room, add a new head and retry */
		VteRingChunk *new_chunk = _vte_ring_chunk_new_compact (head->start);

		_vte_debug_print(VTE_DEBUG_RING, "Allocating chunk.\n");

		_vte_ring_chunk_insert_chunk_before (head, new_chunk);
		if (ring->tail == head)
			ring->tail = new_chunk;

		/* TODO this may fail too */
		_vte_ring_chunk_compact_push_head_row (head->prev_chunk, row);
	}

	head->start++;
}

static void
_vte_ring_ensure_writable_head (VteRing *ring)
{
	if (G_LIKELY (ring->head->start + ring->head->mask == ring->head->end))
		_vte_ring_compact_one_row (ring);
}

static void
_vte_ring_ensure_writable_tail (VteRing *ring)
{
	_vte_ring_chunk_writable_ensure_tail (ring->head);
}

static void
_vte_ring_uncompact_one_row (VteRing *ring)
{
	VteRowData *row;
	VteRingChunk *head = ring->head;

	_vte_debug_print(VTE_DEBUG_RING, "Uncompacting row %d.\n", head->start - 1);

	_vte_ring_ensure_writable_tail (ring);

	head->start--;

	if (head->start == ring->cached_row_num)
		/* Invalidate cached row */
		ring->cached_row_num = (guint) -1;

	row = _vte_ring_chunk_writable_index (head, head->start);
	_vte_row_data_clear (row);

	if (!head->prev_chunk)
		return;

	_vte_ring_chunk_compact_pop_head_row (head->prev_chunk, row);
	if (head->prev_chunk->start == head->prev_chunk->end)
		_vte_ring_free_chunk (ring, head->prev_chunk);
}

static void
_vte_ring_ensure_writable (VteRing *ring, guint position)
{
	if (G_LIKELY (position >= ring->head->start))
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Ensure writable %d.\n", position);

	while (position < ring->head->start)
		_vte_ring_uncompact_one_row (ring);
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

	/* Get rid of unneeded chunks at the tail */
	while (ring->head != ring->tail && ring->head->end - ring->tail->end >= max_rows)
		_vte_ring_free_chunk (ring, ring->tail);

	/* Adjust the start of tail chunk now */
	if (_vte_ring_length (ring) > max_rows)
		ring->tail->start = ring->head->end - max_rows;

	ring->max = max_rows;
}

void
_vte_ring_shrink (VteRing *ring, guint max_len)
{
	if (_vte_ring_length (ring) <= max_len)
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Shrinking to %d.\n", max_len);
	_vte_ring_validate(ring);

	if (ring->head->start - ring->tail->start <= max_len)
		ring->head->end = ring->tail->start + max_len;
	else {
		while (ring->head->start - ring->tail->start > max_len) {
			_vte_ring_ensure_writable (ring, ring->head->start - 1);
			ring->head->end = ring->head->start;
		}
	}

	/* TODO May want to shrink down ring->head */

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
	VteRowData *row;

	_vte_debug_print(VTE_DEBUG_RING, "Inserting at position %u.\n", position);
	_vte_ring_validate(ring);

	if (_vte_ring_length (ring) == ring->max)
		_vte_ring_pop_tail_row (ring);

	g_assert (position >= ring->tail->start);
	g_assert (position <= ring->head->end);

	_vte_ring_ensure_writable (ring, position);
	if (position == ring->head->start)
		_vte_ring_ensure_writable_tail (ring);
	_vte_ring_ensure_writable_head (ring);

	row = _vte_ring_chunk_writable_insert (ring->head, position);

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
	_vte_debug_print(VTE_DEBUG_RING, "Removing item at position %u.\n", position);
	_vte_ring_validate(ring);

	g_assert (_vte_ring_contains (ring, position));

	_vte_ring_ensure_writable (ring, position);
	_vte_ring_chunk_writable_remove (ring->head, position);

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

