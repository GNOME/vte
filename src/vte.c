/*
 * Copyright (C) 2001-2004 Red Hat, Inc.
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include "iso2022.h"
#include "keymap.h"
#include "marshal.h"
#include "matcher.h"
#include "pty.h"
#include "vteaccess.h"
#include "vteint.h"
#include "vteregex.h"
#include "vtetc.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifndef HAVE_WINT_T
typedef gunichar wint_t;
#endif

#ifndef howmany
#define howmany(x, y) (((x) + ((y) - 1)) / (y))
#endif

#define STATIC_PARAMS (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)


static void vte_terminal_set_visibility (VteTerminal *terminal, GdkVisibilityState state);
static void vte_terminal_set_termcap(VteTerminal *terminal, const char *path,
				     gboolean reset);
static void vte_terminal_paste(VteTerminal *terminal, GdkAtom board);
static void vte_terminal_real_copy_clipboard(VteTerminal *terminal);
static void vte_terminal_real_paste_clipboard(VteTerminal *terminal);
static gboolean vte_terminal_io_read(GIOChannel *channel,
				     GIOCondition condition,
				     VteTerminal *terminal);
static gboolean vte_terminal_io_write(GIOChannel *channel,
				      GIOCondition condition,
				      VteTerminal *terminal);
static void vte_terminal_match_hilite_clear(VteTerminal *terminal);
static void vte_terminal_match_hilite_hide(VteTerminal *terminal);
static void vte_terminal_match_hilite_show(VteTerminal *terminal, long x, long y);
static void vte_terminal_match_hilite_update(VteTerminal *terminal, long x, long y);
static void vte_terminal_match_contents_clear(VteTerminal *terminal);
static gboolean vte_terminal_background_update(VteTerminal *data);
static void vte_terminal_queue_background_update(VteTerminal *terminal);
static void vte_terminal_process_incoming(VteTerminal *terminal);
static void vte_terminal_emit_pending_signals(VteTerminal *terminal);
static gboolean vte_cell_is_selected(VteTerminal *terminal,
				     glong col, glong row, gpointer data);
static char *vte_terminal_get_text_range_maybe_wrapped(VteTerminal *terminal,
						       glong start_row,
						       glong start_col,
						       glong end_row,
						       glong end_col,
						       gboolean wrap,
						       VteSelectionFunc is_selected,
						       gpointer data,
						       GArray *attributes,
						       gboolean include_trailing_spaces);
static char *vte_terminal_get_text_maybe_wrapped(VteTerminal *terminal,
						 gboolean wrap,
						 VteSelectionFunc is_selected,
						 gpointer data,
						 GArray *attributes,
						 gboolean include_trailing_spaces);
static void _vte_terminal_disconnect_pty_read(VteTerminal *terminal);
static void _vte_terminal_disconnect_pty_write(VteTerminal *terminal);
static void vte_terminal_stop_processing (VteTerminal *terminal);

static inline gboolean vte_terminal_is_processing (VteTerminal *terminal);
static inline void vte_terminal_start_processing (VteTerminal *terminal);
static void vte_terminal_add_process_timeout (VteTerminal *terminal);
static void add_update_timeout (VteTerminal *terminal);
static void remove_update_timeout (VteTerminal *terminal);
static void reset_update_regions (VteTerminal *terminal);
static void vte_terminal_set_cursor_blinks_internal(VteTerminal *terminal, gboolean blink);
static void vte_terminal_set_font_full_internal(VteTerminal *terminal,
                                                const PangoFontDescription *font_desc,
                                                VteTerminalAntiAlias antialias);
static void vte_terminal_set_selection_block_mode (VteTerminal *terminal,
						   gboolean     selection_block_mode);

static gboolean process_timeout (gpointer data);
static gboolean update_timeout (gpointer data);

enum {
    COPY_CLIPBOARD,
    PASTE_CLIPBOARD,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

enum {
        PROP_0,
        PROP_ALLOW_BOLD,
        PROP_AUDIBLE_BELL,
        PROP_BACKGROUND_IMAGE_FILE,
        PROP_BACKGROUND_IMAGE_PIXBUF,
        PROP_BACKGROUND_OPACITY,
        PROP_BACKGROUND_SATURATION,
        PROP_BACKGROUND_TINT_COLOR,
        PROP_BACKGROUND_TRANSPARENT,
        PROP_BACKSPACE_BINDING,
        PROP_CURSOR_BLINK_MODE,
        PROP_CURSOR_SHAPE,
        PROP_DELETE_BINDING,
        PROP_EMULATION,
        PROP_ENCODING,
        PROP_FONT_DESC,
        PROP_ICON_TITLE,
        PROP_MOUSE_POINTER_AUTOHIDE,
        PROP_PTY,
        PROP_SCROLL_BACKGROUND,
        PROP_SCROLLBACK_LINES,
        PROP_SCROLL_ON_KEYSTROKE,
        PROP_SCROLL_ON_OUTPUT,
        PROP_WINDOW_TITLE,
        PROP_WORD_CHARS,
        PROP_VISIBLE_BELL
};

/* these static variables are guarded by the GDK mutex */
static guint process_timeout_tag = 0;
static gboolean in_process_timeout;
static guint update_timeout_tag = 0;
static gboolean in_update_timeout;
static GList *active_terminals;
static GTimer *process_timer;

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
G_DEFINE_TYPE_WITH_CODE(VteTerminal, vte_terminal, GTK_TYPE_WIDGET,
		if (_vte_debug_on(VTE_DEBUG_LIFECYCLE)) {
			g_printerr("vte_terminal_get_type()\n");
		})
#else
G_DEFINE_TYPE(VteTerminal, vte_terminal, GTK_TYPE_WIDGET)
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


/* Reset defaults for character insertion. */
void
_vte_terminal_set_default_attributes(VteTerminal *terminal)
{
	VteScreen *screen;

	screen = terminal->pvt->screen;

	screen->defaults = basic_cell.cell;
	screen->color_defaults = screen->defaults;
	screen->fill_defaults = screen->defaults;
}

/* Cause certain cells to be repainted. */
void
_vte_invalidate_cells(VteTerminal *terminal,
		      glong column_start, gint column_count,
		      glong row_start, gint row_count)
{
	GdkRectangle rect;
	glong i;

	if (!column_count || !row_count) {
		return;
	}

	if (G_UNLIKELY (!GTK_WIDGET_DRAWABLE(terminal) ||
				terminal->pvt->invalidated_all)) {
		return;
	}

	_vte_debug_print (VTE_DEBUG_UPDATES,
			"Invalidating cells at (%ld,%ld+%ld)x(%d,%d).\n",
			column_start, row_start,
			(long)terminal->pvt->screen->scroll_delta,
			column_count, row_count);
	_vte_debug_print (VTE_DEBUG_WORK, "?");

	/* Subtract the scrolling offset from the row start so that the
	 * resulting rectangle is relative to the visible portion of the
	 * buffer. */
	row_start -= terminal->pvt->screen->scroll_delta;

	/* Ensure the start of region is on screen */
	if (column_start > terminal->column_count ||
			row_start > terminal->row_count) {
		return;
	}

	/* Clamp the start values to reasonable numbers. */
	i = row_start + row_count;
	row_start = MAX (0, row_start);
	row_count = CLAMP (i - row_start, 0, terminal->row_count);

	i = column_start + column_count;
	column_start = MAX (0, column_start);
	column_count = CLAMP (i - column_start, 0 , terminal->column_count);

	if (!column_count || !row_count) {
		return;
	}
	if (column_count == terminal->column_count &&
			row_count == terminal->row_count) {
		_vte_invalidate_all (terminal);
		return;
	}

	/* Convert the column and row start and end to pixel values
	 * by multiplying by the size of a character cell.
	 * Always include the extra pixel border and overlap pixel.
	 */
	rect.x = column_start * terminal->char_width - 1;
	if (column_start != 0) {
		rect.x += VTE_PAD_WIDTH;
	}
	rect.width = (column_start + column_count) * terminal->char_width + 3 + VTE_PAD_WIDTH;
	if (column_start + column_count == terminal->column_count) {
		rect.width += VTE_PAD_WIDTH;
	}
	rect.width -= rect.x;

	rect.y = row_start * terminal->char_height - 1;
	if (row_start != 0) {
		rect.y += VTE_PAD_WIDTH;
	}
	rect.height = (row_start + row_count) * terminal->char_height + 2 + VTE_PAD_WIDTH;
	if (row_start + row_count == terminal->row_count) {
		rect.height += VTE_PAD_WIDTH;
	}
	rect.height -= rect.y;

	_vte_debug_print (VTE_DEBUG_UPDATES,
			"Invalidating pixels at (%d,%d)x(%d,%d).\n",
			rect.x, rect.y, rect.width, rect.height);

	if (terminal->pvt->active != NULL) {
		terminal->pvt->update_regions = g_slist_prepend (
				terminal->pvt->update_regions,
				gdk_region_rectangle (&rect));
		/* Wait a bit before doing any invalidation, just in
		 * case updates are coming in really soon. */
		add_update_timeout (terminal);
	} else {
		gdk_window_invalidate_rect (terminal->widget.window,
				&rect, FALSE);
	}

	_vte_debug_print (VTE_DEBUG_WORK, "!");
}

static void
_vte_invalidate_region (VteTerminal *terminal,
			glong scolumn, glong ecolumn,
			glong srow, glong erow,
			gboolean block)
{
	if (block || srow == erow) {
		_vte_invalidate_cells(terminal,
				scolumn, ecolumn - scolumn + 1,
				srow, erow - srow + 1);
	} else {
		_vte_invalidate_cells(terminal,
				scolumn,
				terminal->column_count - scolumn,
				srow, 1);
		_vte_invalidate_cells(terminal,
				0, terminal->column_count,
				srow + 1, erow - srow - 1);
		_vte_invalidate_cells(terminal,
				0, ecolumn + 1,
				erow, 1);
	}
}


