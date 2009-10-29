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
			" Delta = %lu, Length = %lu, Max = %lu, Writable = %lu.\n",
			ring->start, ring->end - ring->start,
			ring->max, ring->end - ring->writable);

	g_assert (ring->start <= ring->writable);
	g_assert (ring->writable <= ring->end);

	g_assert (ring->end - ring->start <= ring->max);
	g_assert (ring->end - ring->writable <= ring->mask);
}
#else
#define _vte_ring_validate(ring) G_STMT_START {} G_STMT_END
#endif


void
_vte_ring_init (VteRing *ring, gulong max_rows)
{
	_vte_debug_print(VTE_DEBUG_RING, "New ring %p.\n", ring);

	memset (ring, 0, sizeof (*ring));

	ring->max = MAX (max_rows, 3);

	ring->mask = 31;
	ring->array = g_malloc0 (sizeof (ring->array[0]) * (ring->mask + 1));

	ring->attr_stream = _vte_file_stream_new ();
	ring->text_stream = _vte_file_stream_new ();
	ring->row_stream = _vte_file_stream_new ();

	ring->last_attr.text_offset = 0;
	ring->last_attr.attr.i = 0;
	ring->utf8_buffer = g_string_sized_new (128);

	_vte_row_data_init (&ring->cached_row);
	ring->cached_row_num = (gulong) -1;

	_vte_ring_validate(ring);
}

void
_vte_ring_fini (VteRing *ring)
{
	gulong i;

	for (i = 0; i <= ring->mask; i++)
		_vte_row_data_fini (&ring->array[i]);

	g_free (ring->array);

	g_object_unref (ring->attr_stream);
	g_object_unref (ring->text_stream);
	g_object_unref (ring->row_stream);

	g_string_free (ring->utf8_buffer, TRUE);

	_vte_row_data_fini (&ring->cached_row);
}

typedef struct _VteRowRecord {
	gsize text_offset;
	gsize attr_offset;
} VteRowRecord;

static gboolean
_vte_ring_read_row_record (VteRing *ring, VteRowRecord *record, gulong position)
{
	return _vte_stream_read (ring->row_stream, position * sizeof (*record), (char *) record, sizeof (*record));
}

static void
_vte_ring_append_row_record (VteRing *ring, const VteRowRecord *record, gulong position)
{
	_vte_stream_append (ring->row_stream, (const char *) record, sizeof (*record));
}

static void
_vte_ring_freeze_row (VteRing *ring, gulong position, const VteRowData *row)
{
	VteRowRecord record;
	VteCell *cell;
	GString *buffer = ring->utf8_buffer;
	guint32 basic_attr = basic_cell.i.attr;
	int i;

	_vte_debug_print (VTE_DEBUG_RING, "Freezing row %lu.\n", position);

	record.text_offset = _vte_stream_head (ring->text_stream);
	record.attr_offset = _vte_stream_head (ring->attr_stream);

	g_string_set_size (buffer, 0);
	for (i = 0, cell = row->cells; i < row->len; i++, cell++) {
		VteIntCellAttr attr;
		int num_chars;

		/* Attr storage:
		 *
		 * 1. We don't store attrs for fragments.  They can be
		 * reconstructed using the columns of their start cell.
		 *
		 * 2. We store one attr per vteunistr character starting
		 * from the second character, with columns=0.
		 *
		 * That's enough to reconstruct the attrs, and to store
		 * the text in real UTF-8.
		 */
		attr.s = cell->attr;
		if (G_LIKELY (!attr.s.fragment)) {

			attr.i ^= basic_attr;
			if (ring->last_attr.attr.i != attr.i) {
				ring->last_attr.text_offset = record.text_offset + buffer->len;
				_vte_stream_append (ring->attr_stream, (const char *) &ring->last_attr, sizeof (ring->last_attr));
				if (!buffer->len)
					/* This row doesn't use last_attr, adjust */
					record.attr_offset += sizeof (ring->last_attr);
				ring->last_attr.attr = attr;
			}

			num_chars = _vte_unistr_strlen (cell->c);
			if (num_chars > 1) {
				attr.s = cell->attr;
				attr.s.columns = 0;
				attr.i ^= basic_attr;
				ring->last_attr.text_offset = record.text_offset + buffer->len
							    + g_unichar_to_utf8 (_vte_unistr_get_base (cell->c), NULL);
				_vte_stream_append (ring->attr_stream, (const char *) &ring->last_attr, sizeof (ring->last_attr));
				ring->last_attr.attr = attr;
			}

			_vte_unistr_append_to_string (cell->c, buffer);
		}
	}
	if (!row->attr.soft_wrapped)
		g_string_append_c (buffer, '\n');

	_vte_stream_append (ring->text_stream, buffer->str, buffer->len);
	_vte_ring_append_row_record (ring, &record, position);
}

