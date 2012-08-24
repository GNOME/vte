/*
 * Copyright (C) 2001-2004,2009,2010 Red Hat, Inc.
 * Copyright © 2008, 2009, 2010, 2011 Christian Persch
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * SECTION: vte-view
 * @short_description: A terminal widget implementation
 *
 * A VteView is a terminal emulator implemented as a GTK+ widget.
 */

#include <config.h>

#include <math.h>

#include "vte.h"
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
#include "pty.h"
#include "vteaccess.h"
#include "vteint.h"
#include "vtepty.h"
#include "vtepty-private.h"
#include "vtetc.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

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

static void vte_view_emit_copy_clipboard(VteView *terminal);
static void vte_view_emit_paste_clipboard(VteView *terminal);
static void vte_view_emit_copy_primary(VteView *terminal);
static void vte_view_emit_paste_primary(VteView *terminal);
static void vte_view_set_visibility (VteView *terminal, GdkVisibilityState state);
static void vte_buffer_set_termcap(VteBuffer *buffer);
static gboolean vte_buffer_io_read(GIOChannel *channel,
				     GIOCondition condition,
				     VteBuffer *buffer);
static gboolean vte_buffer_io_write(GIOChannel *channel,
				      GIOCondition condition,
				      VteBuffer *buffer);
static void vte_view_match_hilite_clear(VteView *terminal);
static void vte_view_match_hilite_hide(VteView *terminal);
static void vte_view_match_hilite_show(VteView *terminal, long x, long y);
static void vte_view_match_hilite_update(VteView *terminal, long x, long y);
static void vte_view_match_contents_clear(VteView *terminal);
static void vte_view_emit_pending_signals(VteView *terminal);
static gboolean vte_view_cell_is_selected(VteBuffer *buffer,
                                              glong col, glong row,
                                              VteView *terminal);
static char *vte_buffer_get_text_range_maybe_wrapped(VteBuffer *buffer,
						       glong start_row,
						       glong start_col,
						       glong end_row,
						       glong end_col,
						       gboolean wrap,
						       VteSelectionFunc is_selected,
						       gpointer data,
						       GArray *attributes,
						       gboolean include_trailing_spaces);
static char *vte_buffer_get_text_maybe_wrapped(VteBuffer *buffer,
						 gboolean wrap,
						 VteSelectionFunc is_selected,
						 gpointer data,
						 GArray *attributes,
						 gboolean include_trailing_spaces);
static void vte_view_stop_processing (VteView *terminal);

static inline gboolean vte_view_is_processing (VteView *terminal);
static inline void vte_view_start_processing (VteView *terminal);
static void vte_view_add_process_timeout (VteView *terminal);
static void add_update_timeout (VteView *terminal);
static void remove_update_timeout (VteView *terminal);
static void reset_update_regions (VteView *terminal);
static void vte_view_set_cursor_blinks_internal(VteView *terminal, gboolean blink);
static void _vte_check_cursor_blink(VteView *terminal);
static void vte_view_set_font(VteView *terminal, PangoFontDescription *desc /* adopted */);
static void vte_view_beep(VteView *terminal, VteBellType bell_type);
static void vte_view_buffer_contents_changed(VteView *terminal);
static void vte_buffer_process_incoming(VteBuffer *buffer);

static gboolean process_timeout (gpointer data);
static gboolean update_timeout (gpointer data);

enum {
        TERMINAL_COPY_CLIPBOARD,
        TERMINAL_PASTE_CLIPBOARD,
        TERMINAL_COPY_PRIMARY,
        TERMINAL_PASTE_PRIMARY,
        TERMINAL_BUFFER_CHANGED,
        LAST_TERMINAL_SIGNAL
};
static guint signals[LAST_TERMINAL_SIGNAL];

enum {
        PROP_0,
        PROP_BUFFER,
        PROP_HADJUSTMENT,
        PROP_VADJUSTMENT,
        PROP_HSCROLL_POLICY,
        PROP_VSCROLL_POLICY,
        PROP_AUDIBLE_BELL,
        PROP_MOUSE_POINTER_AUTOHIDE,
        PROP_SCROLL_ON_KEYSTROKE,
        PROP_SCROLL_ON_OUTPUT,
        PROP_WORD_CHARS,
        PROP_VISIBLE_BELL,
        PROP_FONT_SCALE
};

enum {
        BUFFER_PROP_0,
        BUFFER_PROP_BACKSPACE_BINDING,
        BUFFER_PROP_DELETE_BINDING,
        BUFFER_PROP_EMULATION,
        BUFFER_PROP_ENCODING,
        BUFFER_PROP_SCROLLBACK_LINES,
        BUFFER_PROP_ICON_TITLE,
        BUFFER_PROP_WINDOW_TITLE,
        BUFFER_PROP_PTY,
        BUFFER_PROP_CURRENT_DIRECTORY_URI,
        BUFFER_PROP_CURRENT_FILE_URI,
};

enum {
        BUFFER_COMMIT,
        BUFFER_EMULATION_CHANGED,
        BUFFER_ENCODING_CHANGED,
        BUFFER_WINDOW_TITLE_CHANGED,
        BUFFER_ICON_TITLE_CHANGED,
        BUFFER_STATUS_LINE_CHANGED,
        BUFFER_EOF,
        BUFFER_CHILD_EXITED,
        BUFFER_DEICONIFY_WINDOW,
        BUFFER_ICONIFY_WINDOW,
        BUFFER_RAISE_WINDOW,
        BUFFER_LOWER_WINDOW,
        BUFFER_REFRESH_WINDOW,
        BUFFER_RESTORE_WINDOW,
        BUFFER_MAXIMIZE_WINDOW,
        BUFFER_RESIZE_WINDOW,
        BUFFER_MOVE_WINDOW,
        BUFFER_CURSOR_MOVED,
        BUFFER_TEXT_MODIFIED,
        BUFFER_TEXT_INSERTED,
        BUFFER_TEXT_DELETED,
        BUFFER_CONTENTS_CHANGED,
        BUFFER_BELL,
        BUFFER_CURRENT_DIRECTORY_URI_CHANGED,
        BUFFER_CURRENT_FILE_URI_CHANGED,
        LAST_BUFFER_SIGNAL,
};

static guint buffer_signals[LAST_BUFFER_SIGNAL];

/* these static variables are guarded by the GDK mutex */
static guint process_timeout_tag = 0;
static gboolean in_process_timeout;
static guint update_timeout_tag = 0;
static gboolean in_update_timeout;
static GList *active_terminals;
static GTimer *process_timer;

static const GtkBorder default_padding = { 1, 1, 1, 1 };

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

#ifdef VTE_DEBUG
G_DEFINE_TYPE_WITH_CODE(VteView, vte_view, GTK_TYPE_WIDGET,
                        g_type_add_class_private (g_define_type_id, sizeof (VteViewClassPrivate));
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL)
                        if (_vte_debug_on(VTE_DEBUG_LIFECYCLE)) {
                                g_printerr("vte_view_get_type()\n");
                        })
#else
G_DEFINE_TYPE_WITH_CODE(VteView, vte_view, GTK_TYPE_WIDGET,
                        g_type_add_class_private (g_define_type_id, sizeof (VteViewClassPrivate));
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))
#endif

/* Indexes in the "palette" color array for the dim colors.
 * Only the first %VTE_LEGACY_COLOR_SET_SIZE colors have dim versions.  */
static const guchar corresponding_dim_index[] = {16,88,28,100,18,90,30,102};

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
_vte_buffer_ring_insert (VteBuffer *buffer,
                         glong position,
                         gboolean fill)
{
	VteRowData *row;
        VteScreen *screen;
        VteRing *ring;

        screen = buffer->pvt->screen;
	ring = screen->row_data;
	while (G_UNLIKELY (_vte_ring_next (ring) < position)) {
		row = _vte_ring_append (ring);
		_vte_row_data_fill (row, &screen->fill_defaults, buffer->pvt->column_count);
	}
	row = _vte_ring_insert (ring, position);
	if (fill)
		_vte_row_data_fill (row, &screen->fill_defaults, buffer->pvt->column_count);
	return row;
}

VteRowData *
_vte_buffer_ring_append (VteBuffer *buffer,
                         gboolean fill)
{
        return _vte_buffer_ring_insert (buffer,
                                        _vte_ring_next (buffer->pvt->screen->row_data),
                                        fill);
}

void
_vte_buffer_ring_remove (VteBuffer *buffer,
                         glong position)
{
        _vte_ring_remove (buffer->pvt->screen->row_data, position);
}

/* Reset defaults for character insertion. */
void
_vte_screen_set_default_attributes(VteScreen *screen)
{
	screen->defaults = basic_cell.cell;
	screen->color_defaults = screen->defaults;
	screen->fill_defaults = screen->defaults;
}

/* Cause certain cells to be repainted. */
void
_vte_invalidate_cells(VteView *terminal,
		      glong column_start, gint column_count,
		      glong row_start, gint row_count)
{
        VteBuffer *buffer;
	cairo_rectangle_int_t rect;
	glong i;

        if (!gtk_widget_get_realized(&terminal->widget)) {
                return;
        }

	if (!column_count || !row_count) {
		return;
	}

	if (terminal->pvt->invalidated_all) {
		return;
	}

        buffer = terminal->pvt->buffer;

	_vte_debug_print (VTE_DEBUG_UPDATES,
			"Invalidating cells at (%ld,%ld+%ld)x(%d,%d).\n",
			column_start, row_start,
			(long)buffer->pvt->screen->scroll_delta,
			column_count, row_count);
	_vte_debug_print (VTE_DEBUG_WORK, "?");

	/* Subtract the scrolling offset from the row start so that the
	 * resulting rectangle is relative to the visible portion of the
	 * buffer. */
	row_start -= buffer->pvt->screen->scroll_delta;

	/* Ensure the start of region is on screen */
	if (column_start > buffer->pvt->column_count ||
            row_start > buffer->pvt->row_count) {
		return;
	}

	/* Clamp the start values to reasonable numbers. */
	i = row_start + row_count;
	row_start = MAX (0, row_start);
	row_count = CLAMP (i - row_start, 0, buffer->pvt->row_count);

	i = column_start + column_count;
	column_start = MAX (0, column_start);
	column_count = CLAMP (i - column_start, 0 , buffer->pvt->column_count);

	if (!column_count || !row_count) {
		return;
	}
	if (column_count == buffer->pvt->column_count &&
            row_count == buffer->pvt->row_count) {
		_vte_invalidate_all (terminal);
		return;
	}

	/* Convert the column and row start and end to pixel values
	 * by multiplying by the size of a character cell.
	 * Always include the extra pixel border and overlap pixel.
	 */
	rect.x = column_start * terminal->pvt->char_width - 1;
	if (column_start != 0) {
		rect.x += terminal->pvt->padding.left;
	}
	rect.width = (column_start + column_count) * terminal->pvt->char_width + 3 + terminal->pvt->padding.left;
	if (column_start + column_count == buffer->pvt->column_count) {
		rect.width += terminal->pvt->padding.right;
	}
	rect.width -= rect.x;

	rect.y = row_start * terminal->pvt->char_height - 1;
	if (row_start != 0) {
		rect.y += terminal->pvt->padding.top;
	}
	rect.height = (row_start + row_count) * terminal->pvt->char_height + 2 + terminal->pvt->padding.top;
	if (row_start + row_count == buffer->pvt->row_count) {
		rect.height += terminal->pvt->padding.bottom;
	}
	rect.height -= rect.y;

	_vte_debug_print (VTE_DEBUG_UPDATES,
			"Invalidating pixels at (%d,%d)x(%d,%d).\n",
			rect.x, rect.y, rect.width, rect.height);

	if (terminal->pvt->active != NULL) {
		terminal->pvt->update_regions = g_slist_prepend (
				terminal->pvt->update_regions,
				cairo_region_create_rectangle (&rect));
		/* Wait a bit before doing any invalidation, just in
		 * case updates are coming in really soon. */
		add_update_timeout (terminal);
	} else {
		gdk_window_invalidate_rect (gtk_widget_get_window (&terminal->widget), &rect, FALSE);
	}

	_vte_debug_print (VTE_DEBUG_WORK, "!");
}

static void
_vte_invalidate_region (VteView *terminal,
			glong scolumn, glong ecolumn,
			glong srow, glong erow,
			gboolean block)
{
	if (block || srow == erow) {
		_vte_invalidate_cells(terminal,
				scolumn, ecolumn - scolumn + 1,
				srow, erow - srow + 1);
	} else {
                VteBuffer *buffer = terminal->pvt->buffer;

		_vte_invalidate_cells(terminal,
				scolumn,
				buffer->pvt->column_count - scolumn,
				srow, 1);
		_vte_invalidate_cells(terminal,
				0, buffer->pvt->column_count,
				srow + 1, erow - srow - 1);
		_vte_invalidate_cells(terminal,
				0, ecolumn + 1,
				erow, 1);
	}
}


/* Redraw the entire visible portion of the window. */
void
_vte_invalidate_all(VteView *terminal)
{
	cairo_rectangle_int_t rect;
	GtkAllocation allocation;

	g_assert(VTE_IS_VIEW(terminal));

        if (!gtk_widget_get_realized(&terminal->widget)) {
                return;
        }
	if (terminal->pvt->invalidated_all) {
		return;
	}

	_vte_debug_print (VTE_DEBUG_WORK, "*");
	_vte_debug_print (VTE_DEBUG_UPDATES, "Invalidating all.\n");

	gtk_widget_get_allocation (&terminal->widget, &allocation);

	/* replace invalid regions with one covering the whole terminal */
	reset_update_regions (terminal);
	rect.x = rect.y = 0;
	rect.width = allocation.width;
	rect.height = allocation.height;
	terminal->pvt->invalidated_all = TRUE;

	if (terminal->pvt->active != NULL) {
		terminal->pvt->update_regions = g_slist_prepend (NULL,
				cairo_region_create_rectangle (&rect));
		/* Wait a bit before doing any invalidation, just in
		 * case updates are coming in really soon. */
		add_update_timeout (terminal);
	} else {
		gdk_window_invalidate_rect (gtk_widget_get_window (&terminal->widget), &rect, FALSE);
	}
}

/* Scroll a rectangular region up or down by a fixed number of lines,
 * negative = up, positive = down. */
void
_vte_view_scroll_region (VteView *terminal,
			     long row, glong count, glong delta)
{
        VteBuffer *buffer;

	if ((delta == 0) || (count == 0)) {
		/* Shenanigans! */
		return;
	}

        buffer = terminal->pvt->buffer;

	if (count >= buffer->pvt->row_count) {
		/* We have to repaint the entire window. */
		_vte_invalidate_all(terminal);
	} else {
		/* We have to repaint the area which is to be
		 * scrolled. */
		_vte_invalidate_cells(terminal,
				     0, buffer->pvt->column_count,
				     row, count);
	}
}

/* Find the row in the given position in the backscroll buffer. */
static inline const VteRowData *
_vte_screen_find_row_data(VteScreen *screen,
                          glong row)
{
	const VteRowData *rowdata = NULL;

	if (G_LIKELY (_vte_ring_contains (screen->row_data, row))) {
		rowdata = _vte_ring_index (screen->row_data, row);
	}
	return rowdata;
}

/* Find the row in the given position in the backscroll buffer. */
static inline VteRowData *
_vte_screen_find_row_data_writable(VteScreen *screen,
                                   glong row)
{
	VteRowData *rowdata = NULL;

	if (G_LIKELY (_vte_ring_contains (screen->row_data, row))) {
		rowdata = _vte_ring_index_writable (screen->row_data, row);
	}
	return rowdata;
}

/* Find the character an the given position in the backscroll buffer. */
static const VteCell *
vte_screen_find_charcell(VteScreen *screen,
                         gulong col,
                         glong row)
{
	const VteRowData *rowdata;
	const VteCell *ret = NULL;

	if (_vte_ring_contains (screen->row_data, row)) {
		rowdata = _vte_ring_index (screen->row_data, row);
		ret = _vte_row_data_get (rowdata, col);
	}
	return ret;
}

/* This could be vte_screen_find_start_column, but for symmetry
 * with find_end_column (which needs buffer->pvt->column_count)
 * it's a VteBuffer method.
 */
static glong
vte_buffer_find_start_column(VteBuffer *buffer,
                             glong col,
                             glong row)
{
	const VteRowData *row_data;

	if (G_UNLIKELY (col < 0))
		return col;

        row_data = _vte_screen_find_row_data(buffer->pvt->screen, row);
	if (row_data != NULL) {
		const VteCell *cell = _vte_row_data_get (row_data, col);
		while (col > 0 && cell != NULL && cell->attr.fragment) {
			cell = _vte_row_data_get (row_data, --col);
		}
	}
	return MAX(col, 0);
}

static glong
vte_buffer_find_end_column(VteBuffer *buffer,
                           glong col,
                           glong row)
{
	const VteRowData *row_data;
        gint columns = 0;

	if (G_UNLIKELY (col < 0))
		return col;

        row_data = _vte_screen_find_row_data(buffer->pvt->screen, row);
        if (row_data != NULL) {
		const VteCell *cell = _vte_row_data_get (row_data, col);
		while (col > 0 && cell != NULL && cell->attr.fragment) {
			cell = _vte_row_data_get (row_data, --col);
		}
		if (cell) {
			columns = cell->attr.columns - 1;
		}
	}
	return MIN(col + columns, buffer->pvt->column_count);
}

/* Determine the width of the portion of the preedit string which lies
 * to the left of the cursor, or the entire string, in columns. */
static gssize
vte_view_preedit_width(VteView *terminal, gboolean left_only)
{
        VteViewPrivate *pvt = terminal->pvt;
        VteBuffer *buffer = terminal->pvt->buffer;
	gunichar c;
	int i;
	gssize ret = 0;
	const char *preedit = NULL;

	if (pvt->im_preedit != NULL) {
		preedit = pvt->im_preedit;
		for (i = 0;
		     (preedit != NULL) &&
		     (preedit[0] != '\0') &&
		     (!left_only || (i < pvt->im_preedit_cursor));
		     i++) {
			c = g_utf8_get_char(preedit);
			ret += _vte_iso2022_unichar_width(buffer->pvt->iso2022, c);
			preedit = g_utf8_next_char(preedit);
		}
	}

	return ret;
}

/* Determine the length of the portion of the preedit string which lies
 * to the left of the cursor, or the entire string, in gunichars. */
static gssize
vte_view_preedit_length(VteView *terminal, gboolean left_only)
{
        VteViewPrivate *pvt = terminal->pvt;
	int i = 0;
	const char *preedit = NULL;

	if (pvt->im_preedit != NULL) {
		preedit = pvt->im_preedit;
		for (i = 0;
		     (preedit != NULL) &&
		     (preedit[0] != '\0') &&
		     (!left_only || (i < pvt->im_preedit_cursor));
		     i++) {
			preedit = g_utf8_next_char(preedit);
		}
	}

	return i;
}

/* Cause the cell to be redrawn. */
void
_vte_invalidate_cell(VteView *terminal, glong col, glong row)
{
        VteBuffer *buffer;
	const VteRowData *row_data;
	int columns;

        if (!gtk_widget_get_realized(&terminal->widget)) {
                return;
        }
	if (terminal->pvt->invalidated_all) {
		return;
	}

        buffer = terminal->pvt->buffer;

	columns = 1;
	row_data = _vte_screen_find_row_data(buffer->pvt->screen, row);
	if (row_data != NULL) {
		const VteCell *cell;
		cell = _vte_row_data_get (row_data, col);
		if (cell != NULL) {
			while (cell->attr.fragment && col> 0) {
				cell = _vte_row_data_get (row_data, --col);
			}
			columns = cell->attr.columns;
			if (cell->c != 0 &&
					_vte_draw_get_char_width (
						terminal->pvt->draw,
						cell->c,
						columns, cell->attr.bold) >
					terminal->pvt->char_width * columns) {
				columns++;
			}
		}
	}

	_vte_debug_print(VTE_DEBUG_UPDATES,
			"Invalidating cell at (%ld,%ld-%ld).\n",
			row, col, col + columns);
	_vte_invalidate_cells(terminal,
			col, columns,
			row, 1);
}

/* Cause the cursor to be redrawn. */
void
_vte_invalidate_cursor_once(VteView *terminal, gboolean periodic)
{
        VteBuffer *buffer;
	VteScreen *screen;
	const VteCell *cell;
	gssize preedit_width;
	glong column, row;
	gint columns;

        if (!gtk_widget_get_realized(&terminal->widget)) {
                return;
        }
	if (terminal->pvt->invalidated_all) {
		return;
	}

	if (periodic) {
		if (!terminal->pvt->cursor_blinks) {
			return;
		}
	}

        buffer = terminal->pvt->buffer;

	if (buffer->pvt->cursor_visible) {
		preedit_width = vte_view_preedit_width(terminal, FALSE);

		screen = buffer->pvt->screen;
		row = screen->cursor_current.row;
		column = screen->cursor_current.col;
		columns = 1;
		column = vte_buffer_find_start_column(buffer, column, row);
		cell = vte_screen_find_charcell(screen, column, row);
		if (cell != NULL) {
			columns = cell->attr.columns;
			if (cell->c != 0 &&
					_vte_draw_get_char_width (
						terminal->pvt->draw,
						cell->c,
						columns, cell->attr.bold) >
			    terminal->pvt->char_width * columns) {
				columns++;
			}
		}
		if (preedit_width > 0) {
			columns += preedit_width;
			columns++; /* one more for the preedit cursor */
		}

		_vte_debug_print(VTE_DEBUG_UPDATES,
				"Invalidating cursor at (%ld,%ld-%ld).\n",
				row, column, column + columns);
		_vte_invalidate_cells(terminal,
				     column, columns,
				     row, 1);
	}
}

/* Invalidate the cursor repeatedly. */
static gboolean
vte_invalidate_cursor_periodic (VteView *terminal)
{
        VteViewPrivate *pvt = terminal->pvt;

	pvt->cursor_blink_state = !pvt->cursor_blink_state;
	pvt->cursor_blink_time += pvt->cursor_blink_cycle;

	_vte_invalidate_cursor_once(terminal, TRUE);

	/* only disable the blink if the cursor is currently shown.
	 * else, wait until next time.
	 */
	if (pvt->cursor_blink_time / 1000 >= pvt->cursor_blink_timeout &&
	    pvt->cursor_blink_state) {
                pvt->cursor_blink_tag = 0;
		return FALSE;
        }

	pvt->cursor_blink_tag = g_timeout_add_full(G_PRIORITY_LOW,
						   terminal->pvt->cursor_blink_cycle,
						   (GSourceFunc)vte_invalidate_cursor_periodic,
						   terminal,
						   NULL);
	return FALSE;
}

static void
vte_view_buffer_contents_changed(VteView *terminal)
{
        /* Update dingus match set. */
        vte_view_match_contents_clear(terminal);
        if (terminal->pvt->mouse_cursor_visible) {
                vte_view_match_hilite_update(terminal,
                                terminal->pvt->mouse_last_x,
                                terminal->pvt->mouse_last_y);
        }
}

/* Emit a "selection_changed" signal. */
static void
vte_view_emit_selection_changed(VteView *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `selection-changed'.\n");
	g_signal_emit_by_name(terminal, "selection-changed");
}

/* Emit a "commit" signal. */
static void
vte_buffer_emit_commit(VteBuffer *buffer, const gchar *text, guint length)
{
	const char *result = NULL;
	char *wrapped = NULL;

	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `commit' of %d bytes.\n", length);

	if (length == (guint)-1) {
		length = strlen(text);
		result = text;
	} else {
		result = wrapped = g_slice_alloc(length + 1);
		memcpy(wrapped, text, length);
		wrapped[length] = '\0';
	}

	g_signal_emit(buffer, buffer_signals[BUFFER_COMMIT], 0,
                      result, length);

	if(wrapped)
		g_slice_free1(length+1, wrapped);
}

/* Emit an "emulation-changed" signal. */
static void
vte_buffer_emit_emulation_changed(VteBuffer *buffer)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `emulation-changed'.\n");
	g_signal_emit(buffer, buffer_signals[BUFFER_EMULATION_CHANGED], 0);
        g_object_notify(G_OBJECT(buffer), "emulation");
}

/* Emit an "encoding-changed" signal. */
static void
vte_buffer_emit_encoding_changed(VteBuffer *buffer)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `encoding-changed'.\n");
	g_signal_emit(buffer, buffer_signals[BUFFER_ENCODING_CHANGED], 0);
        g_object_notify(G_OBJECT(buffer), "encoding");
}

/* Emit a "child-exited" signal. */
static void
vte_buffer_emit_child_exited(VteBuffer *buffer,
                             int status)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `child-exited'.\n");
	g_signal_emit(buffer, buffer_signals[BUFFER_CHILD_EXITED], 0, status);
}

/* Emit a "contents_changed" signal. */
static void
vte_buffer_emit_contents_changed(VteBuffer *buffer)
{
	if (buffer->pvt->contents_changed_pending) {
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting `contents-changed'.\n");
		g_signal_emit(buffer, buffer_signals[BUFFER_CONTENTS_CHANGED], 0);
		buffer->pvt->contents_changed_pending = FALSE;
	}
}

void
_vte_buffer_queue_contents_changed(VteBuffer *buffer)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Queueing `contents-changed'.\n");
	buffer->pvt->contents_changed_pending = TRUE;
}

/* Emit a "cursor_moved" signal. */
static void
vte_buffer_emit_cursor_moved(VteBuffer *buffer)
{
	if (buffer->pvt->cursor_moved_pending) {
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting `cursor-moved'.\n");
		g_signal_emit(buffer, buffer_signals[BUFFER_CURSOR_MOVED], 0);
		buffer->pvt->cursor_moved_pending = FALSE;
	}
}

static void
vte_view_queue_cursor_moved(VteView *terminal)
{
        VteBuffer *buffer;

        buffer = terminal->pvt->buffer;

	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Queueing `cursor-moved'.\n");
	buffer->pvt->cursor_moved_pending = TRUE;
}

/* FIXMEchpe: why is this doing GDK threads? */
static gboolean
vte_buffer_emit_eof(VteBuffer *buffer)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `eof'.\n");
	gdk_threads_enter ();
	g_signal_emit(buffer, buffer_signals[BUFFER_EOF], 0);
	gdk_threads_leave ();

	return FALSE;
}
/* Emit a "eof" signal. */
static void
vte_buffer_queue_eof(VteBuffer *buffer)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Queueing `eof'.\n");
	g_idle_add_full (G_PRIORITY_HIGH,
		(GSourceFunc) vte_buffer_emit_eof,
		g_object_ref (buffer),
		g_object_unref);
}

/* Emit a "char-size-changed" signal. */
static void
vte_view_emit_char_size_changed(VteView *terminal,
				    guint width, guint height)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `char-size-changed'.\n");
	g_signal_emit_by_name(terminal, "char-size-changed",
			      width, height);
/*         g_object_notify(G_OBJECT(terminal), "char-size"); */
}

/* Emit a "status-line-changed" signal. */
static void
_vte_buffer_emit_status_line_changed(VteBuffer *buffer)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `status-line-changed'.\n");
	g_signal_emit(buffer, buffer_signals[BUFFER_STATUS_LINE_CHANGED], 0);
}

/* Emit an "increase-font-size" signal. */
static void
vte_view_emit_increase_font_size(VteView *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `increase-font-size'.\n");
	g_signal_emit_by_name(terminal, "increase-font-size");
}

/* Emit a "decrease-font-size" signal. */
static void
vte_view_emit_decrease_font_size(VteView *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `decrease-font-size'.\n");
	g_signal_emit_by_name(terminal, "decrease-font-size");
}

/* Emit a "text-inserted" signal. */
void
_vte_buffer_emit_text_inserted(VteBuffer *buffer)
{
	if (!buffer->pvt->accessible_emit) {
		return;
	}
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `text-inserted'.\n");
	g_signal_emit(buffer, buffer_signals[BUFFER_TEXT_INSERTED], 0);
}

/* Emit a "text-deleted" signal. */
void
_vte_buffer_emit_text_deleted(VteBuffer *buffer)
{
	if (!buffer->pvt->accessible_emit) {
		return;
	}
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `text-deleted'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_TEXT_DELETED], 0);
}

/* Emit a "text-modified" signal. */
static void
vte_buffer_emit_text_modified(VteBuffer *buffer)
{
	if (!buffer->pvt->accessible_emit) {
		return;
	}
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `text-modified'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_TEXT_MODIFIED], 0);
}

void
_vte_buffer_emit_bell(VteBuffer *buffer, VteBellType bell_type)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `bell'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_BELL], 0, bell_type);
}

/* Emit a "text-scrolled" signal. */
static void
vte_view_emit_text_scrolled(VteView *terminal, gint delta)
{
        VteBuffer *buffer = terminal->pvt->buffer;

	if (!buffer->pvt->accessible_emit) {
		return;
	}
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `text-scrolled'(%d).\n", delta);
	g_signal_emit_by_name(terminal, "text-scrolled", delta);
}

/* Emit a "deiconify-window" signal. */
void
_vte_buffer_emit_deiconify_window(VteBuffer *buffer)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `deiconify-window'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_DEICONIFY_WINDOW], 0);
}

/* Emit a "iconify-window" signal. */
void
_vte_buffer_emit_iconify_window(VteBuffer *buffer)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `iconify-window'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_ICONIFY_WINDOW], 0);
}

/* Emit a "raise-window" signal. */
void
_vte_buffer_emit_raise_window(VteBuffer *buffer)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `raise-window'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_RAISE_WINDOW], 0);
}

/* Emit a "lower-window" signal. */
void
_vte_buffer_emit_lower_window(VteBuffer *buffer)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `lower-window'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_LOWER_WINDOW], 0);
}

/* Emit a "maximize-window" signal. */
void
_vte_buffer_emit_maximize_window(VteBuffer *buffer)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `maximize-window'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_MAXIMIZE_WINDOW], 0);
}

/* Emit a "refresh-window" signal. */
void
_vte_buffer_emit_refresh_window(VteBuffer *buffer)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `refresh-window'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_REFRESH_WINDOW], 0);
}

/* Emit a "restore-window" signal. */
void
_vte_buffer_emit_restore_window(VteBuffer *buffer)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `restore-window'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_RESTORE_WINDOW], 0);
}

/* Emit a "move-window" signal.  (Pixels.) */
void
_vte_buffer_emit_move_window(VteBuffer *buffer, guint x, guint y)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `move-window'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_MOVE_WINDOW], 0, x, y);
}

/* Emit a "resize-window" signal.  (cells.) */
void
_vte_buffer_emit_resize_window(VteBuffer *buffer,
                               guint cols,
                               guint rows)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `resize-window'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_RESIZE_WINDOW], 0,
                      cols, rows);
}

static void
vte_view_emit_copy_clipboard(VteView *terminal)
{
  g_signal_emit (terminal, signals[TERMINAL_COPY_CLIPBOARD], 0);
}

static void
vte_view_emit_paste_clipboard(VteView *terminal)
{
  g_signal_emit (terminal, signals[TERMINAL_PASTE_CLIPBOARD], 0);
}

static void
vte_view_emit_copy_primary(VteView *terminal)
{
  g_signal_emit (terminal, signals[TERMINAL_COPY_PRIMARY], 0);
}

static void
vte_view_emit_paste_primary(VteView *terminal)
{
  g_signal_emit (terminal, signals[TERMINAL_PASTE_PRIMARY], 0);
}

static void
vte_view_real_copy_clipboard(VteView *terminal)
{
        _vte_debug_print(VTE_DEBUG_SELECTION, "Copying to CLIPBOARD.\n");
        vte_view_copy_clipboard(terminal,
                                    gtk_widget_get_clipboard(&terminal->widget,
                                                             GDK_SELECTION_CLIPBOARD));
}

static void
vte_view_real_paste_clipboard(VteView *terminal)
{
        _vte_debug_print(VTE_DEBUG_SELECTION, "Pasting CLIPBOARD.\n");
        vte_view_paste_clipboard(terminal,
                                     gtk_widget_get_clipboard(&terminal->widget,
                                                              GDK_SELECTION_CLIPBOARD));
}

static void
vte_view_real_copy_primary(VteView *terminal)
{
        _vte_debug_print(VTE_DEBUG_SELECTION, "Copying to PRIMARY.\n");
        vte_view_copy_clipboard(terminal,
                                    gtk_widget_get_clipboard(&terminal->widget,
                                                             GDK_SELECTION_PRIMARY));
}

static void
vte_view_real_paste_primary(VteView *terminal)
{
        _vte_debug_print(VTE_DEBUG_SELECTION, "Pasting PRIMARY.\n");
        vte_view_paste_clipboard(terminal,
                                     gtk_widget_get_clipboard(&terminal->widget,
                                                              GDK_SELECTION_PRIMARY));
}

/* Deselect anything which is selected and refresh the screen if needed. */
static void
vte_view_deselect_all(VteView *terminal)
{
	if (terminal->pvt->has_selection) {
		gint sx, sy, ex, ey;

		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Deselecting all text.\n");

		terminal->pvt->has_selection = FALSE;
		/* Don't free the current selection, as we need to keep
		 * hold of it for async copying from the clipboard. */

		vte_view_emit_selection_changed(terminal);

		sx = terminal->pvt->selection_start.col;
		sy = terminal->pvt->selection_start.row;
		ex = terminal->pvt->selection_end.col;
		ey = terminal->pvt->selection_end.row;
		_vte_invalidate_region(terminal,
				MIN (sx, ex), MAX (sx, ex),
				MIN (sy, ey),   MAX (sy, ey),
				FALSE);
	}
}

/* Remove a tabstop. */
void
_vte_buffer_clear_tabstop(VteBuffer *buffer,
                          int column)
{
	g_assert(VTE_IS_BUFFER(buffer));
	if (buffer->pvt->tabstops != NULL) {
		/* Remove a tab stop from the hash table. */
		g_hash_table_remove(buffer->pvt->tabstops,
				    GINT_TO_POINTER(2 * column + 1));
	}
}

void
_vte_buffer_clear_tabstops(VteBuffer *buffer)
{
        if (buffer->pvt->tabstops != NULL) {
                g_hash_table_destroy(buffer->pvt->tabstops);
                buffer->pvt->tabstops = NULL;
        }
}

/* Check if we have a tabstop at a given position. */
gboolean
_vte_buffer_get_tabstop(VteBuffer *buffer,
                        int column)
{
	gpointer hash;
	g_assert(VTE_IS_BUFFER(buffer));
	if (buffer->pvt->tabstops != NULL) {
		hash = g_hash_table_lookup(buffer->pvt->tabstops,
					   GINT_TO_POINTER(2 * column + 1));
		return (hash != NULL);
	} else {
		return FALSE;
	}
}

/* Reset the set of tab stops to the default. */
void
_vte_buffer_set_tabstop(VteBuffer *buffer,
                        int column)
{
	g_assert(VTE_IS_BUFFER(buffer));
	if (buffer->pvt->tabstops != NULL) {
		/* Just set a non-NULL pointer for this column number. */
		g_hash_table_insert(buffer->pvt->tabstops,
				    GINT_TO_POINTER(2 * column + 1),
				    buffer);
	}
}

/* Reset the set of tab stops to the default. */
static void
vte_buffer_set_default_tabstops(VteBuffer *buffer)
{
	int i, width = 0;
	if (buffer->pvt->tabstops != NULL) {
		g_hash_table_destroy(buffer->pvt->tabstops);
	}
	buffer->pvt->tabstops = g_hash_table_new(NULL, NULL);
	if (buffer->pvt->termcap != NULL) {
		width = _vte_termcap_find_numeric(buffer->pvt->termcap,
						  buffer->pvt->emulation,
						  "it");
	}
	if (width == 0) {
		width = VTE_TAB_WIDTH;
	}
	for (i = 0; i <= VTE_TAB_MAX; i += width) {
		_vte_buffer_set_tabstop(buffer, i);
	}
}

/* Clear the cache of the screen contents we keep. */
static void
vte_view_match_contents_clear(VteView *terminal)
{
	g_assert(VTE_IS_VIEW(terminal));
	if (terminal->pvt->match_contents != NULL) {
		g_free(terminal->pvt->match_contents);
		terminal->pvt->match_contents = NULL;
	}
	if (terminal->pvt->match_attributes != NULL) {
		g_array_free(terminal->pvt->match_attributes, TRUE);
		terminal->pvt->match_attributes = NULL;
	}
	vte_view_match_hilite_clear(terminal);
}

/* Refresh the cache of the screen contents we keep. */
static gboolean
always_selected(VteBuffer *buffer, glong column, glong row, gpointer data)
{
	return TRUE;
}

static void
vte_view_match_contents_refresh(VteView *terminal)
{
	GArray *array;
	vte_view_match_contents_clear(terminal);
	array = g_array_new(FALSE, TRUE, sizeof(struct _VteCharAttributes));
	terminal->pvt->match_contents = vte_buffer_get_text(terminal->pvt->buffer,
							      always_selected,
							      NULL,
							      array);
	terminal->pvt->match_attributes = array;
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
regex_match_clear (struct vte_match_regex *regex)
{
        regex_match_clear_cursor(regex);

        g_regex_unref(regex->regex);
        regex->regex = NULL;

        regex->tag = -1;
}

static void
vte_view_set_cursor_from_regex_match(VteView *terminal, struct vte_match_regex *regex)
{
        GdkCursor *cursor = NULL;

        if (! gtk_widget_get_realized (&terminal->widget))
                return;

        switch (regex->cursor_mode) {
                case VTE_REGEX_CURSOR_GDKCURSOR:
                        if (regex->cursor.cursor != NULL) {
                                cursor = g_object_ref(regex->cursor.cursor);
                        }
                        break;
                case VTE_REGEX_CURSOR_GDKCURSORTYPE:
                        cursor = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(terminal)), regex->cursor.cursor_type);
                        break;
                case VTE_REGEX_CURSOR_NAME:
                        cursor = gdk_cursor_new_from_name(gtk_widget_get_display(GTK_WIDGET(terminal)), regex->cursor.cursor_name);
                        break;
		default:
			g_assert_not_reached ();
			return;
        }

	gdk_window_set_cursor (gtk_widget_get_window (&terminal->widget), cursor);

        if (cursor)
                g_object_unref(cursor);
}

/**
 * vte_view_match_remove_all:
 * @terminal: a #VteView
 *
 * Clears the list of regular expressions the terminal uses to highlight text
 * when the user moves the mouse cursor.
 */
void
vte_view_match_remove_all(VteView *terminal)
{
	struct vte_match_regex *regex;
	guint i;
	g_return_if_fail(VTE_IS_VIEW(terminal));
	for (i = 0; i < terminal->pvt->match_regexes->len; i++) {
		regex = &g_array_index(terminal->pvt->match_regexes,
				       struct vte_match_regex,
				       i);
		/* Unless this is a hole, clean it up. */
		if (regex->tag >= 0) {
                        regex_match_clear (regex);
		}
	}
	g_array_set_size(terminal->pvt->match_regexes, 0);
	vte_view_match_hilite_clear(terminal);
}

/**
 * vte_view_match_remove:
 * @terminal: a #VteView
 * @tag: the tag of the regex to remove
 *
 * Removes the regular expression which is associated with the given @tag from
 * the list of expressions which the terminal will highlight when the user
 * moves the mouse cursor over matching text.
 */
void
vte_view_match_remove(VteView *terminal, int tag)
{
	struct vte_match_regex *regex;
	g_return_if_fail(VTE_IS_VIEW(terminal));
	if (terminal->pvt->match_regexes->len > (guint)tag) {
		/* The tag is an index, so find the corresponding struct. */
		regex = &g_array_index(terminal->pvt->match_regexes,
				       struct vte_match_regex,
				       tag);
		/* If it's already been removed, return. */
		if (regex->tag < 0) {
			return;
		}
		/* Remove this item and leave a hole in its place. */
                regex_match_clear (regex);
	}
	vte_view_match_hilite_clear(terminal);
}

static GdkCursor *
vte_view_cursor_new(VteView *terminal, GdkCursorType cursor_type)
{
	GdkDisplay *display;
	GdkCursor *cursor;

	display = gtk_widget_get_display(&terminal->widget);
	cursor = gdk_cursor_new_for_display(display, cursor_type);
	return cursor;
}

/**
 * vte_view_match_add_gregex:
 * @terminal: a #VteView
 * @regex: a #GRegex
 * @flags: the #GRegexMatchFlags to use when matching the regex
 *
 * Adds the regular expression @regex to the list of matching expressions.  When the
 * user moves the mouse cursor over a section of displayed text which matches
 * this expression, the text will be highlighted.
 *
 * Returns: an integer associated with this expression
 */
int
vte_view_match_add_gregex(VteView *terminal, GRegex *regex, GRegexMatchFlags flags)
{
	VteViewPrivate *pvt;
	struct vte_match_regex new_regex_match, *regex_match;
	guint ret, len;

	g_return_val_if_fail(VTE_IS_VIEW(terminal), -1);
	g_return_val_if_fail(regex != NULL, -1);

        pvt = terminal->pvt;

	/* Search for a hole. */
        len = pvt->match_regexes->len;
	for (ret = 0; ret < len; ret++) {
		regex_match = &g_array_index(pvt->match_regexes,
                                             struct vte_match_regex,
                                             ret);
		if (regex_match->tag == -1) {
			break;
		}
	}

	/* Set the tag to the insertion point. */
        new_regex_match.regex = g_regex_ref(regex);
        new_regex_match.match_flags = flags;
	new_regex_match.tag = ret;
        new_regex_match.cursor_mode = VTE_REGEX_CURSOR_GDKCURSORTYPE;
        new_regex_match.cursor.cursor_type = VTE_DEFAULT_CURSOR;
	if (ret < pvt->match_regexes->len) {
		/* Overwrite. */
		g_array_index(pvt->match_regexes,
			      struct vte_match_regex,
			      ret) = new_regex_match;
	} else {
		/* Append. */
		g_array_append_val(pvt->match_regexes, new_regex_match);
	}

	return new_regex_match.tag;
}

/**
 * vte_view_match_set_cursor:
 * @terminal: a #VteView
 * @tag: the tag of the regex which should use the specified cursor
 * @cursor: (allow-none): the #GdkCursor which the terminal should use when the pattern is
 *   highlighted, or %NULL to use the standard cursor
 *
 * Sets which cursor the terminal will use if the pointer is over the pattern
 * specified by @tag.  The terminal keeps a reference to @cursor.
 */
void
vte_view_match_set_cursor(VteView *terminal, int tag, GdkCursor *cursor)
{
	struct vte_match_regex *regex;
	g_return_if_fail(VTE_IS_VIEW(terminal));
	g_return_if_fail((guint) tag < terminal->pvt->match_regexes->len);
	regex = &g_array_index(terminal->pvt->match_regexes,
			       struct vte_match_regex,
			       tag);
        regex_match_clear_cursor(regex);
        regex->cursor_mode = VTE_REGEX_CURSOR_GDKCURSOR;
	regex->cursor.cursor = cursor ? g_object_ref(cursor) : NULL;
	vte_view_match_hilite_clear(terminal);
}

/**
 * vte_view_match_set_cursor_type:
 * @terminal: a #VteView
 * @tag: the tag of the regex which should use the specified cursor
 * @cursor_type: a #GdkCursorType
 *
 * Sets which cursor the terminal will use if the pointer is over the pattern
 * specified by @tag.
 */
void
vte_view_match_set_cursor_type(VteView *terminal,
				   int tag, GdkCursorType cursor_type)
{
	struct vte_match_regex *regex;
	g_return_if_fail(VTE_IS_VIEW(terminal));
	g_return_if_fail((guint) tag < terminal->pvt->match_regexes->len);
	regex = &g_array_index(terminal->pvt->match_regexes,
			       struct vte_match_regex,
			       tag);
        regex_match_clear_cursor(regex);
        regex->cursor_mode = VTE_REGEX_CURSOR_GDKCURSORTYPE;
	regex->cursor.cursor_type = cursor_type;
	vte_view_match_hilite_clear(terminal);
}

/**
 * vte_view_match_set_cursor_name:
 * @terminal: a #VteView
 * @tag: the tag of the regex which should use the specified cursor
 * @cursor_name: the name of the cursor
 *
 * Sets which cursor the terminal will use if the pointer is over the pattern
 * specified by @tag.
 */
void
vte_view_match_set_cursor_name(VteView *terminal,
				   int tag, const char *cursor_name)
{
	struct vte_match_regex *regex;
	g_return_if_fail(VTE_IS_VIEW(terminal));
        g_return_if_fail(cursor_name != NULL);
	g_return_if_fail((guint) tag < terminal->pvt->match_regexes->len);
	regex = &g_array_index(terminal->pvt->match_regexes,
			       struct vte_match_regex,
			       tag);
        regex_match_clear_cursor(regex);
        regex->cursor_mode = VTE_REGEX_CURSOR_NAME;
	regex->cursor.cursor_name = g_strdup (cursor_name);
	vte_view_match_hilite_clear(terminal);
}

/* Check if a given cell on the screen contains part of a matched string.  If
 * it does, return the string, and store the match tag in the optional tag
 * argument. */
static char *
vte_view_match_check_internal_gregex(VteView *terminal,
                                         long column, glong row,
                                         int *tag, int *start, int *end)
{
	gint start_blank, end_blank;
        guint i;
	int offset;
	struct vte_match_regex *regex = NULL;
	struct _VteCharAttributes *attr = NULL;
	gssize sattr, eattr;
	gchar *line, eol;
        GMatchInfo *match_info;

	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Checking for gregex match at (%ld,%ld).\n", row, column);
	if (tag != NULL) {
		*tag = -1;
	}
	if (start != NULL) {
		*start = 0;
	}
	if (end != NULL) {
		*end = 0;
	}
	/* Map the pointer position to a portion of the string. */
	eattr = terminal->pvt->match_attributes->len;
	for (offset = eattr; offset--; ) {
		attr = &g_array_index(terminal->pvt->match_attributes,
				      struct _VteCharAttributes,
				      offset);
		if (row < attr->row) {
			eattr = offset;
		}
		if (row == attr->row &&
		    column == attr->column &&
		    terminal->pvt->match_contents[offset] != ' ') {
			break;
		}
	}

	_VTE_DEBUG_IF(VTE_DEBUG_EVENTS) {
		if (offset < 0)
			g_printerr("Cursor is not on a character.\n");
		else
			g_printerr("Cursor is on character '%c' at %d.\n",
					g_utf8_get_char (terminal->pvt->match_contents + offset),
					offset);
	}

	/* If the pointer isn't on a matchable character, bug out. */
	if (offset < 0) {
		return NULL;
	}

	/* If the pointer is on a newline, bug out. */
	if ((g_ascii_isspace(terminal->pvt->match_contents[offset])) ||
	    (terminal->pvt->match_contents[offset] == '\0')) {
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Cursor is on whitespace.\n");
		return NULL;
	}

	/* Snip off any final newlines. */
	while (terminal->pvt->match_contents[eattr] == '\n' ||
			terminal->pvt->match_contents[eattr] == '\0') {
		eattr--;
	}
	/* and scan forwards to find the end of this line */
	while (!(terminal->pvt->match_contents[eattr] == '\n' ||
			terminal->pvt->match_contents[eattr] == '\0')) {
		eattr++;
	}

	/* find the start of row */
	if (row == 0) {
		sattr = 0;
	} else {
		for (sattr = offset; sattr > 0; sattr--) {
			attr = &g_array_index(terminal->pvt->match_attributes,
					      struct _VteCharAttributes,
					      sattr);
			if (row > attr->row) {
				break;
			}
		}
	}
	/* Scan backwards to find the start of this line */
	while (sattr > 0 &&
		! (terminal->pvt->match_contents[sattr] == '\n' ||
		    terminal->pvt->match_contents[sattr] == '\0')) {
		sattr--;
	}
	/* and skip any initial newlines. */
	while (terminal->pvt->match_contents[sattr] == '\n' ||
		terminal->pvt->match_contents[sattr] == '\0') {
		sattr++;
	}
	if (eattr <= sattr) { /* blank line */
		return NULL;
	}
	if (eattr <= offset || sattr > offset) {
		/* nothing to match on this line */
		return NULL;
	}
	offset -= sattr;
	eattr -= sattr;

	/* temporarily shorten the contents to this row */
	line = terminal->pvt->match_contents + sattr;
	eol = line[eattr];
	line[eattr] = '\0';

	start_blank = 0;
	end_blank = eattr;

	/* Now iterate over each regex we need to match against. */
	for (i = 0; i < terminal->pvt->match_regexes->len; i++) {
		regex = &g_array_index(terminal->pvt->match_regexes,
				       struct vte_match_regex,
				       i);
		/* Skip holes. */
		if (regex->tag < 0) {
			continue;
		}
		/* We'll only match the first item in the buffer which
		 * matches, so we'll have to skip each match until we
		 * stop getting matches. */
                if (!g_regex_match_full(regex->regex,
                                        line, -1, 0,
                                        regex->match_flags,
                                        &match_info,
                                        NULL)) {
                        g_match_info_free(match_info);
                        continue;
                }

                while (g_match_info_matches(match_info)) {
			gint ko = offset;
			gint sblank=G_MININT, eblank=G_MAXINT;
                        gint rm_so, rm_eo;

                        if (g_match_info_fetch_pos (match_info, 0, &rm_so, &rm_eo)) {
				/* The offsets should be "sane". */
				g_assert(rm_so < eattr);
				g_assert(rm_eo <= eattr);
				_VTE_DEBUG_IF(VTE_DEBUG_MISC) {
					gchar *match;
					struct _VteCharAttributes *_sattr, *_eattr;
					match = g_strndup(line + rm_so, rm_eo - rm_so);
					_sattr = &g_array_index(terminal->pvt->match_attributes,
							struct _VteCharAttributes,
							rm_so);
					_eattr = &g_array_index(terminal->pvt->match_attributes,
							struct _VteCharAttributes,
							rm_eo - 1);
					g_printerr("Match `%s' from %d(%ld,%ld) to %d(%ld,%ld) (%d).\n",
							match,
							rm_so,
							_sattr->column,
							_sattr->row,
							rm_eo - 1,
							_eattr->column,
							_eattr->row,
							offset);
					g_free(match);

				}
				/* If the pointer is in this substring,
				 * then we're done. */
				if (ko >= rm_so &&
				    ko < rm_eo) {
					gchar *result;
					if (tag != NULL) {
						*tag = regex->tag;
					}
					if (start != NULL) {
						*start = sattr + rm_so;
					}
					if (end != NULL) {
						*end = sattr + rm_eo - 1;
					}
                                        vte_view_set_cursor_from_regex_match(terminal, regex);
                                        result = g_match_info_fetch(match_info, 0);
					line[eattr] = eol;

                                        g_match_info_free(match_info);
					return result;
				}
				if (ko > rm_eo &&
						rm_eo > sblank) {
					sblank = rm_eo;
				}
				if (ko < rm_so &&
						rm_so < eblank) {
					eblank = rm_so;
				}
			}
			if (sblank > start_blank) {
				start_blank = sblank;
			}
			if (eblank < end_blank) {
				end_blank = eblank;
			}

                        g_match_info_next(match_info, NULL);
		}

                g_match_info_free(match_info);
	}
	line[eattr] = eol;
	if (start != NULL) {
		*start = sattr + start_blank;
	}
	if (end != NULL) {
		*end = sattr + end_blank;
	}
	return NULL;
}

static char *
vte_view_match_check_internal(VteView *terminal,
                                  long column, glong row,
                                  int *tag, int *start, int *end)
{
	if (terminal->pvt->match_contents == NULL) {
		vte_view_match_contents_refresh(terminal);
	}

        return vte_view_match_check_internal_gregex(terminal, column, row, tag, start, end);
}

static gboolean
rowcol_inside_match (VteView *terminal, glong row, glong col)
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

/**
 * vte_view_match_check:
 * @terminal: a #VteView
 * @column: the text column
 * @row: the text row
 * @tag: (out) (allow-none): a location to store the tag, or %NULL
 *
 * Checks if the text in and around the specified position matches any of the
 * regular expressions previously set using vte_view_match_add().  If a
 * match exists, the text string is returned and if @tag is not %NULL, the number
 * associated with the matched regular expression will be stored in @tag.
 *
 * If more than one regular expression has been set with
 * vte_view_match_add(), then expressions are checked in the order in
 * which they were added.
 *
 * Returns: (transfer full): a newly allocated string which matches one of the previously
 *   set regular expressions
 */
char *
vte_view_match_check(VteView *terminal, glong column, glong row,
			 int *tag)
{
        VteBuffer *buffer;

	long delta;
	char *ret;
	g_return_val_if_fail(VTE_IS_VIEW(terminal), NULL);

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return NULL;

	delta = buffer->pvt->screen->scroll_delta;
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Checking for match at (%ld,%ld).\n",
			row, column);
	if (rowcol_inside_match (terminal, row + delta, column)) {
		if (tag) {
			*tag = terminal->pvt->match_tag;
		}
		ret = terminal->pvt->match != NULL ?
			g_strdup (terminal->pvt->match) :
			NULL;
	} else {
		ret = vte_view_match_check_internal(terminal,
				column, row + delta,
				tag, NULL, NULL);
	}
	_VTE_DEBUG_IF(VTE_DEBUG_EVENTS) {
		if (ret != NULL) g_printerr("Matched `%s'.\n", ret);
	}
	return ret;
}

/**
 * vte_view_match_check_event:
 * @terminal: a #VteView
 * @event: a #GdkEvent
 * @tag: (out) (allow-none): a location to store the tag, or %NULL
 *
 * Checks if the text in and around the coordinates of @event matches any of the
 * regular expressions previously set using vte_view_match_add_gregex().  If a
 * match exists, the text string is returned and if @tag is not %NULL, the number
 * associated with the matched regular expression will be stored in @tag.
 *
 * If more than one regular expression has been set with
 * vte_view_match_add_gregex(), then expressions are checked in the order in
 * which they were added.
 *
 * Returns: (transfer full): a newly allocated string which matches one of the previously
 *   set regular expressions
 */
char *
vte_view_match_check_event(VteView *view,
                           GdkEvent *event,
                           int *tag)
{
        VteBufferIter iter;

        if (!vte_view_iter_from_event(view, event, &iter))
                return NULL;

        return vte_view_match_check_iter(view, &iter, tag);
}

/**
 * vte_view_match_check_iter:
 * @terminal: a #VteView
 * @event: a #VteBufferIter
 * @tag: (out) (allow-none): a location to store the tag, or %NULL
 *
 * Checks if the text in and around the coordinates of @iter matches any of the
 * regular expressions previously set using vte_view_match_add_gregex().  If a
 * match exists, the text string is returned and if @tag is not %NULL, the number
 * associated with the matched regular expression will be stored in @tag.
 *
 * If more than one regular expression has been set with
 * vte_view_match_add_gregex(), then expressions are checked in the order in
 * which they were added.
 *
 * Returns: (transfer full): a newly allocated string which matches one of the previously
 *   set regular expressions
 */
char *
vte_view_match_check_iter(VteView *view,
                          const VteBufferIter *iter,
                          int *tag)
{
        VteBufferIterReal *real_iter = (VteBufferIterReal *) iter;
        VteBuffer *buffer;
        glong row, col;
        char *ret;

        g_return_val_if_fail(VTE_IS_VIEW(view), NULL);

        buffer = view->pvt->buffer;
        if (buffer == NULL)
                return NULL;

        if (!vte_buffer_iter_is_valid(iter, view->pvt->buffer))
                return NULL;

        row = real_iter->position.row;
        col = real_iter->position.col;
        _vte_debug_print(VTE_DEBUG_EVENTS,
                        "Checking for match at (%ld,%ld).\n",
                        row, col);

        if (rowcol_inside_match (view, row, col)) {
                if (tag) {
                        *tag = view->pvt->match_tag;
                }
                ret = view->pvt->match != NULL ?
                        g_strdup (view->pvt->match) :
                        NULL;
        } else {
                ret = vte_view_match_check_internal(view,
                                                    col, row,
                                                    tag, NULL, NULL);
        }
        _VTE_DEBUG_IF(VTE_DEBUG_EVENTS) {
                if (ret != NULL)
                        g_printerr("Matched `%s'.\n", ret);
        }

        return ret;
}

/* Emit an adjustment changed signal on our adjustment object. */
static void
vte_view_emit_adjustment_changed(VteView *terminal)
{
        VteBuffer *buffer = terminal->pvt->buffer;
        VteScreen *screen = buffer->pvt->screen;

	if (terminal->pvt->adjustment_changed_pending) {
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

		/* The upper value is the number of rows which might be visible.  (Add
		 * one to the cursor offset because it's zero-based.) */
		v = MAX(_vte_ring_next(screen->row_data),
				screen->cursor_current.row + 1);
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
		glong v, delta;
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting adjustment_value_changed.\n");
		terminal->pvt->adjustment_value_changed_pending = FALSE;
		v = round (gtk_adjustment_get_value(terminal->pvt->vadjustment));
		if (v != screen->scroll_delta) {
			/* this little dance is so that the scroll_delta is
			 * updated immediately, but we still handled scrolling
			 * via the adjustment - e.g. user interaction with the
			 * scrollbar
			 */
			delta = screen->scroll_delta;
			screen->scroll_delta = v;
			gtk_adjustment_set_value(terminal->pvt->vadjustment, delta);
		}
	}
}

/* Queue an adjustment-changed signal to be delivered when convenient. */
static inline void
vte_view_queue_adjustment_changed(VteView *terminal)
{
	terminal->pvt->adjustment_changed_pending = TRUE;
	add_update_timeout (terminal);
}

static void
vte_view_queue_adjustment_value_changed(VteView *terminal, glong v)
{
        VteBuffer *buffer = terminal->pvt->buffer;
        VteScreen *screen = buffer->pvt->screen;

	if (v != screen->scroll_delta) {
		screen->scroll_delta = v;
		terminal->pvt->adjustment_value_changed_pending = TRUE;
		add_update_timeout (terminal);
	}
}

static void
vte_view_queue_adjustment_value_changed_clamped(VteView *terminal, glong v)
{
        VteBuffer *buffer;
	gdouble lower, upper;

	lower = gtk_adjustment_get_lower(terminal->pvt->vadjustment);
	upper = gtk_adjustment_get_upper(terminal->pvt->vadjustment);

        buffer = terminal->pvt->buffer;
	v = CLAMP(v, lower, MAX (lower, upper - buffer->pvt->row_count));

	vte_view_queue_adjustment_value_changed (terminal, v);
}


void
_vte_view_adjust_adjustments(VteView *terminal)
{
        VteBuffer *buffer = terminal->pvt->buffer;
        VteScreen *screen = buffer->pvt->screen;
	long delta;

	g_assert(screen != NULL);
	g_assert(screen->row_data != NULL);

	vte_view_queue_adjustment_changed(terminal);

	/* The lower value should be the first row in the buffer. */
	delta = _vte_ring_delta(screen->row_data);
	/* Snap the insert delta and the cursor position to be in the visible
	 * area.  Leave the scrolling delta alone because it will be updated
	 * when the adjustment changes. */
	screen->insert_delta = MAX(screen->insert_delta, delta);
	screen->cursor_current.row = MAX(screen->cursor_current.row,
					 screen->insert_delta);

	if (screen->scroll_delta > screen->insert_delta) {
		vte_view_queue_adjustment_value_changed(terminal,
				screen->insert_delta);
	}
}

/* Update the adjustment field of the widget.  This function should be called
 * whenever we add rows to or remove rows from the history or switch screens. */
static void
_vte_view_adjust_adjustments_full (VteView *terminal)
{
        VteBuffer *buffer;
        VteScreen *screen;
	gboolean changed = FALSE;
	gdouble v;

        buffer = terminal->pvt->buffer;
        screen = buffer->pvt->screen;

	g_assert(screen != NULL);
	g_assert(screen->row_data != NULL);

	_vte_view_adjust_adjustments(terminal);

        g_object_freeze_notify(G_OBJECT(terminal->pvt->vadjustment));

	/* The step increment should always be one. */
	v = gtk_adjustment_get_step_increment(terminal->pvt->vadjustment);
	if (v != 1) {
		_vte_debug_print(VTE_DEBUG_ADJ,
				"Changing step increment from %.0lf to %ld\n",
				v, buffer->pvt->row_count);
		gtk_adjustment_set_step_increment(terminal->pvt->vadjustment, 1);
		changed = TRUE;
	}

	/* Set the number of rows the user sees to the number of rows the
	 * user sees. */
	v = gtk_adjustment_get_page_size(terminal->pvt->vadjustment);
	if (v != buffer->pvt->row_count) {
		_vte_debug_print(VTE_DEBUG_ADJ,
				"Changing page size from %.0f to %ld\n",
				 v, buffer->pvt->row_count);
		gtk_adjustment_set_page_size(terminal->pvt->vadjustment,
					     buffer->pvt->row_count);
		changed = TRUE;
	}

	/* Clicking in the empty area should scroll one screen, so set the
	 * page size to the number of visible rows. */
	v = gtk_adjustment_get_page_increment(terminal->pvt->vadjustment);
	if (v != buffer->pvt->row_count) {
		_vte_debug_print(VTE_DEBUG_ADJ,
				"Changing page increment from "
				"%.0f to %ld\n",
				v, buffer->pvt->row_count);
		gtk_adjustment_set_page_increment(terminal->pvt->vadjustment,
						  buffer->pvt->row_count);
		changed = TRUE;
	}

	g_object_thaw_notify(G_OBJECT(terminal->pvt->vadjustment));

	if (changed)
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting adjustment_changed.\n");
}

/* Scroll a fixed number of lines up or down in the current screen. */
static void
vte_view_scroll_lines(VteView *terminal, gint lines)
{
        VteBuffer *buffer = terminal->pvt->buffer;
	glong destination;
	_vte_debug_print(VTE_DEBUG_ADJ, "Scrolling %d lines.\n", lines);
	/* Calculate the ideal position where we want to be before clamping. */
	destination = buffer->pvt->screen->scroll_delta;
	destination += lines;
	/* Tell the scrollbar to adjust itself. */
	vte_view_queue_adjustment_value_changed_clamped (terminal, destination);
}

/* Scroll a fixed number of pages up or down, in the current screen. */
static void
vte_view_scroll_pages(VteView *terminal, gint pages)
{
        VteBuffer *buffer;

        buffer = terminal->pvt->buffer;
	vte_view_scroll_lines(terminal, pages * buffer->pvt->row_count);
}

/* Scroll so that the scroll delta is the minimum value. */
static void
vte_view_maybe_scroll_to_top(VteView *terminal)
{
        VteBuffer *buffer;

        buffer = terminal->pvt->buffer;
	vte_view_queue_adjustment_value_changed (terminal,
			_vte_ring_delta(buffer->pvt->screen->row_data));
}

static void
vte_view_maybe_scroll_to_bottom(VteView *terminal)
{
        VteBuffer *buffer = terminal->pvt->buffer;
	glong delta;
	delta = buffer->pvt->screen->insert_delta;
	vte_view_queue_adjustment_value_changed (terminal, delta);
	_vte_debug_print(VTE_DEBUG_ADJ,
			"Snapping to bottom of screen\n");
}

/**
 * vte_buffer_set_encoding:
 * @buffer: a #VteBuffer
 * @codeset: (allow-none): a valid #GIConv target, or %NULL to use the default encoding
 *
 * Changes the encoding the terminal will expect data from the child to
 * be encoded with.  For certain terminal types, applications executing in the
 * terminal can change the encoding.  The default encoding is defined by the
 * application's locale settings.
 */
void
vte_buffer_set_encoding(VteBuffer *buffer,
                        const char *codeset)
{
        VteBufferPrivate *pvt;
        GObject *object;
	const char *old_codeset;
	VteConv conv;
	char *obuf1, *obuf2;
	gsize bytes_written;

	g_return_if_fail(VTE_IS_BUFFER(buffer));

        object = G_OBJECT(buffer);
        pvt = buffer->pvt;

	old_codeset = pvt->encoding;
	if (codeset == NULL) {
		g_get_charset(&codeset);
	}
	if ((old_codeset != NULL) && (strcmp(codeset, old_codeset) == 0)) {
		/* Nothing to do! */
		return;
	}

        g_object_freeze_notify(object);

	/* Open new conversions. */
	conv = _vte_conv_open(codeset, "UTF-8");
	if (conv == VTE_INVALID_CONV) {
		g_warning(_("Unable to convert characters from %s to %s."),
			  "UTF-8", codeset);
		/* fallback to no conversion */
		conv = _vte_conv_open(codeset = "UTF-8", "UTF-8");
	}
	if (buffer->pvt->outgoing_conv != VTE_INVALID_CONV) {
		_vte_conv_close(buffer->pvt->outgoing_conv);
	}
	buffer->pvt->outgoing_conv = conv;

	/* Set the terminal's encoding to the new value. */
        buffer->pvt->encoding = g_intern_string(codeset);

	/* Convert any buffered output bytes. */
        if ((_vte_byte_array_length(buffer->pvt->outgoing) > 0) &&
	    (old_codeset != NULL)) {
		/* Convert back to UTF-8. */
                obuf1 = g_convert((gchar *)buffer->pvt->outgoing->data,
                                  _vte_byte_array_length(buffer->pvt->outgoing),
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
				_vte_byte_array_clear(buffer->pvt->outgoing);
				_vte_byte_array_append(buffer->pvt->outgoing,
						   obuf2, bytes_written);
				g_free(obuf2);
			}
			g_free(obuf1);
		}
	}

	/* Set the encoding for incoming text. */
	_vte_iso2022_state_set_codeset(buffer->pvt->iso2022,
				       buffer->pvt->encoding);

	_vte_debug_print(VTE_DEBUG_IO,
			"Set terminal encoding to `%s'.\n",
			buffer->pvt->encoding);
	vte_buffer_emit_encoding_changed(buffer);

        g_object_thaw_notify(object);
}

/**
 * vte_buffer_get_encoding:
 * @buffer: a #VteBuffer
 *
 * Determines the name of the encoding in which the buffer expects data to be
 * encoded.
 *
 * Returns: (transfer none): the current encoding for the buffer
 */
const char *
vte_buffer_get_encoding(VteBuffer *buffer)
{
	g_return_val_if_fail(VTE_IS_BUFFER(buffer), NULL);
	return buffer->pvt->encoding;
}

static inline VteRowData *
vte_buffer_insert_rows (VteBuffer *buffer, guint cnt)
{
	VteRowData *row;
	do {
		row = _vte_buffer_ring_append (buffer, FALSE);
	} while(--cnt);
	return row;
}

/* Make sure we have enough rows and columns to hold data at the current
 * cursor position. */
VteRowData *
_vte_buffer_ensure_row (VteBuffer *buffer)
{
	VteRowData *row;
	VteScreen *screen;
	gint delta;
	glong v;

	/* Must make sure we're in a sane area. */
	screen = buffer->pvt->screen;
	v = screen->cursor_current.row;

	/* Figure out how many rows we need to add. */
	delta = v - _vte_ring_next(screen->row_data) + 1;
	if (delta > 0) {
		row = vte_buffer_insert_rows (buffer, delta);
		_vte_buffer_view_adjust_adjustments(buffer);
	} else {
		/* Find the row the cursor is in. */
		row = _vte_ring_index_writable (screen->row_data, v);
	}
	g_assert(row != NULL);

	return row;
}

static VteRowData *
vte_buffer_ensure_cursor(VteBuffer *buffer)
{
	VteRowData *row;

	row = _vte_buffer_ensure_row (buffer);
	_vte_row_data_fill (row, &basic_cell.cell, buffer->pvt->screen->cursor_current.col);

	return row;
}

/* Update the insert delta so that the screen which includes it also
 * includes the end of the buffer. */
static void
vte_buffer_update_insert_delta(VteBuffer *buffer)
{
	long delta, rows;
	VteScreen *screen;

	screen = buffer->pvt->screen;

	/* The total number of lines.  Add one to the cursor offset
	 * because it's zero-based. */
	rows = _vte_ring_next (screen->row_data);
	delta = screen->cursor_current.row - rows + 1;
	if (G_UNLIKELY (delta > 0)) {
		vte_buffer_insert_rows (buffer, delta);
		rows = _vte_ring_next (screen->row_data);
	}

	/* Make sure that the bottom row is visible, and that it's in
	 * the buffer (even if it's empty).  This usually causes the
	 * top row to become a history-only row. */
	delta = screen->insert_delta;
	delta = MIN(delta, rows - buffer->pvt->row_count);
	delta = MAX(delta,
		    screen->cursor_current.row - (buffer->pvt->row_count - 1));
	delta = MAX(delta, _vte_ring_delta(screen->row_data));

	/* Adjust the insert delta and scroll if needed. */
	if (delta != screen->insert_delta) {
		screen->insert_delta = delta;
		_vte_buffer_view_adjust_adjustments(buffer);
	}
}

/* Show or hide the pointer. */
void
_vte_view_set_pointer_visible(VteView *terminal, gboolean visible)
{
	GdkWindow *window;
	struct vte_match_regex *regex = NULL;

	terminal->pvt->mouse_cursor_visible = visible;

        if (! gtk_widget_get_realized (&terminal->widget))
                return;

	window = gtk_widget_get_window (&terminal->widget);

	if (visible || !terminal->pvt->mouse_autohide) {
		if (terminal->pvt->mouse_tracking_mode) {
			_vte_debug_print(VTE_DEBUG_CURSOR,
					"Setting mousing cursor.\n");
			gdk_window_set_cursor (window, terminal->pvt->mouse_mousing_cursor);
		} else
		if ( (guint)terminal->pvt->match_tag < terminal->pvt->match_regexes->len) {
			regex = &g_array_index(terminal->pvt->match_regexes,
					       struct vte_match_regex,
					       terminal->pvt->match_tag);
                        vte_view_set_cursor_from_regex_match(terminal, regex);
		} else {
			_vte_debug_print(VTE_DEBUG_CURSOR,
					"Setting default mouse cursor.\n");
			gdk_window_set_cursor (window, terminal->pvt->mouse_default_cursor);
		}
	} else {
		_vte_debug_print(VTE_DEBUG_CURSOR,
				"Setting to invisible cursor.\n");
		gdk_window_set_cursor (window, terminal->pvt->mouse_inviso_cursor);
	}
}

/**
 * vte_view_new:
 *
 * Creates a new terminal widget.
 *
 * Returns: (transfer none) (type Vte.Terminal): a new #VteView object
 */
GtkWidget *
vte_view_new(void)
{
	return g_object_new(VTE_TYPE_VIEW, NULL);
}

/**
 * vte_view_set_buffer:
 * @terminal: a #VteView
 * @buffer: (allow-none): a #VteBuffer, or %NULL
 *
 * Sets @buffer as @terminal's buffer.
 */
void
vte_view_set_buffer(VteView *terminal,
                        VteBuffer *buffer)
{
        VteViewPrivate *pvt;
        VteBuffer *old_buffer;
        GObject *object;

        g_return_if_fail(VTE_IS_VIEW(terminal));
        g_return_if_fail(buffer == NULL || VTE_IS_BUFFER(buffer));

        pvt = terminal->pvt;
        if (pvt->buffer == buffer)
                return;

        object = G_OBJECT(terminal);

        g_object_freeze_notify(object);

        old_buffer = pvt->buffer;
        if (old_buffer) {
                g_signal_handlers_disconnect_by_func(old_buffer, G_CALLBACK(vte_view_beep), terminal);
                g_signal_handlers_disconnect_by_func(old_buffer, G_CALLBACK(vte_view_buffer_contents_changed), terminal);

                old_buffer->pvt->terminal = NULL;
                /* defer unref until after "buffer-changed" signal emission */
        }

        pvt->buffer = buffer;
        if (buffer) {
                g_object_ref(buffer);
                buffer->pvt->terminal = terminal;

                g_signal_connect_swapped(buffer, "bell", G_CALLBACK(vte_view_beep), terminal);
                g_signal_connect_swapped(buffer, "contents-changed", G_CALLBACK(vte_view_buffer_contents_changed), terminal);
        }

        g_object_notify(object, "buffer");

        g_signal_emit(terminal, signals[TERMINAL_BUFFER_CHANGED], 0, old_buffer);
        if (old_buffer) {
              g_object_unref(old_buffer);
        }

        g_object_thaw_notify(object);
}

/**
 * vte_view_get_buffer:
 * @terminal: a #VteView
 *
 * Returns: (transfer none): the terminal's buffer
 */
VteBuffer *
vte_view_get_buffer(VteView *terminal)
{
        g_return_val_if_fail(VTE_IS_VIEW(terminal), NULL);

        return terminal->pvt->buffer;
}

/* Set up a palette entry with a more-or-less match for the requested color. */
static void
vte_view_set_color_internal(VteView *terminal,
                                int entry,
				const GdkRGBA *proposed,
                                gboolean override)
{
        GdkRGBA *color;

	color = &terminal->pvt->palette[entry];
        if (gdk_rgba_equal(color, proposed))
		return;

        if (!override) {
                if (VTE_PALETTE_HAS_OVERRIDE(terminal->pvt->palette_set, entry))
                        return;
                VTE_PALETTE_CLEAR_OVERRIDE(terminal->pvt->palette_set, entry);
        }

	_vte_debug_print(VTE_DEBUG_MISC | VTE_DEBUG_STYLE,
			 "Set color[%d] to rgba(%.3f,%.3f,%.3f,%.3f).\n", entry,
			 proposed->red, proposed->green, proposed->blue, proposed->alpha);

	/* Save the requested color. */
        *color = *proposed;

	/* If we're not realized yet, there's nothing else to do. */
	if (! gtk_widget_get_realized (&terminal->widget)) {
		return;
	}

	/* and redraw */
	if (entry == VTE_CUR_BG)
		_vte_invalidate_cursor_once(terminal, FALSE);
	else
		_vte_invalidate_all (terminal);
}

static void
vte_view_generate_bold(const GdkRGBA *foreground,
			   const GdkRGBA *background,
			   double factor,
			   GdkRGBA *bold)
{
	double fy, fcb, fcr, by, bcb, bcr, r, g, b, a;
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
        a = (factor * foreground->alpha) + ((1.0 - factor) * background->alpha);
	_vte_debug_print(VTE_DEBUG_MISC,
                         "Calculated bold for fg(%.3f,%.3f,%.3f,%.3f) bg(%.3f,%.3f,%.3f,%.3f) is rgba(%.3f,%.3f,%.3f,%.3f) ",
			foreground->red, foreground->green, foreground->blue, foreground->alpha,
                        background->red, background->green, background->blue, background->alpha,
			r, g, b, a);
        bold->red = CLAMP (r, 0., 1.);
        bold->green = CLAMP (g, 0., 1.);
        bold->blue = CLAMP (b, 0., 1.);
        bold->alpha = CLAMP (a, 0., 1.);
	_vte_debug_print(VTE_DEBUG_MISC,
			"normed rgba(%.3f,%.3f,%.3f,%.3f).\n",
			bold->red, bold->green, bold->blue, bold->alpha);
}

/* Cleanup smart-tabs.  See vte_sequence_handler_ta() */
void
_vte_buffer_cleanup_tab_fragments_at_cursor (VteBuffer *buffer)
{
	VteRowData *row = _vte_buffer_ensure_row (buffer);
	VteScreen *screen = buffer->pvt->screen;
	long col = screen->cursor_current.col;
	const VteCell *pcell = _vte_row_data_get (row, col);

	if (G_UNLIKELY (pcell != NULL && pcell->c == '\t')) {
		long i, num_columns;
		VteCell *cell = _vte_row_data_get_writable (row, col);
		
		_vte_debug_print(VTE_DEBUG_MISC,
				 "Cleaning tab fragments at %ld",
				 col);

		/* go back to the beginning of the tab */
		while (cell->attr.fragment && col > 0)
			cell = _vte_row_data_get_writable (row, --col);

		num_columns = cell->attr.columns;
		for (i = 0; i < num_columns; i++) {
			cell = _vte_row_data_get_writable (row, col++);
			if (G_UNLIKELY (!cell))
			  break;
			*cell = screen->fill_defaults;
		}
	}
}

/* Cursor down, with scrolling. */
void
_vte_buffer_cursor_down (VteBuffer *buffer)
{
	long start, end;
	VteScreen *screen;

	screen = buffer->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = start + buffer->pvt->row_count - 1;
	}
	if (screen->cursor_current.row == end) {
		/* Match xterm and fill to the end of row when scrolling. */
		if (screen->fill_defaults.attr.back != VTE_DEF_BG) {
			VteRowData *rowdata;
			rowdata = _vte_buffer_ensure_row (buffer);
			_vte_row_data_fill (rowdata, &screen->fill_defaults, buffer->pvt->column_count);
		}

		if (screen->scrolling_restricted) {
			if (start == screen->insert_delta) {
				/* Scroll this line into the scrollback
				 * buffer by inserting a line at the next
				 * line and scrolling the area up. */
				screen->insert_delta++;
				screen->scroll_delta++;
				screen->cursor_current.row++;
				/* update start and end, as they are relative
				 * to insert_delta. */
				start++;
				end++;
				_vte_buffer_ring_insert (buffer, screen->cursor_current.row, FALSE);
				/* Force the areas below the region to be
				 * redrawn -- they've moved. */
				_vte_buffer_view_scroll_region(buffer, start,
							    end - start + 1, 1);
				/* Force scroll. */
				_vte_buffer_view_adjust_adjustments(buffer);
			} else {
				/* If we're at the bottom of the scrolling
				 * region, add a line at the top to scroll the
				 * bottom off. */
				_vte_buffer_ring_remove (buffer, start);
				_vte_buffer_ring_insert (buffer, end, TRUE);
				/* Update the display. */
				_vte_buffer_view_scroll_region(buffer, start,
							   end - start + 1, -1);
				_vte_buffer_view_invalidate_cells(buffer,
						      0, buffer->pvt->column_count,
						      end - 2, 2);
			}
		} else {
			/* Scroll up with history. */
			screen->cursor_current.row++;
			vte_buffer_update_insert_delta(buffer);
		}

		/* Match xterm and fill the new row when scrolling. */
		if (screen->fill_defaults.attr.back != VTE_DEF_BG) {
			VteRowData *rowdata;
			rowdata = _vte_buffer_ensure_row (buffer);
			_vte_row_data_fill (rowdata, &screen->fill_defaults, buffer->pvt->column_count);
		}
	} else {
		/* Otherwise, just move the cursor down. */
		screen->cursor_current.row++;
	}
}

/* Insert a single character into the stored data array. */
gboolean
_vte_buffer_insert_char(VteBuffer *buffer,
                        gunichar c,
			gboolean insert,
                        gboolean invalidate_now)
{
	VteCellAttr attr;
	VteRowData *row;
	long col;
	int columns, i;
	VteScreen *screen;
	gboolean line_wrapped = FALSE; /* cursor moved before char inserted */

	screen = buffer->pvt->screen;
	insert |= screen->insert_mode;
	invalidate_now |= insert;

	/* If we've enabled the special drawing set, map the characters to
	 * Unicode. */
	if (G_UNLIKELY (screen->alternate_charset)) {
		_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
				"Attempting charset substitution"
				"for U+%04X.\n", c);
		/* See if there's a mapping for it. */
		c = _vte_iso2022_process_single(buffer->pvt->iso2022, c, '0');
	}

	/* If this character is destined for the status line, save it. */
	if (G_UNLIKELY (screen->status_line)) {
		g_string_append_unichar(screen->status_line_contents, c);
		screen->status_line_changed = TRUE;
		return FALSE;
	}

	/* Figure out how many columns this character should occupy. */
	if (G_UNLIKELY (VTE_ISO2022_HAS_ENCODED_WIDTH(c))) {
		columns = _vte_iso2022_get_encoded_width(c);
		c &= ~VTE_ISO2022_ENCODED_WIDTH_MASK;
	} else {
		columns = _vte_iso2022_unichar_width(buffer->pvt->iso2022, c);
	}


	/* If we're autowrapping here, do it. */
	col = screen->cursor_current.col;
	if (G_UNLIKELY (columns && col + columns > buffer->pvt->column_count)) {
		if (buffer->pvt->flags.am) {
			_vte_debug_print(VTE_DEBUG_ADJ,
					"Autowrapping before character\n");
			/* Wrap. */
			/* XXX clear to the end of line */
			col = screen->cursor_current.col = 0;
			/* Mark this line as soft-wrapped. */
			row = _vte_buffer_ensure_row (buffer);
			row->attr.soft_wrapped = 1;
			_vte_buffer_cursor_down (buffer);
		} else {
			/* Don't wrap, stay at the rightmost column. */
			col = screen->cursor_current.col =
				buffer->pvt->column_count - columns;
		}
		line_wrapped = TRUE;
	}

	_vte_debug_print(VTE_DEBUG_PARSE,
			"Inserting %ld '%c' (%d/%d) (%ld+%d, %ld), delta = %ld; ",
			(long)c, c < 256 ? c : ' ',
			screen->defaults.attr.fore,
			screen->defaults.attr.back,
			col, columns, (long)screen->cursor_current.row,
			(long)screen->insert_delta);


	if (G_UNLIKELY (columns == 0)) {

		/* It's a combining mark */

		long row_num;
		VteCell *cell;

		_vte_debug_print(VTE_DEBUG_PARSE, "combining U+%04X", c);

		row_num = screen->cursor_current.row;
		row = NULL;
		if (G_UNLIKELY (col == 0)) {
			/* We are at first column.  See if the previous line softwrapped.
			 * If it did, move there.  Otherwise skip inserting. */

			if (G_LIKELY (row_num > 0)) {
				row_num--;
				row = _vte_screen_find_row_data_writable(screen, row_num);

				if (row) {
					if (!row->attr.soft_wrapped)
						row = NULL;
					else
						col = _vte_row_data_length (row);
				}
			}
		} else {
			row = _vte_screen_find_row_data_writable(screen, row_num);
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
		_vte_buffer_view_invalidate_cells(buffer,
				      col - columns,
				      columns,
				      row_num, 1);

		goto done;
	}

	/* Make sure we have enough rows to hold this data. */
	row = vte_buffer_ensure_cursor (buffer);
	g_assert(row != NULL);

	_vte_buffer_cleanup_tab_fragments_at_cursor (buffer);

	if (insert) {
		for (i = 0; i < columns; i++)
			_vte_row_data_insert (row, col + i, &screen->color_defaults);
	} else {
		_vte_row_data_fill (row, &basic_cell.cell, col + columns);
	}

	/* Convert any wide characters we may have broken into single
	 * cells. (#514632) */
	if (G_LIKELY (col > 0)) {
		glong col2 = col - 1;
		VteCell *cell = _vte_row_data_get_writable (row, col2);
		while (col2 > 0 && cell != NULL && cell->attr.fragment)
			cell = _vte_row_data_get_writable (row, --col2);
		cell->attr.columns = col - col2;
	}
	{
		glong col2 = col + columns;
		VteCell *cell = _vte_row_data_get_writable (row, col2);
		while (cell != NULL && cell->attr.fragment) {
			cell->attr.columns = 1;
			cell->c = 0;
			cell = _vte_row_data_get_writable (row, ++col2);
		}
	}

	attr = screen->defaults.attr;
	attr.columns = columns;

	if (G_UNLIKELY (c == '_' && buffer->pvt->flags.ul)) {
		const VteCell *pcell = _vte_row_data_get (row, col);
		/* Handle overstrike-style underlining. */
		if (pcell->c != 0) {
			/* restore previous contents */
			c = pcell->c;
			attr.columns = pcell->attr.columns;
			attr.fragment = pcell->attr.fragment;

			attr.underline = 1;
		}
	}


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
	_vte_row_data_shrink (row, buffer->pvt->column_count);

	/* Signal that this part of the window needs drawing. */
	if (G_UNLIKELY (invalidate_now)) {
		_vte_buffer_view_invalidate_cells(buffer,
				col - columns,
				insert ? buffer->pvt->column_count : columns,
				screen->cursor_current.row, 1);
	}


	/* If we're autowrapping *here*, do it. */
	screen->cursor_current.col = col;
        if (G_UNLIKELY (col >= buffer->pvt->column_count)) {
		if (buffer->pvt->flags.am && !buffer->pvt->flags.xn) {
			/* Wrap. */
			screen->cursor_current.col = 0;
			/* Mark this line as soft-wrapped. */
			row->attr.soft_wrapped = 1;
                        _vte_buffer_cursor_down (buffer);
		}
	}

done:
	/* We added text, so make a note of it. */
        buffer->pvt->text_inserted_flag = TRUE;

not_inserted:
	_vte_debug_print(VTE_DEBUG_ADJ|VTE_DEBUG_PARSE,
			"insertion delta => %ld.\n",
			(long)screen->insert_delta);
	return line_wrapped;
}

static void
vte_buffer_child_watch_cb(GPid pid,
                          int status,
                          VteBuffer *buffer)
{
	if (pid == buffer->pvt->pty_pid) {
                VteBufferPrivate *pvt = buffer->pvt;
                GObject *object = G_OBJECT(buffer);

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

		pvt->child_watch_source = 0;
		pvt->pty_pid = -1;

		/* Close out the PTY. */
                vte_buffer_set_pty(buffer, NULL);

		/* Tell observers what's happened. */
		vte_buffer_emit_child_exited(buffer, status);

                g_object_thaw_notify(object);
                g_object_unref(object);

                /* Note: @buffer may be destroyed at this point */
	}
}

static void mark_input_source_invalid(VteBuffer *buffer)
{
	_vte_debug_print (VTE_DEBUG_IO, "removed poll of vte_buffer_io_read\n");
	buffer->pvt->pty_input_source = 0;
}

static void
_vte_buffer_connect_pty_read(VteBuffer *buffer)
{
        VteBufferPrivate *pvt = buffer->pvt;

	if (pvt->pty_channel == NULL) {
		return;
	}

	if (pvt->pty_input_source == 0) {
		_vte_debug_print (VTE_DEBUG_IO, "polling vte_buffer_io_read\n");
		pvt->pty_input_source =
			g_io_add_watch_full(pvt->pty_channel,
					    VTE_CHILD_INPUT_PRIORITY,
					    G_IO_IN | G_IO_HUP,
					    (GIOFunc) vte_buffer_io_read,
					    buffer,
					    (GDestroyNotify) mark_input_source_invalid);
	}
}

static void mark_output_source_invalid(VteBuffer *buffer)
{
	_vte_debug_print (VTE_DEBUG_IO, "removed poll of vte_buffer_io_write\n");
	buffer->pvt->pty_output_source = 0;
}

static void
_vte_buffer_connect_pty_write(VteBuffer *buffer)
{
        VteBufferPrivate *pvt = buffer->pvt;

        g_assert(pvt->pty != NULL);
	if (pvt->pty_channel == NULL) {
		pvt->pty_channel =
			g_io_channel_unix_new(vte_pty_get_fd(pvt->pty));
	}

	if (pvt->pty_output_source == 0) {
		if (vte_buffer_io_write (pvt->pty_channel,
					     G_IO_OUT,
					     buffer))
		{
			_vte_debug_print (VTE_DEBUG_IO, "polling vte_buffer_io_write\n");
			pvt->pty_output_source =
				g_io_add_watch_full(pvt->pty_channel,
						    VTE_CHILD_OUTPUT_PRIORITY,
						    G_IO_OUT,
						    (GIOFunc) vte_buffer_io_write,
						    buffer,
						    (GDestroyNotify) mark_output_source_invalid);
		}
	}
}

static void
_vte_buffer_disconnect_pty_read(VteBuffer *buffer)
{
        VteBufferPrivate *pvt = buffer->pvt;

	if (pvt->pty_input_source != 0) {
		_vte_debug_print (VTE_DEBUG_IO, "disconnecting poll of vte_buffer_io_read\n");
		g_source_remove(pvt->pty_input_source);
		pvt->pty_input_source = 0;
	}
}

static void
_vte_buffer_disconnect_pty_write(VteBuffer *buffer)
{
        VteBufferPrivate *pvt = buffer->pvt;

	if (pvt->pty_output_source != 0) {
		_vte_debug_print (VTE_DEBUG_IO, "disconnecting poll of vte_buffer_io_write\n");

		g_source_remove(pvt->pty_output_source);
		pvt->pty_output_source = 0;
	}
}

/**
 * vte_buffer_pty_new_sync:
 * @buffer: a #VteBuffer
 * @flags: flags from #VtePtyFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Creates a new #VtePty, and sets the emulation property
 * from #VteBuffer:emulation.
 *
 * See vte_pty_new() for more information.
 *
 * Returns: (transfer full): a new #VtePty
 */
VtePty *
vte_buffer_pty_new_sync(VteBuffer *buffer,
                          VtePtyFlags flags,
                          GCancellable *cancellable,
                          GError **error)
{
        VtePty *pty;

        g_return_val_if_fail(VTE_IS_BUFFER(buffer), NULL);

        pty = vte_pty_new_sync(flags, cancellable, error);
        if (pty == NULL)
                return NULL;

        vte_pty_set_term(pty, vte_buffer_get_emulation(buffer));

        return pty;
}

/**
 * vte_buffer_watch_child:
 * @buffer: a #VteBuffer
 * @child_pid: a #GPid
 *
 * Watches @child_pid. When the process exists, the #VteBuffer::child-exited
 * signal will be called with the child's exit status.
 *
 * Prior to calling this function, a #VtePty must have been set in @buffer
 * using vte_buffer_set_pty().
 * When the child exits, the buffer's #VtePty will be set to %NULL.
 *
 * Note: g_child_watch_add() or g_child_watch_add_full() must not have
 * been called for @child_pid, nor a #GSource for it been created with
 * g_child_watch_source_new().
 *
 * Note: when using the g_spawn_async() family of functions,
 * the %G_SPAWN_DO_NOT_REAP_CHILD flag MUST have been passed.
 */
void
vte_buffer_watch_child (VteBuffer *buffer,
                        GPid child_pid)
{
        VteBufferPrivate *pvt;

        g_return_if_fail(VTE_IS_BUFFER(buffer));
        g_return_if_fail(child_pid != -1);

        pvt = buffer->pvt;
        g_return_if_fail(pvt->pty != NULL);

        // FIXMEchpe: support passing child_pid = -1 to remove the wathch

        /* Set this as the child's pid. */
        pvt->pty_pid = child_pid;

        /* Catch a child-exited signal from the child pid. */
        if (pvt->child_watch_source != 0) {
                g_source_remove (pvt->child_watch_source);
        }
        pvt->child_watch_source =
                g_child_watch_add_full(G_PRIORITY_HIGH,
                                       child_pid,
                                       (GChildWatchFunc)vte_buffer_child_watch_cb,
                                       buffer, NULL);

        /* FIXMEchpe: call vte_buffer_set_size here? */
        /* FIXMEchpe: probably not; that's the job of set_pty. */
}

/**
 * vte_get_user_shell:
 *
 * Gets the user's shell, or %NULL. In the latter case, the
 * system default (usually "/bin/sh") should be used.
 *
 * Returns: (transfer full) (type filename): a newly allocated string with the
 *   user's shell, or %NULL
 */
char *
vte_get_user_shell (void)
{
	struct passwd *pwd;

	pwd = getpwuid(getuid());
        if (pwd && pwd->pw_shell)
                return g_strdup (pwd->pw_shell);

        return NULL;
}

/**
 * vte_buffer_spawn_sync:
 * @buffer: a #VteBuffer
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
 * pseudo-buffer.  The @argv and @envv lists should be %NULL-terminated.
 * The "TERM" environment variable is automatically set to reflect the
 * buffer widget's emulation setting.
 * @pty_flags controls logging the session to the specified system log files.
 *
 * Note that %G_SPAWN_DO_NOT_REAP_CHILD will always be added to @spawn_flags.
 *
 * Note that unless @spawn_flags contains %G_SPAWN_LEAVE_DESCRIPTORS_OPEN, all file
 * descriptors except stdin/stdout/stderr will be closed before calling exec()
 * in the child.
 *
 * See vte_pty_new(), g_spawn_async() and vte_buffer_watch_child() for more information.
 *
 * Returns: %TRUE on success, or %FALSE on error with @error filled in
 */
gboolean
vte_buffer_spawn_sync(VteBuffer *buffer,
                               VtePtyFlags pty_flags,
                               const char *working_directory,
                               char **argv,
                               char **envv,
                               GSpawnFlags spawn_flags,
                               GSpawnChildSetupFunc child_setup,
                               gpointer child_setup_data,
                               GPid *child_pid /* out */,
                               GCancellable *cancellable,
                               GError **error)
{
        VtePty *pty;
        GPid pid;

        g_return_val_if_fail(VTE_IS_BUFFER(buffer), FALSE);
        g_return_val_if_fail(argv != NULL, FALSE);
        g_return_val_if_fail(child_setup_data == NULL || child_setup, FALSE);
        g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

        pty = vte_buffer_pty_new_sync(buffer, pty_flags, cancellable, error);
        if (pty == NULL)
                return FALSE;

        /* FIXMEchpe: is this flag needed */
        spawn_flags |= G_SPAWN_CHILD_INHERITS_STDIN;

        if (!__vte_pty_spawn(pty,
                             working_directory,
                             argv,
                             envv,
                             spawn_flags,
                             child_setup, child_setup_data,
                             &pid,
                             error)) {
                g_object_unref(pty);
                return FALSE;
        }

        vte_buffer_set_pty(buffer, pty);
        vte_buffer_watch_child(buffer, pid);
        g_object_unref (pty);

        if (child_pid)
                *child_pid = pid;

        return TRUE;
}

/* Handle an EOF from the client. */
static void
vte_view_eof(GIOChannel *channel, VteView *terminal)
{
        GObject *object = G_OBJECT(terminal);

        g_object_freeze_notify(object);

        vte_buffer_set_pty(terminal->pvt->buffer, NULL);

	/* Emit a signal that we read an EOF. */
	vte_buffer_queue_eof(terminal->pvt->buffer);

        g_object_thaw_notify(object);
}

/* Reset the input method context. */
static void
vte_view_im_reset(VteView *terminal)
{
        VteViewPrivate *pvt = terminal->pvt;

	if (gtk_widget_get_realized (&terminal->widget)) {
		gtk_im_context_reset(pvt->im_context);
		if (pvt->im_preedit != NULL) {
			g_free(pvt->im_preedit);
			pvt->im_preedit = NULL;
		}
		if (pvt->im_preedit_attrs != NULL) {
			pango_attr_list_unref(pvt->im_preedit_attrs);
			pvt->im_preedit_attrs = NULL;
		}
	}
}

/* Emit whichever signals are called for here. */
static void
vte_buffer_emit_pending_text_signals(VteBuffer *buffer, GQuark quark)
{
	static struct {
		const char *name;
		GQuark quark;
	} non_visual_quarks[] = {
		{"mb", 0},
		{"md", 0},
		{"mr", 0},
		{"mu", 0},
		{"se", 0},
		{"so", 0},
		{"ta", 0},
		{"character-attributes", 0},
	};
	guint i;

	if (quark != 0) {
		for (i = 0; i < G_N_ELEMENTS(non_visual_quarks); i++) {
			if (non_visual_quarks[i].quark == 0) {
				non_visual_quarks[i].quark =
					g_quark_from_static_string(non_visual_quarks[i].name);
			}
			if (quark == non_visual_quarks[i].quark) {
				return;
			}
		}
	}

	if (buffer->pvt->text_modified_flag) {
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting buffered `text-modified'.\n");
		vte_buffer_emit_text_modified(buffer);
		buffer->pvt->text_modified_flag = FALSE;
	}
	if (buffer->pvt->text_inserted_flag) {
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting buffered `text-inserted'\n");
		_vte_buffer_emit_text_inserted(buffer);
		buffer->pvt->text_inserted_flag = FALSE;
	}
	if (buffer->pvt->text_deleted_flag) {
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting buffered `text-deleted'\n");
		_vte_buffer_emit_text_deleted(buffer);
		buffer->pvt->text_deleted_flag = FALSE;
	}
}

/* Process incoming data, first converting it to unicode characters, and then
 * processing control sequences. */
static void
vte_buffer_process_incoming(VteBuffer *buffer)
{
        VteView *terminal;
	VteScreen *screen;
	VteVisualPosition cursor;
	gboolean cursor_visible;
	GdkPoint bbox_topleft, bbox_bottomright;
	gunichar *wbuf, c;
	long wcount, start, delta;
	gboolean leftovers, modified, bottom, again;
	gboolean invalidated_text;
	GArray *unichars;
	struct _vte_incoming_chunk *chunk, *next_chunk, *achunk = NULL;

	_vte_debug_print(VTE_DEBUG_IO,
			"Handler processing %"G_GSIZE_FORMAT" bytes over %"G_GSIZE_FORMAT" chunks + %d bytes pending.\n",
			_vte_incoming_chunks_length(buffer->pvt->incoming),
			_vte_incoming_chunks_count(buffer->pvt->incoming),
			buffer->pvt->pending->len);
	_vte_debug_print (VTE_DEBUG_WORK, "(");

        screen = buffer->pvt->screen;
        terminal = buffer->pvt->terminal; /* FIXMEchpe cope with NULL here! */

	delta = screen->scroll_delta;
	bottom = screen->insert_delta == delta;

	/* Save the current cursor position. */
	cursor = screen->cursor_current;
	cursor_visible = buffer->pvt->cursor_visible;

	/* We should only be called when there's data to process. */
	g_assert(buffer->pvt->incoming ||
		 (buffer->pvt->pending->len > 0));

	/* Convert the data into unicode characters. */
	unichars = buffer->pvt->pending;
	for (chunk = _vte_incoming_chunks_reverse (buffer->pvt->incoming);
			chunk != NULL;
			chunk = next_chunk) {
		gsize processed;
		next_chunk = chunk->next;
		if (chunk->len == 0) {
			goto skip_chunk;
		}
		processed = _vte_iso2022_process(buffer->pvt->iso2022,
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
	buffer->pvt->incoming = chunk;

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
		GQuark quark;
		const gunichar *next;
		GValueArray *params = NULL;

		/* Try to match any control sequences. */
		_vte_matcher_match(buffer->pvt->matcher,
				   &wbuf[start],
				   wcount - start,
				   &match,
				   &next,
				   &quark,
				   &params);
		/* We're in one of three possible situations now.
		 * First, the match string is a non-empty string and next
		 * points to the first character which isn't part of this
		 * sequence. */
		if ((match != NULL) && (match[0] != '\0')) {
			/* Call the right sequence handler for the requested
			 * behavior. */
			_vte_buffer_handle_sequence(buffer,
						      match,
						      quark,
						      params);
			/* Skip over the proper number of unicode chars. */
			start = (next - wbuf);
			modified = TRUE;

			/* if we have moved during the sequence handler, restart the bbox */
			if (invalidated_text &&
					(screen->cursor_current.col > bbox_bottomright.x + VTE_CELL_BBOX_SLACK ||
					 screen->cursor_current.col < bbox_topleft.x - VTE_CELL_BBOX_SLACK     ||
					 screen->cursor_current.row > bbox_bottomright.y + VTE_CELL_BBOX_SLACK ||
					 screen->cursor_current.row < bbox_topleft.y - VTE_CELL_BBOX_SLACK)) {
				/* Clip off any part of the box which isn't already on-screen. */
				bbox_topleft.x = MAX(bbox_topleft.x, 0);
				bbox_topleft.y = MAX(bbox_topleft.y, delta);
				bbox_bottomright.x = MIN(bbox_bottomright.x,
						buffer->pvt->column_count);
				/* lazily apply the +1 to the cursor_row */
				bbox_bottomright.y = MIN(bbox_bottomright.y + 1,
						delta + buffer->pvt->row_count);

				_vte_buffer_view_invalidate_cells(buffer,
						bbox_topleft.x,
						bbox_bottomright.x - bbox_topleft.x,
						bbox_topleft.y,
						bbox_bottomright.y - bbox_topleft.y);

				invalidated_text = FALSE;
				bbox_bottomright.x = bbox_bottomright.y = -G_MAXINT;
				bbox_topleft.x = bbox_topleft.y = G_MAXINT;
			}
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
				GQuark tquark = 0;
				gunichar ctrl;
				int i;
				/* We don't want to permute it if it's another
				 * control sequence, so check if it is. */
				_vte_matcher_match(buffer->pvt->matcher,
						   next,
						   wcount - (next - wbuf),
						   &tmatch,
						   &tnext,
						   &tquark,
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
				gunichar cc = c & ~VTE_ISO2022_ENCODED_WIDTH_MASK;
				if (cc > 255) {
					g_printerr("U+%04lx\n", (long) cc);
				} else {
					if (cc > 127) {
						g_printerr("%ld = ",
								(long) cc);
					}
					if (cc < 32) {
						g_printerr("^%lc\n",
								(wint_t)cc + 64);
					} else {
						g_printerr("`%lc'\n",
								(wint_t)cc);
					}
				}
			}

			bbox_topleft.x = MIN(bbox_topleft.x,
					screen->cursor_current.col);
			bbox_topleft.y = MIN(bbox_topleft.y,
					screen->cursor_current.row);

			/* Insert the character. */
			if (G_UNLIKELY (_vte_buffer_insert_char(buffer, c,
						 FALSE, FALSE))) {
				/* line wrapped, correct bbox */
				if (invalidated_text &&
						(screen->cursor_current.col > bbox_bottomright.x + VTE_CELL_BBOX_SLACK	||
						 screen->cursor_current.col < bbox_topleft.x - VTE_CELL_BBOX_SLACK	||
						 screen->cursor_current.row > bbox_bottomright.y + VTE_CELL_BBOX_SLACK	||
						 screen->cursor_current.row < bbox_topleft.y - VTE_CELL_BBOX_SLACK)) {
					/* Clip off any part of the box which isn't already on-screen. */
					bbox_topleft.x = MAX(bbox_topleft.x, 0);
					bbox_topleft.y = MAX(bbox_topleft.y, delta);
					bbox_bottomright.x = MIN(bbox_bottomright.x,
							buffer->pvt->column_count);
					/* lazily apply the +1 to the cursor_row */
					bbox_bottomright.y = MIN(bbox_bottomright.y + 1,
							delta + buffer->pvt->row_count);

					_vte_buffer_view_invalidate_cells(buffer,
							bbox_topleft.x,
							bbox_bottomright.x - bbox_topleft.x,
							bbox_topleft.y,
							bbox_bottomright.y - bbox_topleft.y);
					bbox_bottomright.x = bbox_bottomright.y = -G_MAXINT;
					bbox_topleft.x = bbox_topleft.y = G_MAXINT;

				}
				bbox_topleft.x = MIN(bbox_topleft.x, 0);
				bbox_topleft.y = MIN(bbox_topleft.y,
						screen->cursor_current.row);
			}
			/* Add the cells over which we have moved to the region
			 * which we need to refresh for the user. */
			bbox_bottomright.x = MAX(bbox_bottomright.x,
					screen->cursor_current.col);
			/* cursor_current.row + 1 (defer until inv.) */
			bbox_bottomright.y = MAX(bbox_bottomright.y,
					screen->cursor_current.row);
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
		g_assert(screen->cursor_current.row >= screen->insert_delta);
#endif

next_match:
		if (G_LIKELY(params != NULL)) {
			/* Free any parameters we don't care about any more. */
			_vte_matcher_free_params_array(buffer->pvt->matcher,
					params);
		}
	}

	/* Remove most of the processed characters. */
	if (start < wcount) {
		g_array_remove_range(buffer->pvt->pending, 0, start);
	} else {
		g_array_set_size(buffer->pvt->pending, 0);
		/* If we're out of data, we needn't pause to let the
		 * controlling application respond to incoming data, because
		 * the main loop is already going to do that. */
	}

	if (modified) {
		/* Keep the cursor on-screen if we scroll on output, or if
		 * we're currently at the bottom of the buffer. */
		vte_buffer_update_insert_delta(buffer);
		if (terminal->pvt->scroll_on_output || bottom) {
			vte_view_maybe_scroll_to_bottom(terminal);
		}
		/* Deselect the current selection if its contents are changed
		 * by this insertion. */
		if (terminal->pvt->has_selection) {
			char *selection;
			selection =
			vte_buffer_get_text_range(buffer,
						    terminal->pvt->selection_start.row,
						    0,
						    terminal->pvt->selection_end.row,
						    buffer->pvt->column_count,
						    (VteSelectionFunc)vte_view_cell_is_selected,
						    terminal /* user data */,
						    NULL);
			if ((selection == NULL) || (terminal->pvt->selection == NULL) ||
			    (strcmp(selection, terminal->pvt->selection) != 0)) {
				vte_view_deselect_all(terminal);
			}
			g_free(selection);
		}
	}

	if (modified || (screen != buffer->pvt->screen)) {
		/* Signal that the visible contents changed. */
		_vte_buffer_queue_contents_changed(buffer);
	}

	vte_view_emit_pending_signals (terminal);

	if (invalidated_text) {
		/* Clip off any part of the box which isn't already on-screen. */
		bbox_topleft.x = MAX(bbox_topleft.x, 0);
		bbox_topleft.y = MAX(bbox_topleft.y, delta);
		bbox_bottomright.x = MIN(bbox_bottomright.x,
				buffer->pvt->column_count);
		/* lazily apply the +1 to the cursor_row */
		bbox_bottomright.y = MIN(bbox_bottomright.y + 1,
				delta + buffer->pvt->row_count);

		_vte_buffer_view_invalidate_cells(buffer,
				bbox_topleft.x,
				bbox_bottomright.x - bbox_topleft.x,
				bbox_topleft.y,
				bbox_bottomright.y - bbox_topleft.y);
	}

	if ((cursor.col != buffer->pvt->screen->cursor_current.col) ||
	    (cursor.row != buffer->pvt->screen->cursor_current.row)) {
		/* invalidate the old and new cursor positions */
		if (cursor_visible)
			_vte_invalidate_cell(terminal, cursor.col, cursor.row);
		_vte_invalidate_cursor_once(terminal, FALSE);
		_vte_check_cursor_blink(terminal);
		/* Signal that the cursor moved. */
		vte_view_queue_cursor_moved(terminal);
	} else if (cursor_visible != buffer->pvt->cursor_visible) {
		_vte_invalidate_cell(terminal, cursor.col, cursor.row);
		_vte_check_cursor_blink(terminal);
	}

	/* Tell the input method where the cursor is. */
	if (gtk_widget_get_realized (&terminal->widget)) {
		cairo_rectangle_int_t rect;
		rect.x = screen->cursor_current.col *
			 terminal->pvt->char_width + terminal->pvt->padding.left;
		rect.width = terminal->pvt->char_width;
		rect.y = (screen->cursor_current.row - delta) *
			 terminal->pvt->char_height + terminal->pvt->padding.top;
		rect.height = terminal->pvt->char_height;
		gtk_im_context_set_cursor_location(terminal->pvt->im_context,
						   &rect);
	}

	_vte_debug_print (VTE_DEBUG_WORK, ")");
	_vte_debug_print (VTE_DEBUG_IO,
			"%ld chars and %ld bytes in %"G_GSIZE_FORMAT" chunks left to process.\n",
			(long) unichars->len,
			(long) _vte_incoming_chunks_length(buffer->pvt->incoming),
			_vte_incoming_chunks_count(buffer->pvt->incoming));
}

static inline void
_vte_buffer_enable_input_source (VteBuffer *buffer)
{
        VteBufferPrivate *pvt = buffer->pvt;

	if (pvt->pty_channel == NULL) {
		return;
	}

	if (pvt->pty_input_source == 0) {
		_vte_debug_print (VTE_DEBUG_IO, "polling vte_buffer_io_read\n");
		pvt->pty_input_source =
			g_io_add_watch_full(buffer->pvt->pty_channel,
					    VTE_CHILD_INPUT_PRIORITY,
					    G_IO_IN | G_IO_HUP,
					    (GIOFunc) vte_buffer_io_read,
					    buffer,
					    (GDestroyNotify) mark_input_source_invalid);
	}
}

static void
_vte_buffer_feed_chunks (VteBuffer *buffer,
                         struct _vte_incoming_chunk *chunks)
{
	struct _vte_incoming_chunk *last;

	_vte_debug_print(VTE_DEBUG_IO, "Feed %"G_GSIZE_FORMAT" bytes, in %"G_GSIZE_FORMAT" chunks.\n",
			_vte_incoming_chunks_length(chunks),
			_vte_incoming_chunks_count(chunks));

	for (last = chunks; last->next != NULL; last = last->next) ;
	last->next = buffer->pvt->incoming;
	buffer->pvt->incoming = chunks;
}
/* Read and handle data from the child. */
static gboolean
vte_buffer_io_read(GIOChannel *channel,
		     GIOCondition condition,
		     VteBuffer *buffer)
{
        VteView *terminal = buffer->pvt->terminal;
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
			max_bytes = buffer->pvt->max_input_bytes / max_bytes;
		} else {
			max_bytes = VTE_MAX_INPUT_READ;
		}
		bytes = buffer->pvt->input_bytes;

		chunk = buffer->pvt->incoming;
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
			_vte_buffer_feed_chunks (buffer, chunks);
		}
		if (!vte_view_is_processing (terminal)) {
			gdk_threads_enter ();
			vte_view_add_process_timeout (terminal);
			gdk_threads_leave ();
		}
		buffer->pvt->pty_input_active = len != 0;
		buffer->pvt->input_bytes = bytes;
		again = bytes < max_bytes;

		_vte_debug_print (VTE_DEBUG_IO, "read %d/%d bytes, again? %s, active? %s\n",
				bytes, max_bytes,
				again ? "yes" : "no",
				buffer->pvt->pty_input_active ? "yes" : "no");
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
		if (!vte_view_is_processing (terminal)) {
			gdk_threads_enter ();
			vte_view_eof (channel, terminal);
			gdk_threads_leave ();
		} else {
			vte_view_eof (channel, terminal);
		}

		again = FALSE;
	}

	return again;
}

/**
 * vte_buffer_feed:
 * @buffer: a #VteBuffer
 * @data: (allow-none) (array length=length) (element-type guint8): a string in the buffer's current encoding
 * @length: the length of the string
 *
 * Interprets @data as if it were data received from a child process.  This
 * can either be used to drive the buffer without a child process, or just
 * to mess with your users.
 */
void
vte_buffer_feed(VteBuffer *buffer,
                const char *data,
                gssize length)
{
        g_return_if_fail(VTE_IS_BUFFER(buffer));

        if (data == NULL || length == 0)
                return;

	if (length < 0) {
		length = strlen(data);
	}

	/* If we have data, modify the incoming buffer. */
	if (length > 0) {
		struct _vte_incoming_chunk *chunk;
		if (buffer->pvt->incoming &&
				(gsize)length < sizeof (buffer->pvt->incoming->data) - buffer->pvt->incoming->len) {
			chunk = buffer->pvt->incoming;
		} else {
			chunk = get_chunk ();
			_vte_buffer_feed_chunks (buffer, chunk);
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
			_vte_buffer_feed_chunks (buffer, chunk);
		} while (1);
		vte_view_start_processing (buffer->pvt->terminal);
	}
}

/* Send locally-encoded characters to the child. */
static gboolean
vte_buffer_io_write(GIOChannel *channel,
		      GIOCondition condition,
		      VteBuffer *buffer)
{
	gssize count;
	int fd;
	gboolean leave_open;

	fd = g_io_channel_unix_get_fd(channel);

	count = write(fd, buffer->pvt->outgoing->data,
		      _vte_byte_array_length(buffer->pvt->outgoing));
	if (count != -1) {
		_VTE_DEBUG_IF (VTE_DEBUG_IO) {
			gssize i;
			for (i = 0; i < count; i++) {
				g_printerr("Wrote %c%c\n",
					((guint8)buffer->pvt->outgoing->data[i]) >= 32 ?
					' ' : '^',
                                        ((guint8)buffer->pvt->outgoing->data[i]) >= 32 ?
                                        buffer->pvt->outgoing->data[i] :
                                        ((guint8)buffer->pvt->outgoing->data[i])  + 64);
			}
		}
		_vte_byte_array_consume(buffer->pvt->outgoing, count);
	}

	if (_vte_byte_array_length(buffer->pvt->outgoing) == 0) {
		leave_open = FALSE;
	} else {
		leave_open = TRUE;
	}

	return leave_open;
}

/* Convert some arbitrarily-encoded data to send to the child. */
static void
vte_buffer_send(VteBuffer *buffer,
                const char *encoding,
                const void *data,
                gssize length,
                gboolean local_echo,
                gboolean newline_stuff)
{
	gsize icount, ocount;
	const guchar *ibuf;
	guchar *obuf, *obufptr;
	gchar *cooked;
	VteConv conv;
	long crcount, cooked_length, i;

	g_assert(VTE_IS_BUFFER(buffer));
	g_assert(encoding && strcmp(encoding, "UTF-8") == 0);

	conv = VTE_INVALID_CONV;
	if (strcmp(encoding, "UTF-8") == 0) {
		conv = buffer->pvt->outgoing_conv;
	}
	if (conv == VTE_INVALID_CONV) {
		g_warning (_("Unable to send data to child, invalid charset convertor"));
		return;
	}

	icount = length;
	ibuf =  data;
	ocount = ((length + 1) * VTE_UTF8_BPC) + 1;
	_vte_byte_array_set_minimum_size(buffer->pvt->conv_buffer, ocount);
	obuf = obufptr = buffer->pvt->conv_buffer->data;

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
			cooked = g_malloc(obuf - obufptr + crcount);
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
			vte_buffer_emit_commit(buffer, cooked, cooked_length);
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
					_vte_buffer_insert_char(buffer,
								 ucs4[i],
								 FALSE,
								 TRUE);
				}
				g_free(ucs4);
			}
		}
		/* If there's a place for it to go, add the data to the
		 * outgoing buffer. */
		if ((cooked_length > 0) && (buffer->pvt->pty != NULL)) {
			_vte_byte_array_append(buffer->pvt->outgoing,
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
			_vte_buffer_connect_pty_write(buffer);
		}
		if (crcount > 0) {
			g_free(cooked);
		}
	}
	return;
}

/**
 * vte_buffer_feed_child:
 * @buffer: a #VteBuffer
 * @text: (allow-none): data to send to the child
 * @length: length of @text in bytes, or -1 if @text is NUL-terminated
 *
 * Sends a block of UTF-8 text to the child as if it were entered by the user
 * at the keyboard.
 */
void
vte_buffer_feed_child(VteBuffer *buffer,
                      const char *text,
                      gssize length)
{
        g_return_if_fail(VTE_IS_BUFFER(buffer));

        if (text == NULL || length == 0)
                return;

	if (length < 0) {
		length = strlen(text);
	}
	if (length > 0) {
		vte_buffer_send(buffer, "UTF-8", text, length,
				  FALSE, FALSE);
	}
}

/**
 * vte_buffer_feed_child_binary:
 * @buffer: a #VteBuffer
 * @data: (allow-none) (array zero-terminated=0 length=@length) (element-type uint8): data to send to the child
 * @length: length of @data
 *
 * Sends a block of binary data to the child.
 */
void
vte_buffer_feed_child_binary(VteBuffer *buffer,
                             const char *data,
                             gsize length)
{
        g_return_if_fail(VTE_IS_BUFFER(buffer));

        if (data == NULL || length == 0)
                return;

	/* Tell observers that we're sending this to the child. */
	if (length > 0) {
		vte_buffer_emit_commit(buffer, data, length);

		/* If there's a place for it to go, add the data to the
		 * outgoing buffer. */
		if (buffer->pvt->pty != NULL) {
			_vte_byte_array_append(buffer->pvt->outgoing,
					   data, length);
			/* If we need to start waiting for the child pty to
			 * become available for writing, set that up here. */
			_vte_buffer_connect_pty_write(buffer);
		}
	}
}

static void
vte_buffer_feed_child_using_modes(VteBuffer *buffer,
				  const char *data,
                                  gssize length)
{
	if (length < 0) {
		length = strlen(data);
	}
	if (length > 0) {
		vte_buffer_send(buffer, "UTF-8", data, length,
				  !buffer->pvt->screen->sendrecv_mode,
				  buffer->pvt->screen->linefeed_mode);
	}
}

/* Send text from the input method to the child. */
static void
vte_view_im_commit(GtkIMContext *im_context, gchar *text, VteView *terminal)
{
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Input method committed `%s'.\n", text);
	vte_buffer_feed_child_using_modes(terminal->pvt->buffer, text, -1);
	/* Committed text was committed because the user pressed a key, so
	 * we need to obey the scroll-on-keystroke setting. */
	if (terminal->pvt->scroll_on_keystroke) {
		vte_view_maybe_scroll_to_bottom(terminal);
	}
}

/* We've started pre-editing. */
static void
vte_view_im_preedit_start(GtkIMContext *im_context, VteView *terminal)
{
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Input method pre-edit started.\n");
	terminal->pvt->im_preedit_active = TRUE;
}

/* We've stopped pre-editing. */
static void
vte_view_im_preedit_end(GtkIMContext *im_context, VteView *terminal)
{
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Input method pre-edit ended.\n");
	terminal->pvt->im_preedit_active = FALSE;
}

/* The pre-edit string changed. */
static void
vte_view_im_preedit_changed(GtkIMContext *im_context, VteView *terminal)
{
        VteViewPrivate *pvt = terminal->pvt;
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

	g_free(pvt->im_preedit);
	pvt->im_preedit = str;

	if (pvt->im_preedit_attrs != NULL) {
		pango_attr_list_unref(pvt->im_preedit_attrs);
	}
	pvt->im_preedit_attrs = attrs;

	pvt->im_preedit_cursor = cursor;

	_vte_invalidate_cursor_once(terminal, FALSE);
}

static void
vte_view_set_padding(VteView *terminal)
{
        VteViewPrivate *pvt = terminal->pvt;
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

/*
 * _vte_view_set_effect_color:
 * @terminal: a #VteView
 * @entry: the entry in the colour palette
 * @rgba:
 * @effect:
 * @override: whether to override an application-set colour
 *
 * Sets the entry for @entry in the terminal colour palette
 * to the given colour.
 *
 * If the colour was previously set by the terminal application
 * and @override is %FALSE, does nothing.
 */
void
_vte_view_set_effect_color(VteView *terminal,
                               int entry,
                               const GdkRGBA *rgba,
                               VteEffect effect,
                               gboolean override)
{
        VteViewPrivate *pvt = terminal->pvt;
        gboolean has_override, color_set;

        has_override = VTE_PALETTE_HAS_OVERRIDE(pvt->palette_set, entry);
        if (has_override && !override) {
                _vte_debug_print(VTE_DEBUG_STYLE,
                                 "Have color override for %d; not setting new color.\n",
                                 entry);
                return;
        }

        g_assert (rgba != NULL);

        vte_view_set_color_internal(terminal, entry, rgba, override);

        color_set = (effect == VTE_EFFECT_COLOR);
        switch (entry) {
        case VTE_CUR_BG:
                pvt->cursor_color_set = color_set;
                break;
        case VTE_DEF_HL:
                pvt->highlight_color_set = color_set;
                break;
        case VTE_REV_BG:
                pvt->reverse_color_set = color_set;
                break;
        }
}

/*
 * vte_view_set_mixed_color:
 * @terminal: a #VteView
 * @entry: the entry in the colour palette
 * @color: (allow-none): the new dim color or %NULL
 * @factor:
 * @override:
 *
 * Sets the entry in the terminal colour palette to @color, or
 * if @color is %NULL, generates  a colour from the foreground and
 * background color.
 */
static void
vte_view_set_mixed_color(VteView *terminal,
                             int entry,
                             const GdkRGBA *rgba,
                             gdouble factor,
                             gboolean override)
{
        GdkRGBA mixed;

        if (rgba == NULL) {
                vte_view_generate_bold(&terminal->pvt->palette[VTE_DEF_FG],
                                           &terminal->pvt->palette[VTE_DEF_BG],
                                           factor,
                                           &mixed);
                rgba = &mixed;
        }

        vte_view_set_color_internal(terminal, entry, rgba, override);
}

/*
 * _vte_gtk_style_context_lookup_color:
 * @context:
 * @color_name:
 * @color: a location to store the color
 *
 * Like gtk_style_context_lookup_color(), but returns the color
 * instead of boolean.
 *
 * Returns: (transfer none): @color if lookup succeeded, else %NULL
 */
static const GdkRGBA *
_vte_style_context_get_color(GtkStyleContext *context,
                             const char *color_name,
                             GdkRGBA *color)
{
  GdkRGBA *copy;

  gtk_style_context_get_style(context, color_name, &copy, NULL);
  if (copy == NULL)
    {
#if 0
      /* Put in a nice shade of magenta, to indicate something's wrong */
      color->red = color->blue = color->alpha = 1.; color->green = 0.;
      return color;
#endif
      return NULL;
    }

  *color = *copy;
  gdk_rgba_free (copy);
  return color;
}

static const char color_names[8][8] = {
        "black",
        "red",
        "green",
        "yellow",
        "blue",
        "magenta",
        "cyan",
        "white"
};

static void
vte_view_update_style_colors(VteView *terminal,
                                 gboolean override)
{
        GtkStyleContext *context;
        GdkRGBA rgba;
        const GdkRGBA *color;
        int i;
        char name[64];
        int cursor_effect, reverse_effect, selection_effect;

        context = gtk_widget_get_style_context(&terminal->widget);

        /* Get foreground and background first, since other default colours
         * may be defined in terms of these.
         */
        color = _vte_style_context_get_color(context, "foreground-color", &rgba);
        vte_view_set_color_internal(terminal, VTE_DEF_FG, color, FALSE);
        color = _vte_style_context_get_color(context, "background-color", &rgba);
        vte_view_set_color_internal(terminal, VTE_DEF_BG, color, FALSE);

        /* The 256 colour palette */

        for (i = 0; i < 8; ++i) {
                g_snprintf (name, sizeof (name), "%s-color", color_names[i]);
                color = _vte_style_context_get_color(context, name, &rgba);
                vte_view_set_color_internal(terminal, VTE_COLOR_PLAIN_OFFSET + i,
                                                color, override);
        }
        for (i = 0; i < 8; ++i) {
                g_snprintf (name, sizeof (name), "bright-%s-color", color_names[i]);
                color = _vte_style_context_get_color(context, name, &rgba);
                vte_view_set_color_internal(terminal, VTE_COLOR_BRIGHT_OFFSET + i,
                                                color, override);
        }
        for (i = 0; i < 216; ++i) {
                int r, g, b;

                r = i / 36 + 1;
                g = (i / 6) % 6 + 1;
                b = i % 6 + 1;
                g_snprintf (name, sizeof (name), "color-6-cube-%d-%d-%d-color", r, g, b);
                color = _vte_style_context_get_color(context, name, &rgba);
                vte_view_set_color_internal(terminal, VTE_COLOR_COLORCUBE_OFFSET + i,
                                                color, override);
        }
        for (i = 0; i < 24; ++i) {
                g_snprintf (name, sizeof (name), "shade-24-shades-%d-color", i + 1);
                color = _vte_style_context_get_color(context, name, &rgba);
                vte_view_set_color_internal(terminal, VTE_COLOR_SHADES_OFFSET + i,
                                                color, override);
        }

        /* Now the extra colours */

        color = _vte_style_context_get_color(context, "bold-foreground-color", &rgba);
        vte_view_set_mixed_color(terminal, VTE_BOLD_FG, color, 1.8, override);

        color = _vte_style_context_get_color(context, "dim-foreground-color", &rgba);
        vte_view_set_mixed_color(terminal, VTE_DIM_FG, color, 0.5, override);

        gtk_widget_style_get(&terminal->widget,
                             "cursor-effect", &cursor_effect,
                             "reverse-effect", &reverse_effect,
                             "selection-effect", &selection_effect,
                             NULL);

        color = _vte_style_context_get_color(context, "cursor-background-color", &rgba);
        _vte_view_set_effect_color(terminal, VTE_CUR_BG, color, cursor_effect, override);

          color = _vte_style_context_get_color(context, "reverse-background-color", &rgba);
        _vte_view_set_effect_color(terminal, VTE_REV_BG, color, reverse_effect, override);

        color = _vte_style_context_get_color(context, "selection-background-color", &rgba);
        _vte_view_set_effect_color(terminal, VTE_DEF_HL, color, selection_effect, override);
}

static void
vte_view_update_cursor_style(VteView *terminal)
{
        VteViewPrivate *pvt = terminal->pvt;
        GtkWidget *widget = &terminal->widget;
        float aspect;
        int cursor_shape, blink_mode;
        gboolean blinks;

        gtk_widget_style_get(widget,
                             "cursor-blink-mode", &blink_mode,
                             "cursor-shape", &cursor_shape,
                             "cursor-aspect-ratio", &aspect,
                             NULL);

        if ((VteCursorBlinkMode)blink_mode != pvt->cursor_blink_mode) {
                pvt->cursor_blink_mode = (VteCursorBlinkMode)blink_mode;

                switch ((VteCursorBlinkMode)blink_mode) {
                case VTE_CURSOR_BLINK_SYSTEM:
                        g_object_get(gtk_widget_get_settings(GTK_WIDGET(terminal)),
                                                             "gtk-cursor-blink", &blinks,
                                                             NULL);
                        break;
                case VTE_CURSOR_BLINK_ON:
                        blinks = TRUE;
                        break;
                case VTE_CURSOR_BLINK_OFF:
                        blinks = FALSE;
                        break;
                }

                vte_view_set_cursor_blinks_internal(terminal, blinks);
        }

        if ((VteCursorShape)cursor_shape != pvt->cursor_shape) {
                pvt->cursor_shape = (VteCursorShape)cursor_shape;
                _vte_invalidate_cursor_once(terminal, FALSE);
        }

        if (aspect != pvt->cursor_aspect_ratio) {
                pvt->cursor_aspect_ratio = aspect;
                _vte_invalidate_cursor_once(terminal, FALSE);
        }
}

static void
vte_view_update_style(VteView *terminal)
{
        VteViewPrivate *pvt = terminal->pvt;
        GtkWidget *widget = &terminal->widget;
        gboolean allow_bold, reverse;
        PangoFontDescription *font_desc;

        vte_view_set_padding(terminal);
        vte_view_update_style_colors(terminal, FALSE);
        vte_view_update_cursor_style(terminal);

        gtk_widget_style_get(widget,
                             "allow-bold", &allow_bold,
                             "font", &font_desc,
                             "reverse", &reverse,
                             NULL);

        vte_view_set_font(terminal, font_desc /* adopted */);

        if (allow_bold != pvt->allow_bold) {
                pvt->allow_bold = allow_bold;
                _vte_invalidate_all (terminal);
        }

        if (reverse != pvt->reverse) {
                pvt->reverse = reverse;

                _vte_invalidate_all(terminal);
        }
}

static void
vte_view_style_updated(GtkWidget *widget)
{
        GTK_WIDGET_CLASS (vte_view_parent_class)->style_updated (widget);

        vte_view_update_style(VTE_VIEW(widget));
}

static void
add_cursor_timeout (VteView *terminal)
{
	if (terminal->pvt->cursor_blink_tag)
		return; /* already added */

	terminal->pvt->cursor_blink_time = 0;
	terminal->pvt->cursor_blink_tag = g_timeout_add_full(G_PRIORITY_LOW,
							     terminal->pvt->cursor_blink_cycle,
							     (GSourceFunc)vte_invalidate_cursor_periodic,
							     terminal,
							     NULL);
}

static void
remove_cursor_timeout (VteView *terminal)
{
	if (terminal->pvt->cursor_blink_tag == 0)
		return; /* already removed */

	g_source_remove (terminal->pvt->cursor_blink_tag);
	terminal->pvt->cursor_blink_tag = 0;
}

/* Activates / disactivates the cursor blink timer to reduce wakeups */
static void
_vte_check_cursor_blink(VteView *terminal)
{
        VteBuffer *buffer = terminal->pvt->buffer;

	if (terminal->pvt->has_focus &&
	    terminal->pvt->cursor_blinks &&
	    buffer->pvt->cursor_visible)
		add_cursor_timeout(terminal);
	else
		remove_cursor_timeout(terminal);
}

static void
vte_view_audible_beep(VteView *terminal)
{
	GdkDisplay *display;

	g_assert(VTE_IS_VIEW(terminal));
	display = gtk_widget_get_display(&terminal->widget);
	gdk_display_beep(display);
}

static void
vte_view_visible_beep(VteView *terminal)
{
	GtkWidget *widget = &terminal->widget;
	GtkAllocation allocation;
	GdkRGBA color;
        cairo_t *cr;


	if (!gtk_widget_get_realized (widget))
                return;

        {
		gtk_widget_get_allocation (widget, &allocation);
                gtk_style_context_get_color(gtk_widget_get_style_context(widget),
                                            GTK_STATE_FLAG_NORMAL,
                                            &color);

                cr = gdk_cairo_create(gtk_widget_get_window(widget));
                _vte_draw_set_cairo(terminal->pvt->draw, cr);
		_vte_draw_fill_rectangle(terminal->pvt->draw,
					 0, 0,
					 allocation.width, allocation.height,
					 &color);
                _vte_draw_set_cairo(terminal->pvt->draw, NULL);
                cairo_destroy(cr);

		/* Force the repaint, max delay of UPDATE_REPEAT_TIMEOUT */
		_vte_invalidate_all (terminal);
	}
}

static void
vte_view_beep(VteView *terminal,
                   VteBellType bell_type)
{
	if (bell_type == VTE_BELL_AUDIBLE && terminal->pvt->audible_bell) {
		vte_view_audible_beep (terminal);
	}
	if (bell_type == VTE_BELL_VISUAL && terminal->pvt->visible_bell) {
		vte_view_visible_beep (terminal);
	}
}

static guint
vte_translate_ctrlkey (GdkEventKey *event)
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
				event->hardware_keycode, event->state,
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

static void
vte_view_read_modifiers (VteView *terminal,
			     GdkEvent *event)
{
        GdkKeymap *keymap;
	GdkModifierType modifiers;

	/* Read the modifiers. */
	if (!gdk_event_get_state((GdkEvent*)event, &modifiers))
                return;

        keymap = gdk_keymap_get_for_display(gdk_window_get_display(((GdkEventAny*)event)->window));

        gdk_keymap_add_virtual_modifiers (keymap, &modifiers);

#if 1
        /* HACK! Treat ALT as META; see bug #663779. */
        if (modifiers & GDK_MOD1_MASK)
                modifiers |= VTE_META_MASK;
#endif

        terminal->pvt->modifiers = modifiers;
}

/* Read and handle a keypress event. */
static gint
vte_view_key_press(GtkWidget *widget, GdkEventKey *event)
{
	VteView *terminal = VTE_VIEW(widget);
        VteBuffer *buffer;
	GdkModifierType modifiers;
	struct _vte_termcap *termcap;
	const char *tterm;
	char *normal = NULL, *output;
	gssize normal_length = 0;
	int i;
	const char *special = NULL;
	struct termios tio;
	gboolean scrolled = FALSE, steal = FALSE, modifier = FALSE, handled,
		 suppress_meta_esc = FALSE;
	guint keyval = 0;
	gunichar keychar = 0;
	char keybuf[VTE_UTF8_BPC];


	/* First, check if GtkWidget's behavior already does something with
	 * this key. */
	if (GTK_WIDGET_CLASS(vte_view_parent_class)->key_press_event) {
		if ((GTK_WIDGET_CLASS(vte_view_parent_class))->key_press_event(widget,
								      event)) {
			return TRUE;
		}
	}

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return FALSE;

	/* If it's a keypress, record that we got the event, in case the
	 * input method takes the event from us. */
	if (event->type == GDK_KEY_PRESS) {
		/* Store a copy of the key. */
		keyval = event->keyval;
		vte_view_read_modifiers (terminal, (GdkEvent*) event);

		/* If we're in margin bell mode and on the border of the
		 * margin, bell. */
		if (buffer->pvt->margin_bell) {
			if ((buffer->pvt->screen->cursor_current.col +
			     (glong) terminal->pvt->bell_margin) ==
			     buffer->pvt->column_count) {
				_vte_buffer_emit_bell(buffer, VTE_BELL_AUDIBLE);
			}
		}

		if (terminal->pvt->cursor_blink_tag != 0)
		{
			remove_cursor_timeout (terminal);
			terminal->pvt->cursor_blink_state = TRUE;
			add_cursor_timeout (terminal);
		}

		/* Determine if this is just a modifier key. */
		modifier = _vte_keymap_key_is_modifier(keyval);

		/* Unless it's a modifier key, hide the pointer. */
		if (!modifier) {
			_vte_view_set_pointer_visible(terminal, FALSE);
		}

		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Keypress, modifiers=0x%x, "
				"keyval=0x%x, raw string=`%s'.\n",
				terminal->pvt->modifiers,
				keyval, event->string);

		/* We steal many keypad keys here. */
		if (!terminal->pvt->im_preedit_active) {
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
			if (terminal->pvt->modifiers & VTE_META_MASK) {
				steal = TRUE;
			}
			switch (keyval) {
			case GDK_KEY_Multi_key:
			case GDK_KEY_Codeinput:
			case GDK_KEY_SingleCandidate:
			case GDK_KEY_MultipleCandidate:
			case GDK_KEY_PreviousCandidate:
			case GDK_KEY_Kanji:
			case GDK_KEY_Muhenkan:
			case GDK_KEY_Henkan:
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
				steal = FALSE;
				break;
			default:
				break;
			}
		}
	}

	modifiers = terminal->pvt->modifiers;

	/* Let the input method at this one first. */
	if (!steal) {
		if (gtk_widget_get_realized (&terminal->widget)
				&& gtk_im_context_filter_keypress (terminal->pvt->im_context, event)) {
			_vte_debug_print(VTE_DEBUG_EVENTS,
					"Keypress taken by IM.\n");
			return TRUE;
		}
	}

	/* Now figure out what to send to the child. */
	if ((event->type == GDK_KEY_PRESS) && !modifier) {
		handled = FALSE;
		/* Map the key to a sequence name if we can. */
		switch (keyval) {
		case GDK_KEY_BackSpace:
			switch (buffer->pvt->backspace_binding) {
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
				special = "kD";
				suppress_meta_esc = TRUE;
				break;
			case VTE_ERASE_TTY:
				if (buffer->pvt->pty != NULL &&
				    tcgetattr(vte_pty_get_fd(buffer->pvt->pty), &tio) != -1)
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
				if (buffer->pvt->pty != NULL &&
				    tcgetattr(vte_pty_get_fd(buffer->pvt->pty), &tio) != -1 &&
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
			handled = TRUE;
			break;
		case GDK_KEY_KP_Delete:
		case GDK_KEY_Delete:
			switch (buffer->pvt->delete_binding) {
			case VTE_ERASE_ASCII_BACKSPACE:
				normal = g_strdup("\010");
				normal_length = 1;
				break;
			case VTE_ERASE_ASCII_DELETE:
				normal = g_strdup("\177");
				normal_length = 1;
				break;
			case VTE_ERASE_TTY:
				if (buffer->pvt->pty != NULL &&
				    tcgetattr(vte_pty_get_fd(buffer->pvt->pty), &tio) != -1)
				{
					normal = g_strdup_printf("%c", tio.c_cc[VERASE]);
					normal_length = 1;
				}
				suppress_meta_esc = FALSE;
				break;
			case VTE_ERASE_DELETE_SEQUENCE:
			case VTE_ERASE_AUTO:
			default:
				special = "kD";
				break;
			}
			handled = TRUE;
			suppress_meta_esc = TRUE;
			break;
		case GDK_KEY_KP_Insert:
		case GDK_KEY_Insert:
			if (modifiers & GDK_SHIFT_MASK) {
				if (modifiers & GDK_CONTROL_MASK) {
					vte_view_emit_paste_clipboard(terminal);
					handled = TRUE;
					suppress_meta_esc = TRUE;
				} else {
					vte_view_emit_paste_primary(terminal);
					handled = TRUE;
					suppress_meta_esc = TRUE;
				}
			} else if (modifiers & GDK_CONTROL_MASK) {
				vte_view_emit_copy_clipboard(terminal);
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		/* Keypad/motion keys. */
		case GDK_KEY_KP_Up:
		case GDK_KEY_Up:
			if (modifiers & GDK_CONTROL_MASK 
                            && modifiers & GDK_SHIFT_MASK) {
				vte_view_scroll_lines(terminal, -1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Down:
		case GDK_KEY_Down:
			if (modifiers & GDK_CONTROL_MASK
                            && modifiers & GDK_SHIFT_MASK) {
				vte_view_scroll_lines(terminal, 1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Page_Up:
		case GDK_KEY_Page_Up:
			if (modifiers & GDK_SHIFT_MASK) {
				vte_view_scroll_pages(terminal, -1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Page_Down:
		case GDK_KEY_Page_Down:
			if (modifiers & GDK_SHIFT_MASK) {
				vte_view_scroll_pages(terminal, 1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KEY_KP_Home:
		case GDK_KEY_Home:
			if (modifiers & GDK_SHIFT_MASK) {
				vte_view_maybe_scroll_to_top(terminal);
				scrolled = TRUE;
				handled = TRUE;
			}
			break;
		case GDK_KEY_KP_End:
		case GDK_KEY_End:
			if (modifiers & GDK_SHIFT_MASK) {
				vte_view_maybe_scroll_to_bottom(terminal);
				scrolled = TRUE;
				handled = TRUE;
			}
			break;
		/* Let Shift +/- tweak the font, like XTerm does. */
		case GDK_KEY_KP_Add:
		case GDK_KEY_KP_Subtract:
			if (modifiers &
			    (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) {
				switch (keyval) {
				case GDK_KEY_KP_Add:
					vte_view_emit_increase_font_size(terminal);
					handled = TRUE;
					suppress_meta_esc = TRUE;
					break;
				case GDK_KEY_KP_Subtract:
					vte_view_emit_decrease_font_size(terminal);
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
		if (handled == FALSE && buffer->pvt->termcap != NULL) {
			_vte_keymap_map(keyval, modifiers,
					buffer->pvt->sun_fkey_mode,
                                        buffer->pvt->hp_fkey_mode,
                                        buffer->pvt->legacy_fkey_mode,
                                        buffer->pvt->vt220_fkey_mode,
                                        buffer->pvt->cursor_mode == VTE_KEYMODE_APPLICATION,
                                        buffer->pvt->keypad_mode == VTE_KEYMODE_APPLICATION,
                                        buffer->pvt->termcap,
                                        buffer->pvt->emulation ?
                                        buffer->pvt->emulation : vte_get_default_emulation(),
					&normal,
					&normal_length,
					&special);
			/* If we found something this way, suppress
			 * escape-on-meta. */
			if (((normal != NULL) && (normal_length > 0)) ||
			    (special != NULL)) {
				suppress_meta_esc = TRUE;
			}
		}

		/* Shall we do this here or earlier?  See bug 375112 and bug 589557 */
		if (modifiers & GDK_CONTROL_MASK)
			keyval = vte_translate_ctrlkey(event);

		/* If we didn't manage to do anything, try to salvage a
		 * printable string. */
		if (handled == FALSE && normal == NULL && special == NULL) {

			/* Convert the keyval to a gunichar. */
			keychar = gdk_keyval_to_unicode(keyval);
			normal_length = 0;
			if (keychar != 0) {
				/* Convert the gunichar to a string. */
				normal_length = g_unichar_to_utf8(keychar,
								  keybuf);
				if (normal_length != 0) {
					normal = g_malloc(normal_length + 1);
					memcpy(normal, keybuf, normal_length);
					normal[normal_length] = '\0';
				} else {
					normal = NULL;
				}
			}
			if ((normal != NULL) &&
			    (modifiers & GDK_CONTROL_MASK)) {
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
						modifiers,
						keyval, normal);
			}
		}
		/* If we got normal characters, send them to the child. */
		if (normal != NULL) {
			if (buffer->pvt->meta_sends_escape &&
			    !suppress_meta_esc &&
			    (normal_length > 0) &&
			    (modifiers & VTE_META_MASK)) {
				vte_buffer_feed_child(buffer,
							_VTE_CAP_ESC,
							1);
			}
			if (normal_length > 0) {
				vte_buffer_feed_child_using_modes(buffer,
								    normal,
								    normal_length);
			}
			g_free(normal);
		} else
		/* If the key maps to characters, send them to the child. */
		if (special != NULL && buffer->pvt->termcap != NULL) {
			termcap = buffer->pvt->termcap;
			tterm = buffer->pvt->emulation;
			normal = _vte_termcap_find_string_length(termcap,
								 tterm,
								 special,
								 &normal_length);
			_vte_keymap_key_add_key_modifiers(keyval,
							  modifiers,
                                                          buffer->pvt->sun_fkey_mode,
                                                          buffer->pvt->hp_fkey_mode,
                                                          buffer->pvt->legacy_fkey_mode,
                                                          buffer->pvt->vt220_fkey_mode,
                                                          buffer->pvt->cursor_mode == VTE_KEYMODE_APPLICATION,
							  &normal,
							  &normal_length);
			output = g_strdup_printf(normal, 1);
			vte_buffer_feed_child_using_modes(buffer,
							    output, -1);
			g_free(output);
			g_free(normal);
		}
		/* Keep the cursor on-screen. */
		if (!scrolled && !modifier &&
		    terminal->pvt->scroll_on_keystroke) {
			vte_view_maybe_scroll_to_bottom(terminal);
		}
		return TRUE;
	}
	return FALSE;
}

static gboolean
vte_view_key_release(GtkWidget *widget, GdkEventKey *event)
{
	VteView *terminal = VTE_VIEW(widget);
        VteBuffer *buffer;

	vte_view_read_modifiers (terminal, (GdkEvent*) event);

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return FALSE;

	return gtk_widget_get_realized (&terminal->widget)
			&& gtk_im_context_filter_keypress (terminal->pvt->im_context, event);
}

/*
 * __vte_view_is_word_char:
 * @terminal: a #VteView
 * @c: a candidate Unicode code point
 *
 * Checks if a particular character is considered to be part of a word or not,
 * based on the values last passed to vte_view_set_word_chars().
 *
 * Returns: %TRUE if the character is considered to be part of a word
 */
gboolean
_vte_view_is_word_char(VteView *terminal, gunichar c)
{
	guint i;
	VteWordCharRange *range;

	if (terminal->pvt->word_chars != NULL) {
		/* Go through each range and check if c is included. */
		for (i = 0; i < terminal->pvt->word_chars->len; i++) {
			range = &g_array_index(terminal->pvt->word_chars,
					       VteWordCharRange,
					       i);
			if ((c >= range->start) && (c <= range->end))
				return TRUE;
		}
	}

	/* If not ASCII, or ASCII and no array set (or empty array),
	 * fall back on Unicode properties. */
	return (c >= 0x80 ||
	        (terminal->pvt->word_chars == NULL) ||
	        (terminal->pvt->word_chars->len == 0)) &&
		g_unichar_isgraph(c) &&
	       !g_unichar_ispunct(c) &&
	       !g_unichar_isspace(c) &&
	       (c != '\0');
}

/* Check if the characters in the two given locations are in the same class
 * (word vs. non-word characters). */
static gboolean
vte_same_class(VteView *terminal, glong acol, glong arow,
	       glong bcol, glong brow)
{
        VteBuffer *buffer = terminal->pvt->buffer;
	const VteCell *pcell = NULL;
	gboolean word_char;
	if ((pcell = vte_screen_find_charcell(buffer->pvt->screen, acol, arow)) != NULL && pcell->c != 0) {
		word_char = _vte_view_is_word_char(terminal, _vte_unistr_get_base (pcell->c));

		/* Lets not group non-wordchars together (bug #25290) */
		if (!word_char)
			return FALSE;

		pcell = vte_screen_find_charcell(buffer->pvt->screen, bcol, brow);
		if (pcell == NULL || pcell->c == 0) {
			return FALSE;
		}
		if (word_char != _vte_view_is_word_char(terminal, _vte_unistr_get_base (pcell->c))) {
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

/* Check if we soft-wrapped on the given line. */
static gboolean
vte_buffer_line_is_wrappable(VteBuffer *buffer, glong row)
{
	const VteRowData *rowdata;
	rowdata = _vte_screen_find_row_data(buffer->pvt->screen, row);
	return rowdata && rowdata->attr.soft_wrapped;
}

/* Check if the given point is in the region between the two points,
 * optionally treating the second point as included in the region or not. */
static gboolean
vte_cell_is_between(glong col, glong row,
		    glong acol, glong arow, glong bcol, glong brow,
		    gboolean inclusive)
{
	/* Negative between never allowed. */
	if ((arow > brow) || ((arow == brow) && (acol > bcol))) {
		return FALSE;
	}
	/* Zero-length between only allowed if we're being inclusive. */
	if ((row == arow) && (row == brow) && (col == acol) && (col == bcol)) {
		return inclusive;
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
				if ((col == bcol) && inclusive) {
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
				if ((col == bcol) && inclusive) {
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
vte_view_cell_is_selected(VteBuffer *buffer, glong col, glong row, VteView *terminal)
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
	return vte_cell_is_between(col, row, ss.col, ss.row, se.col, se.row, TRUE);
}

/* Once we get text data, actually paste it in. */
static void
vte_view_paste_cb(GtkClipboard *clipboard, const gchar *text, gpointer data)
{
	VteView *terminal = data;
        VteBuffer *buffer;
	gchar *paste, *p;
	long length;

        buffer = terminal->pvt->buffer;

	if (text != NULL) {
		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Pasting %"G_GSIZE_FORMAT" UTF-8 bytes.\n",
				strlen(text));
		if (!g_utf8_validate(text, -1, NULL)) {
			g_warning(_("Error (%s) converting data for child, dropping."), g_strerror(EINVAL));
			return;
		}

		/* Convert newlines to carriage returns, which more software
		 * is able to cope with (cough, pico, cough). */
		paste = g_strdup(text);
		length = strlen(paste);
		p = paste;
		while ((p != NULL) && (p - paste < length)) {
			p = memchr(p, '\n', length - (p - paste));
			if (p != NULL) {
				*p = '\r';
				p++;
			}
		}
		if (buffer->pvt->screen->bracketed_paste_mode)
			vte_buffer_feed_child(buffer, "\e[200~", -1);
		vte_buffer_feed_child(buffer, paste, length);
		if (buffer->pvt->screen->bracketed_paste_mode)
			vte_buffer_feed_child(buffer, "\e[201~", -1);
		g_free(paste);
	}
}

/**
 * _vte_view_xy_to_grid:
 * @x: the X coordinate
 * @y: the Y coordinate
 * @col: return location to store the column
 * @row: return location to store the row
 *
 * Translates from widget coordinates to grid coordinates.
 *
 * If the coordinates are outside the grid, returns %FALSE.
 */
gboolean
_vte_view_xy_to_grid(VteView *terminal,
                         long x,
                         long y,
                         long *col,
                         long *row)
{
        VteBuffer *buffer;
        VteViewPrivate *pvt = terminal->pvt;
        long c, r;

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return FALSE;

        /* FIXMEchpe: is this correct for RTL? */
        c = (x - pvt->padding.left) / pvt->char_width;
        r = (y - pvt->padding.top) / pvt->char_height;

        if ((c < 0 || c >= buffer->pvt->column_count) ||
            (r < 0 || r >= buffer->pvt->row_count))
          return FALSE;

        *col = c;
        *row = r;
        return TRUE;
}

/*
 * _vte_view_size_to_grid_size:
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
_vte_view_size_to_grid_size(VteView *terminal,
                                long w,
                                long h,
                                long *cols,
                                long *rows)
{
        VteViewPrivate *pvt = terminal->pvt;
        long n_cols, n_rows;

        n_cols = (w - pvt->padding.left - pvt->padding.right) / pvt->char_width;
        n_rows = (h - pvt->padding.top -pvt->padding.bottom) / pvt->char_height;

        if (n_cols <= 0 || n_rows <= 0)
                return FALSE;

        *cols = n_cols;
        *rows = n_rows;
        return TRUE;
}

static void
vte_view_get_mouse_tracking_info (VteView   *terminal,
				      int            button,
				      long           col,
				      long           row,
				      unsigned char *pb,
                                       long          *px,
				      long          *py)
{
        VteBuffer *buffer;
	unsigned char cb = 0;
        long cx, cy;

        buffer = terminal->pvt->buffer;

	/* Encode the button information in cb. */
	switch (button) {
	case 0:			/* Release/no buttons. */
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
	cb += 32; /* 32 for normal */

	/* Encode the modifiers. */
	if (terminal->pvt->modifiers & GDK_SHIFT_MASK) {
		cb |= 4;
	}
	if (terminal->pvt->modifiers & VTE_META_MASK) {
		cb |= 8;
	}
	if (terminal->pvt->modifiers & GDK_CONTROL_MASK) {
		cb |= 16;
	}

       /* Cursor coordinates */
       cx = CLAMP(1 + col, 1, buffer->pvt->column_count);
       cy = CLAMP(1 + row, 1, buffer->pvt->row_count);

	*pb = cb;
	*px = cx;
	*py = cy;
}

static void
vte_buffer_feed_mouse_event(VteBuffer *buffer,
                            int        cb,
                            long       cx,
                            long       cy)
{
        char buf[LINE_MAX];
        gint len = 0;

        if (buffer->pvt->mouse_urxvt_extension) {
                /* urxvt's extended mode (1015) */
                len = g_snprintf(buf, sizeof(buf), _VTE_CAP_CSI "%d;%ld;%ldM", cb, cx, cy);
        } else if (cx <= 231 && cy <= 231) {
                /* legacy mode */
                len = g_snprintf(buf, sizeof(buf), _VTE_CAP_CSI "M%c%c%c", cb, 32 + (guchar)cx, 32 + (guchar)cy);
        }

        /* Send event direct to the child, this is binary not text data */
        vte_buffer_feed_child_binary(buffer, buf, len);
}

/*
 * vte_view_send_mouse_button_internal:
 * @terminal:
 * @button: the mouse button, or 0 for keyboard
 * @x: the event X coordinate
 * @y: the event Y coordinate
 */
static void
vte_view_send_mouse_button_internal(VteView *terminal,
					int          button,
					long         x,
					long         y)
{
        VteBuffer *buffer;
	unsigned char cb;
        long cx, cy;
        long col, row;

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return;

        if (!_vte_view_xy_to_grid(terminal, x, y, &col, &row))
                return;

	vte_view_get_mouse_tracking_info (terminal,
					      button, col, row,
					      &cb, &cx, &cy);

        vte_buffer_feed_mouse_event(buffer, cb, cx, cy);
}

/* Send a mouse button click/release notification. */
static void
vte_view_maybe_send_mouse_button(VteView *terminal,
				     GdkEventButton *event)
{
	vte_view_read_modifiers (terminal, (GdkEvent*) event);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (terminal->pvt->mouse_tracking_mode < MOUSE_TRACKING_SEND_XY_ON_CLICK) {
			return;
		}
		break;
	case GDK_BUTTON_RELEASE: {
		if (terminal->pvt->mouse_tracking_mode < MOUSE_TRACKING_SEND_XY_ON_BUTTON) {
			return;
		}
		break;
	}
	default:
		return;
		break;
	}

	vte_view_send_mouse_button_internal(terminal,
					        (event->type == GDK_BUTTON_PRESS) ? event->button : 0,
						event->x, event->y);
}

/* Send a mouse motion notification. */
static void
vte_view_maybe_send_mouse_drag(VteView *terminal, GdkEventMotion *event)
{
        VteBuffer *buffer;
	unsigned char cb;
        long cx, cy;
        long col, row;

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return;

        (void) _vte_view_xy_to_grid(terminal, event->x, event->y, &col, &row);

	/* First determine if we even want to send notification. */
	switch (event->type) {
	case GDK_MOTION_NOTIFY:
		if (terminal->pvt->mouse_tracking_mode < MOUSE_TRACKING_CELL_MOTION_TRACKING)
			return;

		if (terminal->pvt->mouse_tracking_mode < MOUSE_TRACKING_ALL_MOTION_TRACKING) {

			if (terminal->pvt->mouse_last_button == 0) {
				return;
			}
			/* the xterm doc is not clear as to whether
			 * all-tracking also sends degenerate same-cell events */
			if (col == terminal->pvt->mouse_last_cell_x &&
			    row == terminal->pvt->mouse_last_cell_y)
				return;
		}
		break;
	default:
		return;
		break;
	}

	vte_view_get_mouse_tracking_info (terminal,
					      terminal->pvt->mouse_last_button, col, row,
					      &cb, &cx, &cy);
	cb += 32; /* for movement */

        vte_buffer_feed_mouse_event(buffer, cb, cx, cy);
}

/* Clear all match hilites. */
static void
vte_view_match_hilite_clear(VteView *terminal)
{
	long srow, scolumn, erow, ecolumn;
	srow = terminal->pvt->match_start.row;
	scolumn = terminal->pvt->match_start.col;
	erow = terminal->pvt->match_end.row;
	ecolumn = terminal->pvt->match_end.col;
	terminal->pvt->match_start.row = -1;
	terminal->pvt->match_start.col = -1;
	terminal->pvt->match_end.row = -2;
	terminal->pvt->match_end.col = -2;
	if (terminal->pvt->match_tag != -1) {
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Clearing hilite (%ld,%ld) to (%ld,%ld).\n",
				srow, scolumn, erow, ecolumn);
		_vte_invalidate_region (terminal,
				scolumn, ecolumn, srow, erow, FALSE);
		terminal->pvt->match_tag = -1;
	}
	terminal->pvt->show_match = FALSE;
	if (terminal->pvt->match) {
		g_free (terminal->pvt->match);
		terminal->pvt->match = NULL;
	}
}

static gboolean
cursor_inside_match (VteView *terminal, long x, long y)
{
        VteBuffer *buffer = terminal->pvt->buffer;
	gint width = terminal->pvt->char_width;
	gint height = terminal->pvt->char_height;
	glong col = x / width;
	glong row = y / height + buffer->pvt->screen->scroll_delta;
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

static void
vte_view_match_hilite_show(VteView *terminal, long x, long y)
{
	if(terminal->pvt->match != NULL && !terminal->pvt->show_match){
		if (cursor_inside_match (terminal, x, y)) {
			_vte_invalidate_region (terminal,
					terminal->pvt->match_start.col,
					terminal->pvt->match_end.col,
					terminal->pvt->match_start.row,
					terminal->pvt->match_end.row,
					FALSE);
			terminal->pvt->show_match = TRUE;
		}
	}
}
static void
vte_view_match_hilite_hide(VteView *terminal)
{
	if(terminal->pvt->match != NULL && terminal->pvt->show_match){
		_vte_invalidate_region (terminal,
				terminal->pvt->match_start.col,
				terminal->pvt->match_end.col,
				terminal->pvt->match_start.row,
				terminal->pvt->match_end.row,
				FALSE);
		terminal->pvt->show_match = FALSE;
	}
}


static void
vte_view_match_hilite_update(VteView *terminal, long x, long y)
{
        VteBuffer *buffer;
	int start, end, width, height;
	char *match;
	struct _VteCharAttributes *attr;
	VteScreen *screen;
	long delta;

	width = terminal->pvt->char_width;
	height = terminal->pvt->char_height;

        buffer = terminal->pvt->buffer;

	/* Check for matches. */
	screen = buffer->pvt->screen;
	delta = screen->scroll_delta;

	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Match hilite update (%ld, %ld) -> %ld, %ld\n",
			x, y,
			x / width,
			y / height + delta);

	match = vte_view_match_check_internal(terminal,
						  x / width,
						  y / height + delta,
						  &terminal->pvt->match_tag,
						  &start,
						  &end);
	if (terminal->pvt->show_match) {
		/* Repaint what used to be hilited, if anything. */
		_vte_invalidate_region(terminal,
				terminal->pvt->match_start.col,
				terminal->pvt->match_end.col,
				terminal->pvt->match_start.row,
				terminal->pvt->match_end.row,
				FALSE);
	}

	/* Read the new locations. */
	attr = NULL;
	if ((guint) start < terminal->pvt->match_attributes->len) {
		attr = &g_array_index(terminal->pvt->match_attributes,
				struct _VteCharAttributes,
				start);
		terminal->pvt->match_start.row = attr->row;
		terminal->pvt->match_start.col = attr->column;

		attr = NULL;
		if ((guint) end < terminal->pvt->match_attributes->len) {
			attr = &g_array_index(terminal->pvt->match_attributes,
					struct _VteCharAttributes,
					end);
			terminal->pvt->match_end.row = attr->row;
			terminal->pvt->match_end.col = attr->column;
		}
	}
	if (attr == NULL) { /* i.e. if either endpoint is not found */
		terminal->pvt->match_start.row = -1;
		terminal->pvt->match_start.col = -1;
		terminal->pvt->match_end.row = -2;
		terminal->pvt->match_end.col = -2;
		g_assert (match == NULL);
	}

	g_free (terminal->pvt->match);
	terminal->pvt->match = match;

	/* If there are no matches, repaint what we had matched before. */
	if (match == NULL) {
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"No matches. [(%ld,%ld) to (%ld,%ld)]\n",
				terminal->pvt->match_start.col,
				terminal->pvt->match_start.row,
				terminal->pvt->match_end.col,
				terminal->pvt->match_end.row);
		terminal->pvt->show_match = FALSE;
	} else {
		terminal->pvt->show_match = TRUE;
		/* Repaint the newly-hilited area. */
		_vte_invalidate_region(terminal,
				terminal->pvt->match_start.col,
				terminal->pvt->match_end.col,
				terminal->pvt->match_start.row,
				terminal->pvt->match_end.row,
				FALSE);
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Matched (%ld,%ld) to (%ld,%ld).\n",
				terminal->pvt->match_start.col,
				terminal->pvt->match_start.row,
				terminal->pvt->match_end.col,
				terminal->pvt->match_end.row);
	}
}
/* Update the hilited text if the pointer has moved to a new character cell. */
static void
vte_view_match_hilite(VteView *terminal, long x, long y)
{
	int width, height;
	GtkAllocation allocation;

	width = terminal->pvt->char_width;
	height = terminal->pvt->char_height;

	gtk_widget_get_allocation (&terminal->widget, &allocation);

	/* if the cursor is not above a cell, skip */
	if (x < 0 || x > allocation.width
			|| y < 0 || y > allocation.height) {
		return;
	}

	/* If the pointer hasn't moved to another character cell, then we
	 * need do nothing. */
	if (x / width  == terminal->pvt->mouse_last_x / width &&
	    y / height == terminal->pvt->mouse_last_y / height) {
		terminal->pvt->show_match = terminal->pvt->match != NULL;
		return;
	}

	if (cursor_inside_match (terminal, x, y)) {
		terminal->pvt->show_match = terminal->pvt->match != NULL;
		return;
	}

	vte_view_match_hilite_update(terminal, x, y);
}


/* Note that the clipboard has cleared. */
static void
vte_view_clear_cb(GtkClipboard *clipboard, gpointer owner)
{
	VteView *terminal;
	terminal = owner;
	if (terminal->pvt->has_selection) {
		_vte_debug_print(VTE_DEBUG_SELECTION, "Lost selection.\n");
		vte_view_deselect_all(terminal);
	}
}

/* Supply the selected text to the clipboard. */
static void
vte_view_copy_cb(GtkClipboard *clipboard, GtkSelectionData *data,
		     guint info, gpointer owner)
{
	VteView *terminal;
	terminal = owner;
	if (terminal->pvt->selection != NULL) {
		_VTE_DEBUG_IF(VTE_DEBUG_SELECTION) {
			int i;
			g_printerr("Setting selection (%"G_GSIZE_FORMAT" UTF-8 bytes.)\n",
				strlen(terminal->pvt->selection));
			for (i = 0; terminal->pvt->selection[i] != '\0'; i++) {
				g_printerr("0x%04x\n",
					terminal->pvt->selection[i]);
			}
		}
		gtk_selection_data_set_text(data, terminal->pvt->selection, -1);
	}
}

/**
 * VteSelectionFunc:
 * @buffer: buffer in which the cell is.
 * @column: column in which the cell is.
 * @row: row in which the cell is.
 * @data: (closure): user data.
 *
 * Specifies the type of a selection function used to check whether
 * a cell has to be selected or not.
 *
 * Returns: %TRUE if cell has to be selected; %FALSE if otherwise.
 */

/**
 * vte_buffer_get_text_range:
 * @buffer: a #VteBuffer
 * @start_row: first row to search for data
 * @start_col: first column to search for data
 * @end_row: last row to search for data
 * @end_col: last column to search for data
 * @is_selected: (scope call): a #VteSelectionFunc callback
 * @user_data: (closure): user data to be passed to the callback
 * @attributes: (out) (transfer full) (array) (element-type Vte.CharAttributes): location for storing text attributes
 *
 * Extracts a view of the visible part of the buffer.  If @is_selected is not
 * %NULL, characters will only be read if @is_selected returns %TRUE after being
 * passed the column and row, respectively.  A #VteCharAttributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics.  The
 * entire scrollback buffer is scanned, so it is possible to read the entire
 * contents of the buffer using this function.
 *
 * Returns: (transfer full): a newly allocated text string, or %NULL.
 */
char *
vte_buffer_get_text_range(VteBuffer *buffer,
			    glong start_row, glong start_col,
			    glong end_row, glong end_col,
			    VteSelectionFunc is_selected,
			    gpointer user_data,
			    GArray *attributes)
{
	g_return_val_if_fail(VTE_IS_BUFFER(buffer), NULL);
	return vte_buffer_get_text_range_maybe_wrapped(buffer,
							 start_row, start_col,
							 end_row, end_col,
							 TRUE,
							 is_selected,
							 user_data,
							 attributes,
							 FALSE);
}

static char *
vte_buffer_get_text_range_maybe_wrapped(VteBuffer *buffer,
					  glong start_row, glong start_col,
					  glong end_row, glong end_col,
					  gboolean wrap,
					  VteSelectionFunc is_selected,
					  gpointer data,
					  GArray *attributes,
					  gboolean include_trailing_spaces)
{
        VteView *terminal;
	glong col, row, last_empty, last_emptycol, last_nonempty, last_nonemptycol;
	const VteCell *pcell = NULL;
	GString *string;
	struct _VteCharAttributes attr;
	GdkRGBA fore, back, *palette;

	if (!is_selected)
		is_selected = always_selected;

        terminal = buffer->pvt->terminal;

	if (attributes)
		g_array_set_size (attributes, 0);

	string = g_string_new(NULL);
	memset(&attr, 0, sizeof(attr));

	palette = buffer->pvt->palette;
	col = start_col;
	for (row = start_row; row < end_row + 1; row++, col = 0) {
		const VteRowData *row_data = _vte_screen_find_row_data(buffer->pvt->screen, row);
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
				if (!pcell->attr.fragment && is_selected(buffer, col, row, data)) {
					/* Store the attributes of this character. */
					fore = palette[pcell->attr.fore];
					back = palette[pcell->attr.back];
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
		attr.column = MAX(buffer->pvt->column_count, attr.column + 1);

		/* Add a newline in block mode. */
		if (terminal->pvt->selection_block_mode) {
			string = g_string_append_c(string, '\n');
		}
		/* Else, if the last visible column on this line was selected and
		 * not soft-wrapped, append a newline. */
		else if (is_selected(buffer, buffer->pvt->column_count, row, data)) {
			/* If we didn't softwrap, add a newline. */
			/* XXX need to clear row->soft_wrap on deletion! */
			if (!vte_buffer_line_is_wrappable(buffer, row)) {
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
	return g_string_free(string, FALSE);
}

static char *
vte_buffer_get_text_maybe_wrapped(VteBuffer *buffer,
				    gboolean wrap,
				    VteSelectionFunc is_selected,
				    gpointer data,
				    GArray *attributes,
				    gboolean include_trailing_spaces)
{
	long start_row, start_col, end_row, end_col;
	start_row = buffer->pvt->screen->scroll_delta;
	start_col = 0;
	end_row = start_row + buffer->pvt->row_count - 1;
	end_col = buffer->pvt->column_count - 1;
	return vte_buffer_get_text_range_maybe_wrapped(buffer,
							 start_row, start_col,
							 end_row, end_col,
							 wrap,
							 is_selected,
							 data,
							 attributes,
							 include_trailing_spaces);
}

/**
 * vte_buffer_get_text:
 * @buffer: a #VteBuffer
 * @is_selected: (scope call): a #VteSelectionFunc callback
 * @user_data: (closure): user data to be passed to the callback
 * @attributes: (out) (transfer full) (array) (element-type Vte.CharAttributes): location for storing text attributes
 *
 * Extracts a view of the visible part of the buffer.  If @is_selected is not
 * %NULL, characters will only be read if @is_selected returns %TRUE after being
 * passed the column and row, respectively.  A #VteCharAttributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics.
 *
 * Returns: (transfer full): a newly allocated text string, or %NULL.
 */
char *
vte_buffer_get_text(VteBuffer *buffer,
		      VteSelectionFunc is_selected,
		      gpointer user_data,
		      GArray *attributes)
{
	g_return_val_if_fail(VTE_IS_BUFFER(buffer), NULL);
	return vte_buffer_get_text_maybe_wrapped(buffer,
						   TRUE,
						   is_selected,
						   user_data,
						   attributes,
						   FALSE);
}

/**
 * vte_buffer_get_text_include_trailing_spaces:
 * @buffer: a #VteBuffer
 * @is_selected: (scope call): a #VteSelectionFunc callback
 * @user_data: (closure): user data to be passed to the callback
 * @attributes: (out) (transfer full) (array) (element-type Vte.CharAttributes): location for storing text attributes
 *
 * Extracts a view of the visible part of the buffer.  If @is_selected is not
 * %NULL, characters will only be read if @is_selected returns %TRUE after being
 * passed the column and row, respectively.  A #VteCharAttributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics. This function
 * differs from vte_buffer_get_text() in that trailing spaces at the end of
 * lines are included.
 *
 * Returns: (transfer full): a newly allocated text string, or %NULL.
 */
char *
vte_buffer_get_text_include_trailing_spaces(VteBuffer *buffer,
					      VteSelectionFunc is_selected,
					      gpointer user_data,
					      GArray *attributes)
{
	g_return_val_if_fail(VTE_IS_BUFFER(buffer), NULL);
	return vte_buffer_get_text_maybe_wrapped(buffer,
						   TRUE,
						   is_selected,
						   user_data,
						   attributes,
						   TRUE);
}

/**
 * vte_buffer_get_cursor_position:
 * @buffer: a #VteBuffer
 * @column: (out) (allow-none): a location to store the column, or %NULL
 * @row: (out) (allow-none): a location to store the row, or %NULL
 *
 * Reads the location of the insertion cursor and returns it.  The row
 * coordinate is absolute.
 */
void
vte_buffer_get_cursor_position(VteBuffer *buffer,
				 glong *column, glong *row)
{
	g_return_if_fail(VTE_IS_BUFFER(buffer));
	if (column) {
		*column = buffer->pvt->screen->cursor_current.col;
	}
	if (row) {
		*row = buffer->pvt->screen->cursor_current.row;
	}
}

/**
 * vte_view_copy_clipboard:
 * @terminal: a #VteView
 * @clipboard: a #GtkClipboard
 *
 * Copies the selected text in @terminal to @clipboard.
 */
void
vte_view_copy_clipboard(VteView *terminal,
                            GtkClipboard *clipboard)
{
        static GtkTargetEntry *targets = NULL;
        static gint n_targets = 0;
        VteBuffer *buffer;

        g_return_if_fail(VTE_IS_VIEW(terminal));
        g_return_if_fail(GTK_IS_CLIPBOARD(clipboard));

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return;

	/* Chuck old selected text and retrieve the newly-selected text. */
	g_free(terminal->pvt->selection);
	terminal->pvt->selection =
		vte_buffer_get_text_range(terminal->pvt->buffer,
					    terminal->pvt->selection_start.row,
					    0,
					    terminal->pvt->selection_end.row,
					    buffer->pvt->column_count,
					    (VteSelectionFunc)vte_view_cell_is_selected,
					    terminal /* user data */,
					    NULL);
	terminal->pvt->has_selection = TRUE;

	/* Place the text on the clipboard. */
	if (terminal->pvt->selection != NULL) {
		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Assuming ownership of selection.\n");
		if (!targets) {
			GtkTargetList *list;

			list = gtk_target_list_new (NULL, 0);
			gtk_target_list_add_text_targets (list, 0);
                        targets = gtk_target_table_new_from_list (list, &n_targets);
			gtk_target_list_unref (list);
		}

		gtk_clipboard_set_with_owner(clipboard,
					     targets,
					     n_targets,
					     vte_view_copy_cb,
					     vte_view_clear_cb,
					     G_OBJECT(terminal));
		gtk_clipboard_set_can_store(clipboard, NULL, 0);
	}
}

static void
vte_view_invalidate_selection (VteView *terminal)
{
	_vte_invalidate_region (terminal,
				terminal->pvt->selection_start.col,
				terminal->pvt->selection_end.col,
				terminal->pvt->selection_start.row,
				terminal->pvt->selection_end.row,
				terminal->pvt->selection_block_mode);
}


/* Start selection at the location of the event. */
static void
vte_view_start_selection(VteView *terminal, 
                         gdouble x,
                         gdouble y,
                         enum vte_selection_type selection_type)
{
        VteBuffer *buffer;
	long delta;

        buffer = terminal->pvt->buffer;

	terminal->pvt->selection_block_mode = !!(terminal->pvt->modifiers & GDK_CONTROL_MASK);

	if (terminal->pvt->selection_block_mode)
		selection_type = selection_type_char;

	/* Record that we have the selection, and where it started. */
	delta = buffer->pvt->screen->scroll_delta;
	terminal->pvt->has_selection = TRUE;
	terminal->pvt->selection_last.x = x - terminal->pvt->padding.left;
	terminal->pvt->selection_last.y = y - terminal->pvt->padding.top +
					  (terminal->pvt->char_height * delta);

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

	/* Temporarily stop caring about input from the child. */
	_vte_buffer_disconnect_pty_read(terminal->pvt->buffer);
}

static gboolean
_vte_view_maybe_end_selection (VteView *terminal)
{
	if (terminal->pvt->selecting) {
		/* Copy only if something was selected. */
		if (terminal->pvt->has_selection &&
		    !terminal->pvt->selecting_restart &&
		    terminal->pvt->selecting_had_delta) {
			vte_view_emit_copy_primary(terminal);
			vte_view_emit_selection_changed(terminal);
		}
		terminal->pvt->selecting = FALSE;

		/* Reconnect to input from the child if we paused it. */
		_vte_buffer_connect_pty_read(terminal->pvt->buffer);

		return TRUE;
	}
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
vte_view_extend_selection_expand (VteView *terminal)
{
        VteBuffer *buffer;
	long i, j;
	VteScreen *screen;
	const VteRowData *rowdata;
	const VteCell *cell;
	VteVisualPosition *sc, *ec;

	if (terminal->pvt->selection_block_mode)
		return;

        buffer = terminal->pvt->buffer;
	screen = buffer->pvt->screen;
	sc = &terminal->pvt->selection_start;
	ec = &terminal->pvt->selection_end;

	/* Extend the selection to handle end-of-line cases, word, and line
	 * selection.  We do this here because calculating it once is cheaper
	 * than recalculating for each cell as we render it. */

	/* Handle end-of-line at the start-cell. */
	rowdata = _vte_screen_find_row_data(screen, sc->row);
	if (rowdata != NULL) {
		/* Find the last non-empty character on the first line. */
		for (i = _vte_row_data_length (rowdata); i > 0; i--) {
			cell = _vte_row_data_get (rowdata, i - 1);
			if (cell->attr.fragment || cell->c != 0)
				break;
		}
		/* If the start point is to its right, then move the
		 * startpoint up to the beginning of the next line
		 * unless that would move the startpoint after the end
		 * point, or we're in select-by-line mode. */
		if ((sc->col >= i) &&
				(terminal->pvt->selection_type != selection_type_line)) {
			if (sc->row < ec->row) {
				sc->col = 0;
				sc->row++;
			} else {
				sc->col = i;
			}
		}
	} else {
		/* Snap to the leftmost column. */
		sc->col = 0;
	}
	sc->col = vte_buffer_find_start_column(buffer, sc->col, sc->row);

	/* Handle end-of-line at the end-cell. */
	rowdata = _vte_screen_find_row_data(screen, ec->row);
	if (rowdata != NULL) {
		/* Find the last non-empty character on the last line. */
		for (i = _vte_row_data_length (rowdata); i > 0; i--) {
			cell = _vte_row_data_get (rowdata, i - 1);
			if (cell->attr.fragment || cell->c != 0)
				break;
		}
		/* If the end point is to its right, then extend the
		 * endpoint as far right as we can expect. */
		if (ec->col >= i) {
			ec->col = MAX(ec->col,
				    MAX(buffer->pvt->column_count,
					(long) _vte_row_data_length (rowdata)));
		}
	} else {
		/* Snap to the rightmost column, only if selecting anything of
		 * this row. */
		if (ec->col >= 0)
			ec->col = MAX(ec->col, buffer->pvt->column_count);
	}
	ec->col = vte_buffer_find_end_column(buffer, ec->col, ec->row);


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
				 buffer->pvt->column_count;
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
				if (vte_buffer_line_is_wrappable(buffer, j - 1) &&
				    vte_same_class(terminal,
						   buffer->pvt->column_count - 1,
						   j - 1,
						   0,
						   j)) {
					/* Move on to the previous line. */
					j--;
					sc->col = buffer->pvt->column_count - 1;
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
			     i < buffer->pvt->column_count - 1;
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
			if (i < buffer->pvt->column_count - 1) {
				/* We hit a stopping point, so stop. */
				break;
			} else {
				if (vte_buffer_line_is_wrappable(buffer, j) &&
				    vte_same_class(terminal,
						   buffer->pvt->column_count - 1,
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
		       vte_buffer_line_is_wrappable(buffer, j - 1)) {
			j--;
			sc->row = j;
		}
		/* And move forward as far as we can go. */
		j = ec->row;
		while (_vte_ring_contains (screen->row_data, j) &&
		       vte_buffer_line_is_wrappable(buffer, j)) {
			j++;
			ec->row = j;
		}
		/* Make sure we include all of the last line. */
		ec->col = buffer->pvt->column_count;
		if (_vte_ring_contains (screen->row_data, ec->row)) {
			rowdata = _vte_ring_index(screen->row_data, ec->row);
			if (rowdata != NULL) {
				ec->col = MAX(ec->col, (long) _vte_row_data_length (rowdata));
			}
		}
		break;
	}
}

/* Extend selection to include the given event coordinates. */
static void
vte_view_extend_selection(VteView *terminal, long x, long y,
			      gboolean always_grow, gboolean force)
{
        VteBuffer *buffer;
	VteScreen *screen;
	int width, height;
	long delta, residual;
	struct selection_event_coords *origin, *last, *start, *end;
	VteVisualPosition old_start, old_end, *sc, *ec, *so, *eo;
	gboolean invalidate_selected = FALSE;
	gboolean had_selection;

        buffer = terminal->pvt->buffer;

	height = terminal->pvt->char_height;
	width = terminal->pvt->char_width;

	/* Confine y into the visible area. (#563024) */
	if (y < 0) {
		y = 0;
		if (!terminal->pvt->selection_block_mode)
			x = 0;
	} else if (y >= buffer->pvt->row_count * height) {
		if (!terminal->pvt->selection_block_mode) {
			y = buffer->pvt->row_count * height;
			x = -1;
		} else {
			y = buffer->pvt->row_count * height - 1;
		}
	}

	screen = buffer->pvt->screen;
	old_start = terminal->pvt->selection_start;
	old_end = terminal->pvt->selection_end;
	so = &old_start;
	eo = &old_end;

	/* Convert the event coordinates to cell coordinates. */
	delta = screen->scroll_delta;

	/* If we're restarting on a drag, then mark this as the start of
	 * the selected block. */
	if (terminal->pvt->selecting_restart) {
		vte_view_deselect_all(terminal);
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
		last->y = y + height * delta;

		/* We don't support always_grow in block mode */
		if (always_grow)
			vte_view_invalidate_selection (terminal);

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
			last->y = y + height * delta;
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
			if ((y / height < ((start->y / height) - delta)) ||
			    ((y / height == ((start->y / height) - delta)) &&
			     (x / width < start->x / width))) {
				start->x = x;
				start->y = y + height * delta;
			} else {
				/* New endpoint is after existing selection. */
				end->x = x;
				end->y = y + height * delta;
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


	vte_view_extend_selection_expand (terminal);

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
						MAX(sc->col, so->col) - 1,
						MIN(sc->row, so->row),
						MAX(ec->row, eo->row),
						TRUE);
			/* The right band */
			_vte_invalidate_region (terminal,
						MIN(ec->col, eo->col) + 1,
						MAX(ec->col, eo->col),
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
		vte_view_invalidate_selection (terminal);
	}

	_vte_debug_print(VTE_DEBUG_SELECTION,
			"Selection changed to "
			"(%ld,%ld) to (%ld,%ld).\n",
			sc->col, sc->row, ec->col, ec->row);
}

/**
 * vte_view_select_all:
 * @terminal: a #VteView
 *
 * Selects all text within the terminal (including the scrollback buffer).
 */
void
vte_view_select_all (VteView *terminal)
{
        VteBuffer *buffer;
	g_return_if_fail (VTE_IS_VIEW (terminal));

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return;

	vte_view_deselect_all (terminal);

	terminal->pvt->has_selection = TRUE;
	terminal->pvt->selecting_had_delta = TRUE;
	terminal->pvt->selecting_restart = FALSE;

	terminal->pvt->selection_start.row = _vte_ring_delta (buffer->pvt->screen->row_data);
	terminal->pvt->selection_start.col = 0;
	terminal->pvt->selection_end.row = _vte_ring_next (buffer->pvt->screen->row_data);
	terminal->pvt->selection_end.col = -1;

	_vte_debug_print(VTE_DEBUG_SELECTION, "Selecting *all* text.\n");

	vte_view_emit_copy_primary(terminal);
	vte_view_emit_selection_changed (terminal);
	_vte_invalidate_all (terminal);
}

/**
 * vte_view_unselect_all:
 * @terminal: a #VteView
 *
 * Clears the current selection.
 */
void
vte_view_unselect_all(VteView *terminal)
{
	g_return_if_fail (VTE_IS_VIEW (terminal));

	_vte_debug_print(VTE_DEBUG_SELECTION, "Clearing selection.\n");

	vte_view_deselect_all (terminal);
}

/* Autoscroll a bit. */
static gboolean
vte_view_autoscroll(VteView *terminal)
{
        VteBuffer *buffer;
	gboolean extend = FALSE;
	long x, y, xmax, ymax;
	glong adj;

        buffer = terminal->pvt->buffer;

	/* Provide an immediate effect for mouse wigglers. */
	if (terminal->pvt->mouse_last_y < 0) {
		if (terminal->pvt->vadjustment) {
			/* Try to scroll up by one line. */
			adj = buffer->pvt->screen->scroll_delta - 1;
			vte_view_queue_adjustment_value_changed_clamped (terminal, adj);
			extend = TRUE;
		}
		_vte_debug_print(VTE_DEBUG_EVENTS, "Autoscrolling down.\n");
	}
	if (terminal->pvt->mouse_last_y >=
	    buffer->pvt->row_count * terminal->pvt->char_height) {
		if (terminal->pvt->vadjustment) {
			/* Try to scroll up by one line. */
			adj = buffer->pvt->screen->scroll_delta + 1;
			vte_view_queue_adjustment_value_changed_clamped (terminal, adj);
			extend = TRUE;
		}
		_vte_debug_print(VTE_DEBUG_EVENTS, "Autoscrolling up.\n");
	}
	if (extend) {
		/* Don't select off-screen areas.  That just confuses people. */
		xmax = buffer->pvt->column_count * terminal->pvt->char_width;
		ymax = buffer->pvt->row_count * terminal->pvt->char_height;

		x = CLAMP(terminal->pvt->mouse_last_x, 0, xmax);
		y = CLAMP(terminal->pvt->mouse_last_y, 0, ymax);
		/* If we clamped the Y, mess with the X to get the entire
		 * lines. */
		if (terminal->pvt->mouse_last_y < 0 && !terminal->pvt->selection_block_mode) {
			x = 0;
		}
		if (terminal->pvt->mouse_last_y >= ymax && !terminal->pvt->selection_block_mode) {
			x = buffer->pvt->column_count * terminal->pvt->char_width;
		}
		/* Extend selection to cover the newly-scrolled area. */
		vte_view_extend_selection(terminal, x, y, FALSE, TRUE);
	} else {
		/* Stop autoscrolling. */
		terminal->pvt->mouse_autoscroll_tag = 0;
	}
	return (terminal->pvt->mouse_autoscroll_tag != 0);
}

/* Start autoscroll. */
static void
vte_view_start_autoscroll(VteView *terminal)
{
        VteBuffer *buffer;

        buffer = terminal->pvt->buffer;

	if (terminal->pvt->mouse_autoscroll_tag == 0) {
		terminal->pvt->mouse_autoscroll_tag =
			g_timeout_add_full(G_PRIORITY_LOW,
					   666 / buffer->pvt->row_count,
					   (GSourceFunc)vte_view_autoscroll,
					   terminal,
					   NULL);
	}
}

/* Stop autoscroll. */
static void
vte_view_stop_autoscroll(VteView *terminal)
{
	if (terminal->pvt->mouse_autoscroll_tag != 0) {
		g_source_remove(terminal->pvt->mouse_autoscroll_tag);
		terminal->pvt->mouse_autoscroll_tag = 0;
	}
}

/* Read and handle a motion event. */
static gboolean
vte_view_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
        VteView *terminal = VTE_VIEW(widget);
        VteViewPrivate *pvt = terminal->pvt;
        VteBuffer *buffer;
	int height;
	long x, y;
        long cell_x, cell_y;
	gboolean handled = FALSE;

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return FALSE;

        (void) _vte_view_xy_to_grid(terminal, event->x, event->y, &cell_x, &cell_y);
	x = event->x - terminal->pvt->padding.left;
	y = event->y - terminal->pvt->padding.top;
	height = terminal->pvt->char_height;

	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Motion notify (%ld,%ld) [grid %ld,%ld].\n",
			(long)event->x, (long)event->y,
                        cell_x, cell_y + buffer->pvt->screen->scroll_delta);

	vte_view_read_modifiers (terminal, (GdkEvent*) event);

	if (terminal->pvt->mouse_last_button) {
		vte_view_match_hilite_hide (terminal);
	} else {
		/* Hilite any matches. */
		vte_view_match_hilite(terminal, x, y);
		/* Show the cursor. */
		_vte_view_set_pointer_visible(terminal, TRUE);
	}

	switch (event->type) {
	case GDK_MOTION_NOTIFY:
                if (terminal->pvt->selecting_after_threshold) {
                        if (!gtk_drag_check_threshold (widget,
                                                       terminal->pvt->mouse_last_x,
                                                       terminal->pvt->mouse_last_y,
                                                       x, y))
                                return TRUE;

                        vte_view_start_selection(terminal,
                                                 terminal->pvt->mouse_last_x,
                                                 terminal->pvt->mouse_last_y,
                                                 selection_type_char);
                }

		if (terminal->pvt->selecting &&
		    ((terminal->pvt->modifiers & GDK_SHIFT_MASK) ||
		      !terminal->pvt->mouse_tracking_mode))
		{
			_vte_debug_print(VTE_DEBUG_EVENTS, "Mousing drag 1.\n");
			vte_view_extend_selection(terminal,
						      x, y, FALSE, FALSE);

			/* Start scrolling if we need to. */
			if (event->y < terminal->pvt->padding.top ||
			    event->y >= buffer->pvt->row_count * height +
                                        terminal->pvt->padding.top)
			{
				/* Give mouse wigglers something. */
				vte_view_autoscroll(terminal);
				/* Start a timed autoscroll if we're not doing it
				 * already. */
				vte_view_start_autoscroll(terminal);
			}

			handled = TRUE;
		}

		if (!handled)
			vte_view_maybe_send_mouse_drag(terminal, event);
		break;
	default:
		break;
	}

	/* Save the pointer coordinates for later use. */
	terminal->pvt->mouse_last_x = x;
	terminal->pvt->mouse_last_y = y;
        pvt->mouse_last_cell_x = cell_x;
        pvt->mouse_last_cell_y = cell_y;

	return handled;
}

/* Read and handle a pointing device buttonpress event. */
static gint
vte_view_button_press(GtkWidget *widget, GdkEventButton *event)
{
	VteView *terminal = VTE_VIEW(widget);
        VteViewPrivate *pvt = terminal->pvt;
        VteBuffer *buffer;
	long height, width, delta;
	gboolean handled = FALSE;
	gboolean start_selecting = FALSE, extend_selecting = FALSE;
        long cell_x, cell_y;
	long cellx, celly;
	long x,y;

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return FALSE;

	x = event->x - terminal->pvt->padding.left;
	y = event->y - terminal->pvt->padding.top;

	height = terminal->pvt->char_height;
	width = terminal->pvt->char_width;
	delta = buffer->pvt->screen->scroll_delta;

	vte_view_match_hilite(terminal, x, y);

	_vte_view_set_pointer_visible(terminal, TRUE);

	vte_view_read_modifiers (terminal, (GdkEvent*) event);

	/* Convert the event coordinates to cell coordinates. */
        (void) _vte_view_xy_to_grid(terminal, event->x, event->y, &cell_x, &cell_y);
	cellx = x / width;
	celly = y / height + delta;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Button %d single-click at (%ld,%ld)\n",
				event->button,
				x, y + terminal->pvt->char_height * delta);
		/* Handle this event ourselves. */
		switch (event->button) {
		case 1:
			_vte_debug_print(VTE_DEBUG_EVENTS,
					"Handling click ourselves.\n");
			/* Grab focus. */
			if (! gtk_widget_has_focus (widget)) {
				gtk_widget_grab_focus(widget);
			}

			/* If we're in event mode, and the user held down the
			 * shift key, we start selecting. */
			if (terminal->pvt->mouse_tracking_mode) {
				if (terminal->pvt->modifiers & GDK_SHIFT_MASK) {
					start_selecting = TRUE;
				}
			} else {
				/* If the user hit shift, then extend the
				 * selection instead. */
				if ((terminal->pvt->modifiers & GDK_SHIFT_MASK) &&
				    (terminal->pvt->has_selection ||
				     terminal->pvt->selecting_restart) &&
				    !vte_view_cell_is_selected(buffer,
							  cellx,
							  celly,
							  terminal)) {
					extend_selecting = TRUE;
				} else {
					start_selecting = TRUE;
				}
			}
			if (start_selecting) {
				vte_view_deselect_all(terminal);
                                 terminal->pvt->selecting_after_threshold = TRUE;
				handled = TRUE;
			}
			if (extend_selecting) {
				vte_view_extend_selection(terminal,
							      x, y,
							      !terminal->pvt->selecting_restart, TRUE);
				/* The whole selection code needs to be
				 * rewritten.  For now, put this here to
				 * fix bug 614658 */
				terminal->pvt->selecting = TRUE;
				handled = TRUE;
			}
			break;
		/* Paste if the user pressed shift or we're not sending events
		 * to the app. */
		case 2:
			if ((terminal->pvt->modifiers & GDK_SHIFT_MASK) ||
			    !terminal->pvt->mouse_tracking_mode) {
				vte_view_emit_paste_primary(terminal);
				handled = TRUE;
			}
			break;
		case 3:
		default:
			break;
		}
		/* If we haven't done anything yet, try sending the mouse
		 * event to the app. */
		if (handled == FALSE) {
			vte_view_maybe_send_mouse_button(terminal, event);
			handled = TRUE;
		}
		break;
	case GDK_2BUTTON_PRESS:
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Button %d double-click at (%ld,%ld)\n",
				event->button,
				x, y + (terminal->pvt->char_height * delta));
		switch (event->button) {
		case 1:
                        if (terminal->pvt->selecting_after_threshold) {
                                vte_view_start_selection(terminal,
                                                         x, y,
                                                         selection_type_char);
                        }
			if ((terminal->pvt->modifiers & GDK_SHIFT_MASK) ||
			    !terminal->pvt->mouse_tracking_mode) {
				vte_view_start_selection(terminal,
							x, y,
							     selection_type_word);
				vte_view_extend_selection(terminal,
							      x, y, FALSE, TRUE);
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
				x, y + (terminal->pvt->char_height * delta));
		switch (event->button) {
		case 1:
			if ((terminal->pvt->modifiers & GDK_SHIFT_MASK) ||
			    !terminal->pvt->mouse_tracking_mode) {
				vte_view_start_selection(terminal,
							x, y,
                                                         selection_type_line);
				vte_view_extend_selection(terminal,
							      x, y, FALSE, TRUE);
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
	terminal->pvt->mouse_last_button = event->button;
	terminal->pvt->mouse_last_x = x;
	terminal->pvt->mouse_last_y = y;
        pvt->mouse_last_cell_x = cell_x;
        pvt->mouse_last_cell_y = cell_y;

	return TRUE;
}

/* Read and handle a pointing device buttonrelease event. */
static gint
vte_view_button_release(GtkWidget *widget, GdkEventButton *event)
{
        VteView *terminal = VTE_VIEW(widget);
        VteViewPrivate *pvt = terminal->pvt;
        VteBuffer *buffer;
	gboolean handled = FALSE;
        long cell_x, cell_y;
	int x, y;

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return FALSE;

        (void) _vte_view_xy_to_grid(terminal, event->x, event->y, &cell_x, &cell_y);
	x = event->x - terminal->pvt->padding.left;
	y = event->y - terminal->pvt->padding.top;

	vte_view_match_hilite(terminal, x, y);

	_vte_view_set_pointer_visible(terminal, TRUE);

	vte_view_stop_autoscroll(terminal);

	vte_view_read_modifiers (terminal, (GdkEvent*) event);

	switch (event->type) {
	case GDK_BUTTON_RELEASE:
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Button %d released at (%d,%d).\n",
				event->button, x, y);
		switch (event->button) {
		case 1:
			/* If Shift is held down, or we're not in events mode,
			 * copy the selected text. */
			if ((terminal->pvt->modifiers & GDK_SHIFT_MASK) ||
			    !terminal->pvt->mouse_tracking_mode)
				handled = _vte_view_maybe_end_selection (terminal);
			break;
		case 2:
			if ((terminal->pvt->modifiers & GDK_SHIFT_MASK) ||
			    !terminal->pvt->mouse_tracking_mode) {
				handled = TRUE;
			}
			break;
		case 3:
		default:
			break;
		}
		if (!handled) {
			vte_view_maybe_send_mouse_button(terminal, event);
			handled = TRUE;
		}
		break;
	default:
		break;
	}

	/* Save the pointer state for later use. */
	terminal->pvt->mouse_last_button = 0;
	terminal->pvt->mouse_last_x = x;
	terminal->pvt->mouse_last_y = y;
        terminal->pvt->selecting_after_threshold = FALSE;
        pvt->mouse_last_cell_x = cell_x;
        pvt->mouse_last_cell_y = cell_y;

	return TRUE;
}

/* Handle receiving or losing focus. */
static gboolean
vte_view_focus_in(GtkWidget *widget, GdkEventFocus *event)
{
	VteView *terminal = VTE_VIEW(widget);
        VteViewPrivate *pvt = terminal->pvt;

	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus in.\n");

	gtk_widget_grab_focus (widget);

	/* Read the keyboard modifiers, though they're probably garbage. */
	vte_view_read_modifiers (terminal, (GdkEvent*) event);

	/* We only have an IM context when we're realized, and there's not much
	 * point to painting the cursor if we don't have a window. */
	if (gtk_widget_get_realized (widget)) {
		terminal->pvt->cursor_blink_state = TRUE;
		terminal->pvt->has_focus = TRUE;

		_vte_check_cursor_blink (terminal);

		gtk_im_context_focus_in(pvt->im_context);
		_vte_invalidate_cursor_once(terminal, FALSE);
		_vte_view_set_pointer_visible(terminal, TRUE);
	}

	return FALSE;
}

static gboolean
vte_view_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
	VteView *terminal = VTE_VIEW(widget);
        VteViewPrivate *pvt = terminal->pvt;

	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus out.\n");

	/* Read the keyboard modifiers, though they're probably garbage. */
	vte_view_read_modifiers (terminal, (GdkEvent*) event);
	/* We only have an IM context when we're realized, and there's not much
	 * point to painting ourselves if we don't have a window. */
	if (gtk_widget_get_realized (widget)) {
		_vte_view_maybe_end_selection (terminal);

		gtk_im_context_focus_out(pvt->im_context);
		_vte_invalidate_cursor_once(terminal, FALSE);

		/* XXX Do we want to hide the match just because the terminal
		 * lost keyboard focus, but the pointer *is* still within our
		 * area top? */
		vte_view_match_hilite_hide (terminal);
		/* Mark the cursor as invisible to disable hilite updating */
		terminal->pvt->mouse_cursor_visible = FALSE;
	}

	pvt->has_focus = FALSE;
	_vte_check_cursor_blink (terminal);

	return FALSE;
}

static gboolean
vte_view_enter(GtkWidget *widget, GdkEventCrossing *event)
{
	gboolean ret = FALSE;
	_vte_debug_print(VTE_DEBUG_EVENTS, "Enter.\n");
	if (GTK_WIDGET_CLASS (vte_view_parent_class)->enter_notify_event) {
		ret = GTK_WIDGET_CLASS (vte_view_parent_class)->enter_notify_event (widget, event);
	}
	if (gtk_widget_get_realized (widget)) {
		VteView *terminal = VTE_VIEW (widget);
		/* Hilite any matches. */
		vte_view_match_hilite_show(terminal,
					       event->x - terminal->pvt->padding.left,
					       event->y - terminal->pvt->padding.top);
	}
	return ret;
}
static gboolean
vte_view_leave(GtkWidget *widget, GdkEventCrossing *event)
{
	gboolean ret = FALSE;
	_vte_debug_print(VTE_DEBUG_EVENTS, "Leave.\n");
	if (GTK_WIDGET_CLASS (vte_view_parent_class)->leave_notify_event) {
		ret = GTK_WIDGET_CLASS (vte_view_parent_class)->leave_notify_event (widget, event);
	}
	if (gtk_widget_get_realized (widget)) {
		VteView *terminal = VTE_VIEW (widget);
		vte_view_match_hilite_hide (terminal);
		/* Mark the cursor as invisible to disable hilite updating,
		 * whilst the cursor is absent (otherwise we copy the entire
		 * buffer after each update for nothing...)
		 */
		terminal->pvt->mouse_cursor_visible = FALSE;
	}
	return ret;
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

static void
vte_view_set_visibility (VteView *terminal, GdkVisibilityState state)
{
	_vte_debug_print(VTE_DEBUG_MISC, "change visibility: %s -> %s.\n",
			visibility_state_str(terminal->pvt->visibility_state),
			visibility_state_str(state));

	if (state == terminal->pvt->visibility_state) {
		return;
	}

	/* fully obscured to visible switch, force the fast path */
	if (terminal->pvt->visibility_state == GDK_VISIBILITY_FULLY_OBSCURED) {
		/* set invalidated_all false, since we didn't really mean it
		 * when we set it to TRUE when becoming obscured */
		terminal->pvt->invalidated_all = FALSE;

		/* if all unobscured now, invalidate all, otherwise, wait
		 * for the expose event */
		if (state == GDK_VISIBILITY_UNOBSCURED) {
			_vte_invalidate_all (terminal);
		}
	}

	terminal->pvt->visibility_state = state;

	/* no longer visible, stop processing display updates */
	if (terminal->pvt->visibility_state == GDK_VISIBILITY_FULLY_OBSCURED) {
		remove_update_timeout (terminal);
		/* if fully obscured, just act like we have invalidated all,
		 * so no updates are accumulated. */
		terminal->pvt->invalidated_all = TRUE;
	}
}

static gboolean
vte_view_visibility_notify(GtkWidget *widget, GdkEventVisibility *event)
{
	VteView *terminal;
	terminal = VTE_VIEW(widget);

	_vte_debug_print(VTE_DEBUG_EVENTS, "Visibility (%s -> %s).\n",
			visibility_state_str(terminal->pvt->visibility_state),
			visibility_state_str(event->state));
	vte_view_set_visibility(terminal, event->state);

	return FALSE;
}

/* Apply the changed metrics, and queue a resize if need be. */
static void
vte_view_apply_metrics(VteView *terminal,
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
		vte_view_emit_char_size_changed(terminal,
						    terminal->pvt->char_width,
						    terminal->pvt->char_height);
	}
	/* Repaint. */
	_vte_invalidate_all(terminal);
}


static void
vte_view_ensure_font (VteView *terminal)
{
	if (terminal->pvt->draw != NULL) {
		if (terminal->pvt->fontdirty) {
			gint width, height, ascent;
			terminal->pvt->fontdirty = FALSE;
			_vte_draw_set_text_font (terminal->pvt->draw,
                                                 GTK_WIDGET(terminal),
                                                 terminal->pvt->fontdesc);
			_vte_draw_get_text_metrics (terminal->pvt->draw,
						    &width, &height, &ascent);
			vte_view_apply_metrics(terminal,
						   width, height, ascent, height - ascent);
		}
	}
}

static void
vte_view_update_font(VteView *terminal)
{
        VteViewPrivate *pvt = terminal->pvt;
        PangoFontDescription *desc;
        gdouble size;

        desc = pango_font_description_copy(pvt->unscaled_font_desc);

        size = pango_font_description_get_size(desc);
        if (pango_font_description_get_size_is_absolute(desc)) {
                pango_font_description_set_absolute_size(desc, pvt->font_scale * size);
        } else {
                pango_font_description_set_size(desc, pvt->font_scale * size);
        }

        if (pvt->fontdesc) {
                pango_font_description_free(pvt->fontdesc);
        }
        pvt->fontdesc = desc;

        pvt->fontdirty = TRUE;
        pvt->has_fonts = TRUE;

        /* Set the drawing font. */
        if (gtk_widget_get_realized (&terminal->widget)) {
                vte_view_ensure_font (terminal);
        }
}

/*
 * _vte_view_set_font:
 * @terminal: a #VteView
 * @font_desc: (allow-none): a #PangoFontDescription for the desired font, or %NULL
 *
 * Sets the font used for rendering all text displayed by the terminal,
 * overriding any fonts set using gtk_widget_modify_font().  The terminal
 * will immediately attempt to load the desired font, retrieve its
 * metrics, and attempt to resize itself to keep the same number of rows
 * and columns.
 */
static void
vte_view_set_font(VteView *terminal,
                      PangoFontDescription *desc /* adopted */)
{
        VteViewPrivate *pvt = terminal->pvt;

	/* Create an owned font description. */
        _VTE_DEBUG_IF(VTE_DEBUG_MISC | VTE_DEBUG_STYLE) {
                char *tmp;
                tmp = pango_font_description_to_string(desc);
                g_printerr("Using pango font \"%s\".\n", tmp);
                g_free (tmp);
	}

	/* Note that we proceed to recreating the font even if
         * pango_font_description_equal(@desc, pvt->fontdesc).
         * This is because maybe screen font options were changed,
         * or new fonts installed.  Those will be
         * detected at font creation time and respected.
         *
         * FIXMEchpe: handle these separately!
	 */

	/* Free the old font description and save the new one. */
        if (pvt->unscaled_font_desc != NULL) {
                pango_font_description_free(pvt->unscaled_font_desc);
        }
	pvt->unscaled_font_desc = desc /* adopted */;

        vte_view_update_font(terminal);
}

/**
 * vte_view_set_font_scale:
 * @terminal: a #VteView
 * @scale: the font scale
 *
 * Sets the terminal's font scale to @scale.
 */
void
vte_view_set_font_scale(VteView *terminal,
                            gdouble scale)
{
  g_return_if_fail(VTE_IS_VIEW(terminal));

  terminal->pvt->font_scale = CLAMP(scale, VTE_SCALE_MIN, VTE_SCALE_MAX);

  vte_view_update_font(terminal);

  g_object_notify(G_OBJECT(terminal), "font-scale");  
}

/**
 * vte_view_get_font_scale:
 * @terminal: a #VteView
 *
 * Returns: the terminal's font scale
 */
gdouble
vte_view_get_font_scale(VteView *terminal)
{
  g_return_val_if_fail(VTE_IS_VIEW(terminal), 1.);

  return terminal->pvt->font_scale;
}

/* Read and refresh our perception of the size of the PTY. */
static void
vte_buffer_refresh_size(VteBuffer *buffer)
{
        VteBufferPrivate *pvt = buffer->pvt;
	int rows, columns;
        GError *error = NULL;

        if (pvt->pty == NULL)
                return;

        if (vte_pty_get_size(pvt->pty, &rows, &columns, &error)) {
                buffer->pvt->row_count = rows;
                buffer->pvt->column_count = columns;
        } else {
                g_warning(_("Error reading PTY size, using defaults: %s\n"), error->message);
                g_error_free(error);
                buffer->pvt->row_count = buffer->pvt->default_row_count;
                buffer->pvt->column_count = buffer->pvt->default_column_count;
	}
}

/**
 * vte_buffer_set_size:
 * @buffer: a #VteBuffer
 * @columns: the desired number of columns
 * @rows: the desired number of rows
 *
 * Attempts to change the buffer's size in terms of rows and columns.  If
 * the attempt succeeds, the widget will resize itself to the proper size.
 */
void
vte_buffer_set_size(VteBuffer *buffer, glong columns, glong rows)
{
        VteView *terminal;
	glong old_columns, old_rows;

	g_return_if_fail(VTE_IS_BUFFER(buffer));

        terminal = buffer->pvt->terminal;

	_vte_debug_print(VTE_DEBUG_MISC,
			"Setting PTY size to %ldx%ld.\n",
			columns, rows);

	old_rows = buffer->pvt->row_count;
	old_columns = buffer->pvt->column_count;

	if (buffer->pvt->pty != NULL) {
                GError *error = NULL;

		/* Try to set the buffer size, and read it back,
		 * in case something went awry.
                 */
		if (!vte_pty_set_size(buffer->pvt->pty, rows, columns, &error)) {
			g_warning("%s\n", error->message);
                        g_error_free(error);
		}
		vte_buffer_refresh_size(buffer);
	} else {
		buffer->pvt->row_count = rows;
		buffer->pvt->column_count = columns;
	}

	if (buffer->pvt->terminal != NULL &&
	    (old_rows != buffer->pvt->row_count || old_columns != buffer->pvt->column_count)) {
		VteScreen *screen = buffer->pvt->screen;
		glong visible_rows = MIN (old_rows, _vte_ring_length (screen->row_data));
		if (buffer->pvt->row_count < visible_rows) {
			glong delta = visible_rows - buffer->pvt->row_count;
			screen->insert_delta += delta;
			vte_view_queue_adjustment_value_changed (
					terminal,
					screen->scroll_delta + delta);
		}
		gtk_widget_queue_resize_no_redraw (&terminal->widget);
		/* Our visible text changed. */
		vte_buffer_emit_text_modified(buffer);
	}
}

/* Redraw the widget. */
static void
vte_view_handle_scroll(VteView *terminal)
{
        VteBuffer *buffer;
	long dy, adj;
	VteScreen *screen;

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return;

	screen = buffer->pvt->screen;

	/* Read the new adjustment value and save the difference. */
	adj = round (gtk_adjustment_get_value(terminal->pvt->vadjustment));
	dy = adj - screen->scroll_delta;
	screen->scroll_delta = adj;

	/* Sanity checks. */
        if (!gtk_widget_get_realized(&terminal->widget)) {
                return;
        }
	if (terminal->pvt->visibility_state == GDK_VISIBILITY_FULLY_OBSCURED) {
		return;
	}

	if (dy != 0) {
		_vte_debug_print(VTE_DEBUG_ADJ,
			    "Scrolling by %ld\n", dy);
		_vte_view_scroll_region(terminal, screen->scroll_delta,
					    buffer->pvt->row_count, -dy);
		vte_view_emit_text_scrolled(terminal, dy);
		_vte_buffer_queue_contents_changed(buffer);
	} else {
		_vte_debug_print(VTE_DEBUG_ADJ, "Not scrolling\n");
	}
}

static void
vte_view_set_hadjustment(VteView *terminal,
                             GtkAdjustment *adjustment)
{
  VteViewPrivate *pvt = terminal->pvt;

  if (adjustment == pvt->hadjustment)
    return;

  if (pvt->hadjustment)
    g_object_unref (pvt->hadjustment);

  pvt->hadjustment = adjustment ? g_object_ref_sink (adjustment) : NULL;
}

static void
vte_view_set_vadjustment(VteView *terminal,
                             GtkAdjustment *adjustment)
{
	if (adjustment != NULL && adjustment == terminal->pvt->vadjustment)
		return;
	if (adjustment == NULL && terminal->pvt->vadjustment != NULL)
		return;

	if (adjustment == NULL)
		adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0));
	else
		g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));

	/* Add a reference to the new adjustment object. */
	g_object_ref_sink(adjustment);
	/* Get rid of the old adjustment object. */
	if (terminal->pvt->vadjustment != NULL) {
		/* Disconnect our signal handlers from this object. */
		g_signal_handlers_disconnect_by_func(terminal->pvt->vadjustment,
						     vte_view_handle_scroll,
						     terminal);
		g_object_unref(terminal->pvt->vadjustment);
	}

	/* Set the new adjustment object. */
	terminal->pvt->vadjustment = adjustment;

	/* We care about the offset, not the top or bottom. */
	g_signal_connect_swapped(terminal->pvt->vadjustment,
				 "value-changed",
				 G_CALLBACK(vte_view_handle_scroll),
				 terminal);
}

/**
 * vte_buffer_set_emulation:
 * @buffer: a #VteBuffer
 * @emulation: (allow-none): the name of a buffer description, or %NULL to use the default
 *
 * Sets what type of buffer the widget attempts to emulate by scanning for
 * control sequences defined in the system's termcap file.  Unless you
 * are interested in this feature, always use "xterm".
 */
void
vte_buffer_set_emulation(VteBuffer *buffer, const char *emulation)
{
        GObject *object;
	int columns, rows;

	g_return_if_fail(VTE_IS_BUFFER(buffer));

        object = G_OBJECT(buffer);

        g_object_freeze_notify(object);

	/* Set the emulation type, for reference. */
	if (emulation == NULL) {
		emulation = vte_get_default_emulation();
	}
	buffer->pvt->emulation = g_intern_string(emulation);
	_vte_debug_print(VTE_DEBUG_MISC,
			"Setting emulation to `%s'...\n", emulation);
	/* Find and read the right termcap file. */
	vte_buffer_set_termcap(buffer);

	/* Create a table to hold the control sequences. */
	if (buffer->pvt->matcher != NULL) {
		_vte_matcher_free(buffer->pvt->matcher);
	}
	buffer->pvt->matcher = _vte_matcher_new(emulation, buffer->pvt->termcap);

	if (buffer->pvt->termcap != NULL) {
		/* Read emulation flags. */
		buffer->pvt->flags.am = _vte_termcap_find_boolean(buffer->pvt->termcap,
								    buffer->pvt->emulation,
								    "am");
		buffer->pvt->flags.bw = _vte_termcap_find_boolean(buffer->pvt->termcap,
								    buffer->pvt->emulation,
								    "bw");
		buffer->pvt->flags.LP = _vte_termcap_find_boolean(buffer->pvt->termcap,
								    buffer->pvt->emulation,
								    "LP");
		buffer->pvt->flags.ul = _vte_termcap_find_boolean(buffer->pvt->termcap,
								    buffer->pvt->emulation,
								    "ul");
		buffer->pvt->flags.xn = _vte_termcap_find_boolean(buffer->pvt->termcap,
								    buffer->pvt->emulation,
								    "xn");

		/* Resize to the given default. */
		columns = _vte_termcap_find_numeric(buffer->pvt->termcap,
						    buffer->pvt->emulation,
						    "co");
		if (columns <= 0) {
			columns = VTE_COLUMNS;
		}
		buffer->pvt->default_column_count = columns;

		rows = _vte_termcap_find_numeric(buffer->pvt->termcap,
						 buffer->pvt->emulation,
						 "li");
		if (rows <= 0 ) {
			rows = VTE_ROWS;
		}
		buffer->pvt->default_row_count = rows;
	}

	/* Notify observers that we changed our emulation. */
	vte_buffer_emit_emulation_changed(buffer);

        g_object_thaw_notify(object);
}

/**
 * vte_get_default_emulation:
 *
 * Returns the default emulation, which is used in #VteView if the
 * terminal type passed to vte_view_set_emulation() is %NULL.
 *
 * Returns: (transfer none) (type utf8): an interned string containing the name
 *   of the default terminal type the widget attempts to emulate
 */
const char *
vte_get_default_emulation(void)
{
	return g_intern_static_string(VTE_DEFAULT_EMULATION);
}

/**
 * vte_buffer_get_emulation:
 * @buffer: a #VteBuffer
 *
 * Queries the buffer for its current emulation, as last set by a call to
 * vte_buffer_set_emulation().
 *
 * Returns: an interned string containing the name of the buffer type the
 *   terminal is attempting to emulate
 */
const char *
vte_buffer_get_emulation(VteBuffer *buffer)
{
	g_return_val_if_fail(VTE_IS_BUFFER(buffer), NULL);
	return buffer->pvt->emulation;
}

static void
vte_buffer_inline_error_message(VteBuffer *buffer,
                                const char *format,
                                ...) G_GNUC_PRINTF(2, 3);

static void
vte_buffer_inline_error_message(VteBuffer *buffer,
                                const char *format,
                                ...)
{
	va_list ap;
	char *str;

	va_start (ap, format);
	str = g_strdup_vprintf (format, ap);
	va_end (ap);

	vte_buffer_feed (buffer, "*** VTE ***: ", 13);
	vte_buffer_feed (buffer, str, -1);
	vte_buffer_feed (buffer, "\r\n", 2);
	g_free (str);
}

/* Set the path to the termcap file we read, and read it in. */
static void
vte_buffer_set_termcap(VteBuffer *buffer)
{
        GObject *object = G_OBJECT(buffer);
        const char *emulation;

        g_object_freeze_notify(object);

        emulation = buffer->pvt->emulation ? buffer->pvt->emulation
                                            : vte_get_default_emulation();

	_vte_debug_print(VTE_DEBUG_MISC, "Loading termcap `%s'...",
			 emulation);
	if (buffer->pvt->termcap != NULL) {
		_vte_termcap_free(buffer->pvt->termcap);
	}
	buffer->pvt->termcap = _vte_termcap_new(emulation);
	_vte_debug_print(VTE_DEBUG_MISC, "\n");
	if (buffer->pvt->termcap == NULL) {
		vte_buffer_inline_error_message(buffer,
				"Failed to load buffer capabilities for '%s'",
				emulation);
	}

        g_object_thaw_notify(object);
}

static void
_vte_view_codeset_changed_cb(struct _vte_iso2022_state *state, gpointer p)
{
	vte_buffer_set_encoding(VTE_BUFFER(p), _vte_iso2022_state_get_codeset(state));
}

/* Initialize the terminal widget after the base widget stuff is initialized.
 * We need to create a new psuedo-terminal pair, read in the termcap file, and
 * set ourselves up to do the interpretation of sequences. */
static void
vte_view_init(VteView *terminal)
{
	VteViewPrivate *pvt;
        GtkStyleContext *context;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_view_init()\n");

	/* Initialize private data. */
	pvt = terminal->pvt = G_TYPE_INSTANCE_GET_PRIVATE (terminal, VTE_TYPE_VIEW, VteViewPrivate);

        /* --- */

	gtk_widget_set_can_focus(&terminal->widget, TRUE);

	gtk_widget_set_app_paintable (&terminal->widget, TRUE);

	/* We do our own redrawing. */
	gtk_widget_set_redraw_on_allocate (&terminal->widget, FALSE);

	/* Set an adjustment for the application to use to control scrolling. */
        terminal->pvt->vadjustment = NULL;
        pvt->hadjustment = NULL;
        /* GtkScrollable */
        pvt->hscroll_policy = GTK_SCROLL_NATURAL;
        pvt->vscroll_policy = GTK_SCROLL_NATURAL;

        vte_view_set_hadjustment(terminal, NULL);
	vte_view_set_vadjustment(terminal, NULL);

	/* Set up dummy metrics, value != 0 to avoid division by 0 */
	terminal->pvt->char_width = 1;
	terminal->pvt->char_height = 1;
	terminal->pvt->char_ascent = 1;
	terminal->pvt->char_descent = 1;
	terminal->pvt->line_thickness = 1;
	terminal->pvt->underline_position = 1;
	terminal->pvt->strikethrough_position = 1;

	/* Scrolling options. */
	pvt->scroll_on_keystroke = TRUE;

	/* Selection info. */
	vte_view_set_word_chars(terminal, NULL);

	/* Miscellaneous options. */
	pvt->audible_bell = TRUE;
	pvt->bell_margin = 10;
	pvt->allow_bold = TRUE;

	/* Cursor shape. */
	pvt->cursor_shape = VTE_CURSOR_SHAPE_BLOCK;
        pvt->cursor_aspect_ratio = 0.04;

	/* Cursor blinking. */
	pvt->cursor_blink_timeout = 500;
        pvt->cursor_blinks = FALSE;
        pvt->cursor_blink_mode = VTE_CURSOR_BLINK_SYSTEM;
        pvt->cursor_blink_tag = 0;

        /* Style properties */
        pvt->reverse = FALSE;

	/* Matching data. */
	pvt->match_regexes = g_array_new(FALSE, TRUE,
					 sizeof(struct vte_match_regex));
        pvt->match_tag = -1;
	vte_view_match_hilite_clear(terminal);

	/* Rendering data.  Try everything. */
	pvt->draw = _vte_draw_new();

	pvt->selection_block_mode = FALSE;
        pvt->unscaled_font_desc = pvt->fontdesc = NULL;
        pvt->font_scale = 1.;
	pvt->has_fonts = FALSE;

	/* Not all backends generate GdkVisibilityNotify, so mark the
	 * window as unobscured initially. */
	pvt->visibility_state = GDK_VISIBILITY_UNOBSCURED;

        pvt->padding = default_padding;

#ifdef VTE_DEBUG
	/* In debuggable mode, we always do this. */
	/* gtk_widget_get_accessible(&terminal->widget); */
#endif

        context = gtk_widget_get_style_context (&terminal->widget);
        gtk_style_context_add_provider (context,
                                        VTE_VIEW_GET_CLASS (terminal)->priv->style_provider,
                                        GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);
        gtk_style_context_add_class (context, VTE_STYLE_CLASS_TERMINAL);

        vte_view_update_style (terminal);
}

/* Tell GTK+ how much space we need. */
static void
vte_view_get_preferred_width(GtkWidget *widget,
				 int       *minimum_width,
				 int       *natural_width)
{
        VteView *terminal = VTE_VIEW(widget);
        VteBuffer *buffer;
        glong column_count;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_view_get_preferred_width()\n");

	vte_view_ensure_font (terminal);

        buffer = terminal->pvt->buffer;
        if (buffer) {
                vte_buffer_refresh_size(buffer);
                column_count = vte_buffer_get_column_count(buffer);
        } else {
                column_count = VTE_COLUMNS;
        }

	*minimum_width = terminal->pvt->char_width * 1;
        *natural_width = terminal->pvt->char_width * column_count;

	*minimum_width += terminal->pvt->padding.left +
                          terminal->pvt->padding.right;
	*natural_width += terminal->pvt->padding.left +
                          terminal->pvt->padding.right;

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
			"[Terminal %p] minimum_width=%d, natural_width=%d for %ld cells.\n",
                        terminal,
			*minimum_width, *natural_width,
			column_count);
}

static void
vte_view_get_preferred_height(GtkWidget *widget,
				  int       *minimum_height,
				  int       *natural_height)
{
	VteView *terminal = VTE_VIEW(widget);
        VteBuffer *buffer;
        glong row_count;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_view_get_preferred_height()\n");

	vte_view_ensure_font (terminal);

        buffer = terminal->pvt->buffer;
        if (buffer) {
                vte_buffer_refresh_size(buffer);
                row_count = vte_buffer_get_row_count(buffer);
        } else {
                row_count = VTE_ROWS;
        }

	*minimum_height = terminal->pvt->char_height * 1;
        *natural_height = terminal->pvt->char_height * row_count;

	*minimum_height += terminal->pvt->padding.left +
			   terminal->pvt->padding.right;
	*natural_height += terminal->pvt->padding.left +
			   terminal->pvt->padding.right;

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
			"[Terminal %p] minimum_height=%d, natural_height=%d for %ld cells.\n",
                        terminal,
			*minimum_height, *natural_height,
			row_count);
}

/* Accept a given size from GTK+. */
static void
vte_view_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	VteView *terminal = VTE_VIEW(widget);
        VteBuffer *buffer;
	glong width, height;
	GtkAllocation current_allocation;
	gboolean repaint, update_scrollback;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE,
			"vte_view_size_allocate()\n");

	width = (allocation->width - (terminal->pvt->padding.left + terminal->pvt->padding.right)) /
		terminal->pvt->char_width;
	height = (allocation->height - (terminal->pvt->padding.top + terminal->pvt->padding.bottom)) /
		 terminal->pvt->char_height;
	width = MAX(width, 1);
	height = MAX(height, 1);

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
			"[Terminal %p] Sizing window to %dx%d (%ldx%ld).\n",
                        terminal,
			allocation->width, allocation->height,
			width, height);

	gtk_widget_get_allocation (widget, &current_allocation);

	repaint = current_allocation.width != allocation->width
			|| current_allocation.height != allocation->height;
	update_scrollback = current_allocation.height != allocation->height;

	/* Set our allocation to match the structure. */
	gtk_widget_set_allocation (widget, allocation);

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                goto done_buffer;

	if (width != buffer->pvt->column_count ||
            height != buffer->pvt->row_count ||
            update_scrollback)
	{
		VteScreen *screen = buffer->pvt->screen;

		/* Set the size of the pseudo-terminal. */
		vte_buffer_set_size(buffer, width, height);

		/* Adjust scrolling area in case our boundaries have just been
		 * redefined to be invalid. */
		if (screen->scrolling_restricted) {
			screen->scrolling_region.start =
				MIN(screen->scrolling_region.start,
						buffer->pvt->row_count - 1);
			screen->scrolling_region.end =
				MIN(screen->scrolling_region.end,
						buffer->pvt->row_count - 1);
		}

		/* Ensure scrollback buffers cover the screen. */
		vte_buffer_set_scrollback_lines(buffer, buffer->pvt->scrollback_lines);
		/* Ensure the cursor is valid */
		screen->cursor_current.row = CLAMP (screen->cursor_current.row,
				_vte_ring_delta (screen->row_data),
				MAX (_vte_ring_delta (screen->row_data),
					_vte_ring_next (screen->row_data) - 1));
		/* Notify viewers that the contents have changed. */
		_vte_buffer_queue_contents_changed(buffer);
	}

    done_buffer:

	/* Resize the GDK window. */
	if (gtk_widget_get_realized (widget)) {
		gdk_window_move_resize (gtk_widget_get_window (widget),
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);
		/* Force a repaint if we were resized. */
		if (repaint) {
			reset_update_regions (terminal);
			_vte_invalidate_all(terminal);
		}
	}
}

/* The window is being destroyed. */
static void
vte_view_unrealize(GtkWidget *widget)
{
	GdkWindow *window;
        VteView *terminal = VTE_VIEW(widget);
        VteBuffer *buffer;
        VteViewPrivate *pvt = terminal->pvt;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_view_unrealize()\n");

        buffer = terminal->pvt->buffer;

	window = gtk_widget_get_window (widget);

	/* Deallocate the cursors. */
	terminal->pvt->mouse_cursor_visible = FALSE;
	g_object_unref(terminal->pvt->mouse_default_cursor);
	terminal->pvt->mouse_default_cursor = NULL;
	g_object_unref(terminal->pvt->mouse_mousing_cursor);
	terminal->pvt->mouse_mousing_cursor = NULL;
	g_object_unref(terminal->pvt->mouse_inviso_cursor);
	terminal->pvt->mouse_inviso_cursor = NULL;

	vte_view_match_hilite_clear(terminal);

	/* Shut down input methods. */
	if (pvt->im_context != NULL) {
	        g_signal_handlers_disconnect_by_func (pvt->im_context,
						      vte_view_im_preedit_changed,
						      terminal);
		vte_view_im_reset(terminal);
		gtk_im_context_set_client_window(pvt->im_context, NULL);
		g_object_unref(pvt->im_context);
		pvt->im_context = NULL;
	}
	pvt->im_preedit_active = FALSE;
	if (pvt->im_preedit != NULL) {
		g_free(pvt->im_preedit);
		pvt->im_preedit = NULL;
	}
	if (pvt->im_preedit_attrs != NULL) {
		pango_attr_list_unref(pvt->im_preedit_attrs);
		pvt->im_preedit_attrs = NULL;
	}
	pvt->im_preedit_cursor = 0;

	/* Clean up our draw structure. */
	if (terminal->pvt->draw != NULL) {
		_vte_draw_free(terminal->pvt->draw);
		terminal->pvt->draw = NULL;
	}
	terminal->pvt->fontdirty = TRUE;

	/* Unmap the widget if it hasn't been already. */
	if (gtk_widget_get_mapped (widget)) {
		gtk_widget_unmap (widget);
	}

	/* Remove the GDK window. */
	if (window != NULL) {
		gdk_window_set_user_data (window, NULL);
		gtk_widget_set_window (widget, NULL);

		gdk_window_destroy (window);
	}

	/* Remove the blink timeout function. */
	remove_cursor_timeout(terminal);

	/* Cancel any pending redraws. */
	remove_update_timeout (terminal);

	/* Cancel any pending signals */
        if (buffer) {
                buffer->pvt->contents_changed_pending = FALSE;
                buffer->pvt->cursor_moved_pending = FALSE;
                buffer->pvt->text_modified_flag = FALSE;
                buffer->pvt->text_inserted_flag = FALSE;
                buffer->pvt->text_deleted_flag = FALSE;
        }

	/* Clear modifiers. */
	terminal->pvt->modifiers = 0;

	/* Mark that we no longer have a GDK window. */
	gtk_widget_set_realized (widget, FALSE);
}

static void
vte_view_sync_settings (GtkSettings *settings,
                            GParamSpec *pspec,
                            VteView *terminal)
{
        VteViewPrivate *pvt = terminal->pvt;
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

        if (pvt->cursor_blink_mode == VTE_CURSOR_BLINK_SYSTEM)
                vte_view_set_cursor_blinks_internal(terminal, blink);
}

static void
vte_view_screen_changed (GtkWidget *widget,
                             GdkScreen *previous_screen)
{
        VteView *terminal = VTE_VIEW (widget);
        GdkScreen *screen;
        GtkSettings *settings;

        screen = gtk_widget_get_screen (widget);
        if (previous_screen != NULL &&
            (screen != previous_screen || screen == NULL)) {
                settings = gtk_settings_get_for_screen (previous_screen);
                g_signal_handlers_disconnect_matched (settings, G_SIGNAL_MATCH_DATA,
                                                      0, 0, NULL, NULL,
                                                      widget);
        }

        if (GTK_WIDGET_CLASS (vte_view_parent_class)->screen_changed) {
                GTK_WIDGET_CLASS (vte_view_parent_class)->screen_changed (widget, previous_screen);
        }

        if (screen == previous_screen || screen == NULL)
                return;

        settings = gtk_widget_get_settings (widget);
        vte_view_sync_settings (settings, NULL, terminal);
        g_signal_connect (settings, "notify::gtk-cursor-blink",
                          G_CALLBACK (vte_view_sync_settings), widget);
        g_signal_connect (settings, "notify::gtk-cursor-blink-time",
                          G_CALLBACK (vte_view_sync_settings), widget);
        g_signal_connect (settings, "notify::gtk-cursor-blink-timeout",
                          G_CALLBACK (vte_view_sync_settings), widget);
}

/* Perform final cleanups for the widget before it's freed. */
static void
vte_view_finalize(GObject *object)
{
    	VteView *terminal = VTE_VIEW (object);
        VteViewPrivate *pvt = terminal->pvt;
        VteBuffer *buffer = pvt->buffer;
        GtkWidget *widget = &terminal->widget;
        GtkClipboard *clipboard;
        GtkSettings *settings;
	struct vte_match_regex *regex;
	guint i;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_view_finalize()\n");

	/* Free the draw structure. */
	if (terminal->pvt->draw != NULL) {
		_vte_draw_free(terminal->pvt->draw);
	}

	/* The NLS maps. */
	_vte_iso2022_state_free(buffer->pvt->iso2022);

	/* Free the font description. */
        if (pvt->unscaled_font_desc != NULL) {
                pango_font_description_free(pvt->unscaled_font_desc);
        }
	if (terminal->pvt->fontdesc != NULL) {
		pango_font_description_free(terminal->pvt->fontdesc);
	}

	/* Free matching data. */
	if (terminal->pvt->match_attributes != NULL) {
		g_array_free(terminal->pvt->match_attributes, TRUE);
	}
	g_free(terminal->pvt->match_contents);
	if (terminal->pvt->match_regexes != NULL) {
		for (i = 0; i < terminal->pvt->match_regexes->len; i++) {
			regex = &g_array_index(terminal->pvt->match_regexes,
					       struct vte_match_regex,
					       i);
			/* Skip holes. */
			if (regex->tag < 0) {
				continue;
			}
                        regex_match_clear(regex);
		}
		g_array_free(terminal->pvt->match_regexes, TRUE);
	}

	if (terminal->pvt->search_regex)
		g_regex_unref (terminal->pvt->search_regex);
	if (terminal->pvt->search_attrs)
		g_array_free (terminal->pvt->search_attrs, TRUE);

	/* Disconnect from autoscroll requests. */
	vte_view_stop_autoscroll(terminal);

	/* Cancel pending adjustment change notifications. */
	terminal->pvt->adjustment_changed_pending = FALSE;

	/* Free any selected text, but if we currently own the selection,
	 * throw the text onto the clipboard without an owner so that it
	 * doesn't just disappear. */
	if (terminal->pvt->selection != NULL) {
		clipboard = gtk_clipboard_get_for_display(gtk_widget_get_display(&terminal->widget),
                                                          GDK_SELECTION_PRIMARY);
		if (gtk_clipboard_get_owner(clipboard) == object) {
			gtk_clipboard_set_text(clipboard,
					       terminal->pvt->selection,
					       -1);
		}
		g_free(terminal->pvt->selection);
	}
	if (terminal->pvt->word_chars != NULL) {
		g_array_free(terminal->pvt->word_chars, TRUE);
	}

	/* Stop processing input. */
	vte_view_stop_processing (terminal);

	remove_update_timeout (terminal);

	/* Free public-facing data. */
	if (terminal->pvt->vadjustment != NULL) {
		g_object_unref(terminal->pvt->vadjustment);
	}

        settings = gtk_widget_get_settings (widget);
        g_signal_handlers_disconnect_matched (settings, G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL, NULL,
                                              terminal);

        vte_view_set_buffer(terminal, NULL);

	/* Call the inherited finalize() method. */
	G_OBJECT_CLASS(vte_view_parent_class)->finalize(object);
}

/* Handle realizing the widget.  Most of this is copy-paste from GGAD. */
static void
vte_view_realize(GtkWidget *widget)
{
        VteView *terminal = VTE_VIEW(widget);
        VteViewPrivate *pvt = terminal->pvt;
	GdkWindow *window;
	GdkWindowAttr attributes;
	GtkAllocation allocation;
	guint attributes_mask = 0;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_view_realize()\n");

	gtk_widget_get_allocation (widget, &allocation);

	/* Create the draw structure if we don't already have one. */
	if (terminal->pvt->draw == NULL) {
		terminal->pvt->draw = _vte_draw_new();
	}

	/* Create the stock cursors. */
	terminal->pvt->mouse_cursor_visible = TRUE;
	terminal->pvt->mouse_default_cursor =
		vte_view_cursor_new(terminal, VTE_DEFAULT_CURSOR);
	terminal->pvt->mouse_mousing_cursor =
		vte_view_cursor_new(terminal, VTE_MOUSING_CURSOR);

	/* Create a GDK window for the widget. */
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.event_mask = gtk_widget_get_events(widget) |
				GDK_EXPOSURE_MASK |
				GDK_VISIBILITY_NOTIFY_MASK |
				GDK_FOCUS_CHANGE_MASK |
				GDK_SCROLL_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_POINTER_MOTION_MASK |
				GDK_BUTTON1_MOTION_MASK |
				GDK_ENTER_NOTIFY_MASK |
				GDK_LEAVE_NOTIFY_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK;
	attributes.cursor = terminal->pvt->mouse_default_cursor;
	attributes_mask = GDK_WA_X |
			  GDK_WA_Y |
			  (attributes.visual ? GDK_WA_VISUAL : 0) |
			  GDK_WA_CURSOR;

	window = gdk_window_new (gtk_widget_get_parent_window (widget),
				 &attributes, attributes_mask);

	gtk_widget_set_window (widget, window);
	gdk_window_set_user_data (window, widget);
	_VTE_DEBUG_IF (VTE_DEBUG_UPDATES) gdk_window_set_debug_updates (TRUE);

	/* Set the realized flag. */
	gtk_widget_set_realized (widget, TRUE);

	/* Set up input method support.  FIXME: do we need to handle the
	 * "retrieve-surrounding" and "delete-surrounding" events? */
	if (pvt->im_context != NULL) {
		vte_view_im_reset(terminal);
		g_object_unref(pvt->im_context);
		pvt->im_context = NULL;
	}
	pvt->im_preedit_active = FALSE;
	pvt->im_context = gtk_im_multicontext_new();
	gtk_im_context_set_client_window (pvt->im_context, window);
	g_signal_connect(pvt->im_context, "commit",
			 G_CALLBACK(vte_view_im_commit), terminal);
	g_signal_connect(pvt->im_context, "preedit-start",
			 G_CALLBACK(vte_view_im_preedit_start),
			 terminal);
	g_signal_connect(pvt->im_context, "preedit-changed",
			 G_CALLBACK(vte_view_im_preedit_changed),
			 terminal);
	g_signal_connect(pvt->im_context, "preedit-end",
			 G_CALLBACK(vte_view_im_preedit_end),
			 terminal);
	gtk_im_context_set_use_preedit(pvt->im_context, TRUE);

	/* Clear modifiers. */
	terminal->pvt->modifiers = 0;

	/* Create our invisible cursor. */
	terminal->pvt->mouse_inviso_cursor = gdk_cursor_new_for_display(gtk_widget_get_display(widget), GDK_BLANK_CURSOR);

	vte_view_ensure_font (terminal);
}

static inline void
swap (guint *a, guint *b)
{
	guint tmp;
	tmp = *a, *a = *b, *b = tmp;
}

static void
vte_view_determine_colors_internal(VteView *terminal,
				       const VteCell *cell,
				       gboolean selected,
				       gboolean cursor,
				       guint *pfore, guint *pback)
{
	guint fore, back;

	if (!cell)
		cell = &basic_cell.cell;

	/* Start with cell colors */
	fore = cell->attr.fore;
	back = cell->attr.back;

	/* Reverse-mode switches default fore and back colors */
	if (G_UNLIKELY (terminal->pvt->buffer->pvt->screen->reverse_mode ^ terminal->pvt->reverse)) {
		if (fore == VTE_DEF_FG)
			fore = VTE_DEF_BG;
		if (back == VTE_DEF_BG)
			back = VTE_DEF_FG;
	}

	/* Handle bold by using set bold color or brightening */
	if (cell->attr.bold) {
		if (fore == VTE_DEF_FG)
			fore = VTE_BOLD_FG;
		else if (fore < VTE_LEGACY_COLOR_SET_SIZE) {
			fore += VTE_COLOR_BRIGHT_OFFSET;
		}
	}

	/* Handle half similarly */
	if (cell->attr.half) {
		if (fore == VTE_DEF_FG)
			fore = VTE_DIM_FG;
		else if ((fore < VTE_LEGACY_COLOR_SET_SIZE))
			fore = corresponding_dim_index[fore];
	}

	/* And standout */
	if (cell->attr.standout) {
		if (back < VTE_LEGACY_COLOR_SET_SIZE)
			back += VTE_COLOR_BRIGHT_OFFSET;
	}

	/* Reverse cell? */
	if (cell->attr.reverse) {
                if (terminal->pvt->reverse_color_set)
                        back = VTE_REV_BG;
                else
                        swap (&fore, &back);
	}

	/* Selection: use hightlight back, or inverse */
	if (selected) {
		/* XXX what if hightlight back is same color as current back? */
		if (terminal->pvt->highlight_color_set)
			back = VTE_DEF_HL;
		else
			swap (&fore, &back);
	}

	/* Cursor: use cursor back, or inverse */
	if (cursor) {
		/* XXX what if cursor back is same color as current back? */
		if (terminal->pvt->cursor_color_set)
			back = VTE_CUR_BG;
		else
			swap (&fore, &back);
	}

	/* Invisible? */
	if (cell && cell->attr.invisible) {
		fore = back;
	}

	*pfore = fore;
	*pback = back;
}

static inline void
vte_view_determine_colors (VteView *terminal,
			       const VteCell *cell,
			       gboolean highlight,
			       guint *fore, guint *back)
{
	return vte_view_determine_colors_internal (terminal, cell,
						       highlight, FALSE,
						       fore, back);
}

static inline void
vte_view_determine_cursor_colors (VteView *terminal,
				      const VteCell *cell,
				      gboolean highlight,
				      guint *fore, guint *back)
{
	return vte_view_determine_colors_internal (terminal, cell,
						       highlight, TRUE,
						       fore, back);
}

/* Check if a unicode character is actually a graphic character we draw
 * ourselves to handle cases where fonts don't have glyphs for them. */
static gboolean
vte_unichar_is_local_graphic(vteunistr c)
{
        /* Box Drawing & Block Elements */
        return (c >= 0x2500) && (c <= 0x259f);
}

static gboolean
vte_view_unichar_is_local_graphic(VteView *terminal, vteunistr c, gboolean bold)
{
        return vte_unichar_is_local_graphic (c);
}

static void
vte_view_fill_rectangle(VteView *terminal,
			    const GdkRGBA *color,
			    gint x,
			    gint y,
			    gint width,
			    gint height)
{
	_vte_draw_fill_rectangle(terminal->pvt->draw,
				 x + terminal->pvt->padding.left,
                                 y + terminal->pvt->padding.top,
				 width, height,
				 color);
}

static void
vte_view_draw_line(VteView *terminal,
		       const GdkRGBA *color,
		       gint x,
		       gint y,
		       gint xp,
		       gint yp)
{
	vte_view_fill_rectangle(terminal, color,
				    x, y,
				    MAX(VTE_LINE_WIDTH, xp - x + 1), MAX(VTE_LINE_WIDTH, yp - y + 1));
}

static void
vte_view_draw_rectangle(VteView *terminal,
			    const GdkRGBA *color,
			    gint x,
			    gint y,
			    gint width,
			    gint height)
{
	_vte_draw_draw_rectangle(terminal->pvt->draw,
				 x + terminal->pvt->padding.left,
                                 y + terminal->pvt->padding.top,
				 width, height,
				 color);
}

/* Draw the graphic representation of a line-drawing or special graphics
 * character. */
static gboolean
vte_view_draw_graphic(VteView *view,
                      vteunistr c,
                      guint fore,
                      guint back,
                      gboolean draw_default_bg,
                      gint x,
                      gint y,
                      gint column_width,
                      gint columns,
                      gint row_height,
                      gboolean bold)
{
        VteViewPrivate *pvt = view->pvt;
        gint width, xcenter, xright, ycenter, ybottom;
        int upper_half, lower_half, left_half, right_half;
        int light_line_width, heavy_line_width;
        double adjust;
        cairo_t *cr = _vte_draw_get_context (pvt->draw);

        width = column_width * columns;

        if ((back != VTE_DEF_BG) || draw_default_bg) {
                vte_view_fill_rectangle(view,
                                        &pvt->palette[back],
                                        x, y, width, row_height);
        }

        cairo_save (cr);

        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        gdk_cairo_set_source_rgba(cr, &pvt->palette[fore]);

        // FIXME wtf!?
        x += pvt->padding.left;
        y += pvt->padding.top;

        upper_half = row_height / 2;
        lower_half = row_height - upper_half;
        left_half = width / 2;
        right_half = width - left_half;

        light_line_width = (pvt->char_width + 4) / 5;
        light_line_width = MAX (light_line_width, 1);

        heavy_line_width = light_line_width + 2;

        xcenter = x + left_half;
        ycenter = y + upper_half;
        xright = x + width;
        ybottom = y + row_height;

        switch (c) {

        /* Box Drawing */
        case 0x2500: /* box drawings light horizontal */
        case 0x2501: /* box drawings heavy horizontal */
        case 0x2502: /* box drawings light vertical */
        case 0x2503: /* box drawings heavy vertical */
        case 0x2504: /* box drawings light triple dash horizontal */
        case 0x2505: /* box drawings heavy triple dash horizontal */
        case 0x2506: /* box drawings light triple dash vertical */
        case 0x2507: /* box drawings heavy triple dash vertical */
        case 0x2508: /* box drawings light quadruple dash horizontal */
        case 0x2509: /* box drawings heavy quadruple dash horizontal */
        case 0x250a: /* box drawings light quadruple dash vertical */
        case 0x250b: /* box drawings heavy quadruple dash vertical */
        case 0x254c: /* box drawings light double dash horizontal */
        case 0x254d: /* box drawings heavy double dash horizontal */
        case 0x254e: /* box drawings light double dash vertical */
        case 0x254f: /* box drawings heavy double dash vertical */
        {
                const guint v = c - 0x2500;
                int size, line_width;

                size = (v & 2) ? row_height : width;

                switch (v >> 2) {
                case 0: /* no dashes */
                        break;
                case 1: /* triple dash */
                {
                        double segment = size / 8.;
                        double dashes[2] = { segment * 2., segment };
                        cairo_set_dash(cr, dashes, G_N_ELEMENTS(dashes), 0.);
                        break;
                }
                case 2: /* quadruple dash */
                {
                        double segment = size / 11.;
                        double dashes[2] = { segment * 2., segment };
                        cairo_set_dash(cr, dashes, G_N_ELEMENTS(dashes), 0.);
                        break;
                }
                case 19: /* double dash */
                {
                        double segment = size / 5.;
                        double dashes[2] = { segment * 2., segment };
                        cairo_set_dash(cr, dashes, G_N_ELEMENTS(dashes), 0.);
                        break;
                }
                }

                line_width = (v & 1) ? heavy_line_width : light_line_width;
                adjust = (line_width & 1) ? .5 : 0.;

                cairo_set_line_width(cr, line_width);
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
                if (v & 2) {
                        cairo_move_to(cr, xcenter + adjust, y);
                        cairo_line_to(cr, xcenter + adjust, y + row_height);
                } else {
                        cairo_move_to(cr, x, ycenter + adjust);
                        cairo_line_to(cr, x + width, ycenter + adjust);
                }
                cairo_stroke(cr);
                break;
        }

        case 0x250c: /* box drawings light down and right */
        case 0x250d: /* box drawings down light and right heavy */
        case 0x250e: /* box drawings down heavy and right light */
        case 0x250f: /* box drawings heavy down and right */
        case 0x2510: /* box drawings light down and left */
        case 0x2511: /* box drawings down light and left heavy */
        case 0x2512: /* box drawings down heavy and left light */
        case 0x2513: /* box drawings heavy down and left */
        case 0x2514: /* box drawings light up and right */
        case 0x2515: /* box drawings up light and right heavy */
        case 0x2516: /* box drawings up heavy and right light */
        case 0x2517: /* box drawings heavy up and right */
        case 0x2518: /* box drawings light up and left */
        case 0x2519: /* box drawings up light and left heavy */
        case 0x251a: /* box drawings up heavy and left light */
        case 0x251b: /* box drawings heavy up and left */
        case 0x251c: /* box drawings light vertical and right */
        case 0x251d: /* box drawings vertical light and right heavy */
        case 0x251e: /* box drawings up heavy and right down light */
        case 0x251f: /* box drawings down heavy and right up light */
        case 0x2520: /* box drawings vertical heavy and right light */
        case 0x2521: /* box drawings down light and right up heavy */
        case 0x2522: /* box drawings up light and right down heavy */
        case 0x2523: /* box drawings heavy vertical and right */
        case 0x2524: /* box drawings light vertical and left */
        case 0x2525: /* box drawings vertical light and left heavy */
        case 0x2526: /* box drawings up heavy and left down light */
        case 0x2527: /* box drawings down heavy and left up light */
        case 0x2528: /* box drawings vertical heavy and left light */
        case 0x2529: /* box drawings down light and left up heavy */
        case 0x252a: /* box drawings up light and left down heavy */
        case 0x252b: /* box drawings heavy vertical and left */
        case 0x252c: /* box drawings light down and horizontal */
        case 0x252d: /* box drawings left heavy and right down light */
        case 0x252e: /* box drawings right heavy and left down light */
        case 0x252f: /* box drawings down light and horizontal heavy */
        case 0x2530: /* box drawings down heavy and horizontal light */
        case 0x2531: /* box drawings right light and left down heavy */
        case 0x2532: /* box drawings left light and right down heavy */
        case 0x2533: /* box drawings heavy down and horizontal */
        case 0x2534: /* box drawings light up and horizontal */
        case 0x2535: /* box drawings left heavy and right up light */
        case 0x2536: /* box drawings right heavy and left up light */
        case 0x2537: /* box drawings up light and horizontal heavy */
        case 0x2538: /* box drawings up heavy and horizontal light */
        case 0x2539: /* box drawings right light and left up heavy */
        case 0x253a: /* box drawings left light and right up heavy */
        case 0x253b: /* box drawings heavy up and horizontal */
        case 0x253c: /* box drawings light vertical and horizontal */
        case 0x253d: /* box drawings left heavy and right vertical light */
        case 0x253e: /* box drawings right heavy and left vertical light */
        case 0x253f: /* box drawings vertical light and horizontal heavy */
        case 0x2540: /* box drawings up heavy and down horizontal light */
        case 0x2541: /* box drawings down heavy and up horizontal light */
        case 0x2542: /* box drawings vertical heavy and horizontal light */
        case 0x2543: /* box drawings left up heavy and right down light */
        case 0x2544: /* box drawings right up heavy and left down light */
        case 0x2545: /* box drawings left down heavy and right up light */
        case 0x2546: /* box drawings right down heavy and left up light */
        case 0x2547: /* box drawings down light and up horizontal heavy */
        case 0x2548: /* box drawings up light and down horizontal heavy */
        case 0x2549: /* box drawings right light and left vertical heavy */
        case 0x254a: /* box drawings left light and right vertical heavy */
        case 0x254b: /* box drawings heavy vertical and horizontal */
        case 0x2574: /* box drawings light left */
        case 0x2575: /* box drawings light up */
        case 0x2576: /* box drawings light right */
        case 0x2577: /* box drawings light down */
        case 0x2578: /* box drawings heavy left */
        case 0x2579: /* box drawings heavy up */
        case 0x257a: /* box drawings heavy right */
        case 0x257b: /* box drawings heavy down */
        case 0x257c: /* box drawings light left and heavy right */
        case 0x257d: /* box drawings light up and heavy down */
        case 0x257e: /* box drawings heavy left and light right */
        case 0x257f: /* box drawings heavy up and light down */
        {
                enum { BOX_LEFT_LIGHT       = 1 << 0,
                       BOX_LEFT_HEAVY       = 1 << 1,
                       BOX_RIGHT_LIGHT      = 1 << 2,
                       BOX_RIGHT_HEAVY      = 1 << 3,
                       BOX_TOP_LIGHT        = 1 << 4,
                       BOX_TOP_HEAVY        = 1 << 5,
                       BOX_BOTTOM_LIGHT     = 1 << 6,
                       BOX_BOTTOM_HEAVY     = 1 << 7,
                       BOX_HORIZONTAL_LIGHT = BOX_LEFT_LIGHT | BOX_RIGHT_LIGHT,
                       BOX_HORIZONTAL_HEAVY = BOX_LEFT_HEAVY | BOX_RIGHT_HEAVY,
                       BOX_VERTICAL_LIGHT   = BOX_TOP_LIGHT  | BOX_BOTTOM_LIGHT,
                       BOX_VERTICAL_HEAVY   = BOX_TOP_HEAVY  | BOX_BOTTOM_HEAVY,
                       BOX_LEFT             = BOX_LEFT_LIGHT | BOX_LEFT_HEAVY,
                       BOX_RIGHT            = BOX_RIGHT_LIGHT | BOX_RIGHT_HEAVY,
                       BOX_TOP              = BOX_TOP_LIGHT | BOX_TOP_HEAVY,
                       BOX_BOTTOM           = BOX_BOTTOM_LIGHT | BOX_BOTTOM_HEAVY,
                       BOX_HORIZONTAL       = BOX_HORIZONTAL_LIGHT | BOX_HORIZONTAL_HEAVY,
                       BOX_VERTICAL         = BOX_VERTICAL_LIGHT | BOX_VERTICAL_HEAVY,
                       BOX_LIGHT            = BOX_HORIZONTAL_LIGHT | BOX_VERTICAL_LIGHT,
                       BOX_HEAVY            = BOX_HORIZONTAL_HEAVY | BOX_VERTICAL_HEAVY
                };
                static const guint8 const map[] = {
                        BOX_BOTTOM_LIGHT | BOX_RIGHT_LIGHT,
                        BOX_BOTTOM_LIGHT | BOX_RIGHT_HEAVY,
                        BOX_BOTTOM_HEAVY | BOX_RIGHT_LIGHT,
                        BOX_BOTTOM_HEAVY | BOX_RIGHT_HEAVY,
                        BOX_BOTTOM_LIGHT | BOX_LEFT_LIGHT,
                        BOX_BOTTOM_LIGHT | BOX_LEFT_HEAVY,
                        BOX_BOTTOM_HEAVY | BOX_LEFT_LIGHT,
                        BOX_BOTTOM_HEAVY | BOX_LEFT_HEAVY,
                        BOX_TOP_LIGHT | BOX_RIGHT_LIGHT,
                        BOX_TOP_LIGHT | BOX_RIGHT_HEAVY,
                        BOX_TOP_HEAVY | BOX_RIGHT_LIGHT,
                        BOX_TOP_HEAVY | BOX_RIGHT_HEAVY,
                        BOX_TOP_LIGHT | BOX_LEFT_LIGHT,
                        BOX_TOP_LIGHT | BOX_LEFT_HEAVY,
                        BOX_TOP_HEAVY | BOX_LEFT_LIGHT,
                        BOX_TOP_HEAVY | BOX_LEFT_HEAVY,
                        BOX_VERTICAL_LIGHT | BOX_RIGHT_LIGHT,
                        BOX_VERTICAL_LIGHT | BOX_RIGHT_HEAVY,
                        BOX_TOP_HEAVY | BOX_RIGHT_LIGHT | BOX_BOTTOM_LIGHT,
                        BOX_BOTTOM_HEAVY | BOX_RIGHT_LIGHT | BOX_TOP_LIGHT,
                        BOX_VERTICAL_HEAVY | BOX_RIGHT_LIGHT,
                        BOX_BOTTOM_LIGHT | BOX_RIGHT_HEAVY | BOX_TOP_HEAVY,
                        BOX_TOP_LIGHT | BOX_RIGHT_HEAVY | BOX_BOTTOM_HEAVY,
                        BOX_VERTICAL_HEAVY | BOX_RIGHT_HEAVY,
                        BOX_VERTICAL_LIGHT | BOX_LEFT_LIGHT,
                        BOX_VERTICAL_LIGHT | BOX_LEFT_HEAVY,
                        BOX_TOP_HEAVY | BOX_LEFT_LIGHT | BOX_BOTTOM_LIGHT,
                        BOX_BOTTOM_HEAVY | BOX_LEFT_LIGHT | BOX_TOP_LIGHT,
                        BOX_VERTICAL_HEAVY | BOX_LEFT_LIGHT,
                        BOX_BOTTOM_LIGHT | BOX_LEFT_HEAVY | BOX_TOP_HEAVY,
                        BOX_TOP_LIGHT | BOX_LEFT_HEAVY | BOX_BOTTOM_HEAVY,
                        BOX_VERTICAL_HEAVY | BOX_LEFT_HEAVY,
                        BOX_BOTTOM_LIGHT | BOX_HORIZONTAL_LIGHT,
                        BOX_LEFT_HEAVY | BOX_RIGHT_LIGHT | BOX_BOTTOM_LIGHT,
                        BOX_RIGHT_HEAVY | BOX_LEFT_LIGHT | BOX_BOTTOM_LIGHT,
                        BOX_BOTTOM_LIGHT | BOX_HORIZONTAL_HEAVY,
                        BOX_BOTTOM_HEAVY | BOX_HORIZONTAL_LIGHT,
                        BOX_RIGHT_LIGHT | BOX_LEFT_HEAVY | BOX_BOTTOM_HEAVY,
                        BOX_LEFT_LIGHT | BOX_RIGHT_HEAVY | BOX_BOTTOM_HEAVY,
                        BOX_BOTTOM_HEAVY| BOX_HORIZONTAL_HEAVY,
                        BOX_TOP_LIGHT | BOX_HORIZONTAL_LIGHT,
                        BOX_LEFT_HEAVY | BOX_RIGHT_LIGHT | BOX_TOP_LIGHT,
                        BOX_RIGHT_HEAVY | BOX_LEFT_LIGHT | BOX_TOP_LIGHT,
                        BOX_TOP_LIGHT | BOX_HORIZONTAL_HEAVY,
                        BOX_TOP_HEAVY | BOX_HORIZONTAL_LIGHT,
                        BOX_RIGHT_LIGHT | BOX_LEFT_HEAVY | BOX_TOP_HEAVY,
                        BOX_LEFT_LIGHT | BOX_RIGHT_HEAVY | BOX_TOP_HEAVY,
                        BOX_TOP_HEAVY | BOX_HORIZONTAL_HEAVY,
                        BOX_VERTICAL_LIGHT | BOX_HORIZONTAL_LIGHT,
                        BOX_LEFT_HEAVY | BOX_RIGHT_LIGHT | BOX_VERTICAL_LIGHT,
                        BOX_RIGHT_HEAVY | BOX_LEFT_LIGHT | BOX_VERTICAL_LIGHT,
                        BOX_VERTICAL_LIGHT | BOX_HORIZONTAL_HEAVY,
                        BOX_TOP_HEAVY | BOX_BOTTOM_LIGHT | BOX_HORIZONTAL_LIGHT,
                        BOX_BOTTOM_HEAVY| BOX_TOP_LIGHT | BOX_HORIZONTAL_LIGHT,
                        BOX_VERTICAL_HEAVY | BOX_HORIZONTAL_LIGHT,
                        BOX_LEFT_HEAVY | BOX_RIGHT_LIGHT | BOX_TOP_HEAVY | BOX_BOTTOM_LIGHT,
                        BOX_RIGHT_HEAVY | BOX_TOP_HEAVY | BOX_LEFT_LIGHT | BOX_BOTTOM_LIGHT,
                        BOX_LEFT_HEAVY | BOX_BOTTOM_HEAVY | BOX_RIGHT_LIGHT | BOX_TOP_LIGHT,
                        BOX_RIGHT_HEAVY | BOX_BOTTOM_HEAVY | BOX_LEFT_LIGHT | BOX_TOP_LIGHT,
                        BOX_BOTTOM_LIGHT | BOX_TOP_HEAVY | BOX_HORIZONTAL_HEAVY,
                        BOX_TOP_LIGHT | BOX_BOTTOM_HEAVY | BOX_HORIZONTAL_HEAVY,
                        BOX_RIGHT_LIGHT | BOX_LEFT_HEAVY | BOX_VERTICAL_HEAVY,
                        BOX_LEFT_LIGHT | BOX_RIGHT_HEAVY | BOX_VERTICAL_HEAVY,
                        BOX_VERTICAL_HEAVY | BOX_HORIZONTAL_HEAVY,

                        /* U+254C - U+2573 are handled elsewhere */
                        0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        0, 0, 0, 0,

                        BOX_LEFT_LIGHT,
                        BOX_TOP_LIGHT,
                        BOX_RIGHT_LIGHT,
                        BOX_BOTTOM_LIGHT,
                        BOX_LEFT_HEAVY,
                        BOX_TOP_HEAVY,
                        BOX_RIGHT_HEAVY,
                        BOX_BOTTOM_HEAVY,
                        BOX_LEFT_LIGHT | BOX_RIGHT_HEAVY,
                        BOX_TOP_LIGHT | BOX_BOTTOM_HEAVY,
                        BOX_LEFT_HEAVY | BOX_RIGHT_LIGHT,
                        BOX_TOP_HEAVY | BOX_BOTTOM_LIGHT
                };
                G_STATIC_ASSERT(G_N_ELEMENTS(map) == (0x257f - 0x250c + 1));
                const guint v = c - 0x250c;
                const guint8 m = map[v];
                int line_width;

                cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

                if (m & BOX_LEFT) {
                        line_width = (m & BOX_LEFT_HEAVY) ? heavy_line_width : light_line_width;
                        adjust = (line_width & 1) ? .5 : 0.;
                        cairo_set_line_width(cr, line_width);
                        cairo_move_to(cr, x, ycenter + adjust);
                        cairo_line_to(cr, xcenter, ycenter + adjust);
                        cairo_stroke(cr);
                }
                if (m & BOX_RIGHT) {
                        line_width = (m & BOX_RIGHT_HEAVY) ? heavy_line_width : light_line_width;
                        adjust = (line_width & 1) ? .5 : 0.;
                        cairo_set_line_width(cr, line_width);
                        cairo_move_to(cr, xcenter, ycenter + adjust);
                        cairo_line_to(cr, xright, ycenter + adjust);
                        cairo_stroke(cr);
                }
                if (m & BOX_TOP) {
                        line_width = (m & BOX_TOP_HEAVY) ? heavy_line_width : light_line_width;
                        adjust = (line_width & 1) ? .5 : 0.;
                        cairo_set_line_width(cr, line_width);
                        cairo_move_to(cr, xcenter + adjust, y);
                        cairo_line_to(cr, xcenter + adjust, ycenter);
                        cairo_stroke(cr);
                }
                if (m & BOX_BOTTOM) {
                        line_width = (m & BOX_BOTTOM_HEAVY) ? heavy_line_width : light_line_width;
                        adjust = (line_width & 1) ? .5 : 0.;
                        cairo_set_line_width(cr, line_width);
                        cairo_move_to(cr, xcenter + adjust, ycenter);
                        cairo_line_to(cr, xcenter + adjust, ybottom);
                        cairo_stroke(cr);
                }

                /* Make the join not look jagged */
                if ((m & BOX_HORIZONTAL) && (m & BOX_VERTICAL)) {
                        int xs, ys, w, h;

                        if (m & BOX_HORIZONTAL_HEAVY) {
                                ys = ycenter - heavy_line_width / 2;
                                h = heavy_line_width;
                        } else {
                                ys = ycenter - light_line_width / 2;
                                h = light_line_width;
                        }
                        if (m & BOX_VERTICAL_HEAVY) {
                                xs = xcenter - heavy_line_width / 2;
                                w = heavy_line_width;
                        } else {
                                xs = xcenter - light_line_width / 2;
                                w = light_line_width;
                        }
                        cairo_rectangle(cr, xs, ys, w, h);
                        cairo_fill(cr);
                }

                break;
        }

        case 0x2550: /* box drawings double horizontal */
        case 0x2551: /* box drawings double vertical */
        case 0x2552: /* box drawings down single and right double */
        case 0x2553: /* box drawings down double and right single */
        case 0x2554: /* box drawings double down and right */
        case 0x2555: /* box drawings down single and left double */
        case 0x2556: /* box drawings down double and left single */
        case 0x2557: /* box drawings double down and left */
        case 0x2558: /* box drawings up single and right double */
        case 0x2559: /* box drawings up double and right single */
        case 0x255a: /* box drawings double up and right */
        case 0x255b: /* box drawings up single and left double */
        case 0x255c: /* box drawings up double and left single */
        case 0x255d: /* box drawings double up and left */
        case 0x255e: /* box drawings vertical single and right double */
        case 0x255f: /* box drawings vertical double and right single */
        case 0x2560: /* box drawings double vertical and right */
        case 0x2561: /* box drawings vertical single and left double */
        case 0x2562: /* box drawings vertical double and left single */
        case 0x2563: /* box drawings double vertical and left */
        case 0x2564: /* box drawings down single and horizontal double */
        case 0x2565: /* box drawings down double and horizontal single */
        case 0x2566: /* box drawings double down and horizontal */
        case 0x2567: /* box drawings up single and horizontal double */
        case 0x2568: /* box drawings up double and horizontal single */
        case 0x2569: /* box drawings double up and horizontal */
        case 0x256a: /* box drawings vertical single and horizontal double */
        case 0x256b: /* box drawings vertical double and horizontal single */
        case 0x256c: /* box drawings double vertical and horizontal */
        {
                enum { BOX_LEFT_SINGLE       = 1 << 0,
                       BOX_LEFT_DOUBLE       = 1 << 1,
                       BOX_RIGHT_SINGLE      = 1 << 2,
                       BOX_RIGHT_DOUBLE      = 1 << 3,
                       BOX_TOP_SINGLE        = 1 << 4,
                       BOX_TOP_DOUBLE        = 1 << 5,
                       BOX_BOTTOM_SINGLE     = 1 << 6,
                       BOX_BOTTOM_DOUBLE     = 1 << 7,
                       BOX_LEFT              = BOX_LEFT_SINGLE | BOX_LEFT_DOUBLE,
                       BOX_RIGHT             = BOX_RIGHT_SINGLE | BOX_RIGHT_DOUBLE,
                       BOX_TOP               = BOX_TOP_SINGLE | BOX_TOP_DOUBLE,
                       BOX_BOTTOM            = BOX_BOTTOM_SINGLE | BOX_BOTTOM_DOUBLE,
                       BOX_SINGLE            = BOX_LEFT_SINGLE | BOX_RIGHT_SINGLE | BOX_TOP_SINGLE | BOX_BOTTOM_SINGLE,
                       BOX_DOUBLE            = BOX_LEFT_DOUBLE | BOX_RIGHT_DOUBLE | BOX_TOP_DOUBLE | BOX_BOTTOM_DOUBLE,
                       BOX_HORIZONTAL_SINGLE = BOX_LEFT_SINGLE | BOX_RIGHT_SINGLE,
                       BOX_HORIZONTAL_DOUBLE = BOX_LEFT_DOUBLE | BOX_RIGHT_DOUBLE,
                       BOX_VERTICAL_SINGLE   = BOX_TOP_SINGLE  | BOX_BOTTOM_SINGLE,
                       BOX_VERTICAL_DOUBLE   = BOX_TOP_DOUBLE  | BOX_BOTTOM_DOUBLE
                };
                static const guint8 const map[] = {
                        BOX_HORIZONTAL_DOUBLE,
                        BOX_VERTICAL_DOUBLE,
                        BOX_BOTTOM_SINGLE | BOX_RIGHT_DOUBLE,
                        BOX_BOTTOM_DOUBLE | BOX_RIGHT_SINGLE,
                        BOX_BOTTOM_DOUBLE | BOX_RIGHT_DOUBLE,
                        BOX_BOTTOM_SINGLE | BOX_LEFT_DOUBLE,
                        BOX_BOTTOM_DOUBLE | BOX_LEFT_SINGLE,
                        BOX_BOTTOM_DOUBLE | BOX_LEFT_DOUBLE,
                        BOX_TOP_SINGLE | BOX_RIGHT_DOUBLE,
                        BOX_TOP_DOUBLE | BOX_RIGHT_SINGLE,
                        BOX_TOP_DOUBLE | BOX_RIGHT_DOUBLE,
                        BOX_TOP_SINGLE | BOX_LEFT_DOUBLE,
                        BOX_TOP_DOUBLE | BOX_LEFT_SINGLE,
                        BOX_TOP_DOUBLE | BOX_LEFT_DOUBLE,
                        BOX_VERTICAL_SINGLE | BOX_RIGHT_DOUBLE,
                        BOX_VERTICAL_DOUBLE | BOX_RIGHT_SINGLE,
                        BOX_VERTICAL_DOUBLE | BOX_RIGHT_DOUBLE,
                        BOX_VERTICAL_SINGLE | BOX_LEFT_DOUBLE,
                        BOX_VERTICAL_DOUBLE | BOX_LEFT_SINGLE,
                        BOX_VERTICAL_DOUBLE | BOX_LEFT_DOUBLE,
                        BOX_BOTTOM_SINGLE | BOX_HORIZONTAL_DOUBLE,
                        BOX_BOTTOM_DOUBLE | BOX_HORIZONTAL_SINGLE,
                        BOX_BOTTOM_DOUBLE | BOX_HORIZONTAL_DOUBLE,
                        BOX_TOP_SINGLE | BOX_HORIZONTAL_DOUBLE,
                        BOX_TOP_DOUBLE | BOX_HORIZONTAL_SINGLE,
                        BOX_TOP_DOUBLE | BOX_HORIZONTAL_DOUBLE,
                        BOX_VERTICAL_SINGLE | BOX_HORIZONTAL_DOUBLE,
                        BOX_VERTICAL_DOUBLE | BOX_HORIZONTAL_SINGLE,
                        BOX_VERTICAL_DOUBLE | BOX_HORIZONTAL_DOUBLE
                };
                G_STATIC_ASSERT(G_N_ELEMENTS(map) == (0x256c - 0x2550 + 1));
                const guint v = c - 0x2550;
                const guint8 m = map[v];
                int line_width;
                int double_line_width, half_double_line_width, half_double_line_width_plus_1;
                int inner_line_width;

                cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

                double_line_width = MAX (heavy_line_width, 3);
                half_double_line_width = double_line_width / 2;
                half_double_line_width_plus_1 = (double_line_width + 1) / 2;
                inner_line_width = double_line_width / 3;
                adjust = (inner_line_width & 1) ? .5 : 0.;

                if (m & BOX_LEFT) {
                        line_width = (m & BOX_LEFT_DOUBLE) ? double_line_width: light_line_width;
                        adjust = (line_width & 1) ? .5 : 0.;
                        cairo_set_line_width(cr, line_width);
                        cairo_move_to(cr, x, ycenter + adjust);
                        cairo_line_to(cr,
                                      (m & BOX_VERTICAL_DOUBLE) ? xcenter + half_double_line_width_plus_1: xcenter,
                                      ycenter + adjust);
                        cairo_stroke(cr);
                }
                if (m & BOX_RIGHT) {
                        line_width = (m & BOX_RIGHT_DOUBLE) ? double_line_width: light_line_width;
                        adjust = (line_width & 1) ? .5 : 0.;
                        cairo_set_line_width(cr, line_width);
                        cairo_move_to(cr,
                                      (m & BOX_VERTICAL_DOUBLE) ? xcenter - half_double_line_width: xcenter,
                                      ycenter + adjust);
                        cairo_line_to(cr, xright, ycenter + adjust);
                        cairo_stroke(cr);
                }
                if (m & BOX_TOP) {
                        line_width = (m & BOX_TOP_DOUBLE) ? double_line_width: light_line_width;
                        adjust = (line_width & 1) ? .5 : 0.;
                        cairo_set_line_width(cr, line_width);
                        cairo_move_to(cr, xcenter + adjust, y);
                        cairo_line_to(cr,
                                      xcenter + adjust,
                                      (m & BOX_HORIZONTAL_DOUBLE) ? ycenter + half_double_line_width_plus_1 : ycenter);
                        cairo_stroke(cr);
                }
                if (m & BOX_BOTTOM) {
                        line_width = (m & BOX_BOTTOM_DOUBLE) ? double_line_width: light_line_width;
                        adjust = (line_width & 1) ? .5 : 0.;
                        cairo_set_line_width(cr, line_width);
                        cairo_move_to(cr,
                                      xcenter + adjust,
                                      (m & BOX_HORIZONTAL_DOUBLE) ? ycenter - half_double_line_width : ycenter);
                        cairo_line_to(cr, xcenter + adjust, ybottom);
                        cairo_stroke(cr);
                }

                /* Now take the inside out */
                gdk_cairo_set_source_rgba(cr, &pvt->palette[back]);
                cairo_set_line_width(cr, inner_line_width);
                cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);

                if (m & BOX_VERTICAL_DOUBLE) {
                        if (m & BOX_TOP) {
                                cairo_move_to(cr, xcenter + adjust, y);
                                cairo_line_to(cr, xcenter + adjust, ycenter);
                        } else {
                                cairo_move_to(cr, xcenter + adjust, ycenter);
                        }
                        if (m & BOX_BOTTOM) {
                                cairo_line_to(cr, xcenter + adjust, ybottom);
                        }
                        cairo_stroke(cr);
                }
                if (m & BOX_HORIZONTAL_DOUBLE) {
                        if (m & BOX_LEFT) {
                                cairo_move_to(cr, x, ycenter + adjust);
                                cairo_line_to(cr, xcenter, ycenter + adjust);
                        } else {
                                cairo_move_to(cr, xcenter, ycenter + adjust);
                        }
                        if (m & BOX_RIGHT) {
                                cairo_line_to(cr, xright, ycenter + adjust);
                        }
                        cairo_stroke(cr);
                }

                break;
        }

        case 0x256d: /* box drawings light arc down and right */
        case 0x256e: /* box drawings light arc down and left */
        case 0x256f: /* box drawings light arc up and left */
        case 0x2570: /* box drawings light arc up and right */
        {
                const guint v = c - 0x256d;
                int line_width;

                cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

                line_width = light_line_width;
                adjust = (line_width & 1) ? .5 : 0.;
                cairo_set_line_width(cr, line_width);

                cairo_move_to(cr, xcenter + adjust, (v & 2) ? y : ybottom);
                cairo_curve_to(cr, 
                               xcenter + adjust, ycenter + adjust,
                               xcenter + adjust, ycenter + adjust,
                               (v == 1 || v == 2) ? x : xright, ycenter + adjust);
                cairo_stroke(cr);

                break;
        }

        case 0x2571: /* box drawings light diagonal upper right to lower left */
        case 0x2572: /* box drawings light diagonal upper left to lower right */
        case 0x2573: /* box drawings light diagonal cross */
        {
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
                cairo_set_line_width(cr, light_line_width);
                adjust = light_line_width / 2.;
                if (c != 0x2571) {
                        cairo_move_to(cr, x + adjust, y + adjust);
                        cairo_line_to(cr, xright - adjust, ybottom - adjust);
                        cairo_stroke(cr);
                }
                if (c != 0x2572) {
                        cairo_move_to(cr, xright - adjust, y + adjust);
                        cairo_line_to(cr, x + adjust, ybottom - adjust);
                        cairo_stroke(cr);
                }
                break;
        }

        /* Block Elements */
        case 0x2580: /* upper half block */
                cairo_rectangle(cr, x, y, width, upper_half);
                cairo_fill (cr);
                break;

        case 0x2581: /* lower one eighth block */
        case 0x2582: /* lower one quarter block */
        case 0x2583: /* lower three eighths block */
        case 0x2584: /* lower half block */
        case 0x2585: /* lower five eighths block */
        case 0x2586: /* lower three quarters block */
        case 0x2587: /* lower seven eighths block */
        {
                const guint v = c - 0x2580;
                int h, half;

                if (v & 4) {
                        half = upper_half;
                        h = lower_half;
                } else {
                        half = lower_half;
                        h = 0;
                }

                half /= 2;
                if (v & 2) h += half;
                half /= 2;
                if (v & 1) h += half;

                cairo_rectangle(cr, x, y + row_height - h, width, h);
                cairo_fill (cr);
                break;
        }

        case 0x2588: /* full block */
        case 0x2589: /* left seven eighths block */
        case 0x258a: /* left three quarters block */
        case 0x258b: /* left five eighths block */
        case 0x258c: /* left half block */
        case 0x258d: /* left three eighths block */
        case 0x258e: /* left one quarter block */
        case 0x258f: /* left one eighth block */
        {
                const guint v = c - 0x2588;
                int w, half;

                if (v & 4) {
                        w = half = left_half;
                } else {
                        w = width;
                        half = right_half;
                }

                half /= 2;
                if (v & 2) w -= half;
                half /= 2;
                if (v & 1) w -= half;

                cairo_rectangle(cr, x, y, w, row_height);
                cairo_fill (cr);
                break;
        }

        case 0x2590: /* right half block */
                cairo_rectangle(cr, x + left_half, y, right_half, row_height);
                cairo_fill (cr);
                break;

        case 0x2591: /* light shade */
        case 0x2592: /* medium shade */
        case 0x2593: /* dark shade */
                cairo_set_source_rgba (cr,
                                       pvt->palette[fore].red,
                                       pvt->palette[fore].green,
                                       pvt->palette[fore].blue,
                                       (c - 0x2590) / 4.);
                cairo_rectangle(cr, x, y, width, row_height);
                cairo_fill (cr);
                break;

        case 0x2594: /* upper one eighth block */
                cairo_rectangle(cr, x, y, width, upper_half / 4);
                cairo_fill (cr);
                break;

        case 0x2595: /* right one eighth block */
                cairo_rectangle(cr, x + width - right_half / 4, y, right_half / 4, row_height);
                cairo_fill (cr);
                break;

        case 0x2596: /* quadrant lower left */
                cairo_rectangle(cr, x, y + upper_half, left_half, lower_half);
                cairo_fill (cr);
                break;

        case 0x2597: /* quadrant lower right */
                cairo_rectangle(cr, x + left_half, y + upper_half, right_half, lower_half);
                cairo_fill (cr);
                break;

        case 0x2598: /* quadrant upper left */
                cairo_rectangle(cr, x, y, left_half, upper_half);
                cairo_fill (cr);
                break;

        case 0x2599: /* quadrant upper left and lower left and lower right */
                cairo_rectangle(cr, x, y, left_half, upper_half);
                cairo_rectangle(cr, x, y + upper_half, left_half, lower_half);
                cairo_rectangle(cr, x + left_half, y + upper_half, right_half, lower_half);
                cairo_fill (cr);
                break;

        case 0x259a: /* quadrant upper left and lower right */
                cairo_rectangle(cr, x, y, left_half, upper_half);
                cairo_rectangle(cr, x + left_half, y + upper_half, right_half, lower_half);
                cairo_fill (cr);
                break;

        case 0x259b: /* quadrant upper left and upper right and lower left */
                cairo_rectangle(cr, x, y, left_half, upper_half);
                cairo_rectangle(cr, x + left_half, y, right_half, upper_half);
                cairo_rectangle(cr, x, y + upper_half, left_half, lower_half);
                cairo_fill (cr);
                break;

        case 0x259c: /* quadrant upper left and upper right and lower right */
                cairo_rectangle(cr, x, y, left_half, upper_half);
                cairo_rectangle(cr, x + left_half, y, right_half, upper_half);
                cairo_rectangle(cr, x + left_half, y + upper_half, right_half, lower_half);
                cairo_fill (cr);
                break;

        case 0x259d: /* quadrant upper right */
                cairo_rectangle(cr, x + left_half, y, right_half, upper_half);
                cairo_fill (cr);
                break;

        case 0x259e: /* quadrant upper right and lower left */
                cairo_rectangle(cr, x + left_half, y, right_half, upper_half);
                cairo_rectangle(cr, x, y + upper_half, left_half, lower_half);
                cairo_fill (cr);
                break;

        case 0x259f: /* quadrant upper right and lower left and lower right */
                cairo_rectangle(cr, x + left_half, y, right_half, upper_half);
                cairo_rectangle(cr, x, y + upper_half, left_half, lower_half);
                cairo_rectangle(cr, x + left_half, y + upper_half, right_half, lower_half);
                cairo_fill (cr);
                break;

        default:
                g_assert_not_reached();
        }

        cairo_restore(cr);

        return TRUE;
}

/* Draw a string of characters with similar attributes. */
static void
vte_view_draw_cells(VteView *terminal,
			struct _vte_draw_text_request *items, gssize n,
			guint fore, guint back, gboolean clear,
			gboolean draw_default_bg,
			gboolean bold, gboolean underline,
			gboolean strikethrough, gboolean hilite, gboolean boxed,
			gint column_width, gint row_height)
{
	int i, x, y;
	gint columns = 0;
	const GdkRGBA *fg, *bg, *defbg;

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
	fg = &terminal->pvt->palette[fore];
	bg = &terminal->pvt->palette[back];
	defbg = &terminal->pvt->palette[VTE_DEF_BG];

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
		if (clear && (draw_default_bg || bg != defbg)) {
			_vte_draw_fill_rectangle(terminal->pvt->draw,
					x + terminal->pvt->padding.left,
                                        y + terminal->pvt->padding.top,
					columns * column_width + bold,
					row_height,
					bg);
		}
	} while (i < n);
	_vte_draw_text(terminal->pvt->draw,
			items, n,
			fg, bold);
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
				vte_view_draw_line(terminal,
						&terminal->pvt->palette[fore],
						x,
						y + terminal->pvt->underline_position,
						x + (columns * column_width) - 1,
						y + terminal->pvt->underline_position + terminal->pvt->line_thickness - 1);
			}
			if (strikethrough) {
				vte_view_draw_line(terminal,
						&terminal->pvt->palette[fore],
						x,
						y + terminal->pvt->strikethrough_position,
						x + (columns * column_width) - 1,
						y + terminal->pvt->strikethrough_position + terminal->pvt->line_thickness - 1);
			}
			if (hilite) {
				vte_view_draw_line(terminal,
						&terminal->pvt->palette[fore],
						x,
						y + row_height - 1,
						x + (columns * column_width) - 1,
						y + row_height - 1);
			}
			if (boxed) {
				vte_view_draw_rectangle(terminal,
						&terminal->pvt->palette[fore],
						x, y,
						MAX(0, (columns * column_width)),
						MAX(0, row_height));
			}
		}while (i < n);
	}
}

/* Try to map a GdkRGBA to a palette entry and return its index. */
static guint
_vte_view_map_pango_color(VteView *terminal, const PangoColor *pcolor)
{
	double distance[G_N_ELEMENTS(terminal->pvt->palette)];
	guint i, ret;
        GdkRGBA color;

        color.red = pcolor->red / 65535.;
        color.green = pcolor->green / 65535.;
        color.blue = pcolor->blue / 65535.;
        color.alpha = 1.0;

        /* Calculate a "distance" value.  Could stand to be improved a bit. */
	for (i = 0; i < G_N_ELEMENTS(distance); i++) {
                const GdkRGBA *entry = &terminal->pvt->palette[i];
                GdkRGBA d;

                d.red = entry->red - color.red;
                d.green = entry->green - color.green;
                d.blue = entry->blue - color.blue;
                d.alpha = entry->alpha - color.alpha;

                distance[i] = (d.red * d.red) +
                              (d.green * d.green) +
                              (d.blue * d.blue) +
                              (d.alpha * d.alpha);
	}

	/* Find the index of the minimum value. */
	ret = 0;
	for (i = 1; i < G_N_ELEMENTS(distance); i++) {
		if (distance[i] < distance[ret]) {
			ret = i;
		}
	}

	_vte_debug_print(VTE_DEBUG_UPDATES,
			"mapped rgba(%.3f,%.3f,%.3f,%.3f) to "
                        "palette entry rgba(%.3f,%.3f,%.3f,%.3f)\n",
			color.red, color.green, color.blue, color.alpha,
			terminal->pvt->palette[ret].red,
			terminal->pvt->palette[ret].green,
                        terminal->pvt->palette[ret].blue,
                        terminal->pvt->palette[ret].alpha);

	return ret;
}

/* FIXME: we don't have a way to tell GTK+ what the default text attributes
 * should be, so for now at least it's assuming white-on-black is the norm and
 * is using "black-on-white" to signify "inverse".  Pick up on that state and
 * fix things.  Do this here, so that if we suddenly get red-on-black, we'll do
 * the right thing. */
static void
_vte_view_fudge_pango_colors(VteView *terminal, GSList *attributes,
				 VteCell *cells, gssize n)
{
        VteBuffer *buffer = terminal->pvt->buffer;
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
		PangoAttribute *attr = attributes->data;
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
			cells[i].attr.fore = buffer->pvt->screen->color_defaults.attr.fore;
			cells[i].attr.back = buffer->pvt->screen->color_defaults.attr.back;
			cells[i].attr.reverse = TRUE;
		}
	}
}

/* Apply the attribute given in the PangoAttribute to the list of cells. */
static void
_vte_view_apply_pango_attr(VteView *terminal, PangoAttribute *attr,
			       VteCell *cells, guint n_cells)
{
	guint i, ival;
	PangoAttrInt *attrint;
	PangoAttrColor *attrcolor;

	switch (attr->klass->type) {
	case PANGO_ATTR_FOREGROUND:
	case PANGO_ATTR_BACKGROUND:
		attrcolor = (PangoAttrColor*) attr;
		ival = _vte_view_map_pango_color(terminal,
						     &attrcolor->color);
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
_vte_view_pango_attribute_destroy(gpointer attr, gpointer data)
{
	pango_attribute_destroy(attr);
}
static void
_vte_view_translate_pango_cells(VteView *terminal, PangoAttrList *attrs,
				    VteCell *cells, guint n_cells)
{
        VteBuffer *buffer = terminal->pvt->buffer;
	PangoAttribute *attr;
	PangoAttrIterator *attriter;
	GSList *list, *listiter;
	guint i;

	for (i = 0; i < n_cells; i++) {
		cells[i] = buffer->pvt->screen->fill_defaults;
	}

	attriter = pango_attr_list_get_iterator(attrs);
	if (attriter != NULL) {
		do {
			list = pango_attr_iterator_get_attrs(attriter);
			if (list != NULL) {
				for (listiter = list;
				     listiter != NULL;
				     listiter = g_slist_next(listiter)) {
					attr = listiter->data;
					_vte_view_apply_pango_attr(terminal,
								       attr,
								       cells,
								       n_cells);
				}
				attr = list->data;
				_vte_view_fudge_pango_colors(terminal,
								 list,
								 cells +
								 attr->start_index,
								 attr->end_index -
								 attr->start_index);
				g_slist_foreach(list,
						_vte_view_pango_attribute_destroy,
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
vte_view_draw_cells_with_attributes(VteView *terminal,
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
	_vte_view_translate_pango_cells(terminal, attrs, cells, cell_count);
	for (i = 0, j = 0; i < n; i++) {
		vte_view_determine_colors(terminal, &cells[j], FALSE, &fore, &back);
		vte_view_draw_cells(terminal, items + i, 1,
					fore,
					back,
					TRUE, draw_default_bg,
					cells[j].attr.bold,
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
vte_view_draw_rows(VteView *terminal,
		      VteScreen *screen,
		      gint start_row, gint row_count,
		      gint start_column, gint column_count,
		      gint start_x, gint start_y,
		      gint column_width, gint row_height)
{
	struct _vte_draw_text_request items[4*VTE_DRAW_MAX_LENGTH];
	gint i, j, row, rows, x, y, end_column;
	guint fore, nfore, back, nback;
	gboolean underline, nunderline, bold, nbold, hilite, nhilite,
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
		row_data = _vte_screen_find_row_data(screen, row);
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
				selected = vte_view_cell_is_selected(terminal->pvt->buffer, i, row, terminal);
				vte_view_determine_colors(terminal, cell, selected, &fore, &back);

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
					selected = vte_view_cell_is_selected(terminal->pvt->buffer, j, row, terminal);
					vte_view_determine_colors(terminal, cell, selected, &nfore, &nback);
					if (nback != back) {
						break;
					}
					bold = cell && cell->attr.bold;
					j += cell ? cell->attr.columns : 1;
				}
				if (back != VTE_DEF_BG) {
					_vte_draw_fill_rectangle (
							terminal->pvt->draw,
							x + i * column_width,
							y,
							(j - i) * column_width + bold,
							row_height,
							&terminal->pvt->palette[back]);
				}
				/* We'll need to continue at the first cell which didn't
				 * match the first one in this set. */
				i = j;
			} while (i < end_column);
		} else {
			do {
				selected = vte_view_cell_is_selected(terminal->pvt->buffer, i, row, terminal);
				j = i + 1;
				while (j < end_column){
					nselected = vte_view_cell_is_selected(terminal->pvt->buffer, j, row, terminal);
					if (nselected != selected) {
						break;
					}
					j++;
				}
				vte_view_determine_colors(terminal, NULL, selected, &fore, &back);
				if (back != VTE_DEF_BG) {
					_vte_draw_fill_rectangle (terminal->pvt->draw,
								  x + i *column_width,
								  y,
								  (j - i)  * column_width,
								  row_height,
								  &terminal->pvt->palette[back]);
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
		row_data = _vte_screen_find_row_data(screen, row);
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
			selected = vte_view_cell_is_selected(terminal->pvt->buffer, i, row, terminal);
			vte_view_determine_colors(terminal, cell, selected, &fore, &back);
			underline = cell->attr.underline;
			strikethrough = cell->attr.strikethrough;
			bold = cell->attr.bold;
			if (terminal->pvt->show_match) {
				hilite = vte_cell_is_between(i, row,
						terminal->pvt->match_start.col,
						terminal->pvt->match_start.row,
						terminal->pvt->match_end.col,
						terminal->pvt->match_end.row,
						TRUE);
			} else {
				hilite = FALSE;
			}

			items[0].c = cell->c;
			items[0].columns = cell->attr.columns;
			items[0].x = start_x + i * column_width;
			items[0].y = y;
			j = i + items[0].columns;

			/* If this is a graphics character, draw it locally. */
			if (vte_view_unichar_is_local_graphic(terminal, cell->c, cell->attr.bold)) {
				if (vte_view_draw_graphic(terminal,
							items[0].c,
							fore, back,
							FALSE,
							items[0].x,
							items[0].y,
							column_width,
							items[0].columns,
							row_height,
							cell->attr.bold)) {
					i = j;
					continue;
				}
			}

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
					selected = vte_view_cell_is_selected(terminal->pvt->buffer, j, row, terminal);
					vte_view_determine_colors(terminal, cell, selected, &nfore, &nback);
					/* Graphic characters must be drawn individually. */
					if (vte_view_unichar_is_local_graphic(terminal, cell->c, cell->attr.bold)) {
						if (vte_view_draw_graphic(terminal,
									cell->c,
									nfore, nback,
									FALSE,
									start_x + j * column_width,
									y,
									column_width,
									cell->attr.columns,
									row_height,
									cell->attr.bold)) {

							j += cell->attr.columns;
							continue;
						}
					}
					if (nfore != fore) {
						break;
					}
					nbold = cell->attr.bold;
					if (nbold != bold) {
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
								terminal->pvt->match_end.row,
								TRUE);
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
						row_data = _vte_screen_find_row_data(screen, row);
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
			vte_view_draw_cells(terminal,
					items,
					item_count,
					fore, back, FALSE, FALSE,
					bold, underline,
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
vte_view_expand_region (VteView *terminal, cairo_region_t *region, const cairo_rectangle_int_t *area)
{
        VteBuffer *buffer;
	int width, height;
	int row, col, row_stop, col_stop;
	cairo_rectangle_int_t rect;

        buffer = terminal->pvt->buffer;

	width = terminal->pvt->char_width;
	height = terminal->pvt->char_height;

	/* increase the paint by one pixel on all sides to force the
	 * inclusion of neighbouring cells */
	row = MAX(0, (area->y - terminal->pvt->padding.top - 1) / height);
	row_stop = MIN(howmany(area->height + area->y - terminal->pvt->padding.top + 1, height),
		       buffer->pvt->row_count);
	if (row_stop <= row) {
		return;
	}
	col = MAX(0, (area->x - terminal->pvt->padding.left - 1) / width);
	col_stop = MIN(howmany(area->width + area->x - terminal->pvt->padding.left + 1, width),
		       buffer->pvt->column_count);
	if (col_stop <= col) {
		return;
	}

	rect.x = col*width + terminal->pvt->padding.left;
	rect.width = (col_stop - col) * width;

	rect.y = row*height + terminal->pvt->padding.top;
	rect.height = (row_stop - row)*height;

	/* the rect must be cell aligned to avoid overlapping XY bands */
	cairo_region_union_rectangle(region, &rect);

	_vte_debug_print (VTE_DEBUG_UPDATES,
			"vte_view_expand_region"
			"	(%d,%d)x(%d,%d) pixels,"
			" (%d,%d)x(%d,%d) cells"
			" [(%d,%d)x(%d,%d) pixels]\n",
			area->x, area->y, area->width, area->height,
			col, row, col_stop - col, row_stop - row,
			rect.x, rect.y, rect.width, rect.height);
}

static void
vte_view_paint_area (VteView *terminal, const cairo_rectangle_int_t *area)
{
        VteBuffer *buffer;
	VteScreen *screen;
	int width, height, delta;
	int row, col, row_stop, col_stop;

        buffer = terminal->pvt->buffer;
        g_assert(buffer != NULL);

	screen = buffer->pvt->screen;

	width = terminal->pvt->char_width;
	height = terminal->pvt->char_height;

	row = MAX(0, (area->y - terminal->pvt->padding.top) / height);
	row_stop = MIN((area->height + area->y - terminal->pvt->padding.top) / height,
		       buffer->pvt->row_count);
	if (row_stop <= row) {
		return;
	}
	col = MAX(0, (area->x - terminal->pvt->padding.left) / width);
	col_stop = MIN((area->width + area->x - terminal->pvt->padding.left) / width,
		       buffer->pvt->column_count);
	if (col_stop <= col) {
		return;
	}
	_vte_debug_print (VTE_DEBUG_UPDATES,
			"vte_view_paint_area"
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
	delta = screen->scroll_delta;
	vte_view_draw_rows(terminal,
			      screen,
			      row + delta, row_stop - row,
			      col, col_stop - col,
			      col * width,
			      row * height,
			      width,
			      height);
}

static void
vte_view_paint_cursor(VteView *terminal)
{
        VteBuffer *buffer;
	VteScreen *screen;
	const VteCell *cell;
	struct _vte_draw_text_request item;
	int row, drow, col;
	long width, height, delta, cursor_width;
	guint fore, back;
	int x, y;
	gboolean blink, selected, focus;

        buffer = terminal->pvt->buffer;
        g_assert(buffer != NULL);

	if (!buffer->pvt->cursor_visible)
		return;

	screen = buffer->pvt->screen;
	delta = screen->scroll_delta;
	col = screen->cursor_current.col;
	drow = screen->cursor_current.row;
	row = drow - delta;
	width = terminal->pvt->char_width;
	height = terminal->pvt->char_height;

	if ((CLAMP(col, 0, buffer->pvt->column_count - 1) != col) ||
	    (CLAMP(row, 0, buffer->pvt->row_count    - 1) != row))
		return;

	focus = terminal->pvt->has_focus;
	blink = terminal->pvt->cursor_blink_state;

	if (focus && !blink)
		return;

	/* Find the character "under" the cursor. */
	cell = vte_screen_find_charcell(screen, col, drow);
	while ((cell != NULL) && (cell->attr.fragment) && (col > 0)) {
		col--;
		cell = vte_screen_find_charcell(screen, col, drow);
	}

	/* Draw the cursor. */
	item.c = (cell && cell->c) ? cell->c : ' ';
	item.columns = cell ? cell->attr.columns : 1;
	item.x = col * width;
	item.y = row * height;
	cursor_width = item.columns * width;
	if (cell && cell->c != 0) {
		gint cw = _vte_draw_get_char_width (terminal->pvt->draw,
				cell->c, cell->attr.columns, cell->attr.bold);
		cursor_width = MAX(cursor_width, cw);
	}

	selected = vte_view_cell_is_selected(buffer, col, drow, terminal);

	vte_view_determine_cursor_colors(terminal, cell, selected, &fore, &back);

	x = item.x;
	y = item.y;

	switch (terminal->pvt->cursor_shape) {

		case VTE_CURSOR_SHAPE_IBEAM: {
                        int stem_width;

                        stem_width = (int) (((float) height) * terminal->pvt->cursor_aspect_ratio + 0.5);
                        stem_width = CLAMP (stem_width, VTE_LINE_WIDTH, cursor_width);
		 	
			vte_view_fill_rectangle(terminal, &terminal->pvt->palette[back],
						     x, y, stem_width, height);
			break;
                }

		case VTE_CURSOR_SHAPE_UNDERLINE: {
                        int line_height;

                        line_height = (int) (((float) width) * terminal->pvt->cursor_aspect_ratio + 0.5);
                        line_height = CLAMP (line_height, VTE_LINE_WIDTH, height);

			vte_view_fill_rectangle(terminal, &terminal->pvt->palette[back],
						     x, y + height - line_height,
						     cursor_width, line_height);
			break;
                }

		case VTE_CURSOR_SHAPE_BLOCK:

			if (focus) {
				/* just reverse the character under the cursor */
				vte_view_fill_rectangle (terminal,
							     &terminal->pvt->palette[back],
							     x, y,
							     cursor_width, height);

				if (!vte_view_unichar_is_local_graphic(terminal, item.c, cell ? cell->attr.bold : FALSE) ||
				    !vte_view_draw_graphic(terminal,
							       item.c,
							       fore, back,
							       TRUE,
							       item.x,
							       item.y,
							       width,
							       item.columns,
							       height,
							       cell ? cell->attr.bold : FALSE)) {
					gboolean hilite = FALSE;
					if (cell && terminal->pvt->show_match) {
						hilite = vte_cell_is_between(col, row,
								terminal->pvt->match_start.col,
								terminal->pvt->match_start.row,
								terminal->pvt->match_end.col,
								terminal->pvt->match_end.row,
								TRUE);
					}
					if (cell && cell->c != 0 && cell->c != ' ') {
						vte_view_draw_cells(terminal,
								&item, 1,
								fore, back, TRUE, FALSE,
								cell->attr.bold,
								cell->attr.underline,
								cell->attr.strikethrough,
								hilite,
								FALSE,
								width,
								height);
					}
				}

			} else {
				/* draw a box around the character */

				vte_view_draw_rectangle (terminal,
							    &terminal->pvt->palette[back],
							     x - VTE_LINE_WIDTH,
							     y - VTE_LINE_WIDTH,
							     cursor_width + 2*VTE_LINE_WIDTH,
							     height + 2*VTE_LINE_WIDTH);
			}

			break;
	}
}

static void
vte_view_paint_im_preedit_string(VteView *terminal)
{
        VteViewPrivate *pvt = terminal->pvt;
        VteBuffer *buffer;
	VteScreen *screen;
	int row, col, columns;
	long width, height, delta;
	int i, len;
	guint fore, back;

	if (!pvt->im_preedit)
		return;

        buffer = terminal->pvt->buffer;

	/* Get going. */
	screen = buffer->pvt->screen;

	/* Keep local copies of rendering information. */
	width = terminal->pvt->char_width;
	height = terminal->pvt->char_height;
	delta = screen->scroll_delta;

	row = screen->cursor_current.row - delta;

	/* Find out how many columns the pre-edit string takes up. */
	columns = vte_view_preedit_width(terminal, FALSE);
	len = vte_view_preedit_length(terminal, FALSE);

	/* If the pre-edit string won't fit on the screen if we start
	 * drawing it at the cursor's position, move it left. */
	col = screen->cursor_current.col;
	if (col + columns > buffer->pvt->column_count) {
		col = MAX(0, buffer->pvt->column_count - columns);
	}

	/* Draw the preedit string, boxed. */
	if (len > 0) {
		struct _vte_draw_text_request *items;
		const char *preedit = pvt->im_preedit;
		int preedit_cursor;

		items = g_new(struct _vte_draw_text_request, len);
		for (i = columns = 0; i < len; i++) {
			items[i].c = g_utf8_get_char(preedit);
			items[i].columns = _vte_iso2022_unichar_width(buffer->pvt->iso2022,
								      items[i].c);
			items[i].x = (col + columns) * width;
			items[i].y = row * height;
			columns += items[i].columns;
			preedit = g_utf8_next_char(preedit);
		}
		_vte_draw_clear(terminal->pvt->draw,
				col * width + terminal->pvt->padding.left,
				row * height + terminal->pvt->padding.top,
				width * columns,
				height,
                                &terminal->pvt->palette[VTE_DEF_BG]);
		fore = screen->defaults.attr.fore;
		back = screen->defaults.attr.back;
		vte_view_draw_cells_with_attributes(terminal,
							items, len,
							pvt->im_preedit_attrs,
							TRUE,
							width, height);
		preedit_cursor = pvt->im_preedit_cursor;
		if (preedit_cursor >= 0 && preedit_cursor < len) {
			/* Cursored letter in reverse. */
			vte_view_draw_cells(terminal,
						&items[preedit_cursor], 1,
						back, fore, TRUE, TRUE,
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

static gboolean
vte_view_draw(GtkWidget *widget,
                  cairo_t *cr)
{
        VteView *terminal = VTE_VIEW (widget);
        VteBuffer *buffer;
        cairo_rectangle_int_t clip_rect;
        cairo_region_t *region;
        int allocated_width, allocated_height;

        if (!gdk_cairo_get_clip_rectangle (cr, &clip_rect))
                return FALSE;

        _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_view_draw()\n");
        _vte_debug_print(VTE_DEBUG_WORK, "=");

        region = vte_cairo_get_clip_region (cr);
        if (region == NULL)
                return FALSE;

        _VTE_DEBUG_IF (VTE_DEBUG_UPDATES) {
                g_printerr ("vte_view_draw (%d,%d)x(%d,%d) pixels\n",
                            clip_rect.x, clip_rect.y,
                            clip_rect.width, clip_rect.height);
        }

        _vte_draw_set_cairo(terminal->pvt->draw, cr);

        allocated_width = gtk_widget_get_allocated_width(widget);
        allocated_height = gtk_widget_get_allocated_height(widget);

        buffer = terminal->pvt->buffer;

        /* Designate the start of the drawing operation and clear the area. */
        _vte_draw_clear (terminal->pvt->draw, 0, 0,
                         allocated_width, allocated_height,
                         &terminal->pvt->palette[VTE_DEF_BG]);

        if (buffer == NULL)
                goto done_drawing;

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
                                vte_view_expand_region (terminal, rr, rectangles + n);
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
                        vte_view_paint_area (terminal, rectangles + n);
                }
                g_free (rectangles);
        }

        vte_view_paint_cursor(terminal);

        vte_view_paint_im_preedit_string(terminal);

    done_drawing:

        /* Done with various structures. */
        _vte_draw_set_cairo(terminal->pvt->draw, NULL);

        cairo_region_destroy (region);

        terminal->pvt->invalidated_all = FALSE;

        return FALSE;
}

/* Handle a scroll event. */
static gboolean
vte_view_scroll(GtkWidget *widget, GdkEventScroll *event)
{
        VteView *terminal = VTE_VIEW(widget);
        VteBuffer *buffer;
	GtkAdjustment *adj;
	gdouble v;
	int button;

        buffer = terminal->pvt->buffer;

	vte_view_read_modifiers (terminal, (GdkEvent*) event);

	_VTE_DEBUG_IF(VTE_DEBUG_EVENTS)
		switch (event->direction) {
		case GDK_SCROLL_UP:
			g_printerr("Scroll up.\n");
			break;
		case GDK_SCROLL_DOWN:
			g_printerr("Scroll down.\n");
			break;
		default:
			break;
		}

	/* If we're running a mouse-aware application, map the scroll event
	 * to a button press on buttons four and five. */
	if (terminal->pvt->mouse_tracking_mode) {
		switch (event->direction) {
		case GDK_SCROLL_UP:
			button = 4;
			break;
		case GDK_SCROLL_DOWN:
			button = 5;
			break;
		default:
			button = 0;
			break;
		}
		if (button != 0) {
			/* Encode the parameters and send them to the app. */
			vte_view_send_mouse_button_internal(terminal,
								button,
								event->x,
								event->y);
		}
		return TRUE;
	}

	adj = terminal->pvt->vadjustment;
	v = MAX (1., ceil (gtk_adjustment_get_page_increment (adj) / 10.));
	switch (event->direction) {
	case GDK_SCROLL_UP:
		v = -v;
		break;
	case GDK_SCROLL_DOWN:
		break;
	default:
		return FALSE;
	}

	if (buffer->pvt->screen == &buffer->pvt->alternate_screen ||
            buffer->pvt->normal_screen.scrolling_restricted) {
		char *normal;
		gssize normal_length;
		const gchar *special;
		gint i, cnt = v;

		/* In the alternate screen there is no scrolling,
		 * so fake a few cursor keystrokes. */

		_vte_keymap_map (
				cnt > 0 ? GDK_KEY_Down : GDK_KEY_Up,
				terminal->pvt->modifiers,
				buffer->pvt->sun_fkey_mode,
				buffer->pvt->hp_fkey_mode,
				buffer->pvt->legacy_fkey_mode,
				buffer->pvt->vt220_fkey_mode,
				buffer->pvt->cursor_mode == VTE_KEYMODE_APPLICATION,
				buffer->pvt->keypad_mode == VTE_KEYMODE_APPLICATION,
				buffer->pvt->termcap,
				buffer->pvt->emulation ?
				buffer->pvt->emulation : vte_get_default_emulation(),
				&normal,
				&normal_length,
				&special);
		if (cnt < 0)
			cnt = -cnt;
		for (i = 0; i < cnt; i++) {
			vte_buffer_feed_child_using_modes (buffer,
					normal, normal_length);
		}
		g_free (normal);
	} else {
		/* Perform a history scroll. */
		v += buffer->pvt->screen->scroll_delta;
		vte_view_queue_adjustment_value_changed_clamped (terminal, v);
	}

	return TRUE;
}

/* Create a new accessible object associated with ourselves, and return
 * it to the caller. */
static AtkObject *
vte_view_get_accessible(GtkWidget *widget)
{
	static gboolean first_time = TRUE;

	if (first_time) {
		AtkObjectFactory *factory;
		AtkRegistry *registry;
		GType derived_type;
		GType derived_atk_type;

		/*
		 * Figure out whether accessibility is enabled by looking at the
		 * type of the accessible object which would be created for
		 * the parent type of VteView.
		 */
		derived_type = g_type_parent (VTE_TYPE_VIEW);

		registry = atk_get_default_registry ();
		factory = atk_registry_get_factory (registry,
						    derived_type);

		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE)) {
			atk_registry_set_factory_type (registry,
						       VTE_TYPE_VIEW,
						       _vte_view_accessible_factory_get_type ());
		}
		first_time = FALSE;
	}

	return GTK_WIDGET_CLASS (vte_view_parent_class)->get_accessible (widget);
}

static void
vte_view_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
        VteView *terminal = VTE_VIEW (object);
        VteViewPrivate *pvt = terminal->pvt;

	switch (prop_id)
	{
                case PROP_BUFFER:
                        g_value_set_object (value, vte_view_get_buffer(terminal));
                        break;
                case PROP_HADJUSTMENT:
                        g_value_set_object (value, pvt->hadjustment);
                        break;
                case PROP_VADJUSTMENT:
                        g_value_set_object (value, terminal->pvt->vadjustment);
                        break;
                case PROP_HSCROLL_POLICY:
                        g_value_set_enum (value, pvt->hscroll_policy);
                        break;
                case PROP_VSCROLL_POLICY:
                        g_value_set_enum (value, pvt->vscroll_policy);
                        break;
                case PROP_AUDIBLE_BELL:
                        g_value_set_boolean (value, vte_view_get_audible_bell (terminal));
                        break;
                case PROP_MOUSE_POINTER_AUTOHIDE:
                        g_value_set_boolean (value, vte_view_get_mouse_autohide (terminal));
                        break;
                case PROP_SCROLL_ON_KEYSTROKE:
                        g_value_set_boolean (value, pvt->scroll_on_keystroke);
                        break;
                case PROP_SCROLL_ON_OUTPUT:
                        g_value_set_boolean (value, pvt->scroll_on_output);
                        break;
                case PROP_WORD_CHARS:
                        g_value_set_string (value, NULL /* FIXME */);
                        break;
                case PROP_VISIBLE_BELL:
                        g_value_set_boolean (value, vte_view_get_visible_bell (terminal));
                        break;
                case PROP_FONT_SCALE:
                        g_value_set_double (value, vte_view_get_font_scale (terminal));
                        break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			return;
        }
}

static void
vte_view_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
        VteView *terminal = VTE_VIEW (object);
        VteViewPrivate *pvt = terminal->pvt;

	switch (prop_id)
	{
                case PROP_HADJUSTMENT:
                        vte_view_set_hadjustment (terminal, g_value_get_object (value));
                        break;
                case PROP_VADJUSTMENT:
                        vte_view_set_vadjustment (terminal, g_value_get_object (value));
                        break;
                case PROP_HSCROLL_POLICY:
                        pvt->hscroll_policy = g_value_get_enum (value);
                        gtk_widget_queue_resize_no_redraw (GTK_WIDGET (terminal));
                        break;
                case PROP_VSCROLL_POLICY:
                        pvt->vscroll_policy = g_value_get_enum (value);
                        gtk_widget_queue_resize_no_redraw (GTK_WIDGET (terminal));
                        break;
                case PROP_AUDIBLE_BELL:
                        vte_view_set_audible_bell (terminal, g_value_get_boolean (value));
                        break;
                case PROP_MOUSE_POINTER_AUTOHIDE:
                        vte_view_set_mouse_autohide (terminal, g_value_get_boolean (value));
                        break;
                case PROP_SCROLL_ON_KEYSTROKE:
                        vte_view_set_scroll_on_keystroke(terminal, g_value_get_boolean (value));
                        break;
                case PROP_SCROLL_ON_OUTPUT:
                        vte_view_set_scroll_on_output (terminal, g_value_get_boolean (value));
                        break;
                case PROP_WORD_CHARS:
                        vte_view_set_word_chars (terminal, g_value_get_string (value));
                        break;
                case PROP_VISIBLE_BELL:
                        vte_view_set_visible_bell (terminal, g_value_get_boolean (value));
                        break;
                case PROP_FONT_SCALE:
                        vte_view_set_font_scale (terminal, g_value_get_double (value));
                        break;

                /* Not writable */
                case PROP_BUFFER:
                        g_assert_not_reached ();
                        break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			return;
	}
}

/* Initialize methods. */
static void
vte_view_class_init(VteViewClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;
	GtkBindingSet  *binding_set;

#ifdef VTE_DEBUG
	{
                _vte_debug_init();
		_vte_debug_print(VTE_DEBUG_LIFECYCLE,
				"vte_view_class_init()\n");
		/* print out the legend */
		_vte_debug_print(VTE_DEBUG_WORK,
			"Debugging work flow (top input to bottom output):\n"
					"  .  _vte_view_process_incoming\n"
					"  <  start process_timeout\n"
					"  {[ start update_timeout  [ => rate limited\n"
					"  T  start of terminal in update_timeout\n"
					"  (  start _vte_view_process_incoming\n"
					"  ?  _vte_invalidate_cells (call)\n"
					"  !  _vte_invalidate_cells (dirty)\n"
					"  *  _vte_invalidate_all\n"
					"  )  end _vte_view_process_incoming\n"
					"  -  gdk_window_process_updates\n"
					"  =  vte_view_draw\n"
					"  ]} end update_timeout\n"
					"  >  end process_timeout\n");
	}
#endif

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
#ifdef HAVE_DECL_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif

	g_type_class_add_private(klass, sizeof (VteViewPrivate));

	gobject_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	/* Override some of the default handlers. */
	gobject_class->finalize = vte_view_finalize;
        gobject_class->get_property = vte_view_get_property;
        gobject_class->set_property = vte_view_set_property;
	widget_class->realize = vte_view_realize;
	widget_class->scroll_event = vte_view_scroll;
        widget_class->draw = vte_view_draw;
	widget_class->key_press_event = vte_view_key_press;
	widget_class->key_release_event = vte_view_key_release;
	widget_class->button_press_event = vte_view_button_press;
	widget_class->button_release_event = vte_view_button_release;
	widget_class->motion_notify_event = vte_view_motion_notify;
	widget_class->enter_notify_event = vte_view_enter;
	widget_class->leave_notify_event = vte_view_leave;
	widget_class->focus_in_event = vte_view_focus_in;
	widget_class->focus_out_event = vte_view_focus_out;
	widget_class->visibility_notify_event = vte_view_visibility_notify;
	widget_class->unrealize = vte_view_unrealize;
        widget_class->style_updated = vte_view_style_updated;
	widget_class->get_preferred_width = vte_view_get_preferred_width;
	widget_class->get_preferred_height = vte_view_get_preferred_height;
	widget_class->size_allocate = vte_view_size_allocate;
	widget_class->get_accessible = vte_view_get_accessible;
        widget_class->screen_changed = vte_view_screen_changed;

	/* Initialize default handlers. */
	klass->char_size_changed = NULL;
	klass->selection_changed = NULL;

	klass->increase_font_size = NULL;
	klass->decrease_font_size = NULL;

	klass->text_scrolled = NULL;

	klass->copy_clipboard = vte_view_real_copy_clipboard;
	klass->paste_clipboard = vte_view_real_paste_clipboard;
        klass->copy_primary = vte_view_real_copy_primary;
        klass->paste_primary = vte_view_real_paste_primary;

        /* GtkScrollable interface properties */
        g_object_class_override_property (gobject_class, PROP_HADJUSTMENT, "hadjustment");
        g_object_class_override_property (gobject_class, PROP_VADJUSTMENT, "vadjustment");
        g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
        g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

	/* Register some signals of our own. */

        /**
         * VteView::buffer-changed:
         * @terminal: the object which received the signal
         * @previous_buffer: the previous buffer, or %NULL if there was none
         *
         * Emitted whenever the #VteBuffer of @terminal changes.
         */
        signals[TERMINAL_BUFFER_CHANGED] =
                g_signal_new(I_("buffer-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteViewClass, buffer_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__OBJECT,
                             G_TYPE_NONE,
                             1, G_TYPE_OBJECT);

        /**
         * VteView::char-size-changed:
         * @vteterminal: the object which received the signal
         * @width: the new character cell width
         * @height: the new character cell height
         *
         * Emitted whenever selection of a new font causes the values of the
         * %char_width or %char_height fields to change.
         */
                g_signal_new(I_("char-size-changed"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteViewClass, char_size_changed),
			     NULL,
			     NULL,
			     _vte_marshal_VOID__UINT_UINT,
			     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

        /**
         * VteView::selection-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the contents of terminal's selection changes.
         */
                g_signal_new (I_("selection-changed"),
			      G_OBJECT_CLASS_TYPE(klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(VteViewClass, selection_changed),
			      NULL,
			      NULL,
                              g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

        /**
         * VteView::increase-font-size:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the user hits the '+' key while holding the Control key.
         */
                g_signal_new(I_("increase-font-size"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteViewClass, increase_font_size),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteView::decrease-font-size:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the user hits the '-' key while holding the Control key.
         */
                g_signal_new(I_("decrease-font-size"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteViewClass, decrease_font_size),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);


        /**
         * VteView::text-scrolled:
         * @vteterminal: the object which received the signal
         * @delta: the number of lines scrolled
         *
         * An internal signal used for communication between the terminal and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
                g_signal_new(I_("text-scrolled"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteViewClass, text_scrolled),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__INT,
			     G_TYPE_NONE, 1, G_TYPE_INT);

        /**
         * VteView::copy-clipboard:
         * @vteterminal: the object which received the signal
         *
         * A keybinding signal that is emitted to copy the selection to the
         * %GDK_SELECTION_CLIPBOARD clipboard.
         */
	signals[TERMINAL_COPY_CLIPBOARD] =
                g_signal_new(I_("copy-clipboard"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_STRUCT_OFFSET(VteViewClass, copy_clipboard),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteView::paste-clipboard:
         * @vteterminal: the object which received the signal
         *
         * A keybinding signal that is emitted to paste the data from the
         * %GDK_SELECTION_CLIPBOARD clipboard.
         */
        signals[TERMINAL_PASTE_CLIPBOARD] =
                g_signal_new(I_("paste-clipboard"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_STRUCT_OFFSET(VteViewClass, paste_clipboard),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteView::copy-primary:
         * @vteterminal: the object which received the signal
         *
         * A keybinding signal that is emitted to copy the selection to the
         * %GDK_SELECTION_PRIMARY clipboard.
         */
        signals[TERMINAL_COPY_PRIMARY] =
                g_signal_new(I_("copy-primary"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                             G_STRUCT_OFFSET(VteViewClass, copy_primary),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteView::paste-primary:
         * @vteterminal: the object which received the signal
         *
         * A keybinding signal that is emitted to paste the data from the
         * %GDK_SELECTION_PRIMARY clipboard.
         */
        signals[TERMINAL_PASTE_PRIMARY] =
                g_signal_new(I_("paste-primary"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                             G_STRUCT_OFFSET(VteViewClass, paste_primary),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteView:buffer:
         *
         * The terminal's buffer.
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_AUDIBLE_BELL,
                 g_param_spec_object ("buffer", NULL, NULL,
                                       VTE_TYPE_BUFFER,
                                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

        /**
         * VteView:audible-bell:
         *
         * Controls whether or not the terminal will beep when the child outputs the
         * "bl" sequence.
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_AUDIBLE_BELL,
                 g_param_spec_boolean ("audible-bell", NULL, NULL,
                                       TRUE,
                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
         * VteView:font-scale:
         *
         * The terminal's font scale.
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_AUDIBLE_BELL,
                 g_param_spec_double ("font-scale", NULL, NULL,
                                      VTE_SCALE_MIN,
                                      VTE_SCALE_MAX,
                                      1.,
                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
         * VteView:pointer-autohide:
         *
         * Controls the value of the terminal's mouse autohide setting.  When autohiding
         * is enabled, the mouse cursor will be hidden when the user presses a key and
         * shown when the user moves the mouse.
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_MOUSE_POINTER_AUTOHIDE,
                 g_param_spec_boolean ("pointer-autohide", NULL, NULL,
                                       FALSE,
                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
     
        /**
         * VteView:scroll-on-keystroke:
         *
         * Controls whether or not the terminal will forcibly scroll to the bottom of
         * the viewable history when the user presses a key.  Modifier keys do not
         * trigger this behavior.
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_SCROLL_ON_KEYSTROKE,
                 g_param_spec_boolean ("scroll-on-keystroke", NULL, NULL,
                                       FALSE,
                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
     
        /**
         * VteView:scroll-on-output:
         *
         * Controls whether or not the terminal will forcibly scroll to the bottom of
         * the viewable history when the new data is received from the child.
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_SCROLL_ON_OUTPUT,
                 g_param_spec_boolean ("scroll-on-output", NULL, NULL,
                                       TRUE,
                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
     
        /**
         * VteView:word-chars:
         *
         * When the user double-clicks to start selection, the terminal will extend
         * the selection on word boundaries.  It will treat characters the word-chars
         * characters as parts of words, and all other characters as word separators.
         * Ranges of characters can be specified by separating them with a hyphen.
         *
         * As a special case, when setting this to %NULL or the empty string, the terminal will
         * treat all graphic non-punctuation non-space characters as word characters.
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_WORD_CHARS,
                 g_param_spec_string ("word-chars", NULL, NULL,
                                      NULL,
                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
     
        /**
         * VteView:visible-bell:
         *
         * Controls whether the terminal will present a visible bell to the
         * user when the child outputs the "bl" sequence.  The terminal
         * will clear itself to the default foreground color and then repaint itself.
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_VISIBLE_BELL,
                 g_param_spec_boolean ("visible-bell", NULL, NULL,
                                       FALSE,
                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /* Style properties */

        /**
         * VteView:allow-bold:
         *
         * Controls whether or not the terminal will attempt to draw bold text.
         * This may happen either by using a bold font variant, or by
         * repainting text with a different offset.
         */
        gtk_widget_class_install_style_property
                (widget_class,
                 g_param_spec_boolean ("allow-bold", NULL, NULL,
                                       TRUE,
                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
         * VteView:cursor-blink-mode:
         *
         * Sets whether or not the cursor will blink. Using %VTE_CURSOR_BLINK_SYSTEM
         * will use the #GtkSettings::gtk-cursor-blink setting.
         */
        gtk_widget_class_install_style_property
                (widget_class,
                 g_param_spec_enum ("cursor-blink-mode", NULL, NULL,
                                    VTE_TYPE_CURSOR_BLINK_MODE,
                                    VTE_CURSOR_BLINK_SYSTEM,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
         * VteView:cursor-shape:
         *
         * Controls the shape of the cursor.
         */
        gtk_widget_class_install_style_property
                (widget_class,
                 g_param_spec_enum ("cursor-shape", NULL, NULL,
                                    VTE_TYPE_CURSOR_SHAPE,
                                    VTE_CURSOR_SHAPE_BLOCK,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
         * VteView:font:
         *
         * Specifies the font used for rendering all text displayed by the terminal.
         * Must be a monospaced font!
         */
        gtk_widget_class_install_style_property
                (widget_class,
                 g_param_spec_boxed ("font", NULL, NULL,
                                     PANGO_TYPE_FONT_DESCRIPTION,
                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
         * VteView:reverse:
         *
         * In reverse mode, the terminal draws everything with foreground and
         * background colours reversed.
         *
         * This is a global setting; the terminal application can still
         * set reverse mode explicitly. In case both this style property and
         * the application select reverse mode, the terminal draws in
         * non-reverse mode.
         */
        gtk_widget_class_install_style_property
                (widget_class,
                 g_param_spec_boolean ("reverse", NULL, NULL,
                                       FALSE,
                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /* Colours */

#include "vtepalettedefs.h"

        /**
         * VteView:cursor-effect:
         *
         * Controls how the terminal will draw the cursor.
         *
         * If set to %VTE_EFFECT_COLOR, the cursor is drawn
         * with the background color from the #VteView:cursor-background-color
         * style property.
         */
        gtk_widget_class_install_style_property
                (widget_class,
                 g_param_spec_enum ("cursor-effect", NULL, NULL,
                                    VTE_TYPE_EFFECT,
                                    VTE_EFFECT_REVERSE,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
         * VteView:reverse-effect:
         *
         * Controls how the terminal will draw reversed text.
         *
         * If set to %VTE_EFFECT_COLOR, reversed text is drawn
         * with the background color from the #VteView:reverse-background-color
         * style property.
         */
        gtk_widget_class_install_style_property
                (widget_class,
                 g_param_spec_enum ("reverse-effect", NULL, NULL,
                                    VTE_TYPE_EFFECT,
                                    VTE_EFFECT_REVERSE,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
         * VteView:selection-effect:
         *
         * Controls how the terminal will draw selected text.
         *
         * If set to %VTE_EFFECT_COLOR, selected text is drawn
         * with the background color from the #VteView:selection-background-color
         * style property.
         */
        gtk_widget_class_install_style_property
                (widget_class,
                 g_param_spec_enum ("selection-effect", NULL, NULL,
                                    VTE_TYPE_EFFECT,
                                    VTE_EFFECT_REVERSE,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /* Keybindings */
	binding_set = gtk_binding_set_by_class(klass);

	/* Bind Copy, Paste, Cut keys */
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_F16, 0, "copy-clipboard",0);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_F18, 0, "paste-clipboard", 0);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_F20, 0, "copy-clipboard",0);

	process_timer = g_timer_new ();

        klass->priv = G_TYPE_CLASS_GET_PRIVATE (klass, VTE_TYPE_VIEW, VteViewClassPrivate);

        klass->priv->style_provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
        gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (klass->priv->style_provider),
                                         "VteView {\n"
                                           "padding: 1px 1px 1px 1px;\n"
                                           "-VteView-allow-bold: true;\n"
                                           "-VteView-cursor-blink-mode: system;\n"
                                           "-VteView-cursor-shape: block;\n"
                                           "-VteView-font: Monospace 10;\n"
#include "vtepalettecss.h"
                                           "}\n",
                                         -1, NULL);
}

/**
 * vte_view_set_audible_bell:
 * @terminal: a #VteView
 * @is_audible: %TRUE if the terminal should beep
 *
 * Controls whether or not the terminal will beep when the child outputs the
 * "bl" sequence.
 */
void
vte_view_set_audible_bell(VteView *terminal, gboolean is_audible)
{
        VteViewPrivate *pvt;

	g_return_if_fail(VTE_IS_VIEW(terminal));

        pvt = terminal->pvt;

        is_audible = is_audible != FALSE;
        if (is_audible == pvt->audible_bell)
                return;

	pvt->audible_bell = is_audible;

        g_object_notify (G_OBJECT (terminal), "audible-bell");
}

/**
 * vte_view_get_audible_bell:
 * @terminal: a #VteView
 *
 * Checks whether or not the terminal will beep when the child outputs the
 * "bl" sequence.
 *
 * Returns: %TRUE if audible bell is enabled, %FALSE if not
 */
gboolean
vte_view_get_audible_bell(VteView *terminal)
{
	g_return_val_if_fail(VTE_IS_VIEW(terminal), FALSE);
	return terminal->pvt->audible_bell;
}

/**
 * vte_view_set_visible_bell:
 * @terminal: a #VteView
 * @is_visible: whether the terminal should flash on bell
 *
 * Controls whether or not the terminal will present a visible bell to the
 * user when the child outputs the "bl" sequence.  The terminal
 * will clear itself to the default foreground color and then repaint itself.
 *
 */
void
vte_view_set_visible_bell(VteView *terminal, gboolean is_visible)
{
        VteViewPrivate *pvt;

	g_return_if_fail(VTE_IS_VIEW(terminal));

        pvt = terminal->pvt;

        is_visible = is_visible != FALSE;
        if (is_visible == pvt->visible_bell)
                return;

	pvt->visible_bell = is_visible;

        g_object_notify (G_OBJECT (terminal), "visible-bell");
}

/**
 * vte_view_get_visible_bell:
 * @terminal: a #VteView
 *
 * Checks whether or not the terminal will present a visible bell to the
 * user when the child outputs the "bl" sequence.  The terminal
 * will clear itself to the default foreground color and then repaint itself.
 *
 * Returns: %TRUE if visible bell is enabled, %FALSE if not
 */
gboolean
vte_view_get_visible_bell(VteView *terminal)
{
	g_return_val_if_fail(VTE_IS_VIEW(terminal), FALSE);
	return terminal->pvt->visible_bell;
}

/**
 * vte_view_set_scroll_on_output:
 * @terminal: a #VteView
 * @scroll: whether the terminal should scroll on output
 *
 * Controls whether or not the terminal will forcibly scroll to the bottom of
 * the viewable history when the new data is received from the child.
 */
void
vte_view_set_scroll_on_output(VteView *terminal, gboolean scroll)
{
	g_return_if_fail(VTE_IS_VIEW(terminal));
	terminal->pvt->scroll_on_output = scroll;
}

/**
 * vte_view_set_scroll_on_keystroke:
 * @terminal: a #VteView
 * @scroll: whether the terminal should scroll on keystrokes
 *
 * Controls whether or not the terminal will forcibly scroll to the bottom of
 * the viewable history when the user presses a key.  Modifier keys do not
 * trigger this behavior.
 */
void
vte_view_set_scroll_on_keystroke(VteView *terminal, gboolean scroll)
{
        VteViewPrivate *pvt;

	g_return_if_fail(VTE_IS_VIEW(terminal));

        pvt = terminal->pvt;

        scroll = scroll != FALSE;
        if (scroll == pvt->scroll_on_keystroke)
                return;

	pvt->scroll_on_keystroke = scroll;

        g_object_notify (G_OBJECT (terminal), "scroll-on-keystroke");
}

/**
 * vte_view_paste_clipboard:
 * @terminal: a #VteView
 * @clipboard: a #GtkClipboard
 *
 * Sends the contents of @clipboard to the terminal's child.
 * If necessary, the data is converted from UTF-8 to the
 * terminal's current encoding.
 */
void
vte_view_paste_clipboard(VteView *terminal,
                             GtkClipboard *clipboard)
{
        g_return_if_fail(VTE_IS_VIEW(terminal));
        g_return_if_fail(GTK_IS_CLIPBOARD(clipboard));

        _vte_debug_print(VTE_DEBUG_SELECTION,
                         "Requesting clipboard contents.\n");
        gtk_clipboard_request_text(clipboard,
                                   vte_view_paste_cb,
                                   terminal);
        /* FIXMEchpe! this crashes if the terminal is destroyed before the paste is received! */
}

/**
 * vte_view_im_append_menuitems:
 * @terminal: a #VteView
 * @menushell: a GtkMenuShell
 *
 * Appends menu items for various input methods to the given menu.  The
 * user can select one of these items to modify the input method used by
 * the terminal.
 */
void
vte_view_im_append_menuitems(VteView *terminal, GtkMenuShell *menushell)
{
	GtkIMMulticontext *context;
	g_return_if_fail(VTE_IS_VIEW(terminal));
	g_return_if_fail (gtk_widget_get_realized (&terminal->widget));
        g_return_if_fail(GTK_IS_MENU_SHELL(menushell));
	context = GTK_IM_MULTICONTEXT(terminal->pvt->im_context);
	gtk_im_multicontext_append_menuitems(context, menushell);
}

/**
 * vte_view_get_has_selection:
 * @terminal: a #VteView
 *
 * Checks if the terminal currently contains selected text.  Note that this
 * is different from determining if the terminal is the owner of any
 * #GtkClipboard items.
 *
 * Returns: %TRUE if part of the text in the terminal is selected.
 */
gboolean
vte_view_get_has_selection(VteView *terminal)
{
	g_return_val_if_fail(VTE_IS_VIEW(terminal), FALSE);
	return terminal->pvt->has_selection;
}

/**
 * vte_view_get_selection_bounds:
 * @terminal: a #VteView
 * @start: (allow-none): a #VteBufferIter, or %NULL
 * @end: (allow-none): a #VteBufferIter, or %NULL
 *
 * Returns whether text is selected in a @terminal. If @start or @end
 * are non-%NULL, they are filled in with the start resp. end of the selection.
 *
 * Returns: %TRUE if there is text selected in @terminal
 */ 
gboolean
vte_view_get_selection_bounds(VteView *terminal,
                                  VteBufferIter *start,
                                  VteBufferIter *end)
{
        VteViewPrivate *pvt;
        VteBufferIterReal *real_start, *real_end;

        g_return_val_if_fail(VTE_IS_VIEW(terminal), FALSE);

        pvt = terminal->pvt;
        if (pvt->buffer == NULL)
                return FALSE;

        if (!terminal->pvt->has_selection)
                return FALSE;

        if (start) {
                real_start = (VteBufferIterReal *) start;
                _vte_buffer_iter_init(real_start, pvt->buffer);
                real_start->position = terminal->pvt->selection_start;
        }
        if (end) {
                real_end = (VteBufferIterReal *) end;
                _vte_buffer_iter_init(real_end, pvt->buffer);
                real_end->position = terminal->pvt->selection_end;
        }

        return TRUE;
}

static void
vte_view_set_cursor_blinks_internal(VteView *terminal, gboolean blink)
{
        VteViewPrivate *pvt = terminal->pvt;

	blink = !!blink;
	if (pvt->cursor_blinks == blink)
		return;

	pvt->cursor_blinks = blink;
	_vte_check_cursor_blink (terminal);
}

/**
 * vte_buffer_set_scrollback_lines:
 * @buffer: a #VteBuffer
 * @lines: the length of the history buffer
 *
 * Sets the length of the scrollback buffer used by the terminal.  The size of
 * the scrollback buffer will be set to the larger of this value and the number
 * of visible rows the widget can display, so 0 can safely be used to disable
 * scrollback.
 *
 * A negative value means "infinite scrollback".
 *
 * Note that this setting only affects the normal screen buffer.
 * For terminal types which have an alternate screen buffer, no scrollback is
 * allowed on the alternate screen buffer.
 */
void
vte_buffer_set_scrollback_lines(VteBuffer *buffer,
                                glong lines)
{
        VteBufferPrivate *pvt;
        GObject *object;
	glong scroll_delta;
	VteScreen *screen;

	g_return_if_fail(VTE_IS_BUFFER(buffer));

	if (lines < 0)
		lines = G_MAXLONG;

        object = G_OBJECT(buffer);
        pvt = buffer->pvt;

#if 0
        /* FIXME: this breaks the scrollbar range, bug #562511 */
        if (lines == pvt->scrollback_lines)
                return;
#endif

        g_object_freeze_notify(object);

	_vte_debug_print (VTE_DEBUG_MISC,
			"Setting scrollback lines to %ld\n", lines);

	pvt->scrollback_lines = lines;
	screen = pvt->screen;
	scroll_delta = screen->scroll_delta;

	/* The main screen gets the full scrollback buffer, but the
	 * alternate screen isn't allowed to scroll at all. */
	if (screen == &buffer->pvt->normal_screen) {
		glong low, high, next;
		/* We need at least as many lines as are visible */
		lines = MAX (lines, buffer->pvt->row_count);
		next = MAX (screen->cursor_current.row + 1,
				_vte_ring_next (screen->row_data));
		_vte_ring_resize (screen->row_data, lines);
		low = _vte_ring_delta (screen->row_data);
		high = lines + MIN (G_MAXLONG - lines, low - buffer->pvt->row_count + 1);
		screen->insert_delta = CLAMP (screen->insert_delta, low, high);
		scroll_delta = CLAMP (scroll_delta, low, screen->insert_delta);
		next = MIN (next, screen->insert_delta + buffer->pvt->row_count);
		if (_vte_ring_next (screen->row_data) > next){
			_vte_ring_shrink (screen->row_data, next - low);
		}
	} else {
		_vte_ring_resize (screen->row_data, buffer->pvt->row_count);
		scroll_delta = _vte_ring_delta (screen->row_data);
		screen->insert_delta = _vte_ring_delta (screen->row_data);
		if (_vte_ring_next (screen->row_data) > screen->insert_delta + buffer->pvt->row_count){
			_vte_ring_shrink (screen->row_data, buffer->pvt->row_count);
		}
	}

	/* Adjust the scrollbars to the new locations. */
        if (buffer->pvt->terminal) {
                vte_view_queue_adjustment_value_changed (buffer->pvt->terminal, scroll_delta);
                _vte_view_adjust_adjustments_full (buffer->pvt->terminal);
        }

        g_object_notify(object, "scrollback-lines");

        g_object_thaw_notify(object);
}

/**
 * vte_view_set_word_chars:
 * @terminal: a #VteView
 * @spec: a specification
 *
 * When the user double-clicks to start selection, the terminal will extend
 * the selection on word boundaries.  It will treat characters included in @spec
 * as parts of words, and all other characters as word separators.  Ranges of
 * characters can be specified by separating them with a hyphen.
 *
 * As a special case, if @spec is %NULL or the empty string, the terminal will
 * treat all graphic non-punctuation non-space characters as word characters.
 */
void
vte_view_set_word_chars(VteView *terminal, const char *spec)
{
	gunichar *wbuf;
        glong len, i;
	VteWordCharRange range;

	g_return_if_fail(VTE_IS_VIEW(terminal));

	/* Allocate a new range array. */
	if (terminal->pvt->word_chars != NULL) {
		g_array_free(terminal->pvt->word_chars, TRUE);
	}
	terminal->pvt->word_chars = g_array_new(FALSE, TRUE,
						sizeof(VteWordCharRange));
	/* Special case: if spec is NULL, try to do the right thing. */
	if (spec == NULL || spec[0] == '\0') {
                g_object_notify(G_OBJECT(terminal), "word-chars");
		return;
	}

	/* Convert the spec from UTF-8 to a string of gunichars . */
        wbuf = g_utf8_to_ucs4_fast(spec, -1, &len);
	for (i = 0; i < len; i++) {
		/* The hyphen character. */
		if (wbuf[i] == '-') {
			range.start = wbuf[i];
			range.end = wbuf[i];
			g_array_append_val(terminal->pvt->word_chars, range);
			_vte_debug_print(VTE_DEBUG_MISC,
				"Word charset includes hyphen.\n");
			continue;
		}
		/* A single character, not the start of a range. */
		if ((wbuf[i] != '-') && (wbuf[i + 1] != '-')) {
			range.start = wbuf[i];
			range.end = wbuf[i];
			g_array_append_val(terminal->pvt->word_chars, range);
			_vte_debug_print(VTE_DEBUG_MISC,
					"Word charset includes `%lc'.\n",
					(wint_t) wbuf[i]);
			continue;
		}
		/* The start of a range. */
		if ((wbuf[i] != '-') &&
		    (wbuf[i + 1] == '-') &&
		    (wbuf[i + 2] != '-') &&
		    (wbuf[i + 2] != 0)) {
			range.start = wbuf[i];
			range.end = wbuf[i + 2];
			g_array_append_val(terminal->pvt->word_chars, range);
			_vte_debug_print(VTE_DEBUG_MISC,
					"Word charset includes range from "
					"`%lc' to `%lc'.\n", (wint_t) wbuf[i],
					(wint_t) wbuf[i + 2]);
			i += 2;
			continue;
		}
	}

	g_free(wbuf);

        g_object_notify(G_OBJECT(terminal), "word-chars");
}

/**
 * vte_buffer_set_backspace_binding:
 * @buffer: a #VteBuffer
 * @binding: a #VteEraseBinding for the backspace key
 *
 * Modifies the buffer's backspace key binding, which controls what
 * string or control sequence the buffer sends to its child when the user
 * presses the backspace key.
 */
void
vte_buffer_set_backspace_binding(VteBuffer *buffer,
				 VteEraseBinding binding)
{
        VteBufferPrivate *pvt;

	g_return_if_fail(VTE_IS_BUFFER(buffer));

        pvt = buffer->pvt;

        if (binding == pvt->backspace_binding)
                return;

	/* FIXME: should we set the pty mode to match? */
	pvt->backspace_binding = binding;

        g_object_notify(G_OBJECT(buffer), "backspace-binding");
}

/**
 * vte_buffer_set_delete_binding:
 * @buffer: a #VteBuffer
 * @binding: a #VteEraseBinding for the delete key
 *
 * Modifies the buffer's delete key binding, which controls what
 * string or control sequence the buffer sends to its child when the user
 * presses the delete key.
 */
void
vte_buffer_set_delete_binding(VteBuffer *buffer,
                              VteEraseBinding binding)
{
        VteBufferPrivate *pvt;

	g_return_if_fail(VTE_IS_BUFFER(buffer));

        pvt = buffer->pvt;

        if (binding == pvt->delete_binding)
                return;

	pvt->delete_binding = binding;

        g_object_notify(G_OBJECT(buffer), "delete-binding");
}

/**
 * vte_view_set_mouse_autohide:
 * @terminal: a #VteView
 * @setting: whether the mouse pointer should autohide
 *
 * Changes the value of the terminal's mouse autohide setting.  When autohiding
 * is enabled, the mouse cursor will be hidden when the user presses a key and
 * shown when the user moves the mouse.  This setting can be read using
 * vte_view_get_mouse_autohide().
 */
void
vte_view_set_mouse_autohide(VteView *terminal, gboolean setting)
{
        VteViewPrivate *pvt;

	g_return_if_fail(VTE_IS_VIEW(terminal));

        pvt = terminal->pvt;

        setting = setting != FALSE;
        if (setting == pvt->mouse_autohide)
                return;

	pvt->mouse_autohide = setting;

        g_object_notify(G_OBJECT(terminal), "pointer-autohide");
}

/**
 * vte_view_get_mouse_autohide:
 * @terminal: a #VteView
 *
 * Determines the value of the terminal's mouse autohide setting.  When
 * autohiding is enabled, the mouse cursor will be hidden when the user presses
 * a key and shown when the user moves the mouse.  This setting can be changed
 * using vte_view_set_mouse_autohide().
 *
 * Returns: %TRUE if autohiding is enabled, %FALSE if not
 */
gboolean
vte_view_get_mouse_autohide(VteView *terminal)
{
	g_return_val_if_fail(VTE_IS_VIEW(terminal), FALSE);
	return terminal->pvt->mouse_autohide;
}

/**
 * vte_buffer_reset:
 * @buffer: a #VteBuffer
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
vte_buffer_reset(VteBuffer *buffer,
                 gboolean clear_tabstops,
                 gboolean clear_history)
{
        VteBufferPrivate *pvt;
        VteView *terminal;

	g_return_if_fail(VTE_IS_BUFFER(buffer));

        pvt = buffer->pvt;
        terminal = pvt->terminal;

        g_object_freeze_notify(G_OBJECT(buffer));

	/* Stop processing any of the data we've got backed up. */
	vte_view_stop_processing (terminal);

	/* Clear the input and output buffers. */
	_vte_incoming_chunks_release (pvt->incoming);
	pvt->incoming = NULL;
	g_array_set_size(pvt->pending, 0);
	_vte_byte_array_clear(pvt->outgoing);
	/* Reset charset substitution state. */
	_vte_iso2022_state_free(pvt->iso2022);
	pvt->iso2022 = _vte_iso2022_state_new(NULL,
							&_vte_view_codeset_changed_cb,
							buffer);
	_vte_iso2022_state_set_codeset(pvt->iso2022,
				       pvt->encoding);
	/* Reset keypad/cursor/function key modes. */
	pvt->keypad_mode = VTE_KEYMODE_NORMAL;
	pvt->cursor_mode = VTE_KEYMODE_NORMAL;
	pvt->sun_fkey_mode = FALSE;
	pvt->hp_fkey_mode = FALSE;
	pvt->legacy_fkey_mode = FALSE;
	pvt->vt220_fkey_mode = FALSE;
	/* Enable meta-sends-escape. */
	pvt->meta_sends_escape = TRUE;
	/* Disable smooth scroll. */
	pvt->smooth_scroll = FALSE;
	/* Disable margin bell. */
	pvt->margin_bell = FALSE;
	/* Enable iso2022/NRC processing. */
	pvt->nrc_mode = TRUE;
	/* Reset saved settings. */
	if (pvt->dec_saved != NULL) {
		g_hash_table_destroy(pvt->dec_saved);
		pvt->dec_saved = g_hash_table_new(NULL, NULL);
	}
	/* Reset the color palette. */
        vte_view_update_style_colors(terminal, TRUE);
	/* Reset the default attributes.  Reset the alternate attribute because
	 * it's not a real attribute, but we need to treat it as one here. */
        _vte_screen_set_default_attributes(&pvt->alternate_screen);
        _vte_screen_set_default_attributes(&pvt->normal_screen);
        pvt->screen = &pvt->normal_screen;
	/* Reset alternate charset mode. */
	pvt->normal_screen.alternate_charset = FALSE;
	pvt->alternate_screen.alternate_charset = FALSE;
	/* Clear the scrollback buffers and reset the cursors. */
	if (clear_history) {
		_vte_ring_fini(pvt->normal_screen.row_data);
		_vte_ring_init(pvt->normal_screen.row_data, pvt->scrollback_lines);
		_vte_ring_fini(pvt->alternate_screen.row_data);
		_vte_ring_init(pvt->alternate_screen.row_data, buffer->pvt->row_count);
		pvt->normal_screen.cursor_saved.row = 0;
		pvt->normal_screen.cursor_saved.col = 0;
		pvt->normal_screen.cursor_current.row = 0;
		pvt->normal_screen.cursor_current.col = 0;
		pvt->normal_screen.scroll_delta = 0;
		pvt->normal_screen.insert_delta = 0;
		pvt->alternate_screen.cursor_saved.row = 0;
		pvt->alternate_screen.cursor_saved.col = 0;
		pvt->alternate_screen.cursor_current.row = 0;
		pvt->alternate_screen.cursor_current.col = 0;
		pvt->alternate_screen.scroll_delta = 0;
		pvt->alternate_screen.insert_delta = 0;
		_vte_view_adjust_adjustments_full (terminal);
	}
	/* Clear the status lines. */
	pvt->normal_screen.status_line = FALSE;
	pvt->normal_screen.status_line_changed = FALSE;
	if (pvt->normal_screen.status_line_contents != NULL) {
		g_string_free(pvt->normal_screen.status_line_contents,
			      TRUE);
	}
	pvt->normal_screen.status_line_contents = g_string_new(NULL);
	pvt->alternate_screen.status_line = FALSE;
	pvt->alternate_screen.status_line_changed = FALSE;
	if (pvt->alternate_screen.status_line_contents != NULL) {
		g_string_free(pvt->alternate_screen.status_line_contents,
			      TRUE);
	}
	pvt->alternate_screen.status_line_contents = g_string_new(NULL);
	/* Do more stuff we refer to as a "full" reset. */
	if (clear_tabstops) {
		vte_buffer_set_default_tabstops(buffer);
	}
	/* Reset restricted scrolling regions, leave insert mode, make
	 * the cursor visible again. */
	pvt->normal_screen.scrolling_restricted = FALSE;
	pvt->normal_screen.sendrecv_mode = TRUE;
	pvt->normal_screen.insert_mode = FALSE;
	pvt->normal_screen.linefeed_mode = FALSE;
	pvt->normal_screen.origin_mode = FALSE;
	pvt->normal_screen.reverse_mode = FALSE;
	pvt->normal_screen.bracketed_paste_mode = FALSE;
	pvt->alternate_screen.scrolling_restricted = FALSE;
	pvt->alternate_screen.sendrecv_mode = TRUE;
	pvt->alternate_screen.insert_mode = FALSE;
	pvt->alternate_screen.linefeed_mode = FALSE;
	pvt->alternate_screen.origin_mode = FALSE;
	pvt->alternate_screen.reverse_mode = FALSE;
	pvt->alternate_screen.bracketed_paste_mode = FALSE;
	pvt->cursor_visible = TRUE;
	/* Reset the encoding. */
	vte_buffer_set_encoding(buffer, NULL);
	g_assert(pvt->encoding != NULL);
	/* Reset selection. */
	vte_view_deselect_all(terminal);
	terminal->pvt->has_selection = FALSE;
        terminal->pvt->selecting = FALSE;
        terminal->pvt->selecting_restart = FALSE;
        terminal->pvt->selecting_had_delta = FALSE;
        if (terminal->pvt->selection != NULL) {
		g_free(terminal->pvt->selection);
                terminal->pvt->selection = NULL;
                memset(&terminal->pvt->selection_origin, 0,
                       sizeof(&terminal->pvt->selection_origin));
                memset(&terminal->pvt->selection_last, 0,
                       sizeof(&terminal->pvt->selection_last));
                memset(&terminal->pvt->selection_start, 0,
                       sizeof(&terminal->pvt->selection_start));
                memset(&terminal->pvt->selection_end, 0,
                       sizeof(&terminal->pvt->selection_end));
	}
	/* Reset mouse motion events. */
        terminal->pvt->mouse_tracking_mode = MOUSE_TRACKING_NONE;
        pvt->mouse_urxvt_extension = FALSE;
        terminal->pvt->mouse_last_button = 0;
        terminal->pvt->mouse_last_x = 0;
        terminal->pvt->mouse_last_y = 0;
        terminal->pvt->mouse_last_cell_x = 0;
        terminal->pvt->mouse_last_cell_y = 0;
	/* Clear modifiers. */
        terminal->pvt->modifiers = 0;
	/* Cause everything to be redrawn (or cleared). */
	vte_view_maybe_scroll_to_bottom(terminal);
	_vte_invalidate_all(terminal);

        g_object_thaw_notify(G_OBJECT(buffer));
}

/**
 * vte_buffer_get_status_line:
 * @buffer: a #VteBuffer
 *
 * Some buffer emulations specify a status line which is separate from the
 * main display area, and define a means for applications to move the cursor
 * to the status line and back.
 *
 * Returns: (transfer none): the current contents of the buffer's status line.
 *   For buffers like "xterm", this will usually be the empty string.  The string
 *   must not be modified or freed by the caller.
 */
const char *
vte_buffer_get_status_line(VteBuffer *buffer)
{
	g_return_val_if_fail(VTE_IS_BUFFER(buffer), NULL);
	return buffer->pvt->screen->status_line_contents->str;
}

/*
 * _vte_view_get_char_width:
 * @terminal: a #VteView
 *
 * Returns: the width of a character cell
 */
glong
_vte_view_get_char_width(VteView *terminal)
{
	g_return_val_if_fail(VTE_IS_VIEW(terminal), -1);
	vte_view_ensure_font (terminal);
	return terminal->pvt->char_width;
}

/*
 * _vte_view_get_char_height:
 * @terminal: a #VteView
 *
 * Returns: the height of a character cell
 */
glong
_vte_view_get_char_height(VteView *terminal)
{
	g_return_val_if_fail(VTE_IS_VIEW(terminal), -1);
	vte_view_ensure_font (terminal);
	return terminal->pvt->char_height;
}

/**
 * vte_buffer_get_row_count:
 * @buffer: a #VteBuffer
 *
 * Returns: the number of rows
 */
glong
vte_buffer_get_row_count(VteBuffer *buffer)
{
	g_return_val_if_fail(VTE_IS_BUFFER(buffer), -1);
	return buffer->pvt->row_count;
}

/**
 * vte_buffer_get_column_count:
 * @buffer: a #VteBuffer
 *
 * Returns: the number of columns
 */
glong
vte_buffer_get_column_count(VteBuffer *buffer)
{
	g_return_val_if_fail(VTE_IS_BUFFER(buffer), -1);
	return buffer->pvt->column_count;
}

/**
 * vte_buffer_get_window_title:
 * @buffer: a #VteBuffer
 *
 * Returns: (transfer none): the window title
 */
const char *
vte_buffer_get_window_title(VteBuffer *buffer)
{
	g_return_val_if_fail(VTE_IS_BUFFER(buffer), "");
	return buffer->pvt->window_title;
}

/**
 * vte_buffer_get_icon_title:
 * @buffer: a #VteBuffer
 *
 * Returns: (transfer none): the icon title
 */
const char *
vte_buffer_get_icon_title(VteBuffer *buffer)
{
	g_return_val_if_fail(VTE_IS_BUFFER(buffer), "");
	return buffer->pvt->icon_title;
}

/**
 * vte_buffer_get_current_directory_uri:
 * @buffer: a #VteBuffer
 *
 * Returns: (transfer none): the URI of the current directory of the
 *   process running in the buffer, or %NULL
 */
const char *
vte_buffer_get_current_directory_uri(VteBuffer *buffer)
{
        g_return_val_if_fail(VTE_IS_BUFFER(buffer), NULL);
        return buffer->pvt->current_directory_uri;
}

/**
 * vte_buffer_get_current_file_uri:
 * @buffer: a #VteBuffer
 *
 * Returns: (transfer none): the URI of the current file the
 *   process running in the buffer is operating on, or %NULL if
 *   not set
 */
const char *
vte_buffer_get_current_file_uri(VteBuffer *buffer)
{
        g_return_val_if_fail(VTE_IS_BUFFER(buffer), NULL);
        return buffer->pvt->current_file_uri;
}

/**
 * vte_buffer_set_pty:
 * @terminal: a #VteView
 * @pty: (allow-none): a #VtePty, or %NULL
 *
 * Sets @pty as the PTY to use in @buffer.
 * Use %NULL to unset the PTY.
 */
void
vte_buffer_set_pty(VteBuffer *buffer,
                   VtePty *pty)
{
        VteBufferPrivate *pvt;
        VteView *terminal;
        GObject *object;
        long flags;
        int pty_master;
        GError *error = NULL;

        g_return_if_fail(VTE_IS_BUFFER(buffer));
        g_return_if_fail(pty == NULL || VTE_IS_PTY(pty));

        pvt = buffer->pvt;
        if (pvt->pty == pty)
                return;

        object = G_OBJECT(buffer);
        terminal = pvt->terminal;

        g_object_freeze_notify(object);
        g_object_freeze_notify(G_OBJECT(terminal));

        if (pvt->pty != NULL) {
                _vte_buffer_disconnect_pty_read(buffer);
                _vte_buffer_disconnect_pty_write(buffer);

                if (pvt->pty_channel != NULL) {
                        g_io_channel_unref (pvt->pty_channel);
                        pvt->pty_channel = NULL;
                }

		/* Take one last shot at processing whatever data is pending,
		 * then flush the buffers in case we're about to run a new
		 * command, disconnecting the timeout. */
		if (pvt->incoming != NULL) {
			vte_buffer_process_incoming(buffer);
			_vte_incoming_chunks_release (pvt->incoming);
			pvt->incoming = NULL;
			pvt->input_bytes = 0;
		}
		g_array_set_size(pvt->pending, 0);
		vte_view_stop_processing (terminal);

		/* Clear the outgoing buffer as well. */
		_vte_byte_array_clear(pvt->outgoing);

                vte_pty_close(pvt->pty);
                g_object_unref(pvt->pty);
                pvt->pty = NULL;
        }

        if (pty == NULL) {
                pvt->pty = NULL;
                g_object_notify(object, "pty");
                g_object_thaw_notify(G_OBJECT(terminal));
                g_object_thaw_notify(object);
                return;
        }

        pvt->pty = g_object_ref(pty);
        pty_master = vte_pty_get_fd(pvt->pty);

        pvt->pty_channel = g_io_channel_unix_new (pty_master);
        g_io_channel_set_close_on_unref (pvt->pty_channel, FALSE);

        /* FIXMEchpe: vte_pty_open_unix98 does the inverse ... */
        /* Set the pty to be non-blocking. */
        flags = fcntl(pty_master, F_GETFL);
        if ((flags & O_NONBLOCK) == 0) {
                fcntl(pty_master, F_SETFL, flags | O_NONBLOCK);
        }

        vte_buffer_set_size(buffer, pvt->column_count, pvt->row_count);

        if (!vte_pty_set_utf8(pvt->pty,
                              g_strcmp0(vte_buffer_get_encoding(buffer), "UTF-8") == 0,
                              &error)) {
                g_warning ("Failed to set UTF8 mode: %s\n", error->message);
                g_error_free (error);
        }

        /* Open channels to listen for input on. */
        _vte_buffer_connect_pty_read (buffer);

        g_object_notify(object, "pty");

        g_object_thaw_notify(G_OBJECT(terminal));
        g_object_thaw_notify(object);
}

/**
 * vte_buffer_get_pty:
 * @buffer: a #VteBuffer
 *
 * Returns the #VtePty of @buffer.
 *
 * Returns: (transfer none): a #VtePty, or %NULL
 */
VtePty *
vte_buffer_get_pty(VteBuffer *buffer)
{
        g_return_val_if_fail (VTE_IS_VIEW (buffer), NULL);

        return buffer->pvt->pty;
}

/* We need this bit of glue to ensure that accessible objects will always
 * get signals. */
void
_vte_view_accessible_ref(VteView *terminal)
{
        VteBuffer *buffer;

	g_return_if_fail(VTE_IS_VIEW(terminal));

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return;

	buffer->pvt->accessible_emit = TRUE;
}

char *
_vte_view_get_selection(VteView *terminal)
{
	g_return_val_if_fail(VTE_IS_VIEW(terminal), NULL);

	return g_strdup (terminal->pvt->selection);
}

void
_vte_view_get_start_selection(VteView *terminal, long *col, long *row)
{
	VteVisualPosition ss;

	g_return_if_fail(VTE_IS_VIEW(terminal));

	ss = terminal->pvt->selection_start;

	if (col) {
		*col = ss.col;
	}

	if (row) {
		*row = ss.row;
	}
}

void
_vte_view_get_end_selection(VteView *terminal, long *col, long *row)
{
	VteVisualPosition se;

	g_return_if_fail(VTE_IS_VIEW(terminal));

	se = terminal->pvt->selection_end;

	if (col) {
		*col = se.col;
	}

	if (row) {
		*row = se.row;
	}
}

void
_vte_view_select_text(VteView *terminal,
			  long start_col, long start_row,
			  long end_col, long end_row,
			  int start_offset, int end_offset)
{
	g_return_if_fail(VTE_IS_VIEW(terminal));

	vte_view_deselect_all (terminal);

	terminal->pvt->selection_type = selection_type_char;
	terminal->pvt->selecting_had_delta = TRUE;
	terminal->pvt->selection_start.col = start_col;
	terminal->pvt->selection_start.row = start_row;
	terminal->pvt->selection_end.col = end_col;
	terminal->pvt->selection_end.row = end_row;
	vte_view_emit_copy_primary(terminal);
	vte_view_emit_selection_changed(terminal);

	_vte_invalidate_region (terminal,
			MIN (start_col, end_col), MAX (start_col, end_col),
			MIN (start_row, end_row), MAX (start_row, end_row),
			FALSE);

}

void
_vte_view_remove_selection(VteView *terminal)
{
	vte_view_deselect_all (terminal);
}

static void
_vte_view_select_empty_at(VteView *terminal,
			      long col, long row)
{
	_vte_view_select_text(terminal, col, row, col - 1, row, 0, 0);
}

static void
add_update_timeout (VteView *terminal)
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
reset_update_regions (VteView *terminal)
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
remove_from_active_list (VteView *terminal)
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
remove_update_timeout (VteView *terminal)
{
	reset_update_regions (terminal);
	remove_from_active_list (terminal);
}

static void
vte_view_add_process_timeout (VteView *terminal)
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
vte_view_is_processing (VteView *terminal)
{
	return terminal->pvt->active != NULL;
}
static inline void
vte_view_start_processing (VteView *terminal)
{
	if (!vte_view_is_processing (terminal)) {
		vte_view_add_process_timeout (terminal);
	}
}

static void
vte_view_stop_processing (VteView *terminal)
{
	remove_from_active_list (terminal);
}

static inline gboolean
need_processing (VteView *terminal)
{
        VteBuffer *buffer = terminal->pvt->buffer;
	return _vte_incoming_chunks_length (buffer->pvt->incoming) != 0;
}

/* Emit an "icon-title-changed" signal. */
static void
vte_buffer_emit_icon_title_changed(VteBuffer *buffer)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `icon-title-changed'.\n");
	g_signal_emit(buffer, buffer_signals[BUFFER_ICON_TITLE_CHANGED], 0);
}

/* Emit a "window-title-changed" signal. */
static void
vte_buffer_emit_window_title_changed(VteBuffer *buffer)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `window-title-changed'.\n");
	g_signal_emit(buffer, buffer_signals[BUFFER_WINDOW_TITLE_CHANGED], 0);
}

static void
vte_buffer_emit_current_directory_uri_changed(VteBuffer *buffer)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `current-directory-uri-changed'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_CURRENT_DIRECTORY_URI_CHANGED], 0);
}

static void
vte_buffer_emit_current_file_uri_changed(VteBuffer *buffer)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS,
                        "Emitting `current-file-uri-changed'.\n");
        g_signal_emit(buffer, buffer_signals[BUFFER_CURRENT_FILE_URI_CHANGED], 0);
}

static void
vte_view_emit_pending_signals(VteView *terminal)
{
        VteBuffer *buffer;
        GObject *object, *buffer_object;
	GdkWindow *window;

	object = G_OBJECT (terminal);
	window = gtk_widget_get_window (&terminal->widget);

        buffer = terminal->pvt->buffer;
        buffer_object = G_OBJECT(buffer);

        g_object_freeze_notify(object);
        g_object_freeze_notify(buffer_object);

	vte_view_emit_adjustment_changed (terminal);

	if (buffer->pvt->screen->status_line_changed) {
		_vte_buffer_emit_status_line_changed (buffer);
		buffer->pvt->screen->status_line_changed = FALSE;
	}

	if (buffer->pvt->window_title_changed) {
		g_free (buffer->pvt->window_title);
		buffer->pvt->window_title = buffer->pvt->window_title_changed;
		buffer->pvt->window_title_changed = NULL;

		if (window)
			gdk_window_set_title (window, buffer->pvt->window_title);
		vte_buffer_emit_window_title_changed(buffer);
                g_object_notify(buffer_object, "window-title");
	}

	if (buffer->pvt->icon_title_changed) {
		g_free (buffer->pvt->icon_title);
		buffer->pvt->icon_title = buffer->pvt->icon_title_changed;
		buffer->pvt->icon_title_changed = NULL;

		if (window)
			gdk_window_set_icon_name (window, buffer->pvt->icon_title);
		vte_buffer_emit_icon_title_changed(buffer);
                g_object_notify(buffer_object, "icon-title");
	}

        if (buffer->pvt->current_directory_uri_changed) {
                g_free (buffer->pvt->current_directory_uri);
                buffer->pvt->current_directory_uri = buffer->pvt->current_directory_uri_changed;
                buffer->pvt->current_directory_uri_changed = NULL;

                vte_buffer_emit_current_directory_uri_changed(buffer);
                g_object_notify(buffer_object, "current-directory-uri");
        }

        if (buffer->pvt->current_file_uri_changed) {
                g_free (buffer->pvt->current_file_uri);
                buffer->pvt->current_file_uri = buffer->pvt->current_file_uri_changed;
                buffer->pvt->current_file_uri_changed = NULL;

                vte_buffer_emit_current_file_uri_changed(buffer);
                g_object_notify(buffer_object, "current-file-uri");
        }

	/* Flush any pending "inserted" signals. */
	vte_buffer_emit_cursor_moved(buffer);
	vte_buffer_emit_pending_text_signals(buffer, 0);
	vte_buffer_emit_contents_changed(buffer);

        g_object_thaw_notify(buffer_object);
        g_object_thaw_notify(object);
}

static void time_process_incoming (VteView *terminal)
{
        VteBuffer *buffer = terminal->pvt->buffer;
	gdouble elapsed;
	glong target;
	g_timer_reset (process_timer);
	vte_buffer_process_incoming(buffer);
	elapsed = g_timer_elapsed (process_timer, NULL) * 1000;
	target = VTE_MAX_PROCESS_TIME / elapsed * buffer->pvt->input_bytes;
	buffer->pvt->max_input_bytes =
		(buffer->pvt->max_input_bytes + target) / 2;
}


/* This function is called after DISPLAY_TIMEOUT ms.
 * It makes sure initial output is never delayed by more than DISPLAY_TIMEOUT
 */
static gboolean
process_timeout (gpointer data)
{
	GList *l, *next;
	gboolean again;

	gdk_threads_enter();

	in_process_timeout = TRUE;

	_vte_debug_print (VTE_DEBUG_WORK, "<");
	_vte_debug_print (VTE_DEBUG_TIMEOUT,
			"Process timeout:  %d active\n",
			g_list_length (active_terminals));

	for (l = active_terminals; l != NULL; l = next) {
		VteView *terminal = l->data;
                VteBuffer *buffer = terminal->pvt->buffer;
		gboolean active = FALSE;

		next = g_list_next (l);

		if (l != active_terminals) {
			_vte_debug_print (VTE_DEBUG_WORK, "T");
		}
		if (buffer->pvt->pty_channel != NULL) {
			if (buffer->pvt->pty_input_active ||
                            buffer->pvt->pty_input_source == 0) {
				buffer->pvt->pty_input_active = FALSE;
				vte_buffer_io_read (buffer->pvt->pty_channel,
                                                    G_IO_IN, buffer);
			}
			_vte_buffer_enable_input_source (buffer);
		}
		if (need_processing (terminal)) {
			active = TRUE;
			if (VTE_MAX_PROCESS_TIME) {
				time_process_incoming (terminal);
			} else {
				vte_buffer_process_incoming(buffer);
			}
			buffer->pvt->input_bytes = 0;
		} else
			vte_view_emit_pending_signals (terminal);
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
				"Stoping process timeout\n");
		process_timeout_tag = 0;
		again = FALSE;
	}

	in_process_timeout = FALSE;

	gdk_threads_leave();

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
update_regions (VteView *terminal)
{
	GSList *l;
	cairo_region_t *region;
	GdkWindow *window;

        if (!gtk_widget_get_realized(&terminal->widget) ||
            terminal->pvt->visibility_state == GDK_VISIBILITY_FULLY_OBSCURED) {
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
			cairo_region_union (region, l->data);
			cairo_region_destroy (l->data);
		} while ((l = g_slist_next (l)) != NULL);
	} else {
		region = l->data;
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

	gdk_threads_enter();

	in_update_timeout = TRUE;

	_vte_debug_print (VTE_DEBUG_WORK, "[");
	_vte_debug_print (VTE_DEBUG_TIMEOUT,
			"Repeat timeout:  %d active\n",
			g_list_length (active_terminals));

	for (l = active_terminals; l != NULL; l = next) {
		VteView *terminal = l->data;
                VteBuffer *buffer = terminal->pvt->buffer;

		next = g_list_next (l);

		if (l != active_terminals) {
			_vte_debug_print (VTE_DEBUG_WORK, "T");
		}
		if (buffer->pvt->pty_channel != NULL) {
			if (buffer->pvt->pty_input_active ||
                            buffer->pvt->pty_input_source == 0) {
				buffer->pvt->pty_input_active = FALSE;
				vte_buffer_io_read (buffer->pvt->pty_channel,
						G_IO_IN, buffer);
			}
			_vte_buffer_enable_input_source (buffer);
		}
		vte_view_emit_adjustment_changed (terminal);
		if (need_processing (terminal)) {
			if (VTE_MAX_PROCESS_TIME) {
				time_process_incoming (terminal);
			} else {
				vte_buffer_process_incoming(buffer);
			}
			buffer->pvt->input_bytes = 0;
		} else
			vte_view_emit_pending_signals (terminal);

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
	 * past cycle.
	 */
	again = TRUE;
	if (active_terminals == NULL) {
		_vte_debug_print(VTE_DEBUG_TIMEOUT,
				"Stoping update timeout\n");
		update_timeout_tag = 0;
		again = FALSE;
	}

	in_update_timeout = FALSE;

	gdk_threads_leave();

	if (again) {
		/* Force us to relinquish the CPU as the child is running
		 * at full tilt and making us run to keep up...
		 */
		g_usleep (0);
	} else {
		/* otherwise free up memory used to capture incoming data */
		prune_chunks (10);
	}

	return again;
}

static gboolean
update_timeout (gpointer data)
{
	GList *l, *next;
	gboolean redraw = FALSE;

	gdk_threads_enter();

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
		VteView *terminal = l->data;
                VteBuffer *buffer = terminal->pvt->buffer;

		next = g_list_next (l);

		if (l != active_terminals) {
			_vte_debug_print (VTE_DEBUG_WORK, "T");
		}
		if (buffer->pvt->pty_channel != NULL) {
			if (buffer->pvt->pty_input_active ||
                            buffer->pvt->pty_input_source == 0) {
				buffer->pvt->pty_input_active = FALSE;
				vte_buffer_io_read (buffer->pvt->pty_channel,
						G_IO_IN, buffer);
			}
			_vte_buffer_enable_input_source (buffer);
		}
		vte_view_emit_adjustment_changed (terminal);
		if (need_processing (terminal)) {
			if (VTE_MAX_PROCESS_TIME) {
				time_process_incoming (terminal);
			} else {
				vte_buffer_process_incoming(buffer);
			}
			buffer->pvt->input_bytes = 0;
		} else
			vte_view_emit_pending_signals (terminal);

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

	gdk_threads_leave();

	return FALSE;
}

/**
 * vte_buffer_write_contents_sync:
 * @buffer: a #VteBuffer
 * @stream: a #GOutputStream to write to
 * @flags: a set of #VteBufferWriteFlags
 * @cancellable: (allow-none): a #GCancellable object, or %NULL
 * @error: (allow-none): a #GError location to store the error occuring, or %NULL
 *
 * Write contents of the current contents of @buffer (including any
 * scrollback history) to @stream according to @flags.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by triggering
 * the cancellable object from another thread. If the operation was cancelled,
 * the error %G_IO_ERROR_CANCELLED will be returned in @error.
 *
 * This is a synchronous operation and will make the widget (and input
 * processing) during the write operation, which may take a long time
 * depending on scrollback history and @stream availability for writing.
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 */
gboolean
vte_buffer_write_contents_sync (VteBuffer *buffer,
                                GOutputStream *stream,
                                VteWriteFlags flags,
                                GCancellable *cancellable,
                                GError **error)
{
        g_return_val_if_fail(VTE_IS_BUFFER(buffer), FALSE);
        g_return_val_if_fail(G_IS_OUTPUT_STREAM(stream), FALSE);
        g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	return _vte_ring_write_contents_sync(buffer->pvt->screen->row_data,
                                             stream, flags,
                                             cancellable, error);
}

/*
 * Buffer search
 */

/* TODO Add properties & signals */

/**
 * vte_view_search_set_gregex:
 * @terminal: a #VteView
 * @regex: (allow-none): a #GRegex, or %NULL
 * @flags: flags from #GRegexMatchFlags
 *
 * Sets the #GRegex regex to search for. Unsets the search regex when passed %NULL.
 */
void
vte_view_search_set_gregex (VteView *terminal,
				GRegex      *regex,
                                GRegexMatchFlags flags)
{
        g_return_if_fail(VTE_IS_VIEW(terminal));

	if (terminal->pvt->search_regex == regex)
		return;

	if (terminal->pvt->search_regex) {
		g_regex_unref (terminal->pvt->search_regex);
		terminal->pvt->search_regex = NULL;
	}

	if (regex)
		terminal->pvt->search_regex = g_regex_ref (regex);

        terminal->pvt->search_match_flags = flags;

	_vte_invalidate_all (terminal);
}

/**
 * vte_view_search_get_gregex:
 * @terminal: a #VteView
 *
 * Returns: (transfer none): the search #GRegex regex set in @terminal, or %NULL
 */
GRegex *
vte_view_search_get_gregex (VteView *terminal)
{
	g_return_val_if_fail(VTE_IS_VIEW(terminal), NULL);

	return terminal->pvt->search_regex;
}

/**
 * vte_view_search_set_wrap_around:
 * @terminal: a #VteView
 * @wrap_around: whether search should wrap
 *
 * Sets whether search should wrap around to the beginning of the
 * terminal content when reaching its end.
 */
void
vte_view_search_set_wrap_around (VteView *terminal,
				     gboolean     wrap_around)
{
	g_return_if_fail(VTE_IS_VIEW(terminal));

	terminal->pvt->search_wrap_around = !!wrap_around;
}

/**
 * vte_view_search_get_wrap_around:
 * @terminal: a #VteView
 *
 * Returns: whether searching will wrap around
 */
gboolean
vte_view_search_get_wrap_around (VteView *terminal)
{
	g_return_val_if_fail(VTE_IS_VIEW(terminal), FALSE);

	return terminal->pvt->search_wrap_around;
}

static gboolean
vte_view_search_rows (VteView *terminal,
			  long start_row,
			  long end_row,
			  gboolean backward)
{
        VteViewPrivate *pvt;
        VteBuffer *buffer;
	char *row_text;
	GMatchInfo *match_info;
	GError *error = NULL;
	int start, end;
	long start_col, end_col;
	gchar *word;
	VteCharAttributes *ca;
	GArray *attrs;
	gdouble value, page_size;

	pvt = terminal->pvt;
        buffer = terminal->pvt->buffer;

	row_text = vte_buffer_get_text_range (buffer, start_row, 0, end_row, -1, NULL, NULL, NULL);

	g_regex_match_full (pvt->search_regex, row_text, -1, 0,
                            pvt->search_match_flags | G_REGEX_MATCH_NOTEMPTY,
                            &match_info, &error);
	if (error) {
		g_printerr ("Error while matching: %s\n", error->message);
		g_error_free (error);
		g_match_info_free (match_info);
		g_free (row_text);
		return TRUE;
	}

	if (!g_match_info_matches (match_info)) {
		g_match_info_free (match_info);
		g_free (row_text);
		return FALSE;
	}

	word = g_match_info_fetch (match_info, 0);

	/* Fetch text again, with attributes */
	g_free (row_text);
	if (!pvt->search_attrs)
		pvt->search_attrs = g_array_new (FALSE, TRUE, sizeof (VteCharAttributes));
	attrs = pvt->search_attrs;
	row_text = vte_buffer_get_text_range (buffer, start_row, 0, end_row, -1, NULL, NULL, attrs);

	/* This gives us the offset in the buffer */
	g_match_info_fetch_pos (match_info, 0, &start, &end);

	ca = &g_array_index (attrs, VteCharAttributes, start);
	start_row = ca->row;
	start_col = ca->column;
	ca = &g_array_index (attrs, VteCharAttributes, end - 1);
	end_row = ca->row;
	end_col = ca->column;

	g_free (word);
	g_free (row_text);
	g_match_info_free (match_info);

	_vte_view_select_text (terminal, start_col, start_row, end_col, end_row, 0, 0);
	/* Quite possibly the math here should not access adjustment directly... */
	value = gtk_adjustment_get_value(terminal->pvt->vadjustment);
	page_size = gtk_adjustment_get_page_size(terminal->pvt->vadjustment);
	if (backward) {
		if (end_row < value || end_row >= value + page_size)
			vte_view_queue_adjustment_value_changed_clamped (terminal, end_row - page_size + 1);
	} else {
		if (start_row < value || start_row >= value + page_size)
			vte_view_queue_adjustment_value_changed_clamped (terminal, start_row);
	}

	return TRUE;
}

static gboolean
vte_view_search_rows_iter (VteView *terminal,
			       long start_row,
			       long end_row,
			       gboolean backward)
{
        VteBuffer *buffer = terminal->pvt->buffer;
	const VteRowData *row;
	long iter_start_row, iter_end_row;

	if (backward) {
		iter_start_row = end_row;
		while (iter_start_row > start_row) {
			iter_end_row = iter_start_row;

			do {
				iter_start_row--;
				row = _vte_screen_find_row_data(buffer->pvt->screen, iter_start_row);
			} while (row && row->attr.soft_wrapped);

			if (vte_view_search_rows (terminal, iter_start_row, iter_end_row, backward))
				return TRUE;
		}
	} else {
		iter_end_row = start_row;
		while (iter_end_row < end_row) {
			iter_start_row = iter_end_row;

			do {
				row = _vte_screen_find_row_data(buffer->pvt->screen, iter_end_row);
				iter_end_row++;
			} while (row && row->attr.soft_wrapped);

			if (vte_view_search_rows (terminal, iter_start_row, iter_end_row, backward))
				return TRUE;
		}
	}

	return FALSE;
}

static gboolean
vte_view_search_find (VteView *terminal,
			  gboolean     backward)
{
        VteViewPrivate *pvt;
        VteBuffer *buffer;
        VteScreen *screen;
	long buffer_start_row, buffer_end_row;
	long last_start_row, last_end_row;

	g_return_val_if_fail(VTE_IS_VIEW(terminal), FALSE);

        buffer = terminal->pvt->buffer;
        if (buffer == NULL)
                return FALSE;

        pvt = terminal->pvt;
	if (!pvt->search_regex)
		return FALSE;

        screen = buffer->pvt->screen;

	/* TODO
	 * Currently We only find one result per extended line, and ignore columns
	 * Moreover, the whole search thing is implemented very inefficiently.
	 */

	buffer_start_row = _vte_ring_delta (screen->row_data);
	buffer_end_row = _vte_ring_next (screen->row_data);

	if (pvt->has_selection) {
		last_start_row = pvt->selection_start.row;
		last_end_row = pvt->selection_end.row + 1;
	} else {
		last_start_row = screen->scroll_delta + buffer->pvt->row_count;
		last_end_row = screen->scroll_delta;
	}
	last_start_row = MAX (buffer_start_row, last_start_row);
	last_end_row = MIN (buffer_end_row, last_end_row);

	/* If search fails, we make an empty selection at the last searched
	 * position... */
	if (backward) {
		if (vte_view_search_rows_iter (terminal, buffer_start_row, last_start_row, backward))
			return TRUE;
		if (pvt->search_wrap_around &&
		    vte_view_search_rows_iter (terminal, last_end_row, buffer_end_row, backward))
			return TRUE;
		if (pvt->has_selection) {
			if (pvt->search_wrap_around)
			    _vte_view_select_empty_at (terminal,
							   pvt->selection_start.col,
							   pvt->selection_start.row);
			else
			    _vte_view_select_empty_at (terminal,
							   -1,
							   buffer_start_row - 1);
		}
	} else {
		if (vte_view_search_rows_iter (terminal, last_end_row, buffer_end_row, backward))
			return TRUE;
		if (pvt->search_wrap_around &&
		    vte_view_search_rows_iter (terminal, buffer_start_row, last_start_row, backward))
			return TRUE;
		if (pvt->has_selection) {
			if (pvt->search_wrap_around)
			    _vte_view_select_empty_at (terminal,
							   pvt->selection_end.col + 1,
							   pvt->selection_end.row);
			else
			    _vte_view_select_empty_at (terminal,
							   -1,
							   buffer_end_row);
		}
	}

	return FALSE;
}

/**
 * vte_view_search_find_previous:
 * @terminal: a #VteView
 *
 * Searches the previous string matching the search regex set with
 * vte_view_search_set_gregex().
 *
 * Returns: %TRUE if a match was found
 */
gboolean
vte_view_search_find_previous (VteView *terminal)
{
	return vte_view_search_find (terminal, TRUE);
}

/**
 * vte_view_search_find_next:
 * @terminal: a #VteView
 *
 * Searches the next string matching the search regex set with
 * vte_view_search_set_gregex().
 *
 * Returns: %TRUE if a match was found
 */
gboolean
vte_view_search_find_next (VteView *terminal)
{
	return vte_view_search_find (terminal, FALSE);
}

#define MIN_COLUMNS (8)
#define MIN_ROWS    (1)

/**
 * vte_view_get_geometry_hints:
 * @view: a #VteView
 * @hints: a #GdkGeometry
 * @min_rows: the minimum number of rows to request
 * @min_columns: the minimum number of columns to request
 *
 * Fills in some @hints from @view's geometry. The hints
 * filled are those covered by the %GDK_HINT_RESIZE_INC,
 * %GDK_HINT_MIN_SIZE and %GDK_HINT_BASE_SIZE flags.
 *
 * See gtk_window_set_geometry_hints() for more information.
 *
 * @view must be realized (see gtk_widget_get_realized()).
 */
void
vte_view_get_geometry_hints(VteView *view,
                            GdkGeometry *hints,
                            int min_rows,
                            int min_columns)
{
        VteViewPrivate *pvt;

        g_return_if_fail(VTE_IS_VIEW(view));
        g_return_if_fail(hints != NULL);
        g_return_if_fail(gtk_widget_get_realized(&view->widget));

        pvt = view->pvt;

        hints->base_width  = pvt->padding.left + pvt->padding.right;
        hints->base_height = pvt->padding.top  + pvt->padding.bottom;
        hints->width_inc   = pvt->char_width;
        hints->height_inc  = pvt->char_height;
        hints->min_width   = hints->base_width  + hints->width_inc  * min_columns;
        hints->min_height  = hints->base_height + hints->height_inc * min_rows;
}

/**
 * vte_view_set_window_geometry_hints:
 * @view: a #VteView
 * @window: a #GtkWindow
 *
 * Sets @view as @window's geometry widget. See
 * gtk_window_set_geometry_hints() for more information.
 *
 * @view must be realized (see gtk_widget_get_realized()).
 */
void
vte_view_set_window_geometry_hints(VteView *view,
                                   GtkWindow *window)
{
        GdkGeometry hints;

        g_return_if_fail(VTE_IS_VIEW(view));
        g_return_if_fail(gtk_widget_get_realized(&view->widget));

        vte_view_get_geometry_hints(view, &hints, MIN_ROWS, MIN_COLUMNS);
        gtk_window_set_geometry_hints(window,
                                      &view->widget,
                                      &hints,
                                      GDK_HINT_RESIZE_INC |
                                      GDK_HINT_MIN_SIZE |
                                      GDK_HINT_BASE_SIZE);
}

/**
 * vte_view_iter_from_event:
 * @view: a #VteView
 * @event: a #GdkEvent
 * @iter: (out) (allow-none): a location to store a #VteBufferIter
 *
 * Converts the event coordinates in @event to a #VteBufferIter
 * on @view's buffer.
 *
 * If @event does not have coordinates, or @view has no buffer,
 * or the event coordinates are outside the visible grid,
 * returns %FALSE.
 *
 * Returns: %TRUE iff @iter was filled in
 */
gboolean
vte_view_iter_from_event(VteView *view,
                         GdkEvent *event,
                         VteBufferIter *iter)
{
        VteBufferIterReal *real_iter = (VteBufferIterReal *) iter;
        VteBuffer *buffer;
        gdouble x, y;
        glong row, col;

        g_return_val_if_fail(VTE_IS_VIEW(view), FALSE);
        g_return_val_if_fail(event != NULL, FALSE);

        buffer = view->pvt->buffer;
        if (buffer == NULL)
                return FALSE;

        if (!gdk_event_get_coords(event, &x, &y))
                return FALSE;

        if (!_vte_view_xy_to_grid(view, x, y, &col, &row))
                return FALSE;

        if (iter) {
                _vte_buffer_iter_init(real_iter, buffer);

                real_iter->position.col = col;
                real_iter->position.row = row + buffer->pvt->screen->scroll_delta;
        }

        return TRUE;
}

/**
 * vte_view_iter_is_visible:
 * @view: a #VteView
 * @iter: a valid #VteBufferIter for @view's buffer
 *
 * Returns: %TRUE iff the grid coordinates in @iter are within
 *   the visible grid of @view
 */
gboolean
vte_view_iter_is_visible(VteView *view,
                         const VteBufferIter *iter)
{
        VteBufferIterReal *real_iter = (VteBufferIterReal *) iter;
        VteBuffer *buffer;
        glong row;

        g_return_val_if_fail(VTE_IS_VIEW(view), FALSE);

        buffer = view->pvt->buffer;
        if (vte_buffer_iter_is_valid(iter, buffer))
                return FALSE;

        if (real_iter->screen != buffer->pvt->screen)
                return FALSE;

        row = real_iter->position.row - buffer->pvt->screen->scroll_delta;

        return row >= 0 && row < buffer->pvt->row_count;
}

/* *********
 * VteBuffer
 * *********
 */

/**
 * SECTION: vte-buffer
 * @short_description: FIXME
 *
 * Long description FIXME.
 */

void
_vte_buffer_view_adjust_adjustments(VteBuffer *buffer)
{
        VteBufferPrivate *pvt = buffer->pvt;

        if (pvt->terminal == NULL)
                return;

        _vte_view_adjust_adjustments(pvt->terminal);
}

void
_vte_buffer_view_invalidate_all(VteBuffer *buffer)
{
        VteBufferPrivate *pvt = buffer->pvt;

        if (pvt->terminal == NULL)
                return;

        _vte_invalidate_all(pvt->terminal);
}

void
_vte_buffer_view_invalidate_cells(VteBuffer *buffer,
                                  glong column_start,
                                  gint column_count,
                                  glong row_start,
                                  gint row_count)
{
        VteBufferPrivate *pvt = buffer->pvt;

        if (pvt->terminal == NULL)
                return;

        _vte_invalidate_cells(pvt->terminal, column_start, column_count, row_start, row_count);
}

void
_vte_buffer_view_scroll_region(VteBuffer *buffer,
                               glong row,
                               glong count,
                               glong delta)
{
        VteBufferPrivate *pvt = buffer->pvt;

        if (pvt->terminal == NULL)
                return;

        _vte_view_scroll_region(pvt->terminal, row, count, delta);
}

#ifdef VTE_DEBUG
G_DEFINE_TYPE_WITH_CODE(VteBuffer, vte_buffer, G_TYPE_OBJECT,
                        g_type_add_class_private (g_define_type_id, sizeof (VteBufferClassPrivate));
                        _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_buffer_get_type()\n");
                        )
#else
G_DEFINE_TYPE_WITH_CODE(VteBuffer, vte_buffer, G_TYPE_OBJECT,
                        g_type_add_class_private (g_define_type_id, sizeof (VteBufferClassPrivate));
                        )
#endif

static void
vte_buffer_init(VteBuffer *buffer)
{
        VteBufferPrivate *pvt;

        _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_buffer_init()\n");

        pvt = buffer->pvt = G_TYPE_INSTANCE_GET_PRIVATE (buffer, VTE_TYPE_BUFFER, VteBufferPrivate);

        /* We allocated zeroed memory, just fill in non-zero stuff. */

        /* Initialize the screens and histories. */
        _vte_ring_init (pvt->alternate_screen.row_data, buffer->pvt->row_count);
        pvt->alternate_screen.sendrecv_mode = TRUE;
        pvt->alternate_screen.status_line_contents = g_string_new(NULL);
        _vte_screen_set_default_attributes(&pvt->alternate_screen);

        _vte_ring_init (pvt->normal_screen.row_data,  VTE_SCROLLBACK_INIT);
        pvt->normal_screen.sendrecv_mode = TRUE;
        pvt->normal_screen.status_line_contents = g_string_new(NULL);
        _vte_screen_set_default_attributes(&pvt->normal_screen);

        pvt->screen = &pvt->normal_screen;

        /* Set up I/O encodings. */
        pvt->iso2022 = _vte_iso2022_state_new(pvt->encoding,
                                              &_vte_view_codeset_changed_cb,
                                              buffer);
        pvt->incoming = NULL;
        pvt->pending = g_array_new(FALSE, TRUE, sizeof(gunichar));
        pvt->max_input_bytes = VTE_MAX_INPUT_READ;
        pvt->outgoing = _vte_byte_array_new();
        pvt->outgoing_conv = VTE_INVALID_CONV;
        pvt->conv_buffer = _vte_byte_array_new();
        vte_buffer_set_encoding(buffer, NULL);
        g_assert(buffer->pvt->encoding != NULL);

        /* Load the termcap data and set up the emulation. */
        pvt->keypad_mode = VTE_KEYMODE_NORMAL;
        pvt->cursor_mode = VTE_KEYMODE_NORMAL;
        pvt->dec_saved = g_hash_table_new(NULL, NULL);
        pvt->default_column_count = VTE_COLUMNS;
        pvt->default_row_count = VTE_ROWS;

        /* Setting the terminal type and size requires the PTY master to
         * be set up properly first. */
        pvt->pty = NULL;
        vte_buffer_set_emulation(buffer, NULL);
        vte_buffer_set_size(buffer,
                              pvt->default_column_count,
                              pvt->default_row_count);
        pvt->pty_input_source = 0;
        pvt->pty_output_source = 0;
        pvt->pty_pid = -1;

        pvt->cursor_visible = TRUE;
        pvt->scrollback_lines = -1; /* force update in vte_view_set_scrollback_lines */
        vte_buffer_set_scrollback_lines(buffer, VTE_SCROLLBACK_INIT);

        /* Miscellaneous options. */
        vte_buffer_set_backspace_binding(buffer, VTE_ERASE_AUTO);
        vte_buffer_set_delete_binding(buffer, VTE_ERASE_AUTO);
        pvt->meta_sends_escape = TRUE;
        pvt->nrc_mode = TRUE;
        vte_buffer_set_default_tabstops(buffer);

        /* Cursor */
        pvt->cursor_visible = TRUE;
}

static void
vte_buffer_dispose(GObject *object)
{
        _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_buffer_dispose()\n");

        G_OBJECT_CLASS(vte_buffer_parent_class)->dispose(object);
}

static void
vte_buffer_finalize(GObject *object)
{
        VteBuffer *buffer = VTE_BUFFER(object);

        _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_buffer_finalize()\n");

        /* Tabstop information. */
        if (buffer->pvt->tabstops != NULL) {
                g_hash_table_destroy(buffer->pvt->tabstops);
        }

        /* Clear the output histories. */
        _vte_ring_fini(buffer->pvt->normal_screen.row_data);
        _vte_ring_fini(buffer->pvt->alternate_screen.row_data);

        /* Clear the status lines. */
        g_string_free(buffer->pvt->normal_screen.status_line_contents,
                      TRUE);
        g_string_free(buffer->pvt->alternate_screen.status_line_contents,
                      TRUE);

        /* Free conversion descriptors. */
        if (buffer->pvt->outgoing_conv != VTE_INVALID_CONV) {
                _vte_conv_close(buffer->pvt->outgoing_conv);
                buffer->pvt->outgoing_conv = VTE_INVALID_CONV;
        }

        /* Stop listening for child-exited signals. */
        if (buffer->pvt->child_watch_source != 0) {
                g_source_remove (buffer->pvt->child_watch_source);
                buffer->pvt->child_watch_source = 0;
        }

        /* Stop processing input. */
        // vte_view_stop_processing (terminal);

        /* Discard any pending data. */
        _vte_incoming_chunks_release (buffer->pvt->incoming);
        _vte_byte_array_free(buffer->pvt->outgoing);
        g_array_free(buffer->pvt->pending, TRUE);
        _vte_byte_array_free(buffer->pvt->conv_buffer);

        /* Stop the child and stop watching for input from the child. */
        if (buffer->pvt->pty_pid != -1) {
#ifdef HAVE_GETPGID
                pid_t pgrp;
                pgrp = getpgid(buffer->pvt->pty_pid);
                if (pgrp != -1) {
                        kill(-pgrp, SIGHUP);
                }
#endif
                kill(buffer->pvt->pty_pid, SIGHUP);
        }
        _vte_buffer_disconnect_pty_read(buffer);
        _vte_buffer_disconnect_pty_write(buffer);
        if (buffer->pvt->pty_channel != NULL) {
                g_io_channel_unref (buffer->pvt->pty_channel);
        }
        if (buffer->pvt->pty != NULL) {
                vte_pty_close(buffer->pvt->pty);
                g_object_unref(buffer->pvt->pty);
        }

        /* Remove hash tables. */
        if (buffer->pvt->dec_saved != NULL) {
                g_hash_table_destroy(buffer->pvt->dec_saved);
        }

        /* Clean up emulation structures. */
        if (buffer->pvt->matcher != NULL) {
                _vte_matcher_free(buffer->pvt->matcher);
        }
        if (buffer->pvt->termcap != NULL) {
                _vte_termcap_free(buffer->pvt->termcap);
        }

        // remove_update_timeout (terminal);

        /* discard title updates */
        g_free(buffer->pvt->window_title);
        g_free(buffer->pvt->icon_title);
        g_free(buffer->pvt->window_title_changed);
        g_free(buffer->pvt->icon_title_changed);
        g_free(buffer->pvt->current_directory_uri_changed);
        g_free(buffer->pvt->current_directory_uri);
        g_free(buffer->pvt->current_file_uri_changed);
        g_free(buffer->pvt->current_file_uri);

        G_OBJECT_CLASS(vte_buffer_parent_class)->finalize(object);
}

static void
vte_buffer_get_property (GObject *object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
        VteBuffer *buffer = VTE_BUFFER(object);

        switch (prop_id) {
        case BUFFER_PROP_BACKSPACE_BINDING:
                g_value_set_enum(value, buffer->pvt->backspace_binding);
                break;
        case BUFFER_PROP_CURRENT_DIRECTORY_URI:
                g_value_set_string (value, vte_buffer_get_current_directory_uri (buffer));
                break;
        case BUFFER_PROP_CURRENT_FILE_URI:
                g_value_set_string (value, vte_buffer_get_current_file_uri (buffer));
                break;
        case BUFFER_PROP_DELETE_BINDING:
                g_value_set_enum(value, buffer->pvt->delete_binding);
                break;
        case BUFFER_PROP_EMULATION:
                g_value_set_string (value, vte_buffer_get_emulation(buffer));
                break;
        case BUFFER_PROP_ENCODING:
                g_value_set_string(value, vte_buffer_get_encoding(buffer));
                break;
        case BUFFER_PROP_SCROLLBACK_LINES:
                g_value_set_uint (value, buffer->pvt->scrollback_lines);
                break;
        case BUFFER_PROP_ICON_TITLE:
                g_value_set_string(value, vte_buffer_get_icon_title(buffer));
                break;
        case BUFFER_PROP_WINDOW_TITLE:
                g_value_set_string(value, vte_buffer_get_window_title(buffer));
                break;
        case BUFFER_PROP_PTY:
                g_value_set_object(value, vte_buffer_get_pty(buffer));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                return;
        }
}

static void
vte_buffer_set_property (GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
        VteBuffer *buffer = VTE_BUFFER(object);

        switch (prop_id) {
        case BUFFER_PROP_BACKSPACE_BINDING:
                vte_buffer_set_backspace_binding(buffer, g_value_get_enum (value));
                break;
        case BUFFER_PROP_DELETE_BINDING:
                vte_buffer_set_delete_binding(buffer, g_value_get_enum (value));
                break;
        case BUFFER_PROP_EMULATION:
                vte_buffer_set_emulation(buffer, g_value_get_string (value));
                break;
        case BUFFER_PROP_ENCODING:
                vte_buffer_set_encoding(buffer, g_value_get_string (value));
                break;
        case BUFFER_PROP_SCROLLBACK_LINES:
                vte_buffer_set_scrollback_lines (buffer, g_value_get_uint (value));
                break;
        case BUFFER_PROP_PTY:
                vte_buffer_set_pty(buffer, g_value_get_object (value));
                break;
        /* Not writable */
        case BUFFER_PROP_CURRENT_DIRECTORY_URI:
        case BUFFER_PROP_CURRENT_FILE_URI:
        case BUFFER_PROP_ICON_TITLE:
        case BUFFER_PROP_WINDOW_TITLE:
                g_assert_not_reached ();
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                return;
        }
}

static void
vte_buffer_class_init(VteBufferClass *klass)
{
        GObjectClass *gobject_class = &klass->object_class;

        _vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_buffer_class_init()\n");

        g_type_class_add_private(klass, sizeof (VteBufferPrivate));

        gobject_class->dispose = vte_buffer_dispose;
        gobject_class->finalize = vte_buffer_finalize;
        gobject_class->get_property = vte_buffer_get_property;
        gobject_class->set_property = vte_buffer_set_property;

        klass->child_exited = NULL;
        klass->commit = NULL;
        klass->emulation_changed = NULL;
        klass->encoding_changed = NULL;
        klass->eof = NULL;
        klass->window_title_changed = NULL;
        klass->icon_title_changed = NULL;
        klass->status_line_changed = NULL;
        klass->deiconify_window = NULL;
        klass->iconify_window = NULL;
        klass->raise_window = NULL;
        klass->lower_window = NULL;
        klass->refresh_window = NULL;
        klass->restore_window = NULL;
        klass->maximize_window = NULL;
        klass->resize_window = NULL;
        klass->move_window = NULL;
        klass->cursor_moved = NULL;
        klass->text_modified = NULL;
        klass->text_inserted = NULL;
        klass->text_deleted = NULL;
        klass->contents_changed = NULL;
        klass->bell = NULL;

        /**
         * VteBuffer::child-exited:
         * @buffer: the object which received the signal
         * @status: the child's exit status
         *
         * This signal is emitted when the buffer detects that a child
         * watched using vte_buffer_watch_child() has exited.
         */
        buffer_signals[BUFFER_CHILD_EXITED] =
                g_signal_new(I_("child-exited"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, child_exited),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__INT,
                             G_TYPE_NONE,
                             1, G_TYPE_INT);

        /**
         * VteBuffer::commit:
         * @buffer: the object which received the signal
         * @text: a string of text
         * @size: the length of that string of text
         *
         * Emitted whenever the terminal receives input from the user and
         * prepares to send it to the child process.  The signal is emitted even
         * when there is no child process.
         */
        buffer_signals[BUFFER_COMMIT] =
                g_signal_new(I_("commit"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, commit),
                             NULL,
                             NULL,
                             _vte_marshal_VOID__STRING_UINT,
                             G_TYPE_NONE,
                             2, G_TYPE_STRING, G_TYPE_UINT);

        /**
          * VteBuffer::current-directory-uri-changed:
          * @buffer: the object which received the signal
          *
          * Emitted when the current directory URI is modified.
          */
        buffer_signals[BUFFER_CURRENT_DIRECTORY_URI_CHANGED] =
                g_signal_new(I_("current-directory-uri-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, current_directory_uri_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
          * VteBuffer::current-file-uri-changed:
          * @buffer: the object which received the signal
          *
          * Emitted when the current file URI is modified.
          */
        buffer_signals[BUFFER_CURRENT_FILE_URI_CHANGED] =
                g_signal_new(I_("current-file-uri-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, current_file_uri_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteView::emulation-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the terminal's emulation changes, only possible at
         * the parent application's request.
         */
        buffer_signals[BUFFER_EMULATION_CHANGED] =
                g_signal_new(I_("emulation-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, emulation_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteView::encoding-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the terminal's current encoding has changed, either
         * as a result of receiving a control sequence which toggled between the
         * local and UTF-8 encodings, or at the parent application's request.
         */
        buffer_signals[BUFFER_ENCODING_CHANGED] =
                g_signal_new(I_("encoding-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, encoding_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::eof:
         * @vtebuffer: the object which received the signal
         *
         * Emitted when the buffer receives an end-of-file from a child which
         * is running in the buffer.  This signal is frequently (but not
         * always) emitted with a #VteBuffer::child-exited signal.
         */
        buffer_signals[BUFFER_EOF] =
                g_signal_new(I_("eof"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, eof),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::window-title-changed:
         * @vtebuffer: the object which received the signal
         *
         * Emitted when the buffer's %window_title field is modified.
         */
        buffer_signals[BUFFER_WINDOW_TITLE_CHANGED] =
                g_signal_new(I_("window-title-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, window_title_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::icon-title-changed:
         * @vtebuffer: the object which received the signal
         *
         * Emitted when the buffer's %icon_title field is modified.
         */
        buffer_signals[BUFFER_ICON_TITLE_CHANGED] =
                g_signal_new(I_("icon-title-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, icon_title_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::status-line-changed:
         * @vtebuffer: the object which received the signal
         *
         * Emitted whenever the contents of the status line are modified or
         * cleared.
         */
        buffer_signals[BUFFER_STATUS_LINE_CHANGED] =
                g_signal_new(I_("status-line-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, status_line_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::deiconify-window:
         * @vtebuffer: the object which received the signal
         *
         * Emitted at the child application's request.
         */
        buffer_signals[BUFFER_DEICONIFY_WINDOW] =
                g_signal_new(I_("deiconify-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, deiconify_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::iconify-window:
         * @vtebuffer: the object which received the signal
         *
         * Emitted at the child application's request.
         */
        buffer_signals[BUFFER_ICONIFY_WINDOW] =
                g_signal_new(I_("iconify-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, iconify_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::raise-window:
         * @vtebuffer: the object which received the signal
         *
         * Emitted at the child application's request.
         */
        buffer_signals[BUFFER_RAISE_WINDOW] =
                g_signal_new(I_("raise-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, raise_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::cursor-moved:
         * @vtebuffer: the object which received the signal
         *
         * Emitted whenever the cursor moves to a new character cell.  Used
         * primarily by #VteBufferAccessible.
         */
        buffer_signals[BUFFER_CURSOR_MOVED] =
                g_signal_new(I_("cursor-moved"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, cursor_moved),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::lower-window:
         * @vtebuffer: the object which received the signal
         *
         * Emitted at the child application's request.
         */
        buffer_signals[BUFFER_LOWER_WINDOW] =
                g_signal_new(I_("lower-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, lower_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::refresh-window:
         * @vtebuffer: the object which received the signal
         *
         * Emitted at the child application's request.
         */
        buffer_signals[BUFFER_REFRESH_WINDOW] =
                g_signal_new(I_("refresh-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, refresh_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::restore-window:
         * @vtebuffer: the object which received the signal
         *
         * Emitted at the child application's request.
         */
        buffer_signals[BUFFER_RESTORE_WINDOW] =
                g_signal_new(I_("restore-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, restore_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::maximize-window:
         * @vtebuffer: the object which received the signal
         *
         * Emitted at the child application's request.
         */
        buffer_signals[BUFFER_MAXIMIZE_WINDOW] =
                g_signal_new(I_("maximize-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, maximize_window),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::resize-window:
         * @vtebuffer: the object which received the signal
         * @width: the desired number of columns
         * @height: the desired number of rows
         *
         * Emitted at the child application's request.
         */
        buffer_signals[BUFFER_RESIZE_WINDOW] =
                g_signal_new(I_("resize-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, resize_window),
                             NULL,
                             NULL,
                             _vte_marshal_VOID__UINT_UINT,
                             G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

        /**
         * VteBuffer::move-window:
         * @vtebuffer: the object which received the signal
         * @x: the buffer's desired location, X coordinate
         * @y: the buffer's desired location, Y coordinate
         *
         * Emitted at the child application's request.
         */
        buffer_signals[BUFFER_MOVE_WINDOW] =
                g_signal_new(I_("move-window"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, move_window),
                             NULL,
                             NULL,
                             _vte_marshal_VOID__UINT_UINT,
                             G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

        /**
         * VteBuffer::text-modified:
         * @vtebuffer: the object which received the signal
         *
         * An internal signal used for communication between the buffer and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
        buffer_signals[BUFFER_TEXT_MODIFIED] =
                g_signal_new(I_("text-modified"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, text_modified),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::text-inserted:
         * @vtebuffer: the object which received the signal
         *
         * An internal signal used for communication between the buffer and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
        buffer_signals[BUFFER_TEXT_INSERTED] =
                g_signal_new(I_("text-inserted"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, text_inserted),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::text-deleted:
         * @vtebuffer: the object which received the signal
         *
         * An internal signal used for communication between the buffer and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
        buffer_signals[BUFFER_TEXT_DELETED] =
                g_signal_new(I_("text-deleted"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, text_deleted),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::contents-changed:
         * @buffer: the object which received the signal
         *
         * Emitted whenever the visible appearance of the buffer has changed.
         */
        buffer_signals[BUFFER_CONTENTS_CHANGED] =
                g_signal_new(I_("contents-changed"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, contents_changed),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__VOID,
                             G_TYPE_NONE, 0);

        /**
         * VteBuffer::bell:
         * @vtebuffer: the object which received the signal
         *
         * This signal is emitted when the a child sends a bell request to the
         * buffer.
         */
        buffer_signals[BUFFER_BELL] =
                g_signal_new(I_("bell"),
                             G_OBJECT_CLASS_TYPE(klass),
                             G_SIGNAL_RUN_LAST,
                             G_STRUCT_OFFSET(VteBufferClass, bell),
                             NULL,
                             NULL,
                             g_cclosure_marshal_VOID__ENUM,
                             G_TYPE_NONE,
                             1, VTE_TYPE_BELL_TYPE);

        /* Properties */

        /**
         * VteBuffer:backspace-binding:
         *
         * Controls what string or control sequence the terminal sends to its child
         * when the user presses the backspace key.
         */
        g_object_class_install_property
                (gobject_class,
                 BUFFER_PROP_BACKSPACE_BINDING,
                 g_param_spec_enum ("backspace-binding", NULL, NULL,
                                    VTE_TYPE_ERASE_BINDING,
                                    VTE_ERASE_AUTO,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
          * VteBuffer:current-directory-uri:
          *
          * The current directory URI, or %NULL if unset.
          */
        g_object_class_install_property
                (gobject_class,
                 BUFFER_PROP_CURRENT_DIRECTORY_URI,
                 g_param_spec_string ("current-directory-uri", NULL, NULL,
                                      NULL,
                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

        /**
          * VteBuffer:current-file-uri:
          *
          * The current file URI, or %NULL if unset.
          */
        g_object_class_install_property
                (gobject_class,
                 BUFFER_PROP_CURRENT_FILE_URI,
                 g_param_spec_string ("current-file-uri", NULL, NULL,
                                      NULL,
                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

        /**
         * VteBuffer:delete-binding:
         *
         * Controls what string or control sequence the terminal sends to its child
         * when the user presses the delete key.
         */
        g_object_class_install_property
                (gobject_class,
                 BUFFER_PROP_DELETE_BINDING,
                 g_param_spec_enum ("delete-binding", NULL, NULL,
                                    VTE_TYPE_ERASE_BINDING,
                                    VTE_ERASE_AUTO,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


        /**
         * VteBuffer:emulation:
         *
         * Sets what type of buffer the widget attempts to emulate by scanning for
         * control sequences defined in the system's termcap file.  Unless you
         * are interested in this feature, always use the default which is "xterm".
         */
        g_object_class_install_property
                (gobject_class,
                 BUFFER_PROP_EMULATION,
                 g_param_spec_string ("emulation", NULL, NULL,
                                      VTE_DEFAULT_EMULATION,
                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
         * VteBuffer:encoding:
         *
         * Controls the encoding the buffer will expect data from the child to
         * be encoded with.  For certain buffer types, applications executing in the
         * buffer can change the encoding.  The default is defined by the
         * application's locale settings.
         */
        g_object_class_install_property
                (gobject_class,
                 BUFFER_PROP_ENCODING,
                 g_param_spec_string ("encoding", NULL, NULL,
                                      NULL,
                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
        /**
         * VteBuffer:scrollback-lines:
         *
         * The length of the scrollback buffer used by the terminal.  The size of
         * the scrollback buffer will be set to the larger of this value and the number
         * of visible rows the widget can display, so 0 can safely be used to disable
         * scrollback.  Note that this setting only affects the normal screen buffer.
         * For terminal types which have an alternate screen buffer, no scrollback is
         * allowed on the alternate screen buffer.
         */
        g_object_class_install_property
                (gobject_class,
                 BUFFER_PROP_SCROLLBACK_LINES,
                 g_param_spec_uint ("scrollback-lines", NULL, NULL,
                                    0, G_MAXUINT,
                                    VTE_SCROLLBACK_INIT,
                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        /**
         * VteBuffer:icon-title:
         *
         * The buffer's so-called icon title, or %NULL if no icon title has been set.
         */
        g_object_class_install_property
                (gobject_class,
                 BUFFER_PROP_ICON_TITLE,
                 g_param_spec_string ("icon-title", NULL, NULL,
                                      NULL,
                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

        /**
         * VteBuffer:window-title:
         *
         * The buffer's title.
         */
        g_object_class_install_property
                (gobject_class,
                 BUFFER_PROP_WINDOW_TITLE,
                 g_param_spec_string ("window-title", NULL, NULL,
                                      NULL,
                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));


        /**
         * VteBuffer:pty-object:
         *
         * The PTY object for the buffer.
         */
        g_object_class_install_property
                (gobject_class,
                 BUFFER_PROP_PTY,
                 g_param_spec_object ("pty", NULL, NULL,
                                      VTE_TYPE_PTY,
                                      G_PARAM_READWRITE |
                                      G_PARAM_STATIC_STRINGS));

}

/**
 * vte_buffer_new:
 *
 * Returns: (transfer full): a new #VteBuffer
 */
VteBuffer *
vte_buffer_new(void)
{
        return g_object_new(VTE_TYPE_BUFFER, NULL);
}

/* VteBufferIter */

G_DEFINE_BOXED_TYPE(VteBufferIter, vte_buffer_iter,
                    vte_buffer_iter_copy,
                    vte_buffer_iter_free);

void
_vte_buffer_iter_init(VteBufferIterReal *real_iter,
                      VteBuffer *buffer)
{
        g_return_if_fail(real_iter != NULL);

        real_iter->buffer = buffer;
        real_iter->screen = buffer->pvt->screen;
        memset(&real_iter->position, 0, sizeof(real_iter->position));
}

void
_vte_buffer_iter_get_position(VteBufferIter *iter,
                              glong *row,
                              glong *column)
{
        VteBufferIterReal *real_iter = (VteBufferIterReal *) iter;

        g_return_if_fail(iter != NULL);

        if (row)
                *row = real_iter->position.row;
        if (column)
                *column = real_iter->position.col;
}

/**
 * vte_buffer_iter_copy:
 * @iter: a #VteBufferIter
 *
 * Creates a copy of @iter on the heap.
 *
 * Returns: (transfer full): a copy of @iter
 */
VteBufferIter *
vte_buffer_iter_copy (VteBufferIter *iter)
{
        VteBufferIterReal *real_iter = (VteBufferIterReal *) iter;
        VteBufferIterReal *copy;

        g_return_val_if_fail(iter != NULL, NULL);
        copy = g_slice_dup(VteBufferIterReal, real_iter);

        return (VteBufferIter *) copy;
}

/**
 * vte_buffer_iter_copy:
 * @iter: a #VteBufferIter
 *
 * Frees the heap-allocated @iter. Do not use this with stack-allocated #VteBufferIters!
 */
void
vte_buffer_iter_free (VteBufferIter *iter)
{
        VteBufferIterReal *real_iter = (VteBufferIterReal *) iter;

        if (iter == NULL)
                return;

        g_slice_free(VteBufferIterReal, real_iter);
}

/**
 * vte_buffer_iter_is_valid:
 * @iter: a #VteBufferIter
 * @buffer: a #VteBuffer
 *
 * Checks whether @iter is a valid iter on @buffer.
 *
 * Returns: %TRUE iff @iter is a valid iter on @buffer
 */
gboolean
vte_buffer_iter_is_valid (const VteBufferIter *iter,
                          VteBuffer *buffer)
{
        VteBufferIterReal *real_iter = (VteBufferIterReal *) iter;

        if (iter == NULL)
                return FALSE;

        if (real_iter->buffer != buffer)
                return FALSE;

        return TRUE;
}