/* Redraw the entire visible portion of the window. */
void
_vte_invalidate_all(VteTerminal *terminal)
{
	GdkRectangle rect;

	g_assert(VTE_IS_TERMINAL(terminal));

	if (!GTK_WIDGET_DRAWABLE(terminal)) {
		return;
	}
	if (terminal->pvt->invalidated_all) {
		return;
	}

	_vte_debug_print (VTE_DEBUG_WORK, "*");
	_vte_debug_print (VTE_DEBUG_UPDATES, "Invalidating all.\n");

	/* replace invalid regions with one covering the whole terminal */
	reset_update_regions (terminal);
	rect.x = rect.y = 0;
	rect.width = terminal->widget.allocation.width;
	rect.height = terminal->widget.allocation.height;
	terminal->pvt->invalidated_all = TRUE;

	if (terminal->pvt->active != NULL) {
		terminal->pvt->update_regions = g_slist_prepend (NULL,
				gdk_region_rectangle (&rect));
		/* Wait a bit before doing any invalidation, just in
		 * case updates are coming in really soon. */
		add_update_timeout (terminal);
	} else {
		gdk_window_invalidate_rect (terminal->widget.window,
				&rect, FALSE);
	}
}


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

	if (terminal->pvt->scroll_background || count >= terminal->row_count) {
		/* We have to repaint the entire window. */
		_vte_invalidate_all(terminal);
	} else {
		/* We have to repaint the area which is to be
		 * scrolled. */
		_vte_invalidate_cells(terminal,
				     0, terminal->column_count,
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
		while (cell != NULL && cell->attr.fragment && col > 0) {
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
		while (cell != NULL && cell->attr.fragment && col > 0) {
			cell = _vte_row_data_get (row_data, --col);
		}
		if (cell) {
			columns = cell->attr.columns - 1;
		}
	}
	return MIN(col + columns, terminal->column_count);
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
			ret += _vte_iso2022_unichar_width(terminal->pvt->iso2022, c);
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
	const VteRowData *row_data;
	int columns;

	if (G_UNLIKELY (!GTK_WIDGET_DRAWABLE(terminal) || terminal->pvt->invalidated_all)) {
		return;
	}

	columns = 1;
	row_data = _vte_terminal_find_row_data(terminal, row);
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
					terminal->char_width * columns) {
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
_vte_invalidate_cursor_once(VteTerminal *terminal, gboolean periodic)
{
	VteScreen *screen;
	const VteCell *cell;
	gssize preedit_width;
	glong column, row;
	gint columns;

	if (terminal->pvt->invalidated_all) {
		return;
	}

	if (periodic) {
		if (!terminal->pvt->cursor_blinks) {
			return;
		}
	}

	if (terminal->pvt->cursor_visible && GTK_WIDGET_DRAWABLE(terminal)) {
		preedit_width = vte_terminal_preedit_width(terminal, FALSE);

		screen = terminal->pvt->screen;
		row = screen->cursor_current.row;
		column = screen->cursor_current.col;
		columns = 1;
		column = find_start_column (terminal, column, row);
		cell = vte_terminal_find_charcell(terminal, column, row);
		if (cell != NULL) {
			columns = cell->attr.columns;
			if (cell->c != 0 &&
					_vte_draw_get_char_width (
						terminal->pvt->draw,
						cell->c,
						columns, cell->attr.bold) >
			    terminal->char_width * columns) {
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
vte_invalidate_cursor_periodic (VteTerminal *terminal)
{
        VteTerminalPrivate *pvt = terminal->pvt;

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
vte_terminal_emit_commit(VteTerminal *terminal, const gchar *text, guint length)
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

	g_signal_emit_by_name(terminal, "commit", result, length);

	if(wrapped)
		g_slice_free1(length+1, wrapped);
}

/* Emit an "emulation-changed" signal. */
static void
vte_terminal_emit_emulation_changed(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `emulation-changed'.\n");
	g_signal_emit_by_name(terminal, "emulation-changed");
        g_object_notify(G_OBJECT(terminal), "emulation");

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
vte_terminal_emit_child_exited(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `child-exited'.\n");
	g_signal_emit_by_name(terminal, "child-exited");
}

/* Emit a "contents_changed" signal. */
static void
vte_terminal_emit_contents_changed(VteTerminal *terminal)
{
	if (terminal->pvt->contents_changed_pending) {
		/* Update dingus match set. */
		vte_terminal_match_contents_clear(terminal);
		if (terminal->pvt->mouse_cursor_visible) {
			vte_terminal_match_hilite_update(terminal,
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
	GDK_THREADS_ENTER ();
	g_signal_emit_by_name(terminal, "eof");
	GDK_THREADS_LEAVE ();

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

/* Emit a "status-line-changed" signal. */
static void
_vte_terminal_emit_status_line_changed(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `status-line-changed'.\n");
	g_signal_emit_by_name(terminal, "status-line-changed");
/*         g_object_notify(G_OBJECT(terminal), "status-line"); */
}

/* Emit an "increase-font-size" signal. */
static void
vte_terminal_emit_increase_font_size(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `increase-font-size'.\n");
	g_signal_emit_by_name(terminal, "increase-font-size");
        g_object_notify(G_OBJECT(terminal), "font-scale");
}

/* Emit a "decrease-font-size" signal. */
static void
vte_terminal_emit_decrease_font_size(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `decrease-font-size'.\n");
	g_signal_emit_by_name(terminal, "decrease-font-size");
        g_object_notify(G_OBJECT(terminal), "font-scale");
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

/* Deselect anything which is selected and refresh the screen if needed. */
static void
vte_terminal_deselect_all(VteTerminal *terminal)
{
	if (terminal->pvt->has_selection) {
		gint sx, sy, ex, ey;

		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Deselecting all text.\n");

		terminal->pvt->has_selection = FALSE;
		/* Don't free the current selection, as we need to keep
		 * hold of it for async copying from the clipboard. */

		vte_terminal_emit_selection_changed(terminal);

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
	int i, width = 0;
	if (terminal->pvt->tabstops != NULL) {
		g_hash_table_destroy(terminal->pvt->tabstops);
	}
	terminal->pvt->tabstops = g_hash_table_new(NULL, NULL);
	if (terminal->pvt->termcap != NULL) {
		width = _vte_termcap_find_numeric(terminal->pvt->termcap,
						  terminal->pvt->emulation,
						  "it");
	}
	if (width == 0) {
		width = VTE_TAB_WIDTH;
	}
	for (i = 0; i <= VTE_TAB_MAX; i += width) {
		_vte_terminal_set_tabstop(terminal, i);
	}
}

/* Clear the cache of the screen contents we keep. */
static void
vte_terminal_match_contents_clear(VteTerminal *terminal)
{
	g_assert(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->match_contents != NULL) {
		g_free(terminal->pvt->match_contents);
		terminal->pvt->match_contents = NULL;
	}
	if (terminal->pvt->match_attributes != NULL) {
		g_array_free(terminal->pvt->match_attributes, TRUE);
		terminal->pvt->match_attributes = NULL;
	}
	vte_terminal_match_hilite_clear(terminal);
}

/* Refresh the cache of the screen contents we keep. */
static gboolean
always_selected(VteTerminal *terminal, glong column, glong row, gpointer data)
{
	return TRUE;
}

static void
vte_terminal_match_contents_refresh(VteTerminal *terminal)
{
	GArray *array;
	vte_terminal_match_contents_clear(terminal);
	array = g_array_new(FALSE, TRUE, sizeof(struct _VteCharAttributes));
	terminal->pvt->match_contents = vte_terminal_get_text(terminal,
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
                                gdk_cursor_unref(regex->cursor.cursor);
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

        if (regex->mode == VTE_REGEX_GREGEX) {
                g_regex_unref(regex->regex.gregex.regex);
                regex->regex.gregex.regex = NULL;
        } else if (regex->mode == VTE_REGEX_VTE) {
                _vte_regex_free(regex->regex.reg);
                regex->regex.reg = NULL;
        }

        regex->tag = -1;
}

static void
vte_terminal_set_cursor_from_regex_match(VteTerminal *terminal, struct vte_match_regex *regex)
{
        GdkCursor *cursor = NULL;

        if (!GTK_WIDGET_REALIZED(terminal))
                return;
        switch (regex->cursor_mode) {
                case VTE_REGEX_CURSOR_GDKCURSOR:
                        if (regex->cursor.cursor != NULL) {
                                cursor = gdk_cursor_ref(regex->cursor.cursor);
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

        gdk_window_set_cursor(GTK_WIDGET(terminal)->window, cursor);
        if (cursor)
                gdk_cursor_unref(cursor);
}

/**
 * vte_terminal_match_clear_all:
 * @terminal: a #VteTerminal
 *
 * Clears the list of regular expressions the terminal uses to highlight text
 * when the user moves the mouse cursor.
 *
 */
void
vte_terminal_match_clear_all(VteTerminal *terminal)
{
	struct vte_match_regex *regex;
	guint i;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
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
	vte_terminal_match_hilite_clear(terminal);
}

/**
 * vte_terminal_match_remove:
 * @terminal: a #VteTerminal
 * @tag: the tag of the regex to remove
 *
 * Removes the regular expression which is associated with the given @tag from
 * the list of expressions which the terminal will highlight when the user
 * moves the mouse cursor over matching text.
 *
 */
void
vte_terminal_match_remove(VteTerminal *terminal, int tag)
{
	struct vte_match_regex *regex;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
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
	vte_terminal_match_hilite_clear(terminal);
}

static GdkCursor *
vte_terminal_cursor_new(VteTerminal *terminal, GdkCursorType cursor_type)
{
	GdkDisplay *display;
	GdkCursor *cursor;

	display = gtk_widget_get_display(&terminal->widget);
	cursor = gdk_cursor_new_for_display(display, cursor_type);
	return cursor;
}

/**
 * vte_terminal_match_add:
 * @terminal: a #VteTerminal
 * @match: a regular expression
 *
 * Adds a regular expression to the list of matching expressions.  When the
 * user moves the mouse cursor over a section of displayed text which matches
 * this expression, the text will be highlighted.
 *
 * Returns: an integer associated with this expression
 *
 * @Deprecated: 0.17.1
 */
int
vte_terminal_match_add(VteTerminal *terminal, const char *match)
{
	struct vte_match_regex new_regex, *regex;
	guint ret;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
        g_return_val_if_fail(terminal->pvt->match_regex_mode != VTE_REGEX_GREGEX, -1);
	g_return_val_if_fail(match != NULL, -1);
	g_return_val_if_fail(strlen(match) > 0, -1);

        terminal->pvt->match_regex_mode = VTE_REGEX_VTE;

	memset(&new_regex, 0, sizeof(new_regex));
        new_regex.mode = VTE_REGEX_VTE;
	new_regex.regex.reg = _vte_regex_compile(match);
	if (new_regex.regex.reg == NULL) {
		g_warning(_("Error compiling regular expression \"%s\"."),
			  match);
		return -1;
	}

	/* Search for a hole. */
	for (ret = 0; ret < terminal->pvt->match_regexes->len; ret++) {
		regex = &g_array_index(terminal->pvt->match_regexes,
				       struct vte_match_regex,
				       ret);
		if (regex->tag == -1) {
			break;
		}
	}
	/* Set the tag to the insertion point. */
	new_regex.tag = ret;
        new_regex.cursor_mode = VTE_REGEX_CURSOR_GDKCURSORTYPE;
        new_regex.cursor.cursor_type = VTE_DEFAULT_CURSOR;
	if (ret < terminal->pvt->match_regexes->len) {
		/* Overwrite. */
		g_array_index(terminal->pvt->match_regexes,
			      struct vte_match_regex,
			      ret) = new_regex;
	} else {
		/* Append. */
		g_array_append_val(terminal->pvt->match_regexes, new_regex);
	}
	return new_regex.tag;
}

/**
 * vte_terminal_match_add_gregex:
 * @terminal: a #VteTerminal
 * @regex: a #GRegex
 * @flags: the #GRegexMatchFlags to use when matching the regex
 *
 * Adds the regular expression @regex to the list of matching expressions.  When the
 * user moves the mouse cursor over a section of displayed text which matches
 * this expression, the text will be highlighted.
 *
 * Returns: an integer associated with this expression
 *
 * Since: 0.17.1
 */
int
vte_terminal_match_add_gregex(VteTerminal *terminal, GRegex *regex, GRegexMatchFlags flags)
{
	VteTerminalPrivate *pvt;
	struct vte_match_regex new_regex_match, *regex_match;
	guint ret, len;

	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
        g_return_val_if_fail(terminal->pvt->match_regex_mode != VTE_REGEX_VTE, -1);
	g_return_val_if_fail(regex != NULL, -1);

        pvt = terminal->pvt;
        pvt->match_regex_mode = VTE_REGEX_GREGEX;

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
        new_regex_match.mode = VTE_REGEX_GREGEX;
        new_regex_match.regex.gregex.regex = g_regex_ref(regex);
        new_regex_match.regex.gregex.flags = flags;
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
 * vte_terminal_match_set_cursor:
 * @terminal: a #VteTerminal
 * @tag: the tag of the regex which should use the specified cursor
 * @cursor: the #GdkCursor which the terminal should use when the pattern is
 * highlighted
 *
 * Sets which cursor the terminal will use if the pointer is over the pattern
 * specified by @tag.  The terminal keeps a reference to @cursor.
 *
 * Since: 0.11
 *
 */
void
vte_terminal_match_set_cursor(VteTerminal *terminal, int tag, GdkCursor *cursor)
{
	struct vte_match_regex *regex;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail((guint) tag < terminal->pvt->match_regexes->len);
	regex = &g_array_index(terminal->pvt->match_regexes,
			       struct vte_match_regex,
			       tag);
        regex_match_clear_cursor(regex);
        regex->cursor_mode = VTE_REGEX_CURSOR_GDKCURSOR;
	regex->cursor.cursor = cursor ? gdk_cursor_ref(cursor) : NULL;
	vte_terminal_match_hilite_clear(terminal);
}

/**
 * vte_terminal_match_set_cursor_type:
 * @terminal: a #VteTerminal
 * @tag: the tag of the regex which should use the specified cursor
 * @cursor_type: a #GdkCursorType
 *
 * Sets which cursor the terminal will use if the pointer is over the pattern
 * specified by @tag.
 *
 * Since: 0.11.9
 *
 */
void
vte_terminal_match_set_cursor_type(VteTerminal *terminal,
				   int tag, GdkCursorType cursor_type)
{
	struct vte_match_regex *regex;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail((guint) tag < terminal->pvt->match_regexes->len);
	regex = &g_array_index(terminal->pvt->match_regexes,
			       struct vte_match_regex,
			       tag);
        regex_match_clear_cursor(regex);
        regex->cursor_mode = VTE_REGEX_CURSOR_GDKCURSORTYPE;
	regex->cursor.cursor_type = cursor_type;
	vte_terminal_match_hilite_clear(terminal);
}

/**
 * vte_terminal_match_set_cursor_name:
 * @terminal: a #VteTerminal
 * @tag: the tag of the regex which should use the specified cursor
 * @cursor_name: the name of the cursor
 *
 * Sets which cursor the terminal will use if the pointer is over the pattern
 * specified by @tag.
 *
 * Since: 0.17.1
 *
 */
void
vte_terminal_match_set_cursor_name(VteTerminal *terminal,
				   int tag, const char *cursor_name)
{
	struct vte_match_regex *regex;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(cursor_name != NULL);
	g_return_if_fail((guint) tag < terminal->pvt->match_regexes->len);
	regex = &g_array_index(terminal->pvt->match_regexes,
			       struct vte_match_regex,
			       tag);
        regex_match_clear_cursor(regex);
        regex->cursor_mode = VTE_REGEX_CURSOR_NAME;
	regex->cursor.cursor_name = g_strdup (cursor_name);
	vte_terminal_match_hilite_clear(terminal);
}

/* Check if a given cell on the screen contains part of a matched string.  If
 * it does, return the string, and store the match tag in the optional tag
 * argument. */
static char *
vte_terminal_match_check_internal_vte(VteTerminal *terminal,
                                      long column, glong row,
                                      int *tag, int *start, int *end)
{
	struct _vte_regex_match matches[256];
	guint i, j;
	gint k;
	gint start_blank, end_blank;
	int ret, offset;
	struct vte_match_regex *regex = NULL;
	struct _VteCharAttributes *attr = NULL;
	gssize sattr, eattr;
	gchar *line, eol;

	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Checking for match at (%ld,%ld).\n", row, column);
	*tag = -1;
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
		k = 0;
		ret = _vte_regex_exec(regex->regex.reg,
				      line + k,
				      G_N_ELEMENTS(matches),
				      matches);
		while (ret == 0) {
			gint ko = offset - k;
			gint sblank=G_MININT, eblank=G_MAXINT;
			for (j = 0;
			     j < G_N_ELEMENTS(matches) &&
			     matches[j].rm_so != -1;
			     j++) {
				/* The offsets should be "sane". */
				g_assert(matches[j].rm_so + k < eattr);
				g_assert(matches[j].rm_eo + k <= eattr);
				_VTE_DEBUG_IF(VTE_DEBUG_MISC) {
					gchar *match;
					struct _VteCharAttributes *_sattr, *_eattr;
					match = g_strndup(line + matches[j].rm_so + k,
							matches[j].rm_eo - matches[j].rm_so);
					_sattr = &g_array_index(terminal->pvt->match_attributes,
							struct _VteCharAttributes,
							matches[j].rm_so + k);
					_eattr = &g_array_index(terminal->pvt->match_attributes,
							struct _VteCharAttributes,
							matches[j].rm_eo + k - 1);
					g_printerr("Match %u `%s' from %d(%ld,%ld) to %d(%ld,%ld) (%d).\n",
							j, match,
							matches[j].rm_so + k,
							_sattr->column,
							_sattr->row,
							matches[j].rm_eo + k - 1,
							_eattr->column,
							_eattr->row,
							offset);
					g_free(match);

				}
				/* If the pointer is in this substring,
				 * then we're done. */
				if (ko >= matches[j].rm_so &&
				    ko < matches[j].rm_eo) {
					gchar *result;
					if (tag != NULL) {
						*tag = regex->tag;
					}
					if (start != NULL) {
						*start = sattr + k + matches[j].rm_so;
					}
					if (end != NULL) {
						*end = sattr + k + matches[j].rm_eo - 1;
					}
                                        vte_terminal_set_cursor_from_regex_match(terminal, regex);
					result = g_strndup(line + k + matches[j].rm_so,
							 matches[j].rm_eo - matches[j].rm_so);
					line[eattr] = eol;
					return result;
				}
				if (ko > matches[j].rm_eo &&
						matches[j].rm_eo > sblank) {
					sblank = matches[j].rm_eo;
				}
				if (ko < matches[j].rm_so &&
						matches[j].rm_so < eblank) {
					eblank = matches[j].rm_so;
				}
			}
			if (k + sblank > start_blank) {
				start_blank = k + sblank;
			}
			if (k + eblank < end_blank) {
				end_blank = k + eblank;
			}
			/* Skip past the beginning of this match to
			 * look for more. */
			k += matches[0].rm_so + 1;
			if (k > offset) {
				break;
			}
			ret = _vte_regex_exec(regex->regex.reg,
					      line + k,
					      G_N_ELEMENTS(matches),
					      matches);
		}
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

/* Check if a given cell on the screen contains part of a matched string.  If
 * it does, return the string, and store the match tag in the optional tag
 * argument. */
static char *
vte_terminal_match_check_internal_gregex(VteTerminal *terminal,
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
			"Checking for match at (%ld,%ld).\n", row, column);
	*tag = -1;
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
                if (!g_regex_match_full(regex->regex.gregex.regex,
                                        line, -1, 0,
                                        regex->regex.gregex.flags,
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
                                        vte_terminal_set_cursor_from_regex_match(terminal, regex);
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
vte_terminal_match_check_internal(VteTerminal *terminal,
                                  long column, glong row,
                                  int *tag, int *start, int *end)
{
	if (terminal->pvt->match_contents == NULL) {
		vte_terminal_match_contents_refresh(terminal);
	}

        if (terminal->pvt->match_regex_mode == VTE_REGEX_GREGEX)
                return vte_terminal_match_check_internal_gregex(terminal, column, row, tag, start, end);
        if (terminal->pvt->match_regex_mode == VTE_REGEX_VTE)
                return vte_terminal_match_check_internal_vte(terminal, column, row, tag, start, end);
        return NULL;
}

static gboolean
rowcol_inside_match (VteTerminal *terminal, glong row, glong col)
{
	if (terminal->pvt->match_start.row == terminal->pvt->match_end.row) {
		return row == terminal->pvt->match_start.row &&
			col >= terminal->pvt->match_start.column &&
			col <= terminal->pvt->match_end.column;
	} else {
		if (row < terminal->pvt->match_start.row ||
				row > terminal->pvt->match_end.row) {
			return FALSE;
		}
		if (row == terminal->pvt->match_start.row) {
			return col >= terminal->pvt->match_start.column;
		}
		if (row == terminal->pvt->match_end.row) {
			return col <= terminal->pvt->match_end.column;
		}
		return TRUE;
	}
}

/**
 * vte_terminal_match_check:
 * @terminal: a #VteTerminal
 * @column: the text column
 * @row: the text row
 * @tag: pointer to an integer
 *
 * Checks if the text in and around the specified position matches any of the
 * regular expressions previously set using vte_terminal_match_add().  If a
 * match exists, the text string is returned and if @tag is not %NULL, the number
 * associated with the matched regular expression will be stored in @tag.
 *
 * If more than one regular expression has been set with
 * vte_terminal_match_add(), then expressions are checked in the order in
 * which they were added.
 *
 * Returns: a string which matches one of the previously set regular
 * expressions, and which must be freed by the caller.
 */
char *
vte_terminal_match_check(VteTerminal *terminal, glong column, glong row,
			 int *tag)
{
	long delta;
	char *ret;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	delta = terminal->pvt->screen->scroll_delta;
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
		ret = vte_terminal_match_check_internal(terminal,
				column, row + delta,
				tag, NULL, NULL);
	}
	_VTE_DEBUG_IF(VTE_DEBUG_EVENTS) {
		if (ret != NULL) g_printerr("Matched `%s'.\n", ret);
	}
	return ret;
}

/* Emit an adjustment changed signal on our adjustment object. */
static void
vte_terminal_emit_adjustment_changed(VteTerminal *terminal)
{
	if (terminal->pvt->adjustment_changed_pending) {
		VteScreen *screen = terminal->pvt->screen;
		gboolean changed = FALSE;
		glong v;

		v = _vte_ring_delta (screen->row_data);
		if (terminal->adjustment->lower != v) {
			_vte_debug_print(VTE_DEBUG_ADJ,
					"Changing lower bound from %.0f to %ld\n",
					terminal->adjustment->lower,
					v);
			terminal->adjustment->lower = v;
			changed = TRUE;
		}

		/* The upper value is the number of rows which might be visible.  (Add
		 * one to the cursor offset because it's zero-based.) */
		v = MAX(_vte_ring_next(screen->row_data),
				screen->cursor_current.row + 1);
		if (terminal->adjustment->upper != v) {
			_vte_debug_print(VTE_DEBUG_ADJ,
					"Changing upper bound from %.0f to %ld\n",
					terminal->adjustment->upper,
					v);
			terminal->adjustment->upper = v;
			changed = TRUE;
		}

		if (changed) {
			_vte_debug_print(VTE_DEBUG_SIGNALS,
					"Emitting adjustment_changed.\n");
			gtk_adjustment_changed(terminal->adjustment);
		}
		terminal->pvt->adjustment_changed_pending = FALSE;
	}
	if (terminal->pvt->adjustment_value_changed_pending) {
		glong v;
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting adjustment_value_changed.\n");
		terminal->pvt->adjustment_value_changed_pending = FALSE;
		v = floor (terminal->adjustment->value);
		if (v != terminal->pvt->screen->scroll_delta) {
			/* this little dance is so that the scroll_delta is
			 * updated immediately, but we still handled scrolling
			 * via the adjustment - e.g. user interaction with the
			 * scrollbar
			 */
			terminal->adjustment->value = terminal->pvt->screen->scroll_delta;
			terminal->pvt->screen->scroll_delta = v;
			gtk_adjustment_value_changed(terminal->adjustment);
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
vte_terminal_queue_adjustment_value_changed(VteTerminal *terminal, glong v)
{
	if (v != terminal->pvt->screen->scroll_delta) {
		terminal->pvt->screen->scroll_delta = v;
		terminal->pvt->adjustment_value_changed_pending = TRUE;
		add_update_timeout (terminal);
	}
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
	screen->cursor_current.row = MAX(screen->cursor_current.row,
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

	g_assert(terminal->pvt->screen != NULL);
	g_assert(terminal->pvt->screen->row_data != NULL);

	_vte_terminal_adjust_adjustments(terminal);

	/* The step increment should always be one. */
	if (terminal->adjustment->step_increment != 1) {
		_vte_debug_print(VTE_DEBUG_ADJ,
				"Changing step increment from %.0lf to %ld\n",
				terminal->adjustment->step_increment,
				terminal->row_count);
		terminal->adjustment->step_increment = 1;
		changed = TRUE;
	}

	/* Set the number of rows the user sees to the number of rows the
	 * user sees. */
	if (terminal->adjustment->page_size != terminal->row_count) {
		_vte_debug_print(VTE_DEBUG_ADJ,
				"Changing page size from %.0f to %ld\n",
				terminal->adjustment->page_size,
				terminal->row_count);
		terminal->adjustment->page_size = terminal->row_count;
		changed = TRUE;
	}

	/* Clicking in the empty area should scroll one screen, so set the
	 * page size to the number of visible rows. */
	if (terminal->adjustment->page_increment != terminal->row_count) {
		_vte_debug_print(VTE_DEBUG_ADJ,
				"Changing page increment from "
				"%.0f to %ld\n",
				terminal->adjustment->page_increment,
				terminal->row_count);
		terminal->adjustment->page_increment = terminal->row_count;
		changed = TRUE;
	}

	if (changed) {
		_vte_debug_print(VTE_DEBUG_SIGNALS,
				"Emitting adjustment_changed.\n");
		gtk_adjustment_changed(terminal->adjustment);
	}
}

/* Scroll a fixed number of lines up or down in the current screen. */
static void
vte_terminal_scroll_lines(VteTerminal *terminal, gint lines)
{
	glong destination;
	_vte_debug_print(VTE_DEBUG_ADJ, "Scrolling %d lines.\n", lines);
	/* Calculate the ideal position where we want to be before clamping. */
	destination = terminal->pvt->screen->scroll_delta;
	destination += lines;
	/* Can't scroll past data we have. */
	destination = CLAMP(destination,
			    terminal->adjustment->lower,
			    MAX (terminal->adjustment->lower, terminal->adjustment->upper - terminal->row_count));
	/* Tell the scrollbar to adjust itself. */
	vte_terminal_queue_adjustment_value_changed (terminal,
			destination);
}

/* Scroll a fixed number of pages up or down, in the current screen. */
static void
vte_terminal_scroll_pages(VteTerminal *terminal, gint pages)
{
	vte_terminal_scroll_lines(terminal, pages * terminal->row_count);
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
  _vte_pty_set_utf8(terminal->pvt->pty_master,
		    (strcmp(terminal->pvt->encoding, "UTF-8") == 0));
}

/**
 * vte_terminal_set_encoding:
 * @terminal: a #VteTerminal
 * @codeset: a valid #g_iconv target
 *
 * Changes the encoding the terminal will expect data from the child to
 * be encoded with.  For certain terminal types, applications executing in the
 * terminal can change the encoding.  The default encoding is defined by the
 * application's locale settings.
 *
 */
void
vte_terminal_set_encoding(VteTerminal *terminal, const char *codeset)
{
        VteTerminalPrivate *pvt;
        GObject *object;
	const char *old_codeset;
	VteConv conv;
	char *obuf1, *obuf2;
	gsize bytes_written;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        object = G_OBJECT(terminal);
        pvt = terminal->pvt;

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
	if (terminal->pvt->outgoing_conv != VTE_INVALID_CONV) {
		_vte_conv_close(terminal->pvt->outgoing_conv);
	}
	terminal->pvt->outgoing_conv = conv;

	/* Set the terminal's encoding to the new value. */
	terminal->pvt->encoding = g_intern_string(codeset);

	/* Convert any buffered output bytes. */
	if ((_vte_buffer_length(terminal->pvt->outgoing) > 0) &&
	    (old_codeset != NULL)) {
		/* Convert back to UTF-8. */
		obuf1 = g_convert((gchar *)terminal->pvt->outgoing->data,
				  _vte_buffer_length(terminal->pvt->outgoing),
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
				_vte_buffer_clear(terminal->pvt->outgoing);
				_vte_buffer_append(terminal->pvt->outgoing,
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
}

/**
 * vte_terminal_get_encoding:
 * @terminal: a #VteTerminal
 *
 * Determines the name of the encoding in which the terminal expects data to be
 * encoded.
 *
 * Returns: the current encoding for the terminal.
 */
const char *
vte_terminal_get_encoding(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return terminal->pvt->encoding;
}

static inline VteRowData *
vte_terminal_insert_rows (VteTerminal *terminal, guint cnt)
{
	VteRowData *row;
	VteScreen *screen = terminal->pvt->screen;
	do {
		row = _vte_ring_append (screen->row_data);
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
	v = screen->cursor_current.row;

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
	_vte_row_data_fill (row, &basic_cell.cell, terminal->pvt->screen->cursor_current.col);

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
	delta = screen->cursor_current.row - rows + 1;
	if (G_UNLIKELY (delta > 0)) {
		vte_terminal_insert_rows (terminal, delta);
		rows = _vte_ring_next (screen->row_data);
	}

	/* Make sure that the bottom row is visible, and that it's in
	 * the buffer (even if it's empty).  This usually causes the
	 * top row to become a history-only row. */
	delta = screen->insert_delta;
	delta = MIN(delta, rows - terminal->row_count);
	delta = MAX(delta,
		    screen->cursor_current.row - (terminal->row_count - 1));
	delta = MAX(delta, _vte_ring_delta(screen->row_data));

	/* Adjust the insert delta and scroll if needed. */
	if (delta != screen->insert_delta) {
		screen->insert_delta = delta;
		_vte_terminal_adjust_adjustments(terminal);
	}
}

/* Show or hide the pointer. */
void
_vte_terminal_set_pointer_visible(VteTerminal *terminal, gboolean visible)
{
	struct vte_match_regex *regex = NULL;
	terminal->pvt->mouse_cursor_visible = visible;

        if (!GTK_WIDGET_REALIZED(terminal))
                return;

	if (visible || !terminal->pvt->mouse_autohide) {
		if (terminal->pvt->mouse_tracking_mode) {
			_vte_debug_print(VTE_DEBUG_CURSOR,
					"Setting mousing cursor.\n");
			gdk_window_set_cursor(terminal->widget.window, terminal->pvt->mouse_mousing_cursor);
		} else
		if ( (guint)terminal->pvt->match_tag < terminal->pvt->match_regexes->len) {
			regex = &g_array_index(terminal->pvt->match_regexes,
					       struct vte_match_regex,
					       terminal->pvt->match_tag);
                        vte_terminal_set_cursor_from_regex_match(terminal, regex);
		} else {
			_vte_debug_print(VTE_DEBUG_CURSOR,
					"Setting default mouse cursor.\n");
			gdk_window_set_cursor(terminal->widget.window, terminal->pvt->mouse_default_cursor);
		}
	} else {
		_vte_debug_print(VTE_DEBUG_CURSOR,
				"Setting to invisible cursor.\n");
		gdk_window_set_cursor(terminal->widget.window, terminal->pvt->mouse_inviso_cursor);
	}
}

/**
 * vte_terminal_new:
 *
 * Create a new terminal widget.
 *
 * Returns: a new #VteTerminal object
 */
GtkWidget *
vte_terminal_new(void)
{
	return g_object_new(VTE_TYPE_TERMINAL, NULL);
}

/* Set up a palette entry with a more-or-less match for the requested color. */
static void
vte_terminal_set_color_internal(VteTerminal *terminal, int entry,
				const GdkColor *proposed)
{
	struct vte_palette_entry *color;

	color = &terminal->pvt->palette[entry];

	if (color->red == proposed->red &&
	    color->green == proposed->green &&
	    color->blue == proposed->blue) {
		return;
	}

	_vte_debug_print(VTE_DEBUG_MISC,
			"Set color[%d] to (%04x,%04x,%04x).\n", entry,
			proposed->red, proposed->green, proposed->blue);

	/* Save the requested color. */
	color->red = proposed->red;
	color->green = proposed->green;
	color->blue = proposed->blue;

	/* If we're not realized yet, there's nothing else to do. */
	if (!GTK_WIDGET_REALIZED(terminal)) {
		return;
	}

	/* If we're setting the background color, set the background color
	 * on the widget as well. */
	if (entry == VTE_DEF_BG) {
		vte_terminal_queue_background_update(terminal);
	}

	/* and redraw */
	if (entry == VTE_CUR_BG)
		_vte_invalidate_cursor_once(terminal, FALSE);
	else
		_vte_invalidate_all (terminal);
}

static void
vte_terminal_generate_bold(const struct vte_palette_entry *foreground,
			   const struct vte_palette_entry *background,
			   double factor,
			   GdkColor *bold)
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

/**
 * vte_terminal_set_color_bold:
 * @terminal: a #VteTerminal
 * @bold: the new bold color
 *
 * Sets the color used to draw bold text in the default foreground color.
 *
 */
void
vte_terminal_set_color_bold(VteTerminal *terminal, const GdkColor *bold)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(bold != NULL);

	_vte_debug_print(VTE_DEBUG_MISC,
			"Set bold color to (%04x,%04x,%04x).\n",
			bold->red, bold->green, bold->blue);
	vte_terminal_set_color_internal(terminal, VTE_BOLD_FG, bold);
}

/**
 * vte_terminal_set_color_dim:
 * @terminal: a #VteTerminal
 * @dim: the new dim color
 *
 * Sets the color used to draw dim text in the default foreground color.
 *
 */
void
vte_terminal_set_color_dim(VteTerminal *terminal, const GdkColor *dim)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(dim != NULL);

	_vte_debug_print(VTE_DEBUG_MISC,
			"Set dim color to (%04x,%04x,%04x).\n",
			dim->red, dim->green, dim->blue);
	vte_terminal_set_color_internal(terminal, VTE_DIM_FG, dim);
}

/**
 * vte_terminal_set_color_foreground:
 * @terminal: a #VteTerminal
 * @foreground: the new foreground color
 *
 * Sets the foreground color used to draw normal text
 *
 */
void
vte_terminal_set_color_foreground(VteTerminal *terminal,
				  const GdkColor *foreground)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(foreground != NULL);

	_vte_debug_print(VTE_DEBUG_MISC,
			"Set foreground color to (%04x,%04x,%04x).\n",
			foreground->red, foreground->green, foreground->blue);
	vte_terminal_set_color_internal(terminal, VTE_DEF_FG, foreground);
}

/**
 * vte_terminal_set_color_background:
 * @terminal: a #VteTerminal
 * @background: the new background color
 *
 * Sets the background color for text which does not have a specific background
 * color assigned.  Only has effect when no background image is set and when
 * the terminal is not transparent.
 *
 */
void
vte_terminal_set_color_background(VteTerminal *terminal,
				  const GdkColor *background)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(background != NULL);

	_vte_debug_print(VTE_DEBUG_MISC,
			"Set background color to (%04x,%04x,%04x).\n",
			background->red, background->green, background->blue);
	vte_terminal_set_color_internal(terminal, VTE_DEF_BG, background);
}

/**
 * vte_terminal_set_color_cursor:
 * @terminal: a #VteTerminal
 * @cursor_background: the new color to use for the text cursor
 *
 * Sets the background color for text which is under the cursor.  If %NULL, text
 * under the cursor will be drawn with foreground and background colors
 * reversed.
 *
 * Since: 0.11.11
 *
 */
void
vte_terminal_set_color_cursor(VteTerminal *terminal,
			      const GdkColor *cursor_background)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	if (cursor_background != NULL) {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Set cursor color to (%04x,%04x,%04x).\n",
				cursor_background->red,
				cursor_background->green,
				cursor_background->blue);
		vte_terminal_set_color_internal(terminal, VTE_CUR_BG,
						cursor_background);
		terminal->pvt->cursor_color_set = TRUE;
	} else {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Cleared cursor color.\n");
		terminal->pvt->cursor_color_set = FALSE;
	}
}

/**
 * vte_terminal_set_color_highlight:
 * @terminal: a #VteTerminal
 * @highlight_background: the new color to use for highlighted text
 *
 * Sets the background color for text which is highlighted.  If %NULL,
 * highlighted text (which is usually highlighted because it is selected) will
 * be drawn with foreground and background colors reversed.
 *
 * Since: 0.11.11
 *
 */
void
vte_terminal_set_color_highlight(VteTerminal *terminal,
				 const GdkColor *highlight_background)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	if (highlight_background != NULL) {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Set highlight color to (%04x,%04x,%04x).\n",
				highlight_background->red,
				highlight_background->green,
				highlight_background->blue);
		vte_terminal_set_color_internal(terminal, VTE_DEF_HL,
						highlight_background);
		terminal->pvt->highlight_color_set = TRUE;
	} else {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Cleared highlight color.\n");
		terminal->pvt->highlight_color_set = FALSE;
	}
}

/**
 * vte_terminal_set_colors:
 * @terminal: a #VteTerminal
 * @foreground: the new foreground color, or %NULL
 * @background: the new background color, or %NULL
 * @palette: the color palette
 * @palette_size: the number of entries in @palette
 *
 * The terminal widget uses a 28-color model comprised of the default foreground
 * and background colors, the bold foreground color, the dim foreground
 * color, an eight color palette, bold versions of the eight color palette,
 * and a dim version of the the eight color palette.
 *
 * @palette_size must be either 0, 8, 16, or 24, or between 25 and 255 inclusive.
 * If @foreground is %NULL and
 * @palette_size is greater than 0, the new foreground color is taken from
 * @palette[7].  If @background is %NULL and @palette_size is greater than 0,
 * the new background color is taken from @palette[0].  If
 * @palette_size is 8 or 16, the third (dim) and possibly the second (bold)
 * 8-color palettes are extrapolated from the new background color and the items
 * in @palette.
 *
 */
void
vte_terminal_set_colors(VteTerminal *terminal,
			const GdkColor *foreground,
			const GdkColor *background,
			const GdkColor *palette,
			glong palette_size)
{
	guint i;
	GdkColor color;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	g_return_if_fail(palette_size >= 0);
	g_return_if_fail((palette_size == 0) ||
			 (palette_size == 8) ||
			 (palette_size == 16) ||
			 (palette_size == 24) ||
			 (palette_size > 24 && palette_size < 256));

	_vte_debug_print(VTE_DEBUG_MISC,
			"Set color palette [%ld elements].\n",
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
			case VTE_DEF_BG:
				if (background != NULL) {
					color = *background;
				} else {
					color.red = 0;
					color.blue = 0;
					color.green = 0;
				}
				break;
			case VTE_DEF_FG:
				if (foreground != NULL) {
					color = *foreground;
				} else {
					color.red = 0xc000;
					color.blue = 0xc000;
					color.green = 0xc000;
				}
				break;
			case VTE_BOLD_FG:
				vte_terminal_generate_bold(&terminal->pvt->palette[VTE_DEF_FG],
							   &terminal->pvt->palette[VTE_DEF_BG],
							   1.8,
							   &color);
				break;
			case VTE_DIM_FG:
				vte_terminal_generate_bold(&terminal->pvt->palette[VTE_DEF_FG],
							   &terminal->pvt->palette[VTE_DEF_BG],
							   0.5,
							   &color);
				break;
			case VTE_DEF_HL:
				color.red = 0xc000;
				color.blue = 0xc000;
				color.green = 0xc000;
				break;
			case VTE_CUR_BG:
				color.red = 0x0000;
				color.blue = 0x0000;
				color.green = 0x0000;
				break;
			}

		/* Override from the supplied palette if there is one. */
		if ((glong) i < palette_size) {
			color = palette[i];
		}

		/* Set up the color entry. */
		vte_terminal_set_color_internal(terminal, i, &color);
	}

	/* Track that we had a color palette set. */
	terminal->pvt->palette_initialized = TRUE;
}

/**
 * vte_terminal_set_opacity:
 * @terminal: a #VteTerminal
 * @opacity: the new opacity
 *
 * Sets the opacity of the terminal background, were 0 means completely
 * transparent and 65535 means completely opaque.
 *
 */
void
vte_terminal_set_opacity(VteTerminal *terminal, guint16 opacity)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        if (opacity == pvt->bg_opacity)
                return;

	pvt->bg_opacity = opacity;

        g_object_notify(G_OBJECT(terminal), "background-opacity");
}

/**
 * vte_terminal_set_default_colors:
 * @terminal: a #VteTerminal
 *
 * Reset the terminal palette to reasonable compiled-in defaults.
 *
 */
void
vte_terminal_set_default_colors(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_terminal_set_colors(terminal, NULL, NULL, NULL, 0);
}


/* Cleanup smart-tabs.  See vte_sequence_handler_ta() */
void
_vte_terminal_cleanup_tab_fragments_at_cursor (VteTerminal *terminal)
{
	VteRowData *row = _vte_terminal_ensure_row (terminal);
	VteScreen *screen = terminal->pvt->screen;
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
_vte_terminal_cursor_down (VteTerminal *terminal)
{
	long start, end;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = start + terminal->row_count - 1;
	}
	if (screen->cursor_current.row == end) {
		/* Match xterm and fill to the end of row when scrolling. */
		if (screen->fill_defaults.attr.back != VTE_DEF_BG) {
			VteRowData *rowdata;
			rowdata = _vte_terminal_ensure_row (terminal);
			_vte_row_data_fill (rowdata, &screen->fill_defaults, terminal->column_count);
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
				_vte_ring_insert (terminal->pvt->screen->row_data,
						  screen->cursor_current.row);
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
				_vte_ring_remove (terminal->pvt->screen->row_data, start);
				_vte_ring_insert (terminal->pvt->screen->row_data, end);
				/* Update the display. */
				_vte_terminal_scroll_region(terminal, start,
							   end - start + 1, -1);
				_vte_invalidate_cells(terminal,
						      0, terminal->column_count,
						      end - 2, 2);
			}
		} else {
			/* Scroll up with history. */
			screen->cursor_current.row++;
			_vte_terminal_update_insert_delta(terminal);
		}

		/* Match xterm and fill the new row when scrolling. */
		if (screen->fill_defaults.attr.back != VTE_DEF_BG) {
			VteRowData *rowdata;
			rowdata = _vte_terminal_ensure_row (terminal);
			_vte_row_data_fill (rowdata, &screen->fill_defaults, terminal->column_count);
		}
	} else {
		/* Otherwise, just move the cursor down. */
		screen->cursor_current.row++;
	}
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

	screen = terminal->pvt->screen;
	insert |= screen->insert_mode;
	invalidate_now |= insert;

	/* If we've enabled the special drawing set, map the characters to
	 * Unicode. */
	if (G_UNLIKELY (screen->alternate_charset)) {
		_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
				"Attempting charset substitution"
				"for U+%04X.\n", c);
		/* See if there's a mapping for it. */
		c = _vte_iso2022_process_single(terminal->pvt->iso2022, c, '0');
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
		columns = _vte_iso2022_unichar_width(terminal->pvt->iso2022, c);
	}


	/* If we're autowrapping here, do it. */
	col = screen->cursor_current.col;
	if (G_UNLIKELY (columns && col + columns > terminal->column_count)) {
		if (terminal->pvt->flags.am) {
			_vte_debug_print(VTE_DEBUG_ADJ,
					"Autowrapping before character\n");
			/* Wrap. */
			/* XXX clear to the end of line */
			col = screen->cursor_current.col = 0;
			/* Mark this line as soft-wrapped. */
			row = _vte_terminal_ensure_row (terminal);
			row->attr.soft_wrapped = 1;
			_vte_terminal_cursor_down (terminal);
		} else {
			/* Don't wrap, stay at the rightmost column. */
			col = screen->cursor_current.col =
				terminal->column_count - columns;
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

	_vte_terminal_cleanup_tab_fragments_at_cursor (terminal);

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
		while (cell != NULL && cell->attr.fragment && col2 > 0)
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

	if (G_UNLIKELY (c == '_' && terminal->pvt->flags.ul)) {
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
	_vte_row_data_shrink (row, terminal->column_count);

	/* Signal that this part of the window needs drawing. */
	if (G_UNLIKELY (invalidate_now)) {
		_vte_invalidate_cells(terminal,
				col - columns,
				insert ? terminal->column_count : columns,
				screen->cursor_current.row, 1);
	}


	/* If we're autowrapping *here*, do it. */
	screen->cursor_current.col = col;
	if (G_UNLIKELY (col >= terminal->column_count)) {
		if (terminal->pvt->flags.am && !terminal->pvt->flags.xn) {
			/* Wrap. */
			screen->cursor_current.col = 0;
			/* Mark this line as soft-wrapped. */
			row->attr.soft_wrapped = 1;
			_vte_terminal_cursor_down (terminal);
		}
	}

done:
	/* We added text, so make a note of it. */
	terminal->pvt->text_inserted_flag = TRUE;

not_inserted:
	_vte_debug_print(VTE_DEBUG_ADJ|VTE_DEBUG_PARSE,
			"insertion delta => %ld.\n",
			(long)screen->insert_delta);
	return line_wrapped;
}

/* Catch a VteReaper child-exited signal, and if it matches the one we're
 * looking for, emit one of our own. */
static void
vte_terminal_catch_child_exited(VteReaper *reaper, int pid, int status,
				VteTerminal *terminal)
{
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
		/* Disconnect from the reaper. */
		if (terminal->pvt->pty_reaper != NULL) {
			g_signal_handlers_disconnect_by_func(terminal->pvt->pty_reaper,
							     vte_terminal_catch_child_exited,
							     terminal);
			g_object_unref(terminal->pvt->pty_reaper);
			terminal->pvt->pty_reaper = NULL;
		}
		terminal->pvt->pty_pid = -1;

		/* Close out the PTY. */
		_vte_terminal_disconnect_pty_read(terminal);
		_vte_terminal_disconnect_pty_write(terminal);
		if (terminal->pvt->pty_channel != NULL) {
			g_io_channel_unref (terminal->pvt->pty_channel);
			terminal->pvt->pty_channel = NULL;
		}
		if (terminal->pvt->pty_master != -1) {
			_vte_pty_close(terminal->pvt->pty_master);
			close(terminal->pvt->pty_master);
			terminal->pvt->pty_master = -1;
                
                        g_object_notify(object, "pty");
		}

		/* Take one last shot at processing whatever data is pending,
		 * then flush the buffers in case we're about to run a new
		 * command, disconnecting the timeout. */
		if (terminal->pvt->incoming != NULL) {
			vte_terminal_process_incoming(terminal);
			_vte_incoming_chunks_release (terminal->pvt->incoming);
			terminal->pvt->incoming = NULL;
			terminal->pvt->input_bytes = 0;
		}
		g_array_set_size(terminal->pvt->pending, 0);
		vte_terminal_stop_processing (terminal);

		/* Clear the outgoing buffer as well. */
		_vte_buffer_clear(terminal->pvt->outgoing);

		/* Tell observers what's happened. */
                terminal->pvt->child_exit_status = status;
		vte_terminal_emit_child_exited(terminal);

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
					    G_IO_IN | G_IO_HUP,
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
	if (terminal->pvt->pty_channel == NULL) {
		terminal->pvt->pty_channel =
			g_io_channel_unix_new(terminal->pvt->pty_master);
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

/* Basic wrapper around _vte_pty_open(), which handles the pipefitting. */
static pid_t
_vte_terminal_fork_basic(VteTerminal *terminal, const char *command,
			 char **argv, char **envv,
			 const char *directory,
			 gboolean lastlog, gboolean utmp, gboolean wtmp)
{
        GObject *object = G_OBJECT(terminal);
	char **env_add;
	int i, fd;
	pid_t pid;
	VteReaper *reaper;

        g_object_freeze_notify(object);

	/* Duplicate the environment, and add one more variable. */
	i = envv ? g_strv_length(envv) : 0;
	env_add = g_new(char *, i + 2);
	env_add[0] = g_strdup_printf("TERM=%s", terminal->pvt->emulation);
	for (i = 0; (envv != NULL) && (envv[i] != NULL); i++) {
		env_add[i + 1] = g_strdup(envv[i]);
	}
	env_add[i + 1] = NULL;

	/* Close any existing ptys. */
	if (terminal->pvt->pty_channel != NULL) {
		g_io_channel_unref (terminal->pvt->pty_channel);
		terminal->pvt->pty_channel = NULL;
	}
	if (terminal->pvt->pty_master != -1) {
		_vte_pty_close(terminal->pvt->pty_master);
		close(terminal->pvt->pty_master);
		terminal->pvt->pty_master = -1;

                g_object_notify(object, "pty");

	}

        terminal->pvt->child_exit_status = 0;

	/* Open the new pty. */
	pid = -1;
	fd = _vte_pty_open(&pid, env_add, command, argv, directory,
			  terminal->column_count, terminal->row_count,
			  lastlog, utmp, wtmp);
	if (pid == -1 || fd == -1) {
		g_strfreev(env_add);

                g_object_thaw_notify(object);

		return -1;
	}

	/* If we successfully started the process, set up to listen for its
	 * output. */
	if (pid != 0) {
		/* Set this as the child's pid. */
		terminal->pvt->pty_pid = pid;

		vte_terminal_set_pty (terminal, fd);

		/* Catch a child-exited signal from the child pid. */
		reaper = vte_reaper_get();
		vte_reaper_add_child(pid);
		if (reaper != terminal->pvt->pty_reaper) {
			if (terminal->pvt->pty_reaper != NULL) {
				g_signal_handlers_disconnect_by_func(terminal->pvt->pty_reaper,
						vte_terminal_catch_child_exited,
						terminal);
				g_object_unref(terminal->pvt->pty_reaper);
			}
			g_signal_connect(reaper, "child-exited",
					G_CALLBACK(vte_terminal_catch_child_exited),
					terminal);
			terminal->pvt->pty_reaper = reaper;
		} else
			g_object_unref(reaper);
	}

	/* Clean up. */
	g_strfreev(env_add);

        g_object_thaw_notify(object);

	/* Return the pid to the caller. */
	return pid;
}

/**
 * vte_terminal_fork_command:
 * @terminal: a #VteTerminal
 * @command: the name of a binary to run, or %NULL to get user's shell
 * @argv: the argument list to be passed to @command, or %NULL
 * @envv: a list of environment variables to be added to the environment before
 * starting @command, or %NULL
 * @directory: the name of a directory the command should start in, or %NULL
 * @lastlog: %TRUE if the session should be logged to the lastlog
 * @utmp: %TRUE if the session should be logged to the utmp/utmpx log
 * @wtmp: %TRUE if the session should be logged to the wtmp/wtmpx log
 *
 * Starts the specified command under a newly-allocated controlling
 * pseudo-terminal.  The @argv and @envv lists should be %NULL-terminated, and
 * argv[0] is expected to be the name of the file being run, as it would be if
 * execve() were being called.  TERM is automatically set to reflect the
 * terminal widget's emulation setting.  If @lastlog, @utmp, or @wtmp are %TRUE,
 * logs the session to the specified system log files.
 *
 * Note that all file descriptors except stdin/stdout/stderr will be closed
 * before calling exec() in the child.
 * 
 * Returns: the ID of the new process
 */
pid_t
vte_terminal_fork_command(VteTerminal *terminal,
			  const char *command, char **argv, char **envv,
			  const char *directory,
			  gboolean lastlog, gboolean utmp, gboolean wtmp)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);

	/* Make the user's shell the default command. */
	if (command == NULL) {
		if (terminal->pvt->shell == NULL) {
			struct passwd *pwd;

			pwd = getpwuid(getuid());
			if (pwd != NULL) {
				terminal->pvt->shell = pwd->pw_shell;
				_vte_debug_print(VTE_DEBUG_MISC,
						"Using user's shell (%s).\n",
						terminal->pvt->shell);
			}
		}
		if (terminal->pvt->shell == NULL) {
			if (getenv ("SHELL")) {
				terminal->pvt->shell = getenv ("SHELL");
				_vte_debug_print(VTE_DEBUG_MISC,
					"Using $SHELL shell (%s).\n",
						terminal->pvt->shell);
			} else {
				terminal->pvt->shell = "/bin/sh";
				_vte_debug_print(VTE_DEBUG_MISC,
						"Using default shell (%s).\n",
						terminal->pvt->shell);
			}
		}
		command = terminal->pvt->shell;
	}

	/* Start up the command and get the PTY of the master. */
	return _vte_terminal_fork_basic(terminal, command, argv, envv,
				        directory, lastlog, utmp, wtmp);
}

/**
 * vte_terminal_forkpty:
 * @terminal: a #VteTerminal
 * @envv: a list of environment variables to be added to the environment before
 * starting returning in the child process, or %NULL
 * @directory: the name of a directory the child process should change to, or
 * %NULL
 * @lastlog: %TRUE if the session should be logged to the lastlog
 * @utmp: %TRUE if the session should be logged to the utmp/utmpx log
 * @wtmp: %TRUE if the session should be logged to the wtmp/wtmpx log
 *
 * Starts a new child process under a newly-allocated controlling
 * pseudo-terminal.  TERM is automatically set to reflect the terminal widget's
 * emulation setting.  If @lastlog, @utmp, or @wtmp are %TRUE, logs the session
 * to the specified system log files.
 *
 * Returns: the ID of the new process in the parent, 0 in the child, and -1 if
 * there was an error
 *
 * Since: 0.11.11
 */
pid_t
vte_terminal_forkpty(VteTerminal *terminal,
		     char **envv, const char *directory,
		     gboolean lastlog, gboolean utmp, gboolean wtmp)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);

	return _vte_terminal_fork_basic(terminal, NULL, NULL, envv,
				       directory, lastlog, utmp, wtmp);
}

/* Handle an EOF from the client. */
static void
vte_terminal_eof(GIOChannel *channel, VteTerminal *terminal)
{
        GObject *object = G_OBJECT(terminal);

        g_object_freeze_notify(object);


	/* Close the connections to the child -- note that the source channel
	 * has already been dereferenced. */

	_vte_terminal_disconnect_pty_read(terminal);
	_vte_terminal_disconnect_pty_write(terminal);
	if (terminal->pvt->pty_channel != NULL) {
		g_io_channel_unref (terminal->pvt->pty_channel);
		terminal->pvt->pty_channel = NULL;
	}
	if (terminal->pvt->pty_master != -1) {
		_vte_pty_close(terminal->pvt->pty_master);
		close(terminal->pvt->pty_master);
		terminal->pvt->pty_master = -1;
        
                g_object_notify(object, "pty");
	}

	/* Take one last shot at processing whatever data is pending, then
	 * flush the buffers in case we're about to run a new command,
	 * disconnecting the timeout. */
	vte_terminal_stop_processing (terminal);
	if (terminal->pvt->incoming) {
		vte_terminal_process_incoming(terminal);
		terminal->pvt->input_bytes = 0;
	}
	g_array_set_size(terminal->pvt->pending, 0);

	/* Clear the outgoing buffer as well. */
	_vte_buffer_clear(terminal->pvt->outgoing);

	/* Emit a signal that we read an EOF. */
	vte_terminal_queue_eof(terminal);

        g_object_thaw_notify(object);
}

/* Reset the input method context. */
static void
vte_terminal_im_reset(VteTerminal *terminal)
{
	if (GTK_WIDGET_REALIZED(terminal)) {
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
vte_terminal_emit_pending_text_signals(VteTerminal *terminal, GQuark quark)
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
	struct vte_cursor_position cursor;
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
			_vte_incoming_chunks_length(terminal->pvt->incoming),
			_vte_incoming_chunks_count(terminal->pvt->incoming),
			terminal->pvt->pending->len);
	_vte_debug_print (VTE_DEBUG_WORK, "(");

	screen = terminal->pvt->screen;

	delta = screen->scroll_delta;
	bottom = screen->insert_delta == delta;

	/* Save the current cursor position. */
	cursor = screen->cursor_current;
	cursor_visible = terminal->pvt->cursor_visible;

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
		GQuark quark;
		const gunichar *next;
		GValueArray *params = NULL;

		/* Try to match any control sequences. */
		_vte_matcher_match(terminal->pvt->matcher,
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
			_vte_terminal_handle_sequence(terminal,
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
						terminal->column_count);
				/* lazily apply the +1 to the cursor_row */
				bbox_bottomright.y = MIN(bbox_bottomright.y + 1,
						delta + terminal->row_count);

				_vte_invalidate_cells(terminal,
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
				_vte_matcher_match(terminal->pvt->matcher,
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
			if (G_UNLIKELY (_vte_terminal_insert_char(terminal, c,
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
							terminal->column_count);
					/* lazily apply the +1 to the cursor_row */
					bbox_bottomright.y = MIN(bbox_bottomright.y + 1,
							delta + terminal->row_count);

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
						    terminal->column_count,
						    vte_cell_is_selected,
						    NULL,
						    NULL);
			if ((selection == NULL) || (terminal->pvt->selection == NULL) ||
			    (strcmp(selection, terminal->pvt->selection) != 0)) {
				vte_terminal_deselect_all(terminal);
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
		bbox_topleft.y = MAX(bbox_topleft.y, delta);
		bbox_bottomright.x = MIN(bbox_bottomright.x,
				terminal->column_count);
		/* lazily apply the +1 to the cursor_row */
		bbox_bottomright.y = MIN(bbox_bottomright.y + 1,
				delta + terminal->row_count);

		_vte_invalidate_cells(terminal,
				bbox_topleft.x,
				bbox_bottomright.x - bbox_topleft.x,
				bbox_topleft.y,
				bbox_bottomright.y - bbox_topleft.y);
	}


	if ((cursor.col != terminal->pvt->screen->cursor_current.col) ||
	    (cursor.row != terminal->pvt->screen->cursor_current.row)) {
		/* invalidate the old and new cursor positions */
		if (cursor_visible)
			_vte_invalidate_cell(terminal, cursor.col, cursor.row);
		_vte_invalidate_cursor_once(terminal, FALSE);
		/* Signal that the cursor moved. */
		vte_terminal_queue_cursor_moved(terminal);
	} else if (cursor_visible != terminal->pvt->cursor_visible) {
		_vte_invalidate_cell(terminal, cursor.col, cursor.row);
	}

	/* Tell the input method where the cursor is. */
	if (GTK_WIDGET_REALIZED(terminal)) {
		GdkRectangle rect;
		rect.x = terminal->pvt->screen->cursor_current.col *
			 terminal->char_width + VTE_PAD_WIDTH;
		rect.width = terminal->char_width;
		rect.y = (terminal->pvt->screen->cursor_current.row - delta) *
			 terminal->char_height + VTE_PAD_WIDTH;
		rect.height = terminal->char_height;
		gtk_im_context_set_cursor_location(terminal->pvt->im_context,
						   &rect);
	}

	_vte_debug_print (VTE_DEBUG_WORK, ")");
	_vte_debug_print (VTE_DEBUG_IO,
			"%ld chars and %ld bytes in %"G_GSIZE_FORMAT" chunks left to process.\n",
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
					    G_IO_IN | G_IO_HUP,
					    (GIOFunc) vte_terminal_io_read,
					    terminal,
					    (GDestroyNotify) mark_input_source_invalid);
	}
}
static void
_vte_terminal_feed_chunks (VteTerminal *terminal, struct _vte_incoming_chunk *chunks)
{
	struct _vte_incoming_chunk *last;

	_vte_debug_print(VTE_DEBUG_IO, "Feed %"G_GSIZE_FORMAT" bytes, in %"G_GSIZE_FORMAT" chunks.\n",
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
			max_bytes = VTE_MAX_INPUT_READ;
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
			GDK_THREADS_ENTER ();
			vte_terminal_add_process_timeout (terminal);
			GDK_THREADS_LEAVE ();
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
			GDK_THREADS_ENTER ();
			vte_terminal_eof (channel, terminal);
			GDK_THREADS_LEAVE ();
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
 * @data: a string in the terminal's current encoding
 * @length: the length of the string
 *
 * Interprets @data as if it were data received from a child process.  This
 * can either be used to drive the terminal without a child process, or just
 * to mess with your users.
 *
 */
void
vte_terminal_feed(VteTerminal *terminal, const char *data, glong length)
{
	/* If length == -1, use the length of the data string. */
	if (length == ((gssize)-1)) {
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
		      _vte_buffer_length(terminal->pvt->outgoing));
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
		_vte_buffer_consume(terminal->pvt->outgoing, count);
	}

	if (_vte_buffer_length(terminal->pvt->outgoing) == 0) {
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

	conv = VTE_INVALID_CONV;
	if (strcmp(encoding, "UTF-8") == 0) {
		conv = terminal->pvt->outgoing_conv;
	}
	if (conv == VTE_INVALID_CONV) {
		g_warning (_("Unable to send data to child, invalid charset convertor"));
		return;
	}

	icount = length;
	ibuf =  data;
	ocount = ((length + 1) * VTE_UTF8_BPC) + 1;
	_vte_buffer_set_minimum_size(terminal->pvt->conv_buffer, ocount);
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
		if ((cooked_length > 0) && (terminal->pvt->pty_master != -1)) {
			_vte_buffer_append(terminal->pvt->outgoing,
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
vte_terminal_feed_child(VteTerminal *terminal, const char *text, glong length)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (length == ((gssize)-1)) {
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
 *
 * Since: 0.12.1
 */
void
vte_terminal_feed_child_binary(VteTerminal *terminal, const char *data, glong length)
{
	g_assert(VTE_IS_TERMINAL(terminal));

	/* Tell observers that we're sending this to the child. */
	if (length > 0) {
		vte_terminal_emit_commit(terminal,
					 data, length);

		/* If there's a place for it to go, add the data to the
		 * outgoing buffer. */
		if (terminal->pvt->pty_master != -1) {
			_vte_buffer_append(terminal->pvt->outgoing,
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
				  !terminal->pvt->screen->sendrecv_mode,
				  terminal->pvt->screen->linefeed_mode);
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

/* Handle the toplevel being reconfigured. */
static gboolean
vte_terminal_configure_toplevel(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Top level parent configured.\n");

	if (terminal->pvt->bg_transparent) {
		/* We have to repaint the entire window, because we don't get
		 * an expose event unless some portion of our visible area
		 * moved out from behind another window. */
		_vte_invalidate_all(terminal);
	}

	return FALSE;
}

/* Handle a hierarchy-changed signal. */
static void
vte_terminal_hierarchy_changed(GtkWidget *widget, GtkWidget *old_toplevel,
			       gpointer data)
{
	GtkWidget *toplevel;

	_vte_debug_print(VTE_DEBUG_EVENTS, "Hierarchy changed.\n");
	if (old_toplevel != NULL) {
		g_signal_handlers_disconnect_by_func(old_toplevel,
						     vte_terminal_configure_toplevel,
						     widget);
	}

	toplevel = gtk_widget_get_toplevel(widget);
	if (toplevel != NULL) {
		g_signal_connect_swapped (toplevel, "configure-event",
				 G_CALLBACK (vte_terminal_configure_toplevel),
				 widget);
	}
}

static void
vte_terminal_style_set (GtkWidget      *widget,
			GtkStyle       *prev_style)
{
	VteTerminal *terminal;
	if (!GTK_WIDGET_REALIZED(widget)) {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Don't change style if we aren't realized.\n");
		return;
	}

	terminal = VTE_TERMINAL(widget);

        vte_terminal_set_font_full_internal(terminal, terminal->pvt->fontdesc,
                                            terminal->pvt->fontantialias);
}

static void
add_cursor_timeout (VteTerminal *terminal)
{
	terminal->pvt->cursor_blink_time = 0;
	terminal->pvt->cursor_blink_tag = g_timeout_add_full(G_PRIORITY_LOW,
							     terminal->pvt->cursor_blink_cycle,
							     (GSourceFunc)vte_invalidate_cursor_periodic,
							     terminal,
							     NULL);
}

static void
remove_cursor_timeout (VteTerminal *terminal)
{
	g_source_remove (terminal->pvt->cursor_blink_tag);
	terminal->pvt->cursor_blink_tag = 0;
}


void
_vte_terminal_audible_beep(VteTerminal *terminal)
{
	GdkDisplay *display;

	g_assert(VTE_IS_TERMINAL(terminal));
	display = gtk_widget_get_display(&terminal->widget);
	gdk_display_beep(display);
}

void
_vte_terminal_visible_beep(VteTerminal *terminal)
{
	GtkWidget *widget;

	widget = &terminal->widget;
	if (GTK_WIDGET_REALIZED(widget)) {
		/* Fill the screen with the default foreground color, and then
		 * repaint everything, to provide visual bell. */
		gdk_draw_rectangle(widget->window,
				   widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
				   TRUE,
				   0, 0,
				   widget->allocation.width, widget->allocation.height);
		gdk_flush();
		/* Force the repaint. */
		_vte_invalidate_all(terminal); /* max delay of UPDATE_REPEAT_TIMEOUT */
	}
}

void
_vte_terminal_beep(VteTerminal *terminal)
{
	if (terminal->pvt->audible_bell) {
		_vte_terminal_audible_beep (terminal);
	}
	if (terminal->pvt->visible_bell) {
		_vte_terminal_visible_beep (terminal);
	}
}


/*
 * Translate national keys with Crtl|Alt modifier
 * Refactored from http://bugzilla.gnome.org/show_bug.cgi?id=375112
 */
static guint
vte_translate_national_ctrlkeys (GdkEventKey *event)
{
	guint keyval;
	GdkModifierType consumed_modifiers;
	GdkKeymap *keymap;

	keymap = gdk_keymap_get_for_display (
			gdk_drawable_get_display (event->window));

	gdk_keymap_translate_keyboard_state (keymap,
			event->hardware_keycode, event->state,
			0, /* Force 0 group (English) */
			&keyval, NULL, NULL, &consumed_modifiers);

	_vte_debug_print (VTE_DEBUG_EVENTS,
			"ctrl+Key, group=%d de-grouped into keyval=0x%x\n",
			event->group, keyval);

	return keyval;
}

static void
vte_terminal_read_modifiers (VteTerminal *terminal,
			     GdkEvent    *event)
{
	GdkModifierType modifiers;

	/* Read the modifiers. */
	if (gdk_event_get_state((GdkEvent*)event, &modifiers)) {
		terminal->pvt->modifiers = modifiers;

		vte_terminal_set_selection_block_mode (terminal, modifiers & GDK_CONTROL_MASK);
	}
}

/* Read and handle a keypress event. */
static gint
vte_terminal_key_press(GtkWidget *widget, GdkEventKey *event)
{
	VteTerminal *terminal;
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

	terminal = VTE_TERMINAL(widget);

	/* First, check if GtkWidget's behavior already does something with
	 * this key. */
	if (GTK_WIDGET_CLASS(vte_terminal_parent_class)->key_press_event) {
		if ((GTK_WIDGET_CLASS(vte_terminal_parent_class))->key_press_event(widget,
								      event)) {
			return TRUE;
		}
	}

	/* If it's a keypress, record that we got the event, in case the
	 * input method takes the event from us. */
	if (event->type == GDK_KEY_PRESS) {
		/* Store a copy of the key. */
		keyval = event->keyval;
		vte_terminal_read_modifiers (terminal, (GdkEvent*) event);

		/* If we're in margin bell mode and on the border of the
		 * margin, bell. */
		if (terminal->pvt->margin_bell) {
			if ((terminal->pvt->screen->cursor_current.col +
			     (glong) terminal->pvt->bell_margin) ==
			     terminal->column_count) {
				_vte_terminal_beep (terminal);
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

		if (G_UNLIKELY (keyval == GDK_Control_L || keyval == GDK_Control_R))
			vte_terminal_set_selection_block_mode (terminal, TRUE);

		/* Unless it's a modifier key, hide the pointer. */
		if (!modifier) {
			_vte_terminal_set_pointer_visible(terminal, FALSE);
		}

		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Keypress, modifiers=0x%x, "
				"keyval=0x%x, raw string=`%s'.\n",
				terminal->pvt->modifiers,
				keyval, event->string);

		/* We steal many keypad keys here. */
		if (!terminal->pvt->im_preedit_active) {
			switch (keyval) {
			case GDK_KP_Add:
			case GDK_KP_Subtract:
			case GDK_KP_Multiply:
			case GDK_KP_Divide:
			case GDK_KP_Enter:
				steal = TRUE;
				break;
			default:
				break;
			}
			if (terminal->pvt->modifiers & VTE_META_MASK) {
				steal = TRUE;
			}
			switch (keyval) {
			case GDK_Multi_key:
			case GDK_Codeinput:
			case GDK_SingleCandidate:
			case GDK_MultipleCandidate:
			case GDK_PreviousCandidate:
			case GDK_Kanji:
			case GDK_Muhenkan:
			case GDK_Henkan:
			case GDK_Romaji:
			case GDK_Hiragana:
			case GDK_Katakana:
			case GDK_Hiragana_Katakana:
			case GDK_Zenkaku:
			case GDK_Hankaku:
			case GDK_Zenkaku_Hankaku:
			case GDK_Touroku:
			case GDK_Massyo:
			case GDK_Kana_Lock:
			case GDK_Kana_Shift:
			case GDK_Eisu_Shift:
			case GDK_Eisu_toggle:
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
		if (GTK_WIDGET_REALIZED(terminal) &&
		    gtk_im_context_filter_keypress(terminal->pvt->im_context,
						   event)) {
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
		case GDK_BackSpace:
			switch (terminal->pvt->backspace_binding) {
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
				if (terminal->pvt->pty_master != -1 &&
				    tcgetattr(terminal->pvt->pty_master, &tio) != -1)
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
				if (terminal->pvt->pty_master != -1 &&
				    tcgetattr(terminal->pvt->pty_master, &tio) != -1 &&
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
		case GDK_KP_Delete:
		case GDK_Delete:
			switch (terminal->pvt->delete_binding) {
			case VTE_ERASE_ASCII_BACKSPACE:
				normal = g_strdup("\010");
				normal_length = 1;
				break;
			case VTE_ERASE_ASCII_DELETE:
				normal = g_strdup("\177");
				normal_length = 1;
				break;
			case VTE_ERASE_TTY:
				if (terminal->pvt->pty_master != -1 &&
				    tcgetattr(terminal->pvt->pty_master, &tio) != -1)
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
		case GDK_KP_Insert:
		case GDK_Insert:
			if (modifiers & GDK_SHIFT_MASK) {
				if (modifiers & GDK_CONTROL_MASK) {
					vte_terminal_paste_clipboard(terminal);
					handled = TRUE;
					suppress_meta_esc = TRUE;
				} else {
					vte_terminal_paste_primary(terminal);
					handled = TRUE;
					suppress_meta_esc = TRUE;
				}
			} else if (modifiers & GDK_CONTROL_MASK) {
				vte_terminal_copy_clipboard(terminal);
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		/* Keypad/motion keys. */
		case GDK_KP_Up:
		case GDK_Up:
			if (modifiers & GDK_CONTROL_MASK 
                            && modifiers & GDK_SHIFT_MASK) {
				vte_terminal_scroll_lines(terminal, -1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KP_Down:
		case GDK_Down:
			if (modifiers & GDK_CONTROL_MASK
                            && modifiers & GDK_SHIFT_MASK) {
				vte_terminal_scroll_lines(terminal, 1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KP_Page_Up:
		case GDK_Page_Up:
			if (modifiers & GDK_SHIFT_MASK) {
				vte_terminal_scroll_pages(terminal, -1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KP_Page_Down:
		case GDK_Page_Down:
			if (modifiers & GDK_SHIFT_MASK) {
				vte_terminal_scroll_pages(terminal, 1);
				scrolled = TRUE;
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		case GDK_KP_Home:
		case GDK_Home:
			if (modifiers & GDK_SHIFT_MASK) {
				vte_terminal_maybe_scroll_to_top(terminal);
				scrolled = TRUE;
				handled = TRUE;
			}
			break;
		case GDK_KP_End:
		case GDK_End:
			if (modifiers & GDK_SHIFT_MASK) {
				vte_terminal_maybe_scroll_to_bottom(terminal);
				scrolled = TRUE;
				handled = TRUE;
			}
			break;
		/* Let Shift +/- tweak the font, like XTerm does. */
		case GDK_KP_Add:
		case GDK_KP_Subtract:
			if (modifiers &
			    (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) {
				switch (keyval) {
				case GDK_KP_Add:
					vte_terminal_emit_increase_font_size(terminal);
					handled = TRUE;
					suppress_meta_esc = TRUE;
					break;
				case GDK_KP_Subtract:
					vte_terminal_emit_decrease_font_size(terminal);
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
		if (handled == FALSE && terminal->pvt->termcap != NULL) {
			_vte_keymap_map(keyval, modifiers,
					terminal->pvt->sun_fkey_mode,
					terminal->pvt->hp_fkey_mode,
					terminal->pvt->legacy_fkey_mode,
					terminal->pvt->vt220_fkey_mode,
					terminal->pvt->cursor_mode == VTE_KEYMODE_APPLICATION,
					terminal->pvt->keypad_mode == VTE_KEYMODE_APPLICATION,
					terminal->pvt->termcap,
					terminal->pvt->emulation ?
					terminal->pvt->emulation : vte_terminal_get_default_emulation(terminal),
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
		/* If we didn't manage to do anything, try to salvage a
		 * printable string. */
		if (handled == FALSE && normal == NULL && special == NULL) {
			if (event->group &&
					(modifiers & GDK_CONTROL_MASK))
				keyval = vte_translate_national_ctrlkeys(event);

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
			if (terminal->pvt->meta_sends_escape &&
			    !suppress_meta_esc &&
			    (normal_length > 0) &&
			    (modifiers & VTE_META_MASK)) {
				vte_terminal_feed_child(terminal,
							_VTE_CAP_ESC,
							1);
			}
			if (normal_length > 0) {
				vte_terminal_feed_child_using_modes(terminal,
								    normal,
								    normal_length);
			}
			g_free(normal);
		} else
		/* If the key maps to characters, send them to the child. */
		if (special != NULL && terminal->pvt->termcap != NULL) {
			termcap = terminal->pvt->termcap;
			tterm = terminal->pvt->emulation;
			normal = _vte_termcap_find_string_length(termcap,
								 tterm,
								 special,
								 &normal_length);
			_vte_keymap_key_add_key_modifiers(keyval,
							  modifiers,
							  terminal->pvt->sun_fkey_mode,
							  terminal->pvt->hp_fkey_mode,
							  terminal->pvt->legacy_fkey_mode,
							  terminal->pvt->vt220_fkey_mode,
							  terminal->pvt->cursor_mode == VTE_KEYMODE_APPLICATION,
							  &normal,
							  &normal_length);
			output = g_strdup_printf(normal, 1);
			vte_terminal_feed_child_using_modes(terminal,
							    output, -1);
			g_free(output);
			g_free(normal);
		}
		/* Keep the cursor on-screen. */
		if (!scrolled && !modifier &&
		    terminal->pvt->scroll_on_keystroke) {
			vte_terminal_maybe_scroll_to_bottom(terminal);
		}
		return TRUE;
	}
	return FALSE;
}

static gboolean
vte_terminal_key_release(GtkWidget *widget, GdkEventKey *event)
{
	VteTerminal *terminal;

	terminal = VTE_TERMINAL(widget);

	vte_terminal_read_modifiers (terminal, (GdkEvent*) event);

	if (event->type == GDK_KEY_RELEASE) {
		if (G_UNLIKELY (event->keyval == GDK_Control_L ||
				event->keyval == GDK_Control_R))
			vte_terminal_set_selection_block_mode (terminal, FALSE);
	}

	return GTK_WIDGET_REALIZED(terminal) &&
	       gtk_im_context_filter_keypress(terminal->pvt->im_context, event);
}

/**
 * vte_terminal_is_word_char:
 * @terminal: a #VteTerminal
 * @c: a candidate Unicode code point
 *
 * Checks if a particular character is considered to be part of a word or not,
 * based on the values last passed to vte_terminal_set_word_chars().
 *
 * Returns: %TRUE if the character is considered to be part of a word
 */
gboolean
vte_terminal_is_word_char(VteTerminal *terminal, gunichar c)
{
	guint i;
	VteWordCharRange *range;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);

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
vte_same_class(VteTerminal *terminal, glong acol, glong arow,
	       glong bcol, glong brow)
{
	const VteCell *pcell = NULL;
	gboolean word_char;
	if ((pcell = vte_terminal_find_charcell(terminal, acol, arow)) != NULL && pcell->c != 0) {
		word_char = vte_terminal_is_word_char(terminal, _vte_unistr_get_base (pcell->c));

		/* Lets not group non-wordchars together (bug #25290) */
		if (!word_char)
			return FALSE;

		pcell = vte_terminal_find_charcell(terminal, bcol, brow);
		if (pcell == NULL || pcell->c == 0) {
			return FALSE;
		}
		if (word_char != vte_terminal_is_word_char(terminal, _vte_unistr_get_base (pcell->c))) {
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
vte_cell_is_selected(VteTerminal *terminal, glong col, glong row, gpointer data)
{
	struct selection_cell_coords ss, se;

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
vte_terminal_paste_cb(GtkClipboard *clipboard, const gchar *text, gpointer data)
{
	VteTerminal *terminal;
	gchar *paste, *p;
	long length;
	terminal = data;
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
		vte_terminal_feed_child(terminal, paste, length);
		g_free(paste);
	}
}

static void
vte_terminal_get_mouse_tracking_info (VteTerminal   *terminal,
				      int            button,
				      long           col,
				      long           row,
				      unsigned char *pb,
				      unsigned char *px,
				      unsigned char *py)
{
	unsigned char cb = 0, cx = 0, cy = 0;

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

	/* Encode the cursor coordinates. */
	cx = 32 + CLAMP(1 + col,
			1, terminal->column_count);
	cy = 32 + CLAMP(1 + row,
			1, terminal->row_count);;

	*pb = cb;
	*px = cx;
	*py = cy;
}

static void
vte_terminal_send_mouse_button_internal(VteTerminal *terminal,
					int          button,
					long         x,
					long         y)
{
	unsigned char cb, cx, cy;
	char buf[LINE_MAX];
	gint len;
	int width = terminal->char_width;
	int height = terminal->char_height;
	long col = (x - VTE_PAD_WIDTH) / width;
	long row = (y - VTE_PAD_WIDTH) / height;

	vte_terminal_get_mouse_tracking_info (terminal,
					      button, col, row,
					      &cb, &cx, &cy);

	/* Send event direct to the child, this is binary not text data */
	len = g_snprintf(buf, sizeof(buf), _VTE_CAP_CSI "M%c%c%c", cb, cx, cy);
	vte_terminal_feed_child_binary(terminal, buf, len);
}

/* Send a mouse button click/release notification. */
static void
vte_terminal_maybe_send_mouse_button(VteTerminal *terminal,
				     GdkEventButton *event)
{
	/* Read the modifiers. */
	vte_terminal_read_modifiers (terminal, (GdkEvent*) event);

	/* Decide whether or not to do anything. */
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

	vte_terminal_send_mouse_button_internal(terminal,
					        (event->type == GDK_BUTTON_PRESS) ? event->button : 0,
						event->x, event->y);
}

/* Send a mouse motion notification. */
static void
vte_terminal_maybe_send_mouse_drag(VteTerminal *terminal, GdkEventMotion *event)
{
	unsigned char cb, cx, cy;
	char buf[LINE_MAX];
	gint len;
	int width = terminal->char_width;
	int height = terminal->char_height;
	long col = ((long) event->x - VTE_PAD_WIDTH) / width;
	long row = ((long) event->y - VTE_PAD_WIDTH) / height;

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
			if (col == terminal->pvt->mouse_last_x / width &&
			    row == terminal->pvt->mouse_last_y / height)
				return;
		}
		break;
	default:
		return;
		break;
	}

	vte_terminal_get_mouse_tracking_info (terminal,
					      terminal->pvt->mouse_last_button, col, row,
					      &cb, &cx, &cy);
	cb += 32; /* for movement */

	/* Send event direct to the child, this is binary not text data */
	len = g_snprintf(buf, sizeof(buf), _VTE_CAP_CSI "M%c%c%c", cb, cx, cy);
	vte_terminal_feed_child_binary(terminal, buf, len);
}

/* Clear all match hilites. */
static void
vte_terminal_match_hilite_clear(VteTerminal *terminal)
{
	long srow, scolumn, erow, ecolumn;
	srow = terminal->pvt->match_start.row;
	scolumn = terminal->pvt->match_start.column;
	erow = terminal->pvt->match_end.row;
	ecolumn = terminal->pvt->match_end.column;
	terminal->pvt->match_start.row = -1;
	terminal->pvt->match_start.column = -1;
	terminal->pvt->match_end.row = -2;
	terminal->pvt->match_end.column = -2;
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
cursor_inside_match (VteTerminal *terminal, long x, long y)
{
	gint width = terminal->char_width;
	gint height = terminal->char_height;
	glong col = x / width;
	glong row = y / height + terminal->pvt->screen->scroll_delta;
	if (terminal->pvt->match_start.row == terminal->pvt->match_end.row) {
		return row == terminal->pvt->match_start.row &&
			col >= terminal->pvt->match_start.column &&
			col <= terminal->pvt->match_end.column;
	} else {
		if (row < terminal->pvt->match_start.row ||
				row > terminal->pvt->match_end.row) {
			return FALSE;
		}
		if (row == terminal->pvt->match_start.row) {
			return col >= terminal->pvt->match_start.column;
		}
		if (row == terminal->pvt->match_end.row) {
			return col <= terminal->pvt->match_end.column;
		}
		return TRUE;
	}
}

static void
vte_terminal_match_hilite_show(VteTerminal *terminal, long x, long y)
{
	if(terminal->pvt->match != NULL && !terminal->pvt->show_match){
		if (cursor_inside_match (terminal, x, y)) {
			_vte_invalidate_region (terminal,
					terminal->pvt->match_start.column,
					terminal->pvt->match_end.column,
					terminal->pvt->match_start.row,
					terminal->pvt->match_end.row,
					FALSE);
			terminal->pvt->show_match = TRUE;
		}
	}
}
static void
vte_terminal_match_hilite_hide(VteTerminal *terminal)
{
	if(terminal->pvt->match != NULL && terminal->pvt->show_match){
		_vte_invalidate_region (terminal,
				terminal->pvt->match_start.column,
				terminal->pvt->match_end.column,
				terminal->pvt->match_start.row,
				terminal->pvt->match_end.row,
				FALSE);
		terminal->pvt->show_match = FALSE;
	}
}


static void
vte_terminal_match_hilite_update(VteTerminal *terminal, long x, long y)
{
	int start, end, width, height;
	char *match;
	struct _VteCharAttributes *attr;
	VteScreen *screen;
	long delta;

	width = terminal->char_width;
	height = terminal->char_height;

	/* Check for matches. */
	screen = terminal->pvt->screen;
	delta = screen->scroll_delta;

	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Match hilite update (%ld, %ld) -> %ld, %ld\n",
			x, y,
			x / width,
			y / height + delta);

	match = vte_terminal_match_check_internal(terminal,
						  x / width,
						  y / height + delta,
						  &terminal->pvt->match_tag,
						  &start,
						  &end);
	if (terminal->pvt->show_match) {
		/* Repaint what used to be hilited, if anything. */
		_vte_invalidate_region(terminal,
				terminal->pvt->match_start.column,
				terminal->pvt->match_end.column,
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
		terminal->pvt->match_start.column = attr->column;

		attr = NULL;
		if ((guint) end < terminal->pvt->match_attributes->len) {
			attr = &g_array_index(terminal->pvt->match_attributes,
					struct _VteCharAttributes,
					end);
			terminal->pvt->match_end.row = attr->row;
			terminal->pvt->match_end.column = attr->column;
		}
	}
	if (attr == NULL) { /* i.e. if either endpoint is not found */
		terminal->pvt->match_start.row = -1;
		terminal->pvt->match_start.column = -1;
		terminal->pvt->match_end.row = -2;
		terminal->pvt->match_end.column = -2;
		g_assert (match == NULL);
	}

	g_free (terminal->pvt->match);
	terminal->pvt->match = match;

	/* If there are no matches, repaint what we had matched before. */
	if (match == NULL) {
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"No matches. [(%ld,%ld) to (%ld,%ld)]\n",
				terminal->pvt->match_start.column,
				terminal->pvt->match_start.row,
				terminal->pvt->match_end.column,
				terminal->pvt->match_end.row);
		terminal->pvt->show_match = FALSE;
	} else {
		terminal->pvt->show_match = TRUE;
		/* Repaint the newly-hilited area. */
		_vte_invalidate_region(terminal,
				terminal->pvt->match_start.column,
				terminal->pvt->match_end.column,
				terminal->pvt->match_start.row,
				terminal->pvt->match_end.row,
				FALSE);
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Matched (%ld,%ld) to (%ld,%ld).\n",
				terminal->pvt->match_start.column,
				terminal->pvt->match_start.row,
				terminal->pvt->match_end.column,
				terminal->pvt->match_end.row);
	}
}
/* Update the hilited text if the pointer has moved to a new character cell. */
static void
vte_terminal_match_hilite(VteTerminal *terminal, long x, long y)
{
	int width, height;

	width = terminal->char_width;
	height = terminal->char_height;

	/* if the cursor is not above a cell, skip */
	if (x < 0 || x > terminal->widget.allocation.width
			|| y < 0 || y > terminal->widget.allocation.height) {
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

	vte_terminal_match_hilite_update(terminal, x, y);
}


/* Note that the clipboard has cleared. */
static void
vte_terminal_clear_cb(GtkClipboard *clipboard, gpointer owner)
{
	VteTerminal *terminal;
	terminal = owner;
	if (terminal->pvt->has_selection) {
		_vte_debug_print(VTE_DEBUG_SELECTION, "Lost selection.\n");
		vte_terminal_deselect_all(terminal);
	}
}

/* Supply the selected text to the clipboard. */
static void
vte_terminal_copy_cb(GtkClipboard *clipboard, GtkSelectionData *data,
		     guint info, gpointer owner)
{
	VteTerminal *terminal;
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
 * @terminal: terminal in which the cell is.
 * @column: column in which the cell is.
 * @row: row in which the cell is.
 * @data: user data.
 *
 * Specifies the type of a selection function used to check whether
 * a cell has to be selected or not.
 *
 * Returns: %TRUE if cell has to be selected; %FALSE if otherwise.
 */

/**
 * vte_terminal_get_text_range:
 * @terminal: a #VteTerminal
 * @start_row: first row to search for data
 * @start_col: first column to search for data
 * @end_row: last row to search for data
 * @end_col: last column to search for data
 * @is_selected: a callback
 * @data: user data to be passed to the callback
 * @attributes: location for storing text attributes
 *
 * Extracts a view of the visible part of the terminal.  If @is_selected is not
 * %NULL, characters will only be read if @is_selected returns %TRUE after being
 * passed the column and row, respectively.  A #VteCharAttributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics.  The
 * entire scrollback buffer is scanned, so it is possible to read the entire
 * contents of the buffer using this function.
 *
 * Returns: a text string which must be freed by the caller, or %NULL.
 */
char *
vte_terminal_get_text_range(VteTerminal *terminal,
			    glong start_row, glong start_col,
			    glong end_row, glong end_col,
			    VteSelectionFunc is_selected,
			    gpointer data,
			    GArray *attributes)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return vte_terminal_get_text_range_maybe_wrapped(terminal,
							 start_row, start_col,
							 end_row, end_col,
							 TRUE,
							 is_selected,
							 data,
							 attributes,
							 FALSE);
}

static char *
vte_terminal_get_text_range_maybe_wrapped(VteTerminal *terminal,
					  glong start_row, glong start_col,
					  glong end_row, glong end_col,
					  gboolean wrap,
					  VteSelectionFunc is_selected,
					  gpointer data,
					  GArray *attributes,
					  gboolean include_trailing_spaces)
{
	long col, row, last_empty, last_emptycol, last_nonempty, last_nonemptycol;
	VteScreen *screen;
	const VteCell *pcell = NULL;
	GString *string;
	struct _VteCharAttributes attr;
	struct vte_palette_entry fore, back, *palette;

	screen = terminal->pvt->screen;

	if (attributes)
		g_array_set_size (attributes, 0);

	string = g_string_new(NULL);
	memset(&attr, 0, sizeof(attr));

	palette = terminal->pvt->palette;
	col = start_col;
	for (row = start_row; row <= end_row; row++, col = 0) {
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
				if ((row == end_row) && (col == end_col)) {
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
		attr.column = MAX(terminal->column_count, attr.column + 1);

		/* Add a newline in block mode. */
		if (terminal->pvt->selection_block_mode) {
			string = g_string_append_c(string, '\n');
		}
		/* Else, if the last visible column on this line was selected and
		 * not soft-wrapped, append a newline. */
		else if (is_selected(terminal, terminal->column_count - 1, row, data)) {
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
	return g_string_free(string, FALSE);
}

static char *
vte_terminal_get_text_maybe_wrapped(VteTerminal *terminal,
				    gboolean wrap,
				    VteSelectionFunc is_selected,
				    gpointer data,
				    GArray *attributes,
				    gboolean include_trailing_spaces)
{
	long start_row, start_col, end_row, end_col;
	start_row = terminal->pvt->screen->scroll_delta;
	start_col = 0;
	end_row = start_row + terminal->row_count - 1;
	end_col = terminal->column_count - 1;
	return vte_terminal_get_text_range_maybe_wrapped(terminal,
							 start_row, start_col,
							 end_row, end_col,
							 wrap,
							 is_selected ?
							 is_selected :
							 always_selected,
							 data,
							 attributes,
							 include_trailing_spaces);
}

/**
 * vte_terminal_get_text:
 * @terminal: a #VteTerminal
 * @is_selected: a callback
 * @data: user data to be passed to the callback
 * @attributes: location for storing text attributes
 *
 * Extracts a view of the visible part of the terminal.  If @is_selected is not
 * %NULL, characters will only be read if @is_selected returns %TRUE after being
 * passed the column and row, respectively.  A #VteCharAttributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics.
 *
 * Returns: a text string which must be freed by the caller, or %NULL.
 */
char *
vte_terminal_get_text(VteTerminal *terminal,
		      VteSelectionFunc is_selected,
		      gpointer data,
		      GArray *attributes)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return vte_terminal_get_text_maybe_wrapped(terminal,
						   TRUE,
						   is_selected ?
						   is_selected :
						   always_selected,
						   data,
						   attributes,
						   FALSE);
}

/**
 * vte_terminal_get_text_include_trailing_spaces:
 * @terminal: a #VteTerminal
 * @is_selected: a callback
 * @data: user data to be passed to the callback
 * @attributes: location for storing text attributes
 *
 * Extracts a view of the visible part of the terminal.  If @is_selected is not
 * %NULL, characters will only be read if @is_selected returns %TRUE after being
 * passed the column and row, respectively.  A #VteCharAttributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics. This function
 * differs from vte_terminal_get_text() in that trailing spaces at the end of
 * lines are included.
 *
 * Returns: a text string which must be freed by the caller, or %NULL.
 *
 * Since: 0.11.11
 */
char *
vte_terminal_get_text_include_trailing_spaces(VteTerminal *terminal,
					      VteSelectionFunc is_selected,
					      gpointer data,
					      GArray *attributes)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return vte_terminal_get_text_maybe_wrapped(terminal,
						   TRUE,
						   is_selected ?
						   is_selected :
						   always_selected,
						   data,
						   attributes,
						   TRUE);
}

/**
 * vte_terminal_get_cursor_position:
 * @terminal: a #VteTerminal
 * @column: long which will hold the column
 * @row : long which will hold the row
 *
 * Reads the location of the insertion cursor and returns it.  The row
 * coordinate is absolute.
 *
 */
void
vte_terminal_get_cursor_position(VteTerminal *terminal,
				 glong *column, glong *row)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (column) {
		*column = terminal->pvt->screen->cursor_current.col;
	}
	if (row) {
		*row = terminal->pvt->screen->cursor_current.row;
	}
}

static GtkClipboard *
vte_terminal_clipboard_get(VteTerminal *terminal, GdkAtom board)
{
	GdkDisplay *display;
	display = gtk_widget_get_display(&terminal->widget);
	return gtk_clipboard_get_for_display(display, board);
}

/* Place the selected text onto the clipboard.  Do this asynchronously so that
 * we get notified when the selection we placed on the clipboard is replaced. */
static void
vte_terminal_copy(VteTerminal *terminal, GdkAtom board)
{
	GtkClipboard *clipboard;
	static GtkTargetEntry *targets = NULL;
	static gint n_targets = 0;

	clipboard = vte_terminal_clipboard_get(terminal, board);

	/* Chuck old selected text and retrieve the newly-selected text. */
	g_free(terminal->pvt->selection);
	terminal->pvt->selection =
		vte_terminal_get_text_range(terminal,
					    terminal->pvt->selection_start.row,
					    0,
					    terminal->pvt->selection_end.row,
					    terminal->column_count,
					    vte_cell_is_selected,
					    NULL,
					    NULL);
	terminal->pvt->has_selection = TRUE;

	/* Place the text on the clipboard. */
	if (terminal->pvt->selection != NULL) {
		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Assuming ownership of selection.\n");
		if (!targets) {
			GtkTargetList *list;
			GList *l;
			int i;

			list = gtk_target_list_new (NULL, 0);
			gtk_target_list_add_text_targets (list, 0);

			n_targets = g_list_length (list->list);
			targets = g_new0 (GtkTargetEntry, n_targets);
			for (l = list->list, i = 0; l; l = l->next, i++) {
				GtkTargetPair *pair = (GtkTargetPair *)l->data;
				targets[i].target = gdk_atom_name (pair->target);
			}
			gtk_target_list_unref (list);
		}

		gtk_clipboard_set_with_owner(clipboard,
					     targets,
					     n_targets,
					     vte_terminal_copy_cb,
					     vte_terminal_clear_cb,
					     G_OBJECT(terminal));
		gtk_clipboard_set_can_store(clipboard, NULL, 0);
	}
}

/* Paste from the given clipboard. */
static void
vte_terminal_paste(VteTerminal *terminal, GdkAtom board)
{
	GtkClipboard *clipboard;
	clipboard = vte_terminal_clipboard_get(terminal, board);
	if (clipboard != NULL) {
		_vte_debug_print(VTE_DEBUG_SELECTION,
				"Requesting clipboard contents.\n");
		gtk_clipboard_request_text(clipboard,
					   vte_terminal_paste_cb,
					   terminal);
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


/* Start selection at the location of the event. */
static void
vte_terminal_start_selection(VteTerminal *terminal, GdkEventButton *event,
			     enum vte_selection_type selection_type)
{
	long delta;

	if (terminal->pvt->selection_block_mode)
		selection_type = selection_type_char;

	/* Record that we have the selection, and where it started. */
	delta = terminal->pvt->screen->scroll_delta;
	terminal->pvt->has_selection = TRUE;
	terminal->pvt->selection_last.x = event->x - VTE_PAD_WIDTH;
	terminal->pvt->selection_last.y = event->y - VTE_PAD_WIDTH +
					  (terminal->char_height * delta);

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

	_vte_debug_print(VTE_DEBUG_SELECTION,
			"Selection started at (%ld,%ld).\n",
			terminal->pvt->selection_start.col,
			terminal->pvt->selection_start.row);

	/* Temporarily stop caring about input from the child. */
	_vte_terminal_disconnect_pty_read(terminal);
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
	struct selection_cell_coords *sc, *ec;

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
		 * endpoint as far right as we can expect. */
		if (ec->col >= i) {
			ec->col = MAX(ec->col,
				    MAX(terminal->column_count - 1,
					(long) _vte_row_data_length (rowdata)));
		}
	} else {
		/* Snap to the rightmost column, only if selecting anything of
		 * this row. */
		if (ec->col >= 0)
			ec->col = MAX(ec->col, terminal->column_count - 1);
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
				 terminal->column_count - 1;
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
						   terminal->column_count - 1,
						   j - 1,
						   0,
						   j)) {
					/* Move on to the previous line. */
					j--;
					sc->col = terminal->column_count - 1;
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
			     i < terminal->column_count - 1;
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
			if (i < terminal->column_count - 1) {
				/* We hit a stopping point, so stop. */
				break;
			} else {
				if (vte_line_is_wrappable(terminal, j) &&
				    vte_same_class(terminal,
						   terminal->column_count - 1,
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
		j = ec->row;
		while (_vte_ring_contains (screen->row_data, j) &&
		       vte_line_is_wrappable(terminal, j)) {
			j++;
			ec->row = j;
		}
		/* Make sure we include all of the last line. */
		ec->col = terminal->column_count - 1;
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
vte_terminal_extend_selection(VteTerminal *terminal, long x, long y,
			      gboolean always_grow, gboolean force)
{
	VteScreen *screen;
	int width, height;
	long delta, residual;
	struct selection_event_coords *origin, *last, *start, *end;
	struct selection_cell_coords old_start, old_end, *sc, *ec, *so, *eo;
	gboolean invalidate_selected = FALSE;
	gboolean had_selection;

	height = terminal->char_height;
	width = terminal->char_width;

	/* Confine y into the visible area. (#563024) */
	if (y < 0) {
		y = 0;
		if (!terminal->pvt->selection_block_mode)
			x = 0;
	} else if (y >= terminal->row_count * height) {
		if (!terminal->pvt->selection_block_mode) {
			y = terminal->row_count * height;
			x = -1;
		} else {
			y = terminal->row_count * height - 1;
		}
	}

	screen = terminal->pvt->screen;
	old_start = terminal->pvt->selection_start;
	old_end = terminal->pvt->selection_end;
	so = &old_start;
	eo = &old_end;

	/* Convert the event coordinates to cell coordinates. */
	delta = screen->scroll_delta;

	/* If we're restarting on a drag, then mark this as the start of
	 * the selected block. */
	if (terminal->pvt->selecting_restart) {
		vte_terminal_deselect_all(terminal);
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
	 * We round to closest logical position (positions are located between
	 * cells).  But we don't want to fully round.  So we divide the cell
	 * width/height into three parts.  The side parts round to their nearest
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
		vte_terminal_invalidate_selection (terminal);
	}

	_vte_debug_print(VTE_DEBUG_SELECTION,
			"Selection changed to "
			"(%ld,%ld) to (%ld,%ld).\n",
			sc->col, sc->row, ec->col, ec->row);

	vte_terminal_copy_primary(terminal);
	vte_terminal_emit_selection_changed(terminal);
}

static void
vte_terminal_set_selection_block_mode (VteTerminal *terminal,
				       gboolean     selection_block_mode)
{
	if (G_LIKELY (!terminal->pvt->has_selection))
		return;

	if (G_LIKELY (terminal->pvt->mouse_last_button != 1))
		return;

	selection_block_mode = !!selection_block_mode;

	if (terminal->pvt->selection_block_mode != selection_block_mode) {
		vte_terminal_invalidate_selection (terminal);
		terminal->pvt->selection_block_mode = selection_block_mode;
		vte_terminal_extend_selection(terminal,
					      terminal->pvt->mouse_last_x,
					      terminal->pvt->mouse_last_y,
					      FALSE, TRUE);
		vte_terminal_invalidate_selection (terminal);
	}
}


/**
 * vte_terminal_select_all:
 * @terminal: a #VteTerminal
 *
 * Selects all text within the terminal (including the scrollback buffer).
 *
 * Since: 0.16
 */
void
vte_terminal_select_all (VteTerminal *terminal)
{
	long delta;

	g_return_if_fail (VTE_IS_TERMINAL (terminal));

	vte_terminal_deselect_all (terminal);

	delta = terminal->pvt->screen->scroll_delta;

	terminal->pvt->has_selection = TRUE;
	terminal->pvt->selecting_had_delta = TRUE;
	terminal->pvt->selecting_restart = FALSE;

	terminal->pvt->selection_start.col = 0;
	terminal->pvt->selection_start.row = 0;
	terminal->pvt->selection_end.col = terminal->column_count;
	terminal->pvt->selection_end.row = delta + terminal->row_count;

	_vte_debug_print(VTE_DEBUG_SELECTION, "Selecting *all* text.\n");

	g_free (terminal->pvt->selection);
	terminal->pvt->selection =
		vte_terminal_get_text_range (terminal,
				0, 0,
				delta + terminal->row_count,
				terminal->column_count,
				vte_cell_is_selected,
				NULL, NULL);

	vte_terminal_emit_selection_changed (terminal);
	_vte_invalidate_all (terminal);
}

/**
 * vte_terminal_select_none:
 * @terminal: a #VteTerminal
 *
 * Clears the current selection.
 *
 * Since: 0.16
 */
void
vte_terminal_select_none (VteTerminal *terminal)
{
	g_return_if_fail (VTE_IS_TERMINAL (terminal));

	_vte_debug_print(VTE_DEBUG_SELECTION, "Clearing selection.\n");

	vte_terminal_deselect_all (terminal);
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
		if (terminal->adjustment) {
			/* Try to scroll up by one line. */
			adj = CLAMP(terminal->pvt->screen->scroll_delta - 1,
				    terminal->adjustment->lower,
				    MAX (terminal->adjustment->lower,
					    terminal->adjustment->upper -
					    terminal->row_count));
			vte_terminal_queue_adjustment_value_changed (terminal, adj);
			extend = TRUE;
		}
		_vte_debug_print(VTE_DEBUG_EVENTS, "Autoscrolling down.\n");
	}
	if (terminal->pvt->mouse_last_y >=
	    terminal->row_count * terminal->char_height) {
		if (terminal->adjustment) {
			/* Try to scroll up by one line. */
			adj = CLAMP(terminal->pvt->screen->scroll_delta + 1,
				    terminal->adjustment->lower,
				    MAX (terminal->adjustment->lower,
					    terminal->adjustment->upper -
					    terminal->row_count));
			vte_terminal_queue_adjustment_value_changed (terminal, adj);
			extend = TRUE;
		}
		_vte_debug_print(VTE_DEBUG_EVENTS, "Autoscrolling up.\n");
	}
	if (extend) {
		/* Don't select off-screen areas.  That just confuses people. */
		xmax = terminal->column_count * terminal->char_width;
		ymax = terminal->row_count * terminal->char_height;

		x = CLAMP(terminal->pvt->mouse_last_x, 0, xmax);
		y = CLAMP(terminal->pvt->mouse_last_y, 0, ymax);
		/* If we clamped the Y, mess with the X to get the entire
		 * lines. */
		if (terminal->pvt->mouse_last_y < 0 && !terminal->pvt->selection_block_mode) {
			x = 0;
		}
		if (terminal->pvt->mouse_last_y >= ymax && !terminal->pvt->selection_block_mode) {
			x = terminal->column_count * terminal->char_width;
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
					   666 / terminal->row_count,
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

/* Read and handle a motion event. */
static gboolean
vte_terminal_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
	VteTerminal *terminal;
	int width, height;
	long x, y;

	/* check to see if it matters */
	if (!GTK_WIDGET_DRAWABLE(widget)) {
		return TRUE;
	}

	terminal = VTE_TERMINAL(widget);
	x = event->x - VTE_PAD_WIDTH;
	y = event->y - VTE_PAD_WIDTH;
	width = terminal->char_width;
	height = terminal->char_height;

	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Motion notify (%ld,%ld) [%ld, %ld].\n",
			x, y,
			x / width, y / height + terminal->pvt->screen->scroll_delta);

	/* Read the modifiers. */
	vte_terminal_read_modifiers (terminal, (GdkEvent*) event);

	if (terminal->pvt->mouse_last_button) {
		vte_terminal_match_hilite_hide (terminal);
	} else {
		/* Hilite any matches. */
		vte_terminal_match_hilite(terminal, x, y);
		/* Show the cursor. */
		_vte_terminal_set_pointer_visible(terminal, TRUE);
	}

	switch (event->type) {
	case GDK_MOTION_NOTIFY:
		switch (terminal->pvt->mouse_last_button) {
		case 1:
			_vte_debug_print(VTE_DEBUG_EVENTS, "Mousing drag 1.\n");
			if ((terminal->pvt->modifiers & GDK_SHIFT_MASK) ||
			    !terminal->pvt->mouse_tracking_mode) {
				vte_terminal_extend_selection(terminal,
							      x, y, FALSE, FALSE);
			} else {
				vte_terminal_maybe_send_mouse_drag(terminal,
								   event);
			}
			break;
		default:
			vte_terminal_maybe_send_mouse_drag(terminal, event);
			break;
		}
		break;
	default:
		break;
	}

	/* Start scrolling if we need to. */
	if (event->y < VTE_PAD_WIDTH ||
	    event->y >= terminal->row_count * height + VTE_PAD_WIDTH) {
		switch (terminal->pvt->mouse_last_button) {
		case 1:
			if (!terminal->pvt->mouse_tracking_mode) {
				/* Give mouse wigglers something. */
				vte_terminal_autoscroll(terminal);
				/* Start a timed autoscroll if we're not doing it
				 * already. */
				vte_terminal_start_autoscroll(terminal);
			}
			break;
		case 2:
		case 3:
		default:
			break;
		}
	}

	/* Save the pointer coordinates for later use. */
	terminal->pvt->mouse_last_x = x;
	terminal->pvt->mouse_last_y = y;

	return TRUE;
}

/* Read and handle a pointing device buttonpress event. */
static gint
vte_terminal_button_press(GtkWidget *widget, GdkEventButton *event)
{
	VteTerminal *terminal;
	long height, width, delta;
	gboolean handled = FALSE;
	gboolean start_selecting = FALSE, extend_selecting = FALSE;
	long cellx, celly;
	long x,y;

	x = event->x - VTE_PAD_WIDTH;
	y = event->y - VTE_PAD_WIDTH;

	terminal = VTE_TERMINAL(widget);
	height = terminal->char_height;
	width = terminal->char_width;
	delta = terminal->pvt->screen->scroll_delta;

	/* Hilite any matches. */
	vte_terminal_match_hilite(terminal, x, y);

	_vte_terminal_set_pointer_visible(terminal, TRUE);

	/* Read the modifiers. */
	vte_terminal_read_modifiers (terminal, (GdkEvent*) event);

	/* Convert the event coordinates to cell coordinates. */
	cellx = x / width;
	celly = y / height + delta;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Button %d single-click at (%ld,%ld)\n",
				event->button,
				x, y + terminal->char_height * delta);
		/* Handle this event ourselves. */
		switch (event->button) {
		case 1:
			_vte_debug_print(VTE_DEBUG_EVENTS,
					"Handling click ourselves.\n");
			/* Grab focus. */
			if (!GTK_WIDGET_HAS_FOCUS(widget)) {
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
				    !vte_cell_is_selected(terminal,
							  cellx,
							  celly,
							  NULL)) {
					extend_selecting = TRUE;
				} else {
					start_selecting = TRUE;
				}
			}
			if (start_selecting) {
				vte_terminal_deselect_all(terminal);
				vte_terminal_start_selection(terminal,
							     event,
							     selection_type_char);
				handled = TRUE;
			}
			if (extend_selecting) {
				vte_terminal_extend_selection(terminal,
							      x, y,
							      !terminal->pvt->selecting_restart, TRUE);
				handled = TRUE;
			}
			break;
		/* Paste if the user pressed shift or we're not sending events
		 * to the app. */
		case 2:
			if ((terminal->pvt->modifiers & GDK_SHIFT_MASK) ||
			    !terminal->pvt->mouse_tracking_mode) {
				vte_terminal_paste_primary(terminal);
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
			vte_terminal_maybe_send_mouse_button(terminal, event);
			handled = TRUE;
		}
		break;
	case GDK_2BUTTON_PRESS:
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Button %d double-click at (%ld,%ld)\n",
				event->button,
				x, y + (terminal->char_height * delta));
		switch (event->button) {
		case 1:
			if ((terminal->pvt->modifiers & GDK_SHIFT_MASK) ||
			    !terminal->pvt->mouse_tracking_mode) {
				vte_terminal_start_selection(terminal,
							     event,
							     selection_type_word);
				vte_terminal_extend_selection(terminal,
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
				x, y + (terminal->char_height * delta));
		switch (event->button) {
		case 1:
			if ((terminal->pvt->modifiers & GDK_SHIFT_MASK) ||
			    !terminal->pvt->mouse_tracking_mode) {
				vte_terminal_start_selection(terminal,
							     event,
							     selection_type_line);
				vte_terminal_extend_selection(terminal,
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

	return TRUE;
}

/* Read and handle a pointing device buttonrelease event. */
static gint
vte_terminal_button_release(GtkWidget *widget, GdkEventButton *event)
{
	VteTerminal *terminal;
	gboolean handled = FALSE;
	int x, y;

	x = event->x - VTE_PAD_WIDTH;
	y = event->y - VTE_PAD_WIDTH;

	terminal = VTE_TERMINAL(widget);

	/* Hilite any matches. */
	vte_terminal_match_hilite(terminal, x, y);

	_vte_terminal_set_pointer_visible(terminal, TRUE);

	/* Disconnect from autoscroll requests. */
	vte_terminal_stop_autoscroll(terminal);

	/* Read the modifiers. */
	vte_terminal_read_modifiers (terminal, (GdkEvent*) event);

	switch (event->type) {
	case GDK_BUTTON_RELEASE:
		_vte_debug_print(VTE_DEBUG_EVENTS,
				"Button %d released at (%d,%d).\n",
				event->button, x, y);
		switch (event->button) {
		case 1:
			/* If Shift is held down, or we're not in events mode,
			 * copy the selected text. */
			if (terminal->pvt->selecting || !terminal->pvt->mouse_tracking_mode) {
				/* Copy only if something was selected. */
				if (terminal->pvt->has_selection &&
				    !terminal->pvt->selecting_restart &&
				    terminal->pvt->selecting_had_delta) {
					vte_terminal_copy_primary(terminal);
				}
				terminal->pvt->selecting = FALSE;
				handled = TRUE;
			}
			/* Reconnect to input from the child if we paused it. */
			_vte_terminal_connect_pty_read(terminal);
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
		if (handled == FALSE) {
			vte_terminal_maybe_send_mouse_button(terminal, event);
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

	return TRUE;
}

/* Handle receiving or losing focus. */
static gboolean
vte_terminal_focus_in(GtkWidget *widget, GdkEventFocus *event)
{
	VteTerminal *terminal;

	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus in.\n");

	terminal = VTE_TERMINAL(widget);
	GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);
	/* Read the keyboard modifiers, though they're probably garbage. */
	vte_terminal_read_modifiers (terminal, (GdkEvent*) event);

	/* We only have an IM context when we're realized, and there's not much
	 * point to painting the cursor if we don't have a window. */
	if (GTK_WIDGET_REALIZED(widget)) {
		terminal->pvt->cursor_blink_state = TRUE;

		if (terminal->pvt->cursor_blinks &&
		    terminal->pvt->cursor_blink_tag == 0)
			add_cursor_timeout (terminal);

		gtk_im_context_focus_in(terminal->pvt->im_context);
		_vte_invalidate_cursor_once(terminal, FALSE);
		_vte_terminal_set_pointer_visible(terminal, TRUE);
	}

	return FALSE;
}

static gboolean
vte_terminal_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
	VteTerminal *terminal;
	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus out.\n");
	terminal = VTE_TERMINAL(widget);
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
	/* Read the keyboard modifiers, though they're probably garbage. */
	vte_terminal_read_modifiers (terminal, (GdkEvent*) event);
	/* We only have an IM context when we're realized, and there's not much
	 * point to painting ourselves if we don't have a window. */
	if (GTK_WIDGET_REALIZED(widget)) {
		gtk_im_context_focus_out(terminal->pvt->im_context);
		_vte_invalidate_cursor_once(terminal, FALSE);

		/* XXX Do we want to hide the match just because the terminal
		 * lost keyboard focus, but the pointer *is* still within our
		 * area top? */
		vte_terminal_match_hilite_hide (terminal);
		/* Mark the cursor as invisible to disable hilite updating */
		terminal->pvt->mouse_cursor_visible = FALSE;
	}

	if (terminal->pvt->cursor_blink_tag != 0)
		remove_cursor_timeout (terminal);

	return FALSE;
}

static gboolean
vte_terminal_enter(GtkWidget *widget, GdkEventCrossing *event)
{
	gboolean ret = FALSE;
	_vte_debug_print(VTE_DEBUG_EVENTS, "Enter.\n");
	if (GTK_WIDGET_CLASS (vte_terminal_parent_class)->enter_notify_event) {
		ret = GTK_WIDGET_CLASS (vte_terminal_parent_class)->enter_notify_event (widget, event);
	}
	if (GTK_WIDGET_REALIZED (widget)) {
		VteTerminal *terminal = VTE_TERMINAL (widget);
		/* Hilite any matches. */
		vte_terminal_match_hilite_show(terminal,
					       event->x - VTE_PAD_WIDTH,
					       event->y - VTE_PAD_WIDTH);
	}
	return ret;
}
static gboolean
vte_terminal_leave(GtkWidget *widget, GdkEventCrossing *event)
{
	gboolean ret = FALSE;
	_vte_debug_print(VTE_DEBUG_EVENTS, "Leave.\n");
	if (GTK_WIDGET_CLASS (vte_terminal_parent_class)->leave_notify_event) {
		ret = GTK_WIDGET_CLASS (vte_terminal_parent_class)->leave_notify_event (widget, event);
	}
	if (GTK_WIDGET_REALIZED (widget)) {
		VteTerminal *terminal = VTE_TERMINAL (widget);
		vte_terminal_match_hilite_hide (terminal);
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
vte_terminal_set_visibility (VteTerminal *terminal, GdkVisibilityState state)
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
vte_terminal_visibility_notify(GtkWidget *widget, GdkEventVisibility *event)
{
	VteTerminal *terminal;
	terminal = VTE_TERMINAL(widget);

	_vte_debug_print(VTE_DEBUG_EVENTS, "Visibility (%s -> %s).\n",
			visibility_state_str(terminal->pvt->visibility_state),
			visibility_state_str(event->state));
	vte_terminal_set_visibility(terminal, event->state);

	return FALSE;
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
	if (width != terminal->char_width) {
		resize = cresize = TRUE;
		terminal->char_width = width;
	}
	if (height != terminal->char_height) {
		resize = cresize = TRUE;
		terminal->char_height = height;
	}
	if (ascent != terminal->char_ascent) {
		resize = TRUE;
		terminal->char_ascent = ascent;
	}
	if (descent != terminal->char_descent) {
		resize = TRUE;
		terminal->char_descent = descent;
	}
	terminal->pvt->line_thickness = line_thickness = MAX (MIN ((height - ascent) / 2, height / 14), 1);
	terminal->pvt->underline_position = MIN (ascent + line_thickness, height - line_thickness);
	terminal->pvt->strikethrough_position =  ascent - height / 4;

	/* Queue a resize if anything's changed. */
	if (resize) {
		if (GTK_WIDGET_REALIZED(terminal)) {
			gtk_widget_queue_resize_no_redraw(&terminal->widget);
		}
	}
	/* Emit a signal that the font changed. */
	if (cresize) {
		vte_terminal_emit_char_size_changed(terminal,
						    terminal->char_width,
						    terminal->char_height);
	}
	/* Repaint. */
	_vte_invalidate_all(terminal);
}


static void
vte_terminal_ensure_font (VteTerminal *terminal)
{
	if (terminal->pvt->draw != NULL) {
		/* Load default fonts, if no fonts have been loaded. */
		if (!terminal->pvt->has_fonts) {
			vte_terminal_set_font_full_internal(terminal,
                                                            terminal->pvt->fontdesc,
                                                            terminal->pvt->fontantialias);
		}
		if (terminal->pvt->fontdirty) {
			gint width, height, ascent;
			terminal->pvt->fontdirty = FALSE;
			_vte_draw_set_text_font (terminal->pvt->draw,
					terminal->pvt->fontdesc,
					terminal->pvt->fontantialias);
			_vte_draw_get_text_metrics (terminal->pvt->draw,
						    &width, &height, &ascent);
			vte_terminal_apply_metrics(terminal,
						   width, height, ascent, height - ascent);
		}
	}
}


/**
 * vte_terminal_set_font_full:
 * @terminal: a #VteTerminal
 * @font_desc: The #PangoFontDescription of the desired font.
 * @antialias: Specify if anti aliasing of the fonts is to be used or not.
 *
 * Sets the font used for rendering all text displayed by the terminal,
 * overriding any fonts set using gtk_widget_modify_font().  The terminal
 * will immediately attempt to load the desired font, retrieve its
 * metrics, and attempt to resize itself to keep the same number of rows
 * and columns.
 *
 * Since: 0.11.11
 * 
 * Deprecated: 0.19.1
 */
void
vte_terminal_set_font_full(VteTerminal *terminal,
			   const PangoFontDescription *font_desc,
			   VteTerminalAntiAlias antialias)
{
        vte_terminal_set_font_full_internal(terminal, font_desc, antialias);
}

static void
vte_terminal_set_font_full_internal(VteTerminal *terminal,
                                    const PangoFontDescription *font_desc,
                                    VteTerminalAntiAlias antialias)
{
        VteTerminalPrivate *pvt;
        GObject *object;
	PangoFontDescription *desc;
        gboolean same_desc;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        object = G_OBJECT(terminal);
        pvt = terminal->pvt;

	/* Create an owned font description. */
	gtk_widget_ensure_style (&terminal->widget);
	desc = pango_font_description_copy (terminal->widget.style->font_desc);
	pango_font_description_set_family_static (desc, "monospace");
	if (font_desc != NULL) {
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

        same_desc = pvt->fontdesc && pango_font_description_equal (pvt->fontdesc, desc);
	
	/* Note that we proceed to recreating the font even if the description
	 * and antialias settings are the same.  This is because maybe screen
	 * font options were changed, or new fonts installed.  Those will be
	 * detected at font creation time and respected.
	 */

        g_object_freeze_notify(object);

	/* Free the old font description and save the new one. */
	if (terminal->pvt->fontdesc != NULL) {
		pango_font_description_free(terminal->pvt->fontdesc);
	}
	pvt->fontdesc = desc;
	pvt->fontantialias = antialias;
	pvt->fontdirty = TRUE;
	pvt->has_fonts = TRUE;

        if (!same_desc)
                g_object_notify(object, "font-desc");

	/* Set the drawing font. */
	if (GTK_WIDGET_REALIZED(terminal)) {
		vte_terminal_ensure_font (terminal);
	}

        g_object_thaw_notify(object);
}

/**
 * vte_terminal_set_font:
 * @terminal: a #VteTerminal
 * @font_desc: The #PangoFontDescription of the desired font.
 *
 * Sets the font used for rendering all text displayed by the terminal,
 * overriding any fonts set using gtk_widget_modify_font().  The terminal
 * will immediately attempt to load the desired font, retrieve its
 * metrics, and attempt to resize itself to keep the same number of rows
 * and columns.
 *
 */
void
vte_terminal_set_font(VteTerminal *terminal,
		      const PangoFontDescription *font_desc)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_terminal_set_font_full_internal(terminal, font_desc,
                                            VTE_ANTI_ALIAS_USE_DEFAULT);
}

static void
vte_terminal_set_font_from_string_full_internal(VteTerminal *terminal,
                                                const char *name,
                                                VteTerminalAntiAlias antialias)
{
	PangoFontDescription *font_desc = NULL;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	if (name)
	  font_desc = pango_font_description_from_string(name);
	vte_terminal_set_font_full_internal(terminal, font_desc, antialias);
	pango_font_description_free(font_desc);
}

/**
 * vte_terminal_set_font_from_string_full:
 * @terminal: a #VteTerminal
 * @name: A string describing the font.
 * @antialias: Whether or not to antialias the font (if possible).
 *
 * A convenience function which converts @name into a #PangoFontDescription and
 * passes it to vte_terminal_set_font_full().
 *
 * Since: 0.11.11
 *
 * Deprecated: 0.19.1
 */
void
vte_terminal_set_font_from_string_full(VteTerminal *terminal, const char *name,
				       VteTerminalAntiAlias antialias)
{
        vte_terminal_set_font_from_string_full_internal(terminal, name, antialias);
}

/**
 * vte_terminal_set_font_from_string:
 * @terminal: a #VteTerminal
 * @name: A string describing the font.
 *
 * A convenience function which converts @name into a #PangoFontDescription and
 * passes it to vte_terminal_set_font().
 *
 */
void
vte_terminal_set_font_from_string(VteTerminal *terminal, const char *name)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(name != NULL);
	vte_terminal_set_font_from_string_full_internal(terminal, name,
                                                        VTE_ANTI_ALIAS_USE_DEFAULT);
}

/**
 * vte_terminal_get_font:
 * @terminal: a #VteTerminal
 *
 * Queries the terminal for information about the fonts which will be
 * used to draw text in the terminal.
 *
 * Returns: a #PangoFontDescription describing the font the terminal is
 * currently using to render text.
 */
const PangoFontDescription *
vte_terminal_get_font(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return terminal->pvt->fontdesc;
}

/* Read and refresh our perception of the size of the PTY. */
static void
vte_terminal_refresh_size(VteTerminal *terminal)
{
	int rows, columns;
	if (terminal->pvt->pty_master != -1) {
		/* Use an ioctl to read the size of the terminal. */
		if (_vte_pty_get_size(terminal->pvt->pty_master, &columns, &rows) != 0) {
                        int errsv = errno;

			g_warning(_("Error reading PTY size, using defaults: "
				    "%s."), g_strerror(errsv));
		} else {
			terminal->row_count = rows;
			terminal->column_count = columns;
		}
	}
}

/**
 * vte_terminal_set_size:
 * @terminal: a #VteTerminal
 * @columns: the desired number of columns
 * @rows: the desired number of rows
 *
 * Attempts to change the terminal's size in terms of rows and columns.  If
 * the attempt succeeds, the widget will resize itself to the proper size.
 *
 */
void
vte_terminal_set_size(VteTerminal *terminal, glong columns, glong rows)
{
	glong old_columns, old_rows;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	_vte_debug_print(VTE_DEBUG_MISC,
			"Setting PTY size to %ldx%ld.\n",
			columns, rows);

	old_rows = terminal->row_count;
	old_columns = terminal->column_count;

	if (terminal->pvt->pty_master != -1) {
		/* Try to set the terminal size. */
		if (_vte_pty_set_size(terminal->pvt->pty_master, columns, rows) != 0) {
			g_warning("Error setting PTY size: %s.",
				  g_strerror(errno));
		}
		/* Read the terminal size, in case something went awry. */
		vte_terminal_refresh_size(terminal);
	} else {
		terminal->row_count = rows;
		terminal->column_count = columns;
	}
	if (old_rows != terminal->row_count ||
			old_columns != terminal->column_count) {
		gtk_widget_queue_resize (&terminal->widget);
		/* Our visible text changed. */
		vte_terminal_emit_text_modified(terminal);
	}
}

/* Redraw the widget. */
static void
vte_terminal_handle_scroll(VteTerminal *terminal)
{
	long dy, adj;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	/* Read the new adjustment value and save the difference. */
	adj = floor (terminal->adjustment->value);
	dy = adj - screen->scroll_delta;
	screen->scroll_delta = adj;

	/* Sanity checks. */
	if (!GTK_WIDGET_DRAWABLE(terminal) ||
			terminal->pvt->visibility_state == GDK_VISIBILITY_FULLY_OBSCURED) {
		return;
	}

	if (dy != 0) {
		_vte_debug_print(VTE_DEBUG_ADJ,
			    "Scrolling by %ld\n", dy);
		_vte_terminal_scroll_region(terminal, screen->scroll_delta,
					   terminal->row_count, -dy);
		vte_terminal_emit_text_scrolled(terminal, dy);
		_vte_terminal_queue_contents_changed(terminal);
	} else {
		_vte_debug_print(VTE_DEBUG_ADJ, "Not scrolling\n");
	}
}

/* Set the adjustment objects used by the terminal widget. */
static void
vte_terminal_set_scroll_adjustments(GtkWidget *widget,
				    GtkAdjustment *hadjustment G_GNUC_UNUSED,
				    GtkAdjustment *adjustment)
{
	VteTerminal *terminal = VTE_TERMINAL (widget);

	if (adjustment != NULL && adjustment == terminal->adjustment)
		return;
	if (adjustment == NULL && terminal->adjustment != NULL)
		return;

	if (adjustment == NULL)
		adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0));
	else
		g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));

	/* Add a reference to the new adjustment object. */
	g_object_ref_sink(adjustment);
	/* Get rid of the old adjustment object. */
	if (terminal->adjustment != NULL) {
		/* Disconnect our signal handlers from this object. */
		g_signal_handlers_disconnect_by_func(terminal->adjustment,
						     vte_terminal_handle_scroll,
						     terminal);
		g_object_unref(terminal->adjustment);
	}

	/* Set the new adjustment object. */
	terminal->adjustment = adjustment;

	/* We care about the offset, not the top or bottom. */
	g_signal_connect_swapped(terminal->adjustment,
				 "value-changed",
				 G_CALLBACK(vte_terminal_handle_scroll),
				 terminal);
}

/**
 * vte_terminal_set_emulation:
 * @terminal: a #VteTerminal
 * @emulation: the name of a terminal description
 *
 * Sets what type of terminal the widget attempts to emulate by scanning for
 * control sequences defined in the system's termcap file.  Unless you
 * are interested in this feature, always use "xterm".
 *
 */
void
vte_terminal_set_emulation(VteTerminal *terminal, const char *emulation)
{
        VteTerminalPrivate *pvt;
        GObject *object;
	int columns, rows;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        object = G_OBJECT(terminal);
        pvt = terminal->pvt;

        g_object_freeze_notify(object);

	/* Set the emulation type, for reference. */
	if (emulation == NULL) {
		emulation = vte_terminal_get_default_emulation(terminal);
	}
	terminal->pvt->emulation = g_intern_string(emulation);
	_vte_debug_print(VTE_DEBUG_MISC,
			"Setting emulation to `%s'...\n", emulation);
	/* Find and read the right termcap file. */
	vte_terminal_set_termcap(terminal, NULL, FALSE);

	/* Create a table to hold the control sequences. */
	if (terminal->pvt->matcher != NULL) {
		_vte_matcher_free(terminal->pvt->matcher);
	}
	terminal->pvt->matcher = _vte_matcher_new(emulation, terminal->pvt->termcap);

	if (terminal->pvt->termcap != NULL) {
		/* Read emulation flags. */
		terminal->pvt->flags.am = _vte_termcap_find_boolean(terminal->pvt->termcap,
								    terminal->pvt->emulation,
								    "am");
		terminal->pvt->flags.bw = _vte_termcap_find_boolean(terminal->pvt->termcap,
								    terminal->pvt->emulation,
								    "bw");
		terminal->pvt->flags.LP = _vte_termcap_find_boolean(terminal->pvt->termcap,
								    terminal->pvt->emulation,
								    "LP");
		terminal->pvt->flags.ul = _vte_termcap_find_boolean(terminal->pvt->termcap,
								    terminal->pvt->emulation,
								    "ul");
		terminal->pvt->flags.xn = _vte_termcap_find_boolean(terminal->pvt->termcap,
								    terminal->pvt->emulation,
								    "xn");

		/* Resize to the given default. */
		columns = _vte_termcap_find_numeric(terminal->pvt->termcap,
						    terminal->pvt->emulation,
						    "co");
		if (columns <= 0) {
			columns = VTE_COLUMNS;
		}
		terminal->pvt->default_column_count = columns;

		rows = _vte_termcap_find_numeric(terminal->pvt->termcap,
						 terminal->pvt->emulation,
						 "li");
		if (rows <= 0 ) {
			rows = VTE_ROWS;
		}
		terminal->pvt->default_row_count = rows;
	}

	/* Notify observers that we changed our emulation. */
	vte_terminal_emit_emulation_changed(terminal);

        g_object_thaw_notify(object);
}

/**
 * vte_terminal_get_default_emulation:
 * @terminal: a #VteTerminal
 *
 * Queries the terminal for its default emulation, which is attempted if the
 * terminal type passed to vte_terminal_set_emulation() is %NULL.
 *
 * Returns: the name of the default terminal type the widget attempts to emulate
 *
 * Since: 0.11.11
 */
const char *
vte_terminal_get_default_emulation(VteTerminal *terminal)
{
	return VTE_DEFAULT_EMULATION;
}

/**
 * vte_terminal_get_emulation:
 * @terminal: a #VteTerminal
 *
 * Queries the terminal for its current emulation, as last set by a call to
 * vte_terminal_set_emulation().
 *
 * Returns: the name of the terminal type the widget is attempting to emulate
 */
const char *
vte_terminal_get_emulation(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return terminal->pvt->emulation;
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

/* Set the path to the termcap file we read, and read it in. */
static void
vte_terminal_set_termcap(VteTerminal *terminal, const char *path,
			 gboolean reset)
{
        GObject *object = G_OBJECT(terminal);
	struct stat st;
	char *wpath;

	if (path == NULL) {
		wpath = g_strdup_printf(DATADIR "/" PACKAGE "/termcap/%s",
					terminal->pvt->emulation ?
					terminal->pvt->emulation :
					vte_terminal_get_default_emulation(terminal));
		if (g_stat(wpath, &st) != 0) {
			g_free(wpath);
			wpath = g_strdup("/etc/termcap");
		}
		path = g_intern_string (wpath);
		g_free(wpath);
	} else {
		path = g_intern_string (path);
	}
	if (path == terminal->pvt->termcap_path) {
		return;
	}

        g_object_freeze_notify(object);

	terminal->pvt->termcap_path = path;

	_vte_debug_print(VTE_DEBUG_MISC, "Loading termcap `%s'...",
			terminal->pvt->termcap_path);
	if (terminal->pvt->termcap != NULL) {
		_vte_termcap_free(terminal->pvt->termcap);
	}
	terminal->pvt->termcap = _vte_termcap_new(terminal->pvt->termcap_path);
	_vte_debug_print(VTE_DEBUG_MISC, "\n");
	if (terminal->pvt->termcap == NULL) {
		_vte_terminal_inline_error_message(terminal,
				"Failed to load terminal capabilities from '%s'",
				terminal->pvt->termcap_path);
	}
	if (reset) {
		vte_terminal_set_emulation(terminal, terminal->pvt->emulation);
	}

        g_object_thaw_notify(object);
}

static void
_vte_terminal_codeset_changed_cb(struct _vte_iso2022_state *state, gpointer p)
{
	vte_terminal_set_encoding(p, _vte_iso2022_state_get_codeset(state));
}

/* Initialize the terminal widget after the base widget stuff is initialized.
 * We need to create a new psuedo-terminal pair, read in the termcap file, and
 * set ourselves up to do the interpretation of sequences. */
static void
vte_terminal_init(VteTerminal *terminal)
{
	VteTerminalPrivate *pvt;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_init()\n");

	/* Initialize private data. */
	pvt = terminal->pvt = G_TYPE_INSTANCE_GET_PRIVATE (terminal, VTE_TYPE_TERMINAL, VteTerminalPrivate);

	GTK_WIDGET_SET_FLAGS(terminal, GTK_CAN_FOCUS);
	/* gtk_widget_set_double_buffered (&terminal->widget, FALSE); */

	/* We do our own redrawing. */
	gtk_widget_set_redraw_on_allocate (&terminal->widget, FALSE);

	/* Set an adjustment for the application to use to control scrolling. */
	vte_terminal_set_scroll_adjustments(GTK_WIDGET(terminal), NULL, NULL);

	/* Set up dummy metrics, value != 0 to avoid division by 0 */
	terminal->char_width = 1;
	terminal->char_height = 1;
	terminal->char_ascent = 1;
	terminal->char_descent = 1;
	terminal->pvt->line_thickness = 1;
	terminal->pvt->underline_position = 1;
	terminal->pvt->strikethrough_position = 1;

	/* We allocated zeroed memory, just fill in non-zero stuff. */

	/* Set up I/O encodings. */
	pvt->iso2022 = _vte_iso2022_state_new(pvt->encoding,
					      &_vte_terminal_codeset_changed_cb,
					      terminal);
	pvt->incoming = NULL;
	pvt->pending = g_array_new(FALSE, TRUE, sizeof(gunichar));
	pvt->max_input_bytes = VTE_MAX_INPUT_READ;
	pvt->cursor_blink_tag = 0;
	pvt->outgoing = _vte_buffer_new();
	pvt->outgoing_conv = VTE_INVALID_CONV;
	pvt->conv_buffer = _vte_buffer_new();
	vte_terminal_set_encoding(terminal, NULL);
	g_assert(terminal->pvt->encoding != NULL);

	/* Load the termcap data and set up the emulation. */
	pvt->keypad_mode = VTE_KEYMODE_NORMAL;
	pvt->cursor_mode = VTE_KEYMODE_NORMAL;
	pvt->dec_saved = g_hash_table_new(NULL, NULL);
	pvt->default_column_count = VTE_COLUMNS;
	pvt->default_row_count = VTE_ROWS;

	/* Setting the terminal type and size requires the PTY master to
	 * be set up properly first. */
	pvt->pty_master = -1;
	vte_terminal_set_emulation(terminal, NULL);
	vte_terminal_set_size(terminal,
			      pvt->default_column_count,
			      pvt->default_row_count);
	g_assert (pvt->pty_master == -1);
	pvt->pty_input_source = 0;
	pvt->pty_output_source = 0;
	pvt->pty_pid = -1;
        pvt->child_exit_status = 0;

	/* Initialize the screens and histories. */
	_vte_ring_init (pvt->alternate_screen.row_data, terminal->row_count);
	pvt->alternate_screen.sendrecv_mode = TRUE;
	pvt->alternate_screen.status_line_contents = g_string_new(NULL);
	pvt->screen = &terminal->pvt->alternate_screen;
	_vte_terminal_set_default_attributes(terminal);

	_vte_ring_init (pvt->normal_screen.row_data,  VTE_SCROLLBACK_INIT);
	pvt->normal_screen.sendrecv_mode = TRUE;
	pvt->normal_screen.status_line_contents = g_string_new(NULL);
	pvt->screen = &terminal->pvt->normal_screen;
	_vte_terminal_set_default_attributes(terminal);

	/* Scrolling options. */
	pvt->scroll_on_keystroke = TRUE;
        pvt->scrollback_lines = -1; /* force update in vte_terminal_set_scrollback_lines */
	vte_terminal_set_scrollback_lines(terminal, VTE_SCROLLBACK_INIT);

	/* Selection info. */
	vte_terminal_set_word_chars(terminal, NULL);

	/* Miscellaneous options. */
	vte_terminal_set_backspace_binding(terminal, VTE_ERASE_AUTO);
	vte_terminal_set_delete_binding(terminal, VTE_ERASE_AUTO);
	pvt->meta_sends_escape = TRUE;
	pvt->audible_bell = TRUE;
	pvt->bell_margin = 10;
	pvt->allow_bold = TRUE;
	pvt->nrc_mode = TRUE;
	vte_terminal_set_default_tabstops(terminal);

	/* Cursor shape. */
	pvt->cursor_shape = VTE_CURSOR_SHAPE_BLOCK;

	/* Cursor blinking. */
	pvt->cursor_visible = TRUE;
	pvt->cursor_blink_timeout = 500;
        pvt->cursor_blinks = FALSE;
        pvt->cursor_blink_mode = VTE_CURSOR_BLINK_SYSTEM;

	/* Matching data. */
        pvt->match_regex_mode = VTE_REGEX_UNDECIDED;
	pvt->match_regexes = g_array_new(FALSE, TRUE,
					 sizeof(struct vte_match_regex));
	vte_terminal_match_hilite_clear(terminal);

	/* Rendering data.  Try everything. */
	pvt->draw = _vte_draw_new(&terminal->widget);

	/* The font description. */
	pvt->fontantialias = VTE_ANTI_ALIAS_USE_DEFAULT;
	gtk_widget_ensure_style(&terminal->widget);

	/* Set up background information. */
	pvt->bg_tint_color.red = 0;
	pvt->bg_tint_color.green = 0;
	pvt->bg_tint_color.blue = 0;
	pvt->bg_saturation = 0.4 * VTE_SATURATION_MAX;
	pvt->bg_opacity = 0xffff;
	pvt->selection_block_mode = FALSE;
	pvt->has_fonts = FALSE;
	pvt->root_pixmap_changed_tag = 0;

	/* Not all backends generate GdkVisibilityNotify, so mark the
	 * window as unobscured initially. */
	pvt->visibility_state = GDK_VISIBILITY_UNOBSCURED;

	/* Listen for hierarchy change notifications. */
	g_signal_connect(terminal, "hierarchy-changed",
			 G_CALLBACK(vte_terminal_hierarchy_changed),
			 NULL);

#ifdef VTE_DEBUG
	/* In debuggable mode, we always do this. */
	/* gtk_widget_get_accessible(&terminal->widget); */
#endif
}

/* Tell GTK+ how much space we need. */
static void
vte_terminal_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	VteTerminal *terminal;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_size_request()\n");

	terminal = VTE_TERMINAL(widget);

	vte_terminal_ensure_font (terminal);

	if (terminal->pvt->pty_master != -1) {
		vte_terminal_refresh_size(terminal);
		requisition->width = terminal->char_width *
			terminal->column_count;
		requisition->height = terminal->char_height *
			terminal->row_count;
	} else {
		requisition->width = terminal->char_width *
			terminal->pvt->default_column_count;
		requisition->height = terminal->char_height *
			terminal->pvt->default_row_count;
	}

	requisition->width += VTE_PAD_WIDTH * 2;
	requisition->height += VTE_PAD_WIDTH * 2;

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
			"[Terminal %p] Size request is %dx%d for %ldx%ld cells.\n",
                        terminal,
			requisition->width, requisition->height,
			(terminal->pvt->pty_master != -1) ?
			terminal->column_count :
			terminal->pvt->default_column_count,
			(terminal->pvt->pty_master != -1) ?
			terminal->row_count :
			terminal->pvt->default_row_count);
}

/* Accept a given size from GTK+. */
static void
vte_terminal_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	VteTerminal *terminal;
	glong width, height;
	gboolean repaint, update_scrollback;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE,
			"vte_terminal_size_allocate()\n");

	terminal = VTE_TERMINAL(widget);

	width = (allocation->width - (2 * VTE_PAD_WIDTH)) /
		terminal->char_width;
	height = (allocation->height - (2 * VTE_PAD_WIDTH)) /
		 terminal->char_height;

	_vte_debug_print(VTE_DEBUG_WIDGET_SIZE,
			"[Terminal %p] Sizing window to %dx%d (%ldx%ld).\n",
                        terminal,
			allocation->width, allocation->height,
			width, height);
	repaint = widget->allocation.width != allocation->width ||
		widget->allocation.height != allocation->height;
	update_scrollback = widget->allocation.height != allocation->height;

	/* Set our allocation to match the structure. */
	widget->allocation = *allocation;

	if (width != terminal->column_count
			|| height != terminal->row_count
			|| update_scrollback) {
		VteScreen *screen = terminal->pvt->screen;
		glong visible_rows = MIN (terminal->row_count,
				_vte_ring_length (screen->row_data));
		if (height < visible_rows) {
			glong delta = visible_rows - height;
			screen->insert_delta += delta;
			vte_terminal_queue_adjustment_value_changed (
					terminal,
					screen->scroll_delta + delta);
		}
		/* Set the size of the pseudo-terminal. */
		vte_terminal_set_size(terminal, width, height);

		/* Adjust scrolling area in case our boundaries have just been
		 * redefined to be invalid. */
		if (terminal->pvt->screen->scrolling_restricted) {
			terminal->pvt->screen->scrolling_region.start =
				MIN(terminal->pvt->screen->scrolling_region.start,
						terminal->row_count - 1);
			terminal->pvt->screen->scrolling_region.end =
				MIN(terminal->pvt->screen->scrolling_region.end,
						terminal->row_count - 1);
		}

		/* Ensure scrollback buffers cover the screen. */
		vte_terminal_set_scrollback_lines(terminal,
				terminal->pvt->scrollback_lines);
		/* Ensure the cursor is valid */
		screen->cursor_current.row = CLAMP (screen->cursor_current.row,
				_vte_ring_delta (screen->row_data),
				MAX (_vte_ring_delta (screen->row_data),
					_vte_ring_next (screen->row_data) - 1));
		/* Notify viewers that the contents have changed. */
		_vte_terminal_queue_contents_changed(terminal);
	}

	/* Resize the GDK window. */
	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize(widget->window,
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

/* Queue a background update. */
static void
root_pixmap_changed_cb(VteBg *bg, VteTerminal *terminal)
{
	_vte_debug_print (VTE_DEBUG_EVENTS, "Root pixmap changed.\n");
	if (terminal->pvt->bg_transparent) {
		vte_terminal_queue_background_update(terminal);
	}
}

/* The window is being destroyed. */
static void
vte_terminal_unrealize(GtkWidget *widget)
{
	VteTerminal *terminal;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_unrealize()\n");

	terminal = VTE_TERMINAL(widget);

	/* Disconnect from background-change events. */
	if (terminal->pvt->root_pixmap_changed_tag != 0) {
		VteBg       *bg;
		bg = vte_bg_get_for_screen(gtk_widget_get_screen(widget));
		g_signal_handler_disconnect (bg,
				terminal->pvt->root_pixmap_changed_tag);
		terminal->pvt->root_pixmap_changed_tag = 0;
	}

	/* Deallocate the cursors. */
	terminal->pvt->mouse_cursor_visible = FALSE;
	gdk_cursor_unref(terminal->pvt->mouse_default_cursor);
	terminal->pvt->mouse_default_cursor = NULL;
	gdk_cursor_unref(terminal->pvt->mouse_mousing_cursor);
	terminal->pvt->mouse_mousing_cursor = NULL;
	gdk_cursor_unref(terminal->pvt->mouse_inviso_cursor);
	terminal->pvt->mouse_inviso_cursor = NULL;

	vte_terminal_match_hilite_clear(terminal);

	/* Shut down input methods. */
	if (terminal->pvt->im_context != NULL) {
	        g_signal_handlers_disconnect_by_func (terminal->pvt->im_context,
						      vte_terminal_im_preedit_changed,
						      terminal);
		vte_terminal_im_reset(terminal);
		gtk_im_context_set_client_window(terminal->pvt->im_context,
						 NULL);
		g_object_unref(terminal->pvt->im_context);
		terminal->pvt->im_context = NULL;
	}
	terminal->pvt->im_preedit_active = FALSE;
	if (terminal->pvt->im_preedit != NULL) {
		g_free(terminal->pvt->im_preedit);
		terminal->pvt->im_preedit = NULL;
	}
	if (terminal->pvt->im_preedit_attrs != NULL) {
		pango_attr_list_unref(terminal->pvt->im_preedit_attrs);
		terminal->pvt->im_preedit_attrs = NULL;
	}
	terminal->pvt->im_preedit_cursor = 0;

	/* Clean up our draw structure. */
	if (terminal->pvt->draw != NULL) {
		_vte_draw_free(terminal->pvt->draw);
		terminal->pvt->draw = NULL;
	}
	terminal->pvt->fontdirty = TRUE;

	/* Unmap the widget if it hasn't been already. */
	if (GTK_WIDGET_MAPPED(widget)) {
		gtk_widget_unmap(widget);
	}

	/* Remove the GDK window. */
	if (widget->window != NULL) {
		/* detach style */
		gtk_style_detach(widget->style);

		gdk_window_set_user_data(widget->window, NULL);
		gdk_window_destroy(widget->window);
		widget->window = NULL;
	}

	/* Remove the blink timeout function. */
	if (terminal->pvt->cursor_blink_tag != 0) {
		g_source_remove(terminal->pvt->cursor_blink_tag);
		terminal->pvt->cursor_blink_tag = 0;
	}
	terminal->pvt->cursor_blink_state = FALSE;

	/* Cancel any pending redraws. */
	remove_update_timeout (terminal);

	/* Cancel any pending signals */
	terminal->pvt->contents_changed_pending = FALSE;
	terminal->pvt->cursor_moved_pending = FALSE;
	terminal->pvt->text_modified_flag = FALSE;
	terminal->pvt->text_inserted_flag = FALSE;
	terminal->pvt->text_deleted_flag = FALSE;

	/* Clear modifiers. */
	terminal->pvt->modifiers = 0;

	/* Mark that we no longer have a GDK window. */
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
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

        if (pvt->cursor_blink_mode == VTE_CURSOR_BLINK_SYSTEM)
                vte_terminal_set_cursor_blinks_internal(terminal, blink);
}

static void
vte_terminal_screen_changed (GtkWidget *widget,
                             GdkScreen *previous_screen)
{
        VteTerminal *terminal = VTE_TERMINAL (widget);
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

        if (GTK_WIDGET_CLASS (vte_terminal_parent_class)->screen_changed) {
                GTK_WIDGET_CLASS (vte_terminal_parent_class)->screen_changed (widget, previous_screen);
        }

        if (screen == previous_screen || screen == NULL)
                return;

        settings = gtk_widget_get_settings (widget);
        vte_terminal_sync_settings (settings, NULL, terminal);
        g_signal_connect (settings, "notify::gtk-cursor-blink",
                          G_CALLBACK (vte_terminal_sync_settings), widget);
        g_signal_connect (settings, "notify::gtk-cursor-blink-time",
                          G_CALLBACK (vte_terminal_sync_settings), widget);
        g_signal_connect (settings, "notify::gtk-cursor-blink-timeout",
                          G_CALLBACK (vte_terminal_sync_settings), widget);
}

/* Perform final cleanups for the widget before it's freed. */
static void
vte_terminal_finalize(GObject *object)
{
    	GtkWidget *widget = GTK_WIDGET (object);
    	VteTerminal *terminal = VTE_TERMINAL (object);
	GtkWidget *toplevel;
	GtkClipboard *clipboard;
        GtkSettings *settings;
	struct vte_match_regex *regex;
	guint i;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_finalize()\n");

	/* Free the draw structure. */
	if (terminal->pvt->draw != NULL) {
		_vte_draw_free(terminal->pvt->draw);
	}

	/* The NLS maps. */
	_vte_iso2022_state_free(terminal->pvt->iso2022);

	/* Free background info. */
	g_free(terminal->pvt->bg_file);

	/* Free the font description. */
	if (terminal->pvt->fontdesc != NULL) {
		pango_font_description_free(terminal->pvt->fontdesc);
	}
	terminal->pvt->fontantialias = VTE_ANTI_ALIAS_USE_DEFAULT;

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

	/* Disconnect from toplevel window configure events. */
	toplevel = gtk_widget_get_toplevel(&terminal->widget);
	if ((toplevel != NULL) && (G_OBJECT(toplevel) != object)) {
		g_signal_handlers_disconnect_by_func(toplevel,
						     vte_terminal_configure_toplevel,
						     terminal);
	}

	/* Disconnect from autoscroll requests. */
	vte_terminal_stop_autoscroll(terminal);

	/* Cancel pending adjustment change notifications. */
	terminal->pvt->adjustment_changed_pending = FALSE;

	/* Tabstop information. */
	if (terminal->pvt->tabstops != NULL) {
		g_hash_table_destroy(terminal->pvt->tabstops);
	}

	/* Free any selected text, but if we currently own the selection,
	 * throw the text onto the clipboard without an owner so that it
	 * doesn't just disappear. */
	if (terminal->pvt->selection != NULL) {
		clipboard = vte_terminal_clipboard_get(terminal,
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

	/* Clear the output histories. */
	_vte_ring_fini(terminal->pvt->normal_screen.row_data);
	_vte_ring_fini(terminal->pvt->alternate_screen.row_data);

	/* Clear the status lines. */
	g_string_free(terminal->pvt->normal_screen.status_line_contents,
		      TRUE);
	g_string_free(terminal->pvt->alternate_screen.status_line_contents,
		      TRUE);

	/* Free conversion descriptors. */
	if (terminal->pvt->outgoing_conv != VTE_INVALID_CONV) {
		_vte_conv_close(terminal->pvt->outgoing_conv);
		terminal->pvt->outgoing_conv = VTE_INVALID_CONV;
	}

	/* Stop listening for child-exited signals. */
	if (terminal->pvt->pty_reaper != NULL) {
		g_signal_handlers_disconnect_by_func(terminal->pvt->pty_reaper,
						     vte_terminal_catch_child_exited,
						     terminal);
		g_object_unref(terminal->pvt->pty_reaper);
	}

	/* Stop processing input. */
	vte_terminal_stop_processing (terminal);

	/* Discard any pending data. */
	_vte_incoming_chunks_release (terminal->pvt->incoming);
	_vte_buffer_free(terminal->pvt->outgoing);
	g_array_free(terminal->pvt->pending, TRUE);
	_vte_buffer_free(terminal->pvt->conv_buffer);

	/* Stop the child and stop watching for input from the child. */
	if (terminal->pvt->pty_pid != -1) {
#ifdef HAVE_GETPGID
		pid_t pgrp;
		pgrp = getpgid(terminal->pvt->pty_pid);
		if (pgrp != -1) {
			kill(-pgrp, SIGHUP);
		}
#endif
		kill(terminal->pvt->pty_pid, SIGHUP);
	}
	_vte_terminal_disconnect_pty_read(terminal);
	_vte_terminal_disconnect_pty_write(terminal);
	if (terminal->pvt->pty_channel != NULL) {
		g_io_channel_unref (terminal->pvt->pty_channel);
	}
	if (terminal->pvt->pty_master != -1) {
		_vte_pty_close(terminal->pvt->pty_master);
		close(terminal->pvt->pty_master);
	}

	/* Remove hash tables. */
	if (terminal->pvt->dec_saved != NULL) {
		g_hash_table_destroy(terminal->pvt->dec_saved);
	}

	/* Clean up emulation structures. */
	if (terminal->pvt->matcher != NULL) {
		_vte_matcher_free(terminal->pvt->matcher);
	}
	if (terminal->pvt->termcap != NULL) {
		_vte_termcap_free(terminal->pvt->termcap);
	}

	remove_update_timeout (terminal);

	/* discard title updates */
	g_free(terminal->pvt->window_title_changed);
	g_free(terminal->pvt->icon_title_changed);

	/* Free public-facing data. */
	g_free(terminal->window_title);
	g_free(terminal->icon_title);
	if (terminal->adjustment != NULL) {
		g_object_unref(terminal->adjustment);
	}

        settings = gtk_widget_get_settings (widget);
        g_signal_handlers_disconnect_matched (settings, G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL, NULL,
                                              terminal);

	/* Call the inherited finalize() method. */
	G_OBJECT_CLASS(vte_terminal_parent_class)->finalize(object);
}

/* Handle realizing the widget.  Most of this is copy-paste from GGAD. */
static void
vte_terminal_realize(GtkWidget *widget)
{
	VteTerminal *terminal;
	GdkWindowAttr attributes;
        GdkColor color;
	guint attributes_mask = 0, i;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_realize()\n");

	terminal = VTE_TERMINAL(widget);

	/* Create the draw structure if we don't already have one. */
	if (terminal->pvt->draw == NULL) {
		terminal->pvt->draw = _vte_draw_new(&terminal->widget);
	}

	/* Create the stock cursors. */
	terminal->pvt->mouse_cursor_visible = TRUE;
	terminal->pvt->mouse_default_cursor =
		vte_terminal_cursor_new(terminal, VTE_DEFAULT_CURSOR);
	terminal->pvt->mouse_mousing_cursor =
		vte_terminal_cursor_new(terminal, VTE_MOUSING_CURSOR);

	/* Create a GDK window for the widget. */
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = _vte_draw_get_visual(terminal->pvt->draw);
	attributes.colormap = _vte_draw_get_colormap(terminal->pvt->draw,
						     FALSE);
	attributes.event_mask = gtk_widget_get_events(widget) |
				GDK_EXPOSURE_MASK |
				GDK_VISIBILITY_NOTIFY_MASK |
				GDK_FOCUS_CHANGE_MASK |
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
			  (attributes.colormap ? GDK_WA_COLORMAP : 0) |
			  GDK_WA_CURSOR;
	widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
					&attributes,
					attributes_mask);
	_VTE_DEBUG_IF(VTE_DEBUG_UPDATES) gdk_window_set_debug_updates(TRUE);
	gdk_window_set_user_data(widget->window, widget);

	/* Set the realized flag. */
	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

	/* Set up the desired palette. */
	if (!terminal->pvt->palette_initialized) {
		vte_terminal_set_default_colors(terminal);
	}

	/* Allocate colors. */
	for (i = 0; i < G_N_ELEMENTS(terminal->pvt->palette); i++) {
		color.red = terminal->pvt->palette[i].red;
		color.green = terminal->pvt->palette[i].green;
		color.blue = terminal->pvt->palette[i].blue;
		color.pixel = 0;
		vte_terminal_set_color_internal(terminal, i, &color);
	}

	/* Set up input method support.  FIXME: do we need to handle the
	 * "retrieve-surrounding" and "delete-surrounding" events? */
	if (terminal->pvt->im_context != NULL) {
		vte_terminal_im_reset(terminal);
		g_object_unref(terminal->pvt->im_context);
		terminal->pvt->im_context = NULL;
	}
	terminal->pvt->im_preedit_active = FALSE;
	terminal->pvt->im_context = gtk_im_multicontext_new();
	gtk_im_context_set_client_window(terminal->pvt->im_context,
					 widget->window);
	g_signal_connect(terminal->pvt->im_context, "commit",
			 G_CALLBACK(vte_terminal_im_commit), terminal);
	g_signal_connect(terminal->pvt->im_context, "preedit-start",
			 G_CALLBACK(vte_terminal_im_preedit_start),
			 terminal);
	g_signal_connect(terminal->pvt->im_context, "preedit-changed",
			 G_CALLBACK(vte_terminal_im_preedit_changed),
			 terminal);
	g_signal_connect(terminal->pvt->im_context, "preedit-end",
			 G_CALLBACK(vte_terminal_im_preedit_end),
			 terminal);
	gtk_im_context_set_use_preedit(terminal->pvt->im_context, TRUE);

	/* Clear modifiers. */
	terminal->pvt->modifiers = 0;

	/* Create our invisible cursor. */
#if GTK_CHECK_VERSION (2, 15, 1)
	terminal->pvt->mouse_inviso_cursor = gdk_cursor_new_for_display(gtk_widget_get_display(widget), GDK_BLANK_CURSOR);
#else
    {
	GdkPixmap *bitmap;
	GdkColor black = {0,0,0,0};

	bitmap = gdk_bitmap_create_from_data(widget->window, "\0", 1, 1);
	terminal->pvt->mouse_inviso_cursor = gdk_cursor_new_from_pixmap(bitmap,
									bitmap,
									&black,
									&black,
									0, 0);
	g_object_unref(bitmap);
    }
#endif /* GTK >= 2.15.1 */

	widget->style = gtk_style_attach(widget->style, widget->window);

	vte_terminal_ensure_font (terminal);

	/* Set up the background, *now*. */
	vte_terminal_background_update(terminal);
}

static void
vte_terminal_determine_colors(VteTerminal *terminal,
			      const VteCell *cell,
			      gboolean reverse,
			      gboolean highlight,
			      gboolean cursor,
			      int *fore, int *back)
{
	/* Determine what the foreground and background colors for rendering
	 * text should be.  If highlight is set and we have a highlight color,
	 * use that scheme.  If cursor is set and we have a cursor color, use
	 * that scheme.  If neither is set, and reverse is set, then use
	 * reverse colors, else use the defaults.  This means that many callers
	 * who specify highlight or cursor should also specify reverse. */
	if (cursor && !highlight && terminal->pvt->cursor_color_set) {
		*fore = cell ? cell->attr.back : VTE_DEF_BG;
		*back = VTE_CUR_BG;
	} else
	if (highlight && !cursor && terminal->pvt->highlight_color_set) {
		*fore = cell ? cell->attr.fore : VTE_DEF_FG;
		*back = VTE_DEF_HL;
	} else
	if (reverse ^ ((cell != NULL) && (cell->attr.reverse))) {
		*fore = cell ? cell->attr.back : VTE_DEF_BG;
		*back = cell ? cell->attr.fore : VTE_DEF_FG;
	} else {
		*fore = cell ? cell->attr.fore : VTE_DEF_FG;
		*back = cell ? cell->attr.back : VTE_DEF_BG;
	}

	/* Handle invisible, bold, and standout text by adjusting colors. */
	if (cell) {
		if (cell->attr.invisible) {
			*fore = *back;
		}
		if (cell->attr.bold) {
			if (*fore == VTE_DEF_FG) {
				*fore = VTE_BOLD_FG;
			} else
			if ((*fore != VTE_DEF_BG) && (*fore < VTE_LEGACY_COLOR_SET_SIZE)) {
				*fore += VTE_COLOR_BRIGHT_OFFSET;
			}
		}
		if (cell->attr.half) {
			if (*fore == VTE_DEF_FG) {
				*fore = VTE_DIM_FG;
			} else
			if ((*fore < VTE_LEGACY_COLOR_SET_SIZE)) {
				*fore = corresponding_dim_index[*fore];;
			}
		}
		if (cell->attr.standout) {
			if (*back < VTE_LEGACY_COLOR_SET_SIZE) {
				*back += VTE_COLOR_BRIGHT_OFFSET;
			}
		}
	}
}

/* Check if a unicode character is actually a graphic character we draw
 * ourselves to handle cases where fonts don't have glyphs for them. */
static gboolean
vte_unichar_is_local_graphic(vteunistr c)
{
	if ((c >= 0x2500) && (c <= 0x257f)) {
		return TRUE;
	}
	switch (c) {
	case 0x00a3: /* british pound */
	case 0x00b0: /* degree */
	case 0x00b1: /* plus/minus */
	case 0x00b7: /* bullet */
	case 0x03c0: /* pi */
	case 0x2190: /* left arrow */
	case 0x2191: /* up arrow */
	case 0x2192: /* right arrow */
	case 0x2193: /* down arrow */
	case 0x2260: /* != */
	case 0x2264: /* <= */
	case 0x2265: /* >= */
	case 0x23ba: /* scanline 1/9 */
	case 0x23bb: /* scanline 3/9 */
	case 0x23bc: /* scanline 7/9 */
	case 0x23bd: /* scanline 9/9 */
	case 0x2409: /* HT symbol */
	case 0x240a: /* LF symbol */
	case 0x240b: /* VT symbol */
	case 0x240c: /* FF symbol */
	case 0x240d: /* CR symbol */
	case 0x2424: /* NL symbol */
	case 0x2592: /* checkerboard */
	case 0x25ae: /* solid rectangle */
	case 0x25c6: /* diamond */
		return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}
static gboolean
vte_terminal_unichar_is_local_graphic(VteTerminal *terminal, vteunistr c, gboolean bold)
{
	return vte_unichar_is_local_graphic (c) &&
		!_vte_draw_has_char (terminal->pvt->draw, c, bold);
}

static void
vte_terminal_fill_rectangle_int(VteTerminal *terminal,
				struct vte_palette_entry *entry,
				gint x,
				gint y,
				gint width,
				gint height)
{
	GdkColor color;
	gboolean wasdrawing;

	wasdrawing = terminal->pvt->draw->started;
	if (!wasdrawing) {
		_vte_draw_start(terminal->pvt->draw);
	}
	color.red = entry->red;
	color.green = entry->green;
	color.blue = entry->blue;
	_vte_draw_fill_rectangle(terminal->pvt->draw,
				 x + VTE_PAD_WIDTH, y + VTE_PAD_WIDTH,
				 width, height,
				 &color, VTE_DRAW_OPAQUE);
	if (!wasdrawing) {
		_vte_draw_end(terminal->pvt->draw);
	}
}

static void
vte_terminal_fill_rectangle(VteTerminal *terminal,
			    struct vte_palette_entry *entry,
			    gint x,
			    gint y,
			    gint width,
			    gint height)
{
	vte_terminal_fill_rectangle_int(terminal, entry, x, y, width, height);
}

static void
vte_terminal_draw_line(VteTerminal *terminal,
		       struct vte_palette_entry *entry,
		       gint x,
		       gint y,
		       gint xp,
		       gint yp)
{
	vte_terminal_fill_rectangle(terminal, entry,
				    x, y,
				    MAX(VTE_LINE_WIDTH, xp - x + 1), MAX(VTE_LINE_WIDTH, yp - y + 1));
}

static void
vte_terminal_draw_rectangle(VteTerminal *terminal,
			    struct vte_palette_entry *entry,
			    gint x,
			    gint y,
			    gint width,
			    gint height)
{
	vte_terminal_draw_line(terminal, entry,
			       x, y, x, y + height);
	vte_terminal_draw_line(terminal, entry,
			       x + width, y, x + width, y + height);
	vte_terminal_draw_line(terminal, entry,
			       x, y, x + width, y);
	vte_terminal_draw_line(terminal, entry,
			       x, y + height, x + width, y + height);
}

static void
vte_terminal_draw_point(VteTerminal *terminal,
			struct vte_palette_entry *entry,
			gint x,
			gint y)
{
	vte_terminal_fill_rectangle(terminal, entry, x, y, 1, 1);
}

/* Draw the graphic representation of a line-drawing or special graphics
 * character. */
static gboolean
vte_terminal_draw_graphic(VteTerminal *terminal, vteunistr c,
			  gint fore, gint back, gboolean draw_default_bg,
			  gint x, gint y,
			  gint column_width, gint columns, gint row_height,
			  gboolean bold)
{
	gboolean ret;
	gint xcenter, xright, ycenter, ybottom, i, j, draw;
	struct _vte_draw_text_request request;
	GdkColor color;

	request.c = c;
	request.x = x + VTE_PAD_WIDTH;
	request.y = y + VTE_PAD_WIDTH;
	request.columns = columns;

	color.red = terminal->pvt->palette[fore].red;
	color.green = terminal->pvt->palette[fore].green;
	color.blue = terminal->pvt->palette[fore].blue;
	xright = x + column_width * columns;
	ybottom = y + row_height;
	xcenter = (x + xright) / 2;
	ycenter = (y + ybottom) / 2;

	if ((back != VTE_DEF_BG) || draw_default_bg) {
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[back],
					    x, y,
					    column_width * columns, row_height);
	}

	if (_vte_draw_char(terminal->pvt->draw, &request,
			   &color, VTE_DRAW_OPAQUE, bold)) {
		/* We were able to draw with actual fonts. */
		return TRUE;
	}

	ret = TRUE;

	switch (c) {
	case 124:
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* != */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (x + xcenter) / 2 - 1, ycenter,
				       (xright + xcenter) / 2 + 1, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (x + xcenter) / 2 - 1,
				       (ybottom + ycenter) / 2,
				       (xright + xcenter) / 2 + 1,
				       (ybottom + ycenter) / 2);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xright - 1, y + 1,
				       x + 1, ybottom - 1);
		break;
	case 127:
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* A "delete" symbol I saw somewhere. */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, ycenter,
				       xcenter, y);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, y,
				       xright - 1, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xright - 1, ycenter,
				       xright - 1, ybottom - 1);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xright - 1, ybottom - 1,
				       x, ybottom - 1);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, ybottom - 1,
				       x, ycenter);
		break;
	case 0x00a3:
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* British pound.  An "L" with a hyphen. */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (x + xcenter) / 2,
				       (y + ycenter) / 2,
				       (x + xcenter) / 2,
				       (ycenter + ybottom) / 2);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (x + xcenter) / 2,
				       (ycenter + ybottom) / 2,
				       (xcenter + xright) / 2,
				       (ycenter + ybottom) / 2);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, ycenter,
				       xcenter + 1, ycenter);
		break;
	case 0x00b0: /* f */
		/* litle circle */
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter - 1, ycenter);
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter + 1, ycenter);
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter, ycenter - 1);
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter, ycenter + 1);
		break;
	case 0x00b1: /* g */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* +/- */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter,
				       (y + ycenter) / 2,
				       xcenter,
				       (ycenter + ybottom) / 2);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (x + xcenter) / 2,
				       ycenter,
				       (xcenter + xright) / 2,
				       ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (x + xcenter) / 2,
				       (ycenter + ybottom) / 2,
				       (xcenter + xright) / 2,
				       (ycenter + ybottom) / 2);
		break;
	case 0x00b7:
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* short hyphen? */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter - 1, ycenter,
				       xcenter + 1, ycenter);
		break;
	case 0x3c0: /* pi */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (x + xcenter) / 2 - 1,
				       (y + ycenter) / 2,
				       (xright + xcenter) / 2 + 1,
				       (y + ycenter) / 2);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (x + xcenter) / 2,
				       (y + ycenter) / 2,
				       (x + xcenter) / 2,
				       (ybottom + ycenter) / 2);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (xright + xcenter) / 2,
				       (y + ycenter) / 2,
				       (xright + xcenter) / 2,
				       (ybottom + ycenter) / 2);
		break;
	/* case 0x2190: FIXME */
	/* case 0x2191: FIXME */
	/* case 0x2192: FIXME */
	/* case 0x2193: FIXME */
	/* case 0x2260: FIXME */
	case 0x2264: /* y */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* <= */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xright - 1, y,
				       x, (y + ycenter) / 2);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, (y + ycenter) / 2,
				       xright - 1, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, ycenter,
				       xright - 1, (ycenter + ybottom) / 2);
		break;
	case 0x2265: /* z */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* >= */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, y,
				       xright - 1, (y + ycenter) / 2);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xright - 1, (y + ycenter) / 2,
				       x, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xright - 1, ycenter,
				       x, (ycenter + ybottom) / 2);
		break;
	case 0x23ba: /* o */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x, y,
					    column_width * columns,
					    VTE_LINE_WIDTH);
		break;
	case 0x23bb: /* p */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x, (y + ycenter) / 2,
					    column_width * columns,
					    VTE_LINE_WIDTH);
		break;
	case 0x23bc: /* r */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    (ycenter + ybottom) / 2,
					    column_width * columns,
					    VTE_LINE_WIDTH);
		break;
	case 0x23bd: /* s */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ybottom - 1,
					    column_width * columns,
					    VTE_LINE_WIDTH);
		break;
	case 0x2409:  /* b */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* H */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, y,
				       x, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, y,
				       xcenter, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, (y + ycenter) / 2,
				       xcenter, (y + ycenter) / 2);
		/* T */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, ycenter,
				       xright - 1, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (xcenter + xright) / 2, ycenter,
				       (xcenter + xright) / 2, ybottom - 1);
		break;
	case 0x240a: /* e */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* L */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, y,
				       x, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, ycenter,
				       xcenter, ycenter);
		/* F */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, ycenter,
				       xcenter, ybottom - 1);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, ycenter,
				       xright - 1, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, (ycenter + ybottom) / 2,
				       xright - 1, (ycenter + ybottom) / 2);
		break;
	case 0x240b: /* i */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* V */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, y,
				       (x + xcenter) / 2, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (x + xcenter) / 2, ycenter,
				       xcenter, y);
		/* T */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, ycenter,
				       xright - 1, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       (xcenter + xright) / 2, ycenter,
				       (xcenter + xright) / 2, ybottom - 1);
		break;
	case 0x240c:  /* c */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* F */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, y,
				       x, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, y,
				       xcenter, y);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, (y + ycenter) / 2,
				       xcenter, (y + ycenter) / 2);
		/* F */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, ycenter,
				       xcenter, ybottom - 1);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, ycenter,
				       xright - 1, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, (ycenter + ybottom) / 2,
				       xright - 1, (ycenter + ybottom) / 2);
		break;
	case 0x240d: /* d */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* C */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, y,
				       x, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, y,
				       xcenter, y);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, ycenter,
				       xcenter, ycenter);
		/* R */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, ycenter,
				       xcenter, ybottom - 1);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, ycenter,
				       xright - 1, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xright - 1, ycenter,
				       xright - 1, (ycenter + ybottom) / 2);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xright - 1, (ycenter + ybottom) / 2,
				       xcenter, (ycenter + ybottom) / 2);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, (ycenter + ybottom) / 2,
				       xright - 1, ybottom - 1);
		break;
	case 0x2424: /* h */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* N */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, y,
				       x, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       x, y,
				       xcenter, ycenter);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, y,
				       xcenter, ycenter);
		/* L */
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, ycenter,
				       xcenter, ybottom - 1);
		vte_terminal_draw_line(terminal,
				       &terminal->pvt->palette[fore],
				       xcenter, ybottom - 1,
				       xright - 1, ybottom - 1);
		break;
	case 0x2500: /* q */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    column_width * columns,
					    VTE_LINE_WIDTH);
		break;
	case 0x2501:
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    column_width * columns,
					    VTE_LINE_WIDTH * 2);
		break;
	case 0x2502: /* x */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH,
					    row_height);
		break;
	case 0x2503:
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH * 2,
					    row_height);
		break;
	case 0x250c: /* l */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    xright - xcenter,
					    VTE_LINE_WIDTH);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    VTE_LINE_WIDTH,
					    ybottom - ycenter);
		break;
	case 0x250f:
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    xright - xcenter,
					    VTE_LINE_WIDTH * 2);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    VTE_LINE_WIDTH * 2,
					    ybottom - ycenter);
		break;
	case 0x2510: /* k */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    xcenter - x + VTE_LINE_WIDTH,
					    VTE_LINE_WIDTH);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    VTE_LINE_WIDTH,
					    ybottom - ycenter);
		break;
	case 0x2513:
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    xcenter - x + VTE_LINE_WIDTH * 2,
					    VTE_LINE_WIDTH * 2);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    VTE_LINE_WIDTH * 2,
					    ybottom - ycenter);
		break;
	case 0x2514: /* m */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    xright - xcenter,
					    VTE_LINE_WIDTH);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH,
					    ycenter - y + VTE_LINE_WIDTH);
		break;
	case 0x2517:
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    xright - xcenter,
					    VTE_LINE_WIDTH * 2);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH * 2,
					    ycenter - y + VTE_LINE_WIDTH * 2);
		break;
	case 0x2518: /* j */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    xcenter - x + VTE_LINE_WIDTH,
					    VTE_LINE_WIDTH);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH,
					    ycenter - y + VTE_LINE_WIDTH);
		break;
	case 0x251b:
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    xcenter - x + VTE_LINE_WIDTH * 2,
					    VTE_LINE_WIDTH * 2);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH * 2,
					    ycenter - y + VTE_LINE_WIDTH * 2);
		break;
	case 0x251c: /* t */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH,
					    row_height);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    xright - xcenter,
					    VTE_LINE_WIDTH);
		break;
	case 0x2523:
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH * 2,
					    row_height);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    xright - xcenter,
					    VTE_LINE_WIDTH * 2);
		break;
	case 0x2524: /* u */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH,
					    row_height);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    xcenter - x + VTE_LINE_WIDTH,
					    VTE_LINE_WIDTH);
		break;
	case 0x252b:
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH * 2,
					    row_height);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    xcenter - x + VTE_LINE_WIDTH * 2,
					    VTE_LINE_WIDTH * 2);
		break;
	case 0x252c: /* w */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    VTE_LINE_WIDTH,
					    ybottom - ycenter);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    column_width * columns,
					    VTE_LINE_WIDTH);
		break;
	case 0x2533:
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    ycenter,
					    VTE_LINE_WIDTH * 2,
					    ybottom - ycenter);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    column_width * columns,
					    VTE_LINE_WIDTH * 2);
		break;
	case 0x2534: /* v */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH,
					    ycenter - y + VTE_LINE_WIDTH);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    column_width * columns,
					    VTE_LINE_WIDTH);
		break;
	case 0x253c: /* n */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH,
					    row_height);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    column_width * columns,
					    VTE_LINE_WIDTH);
		break;
	case 0x254b:
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    xcenter,
					    y,
					    VTE_LINE_WIDTH * 2,
					    row_height);
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x,
					    ycenter,
					    column_width * columns,
					    VTE_LINE_WIDTH * 2);
		break;
	case 0x2592:  /* a */
		for (i = x; i <= xright; i++) {
			draw = ((i - x) & 1) == 0;
			for (j = y; j < ybottom; j++) {
				if (draw) {
					vte_terminal_draw_point(terminal,
								&terminal->pvt->palette[fore],
								i, j);
				}
				draw = !draw;
			}
		}
		break;
	case 0x25ae: /* solid rectangle */
		vte_terminal_fill_rectangle(terminal,
					    &terminal->pvt->palette[fore],
					    x, y,
					    xright - x, ybottom - y);
		break;
	case 0x25c6:
		/* diamond */
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter - 2, ycenter);
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter + 2, ycenter);
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter, ycenter - 2);
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter, ycenter + 2);
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter - 1, ycenter - 1);
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter - 1, ycenter + 1);
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter + 1, ycenter - 1);
		vte_terminal_draw_point(terminal,
					&terminal->pvt->palette[fore],
					xcenter + 1, ycenter + 1);
		break;
	default:
		ret = FALSE;
		break;
	}
	return ret;
}

