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


#define VTE_RING_CHUNK_COMPACT_BYTES	(1024*1024 - 4 * sizeof (void *)) /* hopefully we get some nice mmapped region */


/*
 * VteCells: A row's cell array
 */

typedef struct _VteCells VteCells;
struct _VteCells {
	guint32 alloc_len;
	VteCell cells[1];
};

static inline VteCells *
_vte_cells_for_cell_array (VteCell *cells)
{
	if (G_UNLIKELY (!cells))
		return NULL;

	return (VteCells *) (((guchar *) cells) - G_STRUCT_OFFSET (VteCells, cells));
}

static VteCells *
_vte_cells_realloc (VteCells *cells, guint len)
{
	guint alloc_len = (1 << g_bit_storage (MAX (len, 80))) - 1;

	_vte_debug_print(VTE_DEBUG_RING, "Enlarging cell array of %d cells to %d cells\n", cells ? cells->alloc_len : 0, alloc_len);
	cells = g_realloc (cells, G_STRUCT_OFFSET (VteCells, cells) + alloc_len * sizeof (cells->cells[0]));
	cells->alloc_len = alloc_len;

	return cells;
}

static void
_vte_cells_free (VteCells *cells)
{
	_vte_debug_print(VTE_DEBUG_RING, "Freeing cell array of %d cells\n", cells->alloc_len);
	g_free (cells);
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
	VteCell *cells = row->cells;
	_vte_row_data_init (row);
	row->cells = cells;
}

static void
_vte_row_data_fini (VteRowData *row)
{
	if (row->cells)
		_vte_cells_free (_vte_cells_for_cell_array (row->cells));
	row->cells = NULL;
}

static inline gboolean
_vte_row_data_ensure (VteRowData *row, guint len)
{
	VteCells *cells = _vte_cells_for_cell_array (row->cells);
	if (G_LIKELY (cells && len <= cells->alloc_len))
		return TRUE;

	if (G_UNLIKELY (len >= 0xFFFF))
		return FALSE;

	row->cells = _vte_cells_realloc (cells, len)->cells;

	return TRUE;
}

void
_vte_row_data_insert (VteRowData *row, guint col, const VteCell *cell)
{
	guint i;

	if (G_UNLIKELY (!_vte_row_data_ensure (row, row->len + 1)))
		return;

	for (i = row->len; i > col; i--)
		row->cells[i] = row->cells[i - 1];

	row->cells[col] = *cell;
	row->len++;
}

void _vte_row_data_append (VteRowData *row, const VteCell *cell)
{
	if (G_UNLIKELY (!_vte_row_data_ensure (row, row->len + 1)))
		return;

	row->cells[row->len] = *cell;
	row->len++;
}

void _vte_row_data_remove (VteRowData *row, guint col)
{
	guint i;

	for (i = col + 1; i < row->len; i++)
		row->cells[i - 1] = row->cells[i];

	if (G_LIKELY (row->len))
		row->len--;
}

void _vte_row_data_fill (VteRowData *row, const VteCell *cell, guint len)
{
	if (row->len < len) {
		guint i = len - row->len;

		if (G_UNLIKELY (!_vte_row_data_ensure (row, len)))
			return;

		for (i = row->len; i < len; i++)
			row->cells[i] = *cell;

		row->len = len;
	}
}

void _vte_row_data_shrink (VteRowData *row, guint max_len)
{
	if (max_len < row->len)
		row->len = max_len;
}


/*
 * VteCompactRowData: Compact representation of a row
 */

typedef struct _VteRowStorage {
	guint8 charbytes : 3;
	guint8 attrbytes : 3;
} VteRowStorage;
ASSERT_STATIC (sizeof (VteRowStorage) == 1);

typedef struct _VteCompactRowData {
	guchar *bytes;
	guint16 len;
	VteRowAttr attr;
	VteRowStorage storage;
} VteCompactRowData;
ASSERT_STATIC (sizeof (VteCompactRowData) <= 2 * sizeof (void *));


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
_vte_row_storage_compute (const VteRowData *row)
{
	guint i;
	const guint32 *c = (const guint32 *) row->cells;
	guint len = row->len;
	guint32 basic_attrs = basic_cell.i.attr;
	guint32 chars = 0, attrs = 0;
	VteRowStorage storage;

	for (i = 0; i < len; i++) {
		chars |= *c;
		c++;
		attrs |= *c ^ basic_attrs;
		c++;
	}

	storage.charbytes = _width (chars);
	storage.attrbytes = _width (attrs);

	return storage;
}