static void
_vte_ring_thaw_row (VteRing *ring, gulong position, VteRowData *row, gboolean truncate)
{
	VteRowRecord records[2], record;
	VteIntCellAttr attr;
	VteCellAttrChange attr_change;
	VteCell cell;
	const char *p, *q, *end;
	GString *buffer = ring->utf8_buffer;
	guint32 basic_attr = basic_cell.i.attr;

	_vte_debug_print (VTE_DEBUG_RING, "Thawing row %lu.\n", position);

	_vte_row_data_clear (row);

	attr_change.text_offset = 0;

	if (!_vte_ring_read_row_record (ring, &records[0], position))
		return;
	if ((position + 1) * sizeof (records[0]) < _vte_stream_head (ring->row_stream)) {
		if (!_vte_ring_read_row_record (ring, &records[1], position + 1))
			return;
	} else
		records[1].text_offset = _vte_stream_head (ring->text_stream);

	g_string_set_size (buffer, records[1].text_offset - records[0].text_offset);
	if (!_vte_stream_read (ring->text_stream, records[0].text_offset, buffer->str, buffer->len))
		return;

	record = records[0];

	if (G_LIKELY (buffer->len && buffer->str[buffer->len - 1] == '\n'))
		buffer->len--;
	else
		row->attr.soft_wrapped = TRUE;

	p = buffer->str;
	end = p + buffer->len;
	while (p < end) {

		if (record.text_offset >= ring->last_attr.text_offset) {
			attr = ring->last_attr.attr;
		} else {
			if (record.text_offset >= attr_change.text_offset) {
				if (!_vte_stream_read (ring->attr_stream, record.attr_offset, (char *) &attr_change, sizeof (attr_change)))
					return;
				record.attr_offset += sizeof (attr_change);
			}
			attr = attr_change.attr;
		}

		attr.i ^= basic_attr;
		cell.attr = attr.s;
		cell.c = g_utf8_get_char (p);

		q = g_utf8_next_char (p);
		record.text_offset += q - p;
		p = q;

		if (G_UNLIKELY (cell.attr.columns == 0)) {
			if (G_LIKELY (row->len)) {
				/* Combine it */
				row->cells[row->len - 1].c = _vte_unistr_append_unichar (row->cells[row->len - 1].c, cell.c);
			} else {
				cell.attr.columns = 1;
				_vte_row_data_append (row, &cell);
			}
		} else {
			_vte_row_data_append (row, &cell);
			if (cell.attr.columns > 1) {
				/* Add the fragments */
				int i, columns = cell.attr.columns;
				cell.attr.fragment = 1;
				cell.attr.columns = 1;
				for (i = 1; i < columns; i++)
					_vte_row_data_append (row, &cell);
			}
		}
	}

	if (truncate) {
		if (records[0].text_offset < ring->last_attr.text_offset)
			if (!_vte_stream_read (ring->attr_stream, records[0].attr_offset, (char *) &ring->last_attr, sizeof (ring->last_attr))) {
				ring->last_attr.text_offset = 0;
				ring->last_attr.attr.i = 0;
			}
		_vte_stream_truncate (ring->row_stream, position * sizeof (record));
		_vte_stream_truncate (ring->attr_stream, records[0].attr_offset);
		_vte_stream_truncate (ring->text_stream, records[0].text_offset);
	}
}

static void
_vte_ring_reset_streams (VteRing *ring, gulong position)
{
	_vte_debug_print (VTE_DEBUG_RING, "Reseting streams to %lu.\n", position);

	_vte_stream_reset (ring->row_stream, position * sizeof (VteRowRecord));
	_vte_stream_reset (ring->text_stream, 0);
	_vte_stream_reset (ring->attr_stream, 0);

	ring->last_attr.text_offset = 0;
	ring->last_attr.attr.i = 0;

	ring->last_page = position;
}

static void
_vte_ring_new_page (VteRing *ring)
{
	_vte_debug_print (VTE_DEBUG_RING, "Starting new stream page at %lu.\n", ring->writable);

	_vte_stream_new_page (ring->attr_stream);
	_vte_stream_new_page (ring->text_stream);
	_vte_stream_new_page (ring->row_stream);

	ring->last_page = ring->writable;
}



static inline VteRowData *
_vte_ring_writable_index (VteRing *ring, gulong position)
{
	return &ring->array[position & ring->mask];
}

const VteRowData *
_vte_ring_index (VteRing *ring, gulong position)
{
	if (G_LIKELY (position >= ring->writable))
		return _vte_ring_writable_index (ring, position);

	if (ring->cached_row_num != position) {
		_vte_debug_print(VTE_DEBUG_RING, "Caching row %lu.\n", position);
		_vte_ring_thaw_row (ring, position, &ring->cached_row, FALSE);
		ring->cached_row_num = position;
	}

	return &ring->cached_row;
}