/* Draw a string of characters with similar attributes. */
static void
vte_terminal_draw_cells(VteTerminal *terminal,
			struct _vte_draw_text_request *items, gssize n,
			gint fore, gint back, gboolean clear,
			gboolean draw_default_bg,
			gboolean bold, gboolean underline,
			gboolean strikethrough, gboolean hilite, gboolean boxed,
			gint column_width, gint row_height)
{
	int i, x, y, ascent;
	gint columns = 0;
	GdkColor color = {0,};
	struct vte_palette_entry *fg, *bg, *defbg;

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
	ascent = terminal->char_ascent;

	color.red = bg->red;
	color.blue = bg->blue;
	color.green = bg->green;
	i = 0;
	do {
		columns = 0;
		x = items[i].x;
		y = items[i].y;
		for (; i < n && items[i].y == y; i++) {
			/* Adjust for the border. */
			items[i].x += VTE_PAD_WIDTH;
			items[i].y += VTE_PAD_WIDTH;
			columns += items[i].columns;
		}
		if (clear && (draw_default_bg || bg != defbg)) {
			_vte_draw_fill_rectangle(terminal->pvt->draw,
					x + VTE_PAD_WIDTH, y + VTE_PAD_WIDTH,
					columns * column_width + bold,
					row_height,
					&color, VTE_DRAW_OPAQUE);
		}
	} while (i < n);
	color.red = fg->red;
	color.blue = fg->blue;
	color.green = fg->green;
	_vte_draw_text(terminal->pvt->draw,
			items, n,
			&color, VTE_DRAW_OPAQUE, bold);
	for (i = 0; i < n; i++) {
		/* Deadjust for the border. */
		items[i].x -= VTE_PAD_WIDTH;
		items[i].y -= VTE_PAD_WIDTH;
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
						&terminal->pvt->palette[fore],
						x,
						y + terminal->pvt->underline_position,
						x + (columns * column_width) - 1,
						y + terminal->pvt->underline_position + terminal->pvt->line_thickness - 1);
			}
			if (strikethrough) {
				vte_terminal_draw_line(terminal,
						&terminal->pvt->palette[fore],
						x,
						y + terminal->pvt->strikethrough_position,
						x + (columns * column_width) - 1,
						y + terminal->pvt->strikethrough_position + terminal->pvt->line_thickness - 1);
			}
			if (hilite) {
				vte_terminal_draw_line(terminal,
						&terminal->pvt->palette[fore],
						x,
						y + row_height - 1,
						x + (columns * column_width) - 1,
						y + row_height - 1);
			}
			if (boxed) {
				vte_terminal_draw_rectangle(terminal,
						&terminal->pvt->palette[fore],
						x, y,
						MAX(0, (columns * column_width) - 1),
						MAX(0, row_height - 1));
			}
		}while (i < n);
	}
}