static inline guint
_vte_row_storage_get_size (VteRowStorage storage, guint len)
{
	return len * (storage.charbytes + storage.attrbytes);
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
_vte_compact_row_init (VteCompactRowData *compact_row, VteRowStorage storage, guchar *bytes)
{
	compact_row->len = 0;
	compact_row->storage = storage;
	compact_row->bytes = bytes;
}

static void
_vte_compact_row_data_compact (VteCompactRowData *compact_row, const VteRowData *row)
{
	guint32 basic_attrs = basic_cell.i.attr;
	guchar *to = compact_row->bytes;
	VteRowStorage storage = compact_row->storage;

	_vte_debug_print(VTE_DEBUG_RING, "Compacting row: %d %d.\n", storage.charbytes, storage.attrbytes);

	compact_row->len = row->len;
	compact_row->attr = row->attr;

	to = _store (to,     (const guint32 *) row->cells, 0,           storage.charbytes, row->len);
	to = _store (to, 1 + (const guint32 *) row->cells, basic_attrs, storage.attrbytes, row->len);
}

static void
_vte_compact_row_data_uncompact (const VteCompactRowData *compact_row, VteRowData *row)
{
	guint32 basic_attrs = basic_cell.i.attr;
	const guchar *from = compact_row->bytes;
	VteRowStorage storage = compact_row->storage;

	_vte_debug_print(VTE_DEBUG_RING, "Uncompacting row: %d %d.\n", storage.charbytes, storage.attrbytes);

	row->attr = compact_row->attr;
	if (G_UNLIKELY (!_vte_row_data_ensure (row, compact_row->len))) {
		row->len = 0;
		return;
	}
	row->len = compact_row->len;

	from = _fetch (from,     (guint32 *) row->cells, 0,           storage.charbytes, compact_row->len);
	from = _fetch (from, 1 + (guint32 *) row->cells, basic_attrs, storage.attrbytes, compact_row->len);
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
	guint bytes_left;
	guchar *cursor; /* move backward */
	union {
		VteCompactRowData rows[1];
		guchar data[1];
	} p;
} VteRingChunkCompact;

static VteRingChunk *
_vte_ring_chunk_new_compact (guint start)
{
	VteRingChunkCompact *chunk;

	_vte_debug_print(VTE_DEBUG_RING, "Allocating compact chunk\n");

	chunk = g_malloc (VTE_RING_CHUNK_COMPACT_BYTES);

	_vte_ring_chunk_init (&chunk->base);
	chunk->base.type = VTE_RING_CHUNK_TYPE_COMPACT;
	chunk->offset = chunk->base.start = chunk->base.end = start;

	chunk->bytes_left = VTE_RING_CHUNK_COMPACT_BYTES - G_STRUCT_OFFSET (VteRingChunkCompact, p);
	chunk->cursor = chunk->p.data + chunk->bytes_left;

	return &chunk->base;
}

static void
_vte_ring_chunk_compact_free (VteRingChunkCompact *bchunk)
{
	_vte_debug_print(VTE_DEBUG_RING, "Freeing compact chunk\n");
	g_assert (bchunk->base.type == VTE_RING_CHUNK_TYPE_COMPACT);
	g_free (bchunk);
}

static inline VteCompactRowData *
_vte_ring_chunk_compact_index (VteRingChunkCompact *chunk, guint position)
{
	return &chunk->p.rows[position - chunk->offset];
}

static gboolean
_vte_ring_chunk_compact_push_head_row (VteRingChunk *bchunk, VteRowData *row)
{
	VteRingChunkCompact *chunk = (VteRingChunkCompact *) bchunk;
	VteRowStorage storage;
	VteCompactRowData *compact_row;
	guint compact_size, total_size;

	storage = _vte_row_storage_compute (row);

	compact_size = _vte_row_storage_get_size (storage, row->len);
	total_size = compact_size + sizeof (chunk->p.rows[0]);

	if (chunk->bytes_left < total_size)
		return FALSE;

	chunk->cursor -= compact_size;
	chunk->bytes_left -= total_size;

	compact_row = _vte_ring_chunk_compact_index (chunk, chunk->base.end);
	_vte_compact_row_init (compact_row, storage, chunk->cursor);
	_vte_compact_row_data_compact (compact_row, row);

	/* Truncate rows of no information */
	if (!compact_size)
		compact_row->len = 0;

	chunk->base.end++;
	return TRUE;
}

static void
_vte_ring_chunk_compact_pop_head_row (VteRingChunk *bchunk, VteRowData *row)
{
	VteRingChunkCompact *chunk = (VteRingChunkCompact *) bchunk;
	const VteCompactRowData *compact_row;
	guint compact_size, total_size;

	compact_row = _vte_ring_chunk_compact_index (chunk, chunk->base.end - 1);

	_vte_compact_row_data_uncompact (compact_row, row);

	compact_size = _vte_row_storage_get_size (compact_row->storage, row->len);
	total_size = compact_size + sizeof (chunk->p.rows[0]);

	chunk->base.end--;
	chunk->cursor += compact_size;
	chunk->bytes_left += total_size;
}


/* Writable chunk type */

static void
_vte_ring_chunk_init_writable (VteRingChunkWritable *chunk)
{
	_vte_ring_chunk_init (&chunk->base);

	chunk->base.type = VTE_RING_CHUNK_TYPE_WRITABLE;
	chunk->mask = 31;
	chunk->array = g_malloc0 (sizeof (chunk->array[0]) * (chunk->mask + 1));
}

static void
_vte_ring_chunk_fini_writable (VteRingChunkWritable *chunk)
{
	guint i;
	g_assert (chunk->base.type == VTE_RING_CHUNK_TYPE_WRITABLE);

	for (i = 0; i <= chunk->mask; i++)
		_vte_row_data_fini (&chunk->array[i]);

	g_free (chunk->array);
	chunk->array = NULL;
}

static inline VteRowData *
_vte_ring_chunk_writable_index (VteRingChunkWritable *chunk, guint position)
{
	return &chunk->array[position & chunk->mask];
}

static void
_vte_ring_chunk_writable_ensure_tail (VteRingChunkWritable *chunk)
{
	guint new_mask, old_mask, i, end;
	VteRowData *old_array, *new_array;;

	if (G_LIKELY (chunk->base.start + chunk->mask > chunk->base.end))
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Enlarging writable array.\n");

	old_mask = chunk->mask;
	old_array = chunk->array;

	chunk->mask = (chunk->mask << 1) + 1;
	chunk->array = g_malloc0 (sizeof (chunk->array[0]) * (chunk->mask + 1));

	new_mask = chunk->mask;
	new_array = chunk->array;

	end = chunk->base.start + old_mask + 1;
	for (i = chunk->base.start; i < end; i++)
		new_array[i & new_mask] = old_array[i & old_mask];

	g_free (old_array);
}

static VteRowData *
_vte_ring_chunk_writable_insert (VteRingChunkWritable *chunk, guint position)
{
	guint i;
	VteRowData *row, tmp;

	tmp = *_vte_ring_chunk_writable_index (chunk, chunk->base.end);
	for (i = chunk->base.end; i > position; i--)
		*_vte_ring_chunk_writable_index (chunk, i) = *_vte_ring_chunk_writable_index (chunk, i - 1);
	*_vte_ring_chunk_writable_index (chunk, position) = tmp;

	row = _vte_ring_chunk_writable_index(chunk, position);
	_vte_row_data_clear (row);
	chunk->base.end++;

	return row;
}

static void
_vte_ring_chunk_writable_remove (VteRingChunkWritable *chunk, guint position)
{
	guint i;
	VteRowData tmp;

	tmp = *_vte_ring_chunk_writable_index (chunk, position);
	for (i = position; i < chunk->base.end - 1; i++)
		*_vte_ring_chunk_writable_index (chunk, i) = *_vte_ring_chunk_writable_index (chunk, i + 1);
	*_vte_ring_chunk_writable_index (chunk, chunk->base.end - 1) = tmp;

	if (chunk->base.end > chunk->base.start)
		chunk->base.end--;
}


/* Generic chunks */

static void
_vte_ring_chunk_free (VteRingChunk *chunk)
{
	g_assert (chunk->type == VTE_RING_CHUNK_TYPE_COMPACT);

	_vte_ring_chunk_compact_free ((VteRingChunkCompact *) chunk);
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
			ring->tail->start, ring->head->base.end - ring->tail->start,
			ring->max, ring->head->base.end - ring->head->base.start);

	g_assert(ring->head->base.end - ring->tail->start <= ring->max);

	g_assert(ring->head->base.start <= ring->head->base.end);
	chunk = ring->head->base.prev_chunk;
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

	ring->tail = ring->cursor = &ring->head->base;

	_vte_ring_chunk_init_writable (ring->head);

	_vte_debug_print(VTE_DEBUG_RING, "New ring %p.\n", ring);
	_vte_ring_validate(ring);
}

