/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2001-2004,2009,2010 Red Hat, Inc.
 * Copyright © 2008, 2009, 2010 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>

#include <math.h>

#include <glib.h>

#include <vte/vte.h>
#include "vte-private.h"

#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include "iso2022.h"
#include "keymap.h"
#include "marshal.h"
#include "matcher.h"
#include "vteaccess.h"
#include "vteint.h"
#include "vtepty.h"
#include "vtepty-private.h"
#include "vtegtk.hh"

#include <new> /* placement new */

#ifndef HAVE_ROUND
static inline double round(double x) {
	if(x - floor(x) < 0.5) {
		return floor(x);
	} else {
		return ceil(x);
	}
}
#endif

#ifndef HAVE_WINT_T
typedef gunichar wint_t;
#endif

#ifndef howmany
#define howmany(x, y) (((x) + ((y) - 1)) / (y))
#endif

#define WORD_CHAR_EXCEPTIONS_DEFAULT "-#%&+,./=?@\\_~\302\267"

#define I_(string) (g_intern_static_string(string))

static int _vte_unichar_width(gunichar c, int utf8_ambiguous_width);
static gboolean vte_terminal_io_read(GIOChannel *channel,
				     GIOCondition condition,
				     VteTerminal *terminal);
static gboolean vte_terminal_io_write(GIOChannel *channel,
				      GIOCondition condition,
				      VteTerminal *terminal);
static void vte_terminal_background_update(VteTerminal *data);
static void vte_terminal_process_incoming(VteTerminal *terminal);
static void vte_terminal_emit_pending_signals(VteTerminal *terminal);
static gboolean vte_cell_is_selected(VteTerminal *terminal,
				     glong col, glong row, gpointer data);
static void vte_terminal_extend_selection(VteTerminal *terminal, long x, long y,
                                          gboolean always_grow, gboolean force);
static void _vte_terminal_disconnect_pty_read(VteTerminal *terminal);
static void _vte_terminal_disconnect_pty_write(VteTerminal *terminal);
static void vte_terminal_stop_processing (VteTerminal *terminal);

static inline gboolean vte_terminal_is_processing (VteTerminal *terminal);
static inline void vte_terminal_start_processing (VteTerminal *terminal);
static void vte_terminal_add_process_timeout (VteTerminal *terminal);
static void add_update_timeout (VteTerminal *terminal);
static void remove_update_timeout (VteTerminal *terminal);
static void reset_update_regions (VteTerminal *terminal);
static void vte_terminal_update_cursor_blinks_internal(VteTerminal *terminal);
static VteCursorShape _vte_terminal_decscusr_cursor_shape(VteTerminal *terminal);
static VteCursorBlinkMode _vte_terminal_decscusr_cursor_blink(VteTerminal *terminal);

static gboolean process_timeout (gpointer data);
static gboolean update_timeout (gpointer data);
static cairo_region_t *vte_cairo_get_clip_region (cairo_t *cr);
static void vte_terminal_determine_colors_internal(VteTerminal *terminal,
                                                   const VteCellAttr *attr,
                                                   gboolean selected,
                                                   gboolean cursor,
                                                   guint *pfore, guint *pback);

/* these static variables are guarded by the GDK mutex */
static guint process_timeout_tag = 0;
static gboolean in_process_timeout;
static guint update_timeout_tag = 0;
static gboolean in_update_timeout;
static GList *active_terminals;

static int
_vte_unichar_width(gunichar c, int utf8_ambiguous_width)
{
        if (G_LIKELY (c < 0x80))
                return 1;
        if (G_UNLIKELY (g_unichar_iszerowidth (c)))
                return 0;
        if (G_UNLIKELY (g_unichar_iswide (c)))
                return 2;
        if (G_LIKELY (utf8_ambiguous_width == 1))
                return 1;
        if (G_UNLIKELY (g_unichar_iswide_cjk (c)))
                return 2;
        return 1;
}

/* process incoming data without copying */
static struct _vte_incoming_chunk *free_chunks;
static struct _vte_incoming_chunk *
get_chunk (void)
{
	struct _vte_incoming_chunk *chunk = NULL;
	if (free_chunks) {
		chunk = free_chunks;
		free_chunks = free_chunks->next;
	}
	if (chunk == NULL) {
		chunk = g_new (struct _vte_incoming_chunk, 1);
	}
	chunk->next = NULL;
	chunk->len = 0;
	return chunk;
}
static void
release_chunk (struct _vte_incoming_chunk *chunk)
{
	chunk->next = free_chunks;
	chunk->len = free_chunks ? free_chunks->len + 1 : 0;
	free_chunks = chunk;
}
static void
prune_chunks (guint len)
{
	struct _vte_incoming_chunk *chunk = NULL;
	if (len && free_chunks != NULL) {
	    if (free_chunks->len > len) {
		struct _vte_incoming_chunk *last;
		chunk = free_chunks;
		while (free_chunks->len > len) {
		    last = free_chunks;
		    free_chunks = free_chunks->next;
		}
		last->next = NULL;
	    }
	} else {
	    chunk = free_chunks;
	    free_chunks = NULL;
	}
	while (chunk != NULL) {
		struct _vte_incoming_chunk *next = chunk->next;
		g_free (chunk);
		chunk = next;
	}
}
static void
_vte_incoming_chunks_release (struct _vte_incoming_chunk *chunk)
{
	while (chunk) {
		struct _vte_incoming_chunk *next = chunk->next;
		release_chunk (chunk);
		chunk = next;
	}
}
static gsize
_vte_incoming_chunks_length (struct _vte_incoming_chunk *chunk)
{
	gsize len = 0;
	while (chunk) {
		len += chunk->len;
		chunk = chunk->next;
	}
	return len;
}
static gsize
_vte_incoming_chunks_count (struct _vte_incoming_chunk *chunk)
{
	gsize cnt = 0;
	while (chunk) {
		cnt ++;
		chunk = chunk->next;
	}
	return cnt;
}
static struct _vte_incoming_chunk *
_vte_incoming_chunks_reverse(struct _vte_incoming_chunk *chunk)
{
	struct _vte_incoming_chunk *prev = NULL;
	while (chunk) {
		struct _vte_incoming_chunk *next = chunk->next;
		chunk->next = prev;
		prev = chunk;
		chunk = next;
	}
	return prev;
}

static void
vte_g_array_fill(GArray *array, gconstpointer item, guint final_size)
{
	if (array->len >= final_size)
		return;

	final_size -= array->len;
	do {
		g_array_append_vals(array, item, 1);
	} while (--final_size);
}


VteRowData *
_vte_terminal_ring_insert (VteTerminal *terminal, glong position, gboolean fill)
{
	VteRowData *row;
	VteRing *ring = terminal->pvt->screen->row_data;
	while (G_UNLIKELY (_vte_ring_next (ring) < position)) {
		row = _vte_ring_append (ring);
                if (terminal->pvt->fill_defaults.attr.back != VTE_DEFAULT_BG)
                        _vte_row_data_fill (row, &terminal->pvt->fill_defaults, terminal->pvt->column_count);
	}
	row = _vte_ring_insert (ring, position);
        if (fill && terminal->pvt->fill_defaults.attr.back != VTE_DEFAULT_BG)
                _vte_row_data_fill (row, &terminal->pvt->fill_defaults, terminal->pvt->column_count);
	return row;
}

VteRowData *
_vte_terminal_ring_append (VteTerminal *terminal, gboolean fill)
{
	return _vte_terminal_ring_insert (terminal, _vte_ring_next (terminal->pvt->screen->row_data), fill);
}

void
_vte_terminal_ring_remove (VteTerminal *terminal, glong position)
{
	_vte_ring_remove (terminal->pvt->screen->row_data, position);
}

/* Reset defaults for character insertion. */
void
_vte_terminal_set_default_attributes(VteTerminal *terminal)
{
        terminal->pvt->defaults = basic_cell.cell;
        terminal->pvt->color_defaults = terminal->pvt->defaults;
        terminal->pvt->fill_defaults = terminal->pvt->defaults;
}

/* Height excluding padding, but including additional bottom area if not grid aligned */
static inline glong
_vte_terminal_usable_height_px (VteTerminal *terminal)
{
        GtkAllocation allocation;
        gtk_widget_get_allocation (&terminal->widget, &allocation);

        return allocation.height - terminal->pvt->padding.top - terminal->pvt->padding.bottom;
}

static inline glong
_vte_terminal_scroll_delta_pixel (VteTerminal *terminal)
{
        return round(terminal->pvt->screen->scroll_delta * terminal->pvt->char_height);
}

/* Pixel is relative to viewport, top padding excluded.
 * Row is relative to the beginning of the terminal history, like {insert,scroll}_delta. */
static inline glong
_vte_terminal_pixel_to_row (VteTerminal *terminal,
                            glong y)
{
        return (_vte_terminal_scroll_delta_pixel(terminal) + y) / terminal->pvt->char_height;
}

/* Row is relative to the beginning of the terminal history, like {insert,scroll}_delta.
 * Pixel is relative to viewport, top padding excluded. */
static inline glong
_vte_terminal_row_to_pixel (VteTerminal *terminal,
                            glong row)
{
        return row * terminal->pvt->char_height - (glong) round(terminal->pvt->screen->scroll_delta * terminal->pvt->char_height);
}

static inline glong
_vte_terminal_first_displayed_row (VteTerminal *terminal)
{
        return _vte_terminal_pixel_to_row (terminal, 0);
}

static inline glong
_vte_terminal_last_displayed_row (VteTerminal *terminal)
{
        glong r;

        /* Get the logical row number displayed at the bottom pixel position */
        r = _vte_terminal_pixel_to_row (terminal,
                                        _vte_terminal_usable_height_px (terminal) - 1);

        /* If we have an extra padding at the bottom which is currently unused,
         * this number is one too big. Adjust here.
         * E.g. have a terminal of size 80 x 24.5.
         * Initially the bottom displayed row is (0-based) 23, but r is now 24.
         * After producing more than a screenful of content and scrolling back
         * all the way to the top, the bottom displayed row is (0-based) 24. */
        r = MIN (r, terminal->pvt->screen->insert_delta + terminal->pvt->row_count - 1);
        return r;
}

/* x, y are coordinates excluding the padding.
 * col, row are in 0..width-1, 0..height-1.
 * Returns FALSE if clicked over scrollback content; output values are unchanged then.
 */
bool
VteTerminalPrivate::mouse_pixels_to_grid (long x,
                                          long y,
                                          vte::grid::column_t *col,
                                          vte::grid::row_t *row)
{
        long c, r, fr, lr;

        /* Confine clicks to the nearest actual cell. This is especially useful for
         * fullscreen vte so that you can click on the very edge of the screen. */
        r = _vte_terminal_pixel_to_row(m_terminal, y);
        fr = _vte_terminal_first_displayed_row (m_terminal);
        lr = _vte_terminal_last_displayed_row (m_terminal);
        r = CLAMP (r, fr, lr);

        /* Bail out if clicking on scrollback contents: bug 755187. */
        if (r < m_screen->insert_delta)
                return false;
        r -= m_screen->insert_delta;

        c = x / m_char_width;
        c = CLAMP (c, 0, m_column_count - 1);

        *col = c;
        *row = r;
        return true;
}

/* Cause certain cells to be repainted. */
void
_vte_invalidate_cells(VteTerminal *terminal,
		      glong column_start, gint n_columns,
		      glong row_start, gint n_rows)
{
        terminal->pvt->invalidate_cells(column_start, n_columns, row_start, n_rows);
}

void
VteTerminalPrivate::invalidate_cells(vte::grid::column_t column_start,
                                     int n_columns,
                                     vte::grid::row_t row_start,
                                     int n_rows)
{
	cairo_rectangle_int_t rect;
	GtkAllocation allocation;

	if (G_UNLIKELY (!gtk_widget_get_realized(m_widget)))
                return;

	if (!n_columns || !n_rows) {
		return;
	}

	if (m_invalidated_all) {
		return;
	}

	_vte_debug_print (VTE_DEBUG_UPDATES,
			"Invalidating cells at (%ld,%ld)x(%d,%d).\n",
			column_start, row_start,
			n_columns, n_rows);
	_vte_debug_print (VTE_DEBUG_WORK, "?");

	if (n_columns == m_column_count &&
            n_rows == m_row_count) {
		invalidate_all();
		return;
	}

        gtk_widget_get_allocation (m_widget, &allocation);

	/* Convert the column and row start and end to pixel values
	 * by multiplying by the size of a character cell.
	 * Always include the extra pixel border and overlap pixel.
	 */
        rect.x = m_padding.left + column_start * m_char_width - 1;
        if (rect.x <= 0)
                rect.x = 0;
        /* Temporarily misuse rect.width for the end x coordinate... */
        rect.width = m_padding.left + (column_start + n_columns) * m_char_width + 2; /* TODO why 2 and not 1? */
        if (rect.width >= allocation.width)
                rect.width = allocation.width;
        /* ... fix that here */
	rect.width -= rect.x;

        rect.y = m_padding.top + _vte_terminal_row_to_pixel(m_terminal, row_start) - 1;
        if (rect.y <= 0)
                rect.y = 0;

        /* Temporarily misuse rect.height for the end y coordinate... */
        rect.height = m_padding.top + _vte_terminal_row_to_pixel(m_terminal, row_start + n_rows) + 1;
        if (rect.height >= allocation.height)
                rect.height = allocation.height;
        /* ... fix that here */
        rect.height -= rect.y;

        /* Ensure the values make sense */
        if (rect.width <= 0 || rect.height <= 0)
                return;

	_vte_debug_print (VTE_DEBUG_UPDATES,
			"Invalidating pixels at (%d,%d)x(%d,%d).\n",
			rect.x, rect.y, rect.width, rect.height);

	if (m_active != NULL) {
                m_update_regions = g_slist_prepend (
                                                    m_update_regions,
				cairo_region_create_rectangle (&rect));
		/* Wait a bit before doing any invalidation, just in
		 * case updates are coming in really soon. */
		add_update_timeout (m_terminal);
	} else {
		gdk_window_invalidate_rect (gtk_widget_get_window (m_widget), &rect, FALSE);
	}

	_vte_debug_print (VTE_DEBUG_WORK, "!");
}

static void
_vte_invalidate_region (VteTerminal *terminal,
			glong scolumn, glong ecolumn,
			glong srow, glong erow,
			gboolean block)
{
        terminal->pvt->invalidate_region(scolumn, ecolumn, srow, erow, block);
}

void
VteTerminalPrivate::invalidate_region(vte::grid::column_t scolumn,
                                      vte::grid::column_t ecolumn,
                                      vte::grid::row_t srow,
                                      vte::grid::row_t erow,
                                      bool block)
{
	if (block || srow == erow) {
		invalidate_cells(
				scolumn, ecolumn - scolumn + 1,
				srow, erow - srow + 1);
	} else {
		invalidate_cells(
				scolumn,
				column_count - scolumn,
				srow, 1);
		invalidate_cells(
				0, column_count,
				srow + 1, erow - srow - 1);
		invalidate_cells(
				0, ecolumn + 1,
				erow, 1);
	}
}

void
VteTerminalPrivate::invalidate(vte::grid::span s,
                               bool block)
{
        invalidate_region(s.start_column(), s.end_column(), s.start_row(), s.end_row(), block);
}

/* Redraw the entire visible portion of the window. */
void
_vte_invalidate_all(VteTerminal *terminal)
{
        terminal->pvt->invalidate_all();
}

void
VteTerminalPrivate::invalidate_all()
{
	cairo_rectangle_int_t rect;
	GtkAllocation allocation;

	if (G_UNLIKELY (!gtk_widget_get_realized(m_widget)))
                return;

	if (m_invalidated_all) {
		return;
	}

	_vte_debug_print (VTE_DEBUG_WORK, "*");
	_vte_debug_print (VTE_DEBUG_UPDATES, "Invalidating all.\n");

	gtk_widget_get_allocation (m_widget, &allocation);

	/* replace invalid regions with one covering the whole terminal */
	reset_update_regions (m_terminal);
	rect.x = rect.y = 0;
	rect.width = allocation.width;
	rect.height = allocation.height;
	m_invalidated_all = TRUE;

        if (m_active != NULL) {
                m_update_regions = g_slist_prepend (NULL,
				cairo_region_create_rectangle (&rect));
		/* Wait a bit before doing any invalidation, just in
		 * case updates are coming in really soon. */
		add_update_timeout (m_terminal);
	} else {
		gdk_window_invalidate_rect (gtk_widget_get_window (m_widget), &rect, FALSE);
	}
}

/* FIXMEchpe: remove this obsolete function. It became useless long ago
 * when we stopped moving window contents around on scrolling. */
/* Scroll a rectangular region up or down by a fixed number of lines,
 * negative = up, positive = down. */
void
_vte_terminal_scroll_region (VteTerminal *terminal,
			     long row, glong count, glong delta)
{
	if ((delta == 0) || (count == 0)) {
		/* Shenanigans! */
		return;
	}

	if (count >= terminal->pvt->row_count) {
		/* We have to repaint the entire window. */
		_vte_invalidate_all(terminal);
	} else {
		/* We have to repaint the area which is to be
		 * scrolled. */
		_vte_invalidate_cells(terminal,
				     0, terminal->pvt->column_count,
				     row, count);
	}
}

/* Find the row in the given position in the backscroll buffer. */
static inline const VteRowData *
_vte_terminal_find_row_data (VteTerminal *terminal, glong row)
{
	const VteRowData *rowdata = NULL;
	VteScreen *screen = terminal->pvt->screen;
	if (G_LIKELY (_vte_ring_contains (screen->row_data, row))) {
		rowdata = _vte_ring_index (screen->row_data, row);
	}
	return rowdata;
}

/* Find the row in the given position in the backscroll buffer. */
static inline VteRowData *
_vte_terminal_find_row_data_writable (VteTerminal *terminal, glong row)
{
	VteRowData *rowdata = NULL;
	VteScreen *screen = terminal->pvt->screen;
	if (G_LIKELY (_vte_ring_contains (screen->row_data, row))) {
		rowdata = _vte_ring_index_writable (screen->row_data, row);
	}
	return rowdata;
}

/* Find the character an the given position in the backscroll buffer. */
static const VteCell *
vte_terminal_find_charcell(VteTerminal *terminal, gulong col, glong row)
{
	const VteRowData *rowdata;
	const VteCell *ret = NULL;
	VteScreen *screen;
	screen = terminal->pvt->screen;
	if (_vte_ring_contains (screen->row_data, row)) {
		rowdata = _vte_ring_index (screen->row_data, row);
		ret = _vte_row_data_get (rowdata, col);
	}
	return ret;
}

static glong
find_start_column (VteTerminal *terminal, glong col, glong row)
{
	const VteRowData *row_data = _vte_terminal_find_row_data (terminal, row);
	if (G_UNLIKELY (col < 0))
		return col;
	if (row_data != NULL) {
		const VteCell *cell = _vte_row_data_get (row_data, col);
		while (col > 0 && cell != NULL && cell->attr.fragment) {
			cell = _vte_row_data_get (row_data, --col);
		}
	}
	return MAX(col, 0);
}
static glong
find_end_column (VteTerminal *terminal, glong col, glong row)
{
	const VteRowData *row_data = _vte_terminal_find_row_data (terminal, row);
	gint columns = 0;
	if (G_UNLIKELY (col < 0))
		return col;
	if (row_data != NULL) {
		const VteCell *cell = _vte_row_data_get (row_data, col);
		while (col > 0 && cell != NULL && cell->attr.fragment) {
			cell = _vte_row_data_get (row_data, --col);
		}
		if (cell) {
			columns = cell->attr.columns - 1;
		}
	}
	return MIN(col + columns, terminal->pvt->column_count);
}


/* Determine the width of the portion of the preedit string which lies
 * to the left of the cursor, or the entire string, in columns. */
static gssize
vte_terminal_preedit_width(VteTerminal *terminal, gboolean left_only)
{
	gunichar c;
	int i;
	gssize ret = 0;
	const char *preedit = NULL;

	if (terminal->pvt->im_preedit != NULL) {
		preedit = terminal->pvt->im_preedit;
		for (i = 0;
		     (preedit != NULL) &&
		     (preedit[0] != '\0') &&
		     (!left_only || (i < terminal->pvt->im_preedit_cursor));
		     i++) {
			c = g_utf8_get_char(preedit);
                        ret += _vte_unichar_width(c, terminal->pvt->utf8_ambiguous_width);
			preedit = g_utf8_next_char(preedit);
		}
	}

	return ret;
}

/* Determine the length of the portion of the preedit string which lies
 * to the left of the cursor, or the entire string, in gunichars. */
static gssize
vte_terminal_preedit_length(VteTerminal *terminal, gboolean left_only)
{
	int i = 0;
	const char *preedit = NULL;

	if (terminal->pvt->im_preedit != NULL) {
		preedit = terminal->pvt->im_preedit;
		for (i = 0;
		     (preedit != NULL) &&
		     (preedit[0] != '\0') &&
		     (!left_only || (i < terminal->pvt->im_preedit_cursor));
		     i++) {
			preedit = g_utf8_next_char(preedit);
		}
	}

	return i;
}

/* Cause the cell to be redrawn. */
void
_vte_invalidate_cell(VteTerminal *terminal, glong col, glong row)
{
        terminal->pvt->invalidate_cell(col, row);
}

void
VteTerminalPrivate::invalidate_cell(vte::grid::column_t col,
                                    vte::grid::row_t row)
{
	const VteRowData *row_data;
	int columns;
	guint style;

	if (G_UNLIKELY (!gtk_widget_get_realized(m_widget)))
                return;

	if (m_invalidated_all) {
		return;
	}

	columns = 1;
	row_data = _vte_terminal_find_row_data(m_terminal, row);
	if (row_data != NULL) {
		const VteCell *cell;
		cell = _vte_row_data_get (row_data, col);
		if (cell != NULL) {
			while (cell->attr.fragment && col> 0) {
				cell = _vte_row_data_get (row_data, --col);
			}
			columns = cell->attr.columns;
			style = _vte_draw_get_style(cell->attr.bold, cell->attr.italic);
			if (cell->c != 0 &&
					_vte_draw_get_char_width (
                                                                  m_draw,
						cell->c, columns, style) >
					m_char_width * columns) {
				columns++;
			}
		}
	}

	_vte_debug_print(VTE_DEBUG_UPDATES,
			"Invalidating cell at (%ld,%ld-%ld).\n",
			row, col, col + columns);

        invalidate_cells(col, columns, row, 1);
}

/* Cause the cursor to be redrawn. */
void
_vte_invalidate_cursor_once(VteTerminal *terminal, gboolean periodic)
{
        terminal->pvt->invalidate_cursor_once(periodic);
}

void
VteTerminalPrivate::invalidate_cursor_once(bool periodic)
{
        if (G_UNLIKELY(!gtk_widget_get_realized(m_widget)))
                return;

	if (m_invalidated_all) {
		return;
	}

	if (periodic) {
		if (!m_cursor_blinks) {
			return;
		}
	}

	if (m_cursor_visible) {
		auto preedit_width = vte_terminal_preedit_width(m_terminal, FALSE);
                auto row = m_cursor.row;
                auto column = m_cursor.col;
		long columns = 1;
		column = find_start_column (m_terminal, column, row);

		auto cell = vte_terminal_find_charcell(m_terminal, column, row);
		if (cell != NULL) {
			columns = cell->attr.columns;
			auto style = _vte_draw_get_style(cell->attr.bold, cell->attr.italic);
			if (cell->c != 0 &&
					_vte_draw_get_char_width (
						m_draw,
						cell->c,
						columns, style) >
			    m_char_width * columns) {
				columns++;
			}
		}
		columns = MAX(columns, preedit_width);
		if (column + columns > m_column_count) {
			column = MAX(0, m_column_count - columns);
		}

		_vte_debug_print(VTE_DEBUG_UPDATES,
				"Invalidating cursor at (%ld,%ld-%ld).\n",
				row, column, column + columns);
		invalidate_cells(
				     column, columns,
				     row, 1);
	}
}

/* Invalidate the cursor repeatedly. */
// FIXMEchpe this continually adds and removes the blink timeout. Find a better solution
static gboolean
invalidate_cursor_periodic_cb(VteTerminalPrivate *that)
{
        that->invalidate_cursor_periodic();
        return G_SOURCE_REMOVE;
}

void
VteTerminalPrivate::invalidate_cursor_periodic()
{
	m_cursor_blink_state = !m_cursor_blink_state;
	m_cursor_blink_time += m_cursor_blink_cycle;

        m_cursor_blink_tag = 0;
	invalidate_cursor_once(true);

	/* only disable the blink if the cursor is currently shown.
	 * else, wait until next time.
	 */
	if (m_cursor_blink_time / 1000 >= m_cursor_blink_timeout &&
	    m_cursor_blink_state) {
		return;
        }

	m_cursor_blink_tag = g_timeout_add_full(G_PRIORITY_LOW,
                                                m_cursor_blink_cycle,
                                                (GSourceFunc)invalidate_cursor_periodic_cb,
                                                this,
                                                NULL);
}

/* Emit a "selection_changed" signal. */
static void
vte_terminal_emit_selection_changed(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `selection-changed'.\n");
	g_signal_emit_by_name(terminal, "selection-changed");
}

/* Emit a "commit" signal. */
static void
vte_terminal_emit_commit(VteTerminal *terminal, const gchar *text, gssize length)
{
	const char *result = NULL;
	char *wrapped = NULL;

	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `commit' of %" G_GSSIZE_FORMAT" bytes.\n", length);

	if (length == -1) {
		length = strlen(text);
		result = text;
	} else {
		result = wrapped = (char *) g_slice_alloc(length + 1);
		memcpy(wrapped, text, length);
		wrapped[length] = '\0';
	}

	g_signal_emit_by_name(terminal, "commit", result, length);

	if(wrapped)
		g_slice_free1(length+1, wrapped);
}

/* Emit an "encoding-changed" signal. */
static void
vte_terminal_emit_encoding_changed(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `encoding-changed'.\n");
	g_signal_emit_by_name(terminal, "encoding-changed");
        g_object_notify(G_OBJECT(terminal), "encoding");
}

/* Emit a "child-exited" signal. */
static void
vte_terminal_emit_child_exited(VteTerminal *terminal,
                               int status)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `child-exited'.\n");
	g_signal_emit_by_name(terminal, "child-exited", status);
}

/* Emit a "contents_changed" signal. */
static void
vte_terminal_emit_contents_changed(VteTerminal *terminal)
{
	if (terminal->pvt->contents_changed_pending) {
		/* Update dingus match set. */
		terminal->pvt->match_contents_clear();
		if (terminal->pvt->mouse_cursor_visible) {
			terminal->pvt->match_hilite_update(
					terminal->pvt->mouse_last_x,
					terminal->pvt->mouse_last_y);
		}

		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting `contents-changed'.\n");
		g_signal_emit_by_name(terminal, "contents-changed");
		terminal->pvt->contents_changed_pending = FALSE;
	}
}
void
_vte_terminal_queue_contents_changed(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Queueing `contents-changed'.\n");
	terminal->pvt->contents_changed_pending = TRUE;
}

/* Emit a "cursor_moved" signal. */
static void
vte_terminal_emit_cursor_moved(VteTerminal *terminal)
{
	if (terminal->pvt->cursor_moved_pending) {
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting `cursor-moved'.\n");
		g_signal_emit_by_name(terminal, "cursor-moved");
		terminal->pvt->cursor_moved_pending = FALSE;
	}
}
static void
vte_terminal_queue_cursor_moved(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Queueing `cursor-moved'.\n");
	terminal->pvt->cursor_moved_pending = TRUE;
}

static gboolean
vte_terminal_emit_eof(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `eof'.\n");
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	gdk_threads_enter ();
        G_GNUC_END_IGNORE_DEPRECATIONS;

	g_signal_emit_by_name(terminal, "eof");

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	gdk_threads_leave ();
        G_GNUC_END_IGNORE_DEPRECATIONS;

	return FALSE;
}
/* Emit a "eof" signal. */
static void
vte_terminal_queue_eof(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Queueing `eof'.\n");
	g_idle_add_full (G_PRIORITY_HIGH,
		(GSourceFunc) vte_terminal_emit_eof,
		g_object_ref (terminal),
		g_object_unref);
}

/* Emit a "char-size-changed" signal. */
static void
vte_terminal_emit_char_size_changed(VteTerminal *terminal,
				    guint width, guint height)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `char-size-changed'.\n");
	g_signal_emit_by_name(terminal, "char-size-changed",
			      width, height);
/*         g_object_notify(G_OBJECT(terminal), "char-size"); */
}

/* Emit an "increase-font-size" signal. */
static void
vte_terminal_emit_increase_font_size(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `increase-font-size'.\n");
	g_signal_emit_by_name(terminal, "increase-font-size");
}

/* Emit a "decrease-font-size" signal. */
static void
vte_terminal_emit_decrease_font_size(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `decrease-font-size'.\n");
	g_signal_emit_by_name(terminal, "decrease-font-size");
}

/* Emit a "text-inserted" signal. */
void
_vte_terminal_emit_text_inserted(VteTerminal *terminal)
{
	if (!terminal->pvt->accessible_emit) {
		return;
	}
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `text-inserted'.\n");
	g_signal_emit_by_name(terminal, "text-inserted");
}

/* Emit a "text-deleted" signal. */
void
_vte_terminal_emit_text_deleted(VteTerminal *terminal)
{
	if (!terminal->pvt->accessible_emit) {
		return;
	}
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `text-deleted'.\n");
	g_signal_emit_by_name(terminal, "text-deleted");
}

/* Emit a "text-modified" signal. */
static void
vte_terminal_emit_text_modified(VteTerminal *terminal)
{
	if (!terminal->pvt->accessible_emit) {
		return;
	}
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `text-modified'.\n");
	g_signal_emit_by_name(terminal, "text-modified");
}

/* Emit a "text-scrolled" signal. */
static void
vte_terminal_emit_text_scrolled(VteTerminal *terminal, gint delta)
{
	if (!terminal->pvt->accessible_emit) {
		return;
	}
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `text-scrolled'(%d).\n", delta);
	g_signal_emit_by_name(terminal, "text-scrolled", delta);
}

void
VteTerminalPrivate::deselect_all()
{
	if (m_has_selection) {
		gint sx, sy, ex, ey, extra;

		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Deselecting all text.\n");

		m_has_selection = FALSE;
		/* Don't free the current selection, as we need to keep
		 * hold of it for async copying from the clipboard. */

		vte_terminal_emit_selection_changed(m_terminal);

		sx = m_selection_start.col;
		sy = m_selection_start.row;
		ex = m_selection_end.col;
		ey = m_selection_end.row;
                extra = m_selection_block_mode ? (VTE_TAB_WIDTH_MAX - 1) : 0;
		invalidate_region(
				MIN (sx, ex), MAX (sx, ex) + extra,
				MIN (sy, ey),   MAX (sy, ey),
				false);
	}
}

/* Remove a tabstop. */
void
_vte_terminal_clear_tabstop(VteTerminal *terminal, int column)
{
	g_assert(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->tabstops != NULL) {
		/* Remove a tab stop from the hash table. */
		g_hash_table_remove(terminal->pvt->tabstops,
				    GINT_TO_POINTER(2 * column + 1));
	}
}

/* Check if we have a tabstop at a given position. */
gboolean
_vte_terminal_get_tabstop(VteTerminal *terminal, int column)
{
	gpointer hash;
	g_assert(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->tabstops != NULL) {
		hash = g_hash_table_lookup(terminal->pvt->tabstops,
					   GINT_TO_POINTER(2 * column + 1));
		return (hash != NULL);
	} else {
		return FALSE;
	}
}

/* Reset the set of tab stops to the default. */
void
_vte_terminal_set_tabstop(VteTerminal *terminal, int column)
{
	g_assert(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->tabstops != NULL) {
		/* Just set a non-NULL pointer for this column number. */
		g_hash_table_insert(terminal->pvt->tabstops,
				    GINT_TO_POINTER(2 * column + 1),
				    terminal);
	}
}

/* Reset the set of tab stops to the default. */
static void
vte_terminal_set_default_tabstops(VteTerminal *terminal)
{
        int i;
	if (terminal->pvt->tabstops != NULL) {
		g_hash_table_destroy(terminal->pvt->tabstops);
	}
	terminal->pvt->tabstops = g_hash_table_new(NULL, NULL);
        for (i = 0; i <= VTE_TAB_MAX; i += VTE_TAB_WIDTH) {
		_vte_terminal_set_tabstop(terminal, i);
	}
}

/* Clear the cache of the screen contents we keep. */
void
VteTerminalPrivate::match_contents_clear()
{
	if (m_match_contents != nullptr) {
		g_free(m_match_contents);
		m_match_contents = nullptr;
	}
	if (m_match_attributes != nullptr) {
		g_array_free(m_match_attributes, TRUE);
		m_match_attributes = nullptr;
	}
	match_hilite_clear();
}

/* Refresh the cache of the screen contents we keep. */
static gboolean
always_selected(VteTerminal *terminal, glong column, glong row, gpointer data)
{
	return TRUE;
}

void
VteTerminalPrivate::match_contents_refresh()

{
        auto start_row = _vte_terminal_first_displayed_row (m_terminal);
        auto start_col = 0;
        auto end_row = _vte_terminal_last_displayed_row (m_terminal);
        auto end_col = m_column_count - 1;

	match_contents_clear();
	GArray *array = g_array_new(FALSE, TRUE, sizeof(struct _VteCharAttributes));
        m_match_contents = vte_terminal_get_text_range(m_terminal,
                                                                    start_row, start_col,
                                                                    end_row, end_col,
                                                                    always_selected,
                                                                    NULL,
                                                                    array);
	m_match_attributes = array;
}

static void
regex_match_clear_cursor (struct vte_match_regex *regex)
{
        switch (regex->cursor_mode) {
                case VTE_REGEX_CURSOR_GDKCURSOR:
                        if (regex->cursor.cursor != NULL) {
                                g_object_unref(regex->cursor.cursor);
                                regex->cursor.cursor = NULL;
                        }
                        break;
                case VTE_REGEX_CURSOR_GDKCURSORTYPE:
                        break;
                case VTE_REGEX_CURSOR_NAME:
                        g_free (regex->cursor.cursor_name);
                        regex->cursor.cursor_name = NULL;
                        break;
		default:
			g_assert_not_reached ();
			return;
        }
}

static void
regex_and_flags_clear(struct vte_regex_and_flags *regex)
{
        if (regex->mode == VTE_REGEX_PCRE2) {
                vte_regex_unref(regex->pcre.regex);
                regex->pcre.regex = NULL;
        } else if (regex->mode == VTE_REGEX_GREGEX) {
                g_regex_unref(regex->gregex.regex);
                regex->gregex.regex = NULL;
        }
        regex->mode = VTE_REGEX_UNDECIDED;
}

static void
regex_match_clear (struct vte_match_regex *regex)
{
        regex_and_flags_clear(&regex->regex);
        regex_match_clear_cursor(regex);

        regex->tag = -1;
}

void
VteTerminalPrivate::set_cursor_from_regex_match(struct vte_match_regex *regex)
{
        GdkCursor *gdk_cursor = nullptr;

        if (!gtk_widget_get_realized(m_widget))
                return;

        switch (regex->cursor_mode) {
                case VTE_REGEX_CURSOR_GDKCURSOR:
                        if (regex->cursor.cursor != NULL &&
                            gdk_cursor_get_display(regex->cursor.cursor) == gtk_widget_get_display(m_widget)) {
                                gdk_cursor = (GdkCursor *)g_object_ref(regex->cursor.cursor);
                        }
                        break;
                case VTE_REGEX_CURSOR_GDKCURSORTYPE:
                        gdk_cursor = gdk_cursor_new_for_display(gtk_widget_get_display(m_widget), regex->cursor.cursor_type);
                        break;
                case VTE_REGEX_CURSOR_NAME:
                        gdk_cursor = gdk_cursor_new_from_name(gtk_widget_get_display(m_widget), regex->cursor.cursor_name);
                        break;
		default:
			g_assert_not_reached ();
			return;
        }

	gdk_window_set_cursor(gtk_widget_get_window(m_widget), gdk_cursor);

        if (gdk_cursor)
                g_object_unref(gdk_cursor);
}

void
VteTerminalPrivate::regex_match_remove_all()
{
	struct vte_match_regex *regex;
	guint i;

	for (i = 0; i < m_match_regexes->len; i++) {
		regex = &g_array_index(m_match_regexes,
				       struct vte_match_regex,
				       i);
		/* Unless this is a hole, clean it up. */
		if (regex->tag >= 0) {
                        regex_match_clear (regex);
		}
	}
	g_array_set_size(m_match_regexes, 0);

	match_hilite_clear();
}

void
VteTerminalPrivate::regex_match_remove(int tag)
{
	struct vte_match_regex *regex;

	if (m_match_regexes->len > (guint)tag) {
		/* The tag is an index, so find the corresponding struct. */
		regex = &g_array_index(m_match_regexes,
				       struct vte_match_regex,
				       tag);
		/* If it's already been removed, return. */
		if (regex->tag < 0) {
			return;
		}
		/* Remove this item and leave a hole in its place. */
                regex_match_clear (regex);
	}
	match_hilite_clear();
}

GdkCursor *
_vte_terminal_cursor_new(VteTerminal *terminal, GdkCursorType cursor_type)
{
	GdkDisplay *display;
	GdkCursor *cursor;

	display = gtk_widget_get_display(&terminal->widget);
	cursor = gdk_cursor_new_for_display(display, cursor_type);
	return cursor;
}

int
VteTerminalPrivate::regex_match_add(struct vte_match_regex *new_regex_match)
{
        struct vte_match_regex *regex_match;
        guint ret, len;

        g_assert(m_match_regex_mode == VTE_REGEX_UNDECIDED || m_match_regex_mode == new_regex_match->regex.mode);
        m_match_regex_mode = new_regex_match->regex.mode;

        /* Search for a hole. */
        len = m_match_regexes->len;
        for (ret = 0; ret < len; ret++) {
                regex_match = &g_array_index(m_match_regexes,
                                             struct vte_match_regex,
                                             ret);
                if (regex_match->tag == -1) {
                        break;
                }
        }

        /* Set the tag to the insertion point. */
        new_regex_match->tag = ret;

        if (ret < len) {
                /* Overwrite. */
                g_array_index(m_match_regexes,
                              struct vte_match_regex,
                              ret) = *new_regex_match;
        } else {
                /* Append. */
                g_array_append_vals(m_match_regexes, new_regex_match, 1);
        }

        return ret;
}

struct vte_match_regex *
VteTerminalPrivate::regex_match_get(int tag)
{
	if ((guint)tag >= m_match_regexes->len)
                return nullptr;

	return &g_array_index(m_match_regexes, struct vte_match_regex, tag);
}

void
VteTerminalPrivate::regex_match_set_cursor(int tag,
                                           GdkCursor *gdk_cursor)
{
        struct vte_match_regex *regex = regex_match_get(tag);
        if (regex == nullptr)
                return;

        regex_match_clear_cursor(regex);
        regex->cursor_mode = VTE_REGEX_CURSOR_GDKCURSOR;
	regex->cursor.cursor = gdk_cursor ? (GdkCursor *)g_object_ref(gdk_cursor) : NULL;

	match_hilite_clear();
}

void
VteTerminalPrivate::regex_match_set_cursor(int tag,
                                           GdkCursorType cursor_type)
{
        struct vte_match_regex *regex = regex_match_get(tag);
        if (regex == nullptr)
                return;

        regex_match_clear_cursor(regex);
        regex->cursor_mode = VTE_REGEX_CURSOR_GDKCURSORTYPE;
	regex->cursor.cursor_type = cursor_type;

	match_hilite_clear();
}

void
VteTerminalPrivate::regex_match_set_cursor(int tag,
                                           char const* cursor_name)
{
        struct vte_match_regex *regex = regex_match_get(tag);
        if (regex == nullptr)
                return;

        regex_match_clear_cursor(regex);
        regex->cursor_mode = VTE_REGEX_CURSOR_NAME;
	regex->cursor.cursor_name = g_strdup (cursor_name);

	match_hilite_clear();
}

/*
 * match_rowcol_to_offset:
 * @terminal:
 * @column:
 * @row:
 * @offset_ptr: (out):
 * @sattr_ptr: (out):
 * @ettr_ptr: (out):
 *
 * Maps (row, column) to an offset in pvt->match_attributes, and returns
 * that offset in @offset_ptr, and the start and end of the corresponding
 * line in @sattr_ptr and @eattr_ptr.
 */
bool
VteTerminalPrivate::match_rowcol_to_offset(vte::grid::column_t column,
                                           vte::grid::row_t row,
                                           gsize *offset_ptr,
                                           gsize *sattr_ptr,
                                           gsize *eattr_ptr)
{
        /* FIXME: use gsize, after making sure the code below doesn't underflow offset */
        gssize offset, sattr, eattr;
        struct _VteCharAttributes *attr = NULL;

	/* Map the pointer position to a portion of the string. */
        // FIXME do a bsearch here?
	eattr = match_attributes->len;
	for (offset = eattr; offset--; ) {
		attr = &g_array_index(m_match_attributes,
				      struct _VteCharAttributes,
				      offset);
		if (row < attr->row) {
			eattr = offset;
		}
		if (row == attr->row &&
		    column == attr->column &&
		    m_match_contents[offset] != ' ') {
			break;
		}
	}

	_VTE_DEBUG_IF(VTE_DEBUG_REGEX) {
		if (offset < 0)
			g_printerr("Cursor is not on a character.\n");
		else {
                        gunichar c;
                        char utf[7];
                        c = g_utf8_get_char (m_match_contents + offset);
                        utf[g_unichar_to_utf8(g_unichar_isprint(c) ? c : 0xFFFD, utf)] = 0;

			g_printerr("Cursor is on character U+%04X '%s' at %" G_GSSIZE_FORMAT ".\n",
                                   c, utf, offset);
                }
	}

	/* If the pointer isn't on a matchable character, bug out. */
	if (offset < 0) {
		return false;
	}

	/* If the pointer is on a newline, bug out. */
	if ((g_ascii_isspace(m_match_contents[offset])) ||
	    (m_match_contents[offset] == '\0')) {
		_vte_debug_print(VTE_DEBUG_EVENTS,
                                 "Cursor is on whitespace.\n");
		return false;
	}

	/* Snip off any final newlines. */
	while (m_match_contents[eattr] == '\n' ||
               m_match_contents[eattr] == '\0') {
		eattr--;
	}
	/* and scan forwards to find the end of this line */
	while (!(m_match_contents[eattr] == '\n' ||
                 m_match_contents[eattr] == '\0')) {
		eattr++;
	}

	/* find the start of row */
	if (row == 0) {
		sattr = 0;
	} else {
		for (sattr = offset; sattr > 0; sattr--) {
			attr = &g_array_index(m_match_attributes,
					      struct _VteCharAttributes,
					      sattr);
			if (row > attr->row) {
				break;
			}
		}
	}
	/* Scan backwards to find the start of this line */
	while (sattr > 0 &&
		! (m_match_contents[sattr] == '\n' ||
                   m_match_contents[sattr] == '\0')) {
		sattr--;
	}
	/* and skip any initial newlines. */
	while (m_match_contents[sattr] == '\n' ||
               m_match_contents[sattr] == '\0') {
		sattr++;
	}
	if (eattr <= sattr) { /* blank line */
		return false;
	}
	if (eattr <= offset || sattr > offset) {
		/* nothing to match on this line */
		return false;
	}

        *offset_ptr = offset;
        *sattr_ptr = sattr;
        *eattr_ptr = eattr;

        _VTE_DEBUG_IF(VTE_DEBUG_REGEX) {
                struct _VteCharAttributes *_sattr, *_eattr;
                _sattr = &g_array_index(m_match_attributes,
                                        struct _VteCharAttributes,
                                        sattr);
                _eattr = &g_array_index(m_match_attributes,
                                        struct _VteCharAttributes,
                                        eattr - 1);
                g_printerr("Cursor is in line from %" G_GSIZE_FORMAT "(%ld,%ld) to %" G_GSIZE_FORMAT "(%ld,%ld)\n",
                           sattr, _sattr->column, _sattr->row,
                           eattr - 1, _eattr->column, _eattr->row);
        }

        return true;
}

#ifdef WITH_PCRE2

/* creates a pcre match context with appropriate limits */
pcre2_match_context_8 *
VteTerminalPrivate::create_match_context()
{
        pcre2_match_context_8 *match_context;

        match_context = pcre2_match_context_create_8(nullptr /* general context */);
        pcre2_set_match_limit_8(match_context, 65536); /* should be plenty */
        pcre2_set_recursion_limit_8(match_context, 64); /* should be plenty */

        return match_context;
}

bool
VteTerminalPrivate::match_check_pcre(
                 pcre2_match_data_8 *match_data,
                 pcre2_match_context_8 *match_context,
                 VteRegex *regex,
                 guint32 match_flags,
                 gsize sattr,
                 gsize eattr,
                 gsize offset,
                 char **result_ptr,
                 gsize *start,
                 gsize *end,
                 gsize *sblank_ptr,
                 gsize *eblank_ptr)
{
        int (* match_fn) (const pcre2_code_8 *,
                          PCRE2_SPTR8, PCRE2_SIZE, PCRE2_SIZE, uint32_t,
                          pcre2_match_data_8 *, pcre2_match_context_8 *);
        gsize sblank = 0, eblank = G_MAXSIZE;
        gsize position, line_length;
        const char *line;
        int r = 0;

        if (_vte_regex_get_jited(regex))
                match_fn = pcre2_jit_match_8;
        else
                match_fn = pcre2_match_8;

        line = m_match_contents;
        /* FIXME: what we really want is to pass the whole data to pcre2_match, but
         * limit matching to between sattr and eattr, so that the extra data can
         * satisfy lookahead assertions. This needs new pcre2 API though.
         */
        line_length = eattr;

        /* Iterate throught the matches until we either find one which contains the
         * offset, or we get no more matches.
         */
        position = sattr;
        while (position < eattr &&
               ((r = match_fn(_vte_regex_get_pcre(regex),
                              (PCRE2_SPTR8)line, line_length, /* subject, length */
                              position, /* start offset */
                              match_flags |
                              PCRE2_NO_UTF_CHECK | PCRE2_NOTEMPTY | PCRE2_PARTIAL_SOFT /* FIXME: HARD? */,
                              match_data,
                              match_context)) >= 0 || r == PCRE2_ERROR_PARTIAL)) {
                gsize ko = offset;
                gsize rm_so, rm_eo;
                gsize *ovector;

                ovector = pcre2_get_ovector_pointer_8(match_data);
                rm_so = ovector[0];
                rm_eo = ovector[1];
                if (G_UNLIKELY(rm_so == PCRE2_UNSET || rm_eo == PCRE2_UNSET))
                        break;

                /* The offsets should be "sane". We set NOTEMPTY, but check anyway */
                if (G_UNLIKELY(position == rm_eo)) {
                        /* rm_eo is before the end of subject string's length, so this is safe */
                        position = g_utf8_next_char(line + rm_eo) - line;
                        continue;
                }

                _VTE_DEBUG_IF(VTE_DEBUG_REGEX) {
                        gchar *result;
                        struct _VteCharAttributes *_sattr, *_eattr;
                        result = g_strndup(line + rm_so, rm_eo - rm_so);
                        _sattr = &g_array_index(m_match_attributes,
                                                struct _VteCharAttributes,
                                                rm_so);
                        _eattr = &g_array_index(m_match_attributes,
                                                struct _VteCharAttributes,
                                                rm_eo - 1);
                        g_printerr("%s match `%s' from %" G_GSIZE_FORMAT "(%ld,%ld) to %" G_GSIZE_FORMAT "(%ld,%ld) (%" G_GSSIZE_FORMAT ").\n",
                                   r == PCRE2_ERROR_PARTIAL ? "Partial":"Full",
                                   result,
                                   rm_so,
                                   _sattr->column,
                                   _sattr->row,
                                   rm_eo - 1,
                                   _eattr->column,
                                   _eattr->row,
                                   offset);
                        g_free(result);
                }

                /* advance position */
                position = rm_eo;

                /* FIXME: do handle newline / partial matches at end of line/start of next line */
                if (r == PCRE2_ERROR_PARTIAL)
                        continue;

                /* If the pointer is in this substring, then we're done. */
                if (ko >= rm_so && ko < rm_eo) {
                        *result_ptr = g_strndup(line + rm_so, rm_eo - rm_so);
                        *start = rm_so;
                        *end = rm_eo - 1;
                        return true;
                }

                if (ko >= rm_eo && rm_eo > sblank) {
                        sblank = rm_eo;
                }
                if (ko < rm_so && rm_so < eblank) {
                        eblank = rm_so;
                }
        }

        if (G_UNLIKELY(r < PCRE2_ERROR_PARTIAL))
                _vte_debug_print(VTE_DEBUG_REGEX, "Unexpected pcre2_match error code: %d\n", r);

        *sblank_ptr = sblank;
        *eblank_ptr = eblank;
        return false;
}

char *
VteTerminalPrivate::match_check_internal_pcre(vte::grid::column_t column,
                                              vte::grid::row_t row,
                                              int *tag,
                                              gsize *start,
                                              gsize *end)
{
        struct vte_match_regex *regex;
        guint i;
	gsize offset, sattr, eattr, start_blank, end_blank;
        pcre2_match_data_8 *match_data;
        pcre2_match_context_8 *match_context;
        char *dingu_match = nullptr;

	_vte_debug_print(VTE_DEBUG_REGEX,
                         "Checking for pcre match at (%ld,%ld).\n", row, column);

        if (!match_rowcol_to_offset(column, row,
                                    &offset, &sattr, &eattr))
                return nullptr;

	start_blank = sattr;
	end_blank = eattr;

        match_context = create_match_context();
        match_data = pcre2_match_data_create_8(256 /* should be plenty */, NULL /* general context */);

	/* Now iterate over each regex we need to match against. */
	for (i = 0; i < m_match_regexes->len; i++) {
                gsize sblank, eblank;

		regex = &g_array_index(m_match_regexes,
				       struct vte_match_regex,
				       i);
		/* Skip holes. */
		if (regex->tag < 0) {
			continue;
		}

                g_assert_cmpint(regex->regex.mode, ==, VTE_REGEX_PCRE2);

                if (match_check_pcre(match_data, match_context,
                                     regex->regex.pcre.regex,
                                     regex->regex.pcre.match_flags,
                                     sattr, eattr, offset,
                                     &dingu_match,
                                     start, end,
                                     &sblank, &eblank)) {
                        _vte_debug_print(VTE_DEBUG_REGEX, "Matched dingu with tag %d\n", regex->tag);
                        set_cursor_from_regex_match(regex);
                        *tag = regex->tag;
                        break;
                }

                if (sblank > start_blank) {
                        start_blank = sblank;
                }
                if (eblank < end_blank) {
                        end_blank = eblank;
                }
	}

        if (dingu_match == nullptr) {
                /* If we get here, there was no dingu match.
                 * Record smallest span where none of the dingus match.
                 */
                *start = start_blank;
                *end = end_blank - 1;

                _VTE_DEBUG_IF(VTE_DEBUG_REGEX) {
                        struct _VteCharAttributes *_sattr, *_eattr;
                        _sattr = &g_array_index(m_match_attributes,
                                                struct _VteCharAttributes,
                                                start_blank);
                        _eattr = &g_array_index(m_match_attributes,
                                                struct _VteCharAttributes,
                                                end_blank - 1);
                        g_printerr("No-match region from %" G_GSIZE_FORMAT "(%ld,%ld) to %" G_GSIZE_FORMAT "(%ld,%ld)\n",
                                   start_blank, _sattr->column, _sattr->row,
                                   end_blank - 1, _eattr->column, _eattr->row);
                }
        }

        pcre2_match_data_free_8(match_data);
        pcre2_match_context_free_8(match_context);

	return dingu_match;
}

#endif /* WITH_PCRE2 */

bool
VteTerminalPrivate::match_check_gregex(GRegex *regex,
                   GRegexMatchFlags match_flags,
                   gsize sattr,
                   gsize eattr,
                   gsize offset,
                   char **result_ptr,
                   gsize *start,
                   gsize *end,
                   gsize *sblank_ptr,
                   gsize *eblank_ptr)
{
        GMatchInfo *match_info;
        const char *line;
        gsize line_length;
        gint sblank = G_MININT, eblank = G_MAXINT;

        line = m_match_contents;
        line_length = eattr;

        /* We'll only match the first item in the buffer which
         * matches, so we'll have to skip each match until we
         * stop getting matches. */
        if (!g_regex_match_full(regex,
                                line, line_length, /* subject, length */
                                sattr, /* start position */
                                match_flags,
                                &match_info,
                                NULL)) {
                g_match_info_free(match_info);
                return FALSE;
        }

        while (g_match_info_matches(match_info)) {
                gint ko = offset;
                gint rm_so, rm_eo;

                if (g_match_info_fetch_pos (match_info, 0, &rm_so, &rm_eo)) {
                        /* The offsets should be "sane". */
                        g_assert(rm_so < (int)eattr);
                        g_assert(rm_eo <= (int)eattr);
                        _VTE_DEBUG_IF(VTE_DEBUG_REGEX) {
                                gchar *result;
                                struct _VteCharAttributes *_sattr, *_eattr;
                                result = g_strndup(line + rm_so, rm_eo - rm_so);
                                _sattr = &g_array_index(m_match_attributes,
							struct _VteCharAttributes,
							rm_so);
                                _eattr = &g_array_index(m_match_attributes,
							struct _VteCharAttributes,
							rm_eo - 1);
                                g_printerr("Match `%s' from %d(%ld,%ld) to %d(%ld,%ld) (%" G_GSIZE_FORMAT ").\n",
                                           result,
                                           rm_so,
                                           _sattr->column,
                                           _sattr->row,
                                           rm_eo - 1,
                                           _eattr->column,
                                           _eattr->row,
                                           offset);
                                g_free(result);

                        }
                        /* If the pointer is in this substring,
                         * then we're done. */
                        if (ko >= rm_so && ko < rm_eo) {
                                *start = rm_so;
                                *end = rm_eo - 1;
                                *result_ptr = g_match_info_fetch(match_info, 0);

                                g_match_info_free(match_info);
                                return true;
                        }

                        if (ko >= rm_eo && rm_eo > sblank) {
                                sblank = rm_eo;
                        }
                        if (ko < rm_so && rm_so < eblank) {
                                eblank = rm_so;
                        }
                }

                g_match_info_next(match_info, NULL);
        }

        g_match_info_free(match_info);

        *sblank_ptr = sblank;
        *eblank_ptr = eblank;
        return false;
}

char *
VteTerminalPrivate::match_check_internal_gregex(vte::grid::column_t column,
                                                vte::grid::row_t row,
                                                int *tag,
                                                gsize *start,
                                                gsize *end)
{
        guint i;
	struct vte_match_regex *regex = nullptr;
	gsize sattr, eattr, offset, start_blank, end_blank;
        char *dingu_match = nullptr;

	_vte_debug_print(VTE_DEBUG_REGEX,
                         "Checking for gregex match at (%ld,%ld).\n", row, column);

        if (!match_rowcol_to_offset(column, row,
                                    &offset, &sattr, &eattr))
                return nullptr;

	start_blank = sattr;
	end_blank = eattr;

	/* Now iterate over each regex we need to match against. */
	for (i = 0; i < m_match_regexes->len; i++) {
                gsize sblank = 0, eblank = G_MAXSIZE;

		regex = &g_array_index(m_match_regexes,
				       struct vte_match_regex,
				       i);
		/* Skip holes. */
		if (regex->tag < 0) {
			continue;
		}

                g_assert_cmpint(regex->regex.mode, ==, VTE_REGEX_GREGEX);

                if (match_check_gregex(regex->regex.gregex.regex,
                                       regex->regex.gregex.match_flags,
                                       sattr, eattr, offset,
                                       &dingu_match,
                                       start, end,
                                       &sblank, &eblank)) {
                        _vte_debug_print(VTE_DEBUG_REGEX, "Matched dingu with tag %d\n", regex->tag);
                        set_cursor_from_regex_match(regex);
                        *tag = regex->tag;
                        break;
                }

                if (sblank > start_blank) {
                        start_blank = sblank;
                }
                if (eblank < end_blank) {
                        end_blank = eblank;
                }
	}

        if (dingu_match == nullptr) {
                /* If we get here, there was no dingu match.
                 * Record smallest span where none of the dingus match.
                 */
                *start = start_blank;
                *end = end_blank - 1;

                _VTE_DEBUG_IF(VTE_DEBUG_REGEX) {
                        struct _VteCharAttributes *_sattr, *_eattr;
                        _sattr = &g_array_index(m_match_attributes,
                                                struct _VteCharAttributes,
                                                start_blank);
                        _eattr = &g_array_index(m_match_attributes,
                                                struct _VteCharAttributes,
                                                end_blank - 1);
                        g_printerr("No-match region from %" G_GSIZE_FORMAT "(%ld,%ld) to %" G_GSIZE_FORMAT "(%ld,%ld)\n",
                                   start_blank, _sattr->column, _sattr->row,
                                   end_blank - 1, _eattr->column, _eattr->row);
                }
        }

	return dingu_match;
}

/*
 * vte_terminal_match_check_internal:
 * @terminal:
 * @column:
 * @row:
 * @tag: (out):
 * @start: (out):
 * @end: (out):
 *
 * Checks pvt->match_contents for dingu matches, and returns the tag, start, and
 * end of the match in @tag, @start, @end. If no match occurs, @tag will be set to
 * -1, and if they are nonzero, @start and @end mark the smallest span in the @row
 * in which none of the dingus match.
 *
 * Returns: (transfer full): the matched string, or %NULL
 */
char *
VteTerminalPrivate::match_check_internal(vte::grid::column_t column,
                                         vte::grid::row_t row,
                                         int *tag,
                                         gsize *start,
                                         gsize *end)
{
	if (m_match_contents == nullptr) {
		match_contents_refresh();
	}

        g_assert(tag != NULL);
        g_assert(start != NULL);
        g_assert(end != NULL);

        *tag = -1;
        *start = 0;
        *end = 0;

#ifdef WITH_PCRE2
        if (G_LIKELY(m_match_regex_mode == VTE_REGEX_PCRE2))
                return match_check_internal_pcre(column, row, tag, start, end);
#endif
        if (m_match_regex_mode == VTE_REGEX_GREGEX)
                return match_check_internal_gregex(column, row, tag, start, end);

        return nullptr;
}

static gboolean
rowcol_inside_match (VteTerminal *terminal, glong row, glong col)
{
	if (terminal->pvt->match_start.row == terminal->pvt->match_end.row) {
		return row == terminal->pvt->match_start.row &&
			col >= terminal->pvt->match_start.col &&
			col <= terminal->pvt->match_end.col;
	} else {
		if (row < terminal->pvt->match_start.row ||
				row > terminal->pvt->match_end.row) {
			return FALSE;
		}
		if (row == terminal->pvt->match_start.row) {
			return col >= terminal->pvt->match_start.col;
		}
		if (row == terminal->pvt->match_end.row) {
			return col <= terminal->pvt->match_end.col;
		}
		return TRUE;
	}
}

char *
VteTerminalPrivate::regex_match_check(vte::grid::column_t column,
                                      vte::grid::row_t row,
                                      int *tag)
{
	char *ret;

	long delta = m_screen->scroll_delta;
	_vte_debug_print(VTE_DEBUG_EVENTS | VTE_DEBUG_REGEX,
			"Checking for match at (%ld,%ld).\n",
			row, column);
	if (rowcol_inside_match(m_terminal, row + delta, column)) {
		if (tag) {
			*tag = m_match_tag;
		}
		ret = m_match != NULL ?
			g_strdup (m_match) :
			NULL;
	} else {
                gsize start, end;
                int ltag;

		ret = match_check_internal(
                                                        column, row + delta,
                                                        tag ? tag : &ltag,
                                                        &start, &end);
	}
	_VTE_DEBUG_IF(VTE_DEBUG_EVENTS | VTE_DEBUG_REGEX) {
		if (ret != NULL) g_printerr("Matched `%s'.\n", ret);
	}
	return ret;
}

static gboolean
rowcol_from_event(VteTerminal *terminal,
                  GdkEvent *event,
                  long *column,
                  long *row)
{
        double x, y;

        if (event == NULL)
                return FALSE;
        if (((GdkEventAny*)event)->window != gtk_widget_get_window(&terminal->widget))
                return FALSE;
        if (!gdk_event_get_coords(event, &x, &y))
                return FALSE;

        x -= terminal->pvt->padding.left;
        y -= terminal->pvt->padding.top;
        if (x < 0 || x >= terminal->pvt->column_count * terminal->pvt->char_width ||
            y < 0 || y >= _vte_terminal_usable_height_px (terminal))
                return FALSE;
        *column = x / terminal->pvt->char_width;
        *row = _vte_terminal_pixel_to_row(terminal, y);

        return TRUE;
}

/**
 * vte_terminal_match_check_event:
 * @terminal: a #VteTerminal
 * @event: a #GdkEvent
 * @tag: (out) (allow-none): a location to store the tag, or %NULL
 *
 * Checks if the text in and around the position of the event matches any of the
 * regular expressions previously set using vte_terminal_match_add().  If a
 * match exists, the text string is returned and if @tag is not %NULL, the number
 * associated with the matched regular expression will be stored in @tag.
 *
 * If more than one regular expression has been set with
 * vte_terminal_match_add(), then expressions are checked in the order in
 * which they were added.
 *
 * Returns: (transfer full): a newly allocated string which matches one of the previously
 *   set regular expressions
 */
char *
vte_terminal_match_check_event(VteTerminal *terminal,
                               GdkEvent *event,
                               int *tag)
{
        long col, row;

        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);

        if (!rowcol_from_event(terminal, event, &col, &row))
                return FALSE;

        /* FIXME Shouldn't rely on a deprecated, not sub-row aware method. */
        return vte_terminal_match_check(terminal, col, row - (long) terminal->pvt->screen->scroll_delta, tag);
}

/**
 * vte_terminal_event_check_regex_simple:
 * @terminal: a #VteTerminal
 * @event: a #GdkEvent
 * @regexes: (array length=n_regexes): an array of #VteRegex
 * @n_regexes: number of items in @regexes
 * @match_flags: PCRE2 match flags, or 0
 * @matches: (out caller-allocates) (array length=n_regexes): a location to store the matches
 *
 * Checks each regex in @regexes if the text in and around the position of
 * the event matches the regular expressions.  If a match exists, the matched
 * text is stored in @matches at the position of the regex in @regexes; otherwise
 * %NULL is stored there.
 *
 * Returns: %TRUE iff any of the regexes produced a match
 *
 * Since: 0.44
 */
gboolean
vte_terminal_event_check_regex_simple(VteTerminal *terminal,
                                      GdkEvent *event,
                                      VteRegex **regexes,
                                      gsize n_regexes,
                                      guint32 match_flags,
                                      char **matches)
{
#ifdef WITH_PCRE2
        VteTerminalPrivate *pvt = terminal->pvt;
	gsize offset, sattr, eattr;
        pcre2_match_data_8 *match_data;
        pcre2_match_context_8 *match_context;
        gboolean any_matches = FALSE;
        long col, row;
        guint i;

        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        g_return_val_if_fail(event != NULL, FALSE);
        g_return_val_if_fail(regexes != NULL || n_regexes == 0, FALSE);
        g_return_val_if_fail(matches != NULL, FALSE);

        if (!rowcol_from_event(terminal, event, &col, &row))
                return FALSE;

	if (pvt->match_contents == NULL) {
		pvt->match_contents_refresh();
	}

        if (!pvt->match_rowcol_to_offset(col, row,
                                    &offset, &sattr, &eattr))
                return FALSE;

        match_context = terminal->pvt->create_match_context();
        match_data = pcre2_match_data_create_8(256 /* should be plenty */, NULL /* general context */);

        for (i = 0; i < n_regexes; i++) {
                gsize start, end, sblank, eblank;
                char *match;

                g_return_val_if_fail(regexes[i] != NULL, FALSE);

                if (pvt->match_check_pcre(
                                     match_data, match_context,
                                     regexes[i], match_flags,
                                     sattr, eattr, offset,
                                     &match,
                                     &start, &end,
                                     &sblank, &eblank)) {
                        _vte_debug_print(VTE_DEBUG_REGEX, "Matched regex with text: %s\n", match);
                        matches[i] = match;
                        any_matches = TRUE;
                } else
                        matches[i] = NULL;
        }

        pcre2_match_data_free_8(match_data);
        pcre2_match_context_free_8(match_context);

        return any_matches;
#else
        return FALSE;
#endif
}

/**
 * vte_terminal_event_check_gregex_simple:
 * @terminal: a #VteTerminal
 * @event: a #GdkEvent
 * @regexes: (array length=n_regexes): an array of #GRegex
 * @n_regexes: number of items in @regexes
 * @match_flags: the #GRegexMatchFlags to use when matching the regexes
 * @matches: (out caller-allocates) (array length=n_regexes): a location to store the matches
 *
 * Checks each regex in @regexes if the text in and around the position of
 * the event matches the regular expressions.  If a match exists, the matched
 * text is stored in @matches at the position of the regex in @regexes; otherwise
 * %NULL is stored there.
 *
 * Returns: %TRUE iff any of the regexes produced a match
 *
 * Since: 0.44
 * Deprecated: 0.44: Use vte_terminal_event_check_regex_simple() instead.
 */
gboolean
vte_terminal_event_check_gregex_simple(VteTerminal *terminal,
                                       GdkEvent *event,
                                       GRegex **regexes,
                                       gsize n_regexes,
                                       GRegexMatchFlags match_flags,
                                       char **matches)
{
        VteTerminalPrivate *pvt = terminal->pvt;
	gsize offset, sattr, eattr;
        gboolean any_matches = FALSE;
        long col, row;
        guint i;

        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        g_return_val_if_fail(event != NULL, FALSE);
        g_return_val_if_fail(regexes != NULL || n_regexes == 0, FALSE);
        g_return_val_if_fail(matches != NULL, FALSE);

        if (!rowcol_from_event(terminal, event, &col, &row))
                return FALSE;

	if (pvt->match_contents == NULL) {
		pvt->match_contents_refresh();
	}

        if (!pvt->match_rowcol_to_offset(col, row,
                                    &offset, &sattr, &eattr))
                return FALSE;

        for (i = 0; i < n_regexes; i++) {
                gsize start, end, sblank, eblank;
                char *match;

                g_return_val_if_fail(regexes[i] != NULL, FALSE);

                if (pvt->match_check_gregex(
                                       regexes[i], match_flags,
                                       sattr, eattr, offset,
                                       &match,
                                       &start, &end,
                                       &sblank, &eblank)) {
                        _vte_debug_print(VTE_DEBUG_REGEX, "Matched gregex with text: %s\n", match);
                        matches[i] = match;
                        any_matches = TRUE;
                } else
                        matches[i] = NULL;
        }

        return any_matches;
}

/* Emit an adjustment changed signal on our adjustment object. */
static void
vte_terminal_emit_adjustment_changed(VteTerminal *terminal)
{
	if (terminal->pvt->adjustment_changed_pending) {
		VteScreen *screen = terminal->pvt->screen;
		gboolean changed = FALSE;
		glong v;
		gdouble current;

		g_object_freeze_notify (G_OBJECT (terminal->pvt->vadjustment));

		v = _vte_ring_delta (screen->row_data);
		current = gtk_adjustment_get_lower(terminal->pvt->vadjustment);
		if (current != v) {
			_vte_debug_print(VTE_DEBUG_ADJ,
					"Changing lower bound from %.0f to %ld\n",
					 current, v);
			gtk_adjustment_set_lower(terminal->pvt->vadjustment, v);
			changed = TRUE;
		}

		v = terminal->pvt->screen->insert_delta + terminal->pvt->row_count;
		current = gtk_adjustment_get_upper(terminal->pvt->vadjustment);
		if (current != v) {
			_vte_debug_print(VTE_DEBUG_ADJ,
					"Changing upper bound from %.0f to %ld\n",
					 current, v);
			gtk_adjustment_set_upper(terminal->pvt->vadjustment, v);
			changed = TRUE;
		}

		g_object_thaw_notify (G_OBJECT (terminal->pvt->vadjustment));

		if (changed)
			_vte_debug_print(VTE_DEBUG_SIGNALS,
					"Emitting adjustment_changed.\n");
		terminal->pvt->adjustment_changed_pending = FALSE;
	}
	if (terminal->pvt->adjustment_value_changed_pending) {
		double v, delta;
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting adjustment_value_changed.\n");
		terminal->pvt->adjustment_value_changed_pending = FALSE;
		v = gtk_adjustment_get_value(terminal->pvt->vadjustment);
		if (v != terminal->pvt->screen->scroll_delta) {
			/* this little dance is so that the scroll_delta is
			 * updated immediately, but we still handled scrolling
			 * via the adjustment - e.g. user interaction with the
			 * scrollbar
			 */
			delta = terminal->pvt->screen->scroll_delta;
			terminal->pvt->screen->scroll_delta = v;
			gtk_adjustment_set_value(terminal->pvt->vadjustment, delta);
		}
	}
}

/* Queue an adjustment-changed signal to be delivered when convenient. */
static inline void
vte_terminal_queue_adjustment_changed(VteTerminal *terminal)
{
	terminal->pvt->adjustment_changed_pending = TRUE;
	add_update_timeout (terminal);
}

static void
vte_terminal_queue_adjustment_value_changed(VteTerminal *terminal, double v)
{
	if (v != terminal->pvt->screen->scroll_delta) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Adjustment value changed to %f\n",
                                 v);
		terminal->pvt->screen->scroll_delta = v;
		terminal->pvt->adjustment_value_changed_pending = TRUE;
		add_update_timeout (terminal);
	}
}

static void
vte_terminal_queue_adjustment_value_changed_clamped(VteTerminal *terminal, double v)
{
	gdouble lower, upper;

	lower = gtk_adjustment_get_lower(terminal->pvt->vadjustment);
	upper = gtk_adjustment_get_upper(terminal->pvt->vadjustment);

	v = CLAMP(v, lower, MAX (lower, upper - terminal->pvt->row_count));

	vte_terminal_queue_adjustment_value_changed (terminal, v);
}


void
_vte_terminal_adjust_adjustments(VteTerminal *terminal)
{
	VteScreen *screen;
	long delta;

	g_assert(terminal->pvt->screen != NULL);
	g_assert(terminal->pvt->screen->row_data != NULL);

	vte_terminal_queue_adjustment_changed(terminal);

	/* The lower value should be the first row in the buffer. */
	screen = terminal->pvt->screen;
	delta = _vte_ring_delta(screen->row_data);
	/* Snap the insert delta and the cursor position to be in the visible
	 * area.  Leave the scrolling delta alone because it will be updated
	 * when the adjustment changes. */
	screen->insert_delta = MAX(screen->insert_delta, delta);
        terminal->pvt->cursor.row = MAX(terminal->pvt->cursor.row,
                                        screen->insert_delta);

	if (screen->scroll_delta > screen->insert_delta) {
		vte_terminal_queue_adjustment_value_changed(terminal,
				screen->insert_delta);
	}
}

/* Update the adjustment field of the widget.  This function should be called
 * whenever we add rows to or remove rows from the history or switch screens. */
static void
_vte_terminal_adjust_adjustments_full (VteTerminal *terminal)
{
	gboolean changed = FALSE;
	gdouble v;

	g_assert(terminal->pvt->screen != NULL);
	g_assert(terminal->pvt->screen->row_data != NULL);

	_vte_terminal_adjust_adjustments(terminal);

        g_object_freeze_notify(G_OBJECT(terminal->pvt->vadjustment));

	/* The step increment should always be one. */
	v = gtk_adjustment_get_step_increment(terminal->pvt->vadjustment);
	if (v != 1) {
		_vte_debug_print(VTE_DEBUG_ADJ,
				"Changing step increment from %.0lf to 1\n", v);
		gtk_adjustment_set_step_increment(terminal->pvt->vadjustment, 1);
		changed = TRUE;
	}

	/* Set the number of rows the user sees to the number of rows the
	 * user sees. */
	v = gtk_adjustment_get_page_size(terminal->pvt->vadjustment);
	if (v != terminal->pvt->row_count) {
		_vte_debug_print(VTE_DEBUG_ADJ,
				"Changing page size from %.0f to %ld\n",
				 v, terminal->pvt->row_count);
		gtk_adjustment_set_page_size(terminal->pvt->vadjustment,
					     terminal->pvt->row_count);
		changed = TRUE;
	}

	/* Clicking in the empty area should scroll one screen, so set the
	 * page size to the number of visible rows. */
	v = gtk_adjustment_get_page_increment(terminal->pvt->vadjustment);
	if (v != terminal->pvt->row_count) {
		_vte_debug_print(VTE_DEBUG_ADJ,
				"Changing page increment from "
				"%.0f to %ld\n",
				v, terminal->pvt->row_count);
		gtk_adjustment_set_page_increment(terminal->pvt->vadjustment,
						  terminal->pvt->row_count);
		changed = TRUE;
	}

	g_object_thaw_notify(G_OBJECT(terminal->pvt->vadjustment));

	if (changed)
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting adjustment_changed.\n");
}

/* Scroll a fixed number of lines up or down in the current screen. */
static void
vte_terminal_scroll_lines(VteTerminal *terminal, gint lines)
{
	double destination;
	_vte_debug_print(VTE_DEBUG_ADJ, "Scrolling %d lines.\n", lines);
	/* Calculate the ideal position where we want to be before clamping. */
	destination = terminal->pvt->screen->scroll_delta;
        /* Snap to whole cell offset. */
        if (lines > 0)
                destination = floor(destination);
        else if (lines < 0)
                destination = ceil(destination);
	destination += lines;
	/* Tell the scrollbar to adjust itself. */
	vte_terminal_queue_adjustment_value_changed_clamped (terminal, destination);
}

/* Scroll a fixed number of pages up or down, in the current screen. */
static void
vte_terminal_scroll_pages(VteTerminal *terminal, gint pages)
{
	vte_terminal_scroll_lines(terminal, pages * terminal->pvt->row_count);
}

/* Scroll so that the scroll delta is the minimum value. */
static void
vte_terminal_maybe_scroll_to_top(VteTerminal *terminal)
{
	vte_terminal_queue_adjustment_value_changed (terminal,
			_vte_ring_delta(terminal->pvt->screen->row_data));
}

static void
vte_terminal_maybe_scroll_to_bottom(VteTerminal *terminal)
{
	glong delta;
	delta = terminal->pvt->screen->insert_delta;
	vte_terminal_queue_adjustment_value_changed (terminal, delta);
	_vte_debug_print(VTE_DEBUG_ADJ,
			"Snapping to bottom of screen\n");
}

static void
_vte_terminal_setup_utf8 (VteTerminal *terminal)
{
        VteTerminalPrivate *pvt = terminal->pvt;
        GError *error = NULL;

        if (!vte_pty_set_utf8(pvt->pty,
                              strcmp(terminal->pvt->encoding, "UTF-8") == 0,
                              &error)) {
                g_warning ("Failed to set UTF8 mode: %s\n", error->message);
                g_error_free (error);
        }
}

/**
 * vte_terminal_set_encoding:
 * @terminal: a #VteTerminal
 * @codeset: (allow-none): a valid #GIConv target, or %NULL to use UTF-8
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Changes the encoding the terminal will expect data from the child to
 * be encoded with.  For certain terminal types, applications executing in the
 * terminal can change the encoding. If @codeset is %NULL, it uses "UTF-8".
 *
 * Returns: %TRUE if the encoding could be changed to the specified one,
 *  or %FALSE with @error set to %G_CONVERT_ERROR_NO_CONVERSION.
 */
gboolean
vte_terminal_set_encoding(VteTerminal *terminal,
                          const char *codeset,
                          GError **error)
{
        VteTerminalPrivate *pvt;
        GObject *object;
	const char *old_codeset;
	VteConv conv;
	char *obuf1, *obuf2;
	gsize bytes_written;

	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

        object = G_OBJECT(terminal);
        pvt = terminal->pvt;

	old_codeset = pvt->encoding;
	if (codeset == NULL) {
                codeset = "UTF-8";
	}
	if ((old_codeset != NULL) && g_str_equal(codeset, old_codeset)) {
		/* Nothing to do! */
		return TRUE;
	}

	/* Open new conversions. */
	conv = _vte_conv_open(codeset, "UTF-8");
	if (conv == VTE_INVALID_CONV) {
		g_set_error(error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
                            _("Unable to convert characters from %s to %s."),
                            "UTF-8", codeset);
                return FALSE;
        }

        g_object_freeze_notify(object);

	if (terminal->pvt->outgoing_conv != VTE_INVALID_CONV) {
		_vte_conv_close(terminal->pvt->outgoing_conv);
	}
	terminal->pvt->outgoing_conv = conv;

	/* Set the terminal's encoding to the new value. */
	terminal->pvt->encoding = g_intern_string(codeset);

	/* Convert any buffered output bytes. */
	if ((_vte_byte_array_length(terminal->pvt->outgoing) > 0) &&
	    (old_codeset != NULL)) {
		/* Convert back to UTF-8. */
		obuf1 = g_convert((gchar *)terminal->pvt->outgoing->data,
				  _vte_byte_array_length(terminal->pvt->outgoing),
				  "UTF-8",
				  old_codeset,
				  NULL,
				  &bytes_written,
				  NULL);
		if (obuf1 != NULL) {
			/* Convert to the new encoding. */
			obuf2 = g_convert(obuf1,
					  bytes_written,
					  codeset,
					  "UTF-8",
					  NULL,
					  &bytes_written,
					  NULL);
			if (obuf2 != NULL) {
				_vte_byte_array_clear(terminal->pvt->outgoing);
				_vte_byte_array_append(terminal->pvt->outgoing,
						   obuf2, bytes_written);
				g_free(obuf2);
			}
			g_free(obuf1);
		}
	}

	/* Set the encoding for incoming text. */
	_vte_iso2022_state_set_codeset(terminal->pvt->iso2022,
				       terminal->pvt->encoding);

	_vte_debug_print(VTE_DEBUG_IO,
			"Set terminal encoding to `%s'.\n",
			terminal->pvt->encoding);
	vte_terminal_emit_encoding_changed(terminal);

        g_object_thaw_notify(object);

        return TRUE;
}

/**
 * vte_terminal_get_encoding:
 * @terminal: a #VteTerminal
 *
 * Determines the name of the encoding in which the terminal expects data to be
 * encoded.
 *
 * Returns: (transfer none): the current encoding for the terminal
 */
const char *
vte_terminal_get_encoding(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return terminal->pvt->encoding;
}

/**
 * vte_terminal_set_cjk_ambiguous_width:
 * @terminal: a #VteTerminal
 * @width: either 1 (narrow) or 2 (wide)
 *
 * This setting controls whether ambiguous-width characters are narrow or wide
 * when using the UTF-8 encoding (vte_terminal_set_encoding()). In all other encodings,
 * the width of ambiguous-width characters is fixed.
 */
void
vte_terminal_set_cjk_ambiguous_width(VteTerminal *terminal, int width)
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(width == 1 || width == 2);

        terminal->pvt->utf8_ambiguous_width = width;
}

/**
 * vte_terminal_get_cjk_ambiguous_width:
 * @terminal: a #VteTerminal
 *
 * Returns whether ambiguous-width characters are narrow or wide when using
 * the UTF-8 encoding (vte_terminal_set_encoding()).
 *
 * Returns: 1 if ambiguous-width characters are narrow, or 2 if they are wide
 */
int
vte_terminal_get_cjk_ambiguous_width(VteTerminal *terminal)
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), 1);
        return terminal->pvt->utf8_ambiguous_width;
}

static inline VteRowData *
vte_terminal_insert_rows (VteTerminal *terminal, guint cnt)
{
	VteRowData *row;
	do {
		row = _vte_terminal_ring_append (terminal, FALSE);
	} while(--cnt);
	return row;
}


/* Make sure we have enough rows and columns to hold data at the current
 * cursor position. */
VteRowData *
_vte_terminal_ensure_row (VteTerminal *terminal)
{
	VteRowData *row;
	VteScreen *screen;
	gint delta;
	glong v;

	/* Must make sure we're in a sane area. */
	screen = terminal->pvt->screen;
        v = terminal->pvt->cursor.row;

	/* Figure out how many rows we need to add. */
	delta = v - _vte_ring_next(screen->row_data) + 1;
	if (delta > 0) {
		row = vte_terminal_insert_rows (terminal, delta);
		_vte_terminal_adjust_adjustments(terminal);
	} else {
		/* Find the row the cursor is in. */
		row = _vte_ring_index_writable (screen->row_data, v);
	}
	g_assert(row != NULL);

	return row;
}

static VteRowData *
vte_terminal_ensure_cursor(VteTerminal *terminal)
{
	VteRowData *row;

	row = _vte_terminal_ensure_row (terminal);
        _vte_row_data_fill (row, &basic_cell.cell, terminal->pvt->cursor.col);

	return row;
}

/* Update the insert delta so that the screen which includes it also
 * includes the end of the buffer. */
void
_vte_terminal_update_insert_delta(VteTerminal *terminal)
{
	long delta, rows;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	/* The total number of lines.  Add one to the cursor offset
	 * because it's zero-based. */
	rows = _vte_ring_next (screen->row_data);
        delta = terminal->pvt->cursor.row - rows + 1;
	if (G_UNLIKELY (delta > 0)) {
		vte_terminal_insert_rows (terminal, delta);
		rows = _vte_ring_next (screen->row_data);
	}

	/* Make sure that the bottom row is visible, and that it's in
	 * the buffer (even if it's empty).  This usually causes the
	 * top row to become a history-only row. */
	delta = screen->insert_delta;
	delta = MIN(delta, rows - terminal->pvt->row_count);
	delta = MAX(delta,
                    terminal->pvt->cursor.row - (terminal->pvt->row_count - 1));
	delta = MAX(delta, _vte_ring_delta(screen->row_data));

	/* Adjust the insert delta and scroll if needed. */
	if (delta != screen->insert_delta) {
		screen->insert_delta = delta;
		_vte_terminal_adjust_adjustments(terminal);
	}
}

/* Show or hide the pointer. */
void
VteTerminalPrivate::set_pointer_visible(bool visible)
{
	m_mouse_cursor_visible = visible;

        if (!gtk_widget_get_realized(m_widget))
                return;

	GdkWindow *window = gtk_widget_get_window(m_widget);

	if (visible || !m_mouse_autohide) {
		if (m_mouse_tracking_mode) {
			_vte_debug_print(VTE_DEBUG_CURSOR,
					"Setting mousing cursor.\n");
			gdk_window_set_cursor(window, m_mouse_mousing_cursor);
		} else
		if ( (guint)m_match_tag < m_match_regexes->len) {
                        struct vte_match_regex *regex =
                                &g_array_index(m_match_regexes,
					       struct vte_match_regex,
					       m_match_tag);
                        set_cursor_from_regex_match(regex);
		} else {
			_vte_debug_print(VTE_DEBUG_CURSOR,
					"Setting default mouse cursor.\n");
			gdk_window_set_cursor(window, m_mouse_default_cursor);
		}
	} else {
		_vte_debug_print(VTE_DEBUG_CURSOR,
				"Setting to invisible cursor.\n");
		gdk_window_set_cursor (window, m_mouse_inviso_cursor);
	}
}

/**
 * vte_terminal_new:
 *
 * Creates a new terminal widget.
 *
 * Returns: (transfer none) (type Vte.Terminal): a new #VteTerminal object
 */
GtkWidget *
vte_terminal_new(void)
{
	return (GtkWidget *)g_object_new(VTE_TYPE_TERMINAL, NULL);
}

/*
 * Get the actually used color from the palette.
 * The return value can be NULL only if entry is one of VTE_CURSOR_BG,
 * VTE_HIGHLIGHT_BG or VTE_HIGHLIGHT_FG.
 */
PangoColor *
_vte_terminal_get_color(const VteTerminal *terminal, int entry)
{
	VtePaletteColor *palette_color = &terminal->pvt->palette[entry];
	guint source;
	for (source = 0; source < G_N_ELEMENTS(palette_color->sources); source++)
		if (palette_color->sources[source].is_set)
			return &palette_color->sources[source].color;
	return NULL;
}

/* Set up a palette entry with a more-or-less match for the requested color. */
void
_vte_terminal_set_color_internal(VteTerminal *terminal,
                                 int entry,
                                 int source,
                                 const PangoColor *proposed)
{
	VtePaletteColor *palette_color = &terminal->pvt->palette[entry];

	/* Save the requested color. */
	if (proposed != NULL) {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Set %s color[%d] to (%04x,%04x,%04x).\n",
				source == VTE_COLOR_SOURCE_ESCAPE ? "escape" : "API",
				entry, proposed->red, proposed->green, proposed->blue);

		if (palette_color->sources[source].is_set &&
		    palette_color->sources[source].color.red == proposed->red &&
		    palette_color->sources[source].color.green == proposed->green &&
		    palette_color->sources[source].color.blue == proposed->blue) {
			return;
		}
		palette_color->sources[source].is_set = TRUE;
		palette_color->sources[source].color.red = proposed->red;
		palette_color->sources[source].color.green = proposed->green;
		palette_color->sources[source].color.blue = proposed->blue;
	} else {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Reset %s color[%d].\n",
				source == VTE_COLOR_SOURCE_ESCAPE ? "escape" : "API",
				entry);

		if (!palette_color->sources[source].is_set) {
			return;
		}
		palette_color->sources[source].is_set = FALSE;
	}

	/* If we're not realized yet, there's nothing else to do. */
	if (! gtk_widget_get_realized (&terminal->widget)) {
		return;
	}

	/* If we're setting the background color, set the background color
	 * on the widget as well. */
	if (entry == VTE_DEFAULT_BG) {
		vte_terminal_background_update(terminal);
	}

	/* and redraw */
	if (entry == VTE_CURSOR_BG)
		_vte_invalidate_cursor_once(terminal, FALSE);
	else
		_vte_invalidate_all (terminal);
}

static void
vte_terminal_generate_bold(const PangoColor *foreground,
			   const PangoColor *background,
			   double factor,
                           PangoColor *bold /* (out) (callee allocates) */)
{
	double fy, fcb, fcr, by, bcb, bcr, r, g, b;
	g_assert(foreground != NULL);
	g_assert(background != NULL);
	g_assert(bold != NULL);
	fy =   0.2990 * foreground->red +
	       0.5870 * foreground->green +
	       0.1140 * foreground->blue;
	fcb = -0.1687 * foreground->red +
	      -0.3313 * foreground->green +
	       0.5000 * foreground->blue;
	fcr =  0.5000 * foreground->red +
	      -0.4187 * foreground->green +
	      -0.0813 * foreground->blue;
	by =   0.2990 * background->red +
	       0.5870 * background->green +
	       0.1140 * background->blue;
	bcb = -0.1687 * background->red +
	      -0.3313 * background->green +
	       0.5000 * background->blue;
	bcr =  0.5000 * background->red +
	      -0.4187 * background->green +
	      -0.0813 * background->blue;
	fy = (factor * fy) + ((1.0 - factor) * by);
	fcb = (factor * fcb) + ((1.0 - factor) * bcb);
	fcr = (factor * fcr) + ((1.0 - factor) * bcr);
	r = fy + 1.402 * fcr;
	g = fy + 0.34414 * fcb - 0.71414 * fcr;
	b = fy + 1.722 * fcb;
	_vte_debug_print(VTE_DEBUG_MISC,
			"Calculated bold (%d, %d, %d) = (%lf,%lf,%lf)",
			foreground->red, foreground->green, foreground->blue,
			r, g, b);
	bold->red = CLAMP(r, 0, 0xffff);
	bold->green = CLAMP(g, 0, 0xffff);
	bold->blue = CLAMP(b, 0, 0xffff);
	_vte_debug_print(VTE_DEBUG_MISC,
			"= (%04x,%04x,%04x).\n",
			bold->red, bold->green, bold->blue);
}

/*
 * _vte_terminal_set_color_bold:
 * @terminal: a #VteTerminal
 * @bold: the new bold color
 *
 * Sets the color used to draw bold text in the default foreground color.
 */
static void
_vte_terminal_set_color_bold(VteTerminal *terminal,
                             const PangoColor *bold)
{
	_vte_debug_print(VTE_DEBUG_MISC,
			"Set bold color to (%04x,%04x,%04x).\n",
			bold->red, bold->green, bold->blue);
	_vte_terminal_set_color_internal(terminal, VTE_BOLD_FG, VTE_COLOR_SOURCE_API, bold);
}

/*
 * _vte_terminal_set_color_foreground:
 * @terminal: a #VteTerminal
 * @foreground: the new foreground color
 *
 * Sets the foreground color used to draw normal text
 */
static void
_vte_terminal_set_color_foreground(VteTerminal *terminal,
                                   const PangoColor *foreground)
{
	_vte_debug_print(VTE_DEBUG_MISC,
			"Set foreground color to (%04x,%04x,%04x).\n",
			foreground->red, foreground->green, foreground->blue);
	_vte_terminal_set_color_internal(terminal, VTE_DEFAULT_FG, VTE_COLOR_SOURCE_API, foreground);
}

/*
 * _vte_terminal_set_color_background:
 * @terminal: a #VteTerminal
 * @background: the new background color
 *
 * Sets the background color for text which does not have a specific background
 * color assigned.  Only has effect when no background image is set.
 */
static void
_vte_terminal_set_color_background(VteTerminal *terminal,
                                   const PangoColor *background)
{
	_vte_debug_print(VTE_DEBUG_MISC,
			"Set background color to (%04x,%04x,%04x).\n",
			background->red, background->green, background->blue);
	_vte_terminal_set_color_internal(terminal, VTE_DEFAULT_BG, VTE_COLOR_SOURCE_API, background);
}

/*
 * _vte_terminal_set_background_alpha:
 * @terminal: a #VteTerminal
 * @alpha: an alpha value from 0.0 to 1.0
 */
static void
_vte_terminal_set_background_alpha(VteTerminal *terminal,
                                   gdouble alpha)
{
        VteTerminalPrivate *pvt = terminal->pvt;

        if (_vte_double_equal(alpha, pvt->background_alpha))
                return;

        _vte_debug_print(VTE_DEBUG_MISC,
                         "Setting background alpha to %.3f\n", alpha);

        pvt->background_alpha = alpha;

        vte_terminal_background_update(terminal);
}

/*
 * _vte_terminal_set_color_cursor:
 * @terminal: a #VteTerminal
 * @cursor_background: (allow-none): the new color to use for the text cursor, or %NULL
 *
 * Sets the background color for text which is under the cursor.  If %NULL, text
 * under the cursor will be drawn with foreground and background colors
 * reversed.
 */
static void
_vte_terminal_set_color_cursor(VteTerminal *terminal,
                               const PangoColor *cursor_background)
{
	if (cursor_background != NULL) {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Set cursor color to (%04x,%04x,%04x).\n",
				cursor_background->red,
				cursor_background->green,
				cursor_background->blue);
	} else {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Reset cursor color.\n");
	}
	_vte_terminal_set_color_internal(terminal, VTE_CURSOR_BG, VTE_COLOR_SOURCE_API, cursor_background);
}

/*
 * _vte_terminal_set_color_highlight:
 * @terminal: a #VteTerminal
 * @highlight_background: (allow-none): the new color to use for highlighted text, or %NULL
 *
 * Sets the background color for text which is highlighted.  If %NULL,
 * it is unset.  If neither highlight background nor highlight foreground are set,
 * highlighted text (which is usually highlighted because it is selected) will
 * be drawn with foreground and background colors reversed.
 */
static void
_vte_terminal_set_color_highlight(VteTerminal *terminal,
                                  const PangoColor *highlight_background)
{
	if (highlight_background != NULL) {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Set highlight background color to (%04x,%04x,%04x).\n",
				highlight_background->red,
				highlight_background->green,
				highlight_background->blue);
	} else {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Reset highlight background color.\n");
	}
	_vte_terminal_set_color_internal(terminal, VTE_HIGHLIGHT_BG, VTE_COLOR_SOURCE_API, highlight_background);
}

/*
 * _vte_terminal_set_color_highlight_foreground:
 * @terminal: a #VteTerminal
 * @highlight_foreground: (allow-none): the new color to use for highlighted text, or %NULL
 *
 * Sets the foreground color for text which is highlighted.  If %NULL,
 * it is unset.  If neither highlight background nor highlight foreground are set,
 * highlighted text (which is usually highlighted because it is selected) will
 * be drawn with foreground and background colors reversed.
 */
static void
_vte_terminal_set_color_highlight_foreground(VteTerminal *terminal,
                                             const PangoColor *highlight_foreground)
{
	if (highlight_foreground != NULL) {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Set highlight foreground color to (%04x,%04x,%04x).\n",
				highlight_foreground->red,
				highlight_foreground->green,
				highlight_foreground->blue);
	} else {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Reset highlight foreground color.\n");
	}
	_vte_terminal_set_color_internal(terminal, VTE_HIGHLIGHT_FG, VTE_COLOR_SOURCE_API, highlight_foreground);
}

/*
 * _vte_terminal_set_colors:
 * @terminal: a #VteTerminal
 * @foreground: (allow-none): the new foreground color, or %NULL
 * @background: (allow-none): the new background color, or %NULL
 * @palette: (array length=palette_size zero-terminated=0) (element-type Gdk.Color): the color palette
 * @palette_size: the number of entries in @palette
 *
 * @palette specifies the new values for the 256 palette colors: 8 standard colors,
 * their 8 bright counterparts, 6x6x6 color cube, and 24 grayscale colors.
 * Omitted entries will default to a hardcoded value.
 *
 * @palette_size must be 0, 8, 16, 232 or 256.
 *
 * If @foreground is %NULL and @palette_size is greater than 0, the new foreground
 * color is taken from @palette[7].  If @background is %NULL and @palette_size is
 * greater than 0, the new background color is taken from @palette[0].
 */
static void
_vte_terminal_set_colors(VteTerminal *terminal,
                         const PangoColor *foreground,
                         const PangoColor *background,
                         const PangoColor *palette,
                         gsize palette_size)
{
	gsize i;
	PangoColor color;
	gboolean unset = FALSE;

	_vte_debug_print(VTE_DEBUG_MISC,
			"Set color palette [%" G_GSIZE_FORMAT " elements].\n",
			palette_size);

	/* Accept NULL as the default foreground and background colors if we
	 * got a palette. */
	if ((foreground == NULL) && (palette_size >= 8)) {
		foreground = &palette[7];
	}
	if ((background == NULL) && (palette_size >= 8)) {
		background = &palette[0];
	}

	memset(&color, 0, sizeof(color));

	/* Initialize each item in the palette if we got any entries to work
	 * with. */
	for (i=0; i < G_N_ELEMENTS(terminal->pvt->palette); i++) {
		unset = FALSE;
		if (i < 16) {
			color.blue = (i & 4) ? 0xc000 : 0;
			color.green = (i & 2) ? 0xc000 : 0;
			color.red = (i & 1) ? 0xc000 : 0;
			if (i > 7) {
				color.blue += 0x3fff;
				color.green += 0x3fff;
				color.red += 0x3fff;
			}
		}
		else if (i < 232) {
			int j = i - 16;
			int r = j / 36, g = (j / 6) % 6, b = j % 6;
			int red =   (r == 0) ? 0 : r * 40 + 55;
			int green = (g == 0) ? 0 : g * 40 + 55;
			int blue =  (b == 0) ? 0 : b * 40 + 55;
			color.red   = red | red << 8  ;
			color.green = green | green << 8;
			color.blue  = blue | blue << 8;
		} else if (i < 256) {
			int shade = 8 + (i - 232) * 10;
			color.red = color.green = color.blue = shade | shade << 8;
		}
		else switch (i) {
			case VTE_DEFAULT_BG:
				if (background != NULL) {
					color = *background;
				} else {
					color.red = 0;
					color.blue = 0;
					color.green = 0;
				}
				break;
			case VTE_DEFAULT_FG:
				if (foreground != NULL) {
					color = *foreground;
				} else {
					color.red = 0xc000;
					color.blue = 0xc000;
					color.green = 0xc000;
				}
				break;
			case VTE_BOLD_FG:
				vte_terminal_generate_bold(_vte_terminal_get_color(terminal, VTE_DEFAULT_FG),
							   _vte_terminal_get_color(terminal, VTE_DEFAULT_BG),
							   1.8,
							   &color);
				break;
			case VTE_HIGHLIGHT_BG:
				unset = TRUE;
				break;
			case VTE_HIGHLIGHT_FG:
				unset = TRUE;
				break;
			case VTE_CURSOR_BG:
				unset = TRUE;
				break;
			}

		/* Override from the supplied palette if there is one. */
		if (i < palette_size) {
			color = palette[i];
		}

		/* Set up the color entry. */
		_vte_terminal_set_color_internal(terminal, i, VTE_COLOR_SOURCE_API, unset ? NULL : &color);
	}
}

static PangoColor *
_pango_color_from_rgba(PangoColor *color,
                       const GdkRGBA *rgba)
{
        if (rgba == NULL)
                return NULL;

        color->red = rgba->red * 65535.;
        color->green = rgba->green * 65535.;
        color->blue = rgba->blue * 65535.;

	return color;
}

/**
 * vte_terminal_set_color_bold:
 * @terminal: a #VteTerminal
 * @bold: (allow-none): the new bold color or %NULL
 *
 * Sets the color used to draw bold text in the default foreground color.
 * If @bold is %NULL then the default color is used.
 */
void
vte_terminal_set_color_bold(VteTerminal *terminal,
                                 const GdkRGBA *bold)
{
	PangoColor color;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));

	if (bold == NULL)
	{
		vte_terminal_generate_bold(_vte_terminal_get_color(terminal, VTE_DEFAULT_FG),
					   _vte_terminal_get_color(terminal, VTE_DEFAULT_BG),
					   1.8,
					   &color);
	}
	else
	{
		_pango_color_from_rgba(&color, bold);
	}

	_vte_terminal_set_color_bold(terminal, &color);
}

/**
 * vte_terminal_set_color_foreground:
 * @terminal: a #VteTerminal
 * @foreground: the new foreground color
 *
 * Sets the foreground color used to draw normal text.
 */
void
vte_terminal_set_color_foreground(VteTerminal *terminal,
				       const GdkRGBA *foreground)
{
	PangoColor color;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(foreground != NULL);

	_vte_terminal_set_color_foreground(terminal,
                                           _pango_color_from_rgba(&color, foreground));
}

/**
 * vte_terminal_set_color_background:
 * @terminal: a #VteTerminal
 * @background: the new background color
 *
 * Sets the background color for text which does not have a specific background
 * color assigned.  Only has effect when no background image is set and when
 * the terminal is not transparent.
 */
void
vte_terminal_set_color_background(VteTerminal *terminal,
				       const GdkRGBA *background)
{
	PangoColor color;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(background != NULL);

	_vte_terminal_set_color_background(terminal,
                                           _pango_color_from_rgba(&color, background));
        _vte_terminal_set_background_alpha(terminal, background->alpha);
}

/**
 * vte_terminal_set_color_cursor:
 * @terminal: a #VteTerminal
 * @cursor_background: (allow-none): the new color to use for the text cursor, or %NULL
 *
 * Sets the background color for text which is under the cursor.  If %NULL, text
 * under the cursor will be drawn with foreground and background colors
 * reversed.
 */
void
vte_terminal_set_color_cursor(VteTerminal *terminal,
				   const GdkRGBA *cursor_background)
{
        PangoColor color;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));

	_vte_terminal_set_color_cursor(terminal,
                                       _pango_color_from_rgba(&color, cursor_background));
}

/**
 * vte_terminal_set_color_highlight:
 * @terminal: a #VteTerminal
 * @highlight_background: (allow-none): the new color to use for highlighted text, or %NULL
 *
 * Sets the background color for text which is highlighted.  If %NULL,
 * it is unset.  If neither highlight background nor highlight foreground are set,
 * highlighted text (which is usually highlighted because it is selected) will
 * be drawn with foreground and background colors reversed.
 */
void
vte_terminal_set_color_highlight(VteTerminal *terminal,
				      const GdkRGBA *highlight_background)
{
	PangoColor color;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));

	_vte_terminal_set_color_highlight(terminal,
                                          _pango_color_from_rgba(&color, highlight_background));
}

/**
 * vte_terminal_set_color_highlight_foreground:
 * @terminal: a #VteTerminal
 * @highlight_foreground: (allow-none): the new color to use for highlighted text, or %NULL
 *
 * Sets the foreground color for text which is highlighted.  If %NULL,
 * it is unset.  If neither highlight background nor highlight foreground are set,
 * highlighted text (which is usually highlighted because it is selected) will
 * be drawn with foreground and background colors reversed.
 */
void
vte_terminal_set_color_highlight_foreground(VteTerminal *terminal,
						 const GdkRGBA *highlight_foreground)
{
	PangoColor color;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));

	_vte_terminal_set_color_highlight_foreground(terminal,
                                                     _pango_color_from_rgba(&color, highlight_foreground));
}

/**
 * vte_terminal_set_colors:
 * @terminal: a #VteTerminal
 * @foreground: (allow-none): the new foreground color, or %NULL
 * @background: (allow-none): the new background color, or %NULL
 * @palette: (array length=palette_size zero-terminated=0) (element-type Gdk.RGBA) (allow-none): the color palette
 * @palette_size: the number of entries in @palette
 *
 * @palette specifies the new values for the 256 palette colors: 8 standard colors,
 * their 8 bright counterparts, 6x6x6 color cube, and 24 grayscale colors.
 * Omitted entries will default to a hardcoded value.
 *
 * @palette_size must be 0, 8, 16, 232 or 256.
 *
 * If @foreground is %NULL and @palette_size is greater than 0, the new foreground
 * color is taken from @palette[7].  If @background is %NULL and @palette_size is
 * greater than 0, the new background color is taken from @palette[0].
 */
void
vte_terminal_set_colors(VteTerminal *terminal,
			     const GdkRGBA *foreground,
			     const GdkRGBA *background,
			     const GdkRGBA *palette,
			     gsize palette_size)
{
	PangoColor fg, bg, *pal;
	gsize i;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail((palette_size == 0) ||
			 (palette_size == 8) ||
			 (palette_size == 16) ||
			 (palette_size == 232) ||
			 (palette_size == 256));

	pal = g_new (PangoColor, palette_size);
	for (i = 0; i < palette_size; ++i)
                _pango_color_from_rgba(&pal[i], &palette[i]);

	_vte_terminal_set_colors(terminal,
                                 _pango_color_from_rgba(&fg, foreground),
                                 _pango_color_from_rgba(&bg, background),
                                 pal, palette_size);

        _vte_terminal_set_background_alpha(terminal, background ? background->alpha : 1.0);

	g_free (pal);
}

/**
 * vte_terminal_set_default_colors:
 * @terminal: a #VteTerminal
 *
 * Reset the terminal palette to reasonable compiled-in default color.
 */
void
vte_terminal_set_default_colors(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	_vte_terminal_set_colors(terminal, NULL, NULL, NULL, 0);
}

/*
 * _vte_terminal_cleanup_fragments:
 * @terminal: a #VteTerminal
 * @start: the starting column, inclusive
 * @end: the end column, exclusive
 *
 * Needs to be called before modifying the contents in the cursor's row,
 * between the two given columns.  Cleans up TAB and CJK fragments to the
 * left of @start and to the right of @end.  If a CJK is split in half,
 * the remaining half is replaced by a space.  If a TAB at @start is split,
 * it is replaced by spaces.  If a TAB at @end is split, it is replaced by
 * a shorter TAB.  @start and @end can be equal if characters will be
 * inserted at the location rather than overwritten.
 *
 * The area between @start and @end is not cleaned up, hence the whole row
 * can be left in an inconsistent state.  It is expected that the caller
 * will fill up that range afterwards, resulting in a consistent row again.
 *
 * Invalidates the cells that visually change outside of the range,
 * because the caller can't reasonably be expected to take care of this.
 */
void
_vte_terminal_cleanup_fragments(VteTerminal *terminal,
                                long start, long end)
{
        VteRowData *row = _vte_terminal_ensure_row (terminal);
        const VteCell *cell_start;
        VteCell *cell_end, *cell_col;
        gboolean cell_start_is_fragment;
        long col;

        g_assert(end >= start);

        /* Remember whether the cell at start is a fragment.  We'll need to know it when
         * handling the left hand side, but handling the right hand side first might
         * overwrite it if start == end (inserting to the middle of a character). */
        cell_start = _vte_row_data_get (row, start);
        cell_start_is_fragment = cell_start != NULL && cell_start->attr.fragment;

        /* On the right hand side, try to replace a TAB by a shorter TAB if we can.
         * This requires that the TAB on the left (which might be the same TAB) is
         * not yet converted to spaces, so start on the right hand side. */
        cell_end = _vte_row_data_get_writable (row, end);
        if (G_UNLIKELY (cell_end != NULL && cell_end->attr.fragment)) {
                col = end;
                do {
                        col--;
                        g_assert(col >= 0);  /* The first cell can't be a fragment. */
                        cell_col = _vte_row_data_get_writable (row, col);
                } while (cell_col->attr.fragment);
                if (cell_col->c == '\t') {
                        _vte_debug_print(VTE_DEBUG_MISC,
                                         "Replacing right part of TAB with a shorter one at %ld (%ld cells) => %ld (%ld cells)\n",
                                         col, (long) cell_col->attr.columns, end, (long) cell_col->attr.columns - (end - col));
                        cell_end->c = '\t';
                        cell_end->attr.fragment = 0;
                        g_assert(cell_col->attr.columns > end - col);
                        cell_end->attr.columns = cell_col->attr.columns - (end - col);
                } else {
                        _vte_debug_print(VTE_DEBUG_MISC,
                                         "Cleaning CJK right half at %ld\n",
                                         end);
                        g_assert(end - col == 1 && cell_col->attr.columns == 2);
                        cell_end->c = ' ';
                        cell_end->attr.fragment = 0;
                        cell_end->attr.columns = 1;
                        _vte_invalidate_cells(terminal,
                                              end, 1,
                                              terminal->pvt->cursor.row, 1);
                }
        }

        /* Handle the left hand side.  Converting longer TABs to shorter ones probably
         * wouldn't make that much sense here, so instead convert to spaces. */
        if (G_UNLIKELY (cell_start_is_fragment)) {
                gboolean keep_going = TRUE;
                col = start;
                do {
                        col--;
                        g_assert(col >= 0);  /* The first cell can't be a fragment. */
                        cell_col = _vte_row_data_get_writable (row, col);
                        if (!cell_col->attr.fragment) {
                                if (cell_col->c == '\t') {
                                        _vte_debug_print(VTE_DEBUG_MISC,
                                                         "Replacing left part of TAB with spaces at %ld (%ld => %ld cells)\n",
                                                         col, (long)cell_col->attr.columns, start - col);
                                        /* nothing to do here */
                                } else {
                                        _vte_debug_print(VTE_DEBUG_MISC,
                                                         "Cleaning CJK left half at %ld\n",
                                                         col);
                                        g_assert(start - col == 1);
                                        _vte_invalidate_cells(terminal,
                                                              col, 1,
                                                              terminal->pvt->cursor.row, 1);
                                }
                                keep_going = FALSE;
                        }
                        cell_col->c = ' ';
                        cell_col->attr.fragment = 0;
                        cell_col->attr.columns = 1;
                } while (keep_going);
        }
}

/* Cursor down, with scrolling. */
void
_vte_terminal_cursor_down (VteTerminal *terminal)
{
	long start, end;
	VteScreen *screen;

	screen = terminal->pvt->screen;

        if (terminal->pvt->scrolling_restricted) {
                start = screen->insert_delta + terminal->pvt->scrolling_region.start;
                end = screen->insert_delta + terminal->pvt->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = start + terminal->pvt->row_count - 1;
	}
        if (terminal->pvt->cursor.row == end) {
                if (terminal->pvt->scrolling_restricted) {
			if (start == screen->insert_delta) {
				/* Scroll this line into the scrollback
				 * buffer by inserting a line at the next
				 * line and scrolling the area up. */
				screen->insert_delta++;
                                terminal->pvt->cursor.row++;
				/* update start and end, as they are relative
				 * to insert_delta. */
				start++;
				end++;
                                _vte_terminal_ring_insert (terminal, terminal->pvt->cursor.row, FALSE);
				/* Force the areas below the region to be
				 * redrawn -- they've moved. */
				_vte_terminal_scroll_region(terminal, start,
							    end - start + 1, 1);
				/* Force scroll. */
				_vte_terminal_adjust_adjustments(terminal);
			} else {
				/* If we're at the bottom of the scrolling
				 * region, add a line at the top to scroll the
				 * bottom off. */
				_vte_terminal_ring_remove (terminal, start);
				_vte_terminal_ring_insert (terminal, end, TRUE);
				/* Update the display. */
				_vte_terminal_scroll_region(terminal, start,
							   end - start + 1, -1);
				_vte_invalidate_cells(terminal,
						      0, terminal->pvt->column_count,
						      end - 2, 2);
			}
		} else {
			/* Scroll up with history. */
                        terminal->pvt->cursor.row++;
			_vte_terminal_update_insert_delta(terminal);
		}

		/* Match xterm and fill the new row when scrolling. */
#if 0           /* Disable for now: see bug 754596. */
                if (terminal->pvt->fill_defaults.attr.back != VTE_DEFAULT_BG) {
			VteRowData *rowdata;
			rowdata = _vte_terminal_ensure_row (terminal);
                        _vte_row_data_fill (rowdata, &terminal->pvt->fill_defaults, terminal->pvt->column_count);
		}
#endif
	} else {
		/* Otherwise, just move the cursor down. */
                terminal->pvt->cursor.row++;
	}
}

/* Drop the scrollback. */
void
_vte_terminal_drop_scrollback (VteTerminal *terminal)
{
        /* Only for normal screen; alternate screen doesn't have a scrollback. */
        _vte_ring_drop_scrollback (terminal->pvt->normal_screen.row_data,
                                   terminal->pvt->normal_screen.insert_delta);

        if (terminal->pvt->screen == &terminal->pvt->normal_screen) {
                vte_terminal_queue_adjustment_value_changed (terminal, terminal->pvt->normal_screen.insert_delta);
                _vte_terminal_adjust_adjustments_full (terminal);
        }
}

/* Restore cursor on a screen. */
void
_vte_terminal_restore_cursor (VteTerminal *terminal, VteScreen *screen)
{
        terminal->pvt->cursor.col = screen->saved.cursor.col;
        terminal->pvt->cursor.row = screen->insert_delta + CLAMP(screen->saved.cursor.row,
                                                                 0, terminal->pvt->row_count - 1);

        terminal->pvt->reverse_mode = screen->saved.reverse_mode;
        terminal->pvt->origin_mode = screen->saved.origin_mode;
        terminal->pvt->sendrecv_mode = screen->saved.sendrecv_mode;
        terminal->pvt->insert_mode = screen->saved.insert_mode;
        terminal->pvt->linefeed_mode = screen->saved.linefeed_mode;
        terminal->pvt->defaults = screen->saved.defaults;
        terminal->pvt->color_defaults = screen->saved.color_defaults;
        terminal->pvt->fill_defaults = screen->saved.fill_defaults;
        terminal->pvt->character_replacements[0] = screen->saved.character_replacements[0];
        terminal->pvt->character_replacements[1] = screen->saved.character_replacements[1];
        terminal->pvt->character_replacement = screen->saved.character_replacement;
}

/* Save cursor on a screen. */
void
_vte_terminal_save_cursor (VteTerminal *terminal, VteScreen *screen)
{
        screen->saved.cursor.col = terminal->pvt->cursor.col;
        screen->saved.cursor.row = terminal->pvt->cursor.row - screen->insert_delta;

        screen->saved.reverse_mode = terminal->pvt->reverse_mode;
        screen->saved.origin_mode = terminal->pvt->origin_mode;
        screen->saved.sendrecv_mode = terminal->pvt->sendrecv_mode;
        screen->saved.insert_mode = terminal->pvt->insert_mode;
        screen->saved.linefeed_mode = terminal->pvt->linefeed_mode;
        screen->saved.defaults = terminal->pvt->defaults;
        screen->saved.color_defaults = terminal->pvt->color_defaults;
        screen->saved.fill_defaults = terminal->pvt->fill_defaults;
        screen->saved.character_replacements[0] = terminal->pvt->character_replacements[0];
        screen->saved.character_replacements[1] = terminal->pvt->character_replacements[1];
        screen->saved.character_replacement = terminal->pvt->character_replacement;
}

/* Insert a single character into the stored data array. */
gboolean
_vte_terminal_insert_char(VteTerminal *terminal, gunichar c,
			 gboolean insert, gboolean invalidate_now)
{
	VteCellAttr attr;
	VteRowData *row;
	long col;
	int columns, i;
	VteScreen *screen;
	gboolean line_wrapped = FALSE; /* cursor moved before char inserted */

        /* DEC Special Character and Line Drawing Set.  VT100 and higher (per XTerm docs). */
        static gunichar line_drawing_map[31] = {
                0x25c6,  /* ` => diamond */
                0x2592,  /* a => checkerboard */
                0x2409,  /* b => HT symbol */
                0x240c,  /* c => FF symbol */
                0x240d,  /* d => CR symbol */
                0x240a,  /* e => LF symbol */
                0x00b0,  /* f => degree */
                0x00b1,  /* g => plus/minus */
                0x2424,  /* h => NL symbol */
                0x240b,  /* i => VT symbol */
                0x2518,  /* j => downright corner */
                0x2510,  /* k => upright corner */
                0x250c,  /* l => upleft corner */
                0x2514,  /* m => downleft corner */
                0x253c,  /* n => cross */
                0x23ba,  /* o => scan line 1/9 */
                0x23bb,  /* p => scan line 3/9 */
                0x2500,  /* q => horizontal line (also scan line 5/9) */
                0x23bc,  /* r => scan line 7/9 */
                0x23bd,  /* s => scan line 9/9 */
                0x251c,  /* t => left t */
                0x2524,  /* u => right t */
                0x2534,  /* v => bottom t */
                0x252c,  /* w => top t */
                0x2502,  /* x => vertical line */
                0x2264,  /* y => <= */
                0x2265,  /* z => >= */
                0x03c0,  /* { => pi */
                0x2260,  /* | => not equal */
                0x00a3,  /* } => pound currency sign */
                0x00b7,  /* ~ => bullet */
        };

	screen = terminal->pvt->screen;
        insert |= terminal->pvt->insert_mode;
	invalidate_now |= insert;

	/* If we've enabled the special drawing set, map the characters to
	 * Unicode. */
        if (G_UNLIKELY (*terminal->pvt->character_replacement == VTE_CHARACTER_REPLACEMENT_LINE_DRAWING)) {
                if (c >= 96 && c <= 126)
                        c = line_drawing_map[c - 96];
        } else if (G_UNLIKELY (*terminal->pvt->character_replacement == VTE_CHARACTER_REPLACEMENT_BRITISH)) {
                if (G_UNLIKELY (c == '#'))
                        c = 0x00a3;  /* pound sign */
        }

	/* Figure out how many columns this character should occupy. */
        columns = _vte_unichar_width(c, terminal->pvt->utf8_ambiguous_width);

	/* If we're autowrapping here, do it. */
        col = terminal->pvt->cursor.col;
	if (G_UNLIKELY (columns && col + columns > terminal->pvt->column_count)) {
		if (terminal->pvt->autowrap) {
			_vte_debug_print(VTE_DEBUG_ADJ,
					"Autowrapping before character\n");
			/* Wrap. */
			/* XXX clear to the end of line */
                        col = terminal->pvt->cursor.col = 0;
			/* Mark this line as soft-wrapped. */
			row = _vte_terminal_ensure_row (terminal);
			row->attr.soft_wrapped = 1;
			_vte_terminal_cursor_down (terminal);
		} else {
			/* Don't wrap, stay at the rightmost column. */
                        col = terminal->pvt->cursor.col =
				terminal->pvt->column_count - columns;
		}
		line_wrapped = TRUE;
	}

	_vte_debug_print(VTE_DEBUG_PARSE,
			"Inserting %ld '%c' (%d/%d) (%ld+%d, %ld), delta = %ld; ",
			(long)c, c < 256 ? c : ' ',
                         (int)terminal->pvt->color_defaults.attr.fore,
                         (int)terminal->pvt->color_defaults.attr.back,
                        col, columns, (long)terminal->pvt->cursor.row,
			(long)screen->insert_delta);


	if (G_UNLIKELY (columns == 0)) {

		/* It's a combining mark */

		long row_num;
		VteCell *cell;

		_vte_debug_print(VTE_DEBUG_PARSE, "combining U+%04X", c);

                row_num = terminal->pvt->cursor.row;
		row = NULL;
		if (G_UNLIKELY (col == 0)) {
			/* We are at first column.  See if the previous line softwrapped.
			 * If it did, move there.  Otherwise skip inserting. */

			if (G_LIKELY (row_num > 0)) {
				row_num--;
				row = _vte_terminal_find_row_data_writable (terminal, row_num);

				if (row) {
					if (!row->attr.soft_wrapped)
						row = NULL;
					else
						col = _vte_row_data_length (row);
				}
			}
		} else {
			row = _vte_terminal_find_row_data_writable (terminal, row_num);
		}

		if (G_UNLIKELY (!row || !col))
			goto not_inserted;

		/* Combine it on the previous cell */

		col--;
		cell = _vte_row_data_get_writable (row, col);

		if (G_UNLIKELY (!cell))
			goto not_inserted;

		/* Find the previous cell */
		while (cell && cell->attr.fragment && col > 0)
			cell = _vte_row_data_get_writable (row, --col);
		if (G_UNLIKELY (!cell || cell->c == '\t'))
			goto not_inserted;

		/* Combine the new character on top of the cell string */
		c = _vte_unistr_append_unichar (cell->c, c);

		/* And set it */
		columns = cell->attr.columns;
		for (i = 0; i < columns; i++) {
			cell = _vte_row_data_get_writable (row, col++);
			cell->c = c;
		}

		/* Always invalidate since we put the mark on the *previous* cell
		 * and the higher level code doesn't know this. */
		_vte_invalidate_cells(terminal,
				      col - columns,
				      columns,
				      row_num, 1);

		goto done;
	}

	/* Make sure we have enough rows to hold this data. */
	row = vte_terminal_ensure_cursor (terminal);
	g_assert(row != NULL);

	if (insert) {
                _vte_terminal_cleanup_fragments (terminal, col, col);
		for (i = 0; i < columns; i++)
                        _vte_row_data_insert (row, col + i, &terminal->pvt->color_defaults);
	} else {
                _vte_terminal_cleanup_fragments (terminal, col, col + columns);
		_vte_row_data_fill (row, &basic_cell.cell, col + columns);
	}

        attr = terminal->pvt->defaults.attr;
        attr.fore = terminal->pvt->color_defaults.attr.fore;
        attr.back = terminal->pvt->color_defaults.attr.back;
	attr.columns = columns;

	{
		VteCell *pcell = _vte_row_data_get_writable (row, col);
		pcell->c = c;
		pcell->attr = attr;
		col++;
	}

	/* insert wide-char fragments */
	attr.fragment = 1;
	for (i = 1; i < columns; i++) {
		VteCell *pcell = _vte_row_data_get_writable (row, col);
		pcell->c = c;
		pcell->attr = attr;
		col++;
	}
	if (_vte_row_data_length (row) > terminal->pvt->column_count)
		_vte_terminal_cleanup_fragments (terminal, terminal->pvt->column_count, _vte_row_data_length (row));
	_vte_row_data_shrink (row, terminal->pvt->column_count);

	/* Signal that this part of the window needs drawing. */
	if (G_UNLIKELY (invalidate_now)) {
		_vte_invalidate_cells(terminal,
				col - columns,
				insert ? terminal->pvt->column_count : columns,
                                terminal->pvt->cursor.row, 1);
	}

        terminal->pvt->cursor.col = col;

done:
	/* We added text, so make a note of it. */
	terminal->pvt->text_inserted_flag = TRUE;

not_inserted:
	_vte_debug_print(VTE_DEBUG_ADJ|VTE_DEBUG_PARSE,
			"insertion delta => %ld.\n",
			(long)screen->insert_delta);
	return line_wrapped;
}

static void
vte_terminal_child_watch_cb(GPid pid,
                            int status,
                            VteTerminal *terminal)
{
	if (terminal == NULL) {
		/* The child outlived VteTerminal. Do nothing, we're happy that Glib
		 * read its exit data and hence it's no longer there as zombie. */
		return;
	}

	if (pid == terminal->pvt->pty_pid) {
                GObject *object = G_OBJECT(terminal);

                g_object_ref(object);
                g_object_freeze_notify(object);

		_VTE_DEBUG_IF (VTE_DEBUG_LIFECYCLE) {
			g_printerr ("Child[%d] exited with status %d\n",
					pid, status);
#ifdef HAVE_SYS_WAIT_H
			if (WIFEXITED (status)) {
				g_printerr ("Child[%d] exit code %d.\n",
						pid, WEXITSTATUS (status));
			}else if (WIFSIGNALED (status)) {
				g_printerr ("Child[%d] dies with signal %d.\n",
						pid, WTERMSIG (status));
			}
#endif
		}

		terminal->pvt->child_watch_source = 0;
		terminal->pvt->pty_pid = -1;

		/* Close out the PTY. */
                vte_terminal_set_pty(terminal, NULL);

		/* Tell observers what's happened. */
		vte_terminal_emit_child_exited(terminal, status);

                g_object_thaw_notify(object);
                g_object_unref(object);

                /* Note: terminal may be destroyed at this point */
	}
}

static void mark_input_source_invalid(VteTerminal *terminal)
{
	_vte_debug_print (VTE_DEBUG_IO, "removed poll of vte_terminal_io_read\n");
	terminal->pvt->pty_input_source = 0;
}
static void
_vte_terminal_connect_pty_read(VteTerminal *terminal)
{
	if (terminal->pvt->pty_channel == NULL) {
		return;
	}

	if (terminal->pvt->pty_input_source == 0) {
		_vte_debug_print (VTE_DEBUG_IO, "polling vte_terminal_io_read\n");
		terminal->pvt->pty_input_source =
			g_io_add_watch_full(terminal->pvt->pty_channel,
					    VTE_CHILD_INPUT_PRIORITY,
					    (GIOCondition)(G_IO_IN | G_IO_HUP),
					    (GIOFunc) vte_terminal_io_read,
					    terminal,
					    (GDestroyNotify) mark_input_source_invalid);
	}
}

static void mark_output_source_invalid(VteTerminal *terminal)
{
	_vte_debug_print (VTE_DEBUG_IO, "removed poll of vte_terminal_io_write\n");
	terminal->pvt->pty_output_source = 0;
}
static void
_vte_terminal_connect_pty_write(VteTerminal *terminal)
{
        VteTerminalPrivate *pvt = terminal->pvt;

        g_assert(pvt->pty != NULL);
        g_warn_if_fail(pvt->input_enabled);

	if (terminal->pvt->pty_channel == NULL) {
		pvt->pty_channel =
			g_io_channel_unix_new(vte_pty_get_fd(pvt->pty));
	}

	if (terminal->pvt->pty_output_source == 0) {
		if (vte_terminal_io_write (terminal->pvt->pty_channel,
					     G_IO_OUT,
					     terminal))
		{
			_vte_debug_print (VTE_DEBUG_IO, "polling vte_terminal_io_write\n");
			terminal->pvt->pty_output_source =
				g_io_add_watch_full(terminal->pvt->pty_channel,
						    VTE_CHILD_OUTPUT_PRIORITY,
						    G_IO_OUT,
						    (GIOFunc) vte_terminal_io_write,
						    terminal,
						    (GDestroyNotify) mark_output_source_invalid);
		}
	}
}

static void
_vte_terminal_disconnect_pty_read(VteTerminal *terminal)
{
	if (terminal->pvt->pty_input_source != 0) {
		_vte_debug_print (VTE_DEBUG_IO, "disconnecting poll of vte_terminal_io_read\n");
		g_source_remove(terminal->pvt->pty_input_source);
		terminal->pvt->pty_input_source = 0;
	}
}

static void
_vte_terminal_disconnect_pty_write(VteTerminal *terminal)
{
	if (terminal->pvt->pty_output_source != 0) {
		_vte_debug_print (VTE_DEBUG_IO, "disconnecting poll of vte_terminal_io_write\n");

		g_source_remove(terminal->pvt->pty_output_source);
		terminal->pvt->pty_output_source = 0;
	}
}

/**
 * vte_terminal_pty_new_sync:
 * @terminal: a #VteTerminal
 * @flags: flags from #VtePtyFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Creates a new #VtePty, and sets the emulation property
 * from #VteTerminal:emulation.
 *
 * See vte_pty_new() for more information.
 *
 * Returns: (transfer full): a new #VtePty
 */
VtePty *
vte_terminal_pty_new_sync(VteTerminal *terminal,
                          VtePtyFlags flags,
                          GCancellable *cancellable,
                          GError **error)
{
        VtePty *pty;

        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);

        pty = vte_pty_new_sync(flags, cancellable, error);
        if (pty == NULL)
                return NULL;

        return pty;
}

/**
 * vte_terminal_watch_child:
 * @terminal: a #VteTerminal
 * @child_pid: a #GPid
 *
 * Watches @child_pid. When the process exists, the #VteTerminal::child-exited
 * signal will be called with the child's exit status.
 *
 * Prior to calling this function, a #VtePty must have been set in @terminal
 * using vte_terminal_set_pty().
 * When the child exits, the terminal's #VtePty will be set to %NULL.
 *
 * Note: g_child_watch_add() or g_child_watch_add_full() must not have
 * been called for @child_pid, nor a #GSource for it been created with
 * g_child_watch_source_new().
 *
 * Note: when using the g_spawn_async() family of functions,
 * the %G_SPAWN_DO_NOT_REAP_CHILD flag MUST have been passed.
 */
void
vte_terminal_watch_child (VteTerminal *terminal,
                          GPid child_pid)
{
        VteTerminalPrivate *pvt;
        GObject *object;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(child_pid != -1);

        pvt = terminal->pvt;
        g_return_if_fail(pvt->pty != NULL);

        // FIXMEchpe: support passing child_pid = -1 to remove the wathch

        object = G_OBJECT(terminal);

        g_object_freeze_notify(object);

        /* Set this as the child's pid. */
        pvt->pty_pid = child_pid;

        /* Catch a child-exited signal from the child pid. */
        if (terminal->pvt->child_watch_source != 0) {
                g_source_remove (terminal->pvt->child_watch_source);
        }
        terminal->pvt->child_watch_source =
                g_child_watch_add_full(G_PRIORITY_HIGH,
                                       child_pid,
                                       (GChildWatchFunc)vte_terminal_child_watch_cb,
                                       terminal, NULL);

        /* FIXMEchpe: call vte_terminal_set_size here? */

        g_object_thaw_notify(object);
}

/**
 * vte_terminal_spawn_sync:
 * @terminal: a #VteTerminal
 * @pty_flags: flags from #VtePtyFlags
 * @working_directory: (allow-none): the name of a directory the command should start
 *   in, or %NULL to use the current working directory
 * @argv: (array zero-terminated=1) (element-type filename): child's argument vector
 * @envv: (allow-none) (array zero-terminated=1) (element-type filename): a list of environment
 *   variables to be added to the environment before starting the process, or %NULL
 * @spawn_flags: flags from #GSpawnFlags
 * @child_setup: (allow-none) (scope call): an extra child setup function to run in the child just before exec(), or %NULL
 * @child_setup_data: user data for @child_setup
 * @child_pid: (out) (allow-none) (transfer full): a location to store the child PID, or %NULL
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Starts the specified command under a newly-allocated controlling
 * pseudo-terminal.  The @argv and @envv lists should be %NULL-terminated.
 * The "TERM" environment variable is automatically set to a default value,
 * but can be overridden from @envv.
 * @pty_flags controls logging the session to the specified system log files.
 *
 * Note that %G_SPAWN_DO_NOT_REAP_CHILD will always be added to @spawn_flags.
 *
 * Note that unless @spawn_flags contains %G_SPAWN_LEAVE_DESCRIPTORS_OPEN, all file
 * descriptors except stdin/stdout/stderr will be closed before calling exec()
 * in the child.
 *
 * See vte_pty_new(), g_spawn_async() and vte_terminal_watch_child() for more information.
 *
 * Returns: %TRUE on success, or %FALSE on error with @error filled in
 */
gboolean
vte_terminal_spawn_sync(VteTerminal *terminal,
                               VtePtyFlags pty_flags,
                               const char *working_directory,
                               char **argv,
                               char **envv,
                               GSpawnFlags spawn_flags_,
                               GSpawnChildSetupFunc child_setup,
                               gpointer child_setup_data,
                               GPid *child_pid /* out */,
                               GCancellable *cancellable,
                               GError **error)
{
        guint spawn_flags = (guint)spawn_flags_;
        VtePty *pty;
        GPid pid;

        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        g_return_val_if_fail(argv != NULL, FALSE);
        g_return_val_if_fail(child_setup_data == NULL || child_setup, FALSE);
        g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

        pty = vte_terminal_pty_new_sync(terminal, pty_flags, cancellable, error);
        if (pty == NULL)
                return FALSE;

        /* FIXMEchpe: is this flag needed */
        spawn_flags |= G_SPAWN_CHILD_INHERITS_STDIN;

        if (!__vte_pty_spawn(pty,
                             working_directory,
                             argv,
                             envv,
                             (GSpawnFlags)spawn_flags,
                             child_setup, child_setup_data,
                             &pid,
                             error)) {
                g_object_unref(pty);
                return FALSE;
        }

        vte_terminal_set_pty(terminal, pty);
        vte_terminal_watch_child(terminal, pid);
        g_object_unref (pty);

        if (child_pid)
                *child_pid = pid;

        return TRUE;
}

/* Handle an EOF from the client. */
static void
vte_terminal_eof(GIOChannel *channel, VteTerminal *terminal)
{
        GObject *object = G_OBJECT(terminal);

        g_object_freeze_notify(object);

        vte_terminal_set_pty(terminal, NULL);

	/* Emit a signal that we read an EOF. */
	vte_terminal_queue_eof(terminal);

        g_object_thaw_notify(object);
}

/* Reset the input method context. */
static void
vte_terminal_im_reset(VteTerminal *terminal)
{
	if (gtk_widget_get_realized (&terminal->widget)) {
		gtk_im_context_reset(terminal->pvt->im_context);
		if (terminal->pvt->im_preedit != NULL) {
			g_free(terminal->pvt->im_preedit);
			terminal->pvt->im_preedit = NULL;
		}
		if (terminal->pvt->im_preedit_attrs != NULL) {
			pango_attr_list_unref(terminal->pvt->im_preedit_attrs);
			terminal->pvt->im_preedit_attrs = NULL;
		}
	}
}

/* Emit whichever signals are called for here. */
static void
vte_terminal_emit_pending_text_signals(VteTerminal *terminal)
{
	if (terminal->pvt->text_modified_flag) {
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting buffered `text-modified'.\n");
		vte_terminal_emit_text_modified(terminal);
		terminal->pvt->text_modified_flag = FALSE;
	}
	if (terminal->pvt->text_inserted_flag) {
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting buffered `text-inserted'\n");
		_vte_terminal_emit_text_inserted(terminal);
		terminal->pvt->text_inserted_flag = FALSE;
	}
	if (terminal->pvt->text_deleted_flag) {
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting buffered `text-deleted'\n");
		_vte_terminal_emit_text_deleted(terminal);
		terminal->pvt->text_deleted_flag = FALSE;
	}
}

/* Process incoming data, first converting it to unicode characters, and then
 * processing control sequences. */
static void
vte_terminal_process_incoming(VteTerminal *terminal)
{
	VteScreen *screen;
	VteVisualPosition cursor;
	gboolean cursor_visible;
	GdkPoint bbox_topleft, bbox_bottomright;
	gunichar *wbuf, c;
	long wcount, start;
        long top_row, bottom_row;
	gboolean leftovers, modified, bottom, again;
	gboolean invalidated_text;
	gboolean in_scroll_region;
	GArray *unichars;
	struct _vte_incoming_chunk *chunk, *next_chunk, *achunk = NULL;

	_vte_debug_print(VTE_DEBUG_IO,
			"Handler processing %" G_GSIZE_FORMAT " bytes over %" G_GSIZE_FORMAT " chunks + %d bytes pending.\n",
			_vte_incoming_chunks_length(terminal->pvt->incoming),
			_vte_incoming_chunks_count(terminal->pvt->incoming),
			terminal->pvt->pending->len);
	_vte_debug_print (VTE_DEBUG_WORK, "(");

	screen = terminal->pvt->screen;

        bottom = screen->insert_delta == (long) screen->scroll_delta;

        top_row = _vte_terminal_first_displayed_row(terminal);
        bottom_row = _vte_terminal_last_displayed_row(terminal);

	/* Save the current cursor position. */
        cursor = terminal->pvt->cursor;
	cursor_visible = terminal->pvt->cursor_visible;

        in_scroll_region = terminal->pvt->scrolling_restricted
            && (terminal->pvt->cursor.row >= (screen->insert_delta + terminal->pvt->scrolling_region.start))
            && (terminal->pvt->cursor.row <= (screen->insert_delta + terminal->pvt->scrolling_region.end));

	/* We should only be called when there's data to process. */
	g_assert(terminal->pvt->incoming ||
		 (terminal->pvt->pending->len > 0));

	/* Convert the data into unicode characters. */
	unichars = terminal->pvt->pending;
	for (chunk = _vte_incoming_chunks_reverse (terminal->pvt->incoming);
			chunk != NULL;
			chunk = next_chunk) {
		gsize processed;
		next_chunk = chunk->next;
		if (chunk->len == 0) {
			goto skip_chunk;
		}
		processed = _vte_iso2022_process(terminal->pvt->iso2022,
				chunk->data, chunk->len,
				unichars);
		if (G_UNLIKELY (processed != chunk->len)) {
			/* shuffle the data about */
			g_memmove (chunk->data, chunk->data + processed,
					chunk->len - processed);
			chunk->len = chunk->len - processed;
			processed = sizeof (chunk->data) - chunk->len;
			if (processed != 0 && next_chunk !=  NULL) {
				if (next_chunk->len <= processed) {
					/* consume it entirely */
					memcpy (chunk->data + chunk->len,
							next_chunk->data,
							next_chunk->len);
					chunk->len += next_chunk->len;
					chunk->next = next_chunk->next;
					release_chunk (next_chunk);
				} else {
					/* next few bytes */
					memcpy (chunk->data + chunk->len,
							next_chunk->data,
							processed);
					chunk->len += processed;
					g_memmove (next_chunk->data,
							next_chunk->data + processed,
							next_chunk->len - processed);
					next_chunk->len -= processed;
				}
				next_chunk = chunk; /* repeat */
			} else {
				break;
			}
		} else {
skip_chunk:
			/* cache the last chunk */
			if (achunk) {
				release_chunk (achunk);
			}
			achunk = chunk;
		}
	}
	if (achunk) {
		if (chunk != NULL) {
			release_chunk (achunk);
		} else {
			chunk = achunk;
			chunk->next = NULL;
			chunk->len = 0;
		}
	}
	terminal->pvt->incoming = chunk;

	/* Compute the number of unicode characters we got. */
	wbuf = &g_array_index(unichars, gunichar, 0);
	wcount = unichars->len;

	/* Try initial substrings. */
	start = 0;
	modified = leftovers = again = FALSE;
	invalidated_text = FALSE;

	bbox_bottomright.x = bbox_bottomright.y = -G_MAXINT;
	bbox_topleft.x = bbox_topleft.y = G_MAXINT;

	while (start < wcount && !leftovers) {
		const char *match;
		const gunichar *next;
		GValueArray *params = NULL;

		/* Try to match any control sequences. */
		_vte_matcher_match(terminal->pvt->matcher,
				   &wbuf[start],
				   wcount - start,
				   &match,
				   &next,
				   &params);
		/* We're in one of three possible situations now.
		 * First, the match string is a non-empty string and next
		 * points to the first character which isn't part of this
		 * sequence. */
		if ((match != NULL) && (match[0] != '\0')) {
			gboolean new_in_scroll_region;

			/* Call the right sequence handler for the requested
			 * behavior. */
			_vte_terminal_handle_sequence(terminal,
						      match,
						      params);
			/* Skip over the proper number of unicode chars. */
			start = (next - wbuf);
			modified = TRUE;

                        new_in_scroll_region = terminal->pvt->scrolling_restricted
                            && (terminal->pvt->cursor.row >= (screen->insert_delta + terminal->pvt->scrolling_region.start))
                            && (terminal->pvt->cursor.row <= (screen->insert_delta + terminal->pvt->scrolling_region.end));

                        /* delta may have changed from sequence. */
                        top_row = _vte_terminal_first_displayed_row(terminal);
                        bottom_row = _vte_terminal_last_displayed_row(terminal);

			/* if we have moved greatly during the sequence handler, or moved
                         * into a scroll_region from outside it, restart the bbox.
                         */
			if (invalidated_text &&
					((new_in_scroll_region && !in_scroll_region) ||
                                         (terminal->pvt->cursor.col > bbox_bottomright.x + VTE_CELL_BBOX_SLACK ||
                                          terminal->pvt->cursor.col < bbox_topleft.x - VTE_CELL_BBOX_SLACK     ||
                                          terminal->pvt->cursor.row > bbox_bottomright.y + VTE_CELL_BBOX_SLACK ||
                                          terminal->pvt->cursor.row < bbox_topleft.y - VTE_CELL_BBOX_SLACK))) {
				/* Clip off any part of the box which isn't already on-screen. */
				bbox_topleft.x = MAX(bbox_topleft.x, 0);
                                bbox_topleft.y = MAX(bbox_topleft.y, top_row);
				bbox_bottomright.x = MIN(bbox_bottomright.x,
						terminal->pvt->column_count);
				/* lazily apply the +1 to the cursor_row */
				bbox_bottomright.y = MIN(bbox_bottomright.y + 1,
                                                bottom_row + 1);

				_vte_invalidate_cells(terminal,
						bbox_topleft.x,
						bbox_bottomright.x - bbox_topleft.x,
						bbox_topleft.y,
						bbox_bottomright.y - bbox_topleft.y);

				invalidated_text = FALSE;
				bbox_bottomright.x = bbox_bottomright.y = -G_MAXINT;
				bbox_topleft.x = bbox_topleft.y = G_MAXINT;
			}

			in_scroll_region = new_in_scroll_region;
		} else
		/* Second, we have a NULL match, and next points to the very
		 * next character in the buffer.  Insert the character which
		 * we're currently examining into the screen. */
		if (match == NULL) {
			c = wbuf[start];
			/* If it's a control character, permute the order, per
			 * vttest. */
			if ((c != *next) &&
			    ((*next & 0x1f) == *next) &&
			    (start + 1 < next - wbuf)) {
				const gunichar *tnext = NULL;
				const char *tmatch = NULL;
				gunichar ctrl;
				int i;
				/* We don't want to permute it if it's another
				 * control sequence, so check if it is. */
				_vte_matcher_match(terminal->pvt->matcher,
						   next,
						   wcount - (next - wbuf),
						   &tmatch,
						   &tnext,
						   NULL);
				/* We only do this for non-control-sequence
				 * characters and random garbage. */
				if (tnext == next + 1) {
					/* Save the control character. */
					ctrl = *next;
					/* Move everything before it up a
					 * slot.  */
					for (i = next - wbuf; i > start; i--) {
						wbuf[i] = wbuf[i - 1];
					}
					/* Move the control character to the
					 * front. */
					wbuf[i] = ctrl;
					goto next_match;
				}
			}
			_VTE_DEBUG_IF(VTE_DEBUG_PARSE) {
                                if (c > 255) {
                                        g_printerr("U+%04lx\n", (long) c);
				} else {
                                        if (c > 127) {
						g_printerr("%ld = ",
                                                                (long) c);
					}
                                        if (c < 32) {
						g_printerr("^%lc\n",
                                                                (wint_t)c + 64);
					} else {
						g_printerr("`%lc'\n",
                                                                (wint_t)c);
					}
				}
			}

			bbox_topleft.x = MIN(bbox_topleft.x,
                                        terminal->pvt->cursor.col);
			bbox_topleft.y = MIN(bbox_topleft.y,
                                        terminal->pvt->cursor.row);

			/* Insert the character. */
			if (G_UNLIKELY (_vte_terminal_insert_char(terminal, c,
						 FALSE, FALSE))) {
				/* line wrapped, correct bbox */
				if (invalidated_text &&
                                                (terminal->pvt->cursor.col > bbox_bottomright.x + VTE_CELL_BBOX_SLACK	||
                                                 terminal->pvt->cursor.col < bbox_topleft.x - VTE_CELL_BBOX_SLACK	||
                                                 terminal->pvt->cursor.row > bbox_bottomright.y + VTE_CELL_BBOX_SLACK	||
                                                 terminal->pvt->cursor.row < bbox_topleft.y - VTE_CELL_BBOX_SLACK)) {
					/* Clip off any part of the box which isn't already on-screen. */
					bbox_topleft.x = MAX(bbox_topleft.x, 0);
                                        bbox_topleft.y = MAX(bbox_topleft.y, top_row);
					bbox_bottomright.x = MIN(bbox_bottomright.x,
							terminal->pvt->column_count);
					/* lazily apply the +1 to the cursor_row */
					bbox_bottomright.y = MIN(bbox_bottomright.y + 1,
                                                        bottom_row + 1);

					_vte_invalidate_cells(terminal,
							bbox_topleft.x,
							bbox_bottomright.x - bbox_topleft.x,
							bbox_topleft.y,
							bbox_bottomright.y - bbox_topleft.y);
					bbox_bottomright.x = bbox_bottomright.y = -G_MAXINT;
					bbox_topleft.x = bbox_topleft.y = G_MAXINT;

				}
				bbox_topleft.x = MIN(bbox_topleft.x, 0);
				bbox_topleft.y = MIN(bbox_topleft.y,
                                                     terminal->pvt->cursor.row);
			}
			/* Add the cells over which we have moved to the region
			 * which we need to refresh for the user. */
			bbox_bottomright.x = MAX(bbox_bottomright.x,
                                                 terminal->pvt->cursor.col);
                        /* cursor.row + 1 (defer until inv.) */
			bbox_bottomright.y = MAX(bbox_bottomright.y,
                                                 terminal->pvt->cursor.row);
			invalidated_text = TRUE;

			/* We *don't* emit flush pending signals here. */
			modified = TRUE;
			start++;
		} else {
			/* Case three: the read broke in the middle of a
			 * control sequence, so we're undecided with no more
			 * data to consult. If we have data following the
			 * middle of the sequence, then it's just garbage data,
			 * and for compatibility, we should discard it. */
			if (wbuf + wcount > next) {
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Invalid control "
						"sequence, discarding %ld "
						"characters.\n",
						(long)(next - (wbuf + start)));
				/* Discard. */
				start = next - wbuf + 1;
			} else {
				/* Pause processing here and wait for more
				 * data before continuing. */
				leftovers = TRUE;
			}
		}

#ifdef VTE_DEBUG
		/* Some safety checks: ensure the visible parts of the buffer
		 * are all in the buffer. */
		g_assert(screen->insert_delta >=
			 _vte_ring_delta(screen->row_data));
		/* The cursor shouldn't be above or below the addressable
		 * part of the display buffer. */
                g_assert(terminal->pvt->cursor.row >= terminal->pvt->screen->insert_delta);
#endif

next_match:
		if (G_LIKELY(params != NULL)) {
			/* Free any parameters we don't care about any more. */
			_vte_matcher_free_params_array(terminal->pvt->matcher,
					params);
		}
	}

	/* Remove most of the processed characters. */
	if (start < wcount) {
		g_array_remove_range(terminal->pvt->pending, 0, start);
	} else {
		g_array_set_size(terminal->pvt->pending, 0);
		/* If we're out of data, we needn't pause to let the
		 * controlling application respond to incoming data, because
		 * the main loop is already going to do that. */
	}

	if (modified) {
		/* Keep the cursor on-screen if we scroll on output, or if
		 * we're currently at the bottom of the buffer. */
		_vte_terminal_update_insert_delta(terminal);
		if (terminal->pvt->scroll_on_output || bottom) {
			vte_terminal_maybe_scroll_to_bottom(terminal);
		}
		/* Deselect the current selection if its contents are changed
		 * by this insertion. */
		if (terminal->pvt->has_selection) {
			char *selection;
			selection =
			vte_terminal_get_text_range(terminal,
						    terminal->pvt->selection_start.row,
						    0,
						    terminal->pvt->selection_end.row,
						    terminal->pvt->column_count,
						    vte_cell_is_selected,
						    NULL,
						    NULL);
			if ((selection == NULL) ||
			    (terminal->pvt->selection_text[VTE_SELECTION_PRIMARY] == NULL) ||
			    (strcmp(selection, terminal->pvt->selection_text[VTE_SELECTION_PRIMARY]) != 0)) {
				terminal->pvt->deselect_all();
			}
			g_free(selection);
		}
	}

	if (modified || (screen != terminal->pvt->screen)) {
		/* Signal that the visible contents changed. */
		_vte_terminal_queue_contents_changed(terminal);
	}

	vte_terminal_emit_pending_signals (terminal);

	if (invalidated_text) {
		/* Clip off any part of the box which isn't already on-screen. */
		bbox_topleft.x = MAX(bbox_topleft.x, 0);
                bbox_topleft.y = MAX(bbox_topleft.y, top_row);
		bbox_bottomright.x = MIN(bbox_bottomright.x,
				terminal->pvt->column_count);
		/* lazily apply the +1 to the cursor_row */
		bbox_bottomright.y = MIN(bbox_bottomright.y + 1,
                                bottom_row + 1);

		_vte_invalidate_cells(terminal,
				bbox_topleft.x,
				bbox_bottomright.x - bbox_topleft.x,
				bbox_topleft.y,
				bbox_bottomright.y - bbox_topleft.y);
	}


        if ((cursor.col != terminal->pvt->cursor.col) ||
            (cursor.row != terminal->pvt->cursor.row)) {
		/* invalidate the old and new cursor positions */
		if (cursor_visible)
			_vte_invalidate_cell(terminal, cursor.col, cursor.row);
		_vte_invalidate_cursor_once(terminal, FALSE);
		terminal->pvt->check_cursor_blink();
		/* Signal that the cursor moved. */
		vte_terminal_queue_cursor_moved(terminal);
	} else if (cursor_visible != terminal->pvt->cursor_visible) {
		_vte_invalidate_cell(terminal, cursor.col, cursor.row);
		terminal->pvt->check_cursor_blink();
	}

	/* Tell the input method where the cursor is. */
	if (gtk_widget_get_realized (&terminal->widget)) {
		GdkRectangle rect;
                rect.x = terminal->pvt->cursor.col *
			 terminal->pvt->char_width + terminal->pvt->padding.left;
		rect.width = terminal->pvt->char_width;
                rect.y = _vte_terminal_row_to_pixel(terminal, terminal->pvt->cursor.row) + terminal->pvt->padding.top;
		rect.height = terminal->pvt->char_height;
		gtk_im_context_set_cursor_location(terminal->pvt->im_context,
						   &rect);
	}

	_vte_debug_print (VTE_DEBUG_WORK, ")");
	_vte_debug_print (VTE_DEBUG_IO,
			"%ld chars and %ld bytes in %" G_GSIZE_FORMAT " chunks left to process.\n",
			(long) unichars->len,
			(long) _vte_incoming_chunks_length(terminal->pvt->incoming),
			_vte_incoming_chunks_count(terminal->pvt->incoming));
}

static inline void
_vte_terminal_enable_input_source (VteTerminal *terminal)
{
	if (terminal->pvt->pty_channel == NULL) {
		return;
	}

	if (terminal->pvt->pty_input_source == 0) {
		_vte_debug_print (VTE_DEBUG_IO, "polling vte_terminal_io_read\n");
		terminal->pvt->pty_input_source =
			g_io_add_watch_full(terminal->pvt->pty_channel,
					    VTE_CHILD_INPUT_PRIORITY,
					    (GIOCondition)(G_IO_IN | G_IO_HUP),
					    (GIOFunc) vte_terminal_io_read,
					    terminal,
					    (GDestroyNotify) mark_input_source_invalid);
	}
}
static void
_vte_terminal_feed_chunks (VteTerminal *terminal, struct _vte_incoming_chunk *chunks)
{
	struct _vte_incoming_chunk *last;

	_vte_debug_print(VTE_DEBUG_IO, "Feed %" G_GSIZE_FORMAT " bytes, in %" G_GSIZE_FORMAT " chunks.\n",
			_vte_incoming_chunks_length(chunks),
			_vte_incoming_chunks_count(chunks));

	for (last = chunks; last->next != NULL; last = last->next) ;
	last->next = terminal->pvt->incoming;
	terminal->pvt->incoming = chunks;
}
/* Read and handle data from the child. */
static gboolean
vte_terminal_io_read(GIOChannel *channel,
		     GIOCondition condition,
		     VteTerminal *terminal)
{
	int err = 0;
	gboolean eof, again = TRUE;

	_vte_debug_print (VTE_DEBUG_WORK, ".");

	/* Check for end-of-file. */
	eof = condition & G_IO_HUP;

	/* Read some data in from this channel. */
	if (condition & G_IO_IN) {
		struct _vte_incoming_chunk *chunk, *chunks = NULL;
		const int fd = g_io_channel_unix_get_fd (channel);
		guchar *bp;
		int rem, len;
		guint bytes, max_bytes;

		/* Limit the amount read between updates, so as to
		 * 1. maintain fairness between multiple terminals;
		 * 2. prevent reading the entire output of a command in one
		 *    pass, i.e. we always try to refresh the terminal ~40Hz.
		 *    See time_process_incoming() where we estimate the
		 *    maximum number of bytes we can read/process in between
		 *    updates.
		 */
		max_bytes = terminal->pvt->active ?
		            g_list_length (active_terminals) - 1 : 0;
		if (max_bytes) {
			max_bytes = terminal->pvt->max_input_bytes / max_bytes;
		} else {
			max_bytes = terminal->pvt->max_input_bytes;
		}
		bytes = terminal->pvt->input_bytes;

		chunk = terminal->pvt->incoming;
		do {
			if (!chunk || chunk->len >= 3*sizeof (chunk->data)/4) {
				chunk = get_chunk ();
				chunk->next = chunks;
				chunks = chunk;
			}
			rem = sizeof (chunk->data) - chunk->len;
			bp = chunk->data + chunk->len;
			len = 0;
			do {
				int ret = read (fd, bp, rem);
				switch (ret){
					case -1:
						err = errno;
						goto out;
					case 0:
						eof = TRUE;
						goto out;
					default:
						bp += ret;
						rem -= ret;
						len += ret;
						break;
				}
			} while (rem);
out:
			chunk->len += len;
			bytes += len;
		} while (bytes < max_bytes &&
		         chunk->len == sizeof (chunk->data));
		if (chunk->len == 0 && chunk == chunks) {
			chunks = chunks->next;
			release_chunk (chunk);
		}

		if (chunks != NULL) {
			_vte_terminal_feed_chunks (terminal, chunks);
		}
		if (!vte_terminal_is_processing (terminal)) {
                        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
			gdk_threads_enter ();
                        G_GNUC_END_IGNORE_DEPRECATIONS;

			vte_terminal_add_process_timeout (terminal);
                        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
			gdk_threads_leave ();
                        G_GNUC_END_IGNORE_DEPRECATIONS;
		}
		terminal->pvt->pty_input_active = len != 0;
		terminal->pvt->input_bytes = bytes;
		again = bytes < max_bytes;

		_vte_debug_print (VTE_DEBUG_IO, "read %d/%d bytes, again? %s, active? %s\n",
				bytes, max_bytes,
				again ? "yes" : "no",
				terminal->pvt->pty_input_active ? "yes" : "no");
	}

	/* Error? */
	switch (err) {
		case 0: /* no error */
			break;
		case EIO: /* Fake an EOF. */
			eof = TRUE;
			break;
		case EAGAIN:
		case EBUSY: /* do nothing */
			break;
		default:
			/* Translators: %s is replaced with error message returned by strerror(). */
			g_warning (_("Error reading from child: " "%s."),
					g_strerror (err));
			break;
	}

	/* If we detected an eof condition, signal one. */
	if (eof) {
		/* potential deadlock ... */
		if (!vte_terminal_is_processing (terminal)) {
                        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
			gdk_threads_enter ();
                        G_GNUC_END_IGNORE_DEPRECATIONS;

			vte_terminal_eof (channel, terminal);

                        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
			gdk_threads_leave ();
                        G_GNUC_END_IGNORE_DEPRECATIONS;
		} else {
			vte_terminal_eof (channel, terminal);
		}

		again = FALSE;
	}

	return again;
}

/**
 * vte_terminal_feed:
 * @terminal: a #VteTerminal
 * @data: (array length=length) (element-type guint8): a string in the terminal's current encoding
 * @length: the length of the string, or -1 to use the full length or a nul-terminated string
 *
 * Interprets @data as if it were data received from a child process.  This
 * can either be used to drive the terminal without a child process, or just
 * to mess with your users.
 */
void
vte_terminal_feed(VteTerminal *terminal, const char *data, gssize length)
{
	/* If length == -1, use the length of the data string. */
	if (length == -1) {
		length = strlen(data);
	}

	/* If we have data, modify the incoming buffer. */
	if (length > 0) {
		struct _vte_incoming_chunk *chunk;
		if (terminal->pvt->incoming &&
				(gsize)length < sizeof (terminal->pvt->incoming->data) - terminal->pvt->incoming->len) {
			chunk = terminal->pvt->incoming;
		} else {
			chunk = get_chunk ();
			_vte_terminal_feed_chunks (terminal, chunk);
		}
		do { /* break the incoming data into chunks */
			gsize rem = sizeof (chunk->data) - chunk->len;
			gsize len = (gsize) length < rem ? (gsize) length : rem;
			memcpy (chunk->data + chunk->len, data, len);
			chunk->len += len;
			length -= len;
			if (length == 0) {
				break;
			}
			data += len;

			chunk = get_chunk ();
			_vte_terminal_feed_chunks (terminal, chunk);
		} while (1);
		vte_terminal_start_processing (terminal);
	}
}

/* Send locally-encoded characters to the child. */
static gboolean
vte_terminal_io_write(GIOChannel *channel,
		      GIOCondition condition,
		      VteTerminal *terminal)
{
	gssize count;
	int fd;
	gboolean leave_open;

	fd = g_io_channel_unix_get_fd(channel);

	count = write(fd, terminal->pvt->outgoing->data,
		      _vte_byte_array_length(terminal->pvt->outgoing));
	if (count != -1) {
		_VTE_DEBUG_IF (VTE_DEBUG_IO) {
			gssize i;
			for (i = 0; i < count; i++) {
				g_printerr("Wrote %c%c\n",
					((guint8)terminal->pvt->outgoing->data[i]) >= 32 ?
					' ' : '^',
					((guint8)terminal->pvt->outgoing->data[i]) >= 32 ?
					terminal->pvt->outgoing->data[i] :
					((guint8)terminal->pvt->outgoing->data[i])  + 64);
			}
		}
		_vte_byte_array_consume(terminal->pvt->outgoing, count);
	}

	if (_vte_byte_array_length(terminal->pvt->outgoing) == 0) {
		leave_open = FALSE;
	} else {
		leave_open = TRUE;
	}

	return leave_open;
}

/* Convert some arbitrarily-encoded data to send to the child. */
static void
vte_terminal_send(VteTerminal *terminal, const char *encoding,
		  const void *data, gssize length,
		  gboolean local_echo, gboolean newline_stuff)
{
	gsize icount, ocount;
	const guchar *ibuf;
	guchar *obuf, *obufptr;
	gchar *cooked;
	VteConv conv;
	long crcount, cooked_length, i;

	g_assert(VTE_IS_TERMINAL(terminal));
	g_assert(encoding && strcmp(encoding, "UTF-8") == 0);

        if (!terminal->pvt->input_enabled)
                return;

	conv = VTE_INVALID_CONV;
	if (strcmp(encoding, "UTF-8") == 0) {
		conv = terminal->pvt->outgoing_conv;
	}
	if (conv == VTE_INVALID_CONV) {
		g_warning (_("Unable to send data to child, invalid charset convertor"));
		return;
	}

	icount = length;
	ibuf = (const guchar *)data;
	ocount = ((length + 1) * VTE_UTF8_BPC) + 1;
	_vte_byte_array_set_minimum_size(terminal->pvt->conv_buffer, ocount);
	obuf = obufptr = terminal->pvt->conv_buffer->data;

	if (_vte_conv(conv, &ibuf, &icount, &obuf, &ocount) == (gsize)-1) {
		g_warning(_("Error (%s) converting data for child, dropping."),
			  g_strerror(errno));
	} else {
		crcount = 0;
		if (newline_stuff) {
			for (i = 0; i < obuf - obufptr; i++) {
				switch (obufptr[i]) {
				case '\015':
					crcount++;
					break;
				default:
					break;
				}
			}
		}
		if (crcount > 0) {
			cooked = (char *)g_malloc(obuf - obufptr + crcount);
			cooked_length = 0;
			for (i = 0; i < obuf - obufptr; i++) {
				switch (obufptr[i]) {
				case '\015':
					cooked[cooked_length++] = '\015';
					cooked[cooked_length++] = '\012';
					break;
				default:
					cooked[cooked_length++] = obufptr[i];
					break;
				}
			}
		} else {
			cooked = (gchar *)obufptr;
			cooked_length = obuf - obufptr;
		}
		/* Tell observers that we're sending this to the child. */
		if (cooked_length > 0) {
			vte_terminal_emit_commit(terminal,
						 cooked, cooked_length);
		}
		/* Echo the text if we've been asked to do so. */
		if ((cooked_length > 0) && local_echo) {
			gunichar *ucs4;
			ucs4 = g_utf8_to_ucs4(cooked, cooked_length,
					      NULL, NULL, NULL);
			if (ucs4 != NULL) {
				int len;
				len = g_utf8_strlen(cooked, cooked_length);
				for (i = 0; i < len; i++) {
					_vte_terminal_insert_char(terminal,
								 ucs4[i],
								 FALSE,
								 TRUE);
				}
				g_free(ucs4);
			}
		}
		/* If there's a place for it to go, add the data to the
		 * outgoing buffer. */
		if ((cooked_length > 0) && (terminal->pvt->pty != NULL)) {
			_vte_byte_array_append(terminal->pvt->outgoing,
					   cooked, cooked_length);
			_VTE_DEBUG_IF(VTE_DEBUG_KEYBOARD) {
				for (i = 0; i < cooked_length; i++) {
					if ((((guint8) cooked[i]) < 32) ||
					    (((guint8) cooked[i]) > 127)) {
						g_printerr(
							"Sending <%02x> "
							"to child.\n",
							cooked[i]);
					} else {
						g_printerr(
							"Sending '%c' "
							"to child.\n",
							cooked[i]);
					}
				}
			}
			/* If we need to start waiting for the child pty to
			 * become available for writing, set that up here. */
			_vte_terminal_connect_pty_write(terminal);
		}
		if (crcount > 0) {
			g_free(cooked);
		}
	}
	return;
}

/**
 * vte_terminal_feed_child:
 * @terminal: a #VteTerminal
 * @text: data to send to the child
 * @length: length of @text in bytes, or -1 if @text is NUL-terminated
 *
 * Sends a block of UTF-8 text to the child as if it were entered by the user
 * at the keyboard.
 */
void
vte_terminal_feed_child(VteTerminal *terminal, const char *text, gssize length)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (!terminal->pvt->input_enabled)
                return;

	if (length == -1) {
		length = strlen(text);
	}
	if (length > 0) {
		vte_terminal_send(terminal, "UTF-8", text, length,
				  FALSE, FALSE);
	}
}

/**
 * vte_terminal_feed_child_binary:
 * @terminal: a #VteTerminal
 * @data: data to send to the child
 * @length: length of @data
 *
 * Sends a block of binary data to the child.
 */
void
vte_terminal_feed_child_binary(VteTerminal *terminal, const guint8 *data, gsize length)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (!terminal->pvt->input_enabled)
                return;

	/* Tell observers that we're sending this to the child. */
	if (length > 0) {
		vte_terminal_emit_commit(terminal,
					 (char*)data, length);

		/* If there's a place for it to go, add the data to the
		 * outgoing buffer. */
		if (terminal->pvt->pty != NULL) {
			_vte_byte_array_append(terminal->pvt->outgoing,
					   data, length);
			/* If we need to start waiting for the child pty to
			 * become available for writing, set that up here. */
			_vte_terminal_connect_pty_write(terminal);
		}
	}
}

static void
vte_terminal_feed_child_using_modes(VteTerminal *terminal,
				    const char *data, glong length)
{
	if (length == ((gssize)-1)) {
		length = strlen(data);
	}
	if (length > 0) {
		vte_terminal_send(terminal, "UTF-8", data, length,
                                  !terminal->pvt->sendrecv_mode,
                                  terminal->pvt->linefeed_mode);
	}
}

/* Send text from the input method to the child. */
static void
vte_terminal_im_commit(GtkIMContext *im_context, gchar *text, VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Input method committed `%s'.\n", text);
	vte_terminal_feed_child_using_modes(terminal, text, -1);
	/* Committed text was committed because the user pressed a key, so
	 * we need to obey the scroll-on-keystroke setting. */
	if (terminal->pvt->scroll_on_keystroke) {
		vte_terminal_maybe_scroll_to_bottom(terminal);
	}
}

/* We've started pre-editing. */
static void
vte_terminal_im_preedit_start(GtkIMContext *im_context, VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Input method pre-edit started.\n");
	terminal->pvt->im_preedit_active = TRUE;
}

/* We've stopped pre-editing. */
static void
vte_terminal_im_preedit_end(GtkIMContext *im_context, VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Input method pre-edit ended.\n");
	terminal->pvt->im_preedit_active = FALSE;
}

/* The pre-edit string changed. */
static void
vte_terminal_im_preedit_changed(GtkIMContext *im_context, VteTerminal *terminal)
{
	gchar *str;
	PangoAttrList *attrs;
	gint cursor;

	gtk_im_context_get_preedit_string(im_context, &str, &attrs, &cursor);
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Input method pre-edit changed (%s,%d).\n",
			str, cursor);

	/* Queue the area where the current preedit string is being displayed
	 * for repainting. */
	_vte_invalidate_cursor_once(terminal, FALSE);

	g_free(terminal->pvt->im_preedit);
	terminal->pvt->im_preedit = str;

	if (terminal->pvt->im_preedit_attrs != NULL) {
		pango_attr_list_unref(terminal->pvt->im_preedit_attrs);
	}
	terminal->pvt->im_preedit_attrs = attrs;

	terminal->pvt->im_preedit_cursor = cursor;

	_vte_invalidate_cursor_once(terminal, FALSE);
}

static void
vte_terminal_set_padding(VteTerminal *terminal)
{
        VteTerminalPrivate *pvt = terminal->pvt;
        GtkWidget *widget = GTK_WIDGET(terminal);
        GtkBorder padding;

        gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
                                      gtk_widget_get_state_flags(widget),
                                      &padding);

        _vte_debug_print(VTE_DEBUG_MISC,
                         "Setting padding to (%d,%d,%d,%d)\n",
                         padding.left, padding.right,
                         padding.top, padding.bottom);

        if (memcmp(&padding, &pvt->padding, sizeof(GtkBorder)) == 0)
                return;

        pvt->padding = padding;

        gtk_widget_queue_resize(widget);
}

void
VteTerminalPrivate::widget_style_updated()
{
        vte_terminal_set_font(m_terminal, m_unscaled_font_desc);
        vte_terminal_set_padding(m_terminal);

        float aspect;
        gtk_widget_style_get(m_widget, "cursor-aspect-ratio", &aspect, nullptr);
        if (!_vte_double_equal(aspect, m_cursor_aspect_ratio)) {
                m_cursor_aspect_ratio = aspect;
                invalidate_cursor_once();
        }
}

void
VteTerminalPrivate::add_cursor_timeout()
{
	if (m_cursor_blink_tag)
		return; /* already added */

	m_cursor_blink_time = 0;
	m_cursor_blink_tag = g_timeout_add_full(G_PRIORITY_LOW,
                                                m_cursor_blink_cycle,
                                                (GSourceFunc)invalidate_cursor_periodic_cb,
                                                this,
                                                NULL);
}

void
VteTerminalPrivate::remove_cursor_timeout()
{
	if (m_cursor_blink_tag == 0)
		return; /* already removed */

	g_source_remove(m_cursor_blink_tag);
	m_cursor_blink_tag = 0;
        if (!m_cursor_blink_state) {
                invalidate_cursor_once();
                m_cursor_blink_state = true;
        }
}

/* Activates / disactivates the cursor blink timer to reduce wakeups */
void
VteTerminalPrivate::check_cursor_blink()
{
	if (m_has_focus &&
	    m_cursor_blinks &&
	    m_cursor_visible)
		add_cursor_timeout();
	else
		remove_cursor_timeout();
}

void
VteTerminalPrivate::beep()
{
	if (m_audible_bell) {
                GdkDisplay *display = gtk_widget_get_display(m_widget);
                gdk_display_beep(display);
	}
}

guint
VteTerminalPrivate::translate_ctrlkey(GdkEventKey *event)
{
	guint keyval;
	GdkKeymap *keymap;
	unsigned int i;

	if (event->keyval < 128)
		return event->keyval;

        keymap = gdk_keymap_get_for_display(gdk_window_get_display (event->window));

	/* Try groups in order to find one mapping the key to ASCII */
	for (i = 0; i < 4; i++) {
		GdkModifierType consumed_modifiers;

		gdk_keymap_translate_keyboard_state (keymap,
                                                     event->hardware_keycode,
                                                     (GdkModifierType)event->state,
                                                     i,
                                                     &keyval, NULL, NULL, &consumed_modifiers);
		if (keyval < 128) {
			_vte_debug_print (VTE_DEBUG_EVENTS,
					"ctrl+Key, group=%d de-grouped into keyval=0x%x\n",
					event->group, keyval);
			return keyval;
		}
	}

	return event->keyval;
}

void
VteTerminalPrivate::read_modifiers(GdkEvent *event)
{
        GdkKeymap *keymap;
	GdkModifierType mods;
        guint mask;

	/* Read the modifiers. */
	if (!gdk_event_get_state((GdkEvent*)event, &mods))
                return;

        keymap = gdk_keymap_get_for_display(gdk_window_get_display(((GdkEventAny*)event)->window));

        gdk_keymap_add_virtual_modifiers (keymap, &mods);

        mask = (guint)mods;
#if 1
        /* HACK! Treat ALT as META; see bug #663779. */
        if (mask & GDK_MOD1_MASK)
                mask |= VTE_META_MASK;
#endif

        m_modifiers = mask;
}

bool
VteTerminalPrivate::widget_key_press(GdkEventKey *event)
{
	char *normal = NULL;
	gssize normal_length = 0;
	int i;
	struct termios tio;
	gboolean scrolled = FALSE, steal = FALSE, modifier = FALSE, handled,
		 suppress_meta_esc = FALSE, add_modifiers = FALSE;
	guint keyval = 0;
	gunichar keychar = 0;
	char keybuf[VTE_UTF8_BPC];

	/* If it's a keypress, record that we got the event, in case the
	 * input method takes the event from us. */
	if (event->type == GDK_KEY_PRESS) {
		/* Store a copy of the key. */
		keyval = event->keyval;
		read_modifiers((GdkEvent*)event);

		/* If we're in margin bell mode and on the border of the
		 * margin, bell. */
		if (m_margin_bell) {
                        if ((m_cursor.col +
			     (glong) m_bell_margin) == m_column_count) {
				beep();
			}
		}

                // FIXMEchpe?
		if (m_cursor_blink_tag != 0) {
			remove_cursor_timeout();
			add_cursor_timeout();
		}

		/* Determine if this is just a modifier key. */
		modifier = _vte_keymap_key_is_modifier(keyval);

		/* Unless it's a modifier key, hide the pointer. */
		if (!modifier) {
			set_pointer_visible(false);
		}

		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Keypress, modifiers=0x%x, "
				"keyval=0x%x, raw string=`%s'.\n",
				m_modifiers,
				keyval, event->string);

		/* We steal many keypad keys here. */
		if (!m_im_preedit_active) {
			switch (keyval) {
			case GDK_KEY_KP_Add:
			case GDK_KEY_KP_Subtract:
			case GDK_KEY_KP_Multiply:
			case GDK_KEY_KP_Divide:
			case GDK_KEY_KP_Enter:
				steal = TRUE;
				break;
			default:
				break;
			}
			if (m_modifiers & VTE_META_MASK) {
				steal = TRUE;
			}
			switch (keyval) {
                        case GDK_KEY_ISO_Lock:
                        case GDK_KEY_ISO_Level2_Latch:
                        case GDK_KEY_ISO_Level3_Shift:
                        case GDK_KEY_ISO_Level3_Latch:
                        case GDK_KEY_ISO_Level3_Lock:
                        case GDK_KEY_ISO_Level5_Shift:
                        case GDK_KEY_ISO_Level5_Latch:
                        case GDK_KEY_ISO_Level5_Lock:
                        case GDK_KEY_ISO_Group_Shift:
                        case GDK_KEY_ISO_Group_Latch:
                        case GDK_KEY_ISO_Group_Lock:
                        case GDK_KEY_ISO_Next_Group:
                        case GDK_KEY_ISO_Next_Group_Lock:
                        case GDK_KEY_ISO_Prev_Group:
                        case GDK_KEY_ISO_Prev_Group_Lock:
                        case GDK_KEY_ISO_First_Group:
                        case GDK_KEY_ISO_First_Group_Lock:
                        case GDK_KEY_ISO_Last_Group:
                        case GDK_KEY_ISO_Last_Group_Lock:
			case GDK_KEY_Multi_key:
			case GDK_KEY_Codeinput:
			case GDK_KEY_SingleCandidate:
			case GDK_KEY_MultipleCandidate:
			case GDK_KEY_PreviousCandidate:
			case GDK_KEY_Kanji:
			case GDK_KEY_Muhenkan:
                        case GDK_KEY_Henkan_Mode:
                        /* case GDK_KEY_Henkan: is GDK_KEY_Henkan_Mode */
			case GDK_KEY_Romaji:
			case GDK_KEY_Hiragana:
			case GDK_KEY_Katakana:
			case GDK_KEY_Hiragana_Katakana:
			case GDK_KEY_Zenkaku:
			case GDK_KEY_Hankaku:
			case GDK_KEY_Zenkaku_Hankaku:
			case GDK_KEY_Touroku:
			case GDK_KEY_Massyo:
			case GDK_KEY_Kana_Lock:
			case GDK_KEY_Kana_Shift:
			case GDK_KEY_Eisu_Shift:
			case GDK_KEY_Eisu_toggle:
                        /* case GDK_KEY_Kanji_Bangou: is GDK_KEY_Codeinput */
                        /* case GDK_KEY_Zen_Koho: is GDK_KEY_MultipleCandidate */
                        /* case GDK_KEY_Mae_Koho: is GDK_KEY_PreviousCandidate */
                        /* case GDK_KEY_kana_switch: is GDK_KEY_ISO_Group_Shift */
                        case GDK_KEY_Hangul:
                        case GDK_KEY_Hangul_Start:
                        case GDK_KEY_Hangul_End:
                        case GDK_KEY_Hangul_Hanja:
                        case GDK_KEY_Hangul_Jamo:
                        case GDK_KEY_Hangul_Romaja:
                        /* case GDK_KEY_Hangul_Codeinput: is GDK_KEY_Codeinput */
                        case GDK_KEY_Hangul_Jeonja:
                        case GDK_KEY_Hangul_Banja:
                        case GDK_KEY_Hangul_PreHanja:
                        case GDK_KEY_Hangul_PostHanja:
                        /* case GDK_KEY_Hangul_SingleCandidate: is GDK_KEY_SingleCandidate */
                        /* case GDK_KEY_Hangul_MultipleCandidate: is GDK_KEY_MultipleCandidate */
                        /* case GDK_KEY_Hangul_PreviousCandidate: is GDK_KEY_PreviousCandidate */
                        case GDK_KEY_Hangul_Special:
                        /* case GDK_KEY_Hangul_switch: is GDK_KEY_ISO_Group_Shift */

				steal = FALSE;
				break;
			default:
				break;
			}
		}
	}

	/* Let the input method at this one first. */
	if (!steal && m_input_enabled) {
		if (gtk_widget_get_realized(m_widget) &&
                    gtk_im_context_filter_keypress(m_im_context, event)) {
			_vte_debug_print(VTE_DEBUG_EVENTS,
					"Keypress taken by IM.\n");
			return true;
		}
	}

	/* Now figure out what to send to the child. */
	if ((event->type == GDK_KEY_PRESS) && !modifier) {
		handled = FALSE;
		/* Map the key to a sequence name if we can. */
		switch (keyval) {
		case GDK_KEY_BackSpace:
			switch (m_backspace_binding) {
			case VTE_ERASE_ASCII_BACKSPACE:
				normal = g_strdup("");
				normal_length = 1;
				suppress_meta_esc = FALSE;
				break;
			case VTE_ERASE_ASCII_DELETE:
				normal = g_strdup("");
				normal_length = 1;
				suppress_meta_esc = FALSE;
				break;
			case VTE_ERASE_DELETE_SEQUENCE:
                                normal = g_strdup("\e[3~");
                                normal_length = 4;
                                add_modifiers = TRUE;
				suppress_meta_esc = TRUE;
				break;
			case VTE_ERASE_TTY:
				if (m_pty != nullptr &&
				    tcgetattr(vte_pty_get_fd(m_pty), &tio) != -1)
				{
					normal = g_strdup_printf("%c", tio.c_cc[VERASE]);
					normal_length = 1;
				}
				suppress_meta_esc = FALSE;
				break;
			case VTE_ERASE_AUTO:
			default:
#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE '\0'
#endif
				if (m_pty != nullptr &&
				    tcgetattr(vte_pty_get_fd(m_pty), &tio) != -1 &&
				    tio.c_cc[VERASE] != _POSIX_VDISABLE)
				{
					normal = g_strdup_printf("%c", tio.c_cc[VERASE]);
					normal_length = 1;
				}
				else
				{
					normal = g_strdup("");
					normal_length = 1;
					suppress_meta_esc = FALSE;
				}
				suppress_meta_esc = FALSE;
				break;
			}
                        /* Toggle ^H vs ^? if Ctrl is pressed */
                        if (normal_length == 1 && m_modifiers & GDK_CONTROL_MASK) {
                                if (normal[0] == '\010')
                                        normal[0] = '\177';
                                else if (normal[0] == '\177')
                                        normal[0] = '\010';
                        }
			handled = TRUE;
			break;
		case GDK_KEY_KP_Delete:
		case GDK_KEY_Delete:
			switch (m_delete_binding) {
			case VTE_ERASE_ASCII_BACKSPACE:
				normal = g_strdup("\010");
				normal_length = 1;
				break;
			case VTE_ERASE_ASCII_DELETE:
				normal = g_strdup("\177");
				normal_length = 1;
				break;
			case VTE_ERASE_TTY:
				if (m_pty != nullptr &&
				    tcgetattr(vte_pty_get_fd(m_pty), &tio) != -1)
				{
					normal = g_strdup_printf("%c", tio.c_cc[VERASE]);
					normal_length = 1;
				}
				suppress_meta_esc = FALSE;
				break;
			case VTE_ERASE_DELETE_SEQUENCE:
			case VTE_ERASE_AUTO:
			default:
                                normal = g_strdup("\e[3~");
                                normal_length = 4;
                                add_modifiers = TRUE;
				break;
			}
			handled = TRUE;
                        /* FIXMEchpe: why? this overrides the FALSE set above? */
			suppress_meta_esc = TRUE;
			break;
		case GDK_KEY_KP_Insert:
		case GDK_KEY_Insert:
			if (m_modifiers & GDK_SHIFT_MASK) {
				if (m_modifiers & GDK_CONTROL_MASK) {
					vte_terminal_paste_clipboard(m_terminal);
					handled = TRUE;
					suppress_meta_esc = TRUE;
				} else {
					vte_terminal_paste_primary(m_terminal);
					handled = TRUE;
					suppress_meta_esc = TRUE;
				}
			} else if (m_modifiers & GDK_CONTROL_MASK) {
				vte_terminal_copy_clipboard(m_terminal);
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		/* Keypad/motion keys. */
		case GDK_KEY_KP_Up:
		case GDK_KEY_Up:
			if (m_screen == &normal_screen &&
			    m_modifiers & GDK_CONTROL_MASK &&
                            m_modifiers & GDK_SHIFT_MASK) {
				vte_terminal_scroll_lines(m_terminal, -1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Down:
		case GDK_KEY_Down:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_CONTROL_MASK &&
                            m_modifiers & GDK_SHIFT_MASK) {
				vte_terminal_scroll_lines(m_terminal, 1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Page_Up:
		case GDK_KEY_Page_Up:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_SHIFT_MASK) {
				vte_terminal_scroll_pages(m_terminal, -1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Page_Down:
		case GDK_KEY_Page_Down:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_SHIFT_MASK) {
				vte_terminal_scroll_pages(m_terminal, 1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Home:
		case GDK_KEY_Home:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_SHIFT_MASK) {
				vte_terminal_maybe_scroll_to_top(m_terminal);
				scrolled = TRUE;
				handled = TRUE;
			}
			break;
		case GDK_KEY_KP_End:
		case GDK_KEY_End:
			if (m_screen == &m_normal_screen &&
			    m_modifiers & GDK_SHIFT_MASK) {
				vte_terminal_maybe_scroll_to_bottom(m_terminal);
				scrolled = TRUE;
				handled = TRUE;
			}
			break;
		/* Let Shift +/- tweak the font, like XTerm does. */
		case GDK_KEY_KP_Add:
		case GDK_KEY_KP_Subtract:
			if (m_modifiers & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) {
				switch (keyval) {
				case GDK_KEY_KP_Add:
					vte_terminal_emit_increase_font_size(m_terminal);
					handled = TRUE;
					suppress_meta_esc = TRUE;
					break;
				case GDK_KEY_KP_Subtract:
					vte_terminal_emit_decrease_font_size(m_terminal);
					handled = TRUE;
					suppress_meta_esc = TRUE;
					break;
				}
			}
			break;
		default:
			break;
		}
		/* If the above switch statement didn't do the job, try mapping
		 * it to a literal or capability name. */
                if (handled == FALSE) {
			_vte_keymap_map(keyval, m_modifiers,
					m_cursor_mode == VTE_KEYMODE_APPLICATION,
					m_keypad_mode == VTE_KEYMODE_APPLICATION,
					&normal,
					&normal_length);
			/* If we found something this way, suppress
			 * escape-on-meta. */
                        if (normal != NULL && normal_length > 0) {
				suppress_meta_esc = TRUE;
			}
		}

		/* Shall we do this here or earlier?  See bug 375112 and bug 589557 */
		if (m_modifiers & GDK_CONTROL_MASK)
			keyval = translate_ctrlkey(event);

		/* If we didn't manage to do anything, try to salvage a
		 * printable string. */
		if (handled == FALSE && normal == NULL) {

			/* Convert the keyval to a gunichar. */
			keychar = gdk_keyval_to_unicode(keyval);
			normal_length = 0;
			if (keychar != 0) {
				/* Convert the gunichar to a string. */
				normal_length = g_unichar_to_utf8(keychar,
								  keybuf);
				if (normal_length != 0) {
					normal = (char *)g_malloc(normal_length + 1);
					memcpy(normal, keybuf, normal_length);
					normal[normal_length] = '\0';
				} else {
					normal = NULL;
				}
			}
			if ((normal != NULL) &&
			    (m_modifiers & GDK_CONTROL_MASK)) {
				/* Replace characters which have "control"
				 * counterparts with those counterparts. */
				for (i = 0; i < normal_length; i++) {
					if ((((guint8)normal[i]) >= 0x40) &&
					    (((guint8)normal[i]) <  0x80)) {
						normal[i] &= (~(0x60));
					}
				}
			}
			_VTE_DEBUG_IF (VTE_DEBUG_EVENTS) {
				if (normal) g_printerr(
						"Keypress, modifiers=0x%x, "
						"keyval=0x%x, cooked string=`%s'.\n",
						m_modifiers,
						keyval, normal);
			}
		}
		/* If we got normal characters, send them to the child. */
		if (normal != NULL) {
                        if (add_modifiers) {
                                _vte_keymap_key_add_key_modifiers(keyval,
                                                                  m_modifiers,
                                                                  m_cursor_mode == VTE_KEYMODE_APPLICATION,
                                                                  &normal,
                                                                  &normal_length);
                        }
			if (m_meta_sends_escape &&
			    !suppress_meta_esc &&
			    (normal_length > 0) &&
			    (m_modifiers & VTE_META_MASK)) {
				vte_terminal_feed_child(m_terminal,
							_VTE_CAP_ESC,
							1);
			}
			if (normal_length > 0) {
				vte_terminal_feed_child_using_modes(m_terminal,
								    normal,
								    normal_length);
			}
			g_free(normal);
		}
		/* Keep the cursor on-screen. */
		if (!scrolled && !modifier &&
		    m_scroll_on_keystroke) {
			vte_terminal_maybe_scroll_to_bottom(m_terminal);
		}
		return true;
	}
	return false;
}

bool
VteTerminalPrivate::widget_key_release(GdkEventKey *event)
{
	read_modifiers((GdkEvent*)event);

	if (gtk_widget_get_realized(m_widget) &&
            m_input_enabled &&
            gtk_im_context_filter_keypress(m_im_context, event))
                return true;

        return false;
}

static int
compare_unichar_p(const void *u1p,
                  const void *u2p)
{
        const gunichar u1 = *(gunichar*)u1p;
        const gunichar u2 = *(gunichar*)u2p;
        return u1 < u2 ? -1 : u1 > u2 ? 1 : 0;
}

static const guint8 word_char_by_category[] = {
        [G_UNICODE_CONTROL]             = 2,
        [G_UNICODE_FORMAT]              = 2,
        [G_UNICODE_UNASSIGNED]          = 2,
        [G_UNICODE_PRIVATE_USE]         = 0,
        [G_UNICODE_SURROGATE]           = 2,
        [G_UNICODE_LOWERCASE_LETTER]    = 1,
        [G_UNICODE_MODIFIER_LETTER]     = 1,
        [G_UNICODE_OTHER_LETTER]        = 1,
        [G_UNICODE_TITLECASE_LETTER]    = 1,
        [G_UNICODE_UPPERCASE_LETTER]    = 1,
        [G_UNICODE_SPACING_MARK]        = 0,
        [G_UNICODE_ENCLOSING_MARK]      = 0,
        [G_UNICODE_NON_SPACING_MARK]    = 0,
        [G_UNICODE_DECIMAL_NUMBER]      = 1,
        [G_UNICODE_LETTER_NUMBER]       = 1,
        [G_UNICODE_OTHER_NUMBER]        = 1,
        [G_UNICODE_CONNECT_PUNCTUATION] = 0,
        [G_UNICODE_DASH_PUNCTUATION]    = 0,
        [G_UNICODE_CLOSE_PUNCTUATION]   = 0,
        [G_UNICODE_FINAL_PUNCTUATION]   = 0,
        [G_UNICODE_INITIAL_PUNCTUATION] = 0,
        [G_UNICODE_OTHER_PUNCTUATION]   = 0,
        [G_UNICODE_OPEN_PUNCTUATION]    = 0,
        [G_UNICODE_CURRENCY_SYMBOL]     = 0,
        [G_UNICODE_MODIFIER_SYMBOL]     = 0,
        [G_UNICODE_MATH_SYMBOL]         = 0,
        [G_UNICODE_OTHER_SYMBOL]        = 0,
        [G_UNICODE_LINE_SEPARATOR]      = 2,
        [G_UNICODE_PARAGRAPH_SEPARATOR] = 2,
        [G_UNICODE_SPACE_SEPARATOR]     = 2,
};

/*
 * _vte_terminal_is_word_char:
 * @terminal: a #VteTerminal
 * @c: a candidate Unicode code point
 *
 * Checks if a particular character is considered to be part of a word or not.
 *
 * Returns: %TRUE if the character is considered to be part of a word
 */
gboolean
_vte_terminal_is_word_char(VteTerminal *terminal,
                           gunichar c)
{
        const guint8 v = word_char_by_category[g_unichar_type(c)];

        if (v)
                return v == 1;

        /* Do we have an exception? */
        return bsearch(&c,
                       terminal->pvt->word_char_exceptions,
                       terminal->pvt->word_char_exceptions_len,
                       sizeof(gunichar),
                       compare_unichar_p) != NULL;
}

/* Check if the characters in the two given locations are in the same class
 * (word vs. non-word characters). */
static gboolean
vte_same_class(VteTerminal *terminal, glong acol, glong arow,
	       glong bcol, glong brow)
{
	const VteCell *pcell = NULL;
	gboolean word_char;
	if ((pcell = vte_terminal_find_charcell(terminal, acol, arow)) != NULL && pcell->c != 0) {
		word_char = _vte_terminal_is_word_char(terminal, _vte_unistr_get_base (pcell->c));

		/* Lets not group non-wordchars together (bug #25290) */
		if (!word_char)
			return FALSE;

		pcell = vte_terminal_find_charcell(terminal, bcol, brow);
		if (pcell == NULL || pcell->c == 0) {
			return FALSE;
		}
		if (word_char != _vte_terminal_is_word_char(terminal, _vte_unistr_get_base (pcell->c))) {
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

/* Check if we soft-wrapped on the given line. */
static gboolean
vte_line_is_wrappable(VteTerminal *terminal, glong row)
{
	const VteRowData *rowdata;
	rowdata = _vte_terminal_find_row_data(terminal, row);
	return rowdata && rowdata->attr.soft_wrapped;
}

/* Check if the given point is in the region between the two points */
static gboolean
vte_cell_is_between(glong col, glong row,
		    glong acol, glong arow, glong bcol, glong brow)
{
	/* Negative between never allowed. */
	if ((arow > brow) || ((arow == brow) && (acol > bcol))) {
		return FALSE;
	}
	/* Degenerate span? */
	if ((row == arow) && (row == brow) && (col == acol) && (col == bcol)) {
		return TRUE;
	}
	/* A cell is between two points if it's on a line after the
	 * specified area starts, or before the line where it ends,
	 * or any of the lines in between. */
	if ((row > arow) && (row < brow)) {
		return TRUE;
	}
	/* It's also between the two points if they're on the same row
	 * the cell lies between the start and end columns. */
	if ((row == arow) && (row == brow)) {
		if (col >= acol) {
			if (col < bcol) {
				return TRUE;
			} else {
				if (col == bcol) {
					return TRUE;
				} else {
					return FALSE;
				}
			}
		} else {
			return FALSE;
		}
	}
	/* It's also "between" if it's on the line where the area starts and
	 * at or after the start column, or on the line where the area ends and
	 * before the end column. */
	if ((row == arow) && (col >= acol)) {
		return TRUE;
	} else {
		if (row == brow) {
			if (col < bcol) {
				return TRUE;
			} else {
				if (col == bcol) {
					return TRUE;
				} else {
					return FALSE;
				}
			}
		} else {
			return FALSE;
		}
	}
	return FALSE;
}

/* Check if a cell is selected or not. */
static gboolean
vte_cell_is_selected(VteTerminal *terminal, glong col, glong row, gpointer data)
{
	VteVisualPosition ss, se;

	/* If there's nothing selected, it's an easy question to answer. */
	if (!terminal->pvt->has_selection) {
		return FALSE;
	}

	/* If the selection is obviously bogus, then it's also very easy. */
	ss = terminal->pvt->selection_start;
	se = terminal->pvt->selection_end;
	if ((ss.row < 0) || (se.row < 0)) {
		return FALSE;
	}

	/* Limit selection in block mode. */
	if (terminal->pvt->selection_block_mode) {
		if (col < ss.col || col > se.col) {
			return FALSE;
		}
	}

	/* Now it boils down to whether or not the point is between the
	 * begin and endpoint of the selection. */
	return vte_cell_is_between(col, row, ss.col, ss.row, se.col, se.row);
}

/* Once we get text data, actually paste it in. */
static void
vte_terminal_paste_cb(GtkClipboard *clipboard, const gchar *text, gpointer data)
{
	VteTerminal *terminal = (VteTerminal *)data;
	gchar *paste, *p;
        gsize run;
        unsigned char c;

	if (text != NULL) {
		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Pasting %" G_GSIZE_FORMAT " UTF-8 bytes.\n",
				strlen(text));
		if (!g_utf8_validate(text, -1, NULL)) {
			g_warning(_("Error (%s) converting data for child, dropping."), g_strerror(EINVAL));
			return;
		}

		/* Convert newlines to carriage returns, which more software
                 * is able to cope with (cough, pico, cough).
                 * Filter out control chars except ^H, ^I, ^J, ^M and ^? (as per xterm).
                 * Also filter out C1 controls: U+0080 (0xC2 0x80) - U+009F (0xC2 0x9F). */
                p = paste = (gchar *) g_malloc(strlen(text));
                while (p != NULL && text[0] != '\0') {
                        run = strcspn(text, "\x01\x02\x03\x04\x05\x06\x07"
                                            "\x0A\x0B\x0C\x0E\x0F"
                                            "\x10\x11\x12\x13\x14\x15\x16\x17"
                                            "\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\xC2");
                        memcpy(p, text, run);
                        p += run;
                        text += run;
                        switch (text[0]) {
                        case '\x00':
                                break;
                        case '\x0A':
                                *p = '\x0D';
                                p++;
                                text++;
                                break;
                        case '\xC2':
                                c = text[1];
                                if (c >= 0x80 && c <= 0x9F) {
                                        /* Skip both bytes of a C1 */
                                        text += 2;
                                } else {
                                        /* Move along, nothing to see here */
                                        *p = '\xC2';
                                        p++;
                                        text++;
                                }
                                break;
                        default:
                                /* Swallow this byte */
                                text++;
                                break;
			}
		}
		if (terminal->pvt->bracketed_paste_mode)
			vte_terminal_feed_child(terminal, "\e[200~", -1);
                vte_terminal_feed_child(terminal, paste, p - paste);
		if (terminal->pvt->bracketed_paste_mode)
			vte_terminal_feed_child(terminal, "\e[201~", -1);
		g_free(paste);
	}
}

/*
 * _vte_terminal_size_to_grid_size:
 * @w: the width in px
 * @h: the height in px
 * @col: return location to store the column count
 * @row: return location to store the row count
 *
 * Translates from widget size to grid size.
 *
 * If the given width or height are insufficient to show even
 * one column or row (i.e due to padding), returns %FALSE.
 */
gboolean
_vte_terminal_size_to_grid_size(VteTerminal *terminal,
                                long w,
                                long h,
                                long *cols,
                                long *rows)
{
        VteTerminalPrivate *pvt = terminal->pvt;
        long n_cols, n_rows;

        n_cols = (w - pvt->padding.left - pvt->padding.right) / pvt->char_width;
        n_rows = (h - pvt->padding.top -pvt->padding.bottom) / pvt->char_height;

        if (n_cols <= 0 || n_rows <= 0)
                return FALSE;

        *cols = n_cols;
        *rows = n_rows;
        return TRUE;
}

void
VteTerminalPrivate::feed_mouse_event(int button,
                                     bool is_drag,
                                     bool is_release,
                                     vte::grid::column_t col,
                                     vte::grid::row_t row)
{
	unsigned char cb = 0;
	long cx, cy;
	char buf[LINE_MAX];
	gint len = 0;

	/* Encode the button information in cb. */
	switch (button) {
        case 0:                 /* No button, just dragging. */
                cb = 3;
                break;
	case 1:			/* Left. */
		cb = 0;
		break;
	case 2:			/* Middle. */
		cb = 1;
		break;
	case 3:			/* Right. */
		cb = 2;
		break;
	case 4:
		cb = 64;	/* Scroll up. */
		break;
	case 5:
		cb = 65;	/* Scroll down. */
		break;
	}

	/* With the exception of the 1006 mode, button release is also encoded here. */
	/* Note that if multiple extensions are enabled, the 1006 is used, so it's okay to check for only that. */
	if (is_release && !m_mouse_xterm_extension) {
		cb = 3;
	}

	/* Encode the modifiers. */
	if (m_modifiers & GDK_SHIFT_MASK) {
		cb |= 4;
	}
	if (m_modifiers & VTE_META_MASK) {
		cb |= 8;
	}
	if (m_modifiers & GDK_CONTROL_MASK) {
		cb |= 16;
	}

	/* Encode a drag event. */
	if (is_drag) {
		cb |= 32;
	}

	/* Make coordinates 1-based. */
	cx = col + 1;
	cy = row + 1;

	/* Check the extensions in decreasing order of preference. Encoding the release event above assumes that 1006 comes first. */
	if (m_mouse_xterm_extension) {
		/* xterm's extended mode (1006) */
		len = g_snprintf(buf, sizeof(buf), _VTE_CAP_CSI "<%d;%ld;%ld%c", cb, cx, cy, is_release ? 'm' : 'M');
	} else if (m_mouse_urxvt_extension) {
		/* urxvt's extended mode (1015) */
		len = g_snprintf(buf, sizeof(buf), _VTE_CAP_CSI "%d;%ld;%ldM", 32 + cb, cx, cy);
	} else if (cx <= 231 && cy <= 231) {
		/* legacy mode */
		len = g_snprintf(buf, sizeof(buf), _VTE_CAP_CSI "M%c%c%c", 32 + cb, 32 + (guchar)cx, 32 + (guchar)cy);
	}

	/* Send event direct to the child, this is binary not text data */
	vte_terminal_feed_child_binary(m_terminal, (guint8*) buf, len);
}

void
VteTerminalPrivate::send_mouse_button_internal(int button,
                                               bool is_release,
                                               long x,
                                               long y)
{
        long col, row;

        if (!mouse_pixels_to_grid (x - m_padding.left,
                                   y - m_padding.top,
                                   &col, &row))
                return;

	feed_mouse_event(button, false /* not drag */, is_release, col, row);
}

void
VteTerminalPrivate::feed_focus_event(bool in)
{
        char buf[8];
        gsize len;

        len = g_snprintf(buf, sizeof(buf), _VTE_CAP_CSI "%c", in ? 'I' : 'O');
        vte_terminal_feed_child_binary(m_terminal, (guint8 *)buf, len);
}

void
VteTerminalPrivate::maybe_feed_focus_event(bool in)
{
        if (m_focus_tracking_mode)
                feed_focus_event(in);
}

/*
 * vte_terminal_maybe_send_mouse_button:
 * @terminal:
 * @event:
 *
 * Sends a mouse button click or release notification to the application,
 * if the terminal is in mouse tracking mode.
 *
 * Returns: %TRUE iff the event was consumed
 */
bool
VteTerminalPrivate::maybe_send_mouse_button(GdkEventButton *event)
{
	read_modifiers((GdkEvent*)event);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (m_mouse_tracking_mode < MOUSE_TRACKING_SEND_XY_ON_CLICK) {
			return false;
		}
		break;
	case GDK_BUTTON_RELEASE: {
		if (m_mouse_tracking_mode < MOUSE_TRACKING_SEND_XY_ON_BUTTON) {
			return false;
		}
		break;
	}
	default:
		return false;
		break;
	}

	send_mouse_button_internal(
						event->button,
						event->type == GDK_BUTTON_RELEASE,
						event->x, event->y);
	return true;
}

/*
 * VteTerminalPrivate::maybe_send_mouse_drag:
 * @terminal:
 * @event:
 *
 * Sends a mouse motion notification to the application,
 * if the terminal is in mouse tracking mode.
 *
 * Returns: %TRUE iff the event was consumed
 */
bool
VteTerminalPrivate::maybe_send_mouse_drag(GdkEventMotion *event)
{
        long col, row;
        int button;

        if (!mouse_pixels_to_grid ((long) event->x - m_padding.left,
                                   (long) event->y - m_padding.top,
                                                 &col, &row))
                return false;

	/* First determine if we even want to send notification. */
	switch (event->type) {
	case GDK_MOTION_NOTIFY:
		if (m_mouse_tracking_mode < MOUSE_TRACKING_CELL_MOTION_TRACKING)
			return false;

		if (m_mouse_tracking_mode < MOUSE_TRACKING_ALL_MOTION_TRACKING) {

                        if (m_mouse_pressed_buttons == 0) {
				return false;
			}
			/* the xterm doc is not clear as to whether
			 * all-tracking also sends degenerate same-cell events */
                        if (col == m_mouse_last_column &&
                            row == m_mouse_last_row)
				return false;
		}
		break;
	default:
		return false;
		break;
	}

        /* As per xterm, report the leftmost pressed button - if any. */
        if (m_mouse_pressed_buttons & 1)
                button = 1;
        else if (m_mouse_pressed_buttons & 2)
                button = 2;
        else if (m_mouse_pressed_buttons & 4)
                button = 3;
        else
                button = 0;

        feed_mouse_event(button,
                         true /* drag */, false /* not release */,
                         col, row);
	return true;
}

/* Clear all match hilites. */
void
VteTerminalPrivate::match_hilite_clear()
{
	auto srow = m_match_start.row;
	auto scolumn = m_match_start.col;
	auto erow = m_match_end.row;
	auto ecolumn = m_match_end.col;
	m_match_start.row = -1;
	m_match_start.col = -1;
	m_match_end.row = -2;
	m_match_end.col = -2;
	if (m_match_tag != -1) {
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Clearing hilite (%ld,%ld) to (%ld,%ld).\n",
				srow, scolumn, erow, ecolumn);
		invalidate_region(
				scolumn, ecolumn, srow, erow, false);
		m_match_tag = -1;
	}
	m_show_match = FALSE;
	if (m_match != nullptr) {
		g_free (m_match);
		m_match = nullptr;
	}
}

bool
VteTerminalPrivate::cursor_inside_match(long x,
                                        long y)
{
	glong col = x / m_char_width;
	glong row = _vte_terminal_pixel_to_row(m_terminal, y);

        return rowcol_inside_match(m_terminal, row, col);
}

void
VteTerminalPrivate::invalidate_match()
{
        invalidate_region(m_match_start.col,
                          m_match_end.col,
                          m_match_start.row,
                          m_match_end.row,
                          false);
}

void
VteTerminalPrivate::match_hilite_show(long x,
                                      long y)
{
	if(m_match != nullptr && !m_show_match){
		if (cursor_inside_match (x, y)) {
                        invalidate_match();
			m_show_match = TRUE;
		}
	}
}

void
VteTerminalPrivate::match_hilite_hide()
{
	if(m_match != nullptr && m_show_match){
                invalidate_match();
		m_show_match = FALSE;
	}
}

void
VteTerminalPrivate::match_hilite_update(long x,
                                        long y)
{
	gsize start, end;
	char *new_match;
	struct _VteCharAttributes *attr;

	/* Check for matches. */

	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Match hilite update (%ld, %ld) -> %ld, %ld\n",
			x, y,
                         x / m_char_width,
                         _vte_terminal_pixel_to_row(m_terminal, y));

	new_match = match_check_internal(
                                                  x / m_char_width,
                                                  _vte_terminal_pixel_to_row(m_terminal, y),
						  &m_match_tag,
						  &start,
						  &end);
	if (m_show_match) {
		/* Repaint what used to be hilited, if anything. */
                invalidate_match();
	}

	/* Read the new locations. */
	attr = NULL;
	if (start < m_match_attributes->len) {
		attr = &g_array_index(m_match_attributes,
				struct _VteCharAttributes,
				start);
		m_match_start.row = attr->row;
		m_match_start.col = attr->column;

		attr = NULL;
		if (end < m_match_attributes->len) {
			attr = &g_array_index(m_match_attributes,
					struct _VteCharAttributes,
					end);
			m_match_end.row = attr->row;
			m_match_end.col = attr->column;
		}
	}
	if (attr == NULL) { /* i.e. if either endpoint is not found */
		m_match_start.row = -1;
		m_match_start.col = -1;
		m_match_end.row = -2;
		m_match_end.col = -2;
		g_assert (m_match == nullptr);// FIXMEchpe this looks bogus. call match_hilite_clear() instead?
	}

	g_free (m_match);
	m_match = new_match;

	/* If there are no matches, repaint what we had matched before. */
	if (m_match == nullptr) {
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"No matches. [(%ld,%ld) to (%ld,%ld)]\n",
				m_match_start.col,
				m_match_start.row,
				m_match_end.col,
				m_match_end.row);
		m_show_match = false;
	} else {
		m_show_match = true;
		/* Repaint the newly-hilited area. */
                invalidate_match();
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Matched (%ld,%ld) to (%ld,%ld).\n",
				m_match_start.col,
				m_match_start.row,
				m_match_end.col,
				m_match_end.row);
	}
}

/* Update the hilited text if the pointer has moved to a new character cell. */
void
VteTerminalPrivate::match_hilite(long x,
                                 long y)
{
	GtkAllocation allocation;

	gtk_widget_get_allocation(m_widget, &allocation);

	/* if the cursor is not above a cell, skip */
	if (x < 0 || x > allocation.width
			|| y < 0 || y > allocation.height) {
		return;
	}

	/* If the pointer hasn't moved to another character cell, then we
	 * need do nothing. Note: Don't use mouse_last_col as that's relative
	 * to insert_delta, and we care about the absolute row number. */
	if (x / m_char_width  == m_mouse_last_x / m_char_width &&
	    _vte_terminal_pixel_to_row(m_terminal, y) == _vte_terminal_pixel_to_row(m_terminal, m_mouse_last_y)) {
		m_show_match = m_match != nullptr;
		return;
	}

	if (cursor_inside_match(x, y)) {
		m_show_match = m_match != nullptr;
		return;
	}

	match_hilite_update(x, y);
}

static GtkClipboard *
vte_terminal_clipboard_get(VteTerminal *terminal, GdkAtom board)
{
	GdkDisplay *display;
	display = gtk_widget_get_display(&terminal->widget);
	return gtk_clipboard_get_for_display(display, board);
}

/* Note that the clipboard has cleared. */
static void
vte_terminal_clear_cb(GtkClipboard *clipboard, gpointer owner)
{
	VteTerminal *terminal = (VteTerminal *)owner;

	if (clipboard == vte_terminal_clipboard_get(terminal, GDK_SELECTION_PRIMARY)) {
		if (terminal->pvt->has_selection) {
			_vte_debug_print(VTE_DEBUG_SELECTION, "Lost selection.\n");
			terminal->pvt->deselect_all();
		}
	}
}

/* Supply the selected text to the clipboard. */
static void
vte_terminal_copy_cb(GtkClipboard *clipboard, GtkSelectionData *data,
		     guint info, gpointer owner)
{
	VteTerminal *terminal = (VteTerminal *)owner;
	int sel;

	for (sel = 0; sel < LAST_VTE_SELECTION; sel++) {
		if (clipboard == terminal->pvt->clipboard[sel] && terminal->pvt->selection_text[sel] != NULL) {
			_VTE_DEBUG_IF(VTE_DEBUG_SELECTION) {
				int i;
				g_printerr("Setting selection %d (%" G_GSIZE_FORMAT " UTF-8 bytes.)\n",
					sel,
					strlen(terminal->pvt->selection_text[sel]));
				for (i = 0; terminal->pvt->selection_text[sel][i] != '\0'; i++) {
					g_printerr("0x%04x\n",
						terminal->pvt->selection_text[sel][i]);
				}
			}
			if (info == VTE_TARGET_TEXT) {
				gtk_selection_data_set_text(data, terminal->pvt->selection_text[sel], -1);
			} else if (info == VTE_TARGET_HTML) {
#ifdef HTML_SELECTION
				gsize len;
				gchar *selection;

				/* Mozilla asks that we start our text/html with the Unicode byte order mark */
				/* (Comment found in gtkimhtml.c of pidgin fame) */
				selection = g_convert(terminal->pvt->selection_html[sel],
					-1, "UTF-16", "UTF-8", NULL, &len, NULL);
				gtk_selection_data_set(data,
					gdk_atom_intern("text/html", FALSE),
					16,
					(const guchar *)selection,
					len);
				g_free(selection);
#endif
			} else {
                                /* Not reached */
                        }
		}
	}
}

/* Convert the internal color code (either index or RGB, see vte-private.h) into RGB. */
static void
vte_terminal_get_rgb_from_index(const VteTerminal *terminal, guint index, PangoColor *color)
{
        gboolean dim = FALSE;
        if (!(index & VTE_RGB_COLOR) && (index & VTE_DIM_COLOR)) {
                index &= ~VTE_DIM_COLOR;
                dim = TRUE;
        }

	if (index >= VTE_LEGACY_COLORS_OFFSET && index < VTE_LEGACY_COLORS_OFFSET + VTE_LEGACY_FULL_COLOR_SET_SIZE)
		index -= VTE_LEGACY_COLORS_OFFSET;
	if (index < VTE_PALETTE_SIZE) {
		memcpy(color, _vte_terminal_get_color(terminal, index), sizeof(PangoColor));
                if (dim) {
                        /* magic formula taken from xterm */
                        color->red = color->red * 2 / 3;
                        color->green = color->green * 2 / 3;
                        color->blue = color->blue * 2 / 3;
                }
	} else if (index & VTE_RGB_COLOR) {
		color->red = ((index >> 16) & 0xFF) * 257;
		color->green = ((index >> 8) & 0xFF) * 257;
		color->blue = (index & 0xFF) * 257;
	} else {
		g_assert_not_reached();
	}
}

char *
_vte_terminal_get_text_range_full(VteTerminal *terminal,
                                 glong start_row, glong start_col,
                                 glong end_row, glong end_col,
                                 VteSelectionFunc is_selected,
                                 gpointer user_data,
                                 GArray *attributes,
                                 gsize *ret_len)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return _vte_terminal_get_text_range_maybe_wrapped(terminal,
							 start_row, start_col,
							 end_row, end_col,
							 TRUE,
							 is_selected,
							 user_data,
							 attributes,
							 FALSE,
                                                         ret_len);
}

char *
_vte_terminal_get_text_range_maybe_wrapped(VteTerminal *terminal,
					  glong start_row, glong start_col,
					  glong end_row, glong end_col,
					  gboolean wrap,
					  VteSelectionFunc is_selected,
					  gpointer data,
					  GArray *attributes,
					  gboolean include_trailing_spaces,
                                          gsize *ret_len)
{
	glong col, row, last_empty, last_emptycol, last_nonempty, last_nonemptycol;
	const VteCell *pcell = NULL;
	GString *string;
	struct _VteCharAttributes attr;
	PangoColor fore, back;

	if (!is_selected)
		is_selected = always_selected;

	if (attributes)
		g_array_set_size (attributes, 0);

	string = g_string_new(NULL);
	memset(&attr, 0, sizeof(attr));

	col = start_col;
	for (row = start_row; row < end_row + 1; row++, col = 0) {
		const VteRowData *row_data = _vte_terminal_find_row_data (terminal, row);
		last_empty = last_nonempty = string->len;
		last_emptycol = last_nonemptycol = -1;

		attr.row = row;
		attr.column = col;
		pcell = NULL;
		if (row_data != NULL) {
			while ((pcell = _vte_row_data_get (row_data, col))) {

				attr.column = col;

				/* If it's not part of a multi-column character,
				 * and passes the selection criterion, add it to
				 * the selection. */
				if (!pcell->attr.fragment && is_selected(terminal, col, row, data)) {
					/* Store the attributes of this character. */
					vte_terminal_get_rgb_from_index(terminal, pcell->attr.fore, &fore);
					vte_terminal_get_rgb_from_index(terminal, pcell->attr.back, &back);
					attr.fore.red = fore.red;
					attr.fore.green = fore.green;
					attr.fore.blue = fore.blue;
					attr.back.red = back.red;
					attr.back.green = back.green;
					attr.back.blue = back.blue;
					attr.underline = pcell->attr.underline;
					attr.strikethrough = pcell->attr.strikethrough;

					/* Store the cell string */
					if (pcell->c == 0) {
						g_string_append_c (string, ' ');
						last_empty = string->len;
						last_emptycol = col;
					} else {
						_vte_unistr_append_to_string (pcell->c, string);
						last_nonempty = string->len;
						last_nonemptycol = col;
					}

					/* If we added text to the string, record its
					 * attributes, one per byte. */
					if (attributes) {
						vte_g_array_fill(attributes,
								&attr, string->len);
					}
				}
				/* If we're on the last line, and have just looked in
				 * the last column, stop. */
				if ((row == end_row) && (col >= end_col)) {
					break;
				}

				col++;
			}
		}

	       /* If the last thing we saw was a empty, and we stopped at the
		* right edge of the selected area, trim the trailing spaces
		* off of the line. */
		if (!include_trailing_spaces && last_empty > last_nonempty) {

			col = last_emptycol + 1;

			if (row_data != NULL) {
				while ((pcell = _vte_row_data_get (row_data, col))) {
					col++;

					if (pcell->attr.fragment)
						continue;

					if (pcell->c != 0)
						break;
				}
			}
			if (pcell == NULL) {
				g_string_truncate(string, last_nonempty);
				if (attributes)
					g_array_set_size(attributes, string->len);
				attr.column = last_nonemptycol;
			}
		}

		/* Adjust column, in case we want to append a newline */
		attr.column = MAX(terminal->pvt->column_count, attr.column + 1);

		/* Add a newline in block mode. */
		if (terminal->pvt->selection_block_mode) {
			string = g_string_append_c(string, '\n');
		}
		/* Else, if the last visible column on this line was selected and
		 * not soft-wrapped, append a newline. */
		else if (is_selected(terminal, terminal->pvt->column_count, row, data)) {
			/* If we didn't softwrap, add a newline. */
			/* XXX need to clear row->soft_wrap on deletion! */
			if (!vte_line_is_wrappable(terminal, row)) {
				string = g_string_append_c(string, '\n');
			}
		}

		/* Make sure that the attributes array is as long as the string. */
		if (attributes) {
			vte_g_array_fill (attributes, &attr, string->len);
		}
	}
	/* Sanity check. */
	g_assert(attributes == NULL || string->len == attributes->len);
        if (ret_len)
                *ret_len = string->len;
	return g_string_free(string, FALSE);
}

char *
_vte_terminal_get_text_maybe_wrapped(VteTerminal *terminal,
				    gboolean wrap,
				    VteSelectionFunc is_selected,
				    gpointer data,
				    GArray *attributes,
				    gboolean include_trailing_spaces,
                                    gsize *ret_len)
{
	long start_row, start_col, end_row, end_col;
	start_row = terminal->pvt->screen->scroll_delta;
	start_col = 0;
	end_row = start_row + terminal->pvt->row_count - 1;
	end_col = terminal->pvt->column_count - 1;
	return _vte_terminal_get_text_range_maybe_wrapped(terminal,
							 start_row, start_col,
							 end_row, end_col,
							 wrap,
							 is_selected,
							 data,
							 attributes,
							 include_trailing_spaces,
                                                         ret_len);
}

/*
 * Compares the visual attributes of a VteCellAttr for equality, but ignores
 * attributes that tend to change from character to character or are otherwise
 * strange (in particular: fragment, columns).
 */
static gboolean
vte_terminal_cellattr_equal(const VteCellAttr *attr1,
                            const VteCellAttr *attr2)
{
	return (attr1->bold          == attr2->bold      &&
	        attr1->fore          == attr2->fore      &&
	        attr1->back          == attr2->back      &&
	        attr1->underline     == attr2->underline &&
	        attr1->strikethrough == attr2->strikethrough &&
	        attr1->reverse       == attr2->reverse   &&
	        attr1->blink         == attr2->blink     &&
	        attr1->invisible     == attr2->invisible);
}

/*
 * Wraps a given string according to the VteCellAttr in HTML tags. Used
 * old-style HTML (and not CSS) for better compatibility with, for example,
 * evolution's mail editor component.
 */
static gchar *
vte_terminal_cellattr_to_html(VteTerminal *terminal,
                              const VteCellAttr *attr,
                              const gchar *text)
{
	GString *string;
	guint fore, back;

	string = g_string_new(text);

	vte_terminal_determine_colors_internal (terminal, attr,
					        FALSE, FALSE,
						&fore, &back);

	if (attr->bold) {
		g_string_prepend(string, "<b>");
		g_string_append(string, "</b>");
	}
	if (attr->fore != VTE_DEFAULT_FG || attr->reverse) {
		PangoColor color;
                char *tag;

                vte_terminal_get_rgb_from_index(terminal, attr->fore, &color);
		tag = g_strdup_printf("<font color=\"#%02X%02X%02X\">",
                                      color.red >> 8,
                                      color.green >> 8,
                                      color.blue >> 8);
		g_string_prepend(string, tag);
		g_free(tag);
		g_string_append(string, "</font>");
	}
	if (attr->back != VTE_DEFAULT_BG || attr->reverse) {
		PangoColor color;
                char *tag;

                vte_terminal_get_rgb_from_index(terminal, attr->back, &color);
		tag = g_strdup_printf("<span style=\"background-color:#%02X%02X%02X\">",
                                      color.red >> 8,
                                      color.green >> 8,
                                      color.blue >> 8);
		g_string_prepend(string, tag);
		g_free(tag);
		g_string_append(string, "</span>");
	}
	if (attr->underline) {
		g_string_prepend(string, "<u>");
		g_string_append(string, "</u>");
	}
	if (attr->strikethrough) {
		g_string_prepend(string, "<strike>");
		g_string_append(string, "</strike>");
	}
	if (attr->blink) {
		g_string_prepend(string, "<blink>");
		g_string_append(string, "</blink>");
	}
	/* reverse and invisible are not supported */

	return g_string_free(string, FALSE);
}

/*
 * Similar to vte_terminal_find_charcell, but takes a VteCharAttribute for
 * indexing and returns the VteCellAttr.
 */
static const VteCellAttr *
vte_terminal_char_to_cell_attr(VteTerminal *terminal,
                               VteCharAttributes *attr)
{
	const VteCell *cell;

	cell = vte_terminal_find_charcell(terminal, attr->column, attr->row);
	if (cell)
		return &cell->attr;
	return NULL;
}


/*
 * _vte_terminal_attributes_to_html:
 * @terminal: a #VteTerminal
 * @text: A string as returned by the vte_terminal_get_* family of functions.
 * @attrs: (array) (element-type Vte.CharAttributes): text attributes, as created by vte_terminal_get_*
 *
 * Marks the given text up according to the given attributes, using HTML <span>
 * commands, and wraps the string in a <pre> element. The attributes have to be
 * "fresh" in the sense that the terminal must not have changed since they were
 * obtained using the vte_terminal_get* function.
 *
 * Returns: (transfer full): a newly allocated text string, or %NULL.
 */
char *
_vte_terminal_attributes_to_html(VteTerminal *terminal,
                                 const gchar *text,
                                 GArray *attrs)
{
	GString *string;
	guint from,to;
	const VteCellAttr *attr;
	char *escaped, *marked;

	g_assert(strlen(text) == attrs->len);

	/* Initial size fits perfectly if the text has no attributes and no
	 * characters that need to be escaped
         */
	string = g_string_sized_new (strlen(text) + 11);
	
	g_string_append(string, "<pre>");
	/* Find streches with equal attributes. Newlines are treated specially,
	 * so that the <span> do not cover multiple lines.
         */
	from = to = 0;
	while (text[from] != '\0') {
		g_assert(from == to);
		if (text[from] == '\n') {
			g_string_append_c(string, '\n');
			from = ++to;
		} else {
			attr = vte_terminal_char_to_cell_attr(terminal,
				&g_array_index(attrs, VteCharAttributes, from));
			while (text[to] != '\0' && text[to] != '\n' &&
			       vte_terminal_cellattr_equal(attr,
					vte_terminal_char_to_cell_attr(terminal,
						&g_array_index(attrs, VteCharAttributes, to))))
			{
				to++;
			}
			escaped = g_markup_escape_text(text + from, to - from);
			marked = vte_terminal_cellattr_to_html(terminal, attr, escaped);
			g_string_append(string, marked);
			g_free(escaped);
			g_free(marked);
			from = to;
		}
	}
	g_string_append(string, "</pre>");

	return g_string_free(string, FALSE);
}

/* Place the selected text onto the clipboard.  Do this asynchronously so that
 * we get notified when the selection we placed on the clipboard is replaced. */
void
VteTerminalPrivate::widget_copy(VteSelection sel)
{
	static GtkTargetEntry *targets = NULL;
	static gint n_targets = 0;
	GArray *attributes;

	attributes = g_array_new(FALSE, TRUE, sizeof(struct _VteCharAttributes));

	/* Chuck old selected text and retrieve the newly-selected text. */
	g_free(m_selection_text[sel]);
	m_selection_text[sel] =
		vte_terminal_get_text_range(m_terminal,
					    m_selection_start.row,
					    0,
					    m_selection_end.row,
					    m_column_count,
					    vte_cell_is_selected,
					    NULL,
					    attributes);
#ifdef HTML_SELECTION
	g_free(m_selection_html[sel]);
	m_selection_html[sel] =
		_vte_terminal_attributes_to_html(m_terminal,
                                                 m_selection_text[sel],
                                                 attributes);
#endif

	g_array_free (attributes, TRUE);

	if (sel == VTE_SELECTION_PRIMARY)
		m_has_selection = TRUE;

	/* Place the text on the clipboard. */
	if (m_selection_text[sel] != NULL) {
		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Assuming ownership of selection.\n");
		if (!targets) {
			GtkTargetList *list;

			list = gtk_target_list_new (NULL, 0);
			gtk_target_list_add_text_targets (list, VTE_TARGET_TEXT);

#ifdef HTML_SELECTION
			gtk_target_list_add (list,
				gdk_atom_intern("text/html", FALSE),
				0,
				VTE_TARGET_HTML);
#endif

                        targets = gtk_target_table_new_from_list (list, &n_targets);
			gtk_target_list_unref (list);
		}

		gtk_clipboard_set_with_owner(m_clipboard[sel],
					     targets,
					     n_targets,
					     vte_terminal_copy_cb,
					     vte_terminal_clear_cb,
					     G_OBJECT(m_terminal));
		gtk_clipboard_set_can_store(m_clipboard[sel], NULL, 0);
	}
}

/* Paste from the given clipboard. */
void
VteTerminalPrivate::widget_paste(GdkAtom board)
{
        if (!m_input_enabled)
                return;

	auto clip = vte_terminal_clipboard_get(m_terminal, board);
	if (clip != nullptr) {
		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Requesting clipboard contents.\n");
		gtk_clipboard_request_text(clip,
					   vte_terminal_paste_cb,
					   m_terminal);
	}
}

static void
vte_terminal_invalidate_selection (VteTerminal *terminal)
{
	_vte_invalidate_region (terminal,
				terminal->pvt->selection_start.col,
				terminal->pvt->selection_end.col,
				terminal->pvt->selection_start.row,
				terminal->pvt->selection_end.row,
				terminal->pvt->selection_block_mode);
}

/* Confine coordinates into the visible area. Padding is already subtracted. */
static void
vte_terminal_confine_coordinates (VteTerminal *terminal, long *xp, long *yp)
{
	long x = *xp;
	long y = *yp;
        long y_stop;

        /* Allow to use the bottom extra padding only if there's content there. */
        y_stop = MIN(_vte_terminal_usable_height_px (terminal),
                     _vte_terminal_row_to_pixel(terminal, terminal->pvt->screen->insert_delta + terminal->pvt->row_count));

	if (y < 0) {
		y = 0;
		if (!terminal->pvt->selection_block_mode)
			x = 0;
        } else if (y >= y_stop) {
                y = y_stop - 1;
		if (!terminal->pvt->selection_block_mode)
			x = terminal->pvt->column_count * terminal->pvt->char_width - 1;
	}
	if (x < 0) {
		x = 0;
	} else if (x >= terminal->pvt->column_count * terminal->pvt->char_width) {
		x = terminal->pvt->column_count * terminal->pvt->char_width - 1;
	}

	*xp = x;
	*yp = y;
}

/* Start selection at the location of the event. */
static void
vte_terminal_start_selection(VteTerminal *terminal, long x, long y,
			     enum vte_selection_type selection_type)
{
	if (terminal->pvt->selection_block_mode)
		selection_type = selection_type_char;

	/* Confine coordinates into the visible area. (#563024, #722635c7) */
	vte_terminal_confine_coordinates(terminal, &x, &y);

	/* Record that we have the selection, and where it started. */
	terminal->pvt->has_selection = TRUE;
	terminal->pvt->selection_last.x = x;
	terminal->pvt->selection_last.y = _vte_terminal_scroll_delta_pixel(terminal) + y;

	/* Decide whether or not to restart on the next drag. */
	switch (selection_type) {
	case selection_type_char:
		/* Restart selection once we register a drag. */
		terminal->pvt->selecting_restart = TRUE;
		terminal->pvt->has_selection = FALSE;
		terminal->pvt->selecting_had_delta = FALSE;

		terminal->pvt->selection_origin = terminal->pvt->selection_last;
		break;
	case selection_type_word:
	case selection_type_line:
		/* Mark the newly-selected areas now. */
		terminal->pvt->selecting_restart = FALSE;
		terminal->pvt->has_selection = FALSE;
		terminal->pvt->selecting_had_delta = FALSE;
		break;
	}

	/* Record the selection type. */
	terminal->pvt->selection_type = selection_type;
	terminal->pvt->selecting = TRUE;
	terminal->pvt->selecting_after_threshold = FALSE;

	_vte_debug_print(VTE_DEBUG_SELECTION,
			"Selection started at (%ld,%ld).\n",
			terminal->pvt->selection_start.col,
			terminal->pvt->selection_start.row);

        /* Take care of updating the display. */
        vte_terminal_extend_selection(terminal, x, y, FALSE, TRUE);

	/* Temporarily stop caring about input from the child. */
	_vte_terminal_disconnect_pty_read(terminal);
}

static gboolean
_vte_terminal_maybe_end_selection (VteTerminal *terminal)
{
	if (terminal->pvt->selecting) {
		/* Copy only if something was selected. */
		if (terminal->pvt->has_selection &&
		    !terminal->pvt->selecting_restart &&
		    terminal->pvt->selecting_had_delta) {
			vte_terminal_copy_primary(terminal);
			vte_terminal_emit_selection_changed(terminal);
		}
		terminal->pvt->selecting = FALSE;

		/* Reconnect to input from the child if we paused it. */
		_vte_terminal_connect_pty_read(terminal);

		return TRUE;
	}

        if (terminal->pvt->selecting_after_threshold)
                return TRUE;

        return FALSE;
}

static long
math_div (long a, long b)
{
	if (G_LIKELY (a >= 0))
		return a / b;
	else
		return (a / b) - 1;
}

/* Helper */
static void
vte_terminal_extend_selection_expand (VteTerminal *terminal)
{
	long i, j;
	VteScreen *screen;
	const VteRowData *rowdata;
	const VteCell *cell;
	VteVisualPosition *sc, *ec;

	if (terminal->pvt->selection_block_mode)
		return;

	screen = terminal->pvt->screen;
	sc = &terminal->pvt->selection_start;
	ec = &terminal->pvt->selection_end;

	/* Extend the selection to handle end-of-line cases, word, and line
	 * selection.  We do this here because calculating it once is cheaper
	 * than recalculating for each cell as we render it. */

	/* Handle end-of-line at the start-cell. */
	rowdata = _vte_terminal_find_row_data(terminal, sc->row);
	if (rowdata != NULL) {
		/* Find the last non-empty character on the first line. */
		for (i = _vte_row_data_length (rowdata); i > 0; i--) {
			cell = _vte_row_data_get (rowdata, i - 1);
			if (cell->attr.fragment || cell->c != 0)
				break;
		}
	} else {
                i = 0;
	}
        if (sc->col > i) {
                if (terminal->pvt->selection_type == selection_type_char) {
                        /* If the start point is neither over the used cells, nor over the first
                         * unused one, then move it to the next line. This way you can still start
                         * selecting at the newline character by clicking over the first unused cell.
                         * See bug 725909. */
                        sc->col = -1;
                        sc->row++;
                } else if (terminal->pvt->selection_type == selection_type_word) {
                        sc->col = i;
                }
        }
        sc->col = find_start_column (terminal, sc->col, sc->row);

	/* Handle end-of-line at the end-cell. */
	rowdata = _vte_terminal_find_row_data(terminal, ec->row);
	if (rowdata != NULL) {
		/* Find the last non-empty character on the last line. */
		for (i = _vte_row_data_length (rowdata); i > 0; i--) {
			cell = _vte_row_data_get (rowdata, i - 1);
			if (cell->attr.fragment || cell->c != 0)
				break;
		}
		/* If the end point is to its right, then extend the
		 * endpoint to the beginning of the next row. */
		if (ec->col >= i) {
			ec->col = -1;
			ec->row++;
		}
	} else {
		/* Snap to the beginning of the next line, only if
		 * selecting anything of this row. */
		if (ec->col >= 0) {
			ec->col = -1;
			ec->row++;
		}
	}
	ec->col = find_end_column (terminal, ec->col, ec->row);


	/* Now extend again based on selection type. */
	switch (terminal->pvt->selection_type) {
	case selection_type_char:
		/* Nothing more to do. */
		break;
	case selection_type_word:
		/* Keep selecting to the left as long as the next character we
		 * look at is of the same class as the current start point. */
		i = sc->col;
		j = sc->row;
		while (_vte_ring_contains (screen->row_data, j)) {
			/* Get the data for the row we're looking at. */
			rowdata = _vte_ring_index(screen->row_data, j);
			if (rowdata == NULL) {
				break;
			}
			/* Back up. */
			for (i = (j == sc->row) ?
				 sc->col :
				 terminal->pvt->column_count;
			     i > 0;
			     i--) {
				if (vte_same_class(terminal,
						   i - 1,
						   j,
						   i,
						   j)) {
					sc->col = i - 1;
					sc->row = j;
				} else {
					break;
				}
			}
			if (i > 0) {
				/* We hit a stopping point, so stop. */
				break;
			} else {
				if (vte_line_is_wrappable(terminal, j - 1) &&
				    vte_same_class(terminal,
						   terminal->pvt->column_count - 1,
						   j - 1,
						   0,
						   j)) {
					/* Move on to the previous line. */
					j--;
					sc->col = terminal->pvt->column_count - 1;
					sc->row = j;
				} else {
					break;
				}
			}
		}
		/* Keep selecting to the right as long as the next character we
		 * look at is of the same class as the current end point. */
		i = ec->col;
		j = ec->row;
		while (_vte_ring_contains (screen->row_data, j)) {
			/* Get the data for the row we're looking at. */
			rowdata = _vte_ring_index(screen->row_data, j);
			if (rowdata == NULL) {
				break;
			}
			/* Move forward. */
			for (i = (j == ec->row) ?
				 ec->col :
				 0;
			     i < terminal->pvt->column_count - 1;
			     i++) {
				if (vte_same_class(terminal,
						   i,
						   j,
						   i + 1,
						   j)) {
					ec->col = i + 1;
					ec->row = j;
				} else {
					break;
				}
			}
			if (i < terminal->pvt->column_count - 1) {
				/* We hit a stopping point, so stop. */
				break;
			} else {
				if (vte_line_is_wrappable(terminal, j) &&
				    vte_same_class(terminal,
						   terminal->pvt->column_count - 1,
						   j,
						   0,
						   j + 1)) {
					/* Move on to the next line. */
					j++;
					ec->col = 0;
					ec->row = j;
				} else {
					break;
				}
			}
		}
		break;
	case selection_type_line:
		/* Extend the selection to the beginning of the start line. */
		sc->col = 0;
		/* Now back up as far as we can go. */
		j = sc->row;
		while (_vte_ring_contains (screen->row_data, j - 1) &&
		       vte_line_is_wrappable(terminal, j - 1)) {
			j--;
			sc->row = j;
		}
		/* And move forward as far as we can go. */
                if (ec->col < 0) {
                        /* If triple clicking on an unused area, ec already points
                         * to the beginning of the next line after the second click.
                         * Go back to the actual row we're at. See bug 725909. */
                        ec->row--;
                }
		j = ec->row;
		while (_vte_ring_contains (screen->row_data, j) &&
		       vte_line_is_wrappable(terminal, j)) {
			j++;
			ec->row = j;
		}
		/* Make sure we include all of the last line by extending
		 * to the beginning of the next line. */
		ec->row++;
		ec->col = -1;
		break;
	}
}

/* Extend selection to include the given event coordinates. */
static void
vte_terminal_extend_selection(VteTerminal *terminal, long x, long y,
			      gboolean always_grow, gboolean force)
{
	int width, height;
	long residual;
	long row;
	struct selection_event_coords *origin, *last, *start, *end;
	VteVisualPosition old_start, old_end, *sc, *ec, *so, *eo;
	gboolean invalidate_selected = FALSE;
	gboolean had_selection;

	height = terminal->pvt->char_height;
	width = terminal->pvt->char_width;

	/* Confine coordinates into the visible area. (#563024, #722635c7) */
	vte_terminal_confine_coordinates(terminal, &x, &y);

	old_start = terminal->pvt->selection_start;
	old_end = terminal->pvt->selection_end;
	so = &old_start;
	eo = &old_end;

	/* If we're restarting on a drag, then mark this as the start of
	 * the selected block. */
	if (terminal->pvt->selecting_restart) {
		terminal->pvt->deselect_all();
		invalidate_selected = TRUE;
		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Selection delayed start at (%ld,%ld).\n",
				terminal->pvt->selection_origin.x / width,
				terminal->pvt->selection_origin.y / height);
	}

	/* Recognize that we've got a selected block. */
	had_selection = terminal->pvt->has_selection;
	terminal->pvt->has_selection = TRUE;
	terminal->pvt->selecting_had_delta = TRUE;
	terminal->pvt->selecting_restart = FALSE;

	/* If we're not in always-grow mode, update the last location of
	 * the selection. */
	last = &terminal->pvt->selection_last;

	/* Map the origin and last selected points to a start and end. */
	origin = &terminal->pvt->selection_origin;
	if (terminal->pvt->selection_block_mode) {
		last->x = x;
		last->y = _vte_terminal_scroll_delta_pixel(terminal) + y;

		/* We don't support always_grow in block mode */
		if (always_grow)
			vte_terminal_invalidate_selection (terminal);

		if (origin->y <= last->y) {
			/* The origin point is "before" the last point. */
			start = origin;
			end = last;
		} else {
			/* The last point is "before" the origin point. */
			start = last;
			end = origin;
		}
	} else {
		if (!always_grow) {
			last->x = x;
			last->y = _vte_terminal_scroll_delta_pixel(terminal) + y;
		}

		if ((origin->y / height < last->y / height) ||
		    ((origin->y / height == last->y / height) &&
		     (origin->x / width < last->x / width ))) {
			/* The origin point is "before" the last point. */
			start = origin;
			end = last;
		} else {
			/* The last point is "before" the origin point. */
			start = last;
			end = origin;
		}

		/* Extend the selection by moving whichever end of the selection is
		 * closer to the new point. */
		if (always_grow) {
			/* New endpoint is before existing selection. */
                        row = _vte_terminal_pixel_to_row(terminal, y);
			if ((row < start->y / height) ||
			    ((row == start->y / height) &&
			     (x / width < start->x / width))) {
				start->x = x;
				start->y = _vte_terminal_scroll_delta_pixel(terminal) + y;
			} else {
				/* New endpoint is after existing selection. */
				end->x = x;
				end->y = _vte_terminal_scroll_delta_pixel(terminal) + y;
			}
		}
	}

#if 0
	_vte_debug_print(VTE_DEBUG_SELECTION,
			"Selection is (%ld,%ld) to (%ld,%ld).\n",
			start->x, start->y, end->x, end->y);
#endif

	/* Recalculate the selection area in terms of cell positions. */

	sc = &terminal->pvt->selection_start;
	ec = &terminal->pvt->selection_end;

	sc->row = MAX (0, start->y / height);
	ec->row = MAX (0, end->y   / height);

	/* Sort x using row cell coordinates */
	if ((terminal->pvt->selection_block_mode || sc->row == ec->row) && (start->x > end->x)) {
		struct selection_event_coords *tmp;
		tmp = start;
		start = end;
		end = tmp;
	}

	/* We want to be more lenient on the user with their column selection.
	 * We round to the closest logical position (positions are located between
	 * cells).  But we don't want to fully round.  So we divide the cell
	 * width into three parts.  The side parts round to their nearest
	 * position.  The middle part is always inclusive in the selection.
	 *
	 * math_div and no MAX, to allow selecting no cells in the line,
	 * ie. ec->col = -1, which is essentially equal to copying the
	 * newline from previous line but no chars from current line. */
	residual = (width + 1) / 3;
	sc->col = math_div (start->x + residual, width);
	ec->col = math_div (end->x - residual, width);


	vte_terminal_extend_selection_expand (terminal);

	if (!invalidate_selected && !force &&
	    0 == memcmp (sc, so, sizeof (*sc)) &&
	    0 == memcmp (ec, eo, sizeof (*ec)))
		/* No change */
		return;

	/* Invalidate */

	if (had_selection) {

		if (terminal->pvt->selection_block_mode) {
			/* Update the selection area diff in block mode. */

			/* The top band */
			_vte_invalidate_region (terminal,
						MIN(sc->col, so->col),
						MAX(ec->col, eo->col),
						MIN(sc->row, so->row),
						MAX(sc->row, so->row) - 1,
						TRUE);
			/* The bottom band */
			_vte_invalidate_region (terminal,
						MIN(sc->col, so->col),
						MAX(ec->col, eo->col),
						MIN(ec->row, eo->row) + 1,
						MAX(ec->row, eo->row),
						TRUE);
			/* The left band */
			_vte_invalidate_region (terminal,
						MIN(sc->col, so->col),
						MAX(sc->col, so->col) - 1 + (VTE_TAB_WIDTH_MAX - 1),
						MIN(sc->row, so->row),
						MAX(ec->row, eo->row),
						TRUE);
			/* The right band */
			_vte_invalidate_region (terminal,
						MIN(ec->col, eo->col) + 1,
						MAX(ec->col, eo->col) + (VTE_TAB_WIDTH_MAX - 1),
						MIN(sc->row, so->row),
						MAX(ec->row, eo->row),
						TRUE);
		} else {
			/* Update the selection area diff in non-block mode. */

			/* The before band */
			if (sc->row < so->row)
				_vte_invalidate_region (terminal,
							sc->col, so->col - 1,
							sc->row, so->row,
							FALSE);
			else if (sc->row > so->row)
				_vte_invalidate_region (terminal,
							so->col, sc->col - 1,
							so->row, sc->row,
							FALSE);
			else
				_vte_invalidate_region (terminal,
							MIN(sc->col, so->col), MAX(sc->col, so->col) - 1,
							sc->row, sc->row,
							TRUE);

			/* The after band */
			if (ec->row < eo->row)
				_vte_invalidate_region (terminal,
							ec->col + 1, eo->col,
							ec->row, eo->row,
							FALSE);
			else if (ec->row > eo->row)
				_vte_invalidate_region (terminal,
							eo->col + 1, ec->col,
							eo->row, ec->row,
							FALSE);
			else
				_vte_invalidate_region (terminal,
							MIN(ec->col, eo->col) + 1, MAX(ec->col, eo->col),
							ec->row, ec->row,
							TRUE);
		}
	}

	if (invalidate_selected || !had_selection) {
		_vte_debug_print(VTE_DEBUG_SELECTION, "Invalidating selection.");
		vte_terminal_invalidate_selection (terminal);
	}

	_vte_debug_print(VTE_DEBUG_SELECTION,
			"Selection changed to "
			"(%ld,%ld) to (%ld,%ld).\n",
			sc->col, sc->row, ec->col, ec->row);
}

/*
 * VteTerminalPrivate::select_all:
 *
 * Selects all text within the terminal (including the scrollback buffer).
 */
void
VteTerminalPrivate::select_all()
{
	deselect_all();

	m_has_selection = TRUE;
	m_selecting_had_delta = TRUE;
	m_selecting_restart = FALSE;

	m_selection_start.row = _vte_ring_delta (m_screen->row_data);
	m_selection_start.col = 0;
	m_selection_end.row = _vte_ring_next (m_screen->row_data);
	m_selection_end.col = -1;

	_vte_debug_print(VTE_DEBUG_SELECTION, "Selecting *all* text.\n");

	vte_terminal_copy_primary(m_terminal);
	vte_terminal_emit_selection_changed(m_terminal);

	invalidate_all();
}

/* Autoscroll a bit. */
static gboolean
vte_terminal_autoscroll(VteTerminal *terminal)
{
	gboolean extend = FALSE;
	long x, y, xmax, ymax;
	glong adj;

	/* Provide an immediate effect for mouse wigglers. */
	if (terminal->pvt->mouse_last_y < 0) {
		if (terminal->pvt->vadjustment) {
			/* Try to scroll up by one line. */
			adj = terminal->pvt->screen->scroll_delta - 1;
			vte_terminal_queue_adjustment_value_changed_clamped (terminal, adj);
			extend = TRUE;
		}
		_vte_debug_print(VTE_DEBUG_EVENTS, "Autoscrolling down.\n");
	}
	if (terminal->pvt->mouse_last_y >= _vte_terminal_usable_height_px (terminal)) {
		if (terminal->pvt->vadjustment) {
			/* Try to scroll up by one line. */
			adj = terminal->pvt->screen->scroll_delta + 1;
			vte_terminal_queue_adjustment_value_changed_clamped (terminal, adj);
			extend = TRUE;
		}
		_vte_debug_print(VTE_DEBUG_EVENTS, "Autoscrolling up.\n");
	}
	if (extend) {
		/* Don't select off-screen areas.  That just confuses people. */
		xmax = terminal->pvt->column_count * terminal->pvt->char_width;
		ymax = terminal->pvt->row_count * terminal->pvt->char_height;

		x = CLAMP(terminal->pvt->mouse_last_x, 0, xmax);
		y = CLAMP(terminal->pvt->mouse_last_y, 0, ymax);
		/* If we clamped the Y, mess with the X to get the entire
		 * lines. */
		if (terminal->pvt->mouse_last_y < 0 && !terminal->pvt->selection_block_mode) {
			x = 0;
		}
		if (terminal->pvt->mouse_last_y >= ymax && !terminal->pvt->selection_block_mode) {
			x = terminal->pvt->column_count * terminal->pvt->char_width;
		}
		/* Extend selection to cover the newly-scrolled area. */
		vte_terminal_extend_selection(terminal, x, y, FALSE, TRUE);
	} else {
		/* Stop autoscrolling. */
		terminal->pvt->mouse_autoscroll_tag = 0;
	}
	return (terminal->pvt->mouse_autoscroll_tag != 0);
}

/* Start autoscroll. */
static void
vte_terminal_start_autoscroll(VteTerminal *terminal)
{
	if (terminal->pvt->mouse_autoscroll_tag == 0) {
		terminal->pvt->mouse_autoscroll_tag =
			g_timeout_add_full(G_PRIORITY_LOW,
					   666 / terminal->pvt->row_count,
					   (GSourceFunc)vte_terminal_autoscroll,
					   terminal,
					   NULL);
	}
}

/* Stop autoscroll. */
static void
vte_terminal_stop_autoscroll(VteTerminal *terminal)
{
	if (terminal->pvt->mouse_autoscroll_tag != 0) {
		g_source_remove(terminal->pvt->mouse_autoscroll_tag);
		terminal->pvt->mouse_autoscroll_tag = 0;
	}
}

bool
VteTerminalPrivate::widget_motion_notify(GdkEventMotion *event)
{
	long x, y;
	bool handled = false;

	/* check to see if it matters */
        // FIXMEchpe this can't happen
        if (G_UNLIKELY(!gtk_widget_get_realized(m_widget)))
                return false;

	x = event->x - m_padding.left;
	y = event->y - m_padding.top;

	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Motion notify (%ld,%ld) [%ld, %ld].\n",
			x, y,
			x / m_char_width,
                        _vte_terminal_pixel_to_row(m_terminal, y));

	read_modifiers((GdkEvent*)event);

        if (m_mouse_pressed_buttons != 0) {
		match_hilite_hide();
	} else {
		/* Hilite any matches. */
		match_hilite(x, y);
		/* Show the cursor. */
		set_pointer_visible(true);
	}

	switch (event->type) {
	case GDK_MOTION_NOTIFY:
		if (m_selecting_after_threshold) {
			if (!gtk_drag_check_threshold (m_widget,
						       m_mouse_last_x,
						       m_mouse_last_y,
						       x, y))
				return true;

			vte_terminal_start_selection(m_terminal,
						     m_mouse_last_x,
						     m_mouse_last_y,
						     selection_type_char);
		}

		if (m_selecting &&
                    (m_mouse_handled_buttons & 1) != 0) {
			_vte_debug_print(VTE_DEBUG_EVENTS, "Mousing drag 1.\n");
			vte_terminal_extend_selection(m_terminal,
						      x, y, FALSE, FALSE);

			/* Start scrolling if we need to. */
			if (event->y < m_padding.top ||
			    event->y >= m_row_count * m_char_height + m_padding.top)
			{
				/* Give mouse wigglers something. */
				vte_terminal_autoscroll(m_terminal);
				/* Start a timed autoscroll if we're not doing it
				 * already. */
				vte_terminal_start_autoscroll(m_terminal);
			}

			handled = true;
		}

		if (!handled && m_input_enabled)
			maybe_send_mouse_drag(event);
		break;
	default:
		break;
	}

	/* Save the pointer coordinates for later use. */
	m_mouse_last_x = x;
	m_mouse_last_y = y;
        mouse_pixels_to_grid (
                                            x, y,
                                            &m_mouse_last_column,
                                            &m_mouse_last_row);

	return handled;
}

bool
VteTerminalPrivate::widget_button_press(GdkEventButton *event)
{
	bool handled = false;
	gboolean start_selecting = FALSE, extend_selecting = FALSE;
	long cellx, celly;
	long x,y;

	x = event->x - m_padding.left;
	y = event->y - m_padding.top;

	match_hilite(x, y);

	set_pointer_visible(true);

	read_modifiers((GdkEvent*)event);

	/* Convert the event coordinates to cell coordinates. */
	cellx = x / m_char_width;
	celly = _vte_terminal_pixel_to_row(m_terminal, y);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Button %d single-click at (%ld,%ld)\n",
				event->button,
				x, _vte_terminal_scroll_delta_pixel(m_terminal) + y);
		/* Handle this event ourselves. */
		switch (event->button) {
		case 1:
			_vte_debug_print(VTE_DEBUG_EVENTS,
					"Handling click ourselves.\n");
			/* Grab focus. */
			if (!gtk_widget_has_focus(m_widget)) {
				gtk_widget_grab_focus(m_widget);
			}

			/* If we're in event mode, and the user held down the
			 * shift key, we start selecting. */
			if (m_mouse_tracking_mode) {
				if (m_modifiers & GDK_SHIFT_MASK) {
					start_selecting = TRUE;
				}
			} else {
				/* If the user hit shift, then extend the
				 * selection instead. */
				if ((m_modifiers & GDK_SHIFT_MASK) &&
				    (m_has_selection ||
				     m_selecting_restart) &&
				    !vte_cell_is_selected(m_terminal,
							  cellx,
							  celly,
							  NULL)) {
					extend_selecting = TRUE;
				} else {
					start_selecting = TRUE;
				}
			}
			if (start_selecting) {
				deselect_all();
				m_selecting_after_threshold = TRUE;
                                m_selection_block_mode = !!(m_modifiers & GDK_CONTROL_MASK);
				handled = true;
			}
			if (extend_selecting) {
				vte_terminal_extend_selection(m_terminal,
							      x, y,
							      !m_selecting_restart, TRUE);
				/* The whole selection code needs to be
				 * rewritten.  For now, put this here to
				 * fix bug 614658 */
				m_selecting = TRUE;
				handled = true;
			}
			break;
		/* Paste if the user pressed shift or we're not sending events
		 * to the app. */
		case 2:
			if ((m_modifiers & GDK_SHIFT_MASK) ||
			    !m_mouse_tracking_mode) {
                                gboolean do_paste;

                                g_object_get (gtk_widget_get_settings(m_widget),
                                              "gtk-enable-primary-paste",
                                              &do_paste, NULL);
                                if (do_paste)
                                        vte_terminal_paste_primary(m_terminal);
				handled = do_paste;
			}
			break;
		case 3:
		default:
			break;
		}
                if (event->button >= 1 && event->button <= 3) {
                        if (handled)
                                m_mouse_handled_buttons |= (1 << (event->button - 1));
                        else
                                m_mouse_handled_buttons &= ~(1 << (event->button - 1));
                }
		/* If we haven't done anything yet, try sending the mouse
		 * event to the app. */
		if (handled == FALSE) {
			handled = maybe_send_mouse_button(event);
		}
		break;
	case GDK_2BUTTON_PRESS:
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Button %d double-click at (%ld,%ld)\n",
				event->button,
				x, _vte_terminal_scroll_delta_pixel(m_terminal) + y);
		switch (event->button) {
		case 1:
			if (m_selecting_after_threshold) {
				vte_terminal_start_selection(m_terminal,
							     x, y,
							     selection_type_char);
				handled = true;
			}
                        if ((mouse_handled_buttons & 1) != 0) {
				vte_terminal_start_selection(m_terminal,
							     x, y,
							     selection_type_word);
				handled = true;
			}
			break;
		case 2:
		case 3:
		default:
			break;
		}
		break;
	case GDK_3BUTTON_PRESS:
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Button %d triple-click at (%ld,%ld).\n",
				event->button,
				x, _vte_terminal_scroll_delta_pixel(m_terminal) + y);
		switch (event->button) {
		case 1:
                        if ((m_mouse_handled_buttons & 1) != 0) {
				vte_terminal_start_selection(m_terminal,
							     x, y,
							     selection_type_line);
				handled = true;
			}
			break;
		case 2:
		case 3:
		default:
			break;
		}
	default:
		break;
	}

	/* Save the pointer state for later use. */
        if (event->button >= 1 && event->button <= 3)
                m_mouse_pressed_buttons |= (1 << (event->button - 1));
	m_mouse_last_x = x;
	m_mouse_last_y = y;
        mouse_pixels_to_grid (
                                            x, y,
                                            &m_mouse_last_column,
                                            &m_mouse_last_row);

	return handled;
}

bool
VteTerminalPrivate::widget_button_release(GdkEventButton *event)
{
	gboolean handled = FALSE;
	int x, y;

	x = event->x - m_padding.left;
	y = event->y - m_padding.top;

	match_hilite(x, y);

	set_pointer_visible(true);

	vte_terminal_stop_autoscroll(m_terminal);

	read_modifiers((GdkEvent*)event);

	switch (event->type) {
	case GDK_BUTTON_RELEASE:
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Button %d released at (%d,%d).\n",
				event->button, x, y);
		switch (event->button) {
		case 1:
                        if ((m_mouse_handled_buttons & 1) != 0)
                                handled = _vte_terminal_maybe_end_selection(m_terminal);
			break;
		case 2:
                        handled = (m_mouse_handled_buttons & 2) != 0;
                        m_mouse_handled_buttons &= ~2;
			break;
		case 3:
		default:
			break;
		}
		if (!handled && m_input_enabled) {
			handled = maybe_send_mouse_button(event);
		}
		break;
	default:
		break;
	}

	/* Save the pointer state for later use. */
        if (event->button >= 1 && event->button <= 3)
                m_mouse_pressed_buttons &= ~(1 << (event->button - 1));
	m_mouse_last_x = x;
	m_mouse_last_y = y;
        mouse_pixels_to_grid (
                                            x, y,
                                            &m_mouse_last_column,
                                            &m_mouse_last_row);
	m_selecting_after_threshold = FALSE;

	return handled;
}

void
VteTerminalPrivate::widget_focus_in(GdkEventFocus *event)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus in.\n");

	gtk_widget_grab_focus(m_widget);

	/* Read the keyboard modifiers, though they're probably garbage. */
	read_modifiers((GdkEvent*)event);

	/* We only have an IM context when we're realized, and there's not much
	 * point to painting the cursor if we don't have a window. */
	if (gtk_widget_get_realized(m_widget)) {
		m_cursor_blink_state = TRUE;
		m_has_focus = TRUE;

		check_cursor_blink();

		gtk_im_context_focus_in(m_im_context);
		invalidate_cursor_once();
		set_pointer_visible(true);
                maybe_feed_focus_event(true);
	}
}

void
VteTerminalPrivate::widget_focus_out(GdkEventFocus *event)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus out.\n");

	/* Read the keyboard modifiers, though they're probably garbage. */
	read_modifiers((GdkEvent*)event);

	/* We only have an IM context when we're realized, and there's not much
	 * point to painting ourselves if we don't have a window. */
	if (gtk_widget_get_realized(m_widget)) {
                maybe_feed_focus_event(false);

		_vte_terminal_maybe_end_selection(m_terminal);

		gtk_im_context_focus_out(m_im_context);
		invalidate_cursor_once();

		/* XXX Do we want to hide the match just because the terminal
		 * lost keyboard focus, but the pointer *is* still within our
		 * area top? */
		match_hilite_hide();
		/* Mark the cursor as invisible to disable hilite updating */
		m_mouse_cursor_visible = FALSE;
                m_mouse_pressed_buttons = 0;
                m_mouse_handled_buttons = 0;
	}

	m_has_focus = false;
	check_cursor_blink();
}

void
VteTerminalPrivate::widget_enter(GdkEventCrossing *event)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Enter.\n");

	if (gtk_widget_get_realized(m_widget)) {
		/* Hilite any matches. */
		match_hilite_show(
					       event->x - m_padding.left,
					       event->y - m_padding.top);
	}
}

void
VteTerminalPrivate::widget_leave(GdkEventCrossing *event)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Leave.\n");

	if (gtk_widget_get_realized(m_widget)) {
		match_hilite_hide();

		/* Mark the cursor as invisible to disable hilite updating,
		 * whilst the cursor is absent (otherwise we copy the entire
		 * buffer after each update for nothing...)
		 */
		m_mouse_cursor_visible = FALSE;
	}
}

static G_GNUC_UNUSED inline const char *
visibility_state_str(GdkVisibilityState state)
{
	switch(state){
		case GDK_VISIBILITY_FULLY_OBSCURED:
			return "fully-obscured";
		case GDK_VISIBILITY_UNOBSCURED:
			return "unobscured";
		default:
			return "partial";
	}
}

void
VteTerminalPrivate::widget_visibility_notify(GdkEventVisibility *event)
{
	_vte_debug_print(VTE_DEBUG_EVENTS | VTE_DEBUG_MISC,
                         "Visibility (%s -> %s).\n",
			visibility_state_str(m_visibility_state),
			visibility_state_str(event->state));

	if (event->state == m_visibility_state) {
		return;
	}

	/* fully obscured to visible switch, force the fast path */
	if (m_visibility_state == GDK_VISIBILITY_FULLY_OBSCURED) {
		/* set invalidated_all false, since we didn't really mean it
		 * when we set it to TRUE when becoming obscured */
		m_invalidated_all = FALSE;

		/* if all unobscured now, invalidate all, otherwise, wait
		 * for the expose event */
		if (event->state == GDK_VISIBILITY_UNOBSCURED) {
			invalidate_all();
		}
	}

	visibility_state = event->state;

	/* no longer visible, stop processing display updates */
	if (m_visibility_state == GDK_VISIBILITY_FULLY_OBSCURED) {
		remove_update_timeout(m_terminal);
		/* if fully obscured, just act like we have invalidated all,
		 * so no updates are accumulated. */
		m_invalidated_all = TRUE;
	}
}

/* Apply the changed metrics, and queue a resize if need be. */
static void
vte_terminal_apply_metrics(VteTerminal *terminal,
			   gint width, gint height, gint ascent, gint descent)
{
	gboolean resize = FALSE, cresize = FALSE;
	gint line_thickness;

	/* Sanity check for broken font changes. */
	width = MAX(width, 1);
	height = MAX(height, 2);
	ascent = MAX(ascent, 1);
	descent = MAX(descent, 1);

	/* Change settings, and keep track of when we've changed anything. */
	if (width != terminal->pvt->char_width) {
		resize = cresize = TRUE;
		terminal->pvt->char_width = width;
	}
	if (height != terminal->pvt->char_height) {
		resize = cresize = TRUE;
		terminal->pvt->char_height = height;
	}
	if (ascent != terminal->pvt->char_ascent) {
		resize = TRUE;
		terminal->pvt->char_ascent = ascent;
	}
	if (descent != terminal->pvt->char_descent) {
		resize = TRUE;
		terminal->pvt->char_descent = descent;
	}
	terminal->pvt->line_thickness = line_thickness = MAX (MIN ((height - ascent) / 2, height / 14), 1);
	terminal->pvt->underline_position = MIN (ascent + line_thickness, height - line_thickness);
	terminal->pvt->strikethrough_position =  ascent - height / 4;

	/* Queue a resize if anything's changed. */
	if (resize) {
		if (gtk_widget_get_realized (&terminal->widget)) {
			gtk_widget_queue_resize_no_redraw(&terminal->widget);
		}
	}
	/* Emit a signal that the font changed. */
	if (cresize) {
		vte_terminal_emit_char_size_changed(terminal,
						    terminal->pvt->char_width,
						    terminal->pvt->char_height);
	}
	/* Repaint. */
	_vte_invalidate_all(terminal);
}

static void
vte_terminal_ensure_font (VteTerminal *terminal)
{
        terminal->pvt->ensure_font();
}

void
VteTerminalPrivate::ensure_font()
{
	if (m_draw != NULL) {
		/* Load default fonts, if no fonts have been loaded. */
		if (!m_has_fonts) {
			vte_terminal_set_font(m_terminal,
                                              m_unscaled_font_desc);
		}
		if (m_fontdirty) {
			gint width, height, ascent;
			m_fontdirty = FALSE;
			_vte_draw_set_text_font (m_draw,
                                                 m_widget,
					m_fontdesc);
			_vte_draw_get_text_metrics (m_draw,
						    &width, &height, &ascent);
			vte_terminal_apply_metrics(m_terminal,
						   width, height, ascent, height - ascent);
		}
	}
}

void
VteTerminalPrivate::update_font()
{
        /* We'll get called again later */
        if (m_unscaled_font_desc == nullptr)
                return;

        auto desc = pango_font_description_copy(m_unscaled_font_desc);

        double size = pango_font_description_get_size(desc);
        if (pango_font_description_get_size_is_absolute(desc)) {
                pango_font_description_set_absolute_size(desc, m_font_scale * size);
        } else {
                pango_font_description_set_size(desc, m_font_scale * size);
        }

        if (m_fontdesc) {
                pango_font_description_free(m_fontdesc);
        }
        m_fontdesc = desc;

        m_fontdirty = TRUE;
        m_has_fonts = TRUE;

        /* Set the drawing font. */
        if (gtk_widget_get_realized(m_widget)) {
                ensure_font();
        }
}

/*
 * VteTerminalPrivate::set_font_desc:
 * @font_desc: (allow-none): a #PangoFontDescription for the desired font, or %nullptr
 *
 * Sets the font used for rendering all text displayed by the terminal,
 * overriding any fonts set using gtk_widget_modify_font().  The terminal
 * will immediately attempt to load the desired font, retrieve its
 * metrics, and attempt to resize itself to keep the same number of rows
 * and columns.  The font scale is applied to the specified font.
 */
bool
VteTerminalPrivate::set_font_desc(PangoFontDescription const* font_desc)
{
	/* Create an owned font description. */
        auto context = gtk_widget_get_style_context(m_widget);
        PangoFontDescription *desc;
        gtk_style_context_get(context, GTK_STATE_FLAG_NORMAL, "font", &desc, nullptr);
	pango_font_description_set_family_static (desc, "monospace");
	if (font_desc != nullptr) {
		pango_font_description_merge (desc, font_desc, TRUE);
		_VTE_DEBUG_IF(VTE_DEBUG_MISC) {
			if (desc) {
				char *tmp;
				tmp = pango_font_description_to_string(desc);
				g_printerr("Using pango font \"%s\".\n", tmp);
				g_free (tmp);
			}
		}
	} else {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Using default monospace font.\n");
	}

        bool same_desc = m_unscaled_font_desc &&
                pango_font_description_equal(m_unscaled_font_desc, desc);

	/* Note that we proceed to recreating the font even if the description
	 * are the same.  This is because maybe screen
	 * font options were changed, or new fonts installed.  Those will be
	 * detected at font creation time and respected.
	 */

	/* Free the old font description and save the new one. */
	if (m_unscaled_font_desc != nullptr) {
		pango_font_description_free(m_unscaled_font_desc);
	}

        m_unscaled_font_desc = desc /* adopted */;

        update_font();

        return !same_desc;
}

bool
VteTerminalPrivate::set_font_scale(gdouble scale)
{
        /* FIXME: compare old and new scale in pixel space */
        if (_vte_double_equal(scale, m_font_scale))
                return false;

        m_font_scale = scale;
        update_font();

        return true;
}

/* Read and refresh our perception of the size of the PTY. */
static void
vte_terminal_refresh_size(VteTerminal *terminal)
{
        VteTerminalPrivate *pvt = terminal->pvt;
	int rows, columns;
        GError *error = NULL;

        if (pvt->pty == NULL)
                return;

        if (vte_pty_get_size(pvt->pty, &rows, &columns, &error)) {
                terminal->pvt->row_count = rows;
                terminal->pvt->column_count = columns;
        } else {
                g_warning(_("Error reading PTY size, using defaults: %s\n"), error->message);
                g_error_free(error);
	}
}

/* Resize the given screen (normal or alternate) of the terminal. */
static void
vte_terminal_screen_set_size(VteTerminal *terminal, VteScreen *screen, glong old_columns, glong old_rows, gboolean do_rewrap)
{
	VteRing *ring = screen->row_data;
	VteVisualPosition cursor_saved_absolute;
	VteVisualPosition below_viewport;
	VteVisualPosition below_current_paragraph;
	VteVisualPosition *markers[7];
        gboolean was_scrolled_to_top = ((long) ceil(screen->scroll_delta) == _vte_ring_delta(ring));
        gboolean was_scrolled_to_bottom = ((long) screen->scroll_delta == screen->insert_delta);
	glong old_top_lines;
	double new_scroll_delta;

        if (terminal->pvt->selection_block_mode && do_rewrap && old_columns != terminal->pvt->column_count)
                terminal->pvt->deselect_all();

	_vte_debug_print(VTE_DEBUG_RESIZE,
			"Resizing %s screen\n"
			"Old  insert_delta=%ld  scroll_delta=%f\n"
                        "     cursor (absolute)  row=%ld  col=%ld\n"
			"     cursor_saved (relative to insert_delta)  row=%ld  col=%ld\n",
			screen == &terminal->pvt->normal_screen ? "normal" : "alternate",
			screen->insert_delta, screen->scroll_delta,
                        terminal->pvt->cursor.row, terminal->pvt->cursor.col,
                        screen->saved.cursor.row, screen->saved.cursor.col);

        cursor_saved_absolute.row = screen->saved.cursor.row + screen->insert_delta;
        cursor_saved_absolute.col = screen->saved.cursor.col;
	below_viewport.row = screen->scroll_delta + old_rows;
	below_viewport.col = 0;
        below_current_paragraph.row = terminal->pvt->cursor.row + 1;
	while (below_current_paragraph.row < _vte_ring_next(ring)
	    && _vte_ring_index(ring, below_current_paragraph.row - 1)->attr.soft_wrapped) {
		below_current_paragraph.row++;
	}
	below_current_paragraph.col = 0;
        memset(&markers, 0, sizeof(markers));
        markers[0] = &cursor_saved_absolute;
        markers[1] = &below_viewport;
        markers[2] = &below_current_paragraph;
        if (screen == terminal->pvt->screen)
                /* Tracking the current cursor only makes sense on the active screen. */
                markers[3] = &terminal->pvt->cursor;
                if (terminal->pvt->has_selection) {
                        /* selection_end is inclusive, make it non-inclusive, see bug 722635. */
                        terminal->pvt->selection_end.col++;
                        markers[4] = &terminal->pvt->selection_start;
                        markers[5] = &terminal->pvt->selection_end;
	}

	old_top_lines = below_current_paragraph.row - screen->insert_delta;

	if (do_rewrap && old_columns != terminal->pvt->column_count)
		_vte_ring_rewrap(ring, terminal->pvt->column_count, markers);

	if (_vte_ring_length(ring) > terminal->pvt->row_count) {
		/* The content won't fit without scrollbars. Before figuring out the position, we might need to
		   drop some lines from the ring if the cursor is not at the bottom, as XTerm does. See bug 708213.
		   This code is really tricky, see ../doc/rewrap.txt for details! */
		glong new_top_lines, drop1, drop2, drop3, drop;
		screen->insert_delta = _vte_ring_next(ring) - terminal->pvt->row_count;
		new_top_lines = below_current_paragraph.row - screen->insert_delta;
		drop1 = _vte_ring_length(ring) - terminal->pvt->row_count;
		drop2 = _vte_ring_next(ring) - below_current_paragraph.row;
		drop3 = old_top_lines - new_top_lines;
		drop = MIN(MIN(drop1, drop2), drop3);
		if (drop > 0) {
			int new_ring_next = screen->insert_delta + terminal->pvt->row_count - drop;
			_vte_debug_print(VTE_DEBUG_RESIZE,
					"Dropping %ld [== MIN(%ld, %ld, %ld)] rows at the bottom\n",
					drop, drop1, drop2, drop3);
			_vte_ring_shrink(ring, new_ring_next - _vte_ring_delta(ring));
		}
	}

	if (screen == terminal->pvt->screen && terminal->pvt->has_selection) {
		/* Make selection_end inclusive again, see above. */
		terminal->pvt->selection_end.col--;
	}

	/* Figure out new insert and scroll deltas */
	if (_vte_ring_length(ring) <= terminal->pvt->row_count) {
		/* Everything fits without scrollbars. Align at top. */
		screen->insert_delta = _vte_ring_delta(ring);
		new_scroll_delta = screen->insert_delta;
		_vte_debug_print(VTE_DEBUG_RESIZE,
				"Everything fits without scrollbars\n");
	} else {
		/* Scrollbar required. Can't afford unused lines at bottom. */
		screen->insert_delta = _vte_ring_next(ring) - terminal->pvt->row_count;
		if (was_scrolled_to_bottom) {
			/* Was scrolled to bottom, keep this way. */
			new_scroll_delta = screen->insert_delta;
			_vte_debug_print(VTE_DEBUG_RESIZE,
					"Scroll to bottom\n");
		} else if (was_scrolled_to_top) {
			/* Was scrolled to top, keep this way. Not sure if this special case is worth it. */
			new_scroll_delta = _vte_ring_delta(ring);
			_vte_debug_print(VTE_DEBUG_RESIZE,
					"Scroll to top\n");
		} else {
			/* Try to scroll so that the bottom visible row stays.
			   More precisely, the character below the bottom left corner stays in that
			   (invisible) row.
			   So if the bottom of the screen was at a hard line break then that hard
			   line break will stay there.
			   TODO: What would be the best behavior if the bottom of the screen is a
			   soft line break, i.e. only a partial line is visible at the bottom? */
			new_scroll_delta = below_viewport.row - terminal->pvt->row_count;
			/* Keep the old fractional part. */
			new_scroll_delta += screen->scroll_delta - floor(screen->scroll_delta);
			_vte_debug_print(VTE_DEBUG_RESIZE,
					"Scroll so bottom row stays\n");
		}
	}

	/* Don't clamp, they'll be clamped when restored. Until then remember off-screen values
	   since they might become on-screen again on subsequent resizes. */
        screen->saved.cursor.row = cursor_saved_absolute.row - screen->insert_delta;
        screen->saved.cursor.col = cursor_saved_absolute.col;

	_vte_debug_print(VTE_DEBUG_RESIZE,
			"New  insert_delta=%ld  scroll_delta=%f\n"
                        "     cursor (absolute)  row=%ld  col=%ld\n"
			"     cursor_saved (relative to insert_delta)  row=%ld  col=%ld\n\n",
			screen->insert_delta, new_scroll_delta,
                        terminal->pvt->cursor.row, terminal->pvt->cursor.col,
                        screen->saved.cursor.row, screen->saved.cursor.col);

	if (screen == terminal->pvt->screen)
		vte_terminal_queue_adjustment_value_changed (
				terminal,
				new_scroll_delta);
	else
		screen->scroll_delta = new_scroll_delta;
}

void
VteTerminalPrivate::set_size(long columns,
                             long rows)
{
	glong old_columns, old_rows;

	_vte_debug_print(VTE_DEBUG_RESIZE,
			"Setting PTY size to %ldx%ld.\n",
			columns, rows);

	old_rows = m_row_count;
	old_columns = m_column_count;

	if (m_pty != NULL) {
                GError *error = NULL;

		/* Try to set the terminal size, and read it back,
		 * in case something went awry.
                 */
		if (!vte_pty_set_size(m_pty, rows, columns, &error)) {
			g_warning("%s\n", error->message);
                        g_error_free(error);
		}
		vte_terminal_refresh_size(m_terminal);
	} else {
		m_row_count = rows;
		m_column_count = columns;
	}
	if (old_rows != m_row_count || old_columns != m_column_count) {
                m_scrolling_restricted = FALSE;

                _vte_ring_set_visible_rows(m_normal_screen.row_data, m_row_count);
                _vte_ring_set_visible_rows(m_alternate_screen.row_data, m_row_count);

		/* Resize the normal screen and (if rewrapping is enabled) rewrap it even if the alternate screen is visible: bug 415277 */
		vte_terminal_screen_set_size(m_terminal, &m_normal_screen, old_columns, old_rows, m_rewrap_on_resize);
		/* Resize the alternate screen if it's the current one, but never rewrap it: bug 336238 comment 60 */
		if (m_screen == &m_alternate_screen)
			vte_terminal_screen_set_size(m_terminal, &m_alternate_screen, old_columns, old_rows, FALSE);

                /* Ensure scrollback buffers cover the screen. */
                vte_terminal_set_scrollback_lines(m_terminal,
                                                  m_scrollback_lines);
                /* Ensure the cursor is valid */
                m_cursor.row = CLAMP (m_cursor.row,
                                                    _vte_ring_delta (m_screen->row_data),
                                                    MAX (_vte_ring_delta (m_screen->row_data),
                                                         _vte_ring_next (m_screen->row_data) - 1));

		_vte_terminal_adjust_adjustments_full(m_terminal);
		gtk_widget_queue_resize_no_redraw(m_widget);
		/* Our visible text changed. */
		vte_terminal_emit_text_modified(m_terminal);
	}
}

/* Redraw the widget. */
static void
vte_terminal_handle_scroll(VteTerminal *terminal)
{
	double dy, adj;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	/* Read the new adjustment value and save the difference. */
	adj = gtk_adjustment_get_value(terminal->pvt->vadjustment);
	dy = adj - screen->scroll_delta;
	screen->scroll_delta = adj;

	/* Sanity checks. */
        if (G_UNLIKELY(!gtk_widget_get_realized(&terminal->widget)))
                return;
	if (terminal->pvt->visibility_state == GDK_VISIBILITY_FULLY_OBSCURED) {
		return;
	}

	if (dy != 0) {
		_vte_debug_print(VTE_DEBUG_ADJ,
			    "Scrolling by %f\n", dy);
                _vte_invalidate_all(terminal);
		vte_terminal_emit_text_scrolled(terminal, dy);
		_vte_terminal_queue_contents_changed(terminal);
	} else {
		_vte_debug_print(VTE_DEBUG_ADJ, "Not scrolling\n");
	}
}

void
VteTerminalPrivate::widget_set_hadjustment(GtkAdjustment *adjustment)
{
  if (adjustment == m_hadjustment)
    return;

  if (m_hadjustment)
    g_object_unref (m_hadjustment);

  m_hadjustment = adjustment ? (GtkAdjustment *)g_object_ref_sink(adjustment) : nullptr;
}

void
VteTerminalPrivate::widget_set_vadjustment(GtkAdjustment *adjustment)
{
	if (adjustment != nullptr && adjustment == m_vadjustment)
		return;
	if (adjustment == nullptr && m_vadjustment != nullptr)
		return;

	if (adjustment == nullptr)
		adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0));

	/* Add a reference to the new adjustment object. */
	g_object_ref_sink(adjustment);
	/* Get rid of the old adjustment object. */
	if (m_vadjustment != nullptr) {
		/* Disconnect our signal handlers from this object. */
		g_signal_handlers_disconnect_by_func(m_vadjustment,
						     (void*)vte_terminal_handle_scroll,
						     m_terminal);
		g_object_unref(m_vadjustment);
	}

	/* Set the new adjustment object. */
	m_vadjustment = adjustment;

	/* We care about the offset, not the top or bottom. */
	g_signal_connect_swapped(m_vadjustment,
				 "value-changed",
				 G_CALLBACK(vte_terminal_handle_scroll),
				 m_terminal);
}

void
_vte_terminal_inline_error_message(VteTerminal *terminal, const char *format, ...)
{
	va_list ap;
	char *str;

	va_start (ap, format);
	str = g_strdup_vprintf (format, ap);
	va_end (ap);

	vte_terminal_feed (terminal, "*** VTE ***: ", 13);
	vte_terminal_feed (terminal, str, -1);
	vte_terminal_feed (terminal, "\r\n", 2);
	g_free (str);
}

VteTerminalPrivate::VteTerminalPrivate(VteTerminal *t) :
        m_terminal(t),
        m_widget(&t->widget)
{
        // FIXMEchpe temporary workaround until all functions have been converted to members
        m_terminal->pvt = this;

	int i;
	GdkDisplay *display;

	/* NOTE! We allocated zeroed memory, just fill in non-zero stuff. */

	gtk_widget_set_can_focus(m_widget, TRUE);

        // FIXMEchpe is this still necessary?
	gtk_widget_set_app_paintable(m_widget, TRUE);

	/* We do our own redrawing. */
        // FIXMEchpe still necessary?
	gtk_widget_set_redraw_on_allocate(m_widget, FALSE);

	/* Set an adjustment for the application to use to control scrolling. */
        m_vadjustment = nullptr;
        m_hadjustment = nullptr;

        /* GtkScrollable */
        m_hscroll_policy = GTK_SCROLL_NATURAL;
        m_vscroll_policy = GTK_SCROLL_NATURAL;

        widget_set_hadjustment(nullptr);
	widget_set_vadjustment(nullptr);

	/* Set up dummy metrics, value != 0 to avoid division by 0 */
	m_char_width = 1;
	m_char_height = 1;
	m_char_ascent = 1;
	m_char_descent = 1;
	m_line_thickness = 1;
	m_underline_position = 1;
	m_strikethrough_position = 1;

        m_row_count = VTE_ROWS;
        m_column_count = VTE_COLUMNS;

	/* Initialize the screens and histories. */
	_vte_ring_init (m_alternate_screen.row_data, m_row_count, FALSE);
	m_screen = &m_alternate_screen;
	_vte_ring_init (m_normal_screen.row_data, VTE_SCROLLBACK_INIT, TRUE);
	m_screen = &m_normal_screen;

	_vte_terminal_set_default_attributes(m_terminal);

        /* Initialize charset modes. */
        m_character_replacements[0] = VTE_CHARACTER_REPLACEMENT_NONE;
        m_character_replacements[1] = VTE_CHARACTER_REPLACEMENT_NONE;
        m_character_replacement = &m_character_replacements[0];

	/* Set up the desired palette. */
	vte_terminal_set_default_colors(m_terminal);
	for (i = 0; i < VTE_PALETTE_SIZE; i++)
		m_palette[i].sources[VTE_COLOR_SOURCE_ESCAPE].is_set = FALSE;

	/* Set up I/O encodings. */
        m_utf8_ambiguous_width = VTE_DEFAULT_UTF8_AMBIGUOUS_WIDTH;
        m_iso2022 = _vte_iso2022_state_new(m_encoding);
	m_incoming = nullptr;
	m_pending = g_array_new(FALSE, TRUE, sizeof(gunichar));
	m_max_input_bytes = VTE_MAX_INPUT_READ;
	m_cursor_blink_tag = 0;
	m_outgoing = _vte_byte_array_new();
	m_outgoing_conv = VTE_INVALID_CONV;
	m_conv_buffer = _vte_byte_array_new();
	vte_terminal_set_encoding(m_terminal, NULL /* UTF-8 */, NULL);
	g_assert_cmpstr(m_encoding, ==, "UTF-8");

        /* Set up the emulation. */
	m_keypad_mode = VTE_KEYMODE_NORMAL;
	m_cursor_mode = VTE_KEYMODE_NORMAL;
        m_autowrap = TRUE;
        m_sendrecv_mode = TRUE;
	m_dec_saved = g_hash_table_new(NULL, NULL);
        m_matcher = _vte_matcher_new();
        m_alternate_screen_scroll = TRUE;

	/* Setting the terminal type and size requires the PTY master to
	 * be set up properly first. */
        m_pty = nullptr;
        vte_terminal_set_size(m_terminal, VTE_COLUMNS, VTE_ROWS);
	m_pty_input_source = 0;
	m_pty_output_source = 0;
	m_pty_pid = -1;

	/* Scrolling options. */
	m_scroll_on_keystroke = TRUE;
	m_alternate_screen_scroll = TRUE;
        m_scrollback_lines = -1; /* force update in vte_terminal_set_scrollback_lines */
	vte_terminal_set_scrollback_lines(m_terminal, VTE_SCROLLBACK_INIT);

	/* Selection info. */
	display = gtk_widget_get_display(m_widget);
	m_clipboard[VTE_SELECTION_PRIMARY] = gtk_clipboard_get_for_display(display, GDK_SELECTION_PRIMARY);
	m_clipboard[VTE_SELECTION_CLIPBOARD] = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);

	/* Miscellaneous options. */
	vte_terminal_set_backspace_binding(m_terminal, VTE_ERASE_AUTO);
	vte_terminal_set_delete_binding(m_terminal, VTE_ERASE_AUTO);
	m_meta_sends_escape = TRUE;
	m_audible_bell = TRUE;
	m_bell_margin = 10;
	m_allow_bold = TRUE;
        m_deccolm_mode = FALSE;
        m_rewrap_on_resize = TRUE;
	vte_terminal_set_default_tabstops(m_terminal);

        m_input_enabled = TRUE;

	/* Cursor shape. */
	m_cursor_shape = VTE_CURSOR_SHAPE_BLOCK;
        m_cursor_aspect_ratio = 0.04;

	/* Cursor blinking. */
	m_cursor_visible = TRUE;
	m_cursor_blink_timeout = 500;
        m_cursor_blinks = FALSE;
        m_cursor_blink_mode = VTE_CURSOR_BLINK_SYSTEM;

        /* DECSCUSR cursor style (shape and blinking possibly overridden
         * via escape sequence) */
        m_cursor_style = VTE_CURSOR_STYLE_TERMINAL_DEFAULT;

        /* Initialize the saved cursor. */
        _vte_terminal_save_cursor(m_terminal, &m_normal_screen);
        _vte_terminal_save_cursor(m_terminal, &m_alternate_screen);

	/* Matching data. */
        m_match_regex_mode = VTE_REGEX_UNDECIDED;
	m_match_regexes = g_array_new(FALSE, TRUE,
					 sizeof(struct vte_match_regex));
        m_match_tag = -1;
	match_hilite_clear(); // FIXMEchpe unnecessary

        /* Search data */
        m_search_regex.mode = VTE_REGEX_UNDECIDED;

	/* Rendering data */
	m_draw = _vte_draw_new();

	/* Set up background information. */
        m_background_alpha = 1.;

        /* Word chars */
        vte_terminal_set_word_char_exceptions(m_terminal, WORD_CHAR_EXCEPTIONS_DEFAULT);

        /* Selection */
	m_selection_block_mode = FALSE;
        m_unscaled_font_desc = nullptr;
        m_fontdesc = nullptr;
        m_font_scale = 1.;
	m_has_fonts = FALSE;

	/* Not all backends generate GdkVisibilityNotify, so mark the
	 * window as unobscured initially. */
	m_visibility_state = GDK_VISIBILITY_UNOBSCURED;

        m_padding = default_padding;
}

void
VteTerminalPrivate::widget_get_preferred_width(int *minimum_width,
                                               int *natural_width)
{
	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_get_preferred_width()\n");

	vte_terminal_ensure_font(m_terminal);

        vte_terminal_refresh_size(m_terminal);
	*minimum_width = m_char_width * 1;
        *natural_width = m_char_width * m_column_count;

	*minimum_width += m_padding.left +
                          m_padding.right;
	*natural_width += m_padding.left +
                          m_padding.right;

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
			"[Terminal %p] minimum_width=%d, natural_width=%d for %ldx%ld cells.\n",
                        m_terminal,
			*minimum_width, *natural_width,
			m_column_count,
			m_row_count);
}

void
VteTerminalPrivate::widget_get_preferred_height(int *minimum_height,
                                                int *natural_height)
{
	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_get_preferred_height()\n");

	vte_terminal_ensure_font(m_terminal);

        vte_terminal_refresh_size(m_terminal);
	*minimum_height = m_char_height * 1;
        *natural_height = m_char_height * m_row_count;

	*minimum_height += m_padding.left +
			   m_padding.right;
	*natural_height += m_padding.left +
			   m_padding.right;

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
			"[Terminal %p] minimum_height=%d, natural_height=%d for %ldx%ld cells.\n",
                        m_terminal,
			*minimum_height, *natural_height,
			m_column_count,
			m_row_count);
}

void
VteTerminalPrivate::widget_size_allocate(GtkAllocation *allocation)
{
	glong width, height;
	GtkAllocation current_allocation;
	gboolean repaint, update_scrollback;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE,
			"vte_terminal_size_allocate()\n");

	width = (allocation->width - (m_padding.left + m_padding.right)) /
		m_char_width;
	height = (allocation->height - (m_padding.top + m_padding.bottom)) /
		 m_char_height;
	width = MAX(width, 1);
	height = MAX(height, 1);

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
			"[Terminal %p] Sizing window to %dx%d (%ldx%ld).\n",
                        m_terminal,
			allocation->width, allocation->height,
			width, height);

	gtk_widget_get_allocation(m_widget, &current_allocation);

	repaint = current_allocation.width != allocation->width
			|| current_allocation.height != allocation->height;
	update_scrollback = current_allocation.height != allocation->height;

	/* Set our allocation to match the structure. */
	gtk_widget_set_allocation(m_widget, allocation);

	if (width != m_column_count
			|| height != m_row_count
			|| update_scrollback)
	{
		/* Set the size of the pseudo-terminal. */
		vte_terminal_set_size(m_terminal, width, height);

		/* Notify viewers that the contents have changed. */
		_vte_terminal_queue_contents_changed(m_terminal);
	}

	/* Resize the GDK window. */
	if (gtk_widget_get_realized(m_widget)) {
		gdk_window_move_resize(gtk_widget_get_window(m_widget),
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);
		/* Force a repaint if we were resized. */
		if (repaint) {
			reset_update_regions(m_terminal);
			invalidate_all();
		}
	}
}

void
VteTerminalPrivate::widget_unrealize()
{
	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_unrealize()\n");

	GdkWindow *window = gtk_widget_get_window(m_widget);

	/* Deallocate the cursors. */
	m_mouse_cursor_visible = FALSE;
	g_object_unref(m_mouse_default_cursor);
	m_mouse_default_cursor = NULL;
	g_object_unref(m_mouse_mousing_cursor);
	m_mouse_mousing_cursor = NULL;
	g_object_unref(m_mouse_inviso_cursor);
	m_mouse_inviso_cursor = NULL;

	match_hilite_clear();

	/* Shut down input methods. */
	if (m_im_context != nullptr) {
	        g_signal_handlers_disconnect_by_func (m_im_context,
						      (void *)vte_terminal_im_preedit_changed,
						      m_terminal);
		vte_terminal_im_reset(m_terminal);
		gtk_im_context_set_client_window(m_im_context,
						 NULL);
		g_object_unref(m_im_context);
		m_im_context = nullptr;
	}
	m_im_preedit_active = FALSE;
	if (m_im_preedit != nullptr) {
		g_free(m_im_preedit);
		m_im_preedit = NULL;
	}
	if (m_im_preedit_attrs != NULL) {
		pango_attr_list_unref(m_im_preedit_attrs);
		m_im_preedit_attrs = NULL;
	}
	m_im_preedit_cursor = 0;

	/* Clean up our draw structure. */
	if (m_draw != NULL) {
		_vte_draw_free(m_draw);
		m_draw = NULL;
	}
	m_fontdirty = TRUE;

	/* Unmap the widget if it hasn't been already. */
        // FIXMEchpe this can't happen
	if (gtk_widget_get_mapped(m_widget)) {
		gtk_widget_unmap(m_widget);
	}

	/* Remove the GDK window. */
	if (window != NULL) {
		gdk_window_set_user_data (window, NULL);
		gtk_widget_set_window(m_widget, NULL);

		gdk_window_destroy (window);
	}

	/* Remove the blink timeout function. */
	remove_cursor_timeout();

	/* Cancel any pending redraws. */
	remove_update_timeout(m_terminal);

	/* Cancel any pending signals */
	m_contents_changed_pending = FALSE;
	m_cursor_moved_pending = FALSE;
	m_text_modified_flag = FALSE;
	m_text_inserted_flag = FALSE;
	m_text_deleted_flag = FALSE;

	/* Clear modifiers. */
	m_modifiers = 0;

	/* Mark that we no longer have a GDK window. */
	gtk_widget_set_realized(m_widget, FALSE);
}

static void
vte_terminal_sync_settings (GtkSettings *settings,
                            GParamSpec *pspec,
                            VteTerminal *terminal)
{
        VteTerminalPrivate *pvt = terminal->pvt;
        gboolean blink;
        int blink_time = 1000;
        int blink_timeout = G_MAXINT;

        g_object_get(G_OBJECT (settings),
                     "gtk-cursor-blink", &blink,
                     "gtk-cursor-blink-time", &blink_time,
                     "gtk-cursor-blink-timeout", &blink_timeout,
                     NULL);

        _vte_debug_print(VTE_DEBUG_MISC,
                         "Cursor blinking settings setting: blink=%d time=%d timeout=%d\n",
                         blink, blink_time, blink_timeout);

        pvt->cursor_blink_cycle = blink_time / 2;
        pvt->cursor_blink_timeout = blink_timeout;

        vte_terminal_update_cursor_blinks_internal(terminal);
}

void
VteTerminalPrivate::widget_screen_changed (GdkScreen *previous_screen)
{
        GtkSettings *settings;

        auto gdk_screen = gtk_widget_get_screen (m_widget);
        if (previous_screen != NULL &&
            (gdk_screen != previous_screen || gdk_screen == NULL)) {
                settings = gtk_settings_get_for_screen (previous_screen);
                g_signal_handlers_disconnect_matched (settings, G_SIGNAL_MATCH_DATA,
                                                      0, 0, NULL, NULL,
                                                      m_widget);
        }

        if (gdk_screen == previous_screen || gdk_screen == nullptr)
                return;

        settings = gtk_widget_get_settings(m_widget);
        vte_terminal_sync_settings(settings, NULL, m_terminal);
        g_signal_connect (settings, "notify::gtk-cursor-blink",
                          G_CALLBACK (vte_terminal_sync_settings), m_widget);
        g_signal_connect (settings, "notify::gtk-cursor-blink-time",
                          G_CALLBACK (vte_terminal_sync_settings), m_widget);
        g_signal_connect (settings, "notify::gtk-cursor-blink-timeout",
                          G_CALLBACK (vte_terminal_sync_settings), m_widget);
}

VteTerminalPrivate::~VteTerminalPrivate()
{
        GtkSettings *settings;
	struct vte_match_regex *regex;
	int sel;
	guint i;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_finalize()\n");

	/* Free the draw structure. */
	if (m_draw != NULL) {
		_vte_draw_free(m_draw);
	}

	/* The NLS maps. */
	_vte_iso2022_state_free(m_iso2022);

	/* Free the font description. */
        if (m_unscaled_font_desc != NULL) {
                pango_font_description_free(m_unscaled_font_desc);
        }
	if (m_fontdesc != NULL) {
		pango_font_description_free(m_fontdesc);
	}

	/* Free matching data. */
	if (m_match_attributes != NULL) {
		g_array_free(m_match_attributes, TRUE);
	}
	g_free(m_match_contents);
	if (m_match_regexes != NULL) {
		for (i = 0; i < m_match_regexes->len; i++) {
			regex = &g_array_index(m_match_regexes,
					       struct vte_match_regex,
					       i);
			/* Skip holes. */
			if (regex->tag < 0) {
				continue;
			}
                        regex_match_clear(regex);
		}
		g_array_free(m_match_regexes, TRUE);
	}

        regex_and_flags_clear(&m_search_regex);
	if (m_search_attrs)
		g_array_free (m_search_attrs, TRUE);

	/* Disconnect from autoscroll requests. */
	vte_terminal_stop_autoscroll(m_terminal);

	/* Cancel pending adjustment change notifications. */
	m_adjustment_changed_pending = FALSE;

	/* Tabstop information. */
	if (m_tabstops != NULL) {
		g_hash_table_destroy(m_tabstops);
	}

	/* Free any selected text, but if we currently own the selection,
	 * throw the text onto the clipboard without an owner so that it
	 * doesn't just disappear. */
        GObject *object = G_OBJECT(m_widget);
	for (sel = VTE_SELECTION_PRIMARY; sel < LAST_VTE_SELECTION; sel++) {
		if (m_selection_text[sel] != NULL) {
			if (gtk_clipboard_get_owner(m_clipboard[sel]) == object) {
				gtk_clipboard_set_text(m_clipboard[sel],
						       m_selection_text[sel],
						       -1);
			}
			g_free(m_selection_text[sel]);
#ifdef HTML_SELECTION
			g_free(m_selection_html[sel]);
#endif
		}
	}

	/* Clear the output histories. */
	_vte_ring_fini(m_normal_screen.row_data);
	_vte_ring_fini(m_alternate_screen.row_data);

	/* Free conversion descriptors. */
	if (m_outgoing_conv != VTE_INVALID_CONV) {
		_vte_conv_close(m_outgoing_conv);
		m_outgoing_conv = VTE_INVALID_CONV;
	}

	/* Start listening for child-exited signals and ignore them, so that no zombie child is left behind. */
        if (m_child_watch_source != 0) {
                g_source_remove (m_child_watch_source);
                m_child_watch_source = 0;
                g_child_watch_add_full(G_PRIORITY_HIGH,
                                       m_pty_pid,
                                       (GChildWatchFunc)vte_terminal_child_watch_cb,
                                       NULL, NULL);
        }

	/* Stop processing input. */
	vte_terminal_stop_processing(m_terminal);

	/* Discard any pending data. */
	_vte_incoming_chunks_release(m_incoming);
	_vte_byte_array_free(m_outgoing);
	g_array_free(m_pending, TRUE);
	_vte_byte_array_free(m_conv_buffer);

	/* Stop the child and stop watching for input from the child. */
	if (m_pty_pid != -1) {
#ifdef HAVE_GETPGID
		pid_t pgrp;
		pgrp = getpgid(m_pty_pid);
		if (pgrp != -1) {
			kill(-pgrp, SIGHUP);
		}
#endif
		kill(m_pty_pid, SIGHUP);
	}
	_vte_terminal_disconnect_pty_read(m_terminal);
	_vte_terminal_disconnect_pty_write(m_terminal);
	if (m_pty_channel != NULL) {
		g_io_channel_unref (m_pty_channel);
	}
	if (m_pty != NULL) {
                g_object_unref(m_pty);
	}

	/* Remove hash tables. */
	if (m_dec_saved != NULL) {
		g_hash_table_destroy(m_dec_saved);
	}

	/* Clean up emulation structures. */
	if (m_matcher != NULL) {
		_vte_matcher_free(m_matcher);
	}

	remove_update_timeout(m_terminal);

	/* discard title updates */
        g_free(m_window_title);
        g_free(m_window_title_changed);
	g_free(m_icon_title_changed);
        g_free(m_current_directory_uri_changed);
        g_free(m_current_directory_uri);
        g_free(m_current_file_uri_changed);
        g_free(m_current_file_uri);

        /* Word char exceptions */
        g_free(m_word_char_exceptions_string);
        g_free(m_word_char_exceptions);

	/* Free public-facing data. */
	g_free(m_icon_title);
	if (m_vadjustment != NULL) {
		g_object_unref(m_vadjustment);
	}

        settings = gtk_widget_get_settings(m_widget);
        g_signal_handlers_disconnect_matched (settings, G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL, NULL,
                                              m_terminal);
}

void
VteTerminalPrivate::widget_realize()
{
	GdkWindow *window;
	GdkWindowAttr attributes;
	GtkAllocation allocation;
	guint attributes_mask = 0;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_realize()\n");

	gtk_widget_get_allocation (m_widget, &allocation);

	/* Create the stock cursors. */
	m_mouse_cursor_visible = TRUE;
	m_mouse_default_cursor =
		_vte_terminal_cursor_new(m_terminal, VTE_DEFAULT_CURSOR);
	m_mouse_mousing_cursor =
		_vte_terminal_cursor_new(m_terminal, VTE_MOUSING_CURSOR);

	/* Create a GDK window for the widget. */
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual(m_widget);
	attributes.event_mask = gtk_widget_get_events(m_widget) |
				GDK_EXPOSURE_MASK |
				GDK_VISIBILITY_NOTIFY_MASK |
				GDK_FOCUS_CHANGE_MASK |
				GDK_SMOOTH_SCROLL_MASK |
				GDK_SCROLL_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_POINTER_MOTION_MASK |
				GDK_BUTTON1_MOTION_MASK |
				GDK_ENTER_NOTIFY_MASK |
				GDK_LEAVE_NOTIFY_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK;
	attributes.cursor = m_mouse_default_cursor;
	attributes_mask = GDK_WA_X |
			  GDK_WA_Y |
			  (attributes.visual ? GDK_WA_VISUAL : 0) |
			  GDK_WA_CURSOR;

	window = gdk_window_new(gtk_widget_get_parent_window (m_widget),
				 &attributes, attributes_mask);

	gtk_widget_set_window(m_widget, window);
	gdk_window_set_user_data(window, m_widget);
        //FIXMEchpe this is obsolete
	gtk_style_context_set_background(gtk_widget_get_style_context(m_widget), window);
        //FIXMEchpe move this to class init
	_VTE_DEBUG_IF (VTE_DEBUG_UPDATES) gdk_window_set_debug_updates (TRUE);

	/* Set the realized flag. */
	gtk_widget_set_realized(m_widget, TRUE);

	/* Create rendering data if this is a re-realise */
        if (m_draw == NULL) {
                m_draw = _vte_draw_new();
        }

	/* Set up input method support.  FIXME: do we need to handle the
	 * "retrieve-surrounding" and "delete-surrounding" events? */
	if (m_im_context != nullptr) {
		vte_terminal_im_reset(m_terminal);
		g_object_unref(m_im_context);
		m_im_context = nullptr;
	}
	m_im_preedit_active = FALSE;
	m_im_context = gtk_im_multicontext_new();
	gtk_im_context_set_client_window(m_im_context, window);
	g_signal_connect(m_im_context, "commit",
			 G_CALLBACK(vte_terminal_im_commit), m_terminal);
	g_signal_connect(m_im_context, "preedit-start",
			 G_CALLBACK(vte_terminal_im_preedit_start),
			 m_terminal);
	g_signal_connect(m_im_context, "preedit-changed",
			 G_CALLBACK(vte_terminal_im_preedit_changed),
			 m_terminal);
	g_signal_connect(m_im_context, "preedit-end",
			 G_CALLBACK(vte_terminal_im_preedit_end),
			 m_terminal);
	gtk_im_context_set_use_preedit(m_im_context, TRUE);

	/* Clear modifiers. */
	m_modifiers = 0;

	/* Create our invisible cursor. */
	m_mouse_inviso_cursor = gdk_cursor_new_for_display(gtk_widget_get_display(m_widget), GDK_BLANK_CURSOR);

        /* Make sure the style is set, bug 727614. */
        widget_style_updated();

	vte_terminal_ensure_font(m_terminal);

	/* Set up the background, *now*. */
        // FIXMEchpe this is obsolete
	vte_terminal_background_update(m_terminal);
}

static inline void
swap (guint *a, guint *b)
{
	guint tmp;
	tmp = *a, *a = *b, *b = tmp;
}

static void
vte_terminal_determine_colors_internal(VteTerminal *terminal,
				       const VteCellAttr *attr,
				       gboolean selected,
				       gboolean cursor,
				       guint *pfore, guint *pback)
{
	guint fore, back;

        g_assert(attr);

	/* Start with cell colors */
	fore = attr->fore;
	back = attr->back;

	/* Reverse-mode switches default fore and back colors */
        if (G_UNLIKELY (terminal->pvt->reverse_mode)) {
		if (fore == VTE_DEFAULT_FG)
			fore = VTE_DEFAULT_BG;
		if (back == VTE_DEFAULT_BG)
			back = VTE_DEFAULT_FG;
	}

	/* Handle bold by using set bold color or brightening */
	if (attr->bold) {
		if (fore == VTE_DEFAULT_FG)
			fore = VTE_BOLD_FG;
		else if (fore >= VTE_LEGACY_COLORS_OFFSET && fore < VTE_LEGACY_COLORS_OFFSET + VTE_LEGACY_COLOR_SET_SIZE) {
			fore += VTE_COLOR_BRIGHT_OFFSET;
		}
	}

        /* Handle dim colors.  Only apply to palette colors, dimming direct RGB wouldn't make sense.
         * Apply to the foreground color only, but do this before handling reverse/highlight so that
         * those can be used to dim the background instead. */
        if (attr->dim && !(fore & VTE_RGB_COLOR)) {
	        fore |= VTE_DIM_COLOR;
        }

	/* Reverse cell? */
	if (attr->reverse) {
		swap (&fore, &back);
	}

	/* Selection: use hightlight back/fore, or inverse */
	if (selected) {
		/* XXX what if hightlight back is same color as current back? */
		gboolean do_swap = TRUE;
		if (_vte_terminal_get_color(terminal, VTE_HIGHLIGHT_BG) != NULL) {
			back = VTE_HIGHLIGHT_BG;
			do_swap = FALSE;
		}
		if (_vte_terminal_get_color(terminal, VTE_HIGHLIGHT_FG) != NULL) {
			fore = VTE_HIGHLIGHT_FG;
			do_swap = FALSE;
		}
		if (do_swap)
			swap (&fore, &back);
	}

	/* Cursor: use cursor back, or inverse */
	if (cursor) {
		/* XXX what if cursor back is same color as current back? */
		if (_vte_terminal_get_color(terminal, VTE_CURSOR_BG) != NULL)
			back = VTE_CURSOR_BG;
		else
			swap (&fore, &back);
	}

	/* Invisible? */
	if (attr->invisible) {
		fore = back;
	}

	*pfore = fore;
	*pback = back;
}

static inline void
vte_terminal_determine_colors (VteTerminal *terminal,
			       const VteCell *cell,
			       gboolean highlight,
			       guint *fore, guint *back)
{
	vte_terminal_determine_colors_internal (terminal, cell ? &cell->attr : &basic_cell.cell.attr,
						       highlight, FALSE,
						       fore, back);
}

static inline void
vte_terminal_determine_cursor_colors (VteTerminal *terminal,
				      const VteCell *cell,
				      gboolean highlight,
				      guint *fore, guint *back)
{
	vte_terminal_determine_colors_internal (terminal, cell ? &cell->attr : &basic_cell.cell.attr,
						       highlight, TRUE,
						       fore, back);
}

static void
vte_terminal_fill_rectangle(VteTerminal *terminal,
			    const PangoColor *color,
			    gint x,
			    gint y,
			    gint width,
			    gint height)
{
	_vte_draw_fill_rectangle(terminal->pvt->draw,
				 x + terminal->pvt->padding.left,
                                 y + terminal->pvt->padding.top,
				 width, height,
				 color, VTE_DRAW_OPAQUE);
}

static void
vte_terminal_draw_line(VteTerminal *terminal,
		       const PangoColor *color,
		       gint x,
		       gint y,
		       gint xp,
		       gint yp)
{
	vte_terminal_fill_rectangle(terminal, color,
				    x, y,
				    MAX(VTE_LINE_WIDTH, xp - x + 1), MAX(VTE_LINE_WIDTH, yp - y + 1));
}

static void
vte_terminal_draw_rectangle(VteTerminal *terminal,
			    const PangoColor *color,
			    gint x,
			    gint y,
			    gint width,
			    gint height)
{
	_vte_draw_draw_rectangle(terminal->pvt->draw,
				 x + terminal->pvt->padding.left,
                                 y + terminal->pvt->padding.top,
				 width, height,
				 color, VTE_DRAW_OPAQUE);
}

/* Draw a string of characters with similar attributes. */
static void
vte_terminal_draw_cells(VteTerminal *terminal,
			struct _vte_draw_text_request *items, gssize n,
			guint fore, guint back, gboolean clear,
			gboolean draw_default_bg,
			gboolean bold, gboolean italic, gboolean underline,
			gboolean strikethrough, gboolean hilite, gboolean boxed,
			gint column_width, gint row_height)
{
	int i, x, y;
	gint columns = 0;
	PangoColor fg, bg;

	g_assert(n > 0);
	_VTE_DEBUG_IF(VTE_DEBUG_CELLS) {
		GString *str = g_string_new (NULL);
		gchar *tmp;
		for (i = 0; i < n; i++) {
			g_string_append_unichar (str, items[i].c);
		}
		tmp = g_string_free (str, FALSE);
		g_printerr ("draw_cells('%s', fore=%d, back=%d, bold=%d,"
				" ul=%d, strike=%d, hilite=%d, boxed=%d)\n",
				tmp, fore, back, bold,
				underline, strikethrough, hilite, boxed);
		g_free (tmp);
	}

	bold = bold && terminal->pvt->allow_bold;
	vte_terminal_get_rgb_from_index(terminal, fore, &fg);
	vte_terminal_get_rgb_from_index(terminal, back, &bg);

	i = 0;
	do {
		columns = 0;
		x = items[i].x;
		y = items[i].y;
		for (; i < n && items[i].y == y; i++) {
			/* Adjust for the border. */
			items[i].x += terminal->pvt->padding.left;
			items[i].y += terminal->pvt->padding.top;
			columns += items[i].columns;
		}
		if (clear && (draw_default_bg || back != VTE_DEFAULT_BG)) {
			gint bold_offset = _vte_draw_has_bold(terminal->pvt->draw,
									VTE_DRAW_BOLD) ? 0 : bold;
			_vte_draw_fill_rectangle(terminal->pvt->draw,
					x + terminal->pvt->padding.left,
                                        y + terminal->pvt->padding.top,
					columns * column_width + bold_offset, row_height,
					&bg, VTE_DRAW_OPAQUE);
		}
	} while (i < n);

	_vte_draw_text(terminal->pvt->draw,
			items, n,
			&fg, VTE_DRAW_OPAQUE,
			_vte_draw_get_style(bold, italic));

	for (i = 0; i < n; i++) {
		/* Deadjust for the border. */
		items[i].x -= terminal->pvt->padding.left;
		items[i].y -= terminal->pvt->padding.top;
	}

	/* Draw whatever SFX are required. */
	if (underline | strikethrough | hilite | boxed) {
		i = 0;
		do {
			x = items[i].x;
			y = items[i].y;
			for (columns = 0; i < n && items[i].y == y; i++) {
				columns += items[i].columns;
			}
			if (underline) {
				vte_terminal_draw_line(terminal,
						&fg,
						x,
						y + terminal->pvt->underline_position,
						x + (columns * column_width) - 1,
						y + terminal->pvt->underline_position + terminal->pvt->line_thickness - 1);
			}
			if (strikethrough) {
				vte_terminal_draw_line(terminal,
						&fg,
						x,
						y + terminal->pvt->strikethrough_position,
						x + (columns * column_width) - 1,
						y + terminal->pvt->strikethrough_position + terminal->pvt->line_thickness - 1);
			}
			if (hilite) {
				vte_terminal_draw_line(terminal,
						&fg,
						x,
						y + row_height - 1,
						x + (columns * column_width) - 1,
						y + row_height - 1);
			}
			if (boxed) {
				vte_terminal_draw_rectangle(terminal,
						&fg,
						x, y,
						MAX(0, (columns * column_width)),
						MAX(0, row_height));
			}
		}while (i < n);
	}
}

/* FIXME: we don't have a way to tell GTK+ what the default text attributes
 * should be, so for now at least it's assuming white-on-black is the norm and
 * is using "black-on-white" to signify "inverse".  Pick up on that state and
 * fix things.  Do this here, so that if we suddenly get red-on-black, we'll do
 * the right thing. */
static void
_vte_terminal_fudge_pango_colors(VteTerminal *terminal, GSList *attributes,
				 VteCell *cells, gssize n)
{
	int i, sumlen = 0;
	struct _fudge_cell_props{
		gboolean saw_fg, saw_bg;
		PangoColor fg, bg;
		guint index;
	}*props = g_newa (struct _fudge_cell_props, n);

	for (i = 0; i < n; i++) {
		gchar ubuf[7];
		gint len = g_unichar_to_utf8 (cells[i].c, ubuf);
		props[i].index = sumlen;
		props[i].saw_fg = props[i].saw_bg = FALSE;
		sumlen += len;
	}

	while (attributes != NULL) {
		PangoAttribute *attr = (PangoAttribute *)attributes->data;
		PangoAttrColor *color;
		switch (attr->klass->type) {
		case PANGO_ATTR_FOREGROUND:
			for (i = 0; i < n; i++) {
				if (props[i].index < attr->start_index) {
					continue;
				}
				if (props[i].index >= attr->end_index) {
					break;
				}
				props[i].saw_fg = TRUE;
				color = (PangoAttrColor*) attr;
				props[i].fg = color->color;
			}
			break;
		case PANGO_ATTR_BACKGROUND:
			for (i = 0; i < n; i++) {
				if (props[i].index < attr->start_index) {
					continue;
				}
				if (props[i].index >= attr->end_index) {
					break;
				}
				props[i].saw_bg = TRUE;
				color = (PangoAttrColor*) attr;
				props[i].bg = color->color;
			}
			break;
		default:
			break;
		}
		attributes = g_slist_next(attributes);
	}

	for (i = 0; i < n; i++) {
		if (props[i].saw_fg && props[i].saw_bg &&
				(props[i].fg.red == 0xffff) &&
				(props[i].fg.green == 0xffff) &&
				(props[i].fg.blue == 0xffff) &&
				(props[i].bg.red == 0) &&
				(props[i].bg.green == 0) &&
				(props[i].bg.blue == 0)) {
                        cells[i].attr.fore = terminal->pvt->color_defaults.attr.fore;
                        cells[i].attr.back = terminal->pvt->color_defaults.attr.back;
			cells[i].attr.reverse = TRUE;
		}
	}
}

/* Apply the attribute given in the PangoAttribute to the list of cells. */
static void
_vte_terminal_apply_pango_attr(VteTerminal *terminal, PangoAttribute *attr,
			       VteCell *cells, guint n_cells)
{
	guint i, ival;
	PangoAttrInt *attrint;
	PangoAttrColor *attrcolor;

	switch (attr->klass->type) {
	case PANGO_ATTR_FOREGROUND:
	case PANGO_ATTR_BACKGROUND:
		attrcolor = (PangoAttrColor*) attr;
		ival = VTE_RGB_COLOR |
		       ((attrcolor->color.red & 0xFF00) << 8) |
		       ((attrcolor->color.green & 0xFF00)) |
		       ((attrcolor->color.blue & 0xFF00) >> 8);
		for (i = attr->start_index;
		     i < attr->end_index && i < n_cells;
		     i++) {
			if (attr->klass->type == PANGO_ATTR_FOREGROUND) {
				cells[i].attr.fore = ival;
			}
			if (attr->klass->type == PANGO_ATTR_BACKGROUND) {
				cells[i].attr.back = ival;
			}
		}
		break;
	case PANGO_ATTR_STRIKETHROUGH:
		attrint = (PangoAttrInt*) attr;
		ival = attrint->value;
		for (i = attr->start_index;
		     (i < attr->end_index) && (i < n_cells);
		     i++) {
			cells[i].attr.strikethrough = (ival != FALSE);
		}
		break;
	case PANGO_ATTR_UNDERLINE:
		attrint = (PangoAttrInt*) attr;
		ival = attrint->value;
		for (i = attr->start_index;
		     (i < attr->end_index) && (i < n_cells);
		     i++) {
			cells[i].attr.underline = (ival != PANGO_UNDERLINE_NONE);
		}
		break;
	case PANGO_ATTR_WEIGHT:
		attrint = (PangoAttrInt*) attr;
		ival = attrint->value;
		for (i = attr->start_index;
		     (i < attr->end_index) && (i < n_cells);
		     i++) {
			cells[i].attr.bold = (ival >= PANGO_WEIGHT_BOLD);
		}
		break;
	default:
		break;
	}
}

/* Convert a PangoAttrList and a location in that list to settings in a
 * charcell structure.  The cells array is assumed to contain enough items
 * so that all ranges in the attribute list can be mapped into the array, which
 * typically means that the cell array should have the same length as the
 * string (byte-wise) which the attributes describe. */
static void
_vte_terminal_pango_attribute_destroy(gpointer attr, gpointer data)
{
	pango_attribute_destroy((PangoAttribute *)attr);
}
static void
_vte_terminal_translate_pango_cells(VteTerminal *terminal, PangoAttrList *attrs,
				    VteCell *cells, guint n_cells)
{
	PangoAttribute *attr;
	PangoAttrIterator *attriter;
	GSList *list, *listiter;
	guint i;

	for (i = 0; i < n_cells; i++) {
                cells[i] = terminal->pvt->fill_defaults;
	}

	attriter = pango_attr_list_get_iterator(attrs);
	if (attriter != NULL) {
		do {
			list = pango_attr_iterator_get_attrs(attriter);
			if (list != NULL) {
				for (listiter = list;
				     listiter != NULL;
				     listiter = g_slist_next(listiter)) {
					attr = (PangoAttribute *)listiter->data;
					_vte_terminal_apply_pango_attr(terminal,
								       attr,
								       cells,
								       n_cells);
				}
				attr = (PangoAttribute *)list->data;
				_vte_terminal_fudge_pango_colors(terminal,
								 list,
								 cells +
								 attr->start_index,
								 attr->end_index -
								 attr->start_index);
				g_slist_foreach(list,
						_vte_terminal_pango_attribute_destroy,
						NULL);
				g_slist_free(list);
			}
		} while (pango_attr_iterator_next(attriter) == TRUE);
		pango_attr_iterator_destroy(attriter);
	}
}

/* Draw the listed items using the given attributes.  Tricky because the
 * attribute string is indexed by byte in the UTF-8 representation of the string
 * of characters.  Because we draw a character at a time, this is slower. */
static void
vte_terminal_draw_cells_with_attributes(VteTerminal *terminal,
					struct _vte_draw_text_request *items,
					gssize n,
					PangoAttrList *attrs,
					gboolean draw_default_bg,
					gint column_width, gint height)
{
	int i, j, cell_count;
	VteCell *cells;
	char scratch_buf[VTE_UTF8_BPC];
	guint fore, back;

	/* Note: since this function is only called with the pre-edit text,
	 * all the items contain gunichar only, not vteunistr. */

	for (i = 0, cell_count = 0; i < n; i++) {
		cell_count += g_unichar_to_utf8(items[i].c, scratch_buf);
	}
	cells = g_new(VteCell, cell_count);
	_vte_terminal_translate_pango_cells(terminal, attrs, cells, cell_count);
	for (i = 0, j = 0; i < n; i++) {
		vte_terminal_determine_colors(terminal, &cells[j], FALSE, &fore, &back);
		vte_terminal_draw_cells(terminal, items + i, 1,
					fore,
					back,
					TRUE, draw_default_bg,
					cells[j].attr.bold,
					cells[j].attr.italic,
					cells[j].attr.underline,
					cells[j].attr.strikethrough,
					FALSE, FALSE, column_width, height);
		j += g_unichar_to_utf8(items[i].c, scratch_buf);
	}
	g_free(cells);
}


/* Paint the contents of a given row at the given location.  Take advantage
 * of multiple-draw APIs by finding runs of characters with identical
 * attributes and bundling them together. */
static void
vte_terminal_draw_rows(VteTerminal *terminal,
		      VteScreen *screen,
		      gint start_row, gint row_count,
		      gint start_column, gint column_count,
		      gint start_x, gint start_y,
		      gint column_width, gint row_height)
{
	struct _vte_draw_text_request items[4*VTE_DRAW_MAX_LENGTH];
	gint i, j, row, rows, x, y, end_column;
	guint fore, nfore, back, nback;
	gboolean underline, nunderline, bold, nbold, italic, nitalic, hilite, nhilite,
		 selected, nselected, strikethrough, nstrikethrough;
	guint item_count;
	const VteCell *cell;
	const VteRowData *row_data;

	/* adjust for the absolute start of row */
	start_x -= start_column * column_width;
	end_column = start_column + column_count;

	/* clear the background */
	x = start_x + terminal->pvt->padding.left;
	y = start_y + terminal->pvt->padding.top;
	row = start_row;
	rows = row_count;
	do {
		row_data = _vte_terminal_find_row_data(terminal, row);
		/* Back up in case this is a multicolumn character,
		 * making the drawing area a little wider. */
		i = start_column;
		if (row_data != NULL) {
			cell = _vte_row_data_get (row_data, i);
			if (cell != NULL) {
				while (cell->attr.fragment && i > 0) {
					cell = _vte_row_data_get (row_data, --i);
				}
			}
			/* Walk the line. */
			do {
				/* Get the character cell's contents. */
				cell = _vte_row_data_get (row_data, i);
				/* Find the colors for this cell. */
				selected = vte_cell_is_selected(terminal, i, row, NULL);
				vte_terminal_determine_colors(terminal, cell, selected, &fore, &back);

				bold = cell && cell->attr.bold;
				j = i + (cell ? cell->attr.columns : 1);

				while (j < end_column){
					/* Retrieve the cell. */
					cell = _vte_row_data_get (row_data, j);
					/* Don't render fragments of multicolumn characters
					 * which have the same attributes as the initial
					 * portions. */
					if (cell != NULL && cell->attr.fragment) {
						j++;
						continue;
					}
					/* Resolve attributes to colors where possible and
					 * compare visual attributes to the first character
					 * in this chunk. */
					selected = vte_cell_is_selected(terminal, j, row, NULL);
					vte_terminal_determine_colors(terminal, cell, selected, &nfore, &nback);
					if (nback != back) {
						break;
					}
					bold = cell && cell->attr.bold;
					j += cell ? cell->attr.columns : 1;
				}
				if (back != VTE_DEFAULT_BG) {
					PangoColor bg;
					gint bold_offset = _vte_draw_has_bold(terminal->pvt->draw,
											VTE_DRAW_BOLD) ? 0 : bold;
					vte_terminal_get_rgb_from_index(terminal, back, &bg);
					_vte_draw_fill_rectangle (
							terminal->pvt->draw,
							x + i * column_width,
							y,
							(j - i) * column_width + bold_offset,
							row_height,
							&bg, VTE_DRAW_OPAQUE);
				}
				/* We'll need to continue at the first cell which didn't
				 * match the first one in this set. */
				i = j;
			} while (i < end_column);
		} else {
			do {
				selected = vte_cell_is_selected(terminal, i, row, NULL);
				j = i + 1;
				while (j < end_column){
					nselected = vte_cell_is_selected(terminal, j, row, NULL);
					if (nselected != selected) {
						break;
					}
					j++;
				}
				vte_terminal_determine_colors(terminal, NULL, selected, &fore, &back);
				if (back != VTE_DEFAULT_BG) {
					PangoColor bg;
					vte_terminal_get_rgb_from_index(terminal, back, &bg);
					_vte_draw_fill_rectangle (terminal->pvt->draw,
								  x + i *column_width,
								  y,
								  (j - i)  * column_width,
								  row_height,
								  &bg, VTE_DRAW_OPAQUE);
				}
				i = j;
			} while (i < end_column);
		}
		row++;
		y += row_height;
	} while (--rows);


	/* render the text */
	y = start_y;
	row = start_row;
	rows = row_count;
	item_count = 1;
	do {
		row_data = _vte_terminal_find_row_data(terminal, row);
		if (row_data == NULL) {
			goto fg_skip_row;
		}
		/* Back up in case this is a multicolumn character,
		 * making the drawing area a little wider. */
		i = start_column;
		cell = _vte_row_data_get (row_data, i);
		if (cell == NULL) {
			goto fg_skip_row;
		}
		while (cell->attr.fragment && i > 0)
			cell = _vte_row_data_get (row_data, --i);

		/* Walk the line. */
		do {
			/* Get the character cell's contents. */
			cell = _vte_row_data_get (row_data, i);
			if (cell == NULL) {
				goto fg_skip_row;
			}
			while (cell->c == 0 || cell->attr.invisible ||
					(cell->c == ' ' &&
					 !cell->attr.underline &&
					 !cell->attr.strikethrough) ||
					cell->attr.fragment) {
				if (++i >= end_column) {
					goto fg_skip_row;
				}
				cell = _vte_row_data_get (row_data, i);
				if (cell == NULL) {
					goto fg_skip_row;
				}
			}
			/* Find the colors for this cell. */
			selected = vte_cell_is_selected(terminal, i, row, NULL);
			vte_terminal_determine_colors(terminal, cell, selected, &fore, &back);
			underline = cell->attr.underline;
			strikethrough = cell->attr.strikethrough;
			bold = cell->attr.bold;
			italic = cell->attr.italic;
			if (terminal->pvt->show_match) {
				hilite = vte_cell_is_between(i, row,
						terminal->pvt->match_start.col,
						terminal->pvt->match_start.row,
						terminal->pvt->match_end.col,
						terminal->pvt->match_end.row);
			} else {
				hilite = FALSE;
			}

			items[0].c = cell->c;
			items[0].columns = cell->attr.columns;
			items[0].x = start_x + i * column_width;
			items[0].y = y;
			j = i + items[0].columns;

			/* Now find out how many cells have the same attributes. */
			do {
				while (j < end_column &&
						item_count < G_N_ELEMENTS(items)) {
					/* Retrieve the cell. */
					cell = _vte_row_data_get (row_data, j);
					if (cell == NULL) {
						goto fg_next_row;
					}
					/* Don't render blank cells or fragments of multicolumn characters
					 * which have the same attributes as the initial
					 * portions.  Don't render invisible cells */
					if (cell->attr.fragment || cell->attr.invisible) {
						j++;
						continue;
					}
					if (cell->c == 0){
						/* only break the run if we
						 * are drawing attributes
						 */
						if (underline || strikethrough || hilite) {
							break;
						} else {
							j++;
							continue;
						}
					}
					/* Resolve attributes to colors where possible and
					 * compare visual attributes to the first character
					 * in this chunk. */
					selected = vte_cell_is_selected(terminal, j, row, NULL);
					vte_terminal_determine_colors(terminal, cell, selected, &nfore, &nback);
					if (nfore != fore) {
						break;
					}
					nbold = cell->attr.bold;
					if (nbold != bold) {
						break;
					}
					nitalic = cell->attr.italic;
					if (nitalic != italic) {
						break;
					}
					/* Break up underlined/not-underlined text. */
					nunderline = cell->attr.underline;
					if (nunderline != underline) {
						break;
					}
					nstrikethrough = cell->attr.strikethrough;
					if (nstrikethrough != strikethrough) {
						break;
					}
					/* Break up matched/not-matched text. */
					nhilite = FALSE;
					if (terminal->pvt->show_match) {
						nhilite = vte_cell_is_between(j, row,
								terminal->pvt->match_start.col,
								terminal->pvt->match_start.row,
								terminal->pvt->match_end.col,
								terminal->pvt->match_end.row);
					}
					if (nhilite != hilite) {
						break;
					}
					/* Add this cell to the draw list. */
					items[item_count].c = cell->c;
					items[item_count].columns = cell->attr.columns;
					items[item_count].x = start_x + j * column_width;
					items[item_count].y = y;
					j +=  items[item_count].columns;
					item_count++;
				}
				/* have we encountered a state change? */
				if (j < end_column) {
					break;
				}
fg_next_row:
				/* is this the last column, on the last row? */
				do {
					do {
						if (!--rows) {
							goto fg_draw;
						}

						/* restart on the next row */
						row++;
						y += row_height;
						row_data = _vte_terminal_find_row_data(terminal, row);
					} while (row_data == NULL);

					/* Back up in case this is a
					 * multicolumn character, making the drawing
					 * area a little wider. */
					j = start_column;
					cell = _vte_row_data_get (row_data, j);
				} while (cell == NULL);
				while (cell->attr.fragment && j > 0) {
					cell = _vte_row_data_get (row_data, --j);
				}
			} while (TRUE);
fg_draw:
			/* Draw the cells. */
			vte_terminal_draw_cells(terminal,
					items,
					item_count,
					fore, back, FALSE, FALSE,
					bold, italic, underline,
					strikethrough, hilite, FALSE,
					column_width, row_height);
			item_count = 1;
			/* We'll need to continue at the first cell which didn't
			 * match the first one in this set. */
			i = j;
			if (!rows) {
				goto fg_out;
			}
		} while (i < end_column);
fg_skip_row:
		row++;
		y += row_height;
	} while (--rows);
fg_out:
	return;
}

static void
vte_terminal_expand_region (VteTerminal *terminal, cairo_region_t *region, const GdkRectangle *area)
{
	int width, height;
	int row, col, row_stop, col_stop;
	cairo_rectangle_int_t rect;
	GtkAllocation allocation;

	width = terminal->pvt->char_width;
	height = terminal->pvt->char_height;

	gtk_widget_get_allocation (&terminal->widget, &allocation);

	/* increase the paint by one pixel on all sides to force the
	 * inclusion of neighbouring cells */
        row = _vte_terminal_pixel_to_row(terminal, MAX(0, area->y - terminal->pvt->padding.top - 1));
        /* Both the value given by MIN() and row_stop are exclusive.
         * _vte_terminal_pixel_to_row expects an actual value corresponding
         * to the bottom visible pixel, hence the - 1 + 1 magic. */
        row_stop = _vte_terminal_pixel_to_row(terminal, MIN(area->height + area->y - terminal->pvt->padding.top + 1,
                                                            allocation.height - terminal->pvt->padding.bottom) - 1) + 1;
	if (row_stop <= row) {
		return;
	}
	col = MAX(0, (area->x - terminal->pvt->padding.left - 1) / width);
	col_stop = MIN(howmany(area->width + area->x - terminal->pvt->padding.left + 1, width),
		       terminal->pvt->column_count);
	if (col_stop <= col) {
		return;
	}

	rect.x = col*width + terminal->pvt->padding.left;
	rect.width = (col_stop - col) * width;

	rect.y = _vte_terminal_row_to_pixel(terminal, row) + terminal->pvt->padding.top;
	rect.height = (row_stop - row)*height;

	/* the rect must be cell aligned to avoid overlapping XY bands */
	cairo_region_union_rectangle(region, &rect);

	_vte_debug_print (VTE_DEBUG_UPDATES,
			"vte_terminal_expand_region"
			"	(%d,%d)x(%d,%d) pixels,"
			" (%d,%d)x(%d,%d) cells"
			" [(%d,%d)x(%d,%d) pixels]\n",
			area->x, area->y, area->width, area->height,
			col, row, col_stop - col, row_stop - row,
			rect.x, rect.y, rect.width, rect.height);
}

static void
vte_terminal_paint_area (VteTerminal *terminal, const GdkRectangle *area)
{
	VteScreen *screen;
	int width, height;
	int row, col, row_stop, col_stop;
	GtkAllocation allocation;

	screen = terminal->pvt->screen;

	width = terminal->pvt->char_width;
	height = terminal->pvt->char_height;

	gtk_widget_get_allocation (&terminal->widget, &allocation);

        row = _vte_terminal_pixel_to_row(terminal, MAX(0, area->y - terminal->pvt->padding.top));
        /* Both the value given by MIN() and row_stop are exclusive.
         * _vte_terminal_pixel_to_row expects an actual value corresponding
         * to the bottom visible pixel, hence the - 1 + 1 magic. */
        row_stop = _vte_terminal_pixel_to_row(terminal, MIN(area->height + area->y - terminal->pvt->padding.top,
                                                            allocation.height - terminal->pvt->padding.bottom) - 1) + 1;
	if (row_stop <= row) {
		return;
	}
	col = MAX(0, (area->x - terminal->pvt->padding.left) / width);
	col_stop = MIN((area->width + area->x - terminal->pvt->padding.left) / width,
		       terminal->pvt->column_count);
	if (col_stop <= col) {
		return;
	}
	_vte_debug_print (VTE_DEBUG_UPDATES,
			"vte_terminal_paint_area"
			"	(%d,%d)x(%d,%d) pixels,"
			" (%d,%d)x(%d,%d) cells"
			" [(%d,%d)x(%d,%d) pixels]\n",
			area->x, area->y, area->width, area->height,
			col, row, col_stop - col, row_stop - row,
			col * width + terminal->pvt->padding.left,
			row * height + terminal->pvt->padding.top,
			(col_stop - col) * width,
			(row_stop - row) * height);

	/* Now we're ready to draw the text.  Iterate over the rows we
	 * need to draw. */
	vte_terminal_draw_rows(terminal,
			      screen,
			      row, row_stop - row,
			      col, col_stop - col,
			      col * width,
			      _vte_terminal_row_to_pixel(terminal, row),
			      width,
			      height);
}

static void
vte_terminal_paint_cursor(VteTerminal *terminal)
{
	const VteCell *cell;
	struct _vte_draw_text_request item;
	int drow, col;
	long width, height, cursor_width;
	guint fore, back;
	PangoColor bg;
	int x, y;
	gboolean blink, selected, focus;

	if (!terminal->pvt->cursor_visible)
		return;

        if (terminal->pvt->im_preedit_active)
                return;

        col = terminal->pvt->cursor.col;
        drow = terminal->pvt->cursor.row;
	width = terminal->pvt->char_width;
	height = terminal->pvt->char_height;

        /* TODOegmont: clamp on rows? tricky... */
	if (CLAMP(col, 0, terminal->pvt->column_count - 1) != col)
		return;

	focus = terminal->pvt->has_focus;
	blink = terminal->pvt->cursor_blink_state;

	if (focus && !blink)
		return;

        /* Find the first cell of the character "under" the cursor.
         * This is for CJK.  For TAB, paint the cursor where it really is. */
	cell = vte_terminal_find_charcell(terminal, col, drow);
        while (cell != NULL && cell->attr.fragment && cell->c != '\t' && col > 0) {
		col--;
		cell = vte_terminal_find_charcell(terminal, col, drow);
	}

	/* Draw the cursor. */
	item.c = (cell && cell->c) ? cell->c : ' ';
	item.columns = item.c == '\t' ? 1 : cell ? cell->attr.columns : 1;
	item.x = col * width;
	item.y = _vte_terminal_row_to_pixel(terminal, drow);
	cursor_width = item.columns * width;
	if (cell && cell->c != 0) {
		guint style;
		gint cw;
		style = _vte_draw_get_style(cell->attr.bold, cell->attr.italic);
		cw = _vte_draw_get_char_width (terminal->pvt->draw, cell->c,
					cell->attr.columns, style);
		cursor_width = MAX(cursor_width, cw);
	}

	selected = vte_cell_is_selected(terminal, col, drow, NULL);

	vte_terminal_determine_cursor_colors(terminal, cell, selected, &fore, &back);
	vte_terminal_get_rgb_from_index(terminal, back, &bg);

	x = item.x;
	y = item.y;

        switch (_vte_terminal_decscusr_cursor_shape(terminal)) {

		case VTE_CURSOR_SHAPE_IBEAM: {
                        int stem_width;

                        stem_width = (int) (((float) height) * terminal->pvt->cursor_aspect_ratio + 0.5);
                        stem_width = CLAMP (stem_width, VTE_LINE_WIDTH, cursor_width);
		 	
			vte_terminal_fill_rectangle(terminal, &bg,
						     x, y, stem_width, height);
			break;
                }

		case VTE_CURSOR_SHAPE_UNDERLINE: {
                        int line_height;

			/* use height (not width) so underline and ibeam will
			 * be equally visible */
                        line_height = (int) (((float) height) * terminal->pvt->cursor_aspect_ratio + 0.5);
                        line_height = CLAMP (line_height, VTE_LINE_WIDTH, height);

			vte_terminal_fill_rectangle(terminal, &bg,
						     x, y + height - line_height,
						     cursor_width, line_height);
			break;
                }

		case VTE_CURSOR_SHAPE_BLOCK:

			if (focus) {
				/* just reverse the character under the cursor */
				vte_terminal_fill_rectangle (terminal,
							     &bg,
							     x, y,
							     cursor_width, height);

                                if (cell && cell->c != 0 && cell->c != ' ') {
                                        vte_terminal_draw_cells(terminal,
                                                        &item, 1,
                                                        fore, back, TRUE, FALSE,
                                                        cell->attr.bold,
                                                        cell->attr.italic,
                                                        cell->attr.underline,
                                                        cell->attr.strikethrough,
                                                        FALSE,
                                                        FALSE,
                                                        width,
                                                        height);
				}

			} else {
				/* draw a box around the character */
				vte_terminal_draw_rectangle (terminal,
							    &bg,
							     x - VTE_LINE_WIDTH,
							     y - VTE_LINE_WIDTH,
							     cursor_width + 2*VTE_LINE_WIDTH,
							     height + 2*VTE_LINE_WIDTH);
			}

			break;
	}
}

static void
vte_terminal_paint_im_preedit_string(VteTerminal *terminal)
{
	int col, columns;
	long width, height;
	int i, len;
	guint fore, back;

	if (!terminal->pvt->im_preedit)
		return;

	/* Keep local copies of rendering information. */
	width = terminal->pvt->char_width;
	height = terminal->pvt->char_height;

	/* Find out how many columns the pre-edit string takes up. */
	columns = vte_terminal_preedit_width(terminal, FALSE);
	len = vte_terminal_preedit_length(terminal, FALSE);

	/* If the pre-edit string won't fit on the screen if we start
	 * drawing it at the cursor's position, move it left. */
        col = terminal->pvt->cursor.col;
	if (col + columns > terminal->pvt->column_count) {
		col = MAX(0, terminal->pvt->column_count - columns);
	}

	/* Draw the preedit string, boxed. */
	if (len > 0) {
		struct _vte_draw_text_request *items;
		const char *preedit = terminal->pvt->im_preedit;
		int preedit_cursor;

		items = g_new(struct _vte_draw_text_request, len);
		for (i = columns = 0; i < len; i++) {
			items[i].c = g_utf8_get_char(preedit);
                        items[i].columns = _vte_unichar_width(items[i].c,
                                                              terminal->pvt->utf8_ambiguous_width);
			items[i].x = (col + columns) * width;
			items[i].y = _vte_terminal_row_to_pixel(terminal, terminal->pvt->cursor.row);
			columns += items[i].columns;
			preedit = g_utf8_next_char(preedit);
		}
		_vte_draw_clear(terminal->pvt->draw,
				col * width + terminal->pvt->padding.left,
				_vte_terminal_row_to_pixel(terminal, terminal->pvt->cursor.row) + terminal->pvt->padding.top,
				width * columns,
				height);
                fore = terminal->pvt->color_defaults.attr.fore;
                back = terminal->pvt->color_defaults.attr.back;
		vte_terminal_draw_cells_with_attributes(terminal,
							items, len,
							terminal->pvt->im_preedit_attrs,
							TRUE,
							width, height);
		preedit_cursor = terminal->pvt->im_preedit_cursor;
		if (preedit_cursor >= 0 && preedit_cursor < len) {
			/* Cursored letter in reverse. */
			vte_terminal_draw_cells(terminal,
						&items[preedit_cursor], 1,
						back, fore, TRUE, TRUE,
						FALSE,
						FALSE,
						FALSE,
						FALSE,
						FALSE,
						TRUE,
						width, height);
		}
		g_free(items);
	}
}

void
VteTerminalPrivate::widget_draw(cairo_t *cr)
{
        cairo_rectangle_int_t clip_rect;
        cairo_region_t *region;
        int allocated_width, allocated_height;
        int extra_area_for_cursor;

        if (!gdk_cairo_get_clip_rectangle (cr, &clip_rect))
                return;

        _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_draw()\n");
        _vte_debug_print (VTE_DEBUG_WORK, "+");
        _vte_debug_print (VTE_DEBUG_UPDATES, "Draw (%d,%d)x(%d,%d)\n",
                          clip_rect.x, clip_rect.y,
                          clip_rect.width, clip_rect.height);

        region = vte_cairo_get_clip_region (cr);
        if (region == NULL)
                return;

        allocated_width = gtk_widget_get_allocated_width(m_widget);
        allocated_height = gtk_widget_get_allocated_height(m_widget);

	/* Designate the start of the drawing operation and clear the area. */
	_vte_draw_set_cairo(m_draw, cr);

	_vte_draw_clear (m_draw, 0, 0,
			 allocated_width, allocated_height);

        /* Clip vertically, for the sake of smooth scrolling. We want the top and bottom paddings to be unused.
         * Don't clip horizontally so that antialiasing can legally overflow to the right padding. */
        cairo_save(cr);
        cairo_rectangle(cr, 0, m_padding.top, allocated_width, allocated_height - m_padding.top - m_padding.bottom);
        cairo_clip(cr);

	/* Calculate the bounding rectangle. */
	{
		cairo_rectangle_int_t *rectangles;
		gint n, n_rectangles;
		n_rectangles = cairo_region_num_rectangles (region);
		rectangles = g_new (cairo_rectangle_int_t, n_rectangles);
		for (n = 0; n < n_rectangles; n++) {
			cairo_region_get_rectangle (region, n, &rectangles[n]);
		}

		/* don't bother to enlarge an invalidate all */
		if (!(n_rectangles == 1
		      && rectangles[0].width == allocated_width
		      && rectangles[0].height == allocated_height)) {
			cairo_region_t *rr = cairo_region_create ();
			/* convert pixels into whole cells */
			for (n = 0; n < n_rectangles; n++) {
				vte_terminal_expand_region (m_terminal, rr, rectangles + n);
			}
			g_free (rectangles);

			n_rectangles = cairo_region_num_rectangles (rr);
			rectangles = g_new (cairo_rectangle_int_t, n_rectangles);
			for (n = 0; n < n_rectangles; n++) {
				cairo_region_get_rectangle (rr, n, &rectangles[n]);
			}
			cairo_region_destroy (rr);
		}

		/* and now paint them */
		for (n = 0; n < n_rectangles; n++) {
			vte_terminal_paint_area (m_terminal, rectangles + n);
		}
		g_free (rectangles);
	}

	vte_terminal_paint_im_preedit_string(m_terminal);

        cairo_restore(cr);

        /* Re-clip, allowing 1 more pixel row for the outline cursor. */
        /* TODOegmont: It's really ugly to do it here. */
        cairo_save(cr);
        extra_area_for_cursor = (_vte_terminal_decscusr_cursor_shape(m_terminal) == VTE_CURSOR_SHAPE_BLOCK && !m_has_focus) ? 1 : 0;
        cairo_rectangle(cr, 0, m_padding.top - extra_area_for_cursor, allocated_width, allocated_height - m_padding.top - m_padding.bottom + 2 * extra_area_for_cursor);
        cairo_clip(cr);

	vte_terminal_paint_cursor(m_terminal);

	cairo_restore(cr);

	/* Done with various structures. */
	_vte_draw_set_cairo(m_draw, NULL);

        cairo_region_destroy (region);

        m_invalidated_all = FALSE;
}

/* Handle an expose event by painting the exposed area. */
static cairo_region_t *
vte_cairo_get_clip_region (cairo_t *cr)
{
        cairo_rectangle_list_t *list;
        cairo_region_t *region;
        int i;

        list = cairo_copy_clip_rectangle_list (cr);
        if (list->status == CAIRO_STATUS_CLIP_NOT_REPRESENTABLE) {
                cairo_rectangle_int_t clip_rect;

                cairo_rectangle_list_destroy (list);

                if (!gdk_cairo_get_clip_rectangle (cr, &clip_rect))
                        return NULL;
                return cairo_region_create_rectangle (&clip_rect);
        }


        region = cairo_region_create ();
        for (i = list->num_rectangles - 1; i >= 0; --i) {
                cairo_rectangle_t *rect = &list->rectangles[i];
                cairo_rectangle_int_t clip_rect;

                clip_rect.x = floor (rect->x);
                clip_rect.y = floor (rect->y);
                clip_rect.width = ceil (rect->x + rect->width) - clip_rect.x;
                clip_rect.height = ceil (rect->y + rect->height) - clip_rect.y;

                if (cairo_region_union_rectangle (region, &clip_rect) != CAIRO_STATUS_SUCCESS) {
                        cairo_region_destroy (region);
                        region = NULL;
                        break;
                }
        }

        cairo_rectangle_list_destroy (list);
        return region;
}

void
VteTerminalPrivate::widget_scroll(GdkEventScroll *event)
{
	gdouble delta_x, delta_y;
	gdouble v;
	gint cnt, i;
	int button;

	read_modifiers((GdkEvent*)event);

	switch (event->direction) {
	case GDK_SCROLL_UP:
		m_mouse_smooth_scroll_delta -= 1.;
		_vte_debug_print(VTE_DEBUG_EVENTS, "Scroll up\n");
		break;
	case GDK_SCROLL_DOWN:
		m_mouse_smooth_scroll_delta += 1.;
		_vte_debug_print(VTE_DEBUG_EVENTS, "Scroll down\n");
		break;
	case GDK_SCROLL_SMOOTH:
		gdk_event_get_scroll_deltas ((GdkEvent*) event, &delta_x, &delta_y);
		m_mouse_smooth_scroll_delta += delta_y;
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Smooth scroll by %f, delta now at %f\n",
				delta_y, m_mouse_smooth_scroll_delta);
		break;
	default:
		break;
	}

	/* If we're running a mouse-aware application, map the scroll event
	 * to a button press on buttons four and five. */
	if (m_mouse_tracking_mode) {
		cnt = m_mouse_smooth_scroll_delta;
		if (cnt == 0)
			return;
		m_mouse_smooth_scroll_delta -= cnt;
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Scroll application by %d lines, smooth scroll delta set back to %f\n",
				cnt, m_mouse_smooth_scroll_delta);

		button = cnt > 0 ? 5 : 4;
		if (cnt < 0)
			cnt = -cnt;
		for (i = 0; i < cnt; i++) {
			/* Encode the parameters and send them to the app. */
			send_mouse_button_internal(
								button,
								false /* not release */,
								event->x,
								event->y);
		}
		return;
	}

	v = MAX (1., ceil (gtk_adjustment_get_page_increment (m_vadjustment) / 10.));
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Scroll speed is %d lines per non-smooth scroll unit\n",
			(int) v);
	if (m_screen == &m_alternate_screen &&
            m_alternate_screen_scroll) {
		char *normal;
		gssize normal_length;

		cnt = v * m_mouse_smooth_scroll_delta;
		if (cnt == 0)
			return;
		m_mouse_smooth_scroll_delta -= cnt / v;
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Scroll by %d lines, smooth scroll delta set back to %f\n",
				cnt, m_mouse_smooth_scroll_delta);

		/* In the alternate screen there is no scrolling,
		 * so fake a few cursor keystrokes. */

		_vte_keymap_map (
				cnt > 0 ? GDK_KEY_Down : GDK_KEY_Up,
				m_modifiers,
				m_cursor_mode == VTE_KEYMODE_APPLICATION,
				m_keypad_mode == VTE_KEYMODE_APPLICATION,
				&normal,
				&normal_length);
		if (cnt < 0)
			cnt = -cnt;
		for (i = 0; i < cnt; i++) {
			vte_terminal_feed_child_using_modes (m_terminal,
					normal, normal_length);
		}
		g_free (normal);
	} else {
		/* Perform a history scroll. */
		double dcnt = m_screen->scroll_delta + v * m_mouse_smooth_scroll_delta;
		vte_terminal_queue_adjustment_value_changed_clamped (m_terminal, dcnt);
		m_mouse_smooth_scroll_delta = 0;
	}
}

bool
VteTerminalPrivate::set_audible_bell(bool setting)
{
        if (setting == m_audible_bell)
                return false;

	m_audible_bell = setting;
        return true;
}

bool
VteTerminalPrivate::set_allow_bold(bool setting)
{
        if (setting == m_allow_bold)
                return false;

	m_allow_bold = setting;
	invalidate_all();

        return true;
}

bool
VteTerminalPrivate::set_scroll_on_output(bool scroll)
{
        if (scroll == m_scroll_on_output)
                return false;

        m_scroll_on_output = scroll;
        return true;
}

bool
VteTerminalPrivate::set_scroll_on_keystroke(bool scroll)
{
        if (scroll == m_scroll_on_keystroke)
                return false;

        m_scroll_on_keystroke = scroll;
        return true;
}

bool
VteTerminalPrivate::set_rewrap_on_resize(bool rewrap)
{
        if (rewrap == m_rewrap_on_resize)
                return false;

        m_rewrap_on_resize = rewrap;
        return true;
}

/* Set up whatever background we wanted. */
static void
vte_terminal_background_update(VteTerminal *terminal)
{
	const PangoColor *entry;
	GdkRGBA color;

	/* If we're not realized yet, don't worry about it, because we get
	 * called when we realize. */
	if (! gtk_widget_get_realized (&terminal->widget)) {
		return;
	}

	_vte_debug_print(VTE_DEBUG_MISC|VTE_DEBUG_EVENTS,
			"Updating background color.\n");

	entry = _vte_terminal_get_color(terminal, VTE_DEFAULT_BG);
	_vte_debug_print(VTE_DEBUG_STYLE,
			 "Setting background color to (%d, %d, %d, %.3f).\n",
			 entry->red, entry->green, entry->blue,
			 terminal->pvt->background_alpha);

	color.red = entry->red / 65535.;
	color.green = entry->green / 65535.;
	color.blue = entry->blue / 65535.;
        color.alpha = terminal->pvt->background_alpha;

        _vte_draw_set_background_solid (terminal->pvt->draw, &color);

	/* Force a redraw for everything. */
	_vte_invalidate_all (terminal);
}

static void
vte_terminal_update_cursor_blinks_internal(VteTerminal *terminal)
{
        VteTerminalPrivate *pvt = terminal->pvt;
        gboolean blink = FALSE;

        switch (_vte_terminal_decscusr_cursor_blink(terminal)) {
        case VTE_CURSOR_BLINK_SYSTEM:
                g_object_get(gtk_widget_get_settings(GTK_WIDGET(terminal)),
                                                     "gtk-cursor-blink",
                                                     &blink, NULL);
                break;
        case VTE_CURSOR_BLINK_ON:
                blink = TRUE;
                break;
        case VTE_CURSOR_BLINK_OFF:
                blink = FALSE;
                break;
        }

	if (pvt->cursor_blinks == blink)
		return;

	pvt->cursor_blinks = blink;
	pvt->check_cursor_blink();
}

bool
VteTerminalPrivate::set_cursor_blink_mode(VteCursorBlinkMode mode)
{
        if (mode == m_cursor_blink_mode)
                return false;

        m_cursor_blink_mode = mode;
        vte_terminal_update_cursor_blinks_internal(m_terminal);

        return true;
}

bool
VteTerminalPrivate::set_cursor_shape(VteCursorShape shape)
{
        if (shape == m_cursor_shape)
                return false;

        m_cursor_shape = shape;
	invalidate_cursor_once();

        return true;
}

/* DECSCUSR set cursor style */
void
_vte_terminal_set_cursor_style(VteTerminal *terminal, VteCursorStyle style)
{
        VteTerminalPrivate *pvt;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        pvt = terminal->pvt;

        if (pvt->cursor_style == style)
                return;

        pvt->cursor_style = style;

        vte_terminal_update_cursor_blinks_internal(terminal);

        /* and this will also make cursor shape match the DECSCUSR style */
        _vte_invalidate_cursor_once(terminal, FALSE);
}

/*
 * _vte_terminal_decscusr_cursor_blink:
 * @terminal: a #VteTerminal
 *
 * Returns the cursor blink mode set by DECSCUSR. If DECSCUSR was never
 * called, or it set the blink mode to terminal default, this returns the
 * value set via API or in dconf. Internal use only.
 *
 * Return value: cursor blink mode
 */
static VteCursorBlinkMode
_vte_terminal_decscusr_cursor_blink(VteTerminal *terminal)
{
        switch (terminal->pvt->cursor_style) {
        default:
        case VTE_CURSOR_STYLE_TERMINAL_DEFAULT:
                return terminal->pvt->cursor_blink_mode;
        case VTE_CURSOR_STYLE_BLINK_BLOCK:
        case VTE_CURSOR_STYLE_BLINK_UNDERLINE:
        case VTE_CURSOR_STYLE_BLINK_IBEAM:
                return VTE_CURSOR_BLINK_ON;
        case VTE_CURSOR_STYLE_STEADY_BLOCK:
        case VTE_CURSOR_STYLE_STEADY_UNDERLINE:
        case VTE_CURSOR_STYLE_STEADY_IBEAM:
                return VTE_CURSOR_BLINK_OFF;
        }
}

/*
 * _vte_terminal_decscusr_cursor_shape:
 * @terminal: a #VteTerminal
 *
 * Returns the cursor shape set by DECSCUSR. If DECSCUSR was never called,
 * or it set the cursor shape to terminal default, this returns the value
 * set via API. Internal use only.
 *
 * Return value: cursor shape
 */
static VteCursorShape
_vte_terminal_decscusr_cursor_shape(VteTerminal *terminal)
{
        switch (terminal->pvt->cursor_style) {
        default:
        case VTE_CURSOR_STYLE_TERMINAL_DEFAULT:
                return terminal->pvt->cursor_shape;
        case VTE_CURSOR_STYLE_BLINK_BLOCK:
        case VTE_CURSOR_STYLE_STEADY_BLOCK:
                return VTE_CURSOR_SHAPE_BLOCK;
        case VTE_CURSOR_STYLE_BLINK_UNDERLINE:
        case VTE_CURSOR_STYLE_STEADY_UNDERLINE:
                return VTE_CURSOR_SHAPE_UNDERLINE;
        case VTE_CURSOR_STYLE_BLINK_IBEAM:
        case VTE_CURSOR_STYLE_STEADY_IBEAM:
                return VTE_CURSOR_SHAPE_IBEAM;
        }
}

bool
VteTerminalPrivate::set_scrollback_lines(long lines)
{
        glong low, high, next;
        double scroll_delta;
	VteScreen *scrn;

	if (lines < 0)
		lines = G_MAXLONG;

#if 0
        /* FIXME: this breaks the scrollbar range, bug #562511 */
        if (lines == m_scrollback_lines)
                return false;
#endif

	_vte_debug_print (VTE_DEBUG_MISC,
			"Setting scrollback lines to %ld\n", lines);

	m_scrollback_lines = lines;

        /* The main screen gets the full scrollback buffer. */
        scrn = &m_normal_screen;
        lines = MAX (lines, m_row_count);
        next = MAX (m_cursor.row + 1,
                    _vte_ring_next (scrn->row_data));
        _vte_ring_resize (scrn->row_data, lines);
        low = _vte_ring_delta (scrn->row_data);
        high = lines + MIN (G_MAXLONG - lines, low - m_row_count + 1);
        scrn->insert_delta = CLAMP (scrn->insert_delta, low, high);
        scrn->scroll_delta = CLAMP (scrn->scroll_delta, low, scrn->insert_delta);
        next = MIN (next, scrn->insert_delta + m_row_count);
        if (_vte_ring_next (scrn->row_data) > next){
                _vte_ring_shrink (scrn->row_data, next - low);
        }

        /* The alternate scrn isn't allowed to scroll at all. */
        scrn = &m_alternate_screen;
        _vte_ring_resize (scrn->row_data, m_row_count);
        scrn->scroll_delta = _vte_ring_delta (scrn->row_data);
        scrn->insert_delta = _vte_ring_delta (scrn->row_data);
        if (_vte_ring_next (scrn->row_data) > scrn->insert_delta + m_row_count){
                _vte_ring_shrink (scrn->row_data, m_row_count);
        }

	/* Adjust the scrollbar to the new location. */
	/* Hack: force a change in scroll_delta even if the value remains, so that
	   vte_term_q_adj_val_changed() doesn't shortcut to no-op, see bug 676075. */
        scroll_delta = m_screen->scroll_delta;
	m_screen->scroll_delta = -1;
	vte_terminal_queue_adjustment_value_changed(m_terminal, scroll_delta);
	_vte_terminal_adjust_adjustments_full(m_terminal);

        return true;
}

bool
VteTerminalPrivate::set_backspace_binding(VteEraseBinding binding)
{
        if (binding == m_backspace_binding)
                return false;

	m_backspace_binding = binding;
        return true;
}

bool
VteTerminalPrivate::set_delete_binding(VteEraseBinding binding)
{
        if (binding == m_delete_binding)
                return false;

	m_delete_binding = binding;
        return true;
}

bool
VteTerminalPrivate::set_mouse_autohide(bool autohide)
{
        if (autohide == m_mouse_autohide)
                return false;

	m_mouse_autohide = autohide;
        /* FIXME: show mouse now if autohide=false! */
        return true;
}

/**
 * vte_terminal_reset:
 * @terminal: a #VteTerminal
 * @clear_tabstops: whether to reset tabstops
 * @clear_history: whether to empty the terminal's scrollback buffer
 *
 * Resets as much of the terminal's internal state as possible, discarding any
 * unprocessed input data, resetting character attributes, cursor state,
 * national character set state, status line, terminal modes (insert/delete),
 * selection state, and encoding.
 *
 */
void
vte_terminal_reset(VteTerminal *terminal,
                   gboolean clear_tabstops,
                   gboolean clear_history)
{
        VteTerminalPrivate *pvt;
        int i, sel;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        g_object_freeze_notify(G_OBJECT(terminal));

	/* Clear the output buffer. */
	_vte_byte_array_clear(pvt->outgoing);
	/* Reset charset substitution state. */
	_vte_iso2022_state_free(pvt->iso2022);
        pvt->iso2022 = _vte_iso2022_state_new(NULL);
	_vte_iso2022_state_set_codeset(pvt->iso2022,
				       pvt->encoding);
	/* Reset keypad/cursor key modes. */
	pvt->keypad_mode = VTE_KEYMODE_NORMAL;
	pvt->cursor_mode = VTE_KEYMODE_NORMAL;
        /* Enable autowrap. */
        pvt->autowrap = TRUE;
	/* Enable meta-sends-escape. */
	pvt->meta_sends_escape = TRUE;
	/* Disable margin bell. */
	pvt->margin_bell = FALSE;
        /* Disable DECCOLM mode. */
        pvt->deccolm_mode = FALSE;
	/* Reset saved settings. */
	if (pvt->dec_saved != NULL) {
		g_hash_table_destroy(pvt->dec_saved);
		pvt->dec_saved = g_hash_table_new(NULL, NULL);
	}
	/* Reset the color palette. Only the 256 indexed colors, not the special ones, as per xterm. */
	for (i = 0; i < 256; i++)
		terminal->pvt->palette[i].sources[VTE_COLOR_SOURCE_ESCAPE].is_set = FALSE;
	/* Reset the default attributes.  Reset the alternate attribute because
	 * it's not a real attribute, but we need to treat it as one here. */
	_vte_terminal_set_default_attributes(terminal);
        /* Reset charset modes. */
        pvt->character_replacements[0] = VTE_CHARACTER_REPLACEMENT_NONE;
        pvt->character_replacements[1] = VTE_CHARACTER_REPLACEMENT_NONE;
        pvt->character_replacement = &pvt->character_replacements[0];
	/* Clear the scrollback buffers and reset the cursors. Switch to normal screen. */
	if (clear_history) {
                pvt->screen = &pvt->normal_screen;
                pvt->normal_screen.scroll_delta = pvt->normal_screen.insert_delta =
                        _vte_ring_reset(pvt->normal_screen.row_data);
                pvt->alternate_screen.scroll_delta = pvt->alternate_screen.insert_delta =
                        _vte_ring_reset(pvt->alternate_screen.row_data);
                pvt->cursor.row = pvt->screen->insert_delta;
                pvt->cursor.col = 0;
                /* Adjust the scrollbar to the new location. */
                /* Hack: force a change in scroll_delta even if the value remains, so that
                   vte_term_q_adj_val_changed() doesn't shortcut to no-op, see bug 730599. */
                pvt->screen->scroll_delta = -1;
                vte_terminal_queue_adjustment_value_changed (terminal, pvt->screen->insert_delta);
		_vte_terminal_adjust_adjustments_full (terminal);
	}
        /* DECSCUSR cursor style */
        pvt->cursor_style = VTE_CURSOR_STYLE_TERMINAL_DEFAULT;
	/* Do more stuff we refer to as a "full" reset. */
	if (clear_tabstops) {
		vte_terminal_set_default_tabstops(terminal);
	}
	/* Reset restricted scrolling regions, leave insert mode, make
	 * the cursor visible again. */
        pvt->scrolling_restricted = FALSE;
        pvt->sendrecv_mode = TRUE;
        pvt->insert_mode = FALSE;
        pvt->linefeed_mode = FALSE;
        pvt->origin_mode = FALSE;
        pvt->reverse_mode = FALSE;
	pvt->cursor_visible = TRUE;
        /* For some reason, xterm doesn't reset alternateScroll, but we do. */
        pvt->alternate_screen_scroll = TRUE;
	/* Reset the encoding. */
	vte_terminal_set_encoding(terminal, NULL /* UTF-8 */, NULL);
	g_assert_cmpstr(pvt->encoding, ==, "UTF-8");
	/* Reset selection. */
	pvt->deselect_all();
	pvt->has_selection = FALSE;
	pvt->selecting = FALSE;
	pvt->selecting_restart = FALSE;
	pvt->selecting_had_delta = FALSE;
	for (sel = VTE_SELECTION_PRIMARY; sel < LAST_VTE_SELECTION; sel++) {
		if (pvt->selection_text[sel] != NULL) {
			g_free(pvt->selection_text[sel]);
			pvt->selection_text[sel] = NULL;
#ifdef HTML_SELECTION
			g_free(pvt->selection_html[sel]);
			pvt->selection_html[sel] = NULL;
#endif
		}
	}
        memset(&pvt->selection_origin, 0,
               sizeof(pvt->selection_origin));
        memset(&pvt->selection_last, 0,
               sizeof(pvt->selection_last));
        memset(&pvt->selection_start, 0,
               sizeof(pvt->selection_start));
        memset(&pvt->selection_end, 0,
               sizeof(pvt->selection_end));

	/* Reset mouse motion events. */
	pvt->mouse_tracking_mode = MOUSE_TRACKING_NONE;
        pvt->mouse_pressed_buttons = 0;
        pvt->mouse_handled_buttons = 0;
	pvt->mouse_last_x = 0;
	pvt->mouse_last_y = 0;
        pvt->mouse_last_col = 0;
        pvt->mouse_last_row = 0;
	pvt->mouse_xterm_extension = FALSE;
	pvt->mouse_urxvt_extension = FALSE;
	pvt->mouse_smooth_scroll_delta = 0.;
        /* Reset focus tracking */
        pvt->focus_tracking_mode = FALSE;
	/* Clear modifiers. */
	pvt->modifiers = 0;
	/* Reset miscellaneous stuff. */
	pvt->bracketed_paste_mode = FALSE;
        /* Reset the saved cursor. */
        _vte_terminal_save_cursor(terminal, &terminal->pvt->normal_screen);
        _vte_terminal_save_cursor(terminal, &terminal->pvt->alternate_screen);
	/* Cause everything to be redrawn (or cleared). */
	vte_terminal_maybe_scroll_to_bottom(terminal);
	_vte_invalidate_all(terminal);

        g_object_thaw_notify(G_OBJECT(terminal));
}

bool
VteTerminalPrivate::set_pty(VtePty *new_pty)
{
        if (new_pty == m_pty)
                return false;

        if (m_pty != NULL) {
                _vte_terminal_disconnect_pty_read(m_terminal);
                _vte_terminal_disconnect_pty_write(m_terminal);

                if (m_pty_channel != NULL) {
                        g_io_channel_unref (m_pty_channel);
                        m_pty_channel = NULL;
                }

		/* Take one last shot at processing whatever data is pending,
		 * then flush the buffers in case we're about to run a new
		 * command, disconnecting the timeout. */
		if (m_incoming != NULL) {
			vte_terminal_process_incoming(m_terminal);
			_vte_incoming_chunks_release (m_incoming);
			m_incoming = NULL;
			m_input_bytes = 0;
		}
		g_array_set_size(m_pending, 0);
		vte_terminal_stop_processing(m_terminal);

		/* Clear the outgoing buffer as well. */
		_vte_byte_array_clear(m_outgoing);

                g_object_unref(m_pty);
                m_pty = NULL;
        }

        if (new_pty == NULL) {
                m_pty = NULL;
                return true;
        }

        m_pty = (VtePty *)g_object_ref(new_pty);
        int pty_master = vte_pty_get_fd(m_pty);

        m_pty_channel = g_io_channel_unix_new(pty_master);
        g_io_channel_set_close_on_unref(m_pty_channel, FALSE);

        /* FIXMEchpe: vte_pty_open_unix98 does the inverse ... */
        /* Set the pty to be non-blocking. */
        long flags = fcntl(pty_master, F_GETFL);
        if ((flags & O_NONBLOCK) == 0) {
                fcntl(pty_master, F_SETFL, flags | O_NONBLOCK);
        }

        vte_terminal_set_size(m_terminal,
                              m_column_count,
                              m_row_count);

        _vte_terminal_setup_utf8(m_terminal);

        /* Open channels to listen for input on. */
        _vte_terminal_connect_pty_read(m_terminal);

        return true;
}

/* We need this bit of glue to ensure that accessible objects will always
 * get signals. */
void
_vte_terminal_accessible_ref(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->accessible_emit = TRUE;
}

char *
_vte_terminal_get_selection(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);

	return g_strdup (terminal->pvt->selection_text[VTE_SELECTION_PRIMARY]);
}

void
_vte_terminal_get_start_selection(VteTerminal *terminal, long *col, long *row)
{
	VteVisualPosition ss;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	ss = terminal->pvt->selection_start;

	if (col) {
		*col = ss.col;
	}

	if (row) {
		*row = ss.row;
	}
}

void
_vte_terminal_get_end_selection(VteTerminal *terminal, long *col, long *row)
{
	VteVisualPosition se;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	se = terminal->pvt->selection_end;

	if (col) {
		*col = se.col;
	}

	if (row) {
		*row = se.row;
	}
}

void
_vte_terminal_select_text(VteTerminal *terminal,
			  long start_col, long start_row,
			  long end_col, long end_row,
			  int start_offset, int end_offset)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	terminal->pvt->deselect_all();

	terminal->pvt->selection_type = selection_type_char;
	terminal->pvt->selecting_had_delta = TRUE;
	terminal->pvt->selection_start.col = start_col;
	terminal->pvt->selection_start.row = start_row;
	terminal->pvt->selection_end.col = end_col;
	terminal->pvt->selection_end.row = end_row;
	vte_terminal_copy_primary(terminal);
	vte_terminal_emit_selection_changed(terminal);

	_vte_invalidate_region (terminal,
			MIN (start_col, end_col), MAX (start_col, end_col),
			MIN (start_row, end_row), MAX (start_row, end_row),
			FALSE);

}

void
_vte_terminal_remove_selection(VteTerminal *terminal)
{
	terminal->pvt->deselect_all();
}

static void
_vte_terminal_select_empty_at(VteTerminal *terminal,
			      long col, long row)
{
	_vte_terminal_select_text(terminal, col, row, col - 1, row, 0, 0);
}

static void
add_update_timeout (VteTerminal *terminal)
{
	if (update_timeout_tag == 0) {
		_vte_debug_print (VTE_DEBUG_TIMEOUT,
				"Starting update timeout\n");
		update_timeout_tag =
			g_timeout_add_full (GDK_PRIORITY_REDRAW,
					VTE_UPDATE_TIMEOUT,
					update_timeout, NULL,
					NULL);
	}
	if (in_process_timeout == FALSE &&
			process_timeout_tag != 0) {
		_vte_debug_print (VTE_DEBUG_TIMEOUT,
				"Removing process timeout\n");
		g_source_remove (process_timeout_tag);
		process_timeout_tag = 0;
	}
	if (terminal->pvt->active == NULL) {
		_vte_debug_print (VTE_DEBUG_TIMEOUT,
				"Adding terminal to active list\n");
		terminal->pvt->active = active_terminals =
			g_list_prepend (active_terminals, terminal);
	}

}
static void
reset_update_regions (VteTerminal *terminal)
{
	if (terminal->pvt->update_regions != NULL) {
		g_slist_foreach (terminal->pvt->update_regions,
				(GFunc)cairo_region_destroy, NULL);
		g_slist_free (terminal->pvt->update_regions);
		terminal->pvt->update_regions = NULL;
	}
	/* the invalidated_all flag also marks whether to skip processing
	 * due to the widget being invisible */
	terminal->pvt->invalidated_all =
		terminal->pvt->visibility_state==GDK_VISIBILITY_FULLY_OBSCURED;
}

static void
remove_from_active_list (VteTerminal *terminal)
{
	if (terminal->pvt->active != NULL
			&& terminal->pvt->update_regions == NULL) {
		_vte_debug_print(VTE_DEBUG_TIMEOUT,
			"Removing terminal from active list\n");
		active_terminals = g_list_delete_link (active_terminals,
				terminal->pvt->active);
		terminal->pvt->active = NULL;

		if (active_terminals == NULL) {
			if (in_process_timeout == FALSE &&
					process_timeout_tag != 0) {
				_vte_debug_print(VTE_DEBUG_TIMEOUT,
						"Removing process timeout\n");
				g_source_remove (process_timeout_tag);
				process_timeout_tag = 0;
			}
			if (in_update_timeout == FALSE &&
					update_timeout_tag != 0) {
				_vte_debug_print(VTE_DEBUG_TIMEOUT,
						"Removing update timeout\n");
				g_source_remove (update_timeout_tag);
				update_timeout_tag = 0;
			}
		}
	}
}
static void
remove_update_timeout (VteTerminal *terminal)
{
	reset_update_regions (terminal);
	remove_from_active_list (terminal);
}

static void
vte_terminal_add_process_timeout (VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_TIMEOUT,
			"Adding terminal to active list\n");
	terminal->pvt->active = active_terminals =
		g_list_prepend (active_terminals, terminal);
	if (update_timeout_tag == 0 &&
			process_timeout_tag == 0) {
		_vte_debug_print(VTE_DEBUG_TIMEOUT,
				"Starting process timeout\n");
		process_timeout_tag =
			g_timeout_add (VTE_DISPLAY_TIMEOUT,
					process_timeout, NULL);
	}
}
static inline gboolean
vte_terminal_is_processing (VteTerminal *terminal)
{
	return terminal->pvt->active != NULL;
}
static inline void
vte_terminal_start_processing (VteTerminal *terminal)
{
	if (!vte_terminal_is_processing (terminal)) {
		vte_terminal_add_process_timeout (terminal);
	}
}

static void
vte_terminal_stop_processing (VteTerminal *terminal)
{
	remove_from_active_list (terminal);
}

static inline gboolean
need_processing (VteTerminal *terminal)
{
	return _vte_incoming_chunks_length (terminal->pvt->incoming) != 0;
}

/* Emit an "icon-title-changed" signal. */
static void
vte_terminal_emit_icon_title_changed(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `icon-title-changed'.\n");
	g_signal_emit_by_name(terminal, "icon-title-changed");
}

/* Emit a "window-title-changed" signal. */
static void
vte_terminal_emit_window_title_changed(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `window-title-changed'.\n");
	g_signal_emit_by_name(terminal, "window-title-changed");
}

static void
vte_terminal_emit_current_directory_uri_changed(VteTerminal *terminal)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `current-directory-uri-changed'.\n");
        g_signal_emit_by_name(terminal, "current-directory-uri-changed");
}

static void
vte_terminal_emit_current_file_uri_changed(VteTerminal *terminal)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `current-file-uri-changed'.\n");
        g_signal_emit_by_name(terminal, "current-file-uri-changed");
}

static void
vte_terminal_emit_pending_signals(VteTerminal *terminal)
{
        GObject *object;
	GdkWindow *window;

	object = G_OBJECT (terminal);
	window = gtk_widget_get_window (&terminal->widget);

        g_object_freeze_notify(object);

	vte_terminal_emit_adjustment_changed (terminal);

	if (terminal->pvt->window_title_changed) {
		g_free (terminal->pvt->window_title);
		terminal->pvt->window_title = terminal->pvt->window_title_changed;
		terminal->pvt->window_title_changed = NULL;

		if (window)
			gdk_window_set_title (window, terminal->pvt->window_title);
		vte_terminal_emit_window_title_changed(terminal);
                g_object_notify(object, "window-title");
	}

	if (terminal->pvt->icon_title_changed) {
		g_free (terminal->pvt->icon_title);
		terminal->pvt->icon_title = terminal->pvt->icon_title_changed;
		terminal->pvt->icon_title_changed = NULL;

		if (window)
			gdk_window_set_icon_name (window, terminal->pvt->icon_title);
		vte_terminal_emit_icon_title_changed(terminal);
                g_object_notify(object, "icon-title");
	}

	if (terminal->pvt->current_directory_uri_changed) {
                g_free (terminal->pvt->current_directory_uri);
                terminal->pvt->current_directory_uri = terminal->pvt->current_directory_uri_changed;
                terminal->pvt->current_directory_uri_changed = NULL;

                vte_terminal_emit_current_directory_uri_changed(terminal);
                g_object_notify(object, "current-directory-uri");
        }

        if (terminal->pvt->current_file_uri_changed) {
                g_free (terminal->pvt->current_file_uri);
                terminal->pvt->current_file_uri = terminal->pvt->current_file_uri_changed;
                terminal->pvt->current_file_uri_changed = NULL;

                vte_terminal_emit_current_file_uri_changed(terminal);
                g_object_notify(object, "current-file-uri");
        }

	/* Flush any pending "inserted" signals. */
	vte_terminal_emit_cursor_moved(terminal);
	vte_terminal_emit_pending_text_signals(terminal);
	vte_terminal_emit_contents_changed (terminal);

        g_object_thaw_notify(object);
}

static void time_process_incoming (VteTerminal *terminal)
{
	gdouble elapsed;
	glong target;
	g_timer_reset (process_timer);
	vte_terminal_process_incoming (terminal);
	elapsed = g_timer_elapsed (process_timer, NULL) * 1000;
	target = VTE_MAX_PROCESS_TIME / elapsed * terminal->pvt->input_bytes;
	terminal->pvt->max_input_bytes =
		(terminal->pvt->max_input_bytes + target) / 2;
}


/* This function is called after DISPLAY_TIMEOUT ms.
 * It makes sure initial output is never delayed by more than DISPLAY_TIMEOUT
 */
static gboolean
process_timeout (gpointer data)
{
	GList *l, *next;
	gboolean again;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	gdk_threads_enter();
        G_GNUC_END_IGNORE_DEPRECATIONS;

	in_process_timeout = TRUE;

	_vte_debug_print (VTE_DEBUG_WORK, "<");
	_vte_debug_print (VTE_DEBUG_TIMEOUT,
			"Process timeout:  %d active\n",
			g_list_length (active_terminals));

	for (l = active_terminals; l != NULL; l = next) {
		VteTerminal *terminal = (VteTerminal *)l->data;
		gboolean active = FALSE;

		next = g_list_next (l);

		if (l != active_terminals) {
			_vte_debug_print (VTE_DEBUG_WORK, "T");
		}
		if (terminal->pvt->pty_channel != NULL) {
			if (terminal->pvt->pty_input_active ||
					terminal->pvt->pty_input_source == 0) {
				terminal->pvt->pty_input_active = FALSE;
				vte_terminal_io_read (terminal->pvt->pty_channel,
						G_IO_IN, terminal);
			}
			_vte_terminal_enable_input_source (terminal);
		}
		if (need_processing (terminal)) {
			active = TRUE;
			if (VTE_MAX_PROCESS_TIME) {
				time_process_incoming (terminal);
			} else {
				vte_terminal_process_incoming(terminal);
			}
			terminal->pvt->input_bytes = 0;
		} else
			vte_terminal_emit_pending_signals (terminal);
		if (!active && terminal->pvt->update_regions == NULL) {
			if (terminal->pvt->active != NULL) {
				_vte_debug_print(VTE_DEBUG_TIMEOUT,
						"Removing terminal from active list [process]\n");
				active_terminals = g_list_delete_link (
						active_terminals,
						terminal->pvt->active);
				terminal->pvt->active = NULL;
			}
		}
	}

	_vte_debug_print (VTE_DEBUG_WORK, ">");

	if (active_terminals && update_timeout_tag == 0) {
		again = TRUE;
	} else {
		_vte_debug_print(VTE_DEBUG_TIMEOUT,
				"Stopping process timeout\n");
		process_timeout_tag = 0;
		again = FALSE;
	}

	in_process_timeout = FALSE;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	gdk_threads_leave();
        G_GNUC_END_IGNORE_DEPRECATIONS;

	if (again) {
		/* Force us to relinquish the CPU as the child is running
		 * at full tilt and making us run to keep up...
		 */
		g_usleep (0);
	} else if (update_timeout_tag == 0) {
		/* otherwise free up memory used to capture incoming data */
		prune_chunks (10);
	}

	return again;
}


static gboolean
update_regions (VteTerminal *terminal)
{
	GSList *l;
	cairo_region_t *region;
	GdkWindow *window;

        if (G_UNLIKELY(!gtk_widget_get_realized(&terminal->widget)))
                return FALSE;
	if (terminal->pvt->visibility_state == GDK_VISIBILITY_FULLY_OBSCURED) {
		reset_update_regions (terminal);
		return FALSE;
	}

	if (G_UNLIKELY (!terminal->pvt->update_regions))
		return FALSE;


	l = terminal->pvt->update_regions;
	if (g_slist_next (l) != NULL) {
		/* amalgamate into one super-region */
		region = cairo_region_create ();
		do {
                        cairo_region_t *r = (cairo_region_t *)l->data;
			cairo_region_union (region, r);
			cairo_region_destroy (r);
		} while ((l = g_slist_next (l)) != NULL);
	} else {
		region = (cairo_region_t *)l->data;
	}
	g_slist_free (terminal->pvt->update_regions);
	terminal->pvt->update_regions = NULL;
	terminal->pvt->invalidated_all = FALSE;

	/* and perform the merge with the window visible area */
	window = gtk_widget_get_window (&terminal->widget);
	gdk_window_invalidate_region (window, region, FALSE);
	gdk_window_process_updates (window, FALSE);
	cairo_region_destroy (region);

	_vte_debug_print (VTE_DEBUG_WORK, "-");

	return TRUE;
}

static gboolean
update_repeat_timeout (gpointer data)
{
	GList *l, *next;
	gboolean again;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	gdk_threads_enter();
        G_GNUC_END_IGNORE_DEPRECATIONS;

	in_update_timeout = TRUE;

	_vte_debug_print (VTE_DEBUG_WORK, "[");
	_vte_debug_print (VTE_DEBUG_TIMEOUT,
			"Repeat timeout:  %d active\n",
			g_list_length (active_terminals));

	for (l = active_terminals; l != NULL; l = next) {
		VteTerminal *terminal = (VteTerminal *)l->data;

		next = g_list_next (l);

		if (l != active_terminals) {
			_vte_debug_print (VTE_DEBUG_WORK, "T");
		}
		if (terminal->pvt->pty_channel != NULL) {
			if (terminal->pvt->pty_input_active ||
					terminal->pvt->pty_input_source == 0) {
				terminal->pvt->pty_input_active = FALSE;
				vte_terminal_io_read (terminal->pvt->pty_channel,
						G_IO_IN, terminal);
			}
			_vte_terminal_enable_input_source (terminal);
		}
		vte_terminal_emit_adjustment_changed (terminal);
		if (need_processing (terminal)) {
			if (VTE_MAX_PROCESS_TIME) {
				time_process_incoming (terminal);
			} else {
				vte_terminal_process_incoming (terminal);
			}
			terminal->pvt->input_bytes = 0;
		} else
			vte_terminal_emit_pending_signals (terminal);

		again = update_regions (terminal);
		if (!again) {
			if (terminal->pvt->active != NULL) {
				_vte_debug_print(VTE_DEBUG_TIMEOUT,
						"Removing terminal from active list [update]\n");
				active_terminals = g_list_delete_link (
						active_terminals,
						terminal->pvt->active);
				terminal->pvt->active = NULL;
			}
		}
	}


	if (active_terminals != NULL) {
		/* remove the idle source, and draw non-Terminals
		 * (except for gdk/{directfb,quartz}!)
		 */
		gdk_window_process_all_updates ();
	}

	_vte_debug_print (VTE_DEBUG_WORK, "]");

	/* We only stop the timer if no update request was received in this
         * past cycle.  Technically, always stop this timer object and maybe
         * reinstall a new one because we need to delay by the amount of time
         * it took to repaint the screen: bug 730732.
	 */
	if (active_terminals == NULL) {
		_vte_debug_print(VTE_DEBUG_TIMEOUT,
				"Stopping update timeout\n");
		update_timeout_tag = 0;
		again = FALSE;
        } else {
                update_timeout_tag =
                        g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
                                            VTE_UPDATE_REPEAT_TIMEOUT,
                                            update_repeat_timeout, NULL,
                                            NULL);
                again = TRUE;
	}

	in_update_timeout = FALSE;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	gdk_threads_leave();
        G_GNUC_END_IGNORE_DEPRECATIONS;

	if (again) {
		/* Force us to relinquish the CPU as the child is running
		 * at full tilt and making us run to keep up...
		 */
		g_usleep (0);
	} else {
		/* otherwise free up memory used to capture incoming data */
		prune_chunks (10);
	}

        return FALSE;  /* If we need to go again, we already have a new timer for that. */
}

static gboolean
update_timeout (gpointer data)
{
	GList *l, *next;
	gboolean redraw = FALSE;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	gdk_threads_enter();
        G_GNUC_END_IGNORE_DEPRECATIONS;

	in_update_timeout = TRUE;

	_vte_debug_print (VTE_DEBUG_WORK, "{");
	_vte_debug_print (VTE_DEBUG_TIMEOUT,
			"Update timeout:  %d active\n",
			g_list_length (active_terminals));

	if (process_timeout_tag != 0) {
		_vte_debug_print(VTE_DEBUG_TIMEOUT,
				"Removing process timeout\n");
		g_source_remove (process_timeout_tag);
		process_timeout_tag = 0;
	}

	for (l = active_terminals; l != NULL; l = next) {
		VteTerminal *terminal = (VteTerminal *)l->data;

		next = g_list_next (l);

		if (l != active_terminals) {
			_vte_debug_print (VTE_DEBUG_WORK, "T");
		}
		if (terminal->pvt->pty_channel != NULL) {
			if (terminal->pvt->pty_input_active ||
					terminal->pvt->pty_input_source == 0) {
				terminal->pvt->pty_input_active = FALSE;
				vte_terminal_io_read (terminal->pvt->pty_channel,
						G_IO_IN, terminal);
			}
			_vte_terminal_enable_input_source (terminal);
		}
		vte_terminal_emit_adjustment_changed (terminal);
		if (need_processing (terminal)) {
			if (VTE_MAX_PROCESS_TIME) {
				time_process_incoming (terminal);
			} else {
				vte_terminal_process_incoming (terminal);
			}
			terminal->pvt->input_bytes = 0;
		} else
			vte_terminal_emit_pending_signals (terminal);

		redraw |= update_regions (terminal);
	}

	if (redraw) {
		/* remove the idle source, and draw non-Terminals
		 * (except for gdk/{directfb,quartz}!)
		 */
		gdk_window_process_all_updates ();
	}

	_vte_debug_print (VTE_DEBUG_WORK, "}");

	/* Set a timer such that we do not invalidate for a while. */
	/* This limits the number of times we draw to ~40fps. */
	update_timeout_tag =
		g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
				    VTE_UPDATE_REPEAT_TIMEOUT,
				    update_repeat_timeout, NULL,
				    NULL);
	in_update_timeout = FALSE;

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	gdk_threads_leave();
        G_GNUC_END_IGNORE_DEPRECATIONS;

	return FALSE;
}

bool
VteTerminalPrivate::write_contents_sync (GOutputStream *stream,
                                         VteWriteFlags flags,
                                         GCancellable *cancellable,
                                         GError **error)
{
	return _vte_ring_write_contents (m_screen->row_data,
					 stream, flags,
					 cancellable, error);
}

/*
 * Buffer search
 */

/* TODO Add properties & signals */

#ifdef WITH_PCRE2

/*
 * VteTerminalPrivate::search_set_regex:
 * @regex: (allow-none): a #VteRegex, or %nullptr
 * @flags: PCRE2 match flags, or 0
 *
 * Sets the regex to search for. Unsets the search regex when passed %nullptr.
 */
bool
VteTerminalPrivate::search_set_regex (VteRegex *regex,
                                      guint32 flags)
{
        struct vte_regex_and_flags *rx;

        rx = &m_search_regex;

        if (rx->mode == VTE_REGEX_PCRE2 &&
            rx->pcre.regex == regex &&
            rx->pcre.match_flags == flags)
                return false;

        regex_and_flags_clear(rx);

        if (regex != NULL) {
                rx->mode = VTE_REGEX_PCRE2;
                rx->pcre.regex = vte_regex_ref(regex);
                rx->pcre.match_flags = flags;
        }

	invalidate_all();

        return true;
}

#endif /* WITH_PCRE2 */

/*
 * VteTerminalPrivate::search_set_gregex:
 * @gregex: (allow-none): a #GRegex, or %nullptr
 * @gflags: flags from #GRegexMatchFlags
 *
 * Sets the #GRegex regex to search for. Unsets the search regex when passed %nullptr.
 */
bool
VteTerminalPrivate::search_set_gregex(GRegex *gregex,
                                      GRegexMatchFlags gflags)
{
        struct vte_regex_and_flags *rx = &m_search_regex;

        if (rx->mode == VTE_REGEX_GREGEX &&
            rx->gregex.regex == gregex &&
            rx->gregex.match_flags == gflags)
                return false;

        regex_and_flags_clear(rx);

        if (gregex != NULL) {
                rx->mode = VTE_REGEX_GREGEX;
                rx->gregex.regex = g_regex_ref(gregex);
                rx->gregex.match_flags = gflags;
        }

	invalidate_all();

        return true;
}

bool
VteTerminalPrivate::search_set_wrap_around(bool wrap)
{
        if (wrap == m_search_wrap_around)
                return false;

        m_search_wrap_around = wrap;
        return true;
}

bool
VteTerminalPrivate::search_rows(
#ifdef WITH_PCRE2
                                pcre2_match_context_8 *match_context,
                                pcre2_match_data_8 *match_data,
#endif
                                vte::grid::row_t start_row,
                                vte::grid::row_t end_row,
                                bool backward)
{
	char *row_text;
        gsize row_text_length;
	int start, end;
	long start_col, end_col;
	gchar *word;
	VteCharAttributes *ca;
	GArray *attrs;
	gdouble value, page_size;


	row_text = _vte_terminal_get_text_range_full(m_terminal, start_row, 0, end_row, -1, NULL, NULL, NULL, &row_text_length);

#ifdef WITH_PCRE2
        if (G_LIKELY(m_search_regex.mode == VTE_REGEX_PCRE2)) {
                int (* match_fn) (const pcre2_code_8 *,
                                  PCRE2_SPTR8, PCRE2_SIZE, PCRE2_SIZE, uint32_t,
                                  pcre2_match_data_8 *, pcre2_match_context_8 *);
                gsize *ovector, so, eo;
                int r;

                if (_vte_regex_get_jited(m_search_regex.pcre.regex))
                        match_fn = pcre2_jit_match_8;
                else
                        match_fn = pcre2_match_8;

                r = match_fn(_vte_regex_get_pcre(m_search_regex.pcre.regex),
                             (PCRE2_SPTR8)row_text, row_text_length , /* subject, length */
                             0, /* start offset */
                             m_search_regex.pcre.match_flags |
                             PCRE2_NO_UTF_CHECK | PCRE2_NOTEMPTY | PCRE2_PARTIAL_SOFT /* FIXME: HARD? */,
                             match_data,
                             match_context);

                if (r == PCRE2_ERROR_NOMATCH)
                        return false;
                // FIXME: handle partial matches (PCRE2_ERROR_PARTIAL)
                if (r < 0)
                        return false;

                ovector = pcre2_get_ovector_pointer_8(match_data);
                so = ovector[0];
                eo = ovector[1];
                if (G_UNLIKELY(so == PCRE2_UNSET || eo == PCRE2_UNSET))
                        return false;

                start = so;
                end = eo;
                word = g_strndup(row_text, end - start);
        } else
#endif /* WITH_PCRE2 */
        {
                GMatchInfo *match_info;
                GError *error = NULL;

                g_assert_cmpint(m_search_regex.mode, ==, VTE_REGEX_GREGEX);

                g_regex_match_full (m_search_regex.gregex.regex, row_text, row_text_length, 0,
                                    (GRegexMatchFlags)(m_search_regex.gregex.match_flags | G_REGEX_MATCH_NOTEMPTY),
                                    &match_info, &error);
                if (error) {
                        g_printerr ("Error while matching: %s\n", error->message);
                        g_error_free (error);
                        g_match_info_free (match_info);
                        g_free (row_text);
                        return false;
                }

                if (!g_match_info_matches (match_info)) {
                        g_match_info_free (match_info);
                        g_free (row_text);
                        return false;
                }

                word = g_match_info_fetch (match_info, 0);
                /* This gives us the offset in the buffer */
                g_match_info_fetch_pos (match_info, 0, &start, &end);

                g_match_info_free (match_info);
        }

	/* Fetch text again, with attributes */
	g_free (row_text);
	if (!m_search_attrs)
		m_search_attrs = g_array_new (FALSE, TRUE, sizeof (VteCharAttributes));
	attrs = m_search_attrs;
	row_text = vte_terminal_get_text_range(m_terminal, start_row, 0, end_row, -1, NULL, NULL, attrs);

	ca = &g_array_index (attrs, VteCharAttributes, start);
	start_row = ca->row;
	start_col = ca->column;
	ca = &g_array_index (attrs, VteCharAttributes, end - 1);
	end_row = ca->row;
	end_col = ca->column;

	g_free (word);
	g_free (row_text);

	_vte_terminal_select_text(m_terminal, start_col, start_row, end_col, end_row, 0, 0);
	/* Quite possibly the math here should not access adjustment directly... */
	value = gtk_adjustment_get_value(m_vadjustment);
	page_size = gtk_adjustment_get_page_size(m_vadjustment);
	if (backward) {
		if (end_row < value || end_row > value + page_size - 1)
			vte_terminal_queue_adjustment_value_changed_clamped(m_terminal, end_row - page_size + 1);
	} else {
		if (start_row < value || start_row > value + page_size - 1)
			vte_terminal_queue_adjustment_value_changed_clamped(m_terminal, start_row);
	}

	return true;
}

bool
VteTerminalPrivate::search_rows_iter(
#ifdef WITH_PCRE2
                                     pcre2_match_context_8 *match_context,
                                     pcre2_match_data_8 *match_data,
#endif
                                     vte::grid::row_t start_row,
                                     vte::grid::row_t end_row,
                                     bool backward)
{
	const VteRowData *row;
	long iter_start_row, iter_end_row;

	if (backward) {
		iter_start_row = end_row;
		while (iter_start_row > start_row) {
			iter_end_row = iter_start_row;

			do {
				iter_start_row--;
				row = _vte_terminal_find_row_data(m_terminal, iter_start_row);
			} while (row && row->attr.soft_wrapped);

			if (search_rows(
#ifdef WITH_PCRE2
                                                      match_context, match_data,
#endif
                                                      iter_start_row, iter_end_row, backward))
				return true;
		}
	} else {
		iter_end_row = start_row;
		while (iter_end_row < end_row) {
			iter_start_row = iter_end_row;

			do {
				row = _vte_terminal_find_row_data(m_terminal, iter_end_row);
				iter_end_row++;
			} while (row && row->attr.soft_wrapped);

			if (search_rows(
#ifdef WITH_PCRE2
                                                      match_context, match_data,
#endif
                                                      iter_start_row, iter_end_row, backward))
				return true;
		}
	}

	return false;
}

bool
VteTerminalPrivate::search_find (bool backward)
{
        vte::grid::row_t buffer_start_row, buffer_end_row;
        vte::grid::row_t last_start_row, last_end_row;
        bool match_found = false;
#ifdef WITH_PCRE2
        pcre2_match_context_8 *match_context = nullptr;
        pcre2_match_data_8 *match_data = nullptr;
#endif

	if (m_search_regex.mode == VTE_REGEX_UNDECIDED)
		return false;

	/* TODO
	 * Currently We only find one result per extended line, and ignore columns
	 * Moreover, the whole search thing is implemented very inefficiently.
	 */

#ifdef WITH_PCRE2
        if (G_LIKELY(m_search_regex.mode == VTE_REGEX_PCRE2)) {
                match_context = create_match_context();
                match_data = pcre2_match_data_create_8(256 /* should be plenty */, nullptr /* general context */);
        }
#endif

	buffer_start_row = _vte_ring_delta (m_screen->row_data);
	buffer_end_row = _vte_ring_next (m_screen->row_data);

	if (m_has_selection) {
		last_start_row = m_selection_start.row;
		last_end_row = m_selection_end.row + 1;
	} else {
		last_start_row = m_screen->scroll_delta + m_row_count;
		last_end_row = m_screen->scroll_delta;
	}
	last_start_row = MAX (buffer_start_row, last_start_row);
	last_end_row = MIN (buffer_end_row, last_end_row);

	/* If search fails, we make an empty selection at the last searched
	 * position... */
	if (backward) {
		if (search_rows_iter (
#ifdef WITH_PCRE2
                                                   match_context, match_data,
#endif
                                                   buffer_start_row, last_start_row, backward))
			goto found;
		if (m_search_wrap_around &&
		    search_rows_iter (
#ifdef WITH_PCRE2
                                                   match_context, match_data,
#endif
                                                   last_end_row, buffer_end_row, backward))
			goto found;
		if (m_has_selection) {
			if (m_search_wrap_around)
			    _vte_terminal_select_empty_at (m_terminal,
							   m_selection_start.col,
							   m_selection_start.row);
			else
			    _vte_terminal_select_empty_at (m_terminal,
							   -1,
							   buffer_start_row - 1);
		}
                match_found = false;
	} else {
		if (search_rows_iter (
#ifdef WITH_PCRE2
                                                   match_context, match_data,
#endif
                                                   last_end_row, buffer_end_row, backward))
			goto found;
		if (m_search_wrap_around &&
		    search_rows_iter (
#ifdef WITH_PCRE2
                                                   match_context, match_data,
#endif
                                                   buffer_start_row, last_start_row, backward))
			goto found;
		if (m_has_selection) {
			if (m_search_wrap_around)
			    _vte_terminal_select_empty_at (m_terminal,
							   m_selection_end.col + 1,
							   m_selection_end.row);
			else
			    _vte_terminal_select_empty_at (m_terminal,
							   -1,
							   buffer_end_row);
		}
                match_found = false;
	}

 found:

#ifdef WITH_PCRE2
        if (match_data)
                pcre2_match_data_free_8(match_data);
        if (match_context)
                pcre2_match_context_free_8(match_context);
#endif

	return match_found;
}

/*
 * VteTerminalPrivate::set_input_enabled:
 * @enabled: whether to enable user input
 *
 * Enables or disables user input. When user input is disabled,
 * the terminal's child will not receive any key press, or mouse button
 * press or motion events sent to it.
 *
 * Returns: %true iff the setting changed
 */
bool
VteTerminalPrivate::set_input_enabled (bool enabled)
{
        if (enabled == m_input_enabled)
                return false;

        m_input_enabled = enabled;

        auto context = gtk_widget_get_style_context(m_widget);

        /* FIXME: maybe hide cursor when input disabled, too? */

        if (enabled) {
                if (gtk_widget_has_focus(m_widget))
                        gtk_im_context_focus_in(m_im_context);

                gtk_style_context_remove_class (context, GTK_STYLE_CLASS_READ_ONLY);
        } else {
                vte_terminal_im_reset(m_terminal);
                if (gtk_widget_has_focus(m_widget))
                        gtk_im_context_focus_out(m_im_context);

                _vte_terminal_disconnect_pty_write(m_terminal);
                _vte_byte_array_clear(m_outgoing);

                gtk_style_context_add_class (context, GTK_STYLE_CLASS_READ_ONLY);
        }

        return true;
}

bool
VteTerminalPrivate::process_word_char_exceptions(char const *str,
                                                 gunichar **arrayp,
                                                 gsize *lenp)
{
        const char *p;
        gunichar *array, c;
        gsize len, i;

        if (str == NULL)
                str = WORD_CHAR_EXCEPTIONS_DEFAULT;

        len = g_utf8_strlen(str, -1);
        array = g_new(gunichar, len);
        i = 0;

        for (p = str; *p; p = g_utf8_next_char(p)) {
                c = g_utf8_get_char(p);

                /* For forward compatibility reasons, we skip
                 * characters that aren't supposed to be here,
                 * instead of erroring out.
                 */
                /* '-' must only be used*  at the start of the string */
                if (c == (gunichar)'-' && p != str)
                        continue;
                if (!g_unichar_isgraph(c))
                        continue;
                if (g_unichar_isspace(c))
                        continue;
                if (g_unichar_isalnum(c))
                        continue;

                array[i++] = g_utf8_get_char(p);
        }

        g_assert(i <= len);
        len = i; /* we may have skipped some characters */

        /* Sort the result since we want to use bsearch on it */
        qsort(array, len, sizeof(gunichar), compare_unichar_p);

        /* Check that no character occurs twice */
        for (i = 1; i < len; i++) {
                if (array[i-1] != array[i])
                        continue;

                g_free(array);
                return false;
        }

#if 0
        /* Debug */
        for (i = 0; i < len; i++) {
                char utf[7];
                c = array[i];
                utf[g_unichar_to_utf8(c, utf)] = '\0';
                g_printerr("Word char exception: U+%04X %s\n", c, utf);
        }
#endif

        *lenp = len;
        *arrayp = array;
        return true;
}

/*
 * VteTerminalPrivate::set_word_char_exceptions:
 * @exceptions: a string of ASCII punctuation characters, or %nullptr
 *
 * With this function you can provide a set of characters which will
 * be considered parts of a word when doing word-wise selection, in
 * addition to the default which only considers alphanumeric characters
 * part of a word.
 *
 * The characters in @exceptions must be non-alphanumeric, each character
 * must occur only once, and if @exceptions contains the character
 * U+002D HYPHEN-MINUS, it must be at the start of the string.
 *
 * Use %nullptr to reset the set of exception characters to the default.
 *
 * Returns: %true if the word char exceptions changed
 */
bool
VteTerminalPrivate::set_word_char_exceptions(char const* exceptions)
{
        gunichar *array;
        gsize len;

        if (g_strcmp0(exceptions, m_word_char_exceptions_string) == 0)
                return false;

        if (!process_word_char_exceptions(exceptions, &array, &len))
                return false;

        g_free(m_word_char_exceptions_string);
        m_word_char_exceptions_string = g_strdup(exceptions);

        g_free(m_word_char_exceptions);
        m_word_char_exceptions = array;
        m_word_char_exceptions_len = len;

        return true;
}