/* Try to map a PangoColor to a palette entry and return its index. */
static guint
_vte_terminal_map_pango_color(VteTerminal *terminal, PangoColor *color)
{
	long distance[G_N_ELEMENTS(terminal->pvt->palette)];
	struct vte_palette_entry *entry;
	guint i, ret;

	/* Calculate a "distance" value.  Could stand to be improved a bit. */
	for (i = 0; i < G_N_ELEMENTS(distance); i++) {
		entry = &terminal->pvt->palette[i];
		distance[i] = 0;
		distance[i] += ((entry->red >> 8) - (color->red >> 8)) *
			       ((entry->red >> 8) - (color->red >> 8));
		distance[i] += ((entry->blue >> 8) - (color->blue >> 8)) *
			       ((entry->blue >> 8) - (color->blue >> 8));
		distance[i] += ((entry->green >> 8) - (color->green >> 8)) *
			       ((entry->green >> 8) - (color->green >> 8));
	}

	/* Find the index of the minimum value. */
	ret = 0;
	for (i = 1; i < G_N_ELEMENTS(distance); i++) {
		if (distance[i] < distance[ret]) {
			ret = i;
		}
	}

	_vte_debug_print(VTE_DEBUG_UPDATES,
			"mapped PangoColor(%04x,%04x,%04x) to "
			"palette entry (%04x,%04x,%04x)\n",
			color->red, color->green, color->blue,
			terminal->pvt->palette[ret].red,
			terminal->pvt->palette[ret].green,
			terminal->pvt->palette[ret].blue);

	return ret;
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
			cells[i].attr.fore = terminal->pvt->screen->color_defaults.attr.fore;
			cells[i].attr.back = terminal->pvt->screen->color_defaults.attr.back;
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
		ival = _vte_terminal_map_pango_color(terminal,
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
_vte_terminal_pango_attribute_destroy(gpointer attr, gpointer data)
{
	pango_attribute_destroy(attr);
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
		cells[i] = terminal->pvt->screen->fill_defaults;
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
					_vte_terminal_apply_pango_attr(terminal,
								       attr,
								       cells,
								       n_cells);
				}
				attr = list->data;
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
	int fore, back;

	/* Note: since this function is only called with the pre-edit text,
	 * all the items contain gunichar only, not vteunistr. */

	for (i = 0, cell_count = 0; i < n; i++) {
		cell_count += g_unichar_to_utf8(items[i].c, scratch_buf);
	}
	cells = g_new(VteCell, cell_count);
	_vte_terminal_translate_pango_cells(terminal, attrs, cells, cell_count);
	for (i = 0, j = 0; i < n; i++) {
		vte_terminal_determine_colors(terminal, &cells[j],
					      FALSE,
					      FALSE,
					      FALSE,
					      &fore, &back);
		vte_terminal_draw_cells(terminal, items + i, 1,
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
vte_terminal_draw_rows(VteTerminal *terminal,
		      VteScreen *screen,
		      gint start_row, gint row_count,
		      gint start_column, gint column_count,
		      gint start_x, gint start_y,
		      gint column_width, gint row_height)
{
	struct _vte_draw_text_request items[4*VTE_DRAW_MAX_LENGTH];
	gint i, j, row, rows, x, y, end_column;
	gint fore, nfore, back, nback;
	glong delta;
	gboolean underline, nunderline, bold, nbold, hilite, nhilite, reverse,
		 selected, nselected, strikethrough, nstrikethrough;
	guint item_count;
	const VteCell *cell;
	const VteRowData *row_data;

	reverse = terminal->pvt->screen->reverse_mode;

	/* adjust for the absolute start of row */
	start_x -= start_column * column_width;
	end_column = start_column + column_count;

	/* clear the background */
	delta = screen->scroll_delta;
	x = start_x + VTE_PAD_WIDTH;
	y = start_y + VTE_PAD_WIDTH;
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
				vte_terminal_determine_colors(terminal, cell,
						reverse|selected,
						selected,
						FALSE,
						&fore, &back);

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
					vte_terminal_determine_colors(terminal, cell,
							reverse|selected,
							selected,
							FALSE,
							&nfore, &nback);
					if (nback != back) {
						break;
					}
					bold = cell && cell->attr.bold;
					j += cell ? cell->attr.columns : 1;
				}
				if (back != VTE_DEF_BG) {
					GdkColor color;
					const struct vte_palette_entry *bg = &terminal->pvt->palette[back];
					color.red = bg->red;
					color.blue = bg->blue;
					color.green = bg->green;
					_vte_draw_fill_rectangle (
							terminal->pvt->draw,
							x + i * column_width,
							y,
							(j - i) * column_width + bold,
							row_height,
							&color, VTE_DRAW_OPAQUE);
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
				vte_terminal_determine_colors(terminal, NULL,
						reverse|selected,
						selected,
						FALSE,
						&fore, &back);
				if (back != VTE_DEF_BG) {
					GdkColor color;
					const struct vte_palette_entry *bg = &terminal->pvt->palette[back];
					color.red = bg->red;
					color.blue = bg->blue;
					color.green = bg->green;
					_vte_draw_fill_rectangle (
							terminal->pvt->draw,
							x + i *column_width,
							y,
							(j - i)  * column_width,
							row_height,
							&color, VTE_DRAW_OPAQUE);
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
			vte_terminal_determine_colors(terminal, cell,
					reverse|selected,
					selected,
					FALSE,
					&fore, &back);
			underline = cell->attr.underline;
			strikethrough = cell->attr.strikethrough;
			bold = cell->attr.bold;
			if (terminal->pvt->show_match) {
				hilite = vte_cell_is_between(i, row,
						terminal->pvt->match_start.column,
						terminal->pvt->match_start.row,
						terminal->pvt->match_end.column,
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
			if (vte_terminal_unichar_is_local_graphic(terminal, cell->c, cell->attr.bold)) {
				if (vte_terminal_draw_graphic(terminal,
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
					selected = vte_cell_is_selected(terminal, j, row, NULL);
					vte_terminal_determine_colors(terminal, cell,
							reverse|selected,
							selected,
							FALSE,
							&nfore, &nback);
					/* Graphic characters must be drawn individually. */
					if (vte_terminal_unichar_is_local_graphic(terminal, cell->c, cell->attr.bold)) {
						if (vte_terminal_draw_graphic(terminal,
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
								terminal->pvt->match_start.column,
								terminal->pvt->match_start.row,
								terminal->pvt->match_end.column,
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
vte_terminal_expand_region (VteTerminal *terminal, GdkRegion *region, const GdkRectangle *area)
{
	VteScreen *screen;
	int width, height;
	int row, col, row_stop, col_stop;
	GdkRectangle rect;

	screen = terminal->pvt->screen;

	width = terminal->char_width;
	height = terminal->char_height;

	/* increase the paint by one pixel on all sides to force the
	 * inclusion of neighbouring cells */
	row = MAX(0, (area->y - VTE_PAD_WIDTH - 1) / height);
	row_stop = MIN(howmany(area->height + area->y - VTE_PAD_WIDTH + 1, height),
		       terminal->row_count);
	if (row_stop <= row) {
		return;
	}
	col = MAX(0, (area->x - VTE_PAD_WIDTH - 1) / width);
	col_stop = MIN(howmany(area->width + area->x - VTE_PAD_WIDTH + 1, width),
		       terminal->column_count);
	if (col_stop <= col) {
		return;
	}

	rect.x = col*width + VTE_PAD_WIDTH;
	rect.width = (col_stop - col) * width;

	rect.y = row*height + VTE_PAD_WIDTH;
	rect.height = (row_stop - row)*height;

	/* the rect must be cell aligned to avoid overlapping XY bands */
	gdk_region_union_with_rect(region, &rect);

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
	int width, height, delta;
	int row, col, row_stop, col_stop;

	screen = terminal->pvt->screen;

	width = terminal->char_width;
	height = terminal->char_height;

	row = MAX(0, (area->y - VTE_PAD_WIDTH) / height);
	row_stop = MIN((area->height + area->y - VTE_PAD_WIDTH) / height,
		       terminal->row_count);
	if (row_stop <= row) {
		return;
	}
	col = MAX(0, (area->x - VTE_PAD_WIDTH) / width);
	col_stop = MIN((area->width + area->x - VTE_PAD_WIDTH) / width,
		       terminal->column_count);
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
			col * width + VTE_PAD_WIDTH,
			row * height + VTE_PAD_WIDTH,
			(col_stop - col) * width,
			(row_stop - row) * height);
	if (!GTK_WIDGET_DOUBLE_BUFFERED (terminal) ||
			_vte_draw_requires_clear (terminal->pvt->draw)) {
		GdkRectangle rect;

		/* expand clear area to cover borders */
		if (col == 0)
			rect.x = 0;
		else
			rect.x = area->x;
		if (col_stop == terminal->column_count)
			rect.width = terminal->widget.allocation.width;
		else
			rect.width = area->x + area->width;
		rect.width -= rect.x;
		if (row == 0)
			rect.y = 0;
		else
			rect.y = area->y;
		if (row_stop == terminal->row_count)
			rect.height = terminal->widget.allocation.height;
		else
			rect.height = area->y + area->height;
		rect.height -= rect.y;

		_vte_draw_clear (terminal->pvt->draw,
				rect.x, rect.y, rect.width, rect.height);
	}

	/* Now we're ready to draw the text.  Iterate over the rows we
	 * need to draw. */
	delta = screen->scroll_delta;
	vte_terminal_draw_rows(terminal,
			      screen,
			      row + delta, row_stop - row,
			      col, col_stop - col,
			      col * width,
			      row * height,
			      width,
			      height);
}

static void
vte_terminal_paint_cursor(VteTerminal *terminal)
{
	VteScreen *screen;
	GdkColor color;
	const VteCell *cell;
	struct _vte_draw_text_request item;
	int row, drow, col;
	long width, height, delta, cursor_width;
	int fore, back, x, y;
	gboolean blink, selected, focus, reverse;

	if (!terminal->pvt->cursor_visible)
		return;

	screen = terminal->pvt->screen;
	delta = screen->scroll_delta;
	col = screen->cursor_current.col;
	drow = screen->cursor_current.row;
	row = drow - delta;
	width = terminal->char_width;
	height = terminal->char_height;

	if ((CLAMP(col, 0, terminal->column_count - 1) != col) ||
	    (CLAMP(row, 0, terminal->row_count    - 1) != row))
		return;

	focus = GTK_WIDGET_HAS_FOCUS(terminal);
	blink = terminal->pvt->cursor_blink_state;
	reverse = terminal->pvt->screen->reverse_mode;

	if (focus && !blink)
		return;

	/* Find the character "under" the cursor. */
	cell = vte_terminal_find_charcell(terminal, col, drow);
	while ((cell != NULL) && (cell->attr.fragment) && (col > 0)) {
		col--;
		cell = vte_terminal_find_charcell(terminal, col, drow);
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

	selected = vte_cell_is_selected(terminal, col, drow, NULL);

	vte_terminal_determine_colors(terminal, cell,
			TRUE^(reverse|selected), selected, TRUE,
			&fore, &back);

	x = item.x + VTE_PAD_WIDTH;
	y = item.y + VTE_PAD_WIDTH;

	switch (terminal->pvt->cursor_shape) {

		case VTE_CURSOR_SHAPE_IBEAM:
		 	
			vte_terminal_draw_line(terminal, &terminal->pvt->palette[back],
					       x - 2*VTE_LINE_WIDTH,
					       y,
					       x - 2*VTE_LINE_WIDTH,
					       y + height - 1);
			break;

		case VTE_CURSOR_SHAPE_UNDERLINE:

			vte_terminal_draw_line(terminal, &terminal->pvt->palette[back],
					       x,
					       y + height - VTE_LINE_WIDTH,
					       x + cursor_width - 1,
					       y + height - VTE_LINE_WIDTH);
			break;

		case VTE_CURSOR_SHAPE_BLOCK:

			color.red = terminal->pvt->palette[back].red;
			color.green = terminal->pvt->palette[back].green;
			color.blue = terminal->pvt->palette[back].blue;

			if (focus) {
				/* just reverse the character under the cursor */

				_vte_draw_fill_rectangle(terminal->pvt->draw,
							 x,
							 y,
							 cursor_width,
							 height,
							 &color,
							 VTE_DRAW_OPAQUE);

				if (!vte_terminal_unichar_is_local_graphic(terminal, item.c, cell ? cell->attr.bold : FALSE) ||
				    !vte_terminal_draw_graphic(terminal,
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
								terminal->pvt->match_start.column,
								terminal->pvt->match_start.row,
								terminal->pvt->match_end.column,
								terminal->pvt->match_end.row,
								TRUE);
					}
					if (cell && cell->c != 0 && cell->c != ' ') {
						vte_terminal_draw_cells(terminal,
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

				_vte_draw_draw_rectangle(terminal->pvt->draw,
							 x - VTE_LINE_WIDTH,
							 y - VTE_LINE_WIDTH,
							 cursor_width + 2*VTE_LINE_WIDTH,
							 height + 2*VTE_LINE_WIDTH,
							 &color,
							 VTE_DRAW_OPAQUE);
			}

			break;
	}
}

static void
vte_terminal_paint_im_preedit_string(VteTerminal *terminal)
{
	VteScreen *screen;
	int row, drow, col, columns;
	long width, height, ascent, descent, delta;
	int i, len, fore, back;

	if (!terminal->pvt->im_preedit)
		return;

	/* Get going. */
	screen = terminal->pvt->screen;

	/* Keep local copies of rendering information. */
	width = terminal->char_width;
	height = terminal->char_height;
	ascent = terminal->char_ascent;
	descent = terminal->char_descent;
	delta = screen->scroll_delta;

	drow = screen->cursor_current.row;
	row = screen->cursor_current.row - delta;

	/* Find out how many columns the pre-edit string takes up. */
	columns = vte_terminal_preedit_width(terminal, FALSE);
	len = vte_terminal_preedit_length(terminal, FALSE);

	/* If the pre-edit string won't fit on the screen if we start
	 * drawing it at the cursor's position, move it left. */
	col = screen->cursor_current.col;
	if (col + columns > terminal->column_count) {
		col = MAX(0, terminal->column_count - columns);
	}

	/* Draw the preedit string, boxed. */
	if (len > 0) {
		struct _vte_draw_text_request *items;
		const char *preedit = terminal->pvt->im_preedit;
		int preedit_cursor;

		items = g_new(struct _vte_draw_text_request, len);
		for (i = columns = 0; i < len; i++) {
			items[i].c = g_utf8_get_char(preedit);
			items[i].columns = _vte_iso2022_unichar_width(terminal->pvt->iso2022,
								      items[i].c);
			items[i].x = (col + columns) * width;
			items[i].y = row * height;
			columns += items[i].columns;
			preedit = g_utf8_next_char(preedit);
		}
		_vte_draw_clear(terminal->pvt->draw,
				col * width + VTE_PAD_WIDTH,
				row * height + VTE_PAD_WIDTH,
				width * columns,
				height);
		fore = screen->defaults.attr.fore;
		back = screen->defaults.attr.back;
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
						TRUE,
						width, height);
		}
		g_free(items);
	}
}

/* Draw the widget. */
static void
vte_terminal_paint(GtkWidget *widget, GdkRegion *region)
{
	VteTerminal *terminal;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_paint()\n");
	_vte_debug_print(VTE_DEBUG_WORK, "=");

	terminal = VTE_TERMINAL(widget);

	/* Designate the start of the drawing operation and clear the area. */
	_vte_draw_start(terminal->pvt->draw);
	if (terminal->pvt->bg_transparent) {
		int x, y;
		gdk_window_get_origin(widget->window, &x, &y);
		_vte_draw_set_background_scroll(terminal->pvt->draw, x, y);
	} else {
		if (terminal->pvt->scroll_background) {
			_vte_draw_set_background_scroll(terminal->pvt->draw,
							0,
							terminal->pvt->screen->scroll_delta *
							terminal->char_height);
		} else {
			_vte_draw_set_background_scroll(terminal->pvt->draw, 0, 0);
		}
	}

	_VTE_DEBUG_IF (VTE_DEBUG_UPDATES) {
		GdkRectangle clip;
		gdk_region_get_clipbox (region, &clip);
		g_printerr ("vte_terminal_paint"
				"	(%d,%d)x(%d,%d) pixels\n",
				clip.x, clip.y, clip.width, clip.height);
	}

	/* Calculate the bounding rectangle. */
	if (!_vte_draw_clip(terminal->pvt->draw, region)) {
		vte_terminal_paint_area (terminal,
				&terminal->widget.allocation);
	} else {
		GdkRectangle *rectangles;
		gint n, n_rectangles;
		gdk_region_get_rectangles (region, &rectangles, &n_rectangles);
		/* don't bother to enlarge an invalidate all */
		if (!(n_rectangles == 1
				&& rectangles[0].width == terminal->widget.allocation.width
				&& rectangles[0].height == terminal->widget.allocation.height)) {
			GdkRegion *rr = gdk_region_new ();
			/* convert pixels into cells */
			for (n = 0; n < n_rectangles; n++) {
				vte_terminal_expand_region (
						terminal, rr, rectangles + n);
			}
			g_free (rectangles);
			gdk_region_get_rectangles (rr, &rectangles, &n_rectangles);
			gdk_region_destroy (rr);
		}
		/* and now paint them */
		for (n = 0; n < n_rectangles; n++) {
			vte_terminal_paint_area (terminal, rectangles + n);
		}
		g_free (rectangles);
	}

	vte_terminal_paint_cursor(terminal);

	vte_terminal_paint_im_preedit_string(terminal);

	/* Done with various structures. */
	_vte_draw_end(terminal->pvt->draw);
}

/* Handle an expose event by painting the exposed area. */
static gint
vte_terminal_expose(GtkWidget *widget, GdkEventExpose *event)
{
	VteTerminal *terminal = VTE_TERMINAL (widget);
	/* Beware the out of order events -
	 *   do not even think about skipping exposes! */
	_vte_debug_print (VTE_DEBUG_WORK, "+");
	_vte_debug_print (VTE_DEBUG_EVENTS, "Expose (%d,%d)x(%d,%d)\n",
			event->area.x, event->area.y,
			event->area.width, event->area.height);
	if (terminal->pvt->active != NULL &&
			update_timeout_tag != 0 &&
			!in_update_timeout) {
		/* fix up a race condition where we schedule a delayed update
		 * after an 'immediate' invalidate all */
		if (terminal->pvt->invalidated_all &&
				terminal->pvt->update_regions == NULL) {
			terminal->pvt->invalidated_all = FALSE;
		}
		/* if we expect to redraw the widget soon,
		 * just add this event to the list */
		if (!terminal->pvt->invalidated_all) {
			if (event->area.width >= widget->allocation.width &&
					event->area.height >= widget->allocation.height) {
				_vte_invalidate_all (terminal);
			} else {
				terminal->pvt->update_regions =
					g_slist_prepend (terminal->pvt->update_regions,
							gdk_region_copy (event->region));
			}
		}
	} else {
		vte_terminal_paint(widget, event->region);
		terminal->pvt->invalidated_all = FALSE;
	}
	return FALSE;
}

/* Handle a scroll event. */
static gboolean
vte_terminal_scroll(GtkWidget *widget, GdkEventScroll *event)
{
	GtkAdjustment *adj;
	VteTerminal *terminal;
	gdouble v;
	glong new_value;
	int button;

	terminal = VTE_TERMINAL(widget);

	vte_terminal_read_modifiers (terminal, (GdkEvent*) event);

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
			vte_terminal_send_mouse_button_internal(terminal,
								button,
								event->x,
								event->y);
		}
		return TRUE;
	}

	adj = terminal->adjustment;
	v = MAX (1., ceil (adj->page_increment / 10.));
	switch (event->direction) {
	case GDK_SCROLL_UP:
		v = -v;
		break;
	case GDK_SCROLL_DOWN:
		break;
	default:
		return FALSE;
	}

	if (terminal->pvt->screen == &terminal->pvt->alternate_screen ||
		terminal->pvt->normal_screen.scrolling_restricted) {
		char *normal;
		gssize normal_length;
		const gchar *special;
		gint i, cnt = v;

		/* In the alternate screen there is no scrolling,
		 * so fake a few cursor keystrokes. */

		_vte_keymap_map (
				cnt > 0 ? GDK_Down : GDK_Up,
				terminal->pvt->modifiers,
				terminal->pvt->sun_fkey_mode,
				terminal->pvt->hp_fkey_mode,
				terminal->pvt->legacy_fkey_mode,
				terminal->pvt->vt220_fkey_mode,
				terminal->pvt->cursor_mode == VTE_KEYMODE_APPLICATION,
				terminal->pvt->keypad_mode == VTE_KEYMODE_APPLICATION,
				terminal->pvt->termcap,
				terminal->pvt->emulation ?
				terminal->pvt->emulation : vte_terminal_get_default_emulation(terminal),
				&normal,
				&normal_length,
				&special);
		if (cnt < 0)
			cnt = -cnt;
		for (i = 0; i < cnt; i++) {
			vte_terminal_feed_child_using_modes (terminal,
					normal, normal_length);
		}
		g_free (normal);
	} else {
		/* Perform a history scroll. */
		v += terminal->pvt->screen->scroll_delta;
		new_value = floor (CLAMP (v, adj->lower,
					MAX (adj->lower, adj->upper - adj->page_size)));
		vte_terminal_queue_adjustment_value_changed (terminal, new_value);
	}

	return TRUE;
}

/* Create a new accessible object associated with ourselves, and return
 * it to the caller. */
static AtkObject *
vte_terminal_get_accessible(GtkWidget *widget)
{
	VteTerminal *terminal;
	static gboolean first_time = TRUE;

	terminal = VTE_TERMINAL(widget);

	if (first_time) {
		AtkObjectFactory *factory;
		AtkRegistry *registry;
		GType derived_type;
		GType derived_atk_type;

		/*
		 * Figure out whether accessibility is enabled by looking at the
		 * type of the accessible object which would be created for
		 * the parent type of VteTerminal.
		 */
		derived_type = g_type_parent (VTE_TYPE_TERMINAL);

		registry = atk_get_default_registry ();
		factory = atk_registry_get_factory (registry,
						    derived_type);

		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE)) {
			atk_registry_set_factory_type (registry,
						       VTE_TYPE_TERMINAL,
						       vte_terminal_accessible_factory_get_type ());
		}
		first_time = FALSE;
	}

	return GTK_WIDGET_CLASS (vte_terminal_parent_class)->get_accessible (widget);
}

static void
vte_terminal_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
        VteTerminal *terminal = VTE_TERMINAL (object);
        VteTerminalPrivate *pvt = terminal->pvt;

	switch (prop_id)
	{
                case PROP_ALLOW_BOLD:
                        g_value_set_boolean (value, vte_terminal_get_allow_bold (terminal));
                        break;
                case PROP_AUDIBLE_BELL:
                        g_value_set_boolean (value, vte_terminal_get_audible_bell (terminal));
                        break;
                case PROP_BACKGROUND_IMAGE_FILE:
                        g_value_set_string (value, pvt->bg_file);
                        break;
                case PROP_BACKGROUND_IMAGE_PIXBUF:
                        g_value_set_object (value, pvt->bg_pixbuf);
                        break;
                case PROP_BACKGROUND_OPACITY:
                        g_value_set_double (value, (double) pvt->bg_opacity / (double) G_MAXUINT16);
                        break;
                case PROP_BACKGROUND_SATURATION:
                        g_value_set_double (value, (double) pvt->bg_saturation / (double) VTE_SATURATION_MAX);
                        break;
                case PROP_BACKGROUND_TINT_COLOR:
                        g_value_set_boxed (value, &pvt->bg_tint_color);
                        break;
                case PROP_BACKGROUND_TRANSPARENT:
                        g_value_set_boolean (value, pvt->bg_transparent);
                        break;
                case PROP_BACKSPACE_BINDING:
                        g_value_set_enum (value, pvt->backspace_binding);
                        break;
                case PROP_CURSOR_BLINK_MODE:
                        g_value_set_enum (value, vte_terminal_get_cursor_blink_mode (terminal));
                        break;
                case PROP_CURSOR_SHAPE:
                        g_value_set_enum (value, vte_terminal_get_cursor_shape (terminal));
                        break;
                case PROP_DELETE_BINDING:
                        g_value_set_enum (value, pvt->delete_binding);
                        break;
                case PROP_EMULATION:
                        g_value_set_string (value, vte_terminal_get_emulation (terminal));
                        break;
                case PROP_ENCODING:
                        g_value_set_string (value, vte_terminal_get_encoding (terminal));
                        break;
                case PROP_FONT_DESC:
                        g_value_set_boxed (value, vte_terminal_get_font (terminal));
                        break;
                case PROP_ICON_TITLE:
                        g_value_set_string (value, vte_terminal_get_icon_title (terminal));
                        break;
                case PROP_MOUSE_POINTER_AUTOHIDE:
                        g_value_set_boolean (value, vte_terminal_get_mouse_autohide (terminal));
                        break;
                case PROP_PTY:
                        g_value_set_int (value, pvt->pty_master);
                        break;
                case PROP_SCROLL_BACKGROUND:
                        g_value_set_boolean (value, pvt->scroll_background);
                        break;
                case PROP_SCROLLBACK_LINES:
                        g_value_set_uint (value, pvt->scrollback_lines);
                        break;
                case PROP_SCROLL_ON_KEYSTROKE:
                        g_value_set_boolean (value, pvt->scroll_on_keystroke);
                        break;
                case PROP_SCROLL_ON_OUTPUT:
                        g_value_set_boolean (value, pvt->scroll_on_output);
                        break;
                case PROP_WINDOW_TITLE:
                        g_value_set_string (value, vte_terminal_get_window_title (terminal));
                        break;
                case PROP_WORD_CHARS:
                        g_value_set_string (value, NULL /* FIXME */);
                        break;
                case PROP_VISIBLE_BELL:
                        g_value_set_boolean (value, vte_terminal_get_visible_bell (terminal));
                        break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			return;
        }
}

static void
vte_terminal_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
        VteTerminal *terminal = VTE_TERMINAL (object);
        VteTerminalPrivate *pvt = terminal->pvt;

	switch (prop_id)
	{
                case PROP_ALLOW_BOLD:
                        vte_terminal_set_allow_bold (terminal, g_value_get_boolean (value));
                        break;
                case PROP_AUDIBLE_BELL:
                        vte_terminal_set_audible_bell (terminal, g_value_get_boolean (value));
                        break;
                case PROP_BACKGROUND_IMAGE_FILE:
                        vte_terminal_set_background_image_file (terminal, g_value_get_string (value));
                        break;
                case PROP_BACKGROUND_IMAGE_PIXBUF:
                        vte_terminal_set_background_image (terminal, g_value_get_object (value));
                        break;
                case PROP_BACKGROUND_OPACITY:
                        vte_terminal_set_opacity (terminal, g_value_get_double (value) * (double) G_MAXUINT16);
                        break;
                case PROP_BACKGROUND_SATURATION:
                        vte_terminal_set_background_saturation (terminal, g_value_get_double (value));
                        break;
                case PROP_BACKGROUND_TINT_COLOR:
                        vte_terminal_set_background_tint_color (terminal, g_value_get_boxed (value));
                        break;
                case PROP_BACKGROUND_TRANSPARENT:
                        vte_terminal_set_background_transparent (terminal, g_value_get_boolean (value));
                        break;
                case PROP_BACKSPACE_BINDING:
                        vte_terminal_set_backspace_binding (terminal, g_value_get_enum (value));
                        break;
                case PROP_CURSOR_BLINK_MODE:
                        vte_terminal_set_cursor_blink_mode (terminal, g_value_get_enum (value));
                        break;
                case PROP_CURSOR_SHAPE:
                        vte_terminal_set_cursor_shape (terminal, g_value_get_enum (value));
                        break;
                case PROP_DELETE_BINDING:
                        vte_terminal_set_delete_binding (terminal, g_value_get_enum (value));
                        break;
                case PROP_EMULATION:
                        vte_terminal_set_emulation (terminal, g_value_get_string (value));
                        break;
                case PROP_ENCODING:
                        vte_terminal_set_encoding (terminal, g_value_get_string (value));
                        break;
                case PROP_FONT_DESC:
                        vte_terminal_set_font_full_internal (terminal, g_value_get_boxed (value), pvt->fontantialias);
                        break;
                case PROP_MOUSE_POINTER_AUTOHIDE:
                        vte_terminal_set_mouse_autohide (terminal, g_value_get_boolean (value));
                        break;
                case PROP_PTY:
                        vte_terminal_set_pty (terminal, g_value_get_int (value));
                        break;
                case PROP_SCROLL_BACKGROUND:
                        vte_terminal_set_scroll_background (terminal, g_value_get_boolean (value));
                        break;
                case PROP_SCROLLBACK_LINES:
                        vte_terminal_set_scrollback_lines (terminal, g_value_get_uint (value));
                        break;
                case PROP_SCROLL_ON_KEYSTROKE:
                        vte_terminal_set_scroll_on_keystroke(terminal, g_value_get_boolean (value));
                        break;
                case PROP_SCROLL_ON_OUTPUT:
                        vte_terminal_set_scroll_on_output (terminal, g_value_get_boolean (value));
                        break;
                case PROP_WORD_CHARS:
                        vte_terminal_set_word_chars (terminal, g_value_get_string (value));
                        break;
                case PROP_VISIBLE_BELL:
                        vte_terminal_set_visible_bell (terminal, g_value_get_boolean (value));
                        break;

                /* Not writable */
                case PROP_ICON_TITLE:
                case PROP_WINDOW_TITLE:
                        g_assert_not_reached ();
                        break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			return;
	}
}

/* Initialize methods. */
static void
vte_terminal_class_init(VteTerminalClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;
	GtkBindingSet  *binding_set;

#ifdef VTE_DEBUG
	{
                _vte_debug_init();
		_vte_debug_print(VTE_DEBUG_LIFECYCLE,
				"vte_terminal_class_init()\n");
		/* print out the legend */
		_vte_debug_print(VTE_DEBUG_WORK,
			"Debugging work flow (top input to bottom output):\n"
					"  .  _vte_terminal_process_incoming\n"
					"  <  start process_timeout\n"
					"  {[ start update_timeout  [ => rate limited\n"
					"  T  start of terminal in update_timeout\n"
					"  (  start _vte_terminal_process_incoming\n"
					"  ?  _vte_invalidate_cells (call)\n"
					"  !  _vte_invalidate_cells (dirty)\n"
					"  *  _vte_invalidate_all\n"
					"  )  end _vte_terminal_process_incoming\n"
					"  -  gdk_window_process_updates\n"
					"  +  vte_terminal_expose\n"
					"  =  vte_terminal_paint\n"
					"  ]} end update_timeout\n"
					"  >  end process_timeout\n");
	}
#endif

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
#ifdef HAVE_DECL_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif

	g_type_class_add_private(klass, sizeof (VteTerminalPrivate));

	gobject_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	/* Override some of the default handlers. */
	gobject_class->finalize = vte_terminal_finalize;
        gobject_class->get_property = vte_terminal_get_property;
        gobject_class->set_property = vte_terminal_set_property;
	widget_class->realize = vte_terminal_realize;
	widget_class->scroll_event = vte_terminal_scroll;
	widget_class->expose_event = vte_terminal_expose;
	widget_class->key_press_event = vte_terminal_key_press;
	widget_class->key_release_event = vte_terminal_key_release;
	widget_class->button_press_event = vte_terminal_button_press;
	widget_class->button_release_event = vte_terminal_button_release;
	widget_class->motion_notify_event = vte_terminal_motion_notify;
	widget_class->enter_notify_event = vte_terminal_enter;
	widget_class->leave_notify_event = vte_terminal_leave;
	widget_class->focus_in_event = vte_terminal_focus_in;
	widget_class->focus_out_event = vte_terminal_focus_out;
	widget_class->visibility_notify_event = vte_terminal_visibility_notify;
	widget_class->unrealize = vte_terminal_unrealize;
	widget_class->style_set = NULL;
	widget_class->style_set = vte_terminal_style_set;
	widget_class->size_request = vte_terminal_size_request;
	widget_class->size_allocate = vte_terminal_size_allocate;
	widget_class->get_accessible = vte_terminal_get_accessible;
        widget_class->screen_changed = vte_terminal_screen_changed;

	/* Initialize default handlers. */
	klass->eof = NULL;
	klass->child_exited = NULL;
	klass->emulation_changed = NULL;
	klass->encoding_changed = NULL;
	klass->char_size_changed = NULL;
	klass->window_title_changed = NULL;
	klass->icon_title_changed = NULL;
	klass->selection_changed = NULL;
	klass->contents_changed = NULL;
	klass->cursor_moved = NULL;
	klass->status_line_changed = NULL;
	klass->commit = NULL;

	klass->deiconify_window = NULL;
	klass->iconify_window = NULL;
	klass->raise_window = NULL;
	klass->lower_window = NULL;
	klass->refresh_window = NULL;
	klass->restore_window = NULL;
	klass->maximize_window = NULL;
	klass->resize_window = NULL;
	klass->move_window = NULL;

	klass->increase_font_size = NULL;
	klass->decrease_font_size = NULL;

	klass->text_modified = NULL;
	klass->text_inserted = NULL;
	klass->text_deleted = NULL;
	klass->text_scrolled = NULL;

	klass->copy_clipboard = vte_terminal_real_copy_clipboard;
	klass->paste_clipboard = vte_terminal_real_paste_clipboard;

	klass->set_scroll_adjustments = vte_terminal_set_scroll_adjustments;

        klass->beep = NULL;

        /**
         * VteTerminal::set-scroll-adjustments:
         * @vteterminal: the object which received the signal.
         * @horizontal: the horizontal #GtkAdjustment (unused in #VteTerminal).
         * @vertical: the vertical #GtkAdjustment.
         *
         * Set the scroll adjustments for the terminal. Usually scrolled containers
         * like #GtkScrolledWindow will emit this signal to connect two instances
         * of #GtkScrollbar to the scroll directions of the #VteTerminal.
         *
         * Since: 0.17.1
         */
	widget_class->set_scroll_adjustments_signal =
		g_signal_new(I_("set-scroll-adjustments"),
			     G_TYPE_FROM_CLASS (klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET (VteTerminalClass, set_scroll_adjustments),
			     NULL, NULL,
			     _vte_marshal_VOID__OBJECT_OBJECT,
			     G_TYPE_NONE, 2,
			     GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

	/* Register some signals of our own. */

        /**
         * VteTerminal::eof:
         * @vteterminal: the object which received the signal.
         *
         * Emitted when the terminal receives an end-of-file from a child which
         * is running in the terminal.  This signal is frequently (but not
         * always) emitted with a "child-exited" signal.
         */
	klass->eof_signal =
                g_signal_new(I_("eof"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, eof),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::child-exited:
         * @vteterminal: the object which received the signal.
         *
         * This signal is emitted when the terminal detects that a child started
         * using vte_terminal_fork_command() has exited.
         */
	klass->child_exited_signal =
                g_signal_new(I_("child-exited"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, child_exited),
			     NULL,
			     NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::window-title-changed:
         * @vteterminal: the object which received the signal.
         *
         * Emitted when the terminal's %window_title field is modified.
         */
	klass->window_title_changed_signal =
                g_signal_new(I_("window-title-changed"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, window_title_changed),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::icon-title-changed:
         * @vteterminal: the object which received the signal.
         *
         * Emitted when the terminal's %icon_title field is modified.
         */
	klass->icon_title_changed_signal =
                g_signal_new(I_("icon-title-changed"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, icon_title_changed),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::encoding-changed:
         * @vteterminal: the object which received the signal.
         *
         * Emitted whenever the terminal's current encoding has changed, either
         * as a result of receiving a control sequence which toggled between the
         * local and UTF-8 encodings, or at the parent application's request.
         */
	klass->encoding_changed_signal =
                g_signal_new(I_("encoding-changed"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, encoding_changed),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::commit:
         * @vteterminal: the object which received the signal.
         * @text: a string of text.
         * @size: the length of that string of text.
         *
         * Emitted whenever the terminal receives input from the user and
         * prepares to send it to the child process.  The signal is emitted even
         * when there is no child process.
         */
	klass->commit_signal =
                g_signal_new(I_("commit"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, commit),
			     NULL,
			     NULL,
			     _vte_marshal_VOID__STRING_UINT,
			     G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_UINT);

        /**
         * VteTerminal::emulation-changed:
         * @vteterminal: the object which received the signal.
         *
         * Emitted whenever the terminal's emulation changes, only possible at
         * the parent application's request.
         */
	klass->emulation_changed_signal =
                g_signal_new(I_("emulation-changed"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, emulation_changed),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::char-size-changed:
         * @vteterminal: the object which received the signal.
         * @width: the new character cell width.
         * @height: the new character cell height.
         *
         * Emitted whenever selection of a new font causes the values of the
         * %char_width or %char_height fields to change.
         */
	klass->char_size_changed_signal =
                g_signal_new(I_("char-size-changed"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, char_size_changed),
			     NULL,
			     NULL,
			     _vte_marshal_VOID__UINT_UINT,
			     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

        /**
         * VteTerminal::selection-changed:
         * @vteterminal: the object which received the signal.
         *
         * Emitted whenever the contents of terminal's selection changes.
         */
	klass->selection_changed_signal =
                g_signal_new (I_("selection-changed"),
			      G_OBJECT_CLASS_TYPE(klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET(VteTerminalClass, selection_changed),
			      NULL,
			      NULL,
                              g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

        /**
         * VteTerminal::contents-changed:
         * @vteterminal: the object which received the signal.
         *
         * Emitted whenever the visible appearance of the terminal has changed.
         * Used primarily by #VteTerminalAccessible.
         */
	klass->contents_changed_signal =
                g_signal_new(I_("contents-changed"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, contents_changed),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::cursor-moved:
         * @vteterminal: the object which received the signal.
         *
         * Emitted whenever the cursor moves to a new character cell.  Used
         * primarily by #VteTerminalAccessible.
         */
	klass->cursor_moved_signal =
                g_signal_new(I_("cursor-moved"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, cursor_moved),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::deiconify-window:
         * @vteterminal: the object which received the signal.
         *
         * Emitted at the child application's request.
         */
	klass->deiconify_window_signal =
                g_signal_new(I_("deiconify-window"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, deiconify_window),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::iconify-window:
         * @vteterminal: the object which received the signal.
         *
         * Emitted at the child application's request.
         */
	klass->iconify_window_signal =
                g_signal_new(I_("iconify-window"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, iconify_window),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::raise-window:
         * @vteterminal: the object which received the signal.
         *
         * Emitted at the child application's request.
         */
	klass->raise_window_signal =
                g_signal_new(I_("raise-window"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, raise_window),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::lower-window:
         * @vteterminal: the object which received the signal.
         *
         * Emitted at the child application's request.
         */
	klass->lower_window_signal =
                g_signal_new(I_("lower-window"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, lower_window),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::refresh-window:
         * @vteterminal: the object which received the signal.
         *
         * Emitted at the child application's request.
         */
	klass->refresh_window_signal =
                g_signal_new(I_("refresh-window"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, refresh_window),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::restore-window:
         * @vteterminal: the object which received the signal.
         *
         * Emitted at the child application's request.
         */
	klass->restore_window_signal =
                g_signal_new(I_("restore-window"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, restore_window),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::maximize-window:
         * @vteterminal: the object which received the signal.
         *
         * Emitted at the child application's request.
         */
	klass->maximize_window_signal =
                g_signal_new(I_("maximize-window"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, maximize_window),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::resize-window:
         * @vteterminal: the object which received the signal.
         * @width: the desired width in pixels, including padding.
         * @height: the desired height in pixels, including padding.
         *
         * Emitted at the child application's request.
         */
	klass->resize_window_signal =
                g_signal_new(I_("resize-window"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, resize_window),
			     NULL,
			     NULL,
			     _vte_marshal_VOID__UINT_UINT,
			     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

        /**
         * VteTerminal::resize-window:
         * @vteterminal: the object which received the signal.
         * @x: the terminal's desired location, X coordinate.
         * @y: the terminal's desired location, Y coordinate.
         *
         * Emitted at the child application's request.
         */
	klass->move_window_signal =
                g_signal_new(I_("move-window"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, move_window),
			     NULL,
			     NULL,
			     _vte_marshal_VOID__UINT_UINT,
			     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

        /**
         * VteTerminal::status-line-changed:
         * @vteterminal: the object which received the signal.
         *
         * Emitted whenever the contents of the status line are modified or
         * cleared.
         */
	klass->status_line_changed_signal =
                g_signal_new(I_("status-line-changed"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, status_line_changed),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::increase-font-size:
         * @vteterminal: the object which received the signal.
         *
         * Emitted when the user hits the '+' key while holding the Control key.
         */
	klass->increase_font_size_signal =
                g_signal_new(I_("increase-font-size"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, increase_font_size),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::decrease-font-size:
         * @vteterminal: the object which received the signal.
         *
         * Emitted when the user hits the '-' key while holding the Control key.
         */
	klass->decrease_font_size_signal =
                g_signal_new(I_("decrease-font-size"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, decrease_font_size),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-modified:
         * @vteterminal: the object which received the signal.
         *
         * An internal signal used for communication between the terminal and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
	klass->text_modified_signal =
                g_signal_new(I_("text-modified"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, text_modified),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-inserted:
         * @vteterminal: the object which received the signal.
         *
         * An internal signal used for communication between the terminal and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
	klass->text_inserted_signal =
                g_signal_new(I_("text-inserted"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, text_inserted),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-deleted:
         * @vteterminal: the object which received the signal.
         *
         * An internal signal used for communication between the terminal and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
	klass->text_deleted_signal =
                g_signal_new(I_("text-deleted"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, text_deleted),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-scrolled:
         * @vteterminal: the object which received the signal.
         * @delta: the number of lines scrolled.
         *
         * An internal signal used for communication between the terminal and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
	klass->text_scrolled_signal =
                g_signal_new(I_("text-scrolled"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, text_scrolled),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__INT,
			     G_TYPE_NONE, 1, G_TYPE_INT);

        /**
         * VteTerminal::copy-clipboard:
         * @vteterminal: the object which received the signal.
         *
         * Emitted whenever vte_terminal_copy_clipboard() is called.
         */
	signals[COPY_CLIPBOARD] =
                g_signal_new(I_("copy-clipboard"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_STRUCT_OFFSET(VteTerminalClass, copy_clipboard),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::paste-clipboard:
         * @vteterminal: the object which received the signal.
         *
         * Emitted whenever vte_terminal_paste_clipboard() is called.
         */
	signals[PASTE_CLIPBOARD] =
                g_signal_new(I_("paste-clipboard"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			     G_STRUCT_OFFSET(VteTerminalClass, paste_clipboard),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::beep:
         * @vteterminal: the object which received the signal.
         *
         * This signal is emitted when the a child sends a beep request to the
         * terminal.
         */
        g_signal_new(I_("beep"),
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VteTerminalClass, beep),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal:allow-bold:
         *
         * Controls whether or not the terminal will attempt to draw bold text.
         * This may happen either by using a bold font variant, or by
         * repainting text with a different offset.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_ALLOW_BOLD,
                 g_param_spec_boolean ("allow-bold", NULL, NULL,
                                       TRUE,
                                       G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:audible-bell:
         *
         * Controls whether or not the terminal will beep when the child outputs the
         * "bl" sequence.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_AUDIBLE_BELL,
                 g_param_spec_boolean ("audible-bell", NULL, NULL,
                                       TRUE,
                                       G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:background-image-file:
         *
         * Sets a background image file for the widget.  If specified by
         * #VteTerminal:background-saturation:, the terminal will tint its
         * in-memory copy of the image before applying it to the terminal.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_BACKGROUND_IMAGE_FILE,
                 g_param_spec_string ("background-image-file", NULL, NULL,
                                      NULL,
                                      G_PARAM_READWRITE | STATIC_PARAMS));

        /**
         * VteTerminal:background-image-pixbuf:
         *
         * Sets a background image for the widget.  Text which would otherwise be
         * drawn using the default background color will instead be drawn over the
         * specified image.  If necessary, the image will be tiled to cover the
         * widget's entire visible area. If specified by
         * #VteTerminal:background-saturation:, the terminal will tint its
         * in-memory copy of the image before applying it to the terminal.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_BACKGROUND_IMAGE_PIXBUF,
                 g_param_spec_object ("background-image-pixbuf", NULL, NULL,
                                      GDK_TYPE_PIXBUF,
                                      G_PARAM_READWRITE | STATIC_PARAMS));

        /**
         * VteTerminal:background-opacity:
         *
         * Sets the opacity of the terminal background, were 0.0 means completely
         * transparent and 1.0 means completely opaque.
         *
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_BACKGROUND_OPACITY,
                 g_param_spec_double ("background-opacity", NULL, NULL,
                                      0.0, 1.0,
                                      1.0,
                                      G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:background-saturation:
         *
         * If a background image has been set using #VteTerminal:background-image-file: or
         * #VteTermina:background-image-pixbuf:, or #VteTerminal:background-transparent:,
         * and the saturation value is less
         * than 1.0, the terminal will adjust the colors of the image before drawing
         * the image.  To do so, the terminal will create a copy of the background
         * image (or snapshot of the root window) and modify its pixel values.
         *
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_BACKGROUND_SATURATION,
                 g_param_spec_double ("background-saturation", NULL, NULL,
                                      0.0, 1.0,
                                      0.4,
                                      G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:background-tint-color:
         *
         * If a background image has been set using #VteTerminal:background-image-file: or
         * #VteTermina:background-image-pixbuf:, or #VteTerminal:background-transparent:, and
         * and the value set by VteTerminal:background-saturation: is less than 1.0,
         * the terminal
         * will adjust the color of the image before drawing the image.  To do so,
         * the terminal will create a copy of the background image (or snapshot of
         * the root window) and modify its pixel values.  The initial tint color
         * is black.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_BACKGROUND_TINT_COLOR,
                 g_param_spec_boxed ("background-tint-color", NULL, NULL,
                                     GDK_TYPE_COLOR,
                                     G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:background-transparent:
         *
         * Sets whther the terminal uses the pixmap stored in the root
         * window as the background, adjusted so that if there are no windows
         * below your application, the widget will appear to be transparent.
         *
         * Note: When using a compositing window manager, you should instead
         * set a RGBA colourmap on the toplevel window, so you get real transparency.
         *
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_BACKGROUND_TRANSPARENT,
                 g_param_spec_boolean ("background-transparent", NULL, NULL,
                                       FALSE,
                                       G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:backspace-binding:
         *
         * *Controls what string or control sequence the terminal sends to its child
         * when the user presses the backspace key.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_BACKSPACE_BINDING,
                 g_param_spec_enum ("backspace-binding", NULL, NULL,
                                    VTE_TYPE_TERMINAL_ERASE_BINDING,
                                    VTE_ERASE_AUTO,
                                    G_PARAM_READWRITE | STATIC_PARAMS));

        /**
         * VteTerminal:cursor-blink-mode:
         *
         * Sets whether or not the cursor will blink. Using %VTE_CURSOR_BLINK_SYSTEM
         * will use the #GtkSettings::gtk-cursor-blink setting.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_CURSOR_BLINK_MODE,
                 g_param_spec_enum ("cursor-blink-mode", NULL, NULL,
                                    VTE_TYPE_TERMINAL_CURSOR_BLINK_MODE,
                                    VTE_CURSOR_BLINK_SYSTEM,
                                    G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:cursor-shape:
         *
         * Controls the shape of the cursor.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_CURSOR_SHAPE,
                 g_param_spec_enum ("cursor-shape", NULL, NULL,
                                    VTE_TYPE_TERMINAL_CURSOR_SHAPE,
                                    VTE_CURSOR_SHAPE_BLOCK,
                                    G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:delete-binding:
         *
         * Controls what string or control sequence the terminal sends to its child
         * when the user presses the delete key.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_DELETE_BINDING,
                 g_param_spec_enum ("delete-binding", NULL, NULL,
                                    VTE_TYPE_TERMINAL_ERASE_BINDING,
                                    VTE_ERASE_AUTO,
                                    G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:emulation:
         *
         * Sets what type of terminal the widget attempts to emulate by scanning for
         * control sequences defined in the system's termcap file.  Unless you
         * are interested in this feature, always use the default which is "xterm".
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_EMULATION,
                 g_param_spec_string ("emulation", NULL, NULL,
                                      VTE_DEFAULT_EMULATION,
                                      G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:encoding:
         *
         * Controls the encoding the terminal will expect data from the child to
         * be encoded with.  For certain terminal types, applications executing in the
         * terminal can change the encoding.  The default is defined by the
         * application's locale settings.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_ENCODING,
                 g_param_spec_string ("encoding", NULL, NULL,
                                      NULL,
                                      G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:font-desc:
         *
         * Specifies the font used for rendering all text displayed by the terminal,
         * overriding any fonts set using gtk_widget_modify_font().  The terminal
         * will immediately attempt to load the desired font, retrieve its
         * metrics, and attempt to resize itself to keep the same number of rows
         * and columns.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_FONT_DESC,
                 g_param_spec_boxed ("font-desc", NULL, NULL,
                                     PANGO_TYPE_FONT_DESCRIPTION,
                                     G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:icon-title:
         *
         * The terminal's so-called icon title, or %NULL if no icon title has been set.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_ICON_TITLE,
                 g_param_spec_string ("icon-title", NULL, NULL,
                                      NULL,
                                      G_PARAM_READABLE | STATIC_PARAMS));
     
        /**
         * VteTerminal:pointer-autohide:
         *
         * Controls the value of the terminal's mouse autohide setting.  When autohiding
         * is enabled, the mouse cursor will be hidden when the user presses a key and
         * shown when the user moves the mouse.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_MOUSE_POINTER_AUTOHIDE,
                 g_param_spec_boolean ("pointer-autohide", NULL, NULL,
                                       FALSE,
                                       G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:pty:
         *
         * The file descriptor of the master end of the terminal's PTY.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_PTY,
                 g_param_spec_int ("pty", NULL, NULL,
                                   -1, G_MAXINT,
                                   -1,
                                   G_PARAM_READWRITE | STATIC_PARAMS));

        /**
         * VteTerminal:scroll-background:
         *
         * Controls whether or not the terminal will scroll the background image (if
         * one is set) when the text in the window must be scrolled.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_SCROLL_BACKGROUND,
                 g_param_spec_boolean ("scroll-background", NULL, NULL,
                                       FALSE,
                                       G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:scrollback-lines:
         *
         * The length of the scrollback buffer used by the terminal.  The size of
         * the scrollback buffer will be set to the larger of this value and the number
         * of visible rows the widget can display, so 0 can safely be used to disable
         * scrollback.  Note that this setting only affects the normal screen buffer.
         * For terminal types which have an alternate screen buffer, no scrollback is
         * allowed on the alternate screen buffer.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_SCROLLBACK_LINES,
                 g_param_spec_uint ("scrollback-lines", NULL, NULL,
                                    0, G_MAXUINT,
                                    VTE_SCROLLBACK_INIT,
                                    G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:scroll-on-keystroke:
         *
         * Controls whether or not the terminal will forcibly scroll to the bottom of
         * the viewable history when the user presses a key.  Modifier keys do not
         * trigger this behavior.
         *
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_SCROLL_ON_KEYSTROKE,
                 g_param_spec_boolean ("scroll-on-keystroke", NULL, NULL,
                                       FALSE,
                                       G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:scroll-on-output:
         *
         * Controls whether or not the terminal will forcibly scroll to the bottom of
         * the viewable history when the new data is received from the child.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_SCROLL_ON_OUTPUT,
                 g_param_spec_boolean ("scroll-on-output", NULL, NULL,
                                       TRUE,
                                       G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:window-title:
         *
         * The terminal's title.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_WINDOW_TITLE,
                 g_param_spec_string ("window-title", NULL, NULL,
                                      NULL,
                                      G_PARAM_READABLE | STATIC_PARAMS));
     
        /**
         * VteTerminal:word-chars:
         *
         * When the user double-clicks to start selection, the terminal will extend
         * the selection on word boundaries.  It will treat characters the word-chars
         * characters as parts of words, and all other characters as word separators.
         * Ranges of characters can be specified by separating them with a hyphen.
         *
         * As a special case, when setting this to %NULL or the empty string, the terminal will
         * treat all graphic non-punctuation non-space characters as word characters.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_WORD_CHARS,
                 g_param_spec_string ("word-chars", NULL, NULL,
                                      NULL,
                                      G_PARAM_READWRITE | STATIC_PARAMS));
     
        /**
         * VteTerminal:visible-bell:
         *
         * Controls whether the terminal will present a visible bell to the
         * user when the child outputs the "bl" sequence.  The terminal
         * will clear itself to the default foreground color and then repaint itself.
         * 
         * Since: 0.19.1
         */
        g_object_class_install_property
                (gobject_class,
                 PROP_VISIBLE_BELL,
                 g_param_spec_boolean ("visible-bell", NULL, NULL,
                                       FALSE,
                                       G_PARAM_READWRITE | STATIC_PARAMS));
     
        /* Keybindings */
	binding_set = gtk_binding_set_by_class(klass);

	/* Bind Copy, Paste, Cut keys */
	gtk_binding_entry_add_signal(binding_set, GDK_F16, 0, "copy-clipboard",0);
	gtk_binding_entry_add_signal(binding_set, GDK_F18, 0, "paste-clipboard", 0);
	gtk_binding_entry_add_signal(binding_set, GDK_F20, 0, "copy-clipboard",0);

	process_timer = g_timer_new ();
}

/**
 * vte_terminal_set_audible_bell:
 * @terminal: a #VteTerminal
 * @is_audible: %TRUE if the terminal should beep
 *
 * Controls whether or not the terminal will beep when the child outputs the
 * "bl" sequence.
 *
 */
void
vte_terminal_set_audible_bell(VteTerminal *terminal, gboolean is_audible)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        is_audible = is_audible != FALSE;
        if (is_audible == pvt->audible_bell)
                return;

	pvt->audible_bell = is_audible;

        g_object_notify (G_OBJECT (terminal), "audible-bell");
}

/**
 * vte_terminal_get_audible_bell:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not the terminal will beep when the child outputs the
 * "bl" sequence.
 *
 * Returns: %TRUE if audible bell is enabled, %FALSE if not
 */
gboolean
vte_terminal_get_audible_bell(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->audible_bell;
}

/**
 * vte_terminal_set_visible_bell:
 * @terminal: a #VteTerminal
 * @is_visible: %TRUE if the terminal should flash
 *
 * Controls whether or not the terminal will present a visible bell to the
 * user when the child outputs the "bl" sequence.  The terminal
 * will clear itself to the default foreground color and then repaint itself.
 *
 */
void
vte_terminal_set_visible_bell(VteTerminal *terminal, gboolean is_visible)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        is_visible = is_visible != FALSE;
        if (is_visible == pvt->visible_bell)
                return;

	pvt->visible_bell = is_visible;

        g_object_notify (G_OBJECT (terminal), "visible-bell");
}

/**
 * vte_terminal_get_visible_bell:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not the terminal will present a visible bell to the
 * user when the child outputs the "bl" sequence.  The terminal
 * will clear itself to the default foreground color and then repaint itself.
 *
 * Returns: %TRUE if visible bell is enabled, %FALSE if not
 */
gboolean
vte_terminal_get_visible_bell(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->visible_bell;
}

/**
 * vte_terminal_set_allow_bold:
 * @terminal: a #VteTerminal
 * @allow_bold: %TRUE if the terminal should attempt to draw bold text
 *
 * Controls whether or not the terminal will attempt to draw bold text,
 * either by using a bold font variant or by repainting text with a different
 * offset.
 *
 */
void
vte_terminal_set_allow_bold(VteTerminal *terminal, gboolean allow_bold)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        allow_bold = allow_bold != FALSE;
        if (allow_bold == pvt->allow_bold)
                return;

	pvt->allow_bold = allow_bold;
        g_object_notify (G_OBJECT (terminal), "allow-bold");

	_vte_invalidate_all (terminal);
}

/**
 * vte_terminal_get_allow_bold:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not the terminal will attempt to draw bold text by
 * repainting text with a one-pixel offset.
 *
 * Returns: %TRUE if bolding is enabled, %FALSE if not
 */
gboolean
vte_terminal_get_allow_bold(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->allow_bold;
}

/**
 * vte_terminal_set_scroll_background:
 * @terminal: a #VteTerminal
 * @scroll: %TRUE if the terminal should scroll the background image along with
 * text.
 *
 * Controls whether or not the terminal will scroll the background image (if
 * one is set) when the text in the window must be scrolled.
 *
 * Since: 0.11
 *
 */
void
vte_terminal_set_scroll_background(VteTerminal *terminal, gboolean scroll)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        scroll = scroll != FALSE;
        if (scroll == pvt->scroll_background)
                return;

	pvt->scroll_background = scroll;

        g_object_notify (G_OBJECT (terminal), "scroll-background");

        vte_terminal_queue_background_update(terminal);
}

/**
 * vte_terminal_set_scroll_on_output:
 * @terminal: a #VteTerminal
 * @scroll: %TRUE if the terminal should scroll on output
 *
 * Controls whether or not the terminal will forcibly scroll to the bottom of
 * the viewable history when the new data is received from the child.
 *
 */
void
vte_terminal_set_scroll_on_output(VteTerminal *terminal, gboolean scroll)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->scroll_on_output = scroll;
}

/**
 * vte_terminal_set_scroll_on_keystroke:
 * @terminal: a #VteTerminal
 * @scroll: %TRUE if the terminal should scroll on keystrokes
 *
 * Controls whether or not the terminal will forcibly scroll to the bottom of
 * the viewable history when the user presses a key.  Modifier keys do not
 * trigger this behavior.
 *
 */
void
vte_terminal_set_scroll_on_keystroke(VteTerminal *terminal, gboolean scroll)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        scroll = scroll != FALSE;
        if (scroll == pvt->scroll_on_keystroke)
                return;

	pvt->scroll_on_keystroke = scroll;

        g_object_notify (G_OBJECT (terminal), "scroll-on-keystroke");
}

static void
vte_terminal_real_copy_clipboard(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SELECTION, "Copying to CLIPBOARD.\n");
	if (terminal->pvt->selection != NULL) {
		GtkClipboard *clipboard;
		clipboard = vte_terminal_clipboard_get(terminal,
						       GDK_SELECTION_CLIPBOARD);
		gtk_clipboard_set_text(clipboard, terminal->pvt->selection, -1);
	}
}

/**
 * vte_terminal_copy_clipboard:
 * @terminal: a #VteTerminal
 *
 * Places the selected text in the terminal in the #GDK_SELECTION_CLIPBOARD
 * selection.
 *
 */
void
vte_terminal_copy_clipboard(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_signal_emit (terminal, signals[COPY_CLIPBOARD], 0);
}

static void
vte_terminal_real_paste_clipboard(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SELECTION, "Pasting CLIPBOARD.\n");
	vte_terminal_paste(terminal, GDK_SELECTION_CLIPBOARD);
}

/**
 * vte_terminal_paste_clipboard:
 * @terminal: a #VteTerminal
 *
 * Sends the contents of the #GDK_SELECTION_CLIPBOARD selection to the
 * terminal's child.  If necessary, the data is converted from UTF-8 to the
 * terminal's current encoding. It's called on paste menu item, or when
 * user presses Shift+Insert.
 */
void
vte_terminal_paste_clipboard(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_signal_emit (terminal, signals[PASTE_CLIPBOARD], 0);
}

/**
 * vte_terminal_copy_primary:
 * @terminal: a #VteTerminal
 *
 * Places the selected text in the terminal in the #GDK_SELECTION_PRIMARY
 * selection.
 *
 */
void
vte_terminal_copy_primary(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	_vte_debug_print(VTE_DEBUG_SELECTION, "Copying to PRIMARY.\n");
	vte_terminal_copy(terminal, GDK_SELECTION_PRIMARY);
}

/**
 * vte_terminal_paste_primary:
 * @terminal: a #VteTerminal
 *
 * Sends the contents of the #GDK_SELECTION_PRIMARY selection to the terminal's
 * child.  If necessary, the data is converted from UTF-8 to the terminal's
 * current encoding.  The terminal will call also paste the
 * #GDK_SELECTION_PRIMARY selection when the user clicks with the the second
 * mouse button.
 *
 */
void
vte_terminal_paste_primary(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	_vte_debug_print(VTE_DEBUG_SELECTION, "Pasting PRIMARY.\n");
	vte_terminal_paste(terminal, GDK_SELECTION_PRIMARY);
}

/**
 * vte_terminal_im_append_menuitems:
 * @terminal: a #VteTerminal
 * @menushell: a GtkMenuShell
 *
 * Appends menu items for various input methods to the given menu.  The
 * user can select one of these items to modify the input method used by
 * the terminal.
 *
 */
void
vte_terminal_im_append_menuitems(VteTerminal *terminal, GtkMenuShell *menushell)
{
	GtkIMMulticontext *context;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(GTK_WIDGET_REALIZED(terminal));
	context = GTK_IM_MULTICONTEXT(terminal->pvt->im_context);
	gtk_im_multicontext_append_menuitems(context, menushell);
}

/* Set up whatever background we wanted. */
static gboolean
vte_terminal_background_update(VteTerminal *terminal)
{
	GdkColormap *colormap;
	GdkColor bgcolor;
	double saturation;

	/* If we're not realized yet, don't worry about it, because we get
	 * called when we realize. */
	if (!GTK_WIDGET_REALIZED(terminal)) {
		_vte_debug_print(VTE_DEBUG_MISC,
				"Can not set background image without "
				"window.\n");
		return TRUE;
	}

	_vte_debug_print(VTE_DEBUG_MISC|VTE_DEBUG_EVENTS,
			"Updating background image.\n");

	/* Set the default background color. */
	bgcolor.red = terminal->pvt->palette[VTE_DEF_BG].red;
	bgcolor.green = terminal->pvt->palette[VTE_DEF_BG].green;
	bgcolor.blue = terminal->pvt->palette[VTE_DEF_BG].blue;
	bgcolor.pixel = 0;
	gtk_widget_ensure_style(&terminal->widget);
	/*colormap = gdk_gc_get_colormap(terminal->widget.style->fg_gc[GTK_WIDGET_STATE(terminal)]);*/
	colormap = gtk_widget_get_colormap (&terminal->widget);
	if (colormap) {
		gdk_rgb_find_color(colormap, &bgcolor);
	}
	_vte_debug_print(VTE_DEBUG_MISC,
			"Setting background color to (%d, %d, %d) cmap index=%d.\n",
			bgcolor.red, bgcolor.green, bgcolor.blue,
			bgcolor.pixel);
	gdk_window_set_background(terminal->widget.window, &bgcolor);
	_vte_draw_set_background_solid (terminal->pvt->draw, &bgcolor, terminal->pvt->bg_opacity);

	/* If we're transparent, and either have no root image or are being
	 * told to update it, get a new copy of the root window. */
	saturation = (double) terminal->pvt->bg_saturation;
	saturation /= VTE_SATURATION_MAX;
	if (terminal->pvt->bg_transparent) {
		if (terminal->pvt->root_pixmap_changed_tag == 0) {
			VteBg *bg;

			/* Connect to background-change events. */
			bg = vte_bg_get_for_screen (gtk_widget_get_screen (&terminal->widget));
			terminal->pvt->root_pixmap_changed_tag =
				g_signal_connect(bg, "root-pixmap-changed",
					G_CALLBACK(root_pixmap_changed_cb),
					terminal);
		}

		_vte_draw_set_background_image(terminal->pvt->draw,
					       VTE_BG_SOURCE_ROOT,
					       NULL,
					       NULL,
					       &terminal->pvt->bg_tint_color,
					       saturation);
	} else
	if (terminal->pvt->bg_file) {
		_vte_draw_set_background_image(terminal->pvt->draw,
					       VTE_BG_SOURCE_FILE,
					       NULL,
					       terminal->pvt->bg_file,
					       &terminal->pvt->bg_tint_color,
					       saturation);
	} else
	if (GDK_IS_PIXBUF(terminal->pvt->bg_pixbuf)) {
		_vte_draw_set_background_image(terminal->pvt->draw,
					       VTE_BG_SOURCE_PIXBUF,
					       terminal->pvt->bg_pixbuf,
					       NULL,
					       &terminal->pvt->bg_tint_color,
					       saturation);
	} else {
		_vte_draw_set_background_image(terminal->pvt->draw,
					       VTE_BG_SOURCE_NONE,
					       NULL,
					       NULL,
					       &terminal->pvt->bg_tint_color,
					       saturation);
	}

	/* Note that the update has finished. */
	terminal->pvt->bg_update_pending = FALSE;

	/* Force a redraw for everything. */
	_vte_invalidate_all (terminal);

	return FALSE;
}

/* Queue an update of the background image, to be done as soon as we can
 * get to it.  Just bail if there's already an update pending, so that if
 * opaque move tables to screw us, we don't end up with an insane backlog
 * of updates after the user finishes moving us. */
static void
vte_terminal_queue_background_update(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_EVENTS,
			"Queued background update.\n");
	terminal->pvt->bg_update_pending = TRUE;
	/* force a redraw when convenient */
	add_update_timeout (terminal);
}

/**
 * vte_terminal_set_background_saturation:
 * @terminal: a #VteTerminal
 * @saturation: a floating point value between 0.0 and 1.0.
 *
 * If a background image has been set using
 * vte_terminal_set_background_image(),
 * vte_terminal_set_background_image_file(), or
 * vte_terminal_set_background_transparent(), and the saturation value is less
 * than 1.0, the terminal will adjust the colors of the image before drawing
 * the image.  To do so, the terminal will create a copy of the background
 * image (or snapshot of the root window) and modify its pixel values.
 *
 */
void
vte_terminal_set_background_saturation(VteTerminal *terminal, double saturation)
{
        VteTerminalPrivate *pvt;
	guint v;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

	v = CLAMP(saturation * VTE_SATURATION_MAX, 0, VTE_SATURATION_MAX);
        if (v == pvt->bg_saturation)
                return;

	_vte_debug_print(VTE_DEBUG_MISC,
			"Setting background saturation to %d/%d.\n",
			v, VTE_SATURATION_MAX);

        pvt->bg_saturation = v;
        g_object_notify(G_OBJECT (terminal), "background-saturation");

        vte_terminal_queue_background_update(terminal);
}

/**
 * vte_terminal_set_background_tint_color:
 * @terminal: a #VteTerminal
 * @color: a color which the terminal background should be tinted to if its
 * saturation is not 1.0.
 *
 * If a background image has been set using
 * vte_terminal_set_background_image(),
 * vte_terminal_set_background_image_file(), or
 * vte_terminal_set_background_transparent(), and the value set by
 * vte_terminal_set_background_saturation() is less than one, the terminal
 * will adjust the color of the image before drawing the image.  To do so,
 * the terminal will create a copy of the background image (or snapshot of
 * the root window) and modify its pixel values.  The initial tint color
 * is black.
 *
 * Since: 0.11
 *
 */
void
vte_terminal_set_background_tint_color(VteTerminal *terminal,
				       const GdkColor *color)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(color != NULL);

        pvt = terminal->pvt;

	_vte_debug_print(VTE_DEBUG_MISC,
			"Setting background tint to %d,%d,%d.\n",
			terminal->pvt->bg_tint_color.red >> 8,
			terminal->pvt->bg_tint_color.green >> 8,
			terminal->pvt->bg_tint_color.blue >> 8);
	if (color->red == terminal->pvt->bg_tint_color.red &&
            color->green == terminal->pvt->bg_tint_color.green &&
            color->blue == terminal->pvt->bg_tint_color.blue)
                return;

        pvt->bg_tint_color = *color;
        g_object_notify(G_OBJECT (terminal), "background-tint-color");

        vte_terminal_queue_background_update(terminal);
}

/**
 * vte_terminal_set_background_transparent:
 * @terminal: a #VteTerminal
 * @transparent: %TRUE if the terminal should fake transparency
 *
 * Sets the terminal's background image to the pixmap stored in the root
 * window, adjusted so that if there are no windows below your application,
 * the widget will appear to be transparent.
 *
 */
void
vte_terminal_set_background_transparent(VteTerminal *terminal,
					gboolean transparent)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        transparent = transparent != FALSE;
        if (transparent == pvt->bg_transparent)
                return;

	_vte_debug_print(VTE_DEBUG_MISC,
		"Turning background transparency %s.\n",
			transparent ? "on" : "off");
		
	pvt->bg_transparent = transparent;
        g_object_notify(G_OBJECT (terminal), "background-transparent");

        /* Update the background. */
        vte_terminal_queue_background_update(terminal);
}

/**
 * vte_terminal_set_background_image:
 * @terminal: a #VteTerminal
 * @image: a #GdkPixbuf to use, or %NULL to cancel
 *
 * Sets a background image for the widget.  Text which would otherwise be
 * drawn using the default background color will instead be drawn over the
 * specified image.  If necessary, the image will be tiled to cover the
 * widget's entire visible area. If specified by
 * vte_terminal_set_background_saturation(), the terminal will tint its
 * in-memory copy of the image before applying it to the terminal.
 *
 */
void
vte_terminal_set_background_image(VteTerminal *terminal, GdkPixbuf *image)
{
        VteTerminalPrivate *pvt;
        GObject *object;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(image==NULL || GDK_IS_PIXBUF(image));

        object = G_OBJECT(terminal);
        pvt = terminal->pvt;

        if (image && image == pvt->bg_pixbuf)
                return;

	_vte_debug_print(VTE_DEBUG_MISC,
			"%s background image.\n",
			GDK_IS_PIXBUF(image) ? "Setting" : "Clearing");

        g_object_freeze_notify(object);

	/* Get a ref to the new image if there is one.  Do it here just in
	 * case we're actually given the same one we're already using. */
	if (image != NULL) {
		g_object_ref(image);
	}

	/* Unref the previous background image. */
	if (pvt->bg_pixbuf != NULL) {
		g_object_unref(pvt->bg_pixbuf);
	}

	/* Clear a background file name, if one was set. */
        if (pvt->bg_file) {
                g_free(pvt->bg_file);
                pvt->bg_file = NULL;

                g_object_notify(object, "background-image-file");
        }

	/* Set the new background. */
	pvt->bg_pixbuf = image;

        g_object_notify(object, "background-image-pixbuf");

	vte_terminal_queue_background_update(terminal);

        g_object_thaw_notify(object);
}

/**
 * vte_terminal_set_background_image_file:
 * @terminal: a #VteTerminal
 * @path: path to an image file
 *
 * Sets a background image for the widget.  If specified by
 * vte_terminal_set_background_saturation(), the terminal will tint its
 * in-memory copy of the image before applying it to the terminal.
 *
 */
void
vte_terminal_set_background_image_file(VteTerminal *terminal, const char *path)
{
        VteTerminalPrivate *pvt;
        GObject *object;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        object = G_OBJECT(terminal);
        pvt = terminal->pvt;

	_vte_debug_print(VTE_DEBUG_MISC,
			"Loading background image from `%s'.\n", path);

        g_object_freeze_notify(G_OBJECT(terminal));

	/* Save this background type. */
	g_free(pvt->bg_file);
	pvt->bg_file = g_strdup(path);

	/* Turn off other background types. */
	if (pvt->bg_pixbuf != NULL) {
		g_object_unref(pvt->bg_pixbuf);
		pvt->bg_pixbuf = NULL;

                g_object_notify(object, "background-image-pixbuf");
	}

        g_object_notify(object, "background-image-file");

	vte_terminal_queue_background_update(terminal);

        g_object_thaw_notify(G_OBJECT(terminal));
}

/**
 * vte_terminal_get_has_selection:
 * @terminal: a #VteTerminal
 *
 * Checks if the terminal currently contains selected text.  Note that this
 * is different from determining if the terminal is the owner of any
 * #GtkClipboard items.
 *
 * Returns: %TRUE if part of the text in the terminal is selected.
 */
gboolean
vte_terminal_get_has_selection(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->has_selection;
}

/**
 * vte_terminal_get_using_xft:
 * @terminal: a #VteTerminal
 *
 * A #VteTerminal can use multiple methods to draw text.  This function
 * allows an application to determine whether or not the current method uses
 * fontconfig to find fonts.  This setting cannot be changed by the caller,
 * but in practice usually matches the behavior of GTK+ itself.
 *
 * Returns: %TRUE
 *
 * Deprecated: 0.19.1
 */
gboolean
vte_terminal_get_using_xft(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), TRUE);
	return TRUE;
}

static void
vte_terminal_set_cursor_blinks_internal(VteTerminal *terminal, gboolean blink)
{
        VteTerminalPrivate *pvt = terminal->pvt;

	blink = !!blink;
	if (pvt->cursor_blinks == blink)
		return;

	pvt->cursor_blinks = blink;

	if (!GTK_WIDGET_REALIZED (terminal) ||
	    !GTK_WIDGET_HAS_FOCUS (terminal))
		return;

	if (blink)
		add_cursor_timeout (terminal);
	else
		remove_cursor_timeout (terminal);
}

/**
 * vte_terminal_set_cursor_blinks:
 * @terminal: a #VteTerminal
 * @blink: %TRUE if the cursor should blink
 *
 *  Sets whether or not the cursor will blink.
 *
 * Deprecated: 0.17.1 Use vte_terminal_set_cursor_blink_mode() instead.
 */
void
vte_terminal_set_cursor_blinks(VteTerminal *terminal, gboolean blink)
{
        vte_terminal_set_cursor_blink_mode(terminal, blink ? VTE_CURSOR_BLINK_ON : VTE_CURSOR_BLINK_OFF);
}

/**
 * vte_terminal_set_cursor_blink_mode:
 * @terminal: a #VteTerminal
 * @mode: the #VteTerminalCursorBlinkMode to use
 *
 * Sets whether or not the cursor will blink. Using %VTE_CURSOR_BLINK_SYSTEM
 * will use the #GtkSettings::gtk-cursor-blink setting.
 *
 * Since: 0.17.1
 */
void
vte_terminal_set_cursor_blink_mode(VteTerminal *terminal, VteTerminalCursorBlinkMode mode)
{
        VteTerminalPrivate *pvt;
        gboolean blinks;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        pvt = terminal->pvt;

        if (pvt->cursor_blink_mode == mode)
                return;

        pvt->cursor_blink_mode = mode;

        switch (mode) {
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

        vte_terminal_set_cursor_blinks_internal(terminal, blinks);

        g_object_notify(G_OBJECT(terminal), "cursor-blink-mode");
}

/**
 * vte_terminal_get_cursor_blink_mode:
 * @terminal: a #VteTerminal
 *
 * Returns the currently set cursor blink mode.
 *
 * Return value: cursor blink mode.
 *
 * Since: 0.17.1
 */
VteTerminalCursorBlinkMode
vte_terminal_get_cursor_blink_mode(VteTerminal *terminal)
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), VTE_CURSOR_BLINK_SYSTEM);

        return terminal->pvt->cursor_blink_mode;
}

/**
 * vte_terminal_set_cursor_shape:
 * @terminal: a #VteTerminal
 * @shape: the #VteTerminalCursorShape to use
 *
 * Sets the shape of the cursor drawn.
 *
 * Since: 0.19.1
 */
void
vte_terminal_set_cursor_shape(VteTerminal *terminal, VteTerminalCursorShape shape)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
        pvt = terminal->pvt;

        if (pvt->cursor_shape == shape)
                return;

        pvt->cursor_shape = shape;
	_vte_invalidate_cursor_once(terminal, FALSE);

        g_object_notify(G_OBJECT(terminal), "cursor-shape");
}

/**
 * vte_terminal_get_cursor_shape:
 * @terminal: a #VteTerminal
 *
 * Returns the currently set cursor shape.
 *
 * Return value: cursor shape.
 *
 * Since: 0.17.6
 */
VteTerminalCursorShape
vte_terminal_get_cursor_shape(VteTerminal *terminal)
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), VTE_CURSOR_SHAPE_BLOCK);

        return terminal->pvt->cursor_shape;
}

/**
 * vte_terminal_set_scrollback_lines:
 * @terminal: a #VteTerminal
 * @lines: the length of the history buffer
 *
 * Sets the length of the scrollback buffer used by the terminal.  The size of
 * the scrollback buffer will be set to the larger of this value and the number
 * of visible rows the widget can display, so 0 can safely be used to disable
 * scrollback.  Note that this setting only affects the normal screen buffer.
 * For terminal types which have an alternate screen buffer, no scrollback is
 * allowed on the alternate screen buffer.
 *
 */
void
vte_terminal_set_scrollback_lines(VteTerminal *terminal, glong lines)
{
        VteTerminalPrivate *pvt;
        GObject *object;
	glong scroll_delta;
	VteScreen *screen;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        object = G_OBJECT(terminal);
        pvt = terminal->pvt;

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
	if (screen == &terminal->pvt->normal_screen) {
		glong low, high, next;
		/* We need at least as many lines as are visible */
		lines = MAX (lines, terminal->row_count);
		next = MAX (screen->cursor_current.row + 1,
				_vte_ring_next (screen->row_data));
		_vte_ring_resize (screen->row_data, lines);
		low = _vte_ring_delta (screen->row_data);
		high = low + lines - terminal->row_count + 1;
		screen->insert_delta = CLAMP (screen->insert_delta, low, high);
		scroll_delta = CLAMP (scroll_delta, low, screen->insert_delta);
		next = MIN (next, screen->insert_delta + terminal->row_count);
		if (_vte_ring_next (screen->row_data) > next){
			_vte_ring_shrink (screen->row_data, next - low);
		}
	} else {
		_vte_ring_resize (screen->row_data, terminal->row_count);
		scroll_delta = _vte_ring_delta (screen->row_data);
		screen->insert_delta = _vte_ring_delta (screen->row_data);
		if (_vte_ring_next (screen->row_data) > screen->insert_delta + terminal->row_count){
			_vte_ring_shrink (screen->row_data, terminal->row_count);
		}
	}

	/* Adjust the scrollbars to the new locations. */
	vte_terminal_queue_adjustment_value_changed (terminal, scroll_delta);
	_vte_terminal_adjust_adjustments_full (terminal);

        g_object_notify(object, "scrollback-lines");

        g_object_thaw_notify(object);
}

/**
 * vte_terminal_set_word_chars:
 * @terminal: a #VteTerminal
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
vte_terminal_set_word_chars(VteTerminal *terminal, const char *spec)
{
	VteConv conv;
	gunichar *wbuf;
	guchar *ibuf, *ibufptr, *obuf, *obufptr;
	gsize ilen, olen;
	VteWordCharRange range;
	guint i;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
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
        /* FIXME: why not just directly use g_utf8_to_ucs4 here? It'll never fail */
	conv = _vte_conv_open(VTE_CONV_GUNICHAR_TYPE, "UTF-8");
	if (conv == VTE_INVALID_CONV) {
		/* Aaargh.  We're screwed. */
		g_warning(_("_vte_conv_open() failed setting word characters"));
		return;
	}
	ilen = strlen(spec);
	ibuf = ibufptr = (guchar *)g_strdup(spec);
	olen = (ilen + 1) * sizeof(gunichar);
	_vte_buffer_set_minimum_size(terminal->pvt->conv_buffer, olen);
	obuf = obufptr = terminal->pvt->conv_buffer->data;
	wbuf = (gunichar*) obuf;
	wbuf[ilen] = '\0';
	_vte_conv(conv, (const guchar **)&ibuf, &ilen, &obuf, &olen);
	_vte_conv_close(conv);
	for (i = 0; i < ((obuf - obufptr) / sizeof(gunichar)); i++) {
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
	g_free(ibufptr);

        g_object_notify(G_OBJECT(terminal), "word-chars");
}

/**
 * vte_terminal_set_backspace_binding:
 * @terminal: a #VteTerminal
 * @binding: a #VteTerminalEraseBinding for the backspace key
 *
 * Modifies the terminal's backspace key binding, which controls what
 * string or control sequence the terminal sends to its child when the user
 * presses the backspace key.
 *
 */
void
vte_terminal_set_backspace_binding(VteTerminal *terminal,
				   VteTerminalEraseBinding binding)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        if (binding == pvt->backspace_binding)
                return;

	/* FIXME: should we set the pty mode to match? */
	pvt->backspace_binding = binding;

        g_object_notify(G_OBJECT(terminal), "backspace-binding");
}

/**
 * vte_terminal_set_delete_binding:
 * @terminal: a #VteTerminal
 * @binding: a #VteTerminalEraseBinding for the delete key
 *
 * Modifies the terminal's delete key binding, which controls what
 * string or control sequence the terminal sends to its child when the user
 * presses the delete key.
 *
 */
void
vte_terminal_set_delete_binding(VteTerminal *terminal,
				VteTerminalEraseBinding binding)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        if (binding == pvt->delete_binding)
                return;

	pvt->delete_binding = binding;

        g_object_notify(G_OBJECT(terminal), "delete-binding");
}

/**
 * vte_terminal_set_mouse_autohide:
 * @terminal: a #VteTerminal
 * @setting: %TRUE if the autohide should be enabled
 *
 * Changes the value of the terminal's mouse autohide setting.  When autohiding
 * is enabled, the mouse cursor will be hidden when the user presses a key and
 * shown when the user moves the mouse.  This setting can be read using
 * vte_terminal_get_mouse_autohide().
 *
 */
void
vte_terminal_set_mouse_autohide(VteTerminal *terminal, gboolean setting)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        setting = setting != FALSE;
        if (setting == pvt->mouse_autohide)
                return;

	pvt->mouse_autohide = setting;

        g_object_notify(G_OBJECT(terminal), "pointer-autohide");
}

/**
 * vte_terminal_get_mouse_autohide:
 * @terminal: a #VteTerminal
 *
 * Determines the value of the terminal's mouse autohide setting.  When
 * autohiding is enabled, the mouse cursor will be hidden when the user presses
 * a key and shown when the user moves the mouse.  This setting can be changed
 * using vte_terminal_set_mouse_autohide().
 *
 * Returns: %TRUE if autohiding is enabled, %FALSE if not.
 */
gboolean
vte_terminal_get_mouse_autohide(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->mouse_autohide;
}

/**
 * vte_terminal_reset:
 * @terminal: a #VteTerminal
 * @full: %TRUE to reset tabstops
 * @clear_history: %TRUE to empty the terminal's scrollback buffer
 *
 * Resets as much of the terminal's internal state as possible, discarding any
 * unprocessed input data, resetting character attributes, cursor state,
 * national character set state, status line, terminal modes (insert/delete),
 * selection state, and encoding.
 *
 */
void
vte_terminal_reset(VteTerminal *terminal, gboolean full, gboolean clear_history)
{
        VteTerminalPrivate *pvt;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        pvt = terminal->pvt;

        g_object_freeze_notify(G_OBJECT(terminal));

	/* Stop processing any of the data we've got backed up. */
	vte_terminal_stop_processing (terminal);

	/* Clear the input and output buffers. */
	_vte_incoming_chunks_release (pvt->incoming);
	pvt->incoming = NULL;
	g_array_set_size(pvt->pending, 0);
	_vte_buffer_clear(pvt->outgoing);
	/* Reset charset substitution state. */
	_vte_iso2022_state_free(pvt->iso2022);
	pvt->iso2022 = _vte_iso2022_state_new(NULL,
							&_vte_terminal_codeset_changed_cb,
							terminal);
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
	/* vte_terminal_set_default_colors(terminal); */
	/* Reset the default attributes.  Reset the alternate attribute because
	 * it's not a real attribute, but we need to treat it as one here. */
	pvt->screen = &pvt->alternate_screen;
	_vte_terminal_set_default_attributes(terminal);
	pvt->screen = &pvt->normal_screen;
	_vte_terminal_set_default_attributes(terminal);
	/* Reset alternate charset mode. */
	pvt->normal_screen.alternate_charset = FALSE;
	pvt->alternate_screen.alternate_charset = FALSE;
	/* Clear the scrollback buffers and reset the cursors. */
	if (clear_history) {
		_vte_ring_fini(pvt->normal_screen.row_data);
		_vte_ring_init(pvt->normal_screen.row_data, pvt->scrollback_lines);
		_vte_ring_fini(pvt->alternate_screen.row_data);
		_vte_ring_init(pvt->alternate_screen.row_data, terminal->row_count);
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
		_vte_terminal_adjust_adjustments_full (terminal);
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
	if (full) {
		vte_terminal_set_default_tabstops(terminal);
	}
	/* Reset restricted scrolling regions, leave insert mode, make
	 * the cursor visible again. */
	pvt->normal_screen.scrolling_restricted = FALSE;
	pvt->normal_screen.sendrecv_mode = TRUE;
	pvt->normal_screen.insert_mode = FALSE;
	pvt->normal_screen.linefeed_mode = FALSE;
	pvt->normal_screen.origin_mode = FALSE;
	pvt->normal_screen.reverse_mode = FALSE;
	pvt->alternate_screen.scrolling_restricted = FALSE;
	pvt->alternate_screen.sendrecv_mode = TRUE;
	pvt->alternate_screen.insert_mode = FALSE;
	pvt->alternate_screen.linefeed_mode = FALSE;
	pvt->alternate_screen.origin_mode = FALSE;
	pvt->alternate_screen.reverse_mode = FALSE;
	pvt->cursor_visible = TRUE;
	/* Reset the encoding. */
	vte_terminal_set_encoding(terminal, NULL);
	g_assert(pvt->encoding != NULL);
	/* Reset selection. */
	vte_terminal_deselect_all(terminal);
	pvt->has_selection = FALSE;
	pvt->selecting = FALSE;
	pvt->selecting_restart = FALSE;
	pvt->selecting_had_delta = FALSE;
	if (pvt->selection != NULL) {
		g_free(pvt->selection);
		pvt->selection = NULL;
		memset(&pvt->selection_origin, 0,
		       sizeof(&pvt->selection_origin));
		memset(&pvt->selection_last, 0,
		       sizeof(&pvt->selection_last));
		memset(&pvt->selection_start, 0,
		       sizeof(&pvt->selection_start));
		memset(&pvt->selection_end, 0,
		       sizeof(&pvt->selection_end));
	}
	/* Reset mouse motion events. */
	pvt->mouse_tracking_mode = MOUSE_TRACKING_NONE;
	pvt->mouse_last_button = 0;
	pvt->mouse_last_x = 0;
	pvt->mouse_last_y = 0;
	/* Clear modifiers. */
	pvt->modifiers = 0;
	/* Cause everything to be redrawn (or cleared). */
	vte_terminal_maybe_scroll_to_bottom(terminal);
	_vte_invalidate_all(terminal);

        g_object_thaw_notify(G_OBJECT(terminal));
}

/**
 * vte_terminal_get_status_line:
 * @terminal: a #VteTerminal
 *
 * Some terminal emulations specify a status line which is separate from the
 * main display area, and define a means for applications to move the cursor
 * to the status line and back.
 *
 * Returns: the current contents of the terminal's status line.  For terminals
 * like "xterm", this will usually be the empty string.  The string must not
 * be modified or freed by the caller.
 */
const char *
vte_terminal_get_status_line(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return terminal->pvt->screen->status_line_contents->str;
}

/**
 * vte_terminal_get_padding:
 * @terminal: a #VteTerminal
 * @xpad: address in which to store left/right-edge padding
 * @ypad: address in which to store top/bottom-edge ypadding
 *
 * Determines the amount of additional space the widget is using to pad the
 * edges of its visible area.  This is necessary for cases where characters in
 * the selected font don't themselves include a padding area and the text
 * itself would otherwise be contiguous with the window border.  Applications
 * which use the widget's %row_count, %column_count, %char_height, and
 * %char_width fields to set geometry hints using
 * gtk_window_set_geometry_hints() will need to add this value to the base
 * size.  The values returned in @xpad and @ypad are the total padding used in
 * each direction, and do not need to be doubled.
 *
 */
void
vte_terminal_get_padding(VteTerminal *terminal, int *xpad, int *ypad)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(xpad != NULL);
	g_return_if_fail(ypad != NULL);
	*xpad = 2 * VTE_PAD_WIDTH;
	*ypad = 2 * VTE_PAD_WIDTH;
}

/**
 * vte_terminal_get_adjustment:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's adjustment field
 */
GtkAdjustment *
vte_terminal_get_adjustment(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return terminal->adjustment;
}

/**
 * vte_terminal_get_char_width:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's char_width field
 */
glong
vte_terminal_get_char_width(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	vte_terminal_ensure_font (terminal);
	return terminal->char_width;
}

/**
 * vte_terminal_get_char_height:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's char_height field
 */
glong
vte_terminal_get_char_height(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	vte_terminal_ensure_font (terminal);
	return terminal->char_height;
}

/**
 * vte_terminal_get_char_descent:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's char_descent field
 *
 * Deprecated: 0.19.1
 */
glong
vte_terminal_get_char_descent(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	vte_terminal_ensure_font (terminal);
	return terminal->char_descent;
}

/**
 * vte_terminal_get_char_ascent:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's char_ascent field
 *
 * Deprecated: 0.19.1
 */
glong
vte_terminal_get_char_ascent(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	vte_terminal_ensure_font (terminal);
	return terminal->char_ascent;
}

/**
 * vte_terminal_get_row_count:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's row_count field
 */
glong
vte_terminal_get_row_count(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	return terminal->row_count;
}

/**
 * vte_terminal_get_column_count:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's column_count field
 */
glong
vte_terminal_get_column_count(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	return terminal->column_count;
}

/**
 * vte_terminal_get_window_title:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's window_title field
 */
const char *
vte_terminal_get_window_title(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), "");
	return terminal->window_title;
}

/**
 * vte_terminal_get_icon_title:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's icon_title field
 */
const char *
vte_terminal_get_icon_title(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), "");
	return terminal->icon_title;
}

/**
 * vte_terminal_set_pty:
 * @terminal: a #VteTerminal
 * @pty_master: a file descriptor of the master end of a PTY
 *
 * Attach an existing PTY master side to the terminal widget.  Use
 * instead of vte_terminal_fork_command() or vte_terminal_forkpty().
 *
 * Since: 0.12.1
 */
void
vte_terminal_set_pty(VteTerminal *terminal, int pty_master)
{
        long flags;
        VteTerminalPrivate *pvt;
        GObject *object;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        object = G_OBJECT(terminal);
        pvt = terminal->pvt;
        if (pty_master == pvt->pty_master) {
                return;
        }

        g_object_freeze_notify(object);

       if (terminal->pvt->pty_channel != NULL) {
	       g_io_channel_unref (terminal->pvt->pty_channel);
       }
       if (terminal->pvt->pty_master != -1) {
               _vte_pty_close (terminal->pvt->pty_master);
               close (terminal->pvt->pty_master);
       }
       terminal->pvt->pty_master = pty_master;
       terminal->pvt->pty_channel = g_io_channel_unix_new (pty_master);
       g_io_channel_set_close_on_unref (terminal->pvt->pty_channel, FALSE);


       /* Set the pty to be non-blocking. */
       flags = fcntl (terminal->pvt->pty_master, F_GETFL);
       if ((flags & O_NONBLOCK) == 0) {
	       fcntl (terminal->pvt->pty_master, F_SETFL, flags | O_NONBLOCK);
       }

       vte_terminal_set_size (terminal,
                              terminal->column_count,
                              terminal->row_count);

       _vte_terminal_setup_utf8 (terminal);

       /* Open channels to listen for input on. */
       _vte_terminal_connect_pty_read (terminal);

        g_object_notify(object, "pty");

        g_object_thaw_notify(object);
}

/**
 * vte_terminal_get_pty:
 * @terminal: a #VteTerminal
 *
 * Returns the file descriptor of the master end of @terminal's PTY.
 *
 * Return value: the file descriptor, or -1 if the terminal has no PTY.
 *
 * Since: 0.19.1
 */
int vte_terminal_get_pty(VteTerminal *terminal)
{
  g_return_val_if_fail (VTE_IS_TERMINAL (terminal), -1);

  return terminal->pvt->pty_master;
}

/**
 * vte_terminal_get_child_exit_status:
 * @terminal: a #VteTerminal
 *
 * Gets the exit status of the command started by vte_terminal_fork_command().
 * See your C library's documentation for more details on how to interpret the
 * exit status.
 * 
 * Note that this function may only be called from the signal handler of
 * the "child-exited" signal.
 * 
 * Returns: the child's exit status
 * 
 * Since: 0.19.1
 */
int
vte_terminal_get_child_exit_status(VteTerminal *terminal)
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
        return terminal->pvt->child_exit_status;
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

	return g_strdup (terminal->pvt->selection);
}

void
_vte_terminal_get_start_selection(VteTerminal *terminal, long *col, long *row)
{
	struct selection_cell_coords ss;

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
	struct selection_cell_coords se;

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

	terminal->pvt->selection_type = selection_type_char;
	terminal->pvt->selecting_had_delta = TRUE;
	terminal->pvt->selection_start.col = start_col;
	terminal->pvt->selection_start.row = start_row;
	terminal->pvt->selection_end.col = end_col;
	terminal->pvt->selection_end.row = end_row;
	vte_terminal_copy_primary(terminal);
	_vte_invalidate_region (terminal,
			MIN (start_col, start_row), MAX (start_col, start_row),
			MIN (start_row, end_row),   MAX (start_row, end_row),
			FALSE);

	vte_terminal_emit_selection_changed(terminal);
}

void
_vte_terminal_remove_selection(VteTerminal *terminal)
{
	vte_terminal_deselect_all (terminal);
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
				(GFunc)gdk_region_destroy, NULL);
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
vte_terminal_emit_pending_signals(VteTerminal *terminal)
{
        GObject *object = G_OBJECT(terminal);

        g_object_freeze_notify(object);

	vte_terminal_emit_adjustment_changed (terminal);

	if (terminal->pvt->screen->status_line_changed) {
		_vte_terminal_emit_status_line_changed (terminal);
		terminal->pvt->screen->status_line_changed = FALSE;
	}

	if (terminal->pvt->window_title_changed) {
		g_free (terminal->window_title);
		terminal->window_title = terminal->pvt->window_title_changed;
		terminal->pvt->window_title_changed = NULL;

		if (terminal->widget.window)
			gdk_window_set_title (terminal->widget.window,
					terminal->window_title);
		vte_terminal_emit_window_title_changed(terminal);
                g_object_notify(object, "window-title");
	}

	if (terminal->pvt->icon_title_changed) {
		g_free (terminal->icon_title);
		terminal->icon_title = terminal->pvt->icon_title_changed;
		terminal->pvt->icon_title_changed = NULL;

		if (terminal->widget.window)
			gdk_window_set_icon_name (terminal->widget.window,
					terminal->icon_title);
		vte_terminal_emit_icon_title_changed(terminal);
                g_object_notify(object, "icon-title");
	}

	/* Flush any pending "inserted" signals. */
	vte_terminal_emit_cursor_moved(terminal);
	vte_terminal_emit_pending_text_signals(terminal, 0);
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

	GDK_THREADS_ENTER();

	in_process_timeout = TRUE;

	_vte_debug_print (VTE_DEBUG_WORK, "<");
	_vte_debug_print (VTE_DEBUG_TIMEOUT,
			"Process timeout:  %d active\n",
			g_list_length (active_terminals));

	for (l = active_terminals; l != NULL; l = next) {
		VteTerminal *terminal = l->data;
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
				"Stoping process timeout\n");
		process_timeout_tag = 0;
		again = FALSE;
	}

	in_process_timeout = FALSE;

	GDK_THREADS_LEAVE();

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
	GdkRegion *region;

	if (G_UNLIKELY (!GTK_WIDGET_DRAWABLE(terminal) ||
				terminal->pvt->visibility_state == GDK_VISIBILITY_FULLY_OBSCURED)) {
		reset_update_regions (terminal);
		return FALSE;
	}

	if (G_UNLIKELY (!terminal->pvt->update_regions))
		return FALSE;


	l = terminal->pvt->update_regions;
	if (g_slist_next (l) != NULL) {
		/* amalgamate into one super-region */
		region = gdk_region_new ();
		do {
			gdk_region_union (region, l->data);
			gdk_region_destroy (l->data);
		} while ((l = g_slist_next (l)) != NULL);
	} else {
		region = l->data;
	}
	g_slist_free (terminal->pvt->update_regions);
	terminal->pvt->update_regions = NULL;
	terminal->pvt->invalidated_all = FALSE;

	/* and perform the merge with the window visible area */
	gdk_window_invalidate_region (terminal->widget.window, region, FALSE);
	gdk_window_process_updates (terminal->widget.window, FALSE);
	gdk_region_destroy (region);

	_vte_debug_print (VTE_DEBUG_WORK, "-");

	return TRUE;
}

static gboolean
update_repeat_timeout (gpointer data)
{
	GList *l, *next;
	gboolean again;

	GDK_THREADS_ENTER();

	in_update_timeout = TRUE;

	_vte_debug_print (VTE_DEBUG_WORK, "[");
	_vte_debug_print (VTE_DEBUG_TIMEOUT,
			"Repeat timeout:  %d active\n",
			g_list_length (active_terminals));

	for (l = active_terminals; l != NULL; l = next) {
		VteTerminal *terminal = l->data;

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
		if (terminal->pvt->bg_update_pending) {
			vte_terminal_background_update (terminal);
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

	GDK_THREADS_LEAVE();

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

	GDK_THREADS_ENTER();

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
		VteTerminal *terminal = l->data;

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
		if (terminal->pvt->bg_update_pending) {
			vte_terminal_background_update (terminal);
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

	GDK_THREADS_LEAVE();

	return FALSE;
}