void
_vte_ring_fini (VteRing *ring)
{
	VteRingChunk *chunk;

	_vte_row_data_fini (&ring->cached_row);

	chunk = ring->head->base.prev_chunk;
	while (chunk) {
		VteRingChunk *prev_chunk = chunk->prev_chunk;
		_vte_ring_chunk_free (chunk);
		chunk = prev_chunk;
	}

	_vte_ring_chunk_fini_writable (ring->head);
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
	if (G_LIKELY (position >= ring->head->base.start))
		return _vte_ring_chunk_writable_index (ring->head, position);

	if (ring->cached_row_num != position) {
		VteRingChunkCompact *chunk = (VteRingChunkCompact *) _vte_ring_find_chunk (ring, position);
		VteCompactRowData *compact_row = _vte_ring_chunk_compact_index (chunk, position);

		_vte_debug_print(VTE_DEBUG_RING, "Caching row %d.\n", position);

		_vte_compact_row_data_uncompact (compact_row, &ring->cached_row);
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

	if (chunk == &ring->head->base)
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
	VteRingChunk *head = &ring->head->base;

	_vte_debug_print(VTE_DEBUG_RING, "Compacting row %d.\n", head->start);

	row = _vte_ring_chunk_writable_index (ring->head, head->start);

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
	if (G_LIKELY (ring->head->base.start + ring->head->mask == ring->head->base.end))
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
	VteRingChunk *head = &ring->head->base;

	_vte_debug_print(VTE_DEBUG_RING, "Uncompacting row %d.\n", head->start - 1);

	_vte_ring_ensure_writable_tail (ring);

	head->start--;

	if (head->start == ring->cached_row_num)
		/* Invalidate cached row */
		ring->cached_row_num = (guint) -1;

	row = _vte_ring_chunk_writable_index (ring->head, head->start);
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
	if (G_LIKELY (position >= ring->head->base.start))
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Ensure writable %d.\n", position);

	while (position < ring->head->base.start)
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
	while (&ring->head->base != ring->tail && ring->head->base.end - ring->tail->end >= max_rows)
		_vte_ring_free_chunk (ring, ring->tail);

	/* Adjust the start of tail chunk now */
	if (_vte_ring_length (ring) > max_rows)
		ring->tail->start = ring->head->base.end - max_rows;

	ring->max = max_rows;
}

void
_vte_ring_shrink (VteRing *ring, guint max_len)
{
	if (_vte_ring_length (ring) <= max_len)
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Shrinking to %d.\n", max_len);
	_vte_ring_validate(ring);

	if (ring->head->base.start - ring->tail->start <= max_len)
		ring->head->base.end = ring->tail->start + max_len;
	else {
		while (ring->head->base.start - ring->tail->start > max_len) {
			_vte_ring_ensure_writable (ring, ring->head->base.start - 1);
			ring->head->base.end = ring->head->base.start;
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
	g_assert (position <= ring->head->base.end);

	_vte_ring_ensure_writable (ring, position);
	if (position == ring->head->base.start)
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