static void _vte_ring_ensure_writable (VteRing *ring, gulong position);
static void _vte_ring_ensure_writable_room (VteRing *ring);

VteRowData *
_vte_ring_index_writable (VteRing *ring, gulong position)
{
	_vte_ring_ensure_writable (ring, position);
	return _vte_ring_writable_index (ring, position);
}

static void
_vte_ring_freeze_one_row (VteRing *ring)
{
	VteRowData *row;

	if (G_UNLIKELY (ring->writable == ring->start))
		_vte_ring_reset_streams (ring, ring->writable);

	row = _vte_ring_writable_index (ring, ring->writable);
	_vte_ring_freeze_row (ring, ring->writable, row);

	ring->writable++;

	if (G_UNLIKELY (ring->writable == ring->last_page || ring->writable - ring->last_page >= ring->max))
		_vte_ring_new_page (ring);
}

static void
_vte_ring_thaw_one_row (VteRing *ring)
{
	VteRowData *row;

	g_assert (ring->start < ring->writable);

	_vte_ring_ensure_writable_room (ring);

	ring->writable--;

	if (ring->writable == ring->cached_row_num)
		ring->cached_row_num = (gulong) -1; /* Invalidate cached row */

	row = _vte_ring_writable_index (ring, ring->writable);

	_vte_ring_thaw_row (ring, ring->writable, row, TRUE);
}

static void
_vte_ring_discard_one_row (VteRing *ring)
{
	ring->start++;
	if (G_UNLIKELY (ring->start == ring->writable)) {
		_vte_ring_reset_streams (ring, 0);
	}
	if (ring->start > ring->writable)
		ring->writable = ring->start;
}

static void
_vte_ring_maybe_freeze_one_row (VteRing *ring)
{
	if (G_LIKELY (ring->writable + ring->mask == ring->end))
		_vte_ring_freeze_one_row (ring);
}

static void
_vte_ring_maybe_discard_one_row (VteRing *ring)
{
	if ((gulong) _vte_ring_length (ring) == ring->max)
		_vte_ring_discard_one_row (ring);
}

static void
_vte_ring_ensure_writable_room (VteRing *ring)
{
	gulong new_mask, old_mask, i, end;
	VteRowData *old_array, *new_array;;

	if (G_LIKELY (ring->writable + ring->mask > ring->end))
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
_vte_ring_ensure_writable (VteRing *ring, gulong position)
{
	if (G_LIKELY (position >= ring->writable))
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Ensure writable %lu.\n", position);

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
_vte_ring_resize (VteRing *ring, gulong max_rows)
{
	_vte_debug_print(VTE_DEBUG_RING, "Resizing to %lu.\n", max_rows);
	_vte_ring_validate(ring);

	/* Adjust the start of tail chunk now */
	if ((gulong) _vte_ring_length (ring) > max_rows) {
		ring->start = ring->end - max_rows;
		if (ring->start >= ring->writable) {
			_vte_ring_reset_streams (ring, 0);
			ring->writable = ring->start;
		}
	}

	ring->max = max_rows;
}

void
_vte_ring_shrink (VteRing *ring, gulong max_len)
{
	if ((gulong) _vte_ring_length (ring) <= max_len)
		return;

	_vte_debug_print(VTE_DEBUG_RING, "Shrinking to %lu.\n", max_len);
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
VteRowData *
_vte_ring_insert (VteRing *ring, gulong position)
{
	gulong i;
	VteRowData *row, tmp;

	_vte_debug_print(VTE_DEBUG_RING, "Inserting at position %lu.\n", position);
	_vte_ring_validate(ring);

	_vte_ring_maybe_discard_one_row (ring);

	_vte_ring_ensure_writable (ring, position);
	_vte_ring_ensure_writable_room (ring);

	g_assert (position >= ring->writable && position <= ring->end);

	tmp = *_vte_ring_writable_index (ring, ring->end);
	for (i = ring->end; i > position; i--)
		*_vte_ring_writable_index (ring, i) = *_vte_ring_writable_index (ring, i - 1);
	*_vte_ring_writable_index (ring, position) = tmp;

	row = _vte_ring_writable_index (ring, position);
	_vte_row_data_clear (row);
	ring->end++;

	_vte_ring_maybe_freeze_one_row (ring);

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
_vte_ring_remove (VteRing * ring, gulong position)
{
	gulong i;
	VteRowData tmp;

	_vte_debug_print(VTE_DEBUG_RING, "Removing item at position %lu.\n", position);
	_vte_ring_validate(ring);

	if (G_UNLIKELY (!_vte_ring_contains (ring, position)))
		return;

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
	return _vte_ring_insert (ring, _vte_ring_next (ring));
}

