/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
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
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <iconv.h>
#include <langinfo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <X11/Xlib.h>
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include "caps.h"
#include "marshal.h"
#include "pty.h"
#include "termcap.h"
#include "trie.h"
#include "vte.h"
#ifdef HAVE_XFT
#include <X11/Xft/Xft.h>
#endif

#define VTE_TAB_WIDTH 8

/* A function which can handle a terminal control sequence. */
typedef void (*VteTerminalSequenceHandler)(VteTerminal *terminal,
					   const char *match,
					   GQuark match_quark,
					   GValueArray *params);

static void vte_sequence_handler_clear_screen(VteTerminal *terminal,
					      const char *match,
					      GQuark match_quark,
					      GValueArray *params);
static void vte_sequence_handler_ho(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_do(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_up(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);

/* Allocate a new line. */
static GArray *
vte_new_row_data()
{
	return g_array_new(FALSE, FALSE, sizeof(struct vte_charcell));
}

/* Reset defaults for character insertion. */
static void
vte_terminal_set_default_attributes(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	memset(&terminal->screen->defaults, 0,
	       sizeof(terminal->screen->defaults));
	terminal->screen->defaults.fore = 7;
	terminal->screen->defaults.back = 0;
	terminal->screen->defaults.reverse = 0;
	terminal->screen->defaults.invisible = 0;
	terminal->screen->defaults.half = 0;
	terminal->screen->defaults.underline = 0;
	terminal->screen->defaults.blink = 0;
	terminal->screen->defaults.standout = 0;
	terminal->screen->defaults.bold = 0;
}

/* Cause certain cells to be updated. */
static void
vte_invalidate_cells(VteTerminal *terminal,
		     gint column_start, gint column_count,
		     gint row_start, gint row_count)
{
	GdkRectangle rect;
	GtkWidget *widget = GTK_WIDGET(terminal);

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	/* Clamp the start values to reasonable numbers. */
	column_start = (column_start > 0) ? column_start : 0;
	row_start = (row_start > 0) ? row_start : 0;

	/* Convert the column and row start and end to pixel values
	 * by multiplying by the size of a character cell. */
	rect.x = widget->allocation.x + column_start * terminal->char_width;
	rect.width = column_count * terminal->char_width;
	rect.y = widget->allocation.y + row_start * terminal->char_height;
	rect.height = row_count * terminal->char_height;

	/* Invalidate the rectangle. */
	gdk_window_invalidate_rect(widget->window, &rect, TRUE);
}

/* Update the adjustment field of the widget.  This function should be called
 * whenever we add rows to the history or switch screens. */
static void
vte_terminal_adjust_adjustments(VteTerminal *terminal)
{
	gboolean changed;
	guint page_size;
	long rows;
	/* Adjust the vertical, uh, adjustment. */
	changed = FALSE;
	/* The lower value should always be zero. */
	if (terminal->adjustment->lower != 0) {
		terminal->adjustment->lower = 0;
		changed = TRUE;
	}
	/* The upper value is the number of rows which might be visible.  (Add
	 * one to the cursor offset because it's zero-based.) */
	rows = MAX(terminal->screen->row_data->len,
		   terminal->screen->cursor_current.row + 1);
	if (terminal->adjustment->upper != rows) {
		terminal->adjustment->upper = rows;
		changed = TRUE;
	}
	/* The step increment should always be one. */
	if (terminal->adjustment->step_increment != 1) {
		terminal->adjustment->step_increment = 1;
		changed = TRUE;
	}
	/* Set the number of rows the user sees to the number of rows the
	 * user sees. */
	page_size = terminal->row_count;
	if (terminal->adjustment->page_size != page_size) {
		terminal->adjustment->page_size = page_size;
		changed = TRUE;
	}
	/* Clicking in the empty area should scroll one screen, so set the
	 * page size to the number of visible rows. */
	if (terminal->adjustment->page_increment != page_size) {
		terminal->adjustment->page_increment = page_size;
		changed = TRUE;
	}
	/* If anything changed, signal that there was a change. */
	if (changed == TRUE) {
		gtk_adjustment_changed(terminal->adjustment);
	}
}

/* Call another function, offsetting any long arguments by the given
 * increment value. */
static void
vte_sequence_handler_offset(VteTerminal *terminal,
			    const char *match,
			    GQuark match_quark,
			    GValueArray *params,
			    int increment,
			    VteTerminalSequenceHandler handler)
{
	int i;
	long val;
	GValue *value;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* Decrement the parameters and let the _cs handler deal with it. */
	for (i = 0; (params != NULL) && (i < params->n_values); i++) {
		value = g_value_array_get_nth(params, i);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = g_value_get_long(value);
			val += increment;
			g_value_set_long(value, val);
		}
	}
	handler(terminal, match, match_quark, params);
}

/* Call another function a given number of times, or once. */
static void
vte_sequence_handler_multiple(VteTerminal *terminal,
			      const char *match,
			      GQuark match_quark,
			      GValueArray *params,
			      VteTerminalSequenceHandler handler)
{
	long val = 1;
	int i;
	GValue *value;
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = g_value_get_long(value);
		}
	}
	for (i = 0; i < val; i++) {
		handler(terminal, match, match_quark, NULL);
	}
}

/* Insert a blank line at an arbitrary position. */
static void
vte_insert_line_int(VteTerminal *terminal, long position)
{
	GArray *array;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* Pad out the line data to the insertion point. */
	while (terminal->screen->row_data->len < position) {
		array = vte_new_row_data();
		g_array_append_val(terminal->screen->row_data, array);
	}
	/* If we haven't inserted a line yet, insert a new one. */
	array = vte_new_row_data();
	if (terminal->screen->row_data->len >= position) {
		g_array_insert_val(terminal->screen->row_data, position, array);
	} else {
		g_array_append_val(terminal->screen->row_data, array);
	}
}

/* Remove a line at an arbitrary position. */
static void
vte_remove_line_int(VteTerminal *terminal, long position)
{
	GArray *array;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->screen->row_data->len > position) {
		array = g_array_index(terminal->screen->row_data,
				      GArray *,
				      position);
		g_array_remove_index(terminal->screen->row_data, position);
		g_array_free(array, TRUE);
	}
}

/* Change the encoding used for the terminal to the given codeset, or the
 * locale default if NULL is passed in. */
static void
vte_terminal_set_encoding(VteTerminal *terminal, const char *codeset)
{
	if (codeset == NULL) {
		codeset = nl_langinfo(CODESET);
	}

	if (terminal->pending_conv != NULL) {
		iconv_close(terminal->pending_conv);
	}
	terminal->pending_conv = iconv_open("WCHAR_T", codeset);

	if (terminal->outgoing_conv != NULL) {
		iconv_close(terminal->outgoing_conv);
	}
	terminal->outgoing_conv = iconv_open(codeset, "WCHAR_T");

	terminal->encoding = g_quark_to_string(g_quark_from_string(codeset));
#ifdef VTE_DEBUG
	g_print("Set encoding to `%s'.\n", terminal->encoding);
#endif
}

/* Add a line at the current cursor position. */
static void
vte_sequence_handler_al(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct _VteScreen *screen;
	long start, end;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	if (screen->scrolling_restricted) {
		start = screen->delta + screen->scrolling_region.start;
		end = screen->delta + screen->scrolling_region.end;
	} else {
		start = screen->delta;
		end = screen->delta + terminal->row_count - 1;
	}
	vte_remove_line_int(terminal, end);
	vte_insert_line_int(terminal, screen->cursor_current.row);
	screen->cursor_current.row++;
	vte_invalidate_cells(terminal,
			     0, terminal->column_count,
			     start - screen->delta, end - start + 1);
}

/* Add N lines at the current cursor position. */
static void
vte_sequence_handler_AL(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_sequence_handler_multiple(terminal, match, match_quark, params,
				      vte_sequence_handler_al);
}

/* Beep. */
static void
vte_sequence_handler_bl(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	gdk_beep();
}

/* Clear from the cursor position to the beginning of the line. */
static void
vte_sequence_handler_cb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GArray *rowdata;
	long delta, i;
	struct _VteScreen *screen;
	struct vte_charcell *pcell;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	if (screen->row_data->len > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = g_array_index(screen->row_data,
					GArray*,
					screen->cursor_current.row);
		/* Clear the data up to the current column. */
		for (i = 0; i < screen->cursor_current.col; i++) {
			pcell = &g_array_index(rowdata, struct vte_charcell, i);
			if (pcell != NULL) {
				pcell->c = ' ';
				pcell->columns = 1;
			}
		}
		/* Repaint this row. */
		delta = screen->delta;
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     screen->cursor_current.row - delta, 1);
	}
}

/* Clear below the current line. */
static void
vte_sequence_handler_cd(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GArray *rowdata;
	long delta, i;
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	/* If the cursor is actually on the screen, clear data in the rows
	 * below the cursor. */
	delta = screen->delta;
	for (i = screen->cursor_current.row + 1;
	     i < screen->row_data->len;
	     i++) {
		/* Get the data for the row we're removing. */
		rowdata = g_array_index(screen->row_data, GArray*, i);
		/* Remove it. */
		while ((rowdata != NULL) && (rowdata->len > 0)) {
			g_array_remove_index(rowdata, rowdata->len - 1);
		}
		/* Repaint this row. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     i - delta, 1);
	}
}

/* Clear from the cursor position to the end of the line. */
static void
vte_sequence_handler_ce(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GArray *rowdata;
	long delta;
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	delta = screen->delta;
	if (screen->row_data->len > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = g_array_index(screen->row_data, GArray*,
					screen->cursor_current.row);
		/* Remove the data at the end of the array. */
		while (rowdata->len > screen->cursor_current.col) {
			g_array_remove_index(rowdata, rowdata->len - 1);
		}
		/* Repaint this row. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     screen->cursor_current.row - delta, 1);
	}
}

/* Move the cursor to the given column (horizontal position). */
static void
vte_sequence_handler_ch(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct _VteScreen *screen;
	GValue *value;
	long delta;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	delta = screen->delta;
	/* Repaint the current cursor position. */
	vte_invalidate_cells(terminal,
			     screen->cursor_current.col, 1,
			     screen->cursor_current.row - delta, 1);
	/* We only care if there's a parameter in there. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			/* Move the cursor and repaint it. */
			screen->cursor_current.col = g_value_get_long(value);
			vte_invalidate_cells(terminal,
					     screen->cursor_current.col,
					     1,
					     screen->cursor_current.row - delta,
					     1);
		}
	}
}

/* Clear the screen and home the cursor. */
static void
vte_sequence_handler_cl(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_clear_screen(terminal, NULL, 0, NULL);
	vte_sequence_handler_ho(terminal, NULL, 0, NULL);
}

/* Move the cursor to the given position. */
static void
vte_sequence_handler_cm(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GValue *row, *col;
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	/* We need at least two parameters. */
	if ((params != NULL) && (params->n_values >= 2)) {
		/* The first is the row, the second is the column. */
		row = g_value_array_get_nth(params, 0);
		col = g_value_array_get_nth(params, 1);
		if (G_VALUE_HOLDS_LONG(row) &&
		    G_VALUE_HOLDS_LONG(col)) {
			screen->cursor_current.row = g_value_get_long(row) +
						     screen->delta;
			screen->cursor_current.col = g_value_get_long(col);
		}
	}
}

/* Clear from the current line. */
static void
vte_sequence_handler_clear_current_line(VteTerminal *terminal,
					const char *match,
					GQuark match_quark,
					GValueArray *params)
{
	GArray *rowdata;
	long delta;
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	delta = screen->delta;
	if (screen->row_data->len > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = g_array_index(screen->row_data, GArray*,
					screen->cursor_current.row);
		/* Remove it. */
		while (rowdata->len > 0) {
			g_array_remove_index(rowdata, rowdata->len - 1);
		}
		/* Repaint this row. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     screen->cursor_current.row - delta, 1);
	}
}

/* Carriage return. */
static void
vte_sequence_handler_cr(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->cursor_current.col = 0;
}

/* Restrict scrolling and updates to a subset of the visible lines. */
static void
vte_sequence_handler_cs(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	long start, end, rows;
	GValue *value;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* We require two parameters. */
	if ((params == NULL) || (params->n_values < 2)) {
		terminal->screen->scrolling_restricted = FALSE;
		return;
	}
	/* Extract the two values. */
	value = g_value_array_get_nth(params, 0);
	start = g_value_get_long(value);
	value = g_value_array_get_nth(params, 1);
	end = g_value_get_long(value);
	/* Set the right values. */
	terminal->screen->scrolling_region.start = start;
	terminal->screen->scrolling_region.end = end;
	terminal->screen->scrolling_restricted = TRUE;
	/* Special case -- run wild, run free. */
	rows = terminal->row_count;
	if ((terminal->screen->scrolling_region.start == 0) &&
	    (terminal->screen->scrolling_region.end == rows - 1)) {
		terminal->screen->scrolling_restricted = FALSE;
	}
}

/* Move the cursor to the given row (vertical position). */
static void
vte_sequence_handler_cv(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct _VteScreen *screen;
	GValue *value;
	long delta;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	delta = screen->delta;
	/* Repaint the current cursor position. */
	vte_invalidate_cells(terminal,
			     screen->cursor_current.col, 1,
			     screen->cursor_current.row - delta, 1);
	/* We only care if there's a parameter in there. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			/* Move the cursor and repaint it. */
			screen->cursor_current.row = g_value_get_long(value);
			vte_invalidate_cells(terminal,
					     screen->cursor_current.col,
					     1,
					     screen->cursor_current.row - delta,
					     1);
		}
	}
}

/* Delete a line at the current cursor position. */
static void
vte_sequence_handler_dl(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct _VteScreen *screen;
	long end;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	if (screen->scrolling_restricted) {
		end = screen->delta + screen->scrolling_region.end;
	} else {
		end = screen->delta + terminal->row_count - 1;
	}
	vte_remove_line_int(terminal, screen->cursor_current.row);
	vte_insert_line_int(terminal, end);
	vte_invalidate_cells(terminal,
			     0, terminal->column_count,
			     0, terminal->row_count);
}

/* Delete N lines at the current cursor position. */
static void
vte_sequence_handler_DL(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_sequence_handler_multiple(terminal, match, match_quark, params,
				      vte_sequence_handler_dl);
}

/* Scroll forward. */
static void
vte_sequence_handler_do(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GtkWidget *widget;
	long delta, rows, col, row, start, end;
	struct _VteScreen *screen;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);
	screen = terminal->screen;

	delta = screen->delta;
	start = screen->scrolling_region.start + delta;
	end = screen->scrolling_region.end + delta;
	col = screen->cursor_current.col;
	row = screen->cursor_current.row;

	if (screen->scrolling_restricted) {
		if (row == end) {
			/* If we're at the end of the scrolling region, add a
			 * line at the bottom to scroll the top off. */
			vte_remove_line_int(terminal, start);
			vte_insert_line_int(terminal, end);
			/* Invalidate the rows the cursor was on and is on. */
			vte_invalidate_cells(terminal,
					     0, terminal->column_count,
					     start - delta, end - start + 1);
		} else {
			/* Otherwise, just move the cursor down. */
			screen->cursor_current.row++;
			/* Invalidate the rows the cursor was on and is on. */
			vte_invalidate_cells(terminal,
					     col, 1,
					     row - delta, 2);
		}
	} else {
		/* Move the cursor down. */
		screen->cursor_current.row++;

		/* Make sure that the bottom row is visible.  This usually
		 * causes the top row to become a history row. */
		rows = MAX(screen->row_data->len,
			   screen->cursor_current.row + 1);
		delta = MAX(0, rows - terminal->row_count);

		/* Invalidate the cells the cursor was on and is on. */
		vte_invalidate_cells(terminal,
				     col, 1,
				     row - delta, 2);

		/* Update scroll bar adjustments. */
		vte_terminal_adjust_adjustments(terminal);

		/* Keep the cursor on-screen. */
		if (floor(gtk_adjustment_get_value(terminal->adjustment)) != delta) {
			gtk_adjustment_set_value(terminal->adjustment, delta);
		}
	}
}

/* Cursor down. */
static void
vte_sequence_handler_DO(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, match, match_quark, params,
				      vte_sequence_handler_do);
}

/* End insert mode. */
static void
vte_sequence_handler_ei(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->insert = FALSE;
}

/* Move the cursor to the home position. */
static void
vte_sequence_handler_ho(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	screen->cursor_current.row = screen->delta;
	screen->cursor_current.col = 0;
}

/* Begin insert mode. */
static void
vte_sequence_handler_im(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->insert = TRUE;
}

/* Cursor left. */
static void
vte_sequence_handler_le(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	screen->cursor_current.col = MAX(0, screen->cursor_current.col - 1);
	vte_invalidate_cells(terminal,
			     screen->cursor_current.col, 2,
			     screen->cursor_current.row, 1);
}

/* Move the cursor left N columns. */
static void
vte_sequence_handler_LE(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, match, match_quark, params,
				      vte_sequence_handler_le);
}

/* Blink on. */
static void
vte_sequence_handler_mb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->defaults.blink = 1;
}

/* Bold on. */
static void
vte_sequence_handler_md(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->defaults.bold = 1;
}

/* End modes. */
static void
vte_sequence_handler_me(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->defaults.blink = 0;
	terminal->screen->defaults.half = 0;
	terminal->screen->defaults.invisible = 0;
	terminal->screen->defaults.reverse = 0;
	terminal->screen->defaults.underline = 0;
	terminal->screen->defaults.bold = 0;
	terminal->screen->defaults.standout = 0;
}

/* Invisible on. */
static void
vte_sequence_handler_mk(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->defaults.invisible = 1;
}

/* Reverse on. */
static void
vte_sequence_handler_mr(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->defaults.reverse = 1;
}

/* Cursor right. */
static void
vte_sequence_handler_nd(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	screen->cursor_current.col++;
}

/* Restore cursor (position). */
static void
vte_sequence_handler_rc(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	screen->cursor_current.col = screen->cursor_saved.col;
	screen->cursor_current.row = screen->cursor_saved.row + screen->delta;
}

/* Cursor right N characters. */
static void
vte_sequence_handler_RI(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, match, match_quark, params,
				      vte_sequence_handler_nd);
}

/* Save cursor (position). */
static void
vte_sequence_handler_sc(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	screen->cursor_saved.col = screen->cursor_current.col;
	screen->cursor_saved.row = screen->cursor_current.row - screen->delta;
}

/* Standout end. */
static void
vte_sequence_handler_se(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->defaults.standout = 0;
}

/* Standout start. */
static void
vte_sequence_handler_so(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->defaults.standout = 1;
}

/* Tab.  FIXME: implement custom tabstop setting and the whole nine yards. */
static void
vte_sequence_handler_ta(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	long newcol;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* Invalidate the cell the cursor is in. */
	vte_invalidate_cells(terminal,
			     terminal->screen->cursor_current.col, 1,
			     terminal->screen->cursor_current.row, 1);
	/* Calculate which column is the next tab stop. */
	newcol = terminal->screen->cursor_current.col;
	do {
		newcol++;
	} while ((newcol % VTE_TAB_WIDTH) != 0);
	/* Wrap to the next line if need be. */
	if (newcol >= terminal->column_count) {
		terminal->screen->cursor_current.col = 0;
		vte_sequence_handler_do(terminal, match, match_quark, params);
	} else {
		terminal->screen->cursor_current.col = newcol;
	}
	/* Invalidate the cell the cursor is in. */
	vte_invalidate_cells(terminal,
			     terminal->screen->cursor_current.col, 1,
			     terminal->screen->cursor_current.row, 1);
}

/* Underline end. */
static void
vte_sequence_handler_ue(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->defaults.underline = 0;
}

/* Cursor up, scrolling if need be. */
static void
vte_sequence_handler_up(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GtkWidget *widget;
	long delta, col, row, start, end;
	struct _VteScreen *screen;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);
	screen = terminal->screen;

	delta = screen->delta;
	col = screen->cursor_current.col;
	row = screen->cursor_current.row;

	if (screen->scrolling_restricted) {
		start = screen->scrolling_region.start + delta;
		end = screen->scrolling_region.end + delta;
		if (row == start) {
			/* If we're at the top of the scrolling region, add a
			 * line at the top to scroll the bottom off. */
			vte_remove_line_int(terminal, end);
			vte_insert_line_int(terminal, start);
			/* Invalidate the scrolling region. */
			vte_invalidate_cells(terminal,
					     0, terminal->column_count,
					     start - delta, end - start + 1);
		} else {
			/* Otherwise, just move the cursor up. */
			screen->cursor_current.row--;
			row = screen->cursor_current.row;
			/* Invalidate the cells the cursor was in and is in. */
			vte_invalidate_cells(terminal,
					     col, 1,
					     row - delta, 2);
		}
	} else {
		start = delta;
		end = delta + terminal->row_count - 1;
		if (row == start) {
			/* Insert a blank line and remove one from the bottom,
			 * to simulate a proper scroll without screwing up the
			 * history. */
			vte_remove_line_int(terminal, end);
			vte_insert_line_int(terminal, start);
			/* We need to redraw everything here. */
			vte_invalidate_cells(terminal,
					     0, terminal->column_count,
					     0, terminal->row_count);
		} else {
			/* Move the cursor up. */
			screen->cursor_current.row--;
			row = screen->cursor_current.row;
			/* Invalidate the places the cursor is and was. */
			vte_invalidate_cells(terminal,
					     col, 1,
					     row - delta, 2);
		}
	}
}

/* Cursor up. */
static void
vte_sequence_handler_UP(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, match, match_quark, params,
				      vte_sequence_handler_up);
}

/* Underline start. */
static void
vte_sequence_handler_us(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->defaults.underline = 1;
}

/* Cursor invisible. */
static void
vte_sequence_handler_vi(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->cursor_visible = FALSE;
}

/* Cursor standout. */
static void
vte_sequence_handler_vs(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->screen->cursor_visible = TRUE;
}

/* Handle ANSI color setting and related stuffs (SGR). */
static void
vte_sequence_handler_character_attributes(VteTerminal *terminal,
					  const char *match,
					  GQuark match_quark,
					  GValueArray *params)
{
	unsigned int i;
	GValue *value;
	long param;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* The default parameter is zero. */
	param = 0;
	/* Step through each numeric parameter. */
	for (i = 0; (params != NULL) && (i < params->n_values); i++) {
		/* If this parameter isn't a number, skip it. */
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		param = g_value_get_long(value);
		switch (param) {
			case 0:
				vte_terminal_set_default_attributes(terminal);
				break;
			case 1:
				terminal->screen->defaults.bold = 1;
				break;
			case 4:
				terminal->screen->defaults.underline = 1;
				break;
			case 5:
				terminal->screen->defaults.blink = 1;
				break;
			case 7:
				terminal->screen->defaults.reverse = 1;
				break;
			case 8:
				terminal->screen->defaults.invisible = 1;
				break;
			case 21: /* one of these is the linux console */
			case 22: /* one of these is ecma, i forget which */
				terminal->screen->defaults.bold = 0;
				break;
			case 24:
				terminal->screen->defaults.underline = 0;
				break;
			case 25:
				terminal->screen->defaults.blink = 0;
				break;
			case 27:
				terminal->screen->defaults.reverse = 0;
				break;
			case 28:
				terminal->screen->defaults.invisible = 0;
				break;
			case 30:
			case 31:
			case 32:
			case 33:
			case 34:
			case 35:
			case 36:
			case 37:
				terminal->screen->defaults.fore = param - 30;
				break;
			case 38:
				/* default foreground, underscore */
				terminal->screen->defaults.fore = 7;
				terminal->screen->defaults.underline = 1;
				break;
			case 39:
				/* default foreground, no underscore */
				terminal->screen->defaults.fore = 7;
				terminal->screen->defaults.underline = 0;
				break;
			case 40:
			case 41:
			case 42:
			case 43:
			case 44:
			case 45:
			case 46:
			case 47:
				terminal->screen->defaults.back = param - 40;
				break;
			case 49:
				/* default background */
				terminal->screen->defaults.back = 0;
				break;
			case 90:
			case 91:
			case 92:
			case 93:
			case 94:
			case 95:
			case 96:
			case 97:
				terminal->screen->defaults.fore = param - 90;
				break;
			case 100:
			case 101:
			case 102:
			case 103:
			case 104:
			case 105:
			case 106:
			case 107:
				terminal->screen->defaults.back = param - 100;
				break;
		}
	}
	/* If we had no parameters, default to the defaults. */
	if (i == 0) {
		vte_terminal_set_default_attributes(terminal);
	}
}

/* Clear above the current line. */
static void
vte_sequence_handler_clear_above_current(VteTerminal *terminal,
					 const char *match,
					 GQuark match_quark,
					 GValueArray *params)
{
	GArray *rowdata;
	long delta, i;
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	delta = screen->delta;
	for (i = delta; i < screen->cursor_current.row; i++) {
		if (screen->row_data->len > i) {
			/* Get the data for the row we're erasing. */
			rowdata = g_array_index(screen->row_data, GArray*, i);
			/* Remove it. */
			while (rowdata->len > 0) {
				g_array_remove_index(rowdata, rowdata->len - 1);
			}
			/* Repaint this row. */
			vte_invalidate_cells(terminal,
					     0, terminal->column_count,
					     i - delta, 1);
		}
	}
}

/* Clear the entire screen. */
static void
vte_sequence_handler_clear_screen(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	GArray *rowdata;
	long delta, i;
	struct _VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	delta = screen->delta;
	for (i = delta; i < delta + terminal->row_count; i++) {
		if (screen->row_data->len > i) {
			/* Get the data for the row we're removing. */
			rowdata = g_array_index(screen->row_data, GArray*, i);
			/* Remove it. */
			while (rowdata->len > 0) {
				g_array_remove_index(rowdata, rowdata->len - 1);
			}
			/* Repaint this row. */
			vte_invalidate_cells(terminal,
					     0, terminal->column_count,
					     i - delta, 1);
		}
	}
}

/* Move the cursor to the given position, 1-based. */
static void
vte_sequence_handler_cursor_position(VteTerminal *terminal,
				     const char *match,
				     GQuark match_quark,
				     GValueArray *params)
{
	vte_sequence_handler_offset(terminal, match, match_quark, params,
				    -1, vte_sequence_handler_cm);
}

/* Set icon/window titles. */
static void
vte_sequence_handler_set_icon_title(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params)
{
	GValue *value;
	iconv_t conv;
	char buf[LINE_MAX];
	char *inbuf, *outbuf;
	size_t inbuf_len, outbuf_len;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* Get the string parameter's value. */
	value = g_value_array_get_nth(params, 0);
	if (value) {
		if (G_VALUE_HOLDS_LONG(value)) {
			/* Convert the long to a string. */
			snprintf(buf, sizeof(buf), "%ld",
				 g_value_get_long(value));
		} else
		if (G_VALUE_HOLDS_STRING(value)) {
			/* Copy the string into the buffer. */
			snprintf(buf, sizeof(buf), "%s",
				 g_value_get_string(value));
		} else
		if (G_VALUE_HOLDS_POINTER(value)) {
			/* Convert the wide-character string into a 
			 * multibyte string. */
			conv = iconv_open("UTF-8", "WCHAR_T");
			inbuf = g_value_get_pointer(value);
			inbuf_len = wcslen((wchar_t*)inbuf) * sizeof(wchar_t);
			memset(buf, 0, sizeof(buf));
			outbuf = buf;
			outbuf_len = sizeof(buf) - 1;
			if (iconv(conv, &inbuf, &inbuf_len,
				  &outbuf, &outbuf_len) == -1) {
				memset(buf, 0, sizeof(buf));
			}
		} else {
			return;
		}
		/* Emit the signal, passing the string. */
		g_signal_emit_by_name(terminal, "set_icon_title", buf);
	}
}
static void
vte_sequence_handler_set_window_title(VteTerminal *terminal,
				      const char *match,
				      GQuark match_quark,
				      GValueArray *params)
{
	GValue *value;
	iconv_t conv;
	char buf[LINE_MAX];
	char *inbuf, *outbuf;
	size_t inbuf_len, outbuf_len;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* Get the string parameter's value. */
	value = g_value_array_get_nth(params, 0);
	if (value) {
		if (G_VALUE_HOLDS_LONG(value)) {
			/* Convert the long to a string. */
			snprintf(buf, sizeof(buf), "%ld",
				 g_value_get_long(value));
		} else
		if (G_VALUE_HOLDS_STRING(value)) {
			/* Copy the string into the buffer. */
			snprintf(buf, sizeof(buf), "%s",
				 g_value_get_string(value));
		} else
		if (G_VALUE_HOLDS_POINTER(value)) {
			/* Convert the wide-character string into a 
			 * multibyte string. */
			conv = iconv_open("UTF-8", "WCHAR_T");
			inbuf = g_value_get_pointer(value);
			inbuf_len = wcslen((wchar_t*)inbuf) * sizeof(wchar_t);
			memset(buf, 0, sizeof(buf));
			outbuf = buf;
			outbuf_len = sizeof(buf) - 1;
			if (iconv(conv, &inbuf, &inbuf_len,
				  &outbuf, &outbuf_len) == -1) {
				memset(buf, 0, sizeof(buf));
			}
			iconv_close(conv);
		} else {
			return;
		}
		/* Emit the signal, passing the string. */
		g_signal_emit_by_name(terminal, "set_window_title", buf);
	}
}

/* Set both the window and icon titles to the same string. */
static void
vte_sequence_handler_set_icon_and_window_title(VteTerminal *terminal,
						  const char *match,
						  GQuark match_quark,
						  GValueArray *params)
{
	vte_sequence_handler_set_icon_title(terminal, match,
					    match_quark, params);
	vte_sequence_handler_set_window_title(terminal, match,
					      match_quark, params);
}

/* Restrict the scrolling region. */
static void
vte_sequence_handler_set_scrolling_region(VteTerminal *terminal,
					  const char *match,
					  GQuark match_quark,
					  GValueArray *params)
{
	vte_sequence_handler_offset(terminal, match, match_quark, params,
				    -1, vte_sequence_handler_cs);
}

/* Manipulate certain terminal attributes. */
static void
vte_sequence_handler_decset_internal(VteTerminal *terminal,
				     const char *match,
				     GQuark match_quark,
				     GValueArray *params,
				     gboolean set)
{
	GValue *value;
	long param;
	int i;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		param = g_value_get_long(value);
		switch (param) {
			case 1:
				/* Set the application keypad. */
				terminal->keypad = set ?
						   VTE_KEYPAD_NORMAL :
						   VTE_KEYPAD_APPLICATION;
				break;
			case 2:
				/* FIXME: reset alternate character sets to
				 * ASCII. */
				break;
			case 3:
				/* FIXME: set 132 (reset to 80) column mode. */
				break;
			case 4:
				/* FIXME: set or unset smooth-scrolling. */
				break;
			case 5:
				/* normal or reverse video. */
				terminal->screen->defaults.reverse = set;
				break;
			case 6:
				/* FIXME: origin or normal cursor mode. */
				break;
			case 7:
				/* FIXME: set or unset wraparound mode. */
				break;
			case 8:
				/* FIXME: set or unset autorepeat keys. */
				break;
			case 9:
				/* FIXME: send mouse X and Y on button. */
				break;
			case 38:
				/* FIXME: Tektronix/Xterm mode. */
				break;
			case 40:
				/* FIXME: Allow/disallow 80/132 column mode. */
				break;
			case 41:
				/* FIXME: more(1) fix. */
				break;
			case 44:
				/* FIXME: set/unset margin bell. */
				break;
			case 45:
				/* FIXME: set/unset reverse-wraparound mode. */
				break;
			case 46:
				/* FIXME(?): enable/disable logging. */
				break;
			case 47:
				/* Set or restore alternate screen. */
				terminal->screen = set ?
						   &terminal->alternate_screen :
						   &terminal->normal_screen;
				/* Force the screen to be redrawn. */
				vte_invalidate_cells(terminal,
						     0, terminal->column_count,
						     0, terminal->row_count);
				/* Fixup the scrollbars. */
				vte_terminal_adjust_adjustments(terminal);
				break;
			case 1000:
				/* FIXME: send mouse X and Y on press and
				 * release. */
				break;
			case 1001:
				/* FIXME: use (or not) hilite mouse tracking. */
				break;
			default:
				break;
		}
	}
}

/* Set the application or normal keypad. */
static void
vte_sequence_handler_application_keypad(VteTerminal *terminal,
					const char *match,
					GQuark match_quark,
					GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->keypad = VTE_KEYPAD_APPLICATION;
}

static void
vte_sequence_handler_normal_keypad(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->keypad = VTE_KEYPAD_NORMAL;
}

/* Move the cursor. */
static void
vte_sequence_handler_character_position_absolute(VteTerminal *terminal,
						 const char *match,
						 GQuark match_quark,
						 GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_sequence_handler_offset(terminal, match, match_quark, params,
				    -1, vte_sequence_handler_ch);
}
static void
vte_sequence_handler_line_position_absolute(VteTerminal *terminal,
					    const char *match,
					    GQuark match_quark,
					    GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_sequence_handler_offset(terminal, match, match_quark, params,
				    -1, vte_sequence_handler_cv);
}

/* Set certain terminal attributes. */
static void
vte_sequence_handler_decset(VteTerminal *terminal,
			    const char *match,
			    GQuark match_quark,
			    GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_sequence_handler_decset_internal(terminal, match, match_quark,
					     params, TRUE);
}

/* Unset certain terminal attributes. */
static void
vte_sequence_handler_decreset(VteTerminal *terminal,
			      const char *match,
			      GQuark match_quark,
			      GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_sequence_handler_decset_internal(terminal, match, match_quark,
					     params, FALSE);
}

/* Erase certain lines in the display. */
static void
vte_sequence_handler_erase_in_display(VteTerminal *terminal,
				      const char *match,
				      GQuark match_quark,
				      GValueArray *params)
{
	GValue *value;
	long param;
	int i;
	/* The default parameter is 0. */
	param = 0;
	/* Pull out a parameter. */
	for (i = 0; (params != NULL) && (i < params->n_values); i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		param = g_value_get_long(value);
	}
	/* Clear the right area. */
	switch (param) {
		case 0:
			/* Clear below the current line. */
			vte_sequence_handler_cd(terminal, NULL, 0, NULL);
			break;
		case 1:
			/* Clear above the current line. */
			vte_sequence_handler_clear_above_current(terminal,
								 NULL,
								 0,
								 NULL);
			break;
		case 2:
			/* Clear the entire screen. */
			vte_sequence_handler_clear_screen(terminal,
							  NULL,
							  0,
							  NULL);
			break;
		default:
			break;
	}
}

/* Erase certain parts of the current line in the display. */
static void
vte_sequence_handler_erase_in_line(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	GValue *value;
	long param;
	int i;
	/* The default parameter is 0. */
	param = 0;
	/* Pull out a parameter. */
	for (i = 0; (params != NULL) && (i < params->n_values); i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		param = g_value_get_long(value);
	}
	/* Clear the right area. */
	switch (param) {
		case 0:
			/* Clear to end of the line. */
			vte_sequence_handler_ce(terminal, NULL, 0, NULL);
			break;
		case 1:
			/* Clear to start of the line. */
			vte_sequence_handler_cb(terminal, NULL, 0, NULL);
			break;
		case 2:
			/* Clear the entire line. */
			vte_sequence_handler_clear_current_line(terminal,
								NULL, 0, NULL);
			break;
		default:
			break;
	}
}

/* Insert a certain number of lines below the current cursor. */
static void
vte_sequence_handler_insert_lines(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	GValue *value;
	struct _VteScreen *screen;
	long param, delta, end, row;
	int i;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	/* The default is one. */
	param = 1;
	/* Extract any parameters. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		param = g_value_get_long(value);
	}
	delta = screen->delta;
	row = screen->cursor_current.row;
	end = screen->scrolling_region.end + screen->delta;
	/* Insert the new lines at the cursor. */
	for (i = 0; i < param; i++) {
		/* Clear lines off the bottom of the scrolling region. */
		if (screen->scrolling_restricted) {
			/* Clear a line off the end of the region. */
			vte_remove_line_int(terminal, end);
		}
		vte_insert_line_int(terminal, row);
	}
	/* Refresh the modified area. */
	vte_invalidate_cells(terminal,
			     0, terminal->column_count,
			     row - delta, end - row + 1);
}

/* Delete certain lines from the scrolling region. */
static void
vte_sequence_handler_delete_lines(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	GValue *value;
	struct _VteScreen *screen;
	long param, delta, end, row;
	int i;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->screen;
	/* The default is one. */
	param = 1;
	/* Extract any parameters. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		param = g_value_get_long(value);
	}
	/* Clear the lines which we need to clear. */
	delta = screen->delta;
	row = screen->cursor_current.row;
	end = screen->delta + screen->scrolling_region.end;
	/* Clear them from below the current cursor. */
	for (i = 0; i < param; i++) {
		/* Insert any new empty lines. */
		if (screen->scrolling_restricted) {
			vte_insert_line_int(terminal, end);
		}
		/* Remove the line at the top of the area. */
		vte_remove_line_int(terminal, row);
	}
	/* Refresh the modified area. */
	vte_invalidate_cells(terminal,
			     0, terminal->column_count,
			     row - delta, end - row + 1);
}

/* Index.  Move the cursor down a row, and if it's in a scrolling region,
 * scroll to keep it on the screen. */
static void
vte_sequence_handler_index(VteTerminal *terminal,
			   const char *match,
			   GQuark match_quark,
			   GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_sequence_handler_DO(terminal, match, match_quark, params);
}

/* Reverse index.  Move the cursor up a row, and if it's in a scrolling
 * region, scroll to keep it on the screen. */
static void
vte_sequence_handler_reverse_index(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_sequence_handler_UP(terminal, match, match_quark, params);
}

/* Set the terminal encoding. */
static void
vte_sequence_handler_iso8859_1(VteTerminal *terminal,
			       const char *match,
			       GQuark match_quark,
			       GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_terminal_set_encoding(terminal, "ISO-8859-1");
}

static void
vte_sequence_handler_utf_8(VteTerminal *terminal,
			   const char *match,
			   GQuark match_quark,
			   GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_terminal_set_encoding(terminal, "UTF-8");
}

/* The table of handlers.  Primarily used at initialization time. */
static struct {
	const char *code;
	VteTerminalSequenceHandler handler;
} vte_sequence_handlers[] = {
	{"!1", NULL},
	{"!2", NULL},
	{"!3", NULL},

	{"#1", NULL},
	{"#2", NULL},
	{"#3", NULL},
	{"#4", NULL},

	{"%1", NULL},
	{"%2", NULL},
	{"%3", NULL},
	{"%4", NULL},
	{"%5", NULL},
	{"%6", NULL},
	{"%7", NULL},
	{"%8", NULL},
	{"%9", NULL},
	{"%a", NULL},
	{"%b", NULL},
	{"%c", NULL},
	{"%d", NULL},
	{"%e", NULL},
	{"%f", NULL},
	{"%g", NULL},
	{"%h", NULL},
	{"%i", NULL},
	{"%j", NULL},

	{"&0", NULL},
	{"&1", NULL},
	{"&2", NULL},
	{"&3", NULL},
	{"&4", NULL},
	{"&5", NULL},
	{"&6", NULL},
	{"&7", NULL},
	{"&8", NULL},
	{"&9", NULL},

	{"*0", NULL},
	{"*1", NULL},
	{"*2", NULL},
	{"*3", NULL},
	{"*4", NULL},
	{"*5", NULL},
	{"*6", NULL},
	{"*7", NULL},
	{"*8", NULL},
	{"*9", NULL},

	{"@0", NULL},
	{"@1", NULL},
	{"@2", NULL},
	{"@3", NULL},
	{"@4", NULL},
	{"@5", NULL},
	{"@6", NULL},
	{"@7", NULL},
	{"@8", NULL},
	{"@9", NULL},

	{"al", vte_sequence_handler_al},
	{"AL", vte_sequence_handler_AL},
	{"ac", NULL},
	{"ae", NULL},
	{"as", NULL},

	{"bc", NULL},
	{"bl", vte_sequence_handler_bl},
	{"bt", NULL},

	{"cb", vte_sequence_handler_cb},
	{"cc", NULL},
	{"cd", vte_sequence_handler_cd},
	{"ce", vte_sequence_handler_ce},
	{"ch", vte_sequence_handler_ch},
	{"cl", vte_sequence_handler_cl},
	{"cm", vte_sequence_handler_cm},
	{"cr", vte_sequence_handler_cr},
	{"cs", vte_sequence_handler_cs},
	{"ct", NULL},
	{"cv", vte_sequence_handler_cv},

	{"dc", NULL},
	{"DC", NULL},
	{"dl", vte_sequence_handler_dl},
	{"DL", vte_sequence_handler_DL},
	{"dm", NULL},
	{"do", vte_sequence_handler_do},
	{"DO", vte_sequence_handler_DO},
	{"ds", NULL},

	{"eA", NULL},
	{"ec", NULL},
	{"ed", NULL},
	{"ei", vte_sequence_handler_ei},

	{"ff", NULL},
	{"fs", NULL},
	{"F1", NULL},
	{"F2", NULL},
	{"F3", NULL},
	{"F4", NULL},
	{"F5", NULL},
	{"F6", NULL},
	{"F7", NULL},
	{"F8", NULL},
	{"F9", NULL},
	{"FA", NULL},
	{"FB", NULL},
	{"FC", NULL},
	{"FD", NULL},
	{"FE", NULL},
	{"FF", NULL},
	{"FG", NULL},
	{"FH", NULL},
	{"FI", NULL},
	{"FJ", NULL},
	{"FK", NULL},
	{"FL", NULL},
	{"FM", NULL},
	{"FN", NULL},
	{"FO", NULL},
	{"FP", NULL},
	{"FQ", NULL},
	{"FR", NULL},
	{"FS", NULL},
	{"FT", NULL},
	{"FU", NULL},
	{"FV", NULL},
	{"FW", NULL},
	{"FX", NULL},
	{"FY", NULL},
	{"FZ", NULL},

	{"Fa", NULL},
	{"Fb", NULL},
	{"Fc", NULL},
	{"Fd", NULL},
	{"Fe", NULL},
	{"Ff", NULL},
	{"Fg", NULL},
	{"Fh", NULL},
	{"Fi", NULL},
	{"Fj", NULL},
	{"Fk", NULL},
	{"Fl", NULL},
	{"Fm", NULL},
	{"Fn", NULL},
	{"Fo", NULL},
	{"Fp", NULL},
	{"Fq", NULL},
	{"Fr", NULL},

	{"hd", NULL},
	{"ho", vte_sequence_handler_ho},
	{"hu", NULL},

	{"i1", NULL},
	{"i3", NULL},

	{"is", NULL},
	{"ic", NULL},
	{"IC", NULL},
	{"if", NULL},
	{"im", vte_sequence_handler_im},
	{"ip", NULL},
	{"iP", NULL},

	{"K1", NULL},
	{"K2", NULL},
	{"K3", NULL},
	{"K4", NULL},
	{"K5", NULL},

	{"k0", NULL},
	{"k1", NULL},
	{"k2", NULL},
	{"k3", NULL},
	{"k4", NULL},
	{"k5", NULL},
	{"k6", NULL},
	{"k7", NULL},
	{"k8", NULL},
	{"k9", NULL},
	{"k;", NULL},
	{"ka", NULL},
	{"kA", NULL},
	{"kb", NULL},
	{"kB", NULL},
	{"kC", NULL},
	{"kd", NULL},
	{"kD", NULL},
	{"ke", NULL},
	{"kE", NULL},
	{"kF", NULL},
	{"kh", NULL},
	{"kH", NULL},
	{"kI", NULL},
	{"kl", NULL},
	{"kL", NULL},
	{"kM", NULL},
	{"kN", NULL},
	{"kP", NULL},
	{"kr", NULL},
	{"kR", NULL},
	{"ks", NULL},
	{"kS", NULL},
	{"kt", NULL},
	{"kT", NULL},
	{"ku", NULL},

	{"l0", NULL},
	{"l1", NULL},
	{"l2", NULL},
	{"l3", NULL},
	{"l4", NULL},
	{"l5", NULL},
	{"l6", NULL},
	{"l7", NULL},
	{"l8", NULL},
	{"l9", NULL},

	{"la", NULL},
	{"le", vte_sequence_handler_le},
	{"LE", vte_sequence_handler_LE},
	{"LF", NULL},
	{"ll", NULL},
	{"LO", NULL},

	{"mb", vte_sequence_handler_mb},
	{"MC", NULL},
	{"md", vte_sequence_handler_md},
	{"me", vte_sequence_handler_me},
	{"mh", NULL},
	{"mk", vte_sequence_handler_mk},
	{"ML", NULL},
	{"mm", NULL},
	{"mo", NULL},
	{"mp", NULL},
	{"mr", vte_sequence_handler_mr},
	{"MR", NULL},

	{"nd", NULL},
	{"nw", NULL},

	{"pc", NULL},
	{"pf", NULL},
	{"pk", NULL},
	{"pl", NULL},
	{"pn", NULL},
	{"po", NULL},
	{"pO", NULL},
	{"ps", NULL},
	{"px", NULL},

	{"r1", NULL},
	{"r2", NULL},
	{"r3", NULL},

	{"..rp", NULL},
	{"RA", NULL},
	{"rc", vte_sequence_handler_rc},
	{"rf", NULL},
	{"RF", NULL},
	{"RI", vte_sequence_handler_RI},
	{"rp", NULL},
	{"rP", NULL},
	{"rs", NULL},
	{"RX", NULL},

	{"s0", NULL},
	{"s1", NULL},
	{"s2", NULL},
	{"s3", NULL},

	{"..sa", NULL},
	{"sa", NULL},
	{"SA", NULL},
	{"sc", vte_sequence_handler_sc},
	{"se", vte_sequence_handler_se},
	{"sf", vte_sequence_handler_do},
	{"SF", vte_sequence_handler_DO},
	{"so", vte_sequence_handler_so},
	{"sr", vte_sequence_handler_up},
	{"SR", vte_sequence_handler_UP},
	{"st", NULL},
	{"SX", NULL},

	{"ta", vte_sequence_handler_ta},
	{"te", NULL},
	{"ti", NULL},
	{"ts", NULL},

	{"uc", NULL},
	{"ue", vte_sequence_handler_ue},
	{"up", vte_sequence_handler_up},
	{"UP", vte_sequence_handler_UP},
	{"us", vte_sequence_handler_us},

	{"vb", NULL},
	{"ve", NULL},
	{"vi", vte_sequence_handler_vi},
	{"vs", vte_sequence_handler_vs},

	{"wi", NULL},

	{"XF", NULL},

	{"character-attributes", vte_sequence_handler_character_attributes},

	{"cursor-backward", vte_sequence_handler_le},
	{"cursor-forward", vte_sequence_handler_RI},
	{"cursor-up", vte_sequence_handler_UP},
	{"cursor-down", vte_sequence_handler_DO},
	{"cursor-position", vte_sequence_handler_cursor_position},

	{"set-icon-title",
	 vte_sequence_handler_set_icon_title},
	{"set-window-title",
	 vte_sequence_handler_set_window_title},
	{"set-icon-and-window-title",
	 vte_sequence_handler_set_icon_and_window_title},

	{"application-keypad", vte_sequence_handler_application_keypad},
	{"normal-keypad", vte_sequence_handler_normal_keypad},
	{"decset", vte_sequence_handler_decset},
	{"decreset", vte_sequence_handler_decreset},
	{"save-cursor", vte_sequence_handler_sc},
	{"restore-cursor", vte_sequence_handler_rc},
	{"normal-keypad", vte_sequence_handler_normal_keypad},
	{"application-keypad", vte_sequence_handler_application_keypad},
	{"erase-in-display", vte_sequence_handler_erase_in_display},
	{"erase-in-line", vte_sequence_handler_erase_in_line},
	{"set-scrolling-region", vte_sequence_handler_set_scrolling_region},
	{"insert-lines", vte_sequence_handler_insert_lines},
	{"delete-lines", vte_sequence_handler_delete_lines},
	{"index", vte_sequence_handler_index},
	{"reverse-index", vte_sequence_handler_reverse_index},
        {"iso8859-1-character-set", vte_sequence_handler_iso8859_1},
	{"utf-8-character-set", vte_sequence_handler_utf_8},
        {"character-position-absolute", vte_sequence_handler_character_position_absolute},
        {"line-position-absolute", vte_sequence_handler_line_position_absolute},
};

/* Create the basic widget.  This more or less creates and initializes a
 * GtkWidget and clears out the rest of the data which is specific to our
 * widget class. */
GtkWidget *
vte_terminal_new(void)
{
	return GTK_WIDGET(g_object_new(vte_terminal_get_type(), NULL));
}

/* Reset palette defaults for character colors. */
static void
vte_terminal_set_default_palette(VteTerminal *terminal)
{
	int i;
	XColor color;
	GtkWidget *widget;
	Display *display;
	GdkColormap *gcolormap;
	Colormap colormap;
	int bright, red, green, blue;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->palette_initialized) {
		return;
	}
	memset(&color, 0, sizeof(color));

	widget = NULL;
	display = NULL;
	gcolormap = NULL;
	colormap = 0;

	/* Initialize each item in the palette. */
	for (i = 0; i < G_N_ELEMENTS(terminal->palette); i++) {
		/* Get X11 attributes used by GDK for the widget. */
		if (widget == NULL) {
			widget = GTK_WIDGET(terminal);
			display = GDK_DISPLAY();
			gcolormap = gtk_widget_get_colormap(widget);
			colormap = gdk_x11_colormap_get_xcolormap(gcolormap);
		}

		/* Make the difference between normal and bright about three
		 * fourths of the total available brightness. */
		bright = (i & 8) ? 0x3fff : 0;
		blue = (i & 4) ? 0xc000 : 0;
		green = (i & 2) ? 0xc000 : 0;
		red = (i & 1) ? 0xc000 : 0;

		/* Allocate a color from the colormap. */
		color.pixel = i;
		color.red = bright + red;
		color.green = bright + green;
		color.blue = bright + blue;

		if (XAllocColor(display, colormap, &color)) {
			terminal->palette[i].red = color.red;
			terminal->palette[i].green = color.green;
			terminal->palette[i].blue = color.blue;
			terminal->palette[i].pixel = color.pixel;
		}
	}
	terminal->palette_initialized = TRUE;
}

/* Insert a single character into the stored data array. */
static void
vte_terminal_insert_char(GtkWidget *widget, wchar_t c)
{
	VteTerminal *terminal;
	GArray *array;
	struct vte_charcell cell, *pcell;
	int columns, i;
	long delta, col;
	struct _VteScreen *screen;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);
	screen = terminal->screen;

	/* Make sure we have enough rows to hold this data. */
	while (screen->cursor_current.row >= screen->row_data->len) {
		array = vte_new_row_data();
		g_array_append_val(screen->row_data, array);
	}

	/* Get a handle on the array for the insertion row. */
	array = g_array_index(screen->row_data,
			      GArray*,
			      screen->cursor_current.row);

	/* Figure out how many columns this character should occupy. */
	columns = wcwidth(c);

	/* Read the deltas. */
	delta = floor(gtk_adjustment_get_value(terminal->adjustment));

	for (i = 0; i < columns; i++) {
		col = terminal->screen->cursor_current.col;

		/* Make sure we have enough columns in this row. */
		if (array->len <= col) {
			/* Add enough characters. */
			memset(&cell, 0, sizeof(cell));
			cell.c = ' ';
			cell.columns = 1;
			cell.fore = 7;
			cell.back = 0;
			while (array->len < col) {
				g_array_append_val(array, cell);
			}
			/* Add one more cell to the end of the line to get
			 * it into the column, and use it. */
			g_array_append_val(array, cell);
			pcell = &g_array_index(array,
					       struct vte_charcell,
					       col);
		} else {
			/* If we're in insert mode, insert a new cell here
			 * and use it. */
			if (screen->insert) {
				memset(&cell, 0, sizeof(cell));
				cell.c = ' ';
				cell.columns = 1;
				cell.fore = 7;
				cell.back = 0;
				g_array_insert_val(array, col, cell);
				pcell = &g_array_index(array,
						       struct vte_charcell,
						       col);
			} else {
				/* We're in overtype mode, so use the existing
				 * character. */
				pcell = &g_array_index(array,
						       struct vte_charcell,
						       col);
			}
		}

		/* Initialize the character cell with the proper data. */
		pcell->c = c;
		pcell->columns = (i == 0) ? columns : 0;
		pcell->fore = terminal->screen->defaults.fore;
		pcell->back = terminal->screen->defaults.back;
		pcell->reverse = terminal->screen->defaults.reverse;
		pcell->invisible = terminal->screen->defaults.invisible;
		pcell->half = terminal->screen->defaults.half;
		pcell->underline = terminal->screen->defaults.underline;
		pcell->bold = terminal->screen->defaults.bold;
		pcell->standout = terminal->screen->defaults.standout;

		/* Signal that this part of the window needs drawing. */
		if (terminal->screen->insert) {
			vte_invalidate_cells(terminal,
					     screen->cursor_current.col - 1,
					     terminal->column_count - screen->cursor_current.col + 1,
					     screen->cursor_current.row - delta,
					     2);
		} else {
			vte_invalidate_cells(terminal,
					     screen->cursor_current.col - 1,
					     3,
					     screen->cursor_current.row - delta,
					     2);
		}

		/* And take a step to the to the right.  We invalidated this
		 * part of the screen already, so no need to do it again. */
		screen->cursor_current.col++;
	}
}

static void
display_control_sequence(const char *name, GValueArray *params)
{
	/* Display the control sequence with its parameters, to
	 * help me debug this thing.  I don't have all of the
	 * sequences implemented yet. */
	int i;
	long l;
	const char *s;
	const wchar_t *w;
	GValue *value;
	fprintf(stderr, "%s(", name);
	if (params != NULL) {
		for (i = 0; i < params->n_values; i++) {
			value = g_value_array_get_nth(params, i);
			if (i > 0) {
				fprintf(stderr, ", ");
			}
			if (G_VALUE_HOLDS_LONG(value)) {
				l = g_value_get_long(value);
				fprintf(stderr, "%ld", l);
			} else
			if (G_VALUE_HOLDS_STRING(value)) {
				s = g_value_get_string(value);
				fprintf(stderr, "\"%s\"", s);
			} else
			if (G_VALUE_HOLDS_POINTER(value)) {
				w = g_value_get_pointer(value);
				fprintf(stderr, "\"%ls\"", w);
			}
		}
	}
	fprintf(stderr, ")\n");
}

/* Handle a terminal control sequence and its parameters. */
static void
vte_terminal_handle_sequence(GtkWidget *widget,
			     const char *match_s,
			     GQuark match,
			     GValueArray *params)
{
	VteTerminal *terminal;
	VteTerminalSequenceHandler handler;
	long delta;
	struct _VteScreen *screen;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);
	screen = terminal->screen;

	/* Read the deltas. */
	delta = floor(gtk_adjustment_get_value(terminal->adjustment));

	/* Signal that the cursor's current position needs redrawing. */
	vte_invalidate_cells(terminal,
			     screen->cursor_current.col - 1, 3,
			     screen->cursor_current.row - delta, 1);

	/* Find the handler for this control sequence. */
	handler = g_tree_lookup(terminal->sequences, GINT_TO_POINTER(match));
#ifdef VTE_DEBUG
	display_control_sequence(match_s, params);
#endif
	if (handler != NULL) {
		/* Let the handler handle it. */
		handler(terminal, match_s, match, params);
	} else {
		g_warning("No handler for control sequence `%s' defined.\n",
			  match_s);
	}

	/* We probably need to update the cursor's new position, too. */
	vte_invalidate_cells(terminal,
			     screen->cursor_current.col - 1, 3,
			     screen->cursor_current.row - delta, 1);
}

/* Handle an EOF from the client. */
static void
vte_terminal_eof(gint source, gpointer data)
{
	VteTerminal *terminal;

	g_return_if_fail(VTE_IS_TERMINAL(data));
	terminal = VTE_TERMINAL(data);

	/* Stop reading input. */
	gtk_input_remove(source);
	terminal->pty_input = -1;

	/* Emit a signal that we read an EOF. */
	g_signal_emit_by_name(terminal, "eof");
}

/* Read and handle data from the child. */
static gboolean
vte_terminal_io_read(GIOChannel *channel,
		     GdkInputCondition condition,
		     gpointer data)
{
	GValueArray *params;
	VteTerminal *terminal;
	GtkWidget *widget;
	char *buf;
	size_t bufsize;
	char *inbuf, *outbuf;
	size_t inbuf_len, outbuf_len;
	wchar_t wbuf[LINE_MAX], c;
	int i, j, wcount, bcount, fd;
	const char *match;
	GQuark quark;
	gboolean leave_open = TRUE;

	widget = GTK_WIDGET(data);
	terminal = VTE_TERMINAL(data);

	/* Allocate a buffer to hold both existing data and new data. */
	bufsize = terminal->n_narrow_pending + LINE_MAX;
	buf = g_malloc0(bufsize);
	if (terminal->n_narrow_pending > 0) {
		memcpy(buf, terminal->narrow_pending,
		       terminal->n_narrow_pending);
		g_free(terminal->narrow_pending);
		terminal->n_narrow_pending = 0;
	}

	/* Read some more data in. */
	fd = g_io_channel_unix_get_fd(channel);
	bcount = read(fd, buf + terminal->n_narrow_pending,
		      bufsize - terminal->n_narrow_pending);

	/* Convert any read bytes into wide characters.  FIXME: handle
	 * cases where the encoding changes mid-stream. */
	if (bcount > 0) {
		inbuf = buf;
		inbuf_len = terminal->n_narrow_pending + bcount;
		outbuf = (char*)wbuf;
		outbuf_len = sizeof(wbuf);
		if (iconv(terminal->pending_conv,
			  &inbuf, &inbuf_len,
			  &outbuf, &outbuf_len) != -1) {
			/* Save the resulting bytes as the narrow pending data
			 * queue. */
			terminal->n_narrow_pending = inbuf_len;
			terminal->narrow_pending = g_malloc(inbuf_len);
			memcpy(terminal->narrow_pending, inbuf, inbuf_len);
			wcount = (outbuf - (char*)wbuf) / sizeof(wchar_t);
		} else {
			wcount = 0;
		}
	} else {
		wcount = 0;
	}

	/* Add the read wchars to the pending array one at a time, then try
	 * to handle the entire array. */
	terminal->pending = g_realloc(terminal->pending,
				      (terminal->n_pending + wcount) *
				      sizeof(wchar_t));
	for (i = 0; i < wcount; i++) {
		terminal->pending[terminal->n_pending] = wbuf[i];
		terminal->n_pending++;
		/* Check if the contents of the array is a control string or
		 * not.  The match function returns NULL if the data is not
		 * a control sequence, the name of the control sequence if it
		 * is one, and an empty string if it might be the beginning of
		 * a control sequence. */
		vte_trie_match(terminal->trie,
			       terminal->pending,
			       terminal->n_pending,
			       &match,
			       &quark,
			       &params);
		if (match == NULL) {
			/* No interesting stuff in the buffer, so dump the
			 * accumulated data out. */
			for (j = 0; j < terminal->n_pending; j++) {
				c = terminal->pending[j];
#ifdef VTE_DEBUG
				if (c > 127) {
					fprintf(stderr, "%ld = ", (long) c);
				}
				if (c < 32) {
					fprintf(stderr, "^%lc\n",
						(wint_t)c + 64);
				} else {
					fprintf(stderr, "`%lc'\n", (wint_t)c);
				}
#endif
				vte_terminal_insert_char(widget, c);
			}
			terminal->n_pending = 0;
		} else if (match[0] != '\0') {
			/* A terminal sequence. */
			vte_terminal_handle_sequence(GTK_WIDGET(terminal),
						     match,
						     quark,
						     params);
			if (params != NULL) {
				g_value_array_free(params);
			}
			terminal->n_pending = 0;
		} else {
			/* It's a zero-length string, so we need to wait for
			 * more data from the client. */
		}
	}

	/* Handle error conditions. */
	if (bcount <= 0) {
		if (bcount == 0) {
			/* EOF */
			g_source_remove(terminal->pty_input);
			vte_terminal_eof(terminal->pty_input, data);
		} else {
			switch (errno) {
				case EIO:
					/* Fake EOF. */
					g_source_remove(terminal->pty_input);
					vte_terminal_eof(terminal->pty_input,
							 data);
					leave_open = FALSE;
					break;
				case EAGAIN:
				case EBUSY:
					break;
				default:
					g_warning("Error reading from child: "
						  "%s.\n", strerror(errno));
			}
		}
	}

	return leave_open;
}

/* Send some data to the child. */
static void
vte_terminal_send(VteTerminal *terminal, const guchar *data, size_t length)
{
	size_t count;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	count = write(terminal->pty_master, data, length);
	if (count != length) {
		g_warning("%s sending data to child\n", strerror(errno));
	}
}

/* Read and handle a keypress event. */
static gint
vte_terminal_key_press(GtkWidget *widget, GdkEventKey *event)
{
	VteTerminal *terminal;
	GdkModifierType modifiers;
	struct vte_termcap *termcap;
	const char *tterm;
	unsigned char *normal = NULL;
	size_t normal_length = 0;
	unsigned char *special = NULL;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(VTE_IS_TERMINAL(widget), FALSE);
	terminal = VTE_TERMINAL(widget);

	if (event->type == GDK_KEY_PRESS) {
		/* Read the modifiers. */
		if (gdk_event_get_state((GdkEvent*)event,
					&modifiers) == FALSE) {
			modifiers = 0;
		}
		/* Map the key to a sequence name if we can. */
		switch (event->keyval) {
#if 0
			case GDK_BackSpace:
				special = "kb";
				break;
#endif
			case GDK_Delete:
				special = "kD";
				break;
			case GDK_KP_Home:
			case GDK_Home:
				special = "kh";
				break;
			case GDK_KP_End:
			case GDK_End:
				special = "@7";
				break;
			case GDK_F1:
				special = "k1";
				break;
			case GDK_F2:
				special = "k2";
				break;
			case GDK_F3:
				special = "k3";
				break;
			case GDK_F4:
				special = "k4";
				break;
			case GDK_F5:
				special = "k5";
				break;
			case GDK_F6:
				special = "k6";
				break;
			case GDK_F7:
				special = "k7";
				break;
			case GDK_F8:
				special = "k8";
				break;
			case GDK_F9:
				special = "k9";
				break;
			case GDK_F10:
				special = "k0";
				break;
			case GDK_F11:
				special = "k;";
				break;
			/* Cursor keys. */
			case GDK_KP_Up:
			case GDK_Up:
				special = "ku";
				break;
			case GDK_KP_Down:
			case GDK_Down:
				special = "kd";
				break;
			case GDK_KP_Left:
			case GDK_Left:
				special = "kl";
				break;
			case GDK_KP_Right:
			case GDK_Right:
				special = "kr";
				break;
			case GDK_Page_Up:
				if (modifiers & GDK_SHIFT_MASK) {
					fprintf(stderr, "Shift-PgUp\n");
				} else {
					special = "kP";
				}
				break;
			case GDK_Page_Down:
				if (modifiers & GDK_SHIFT_MASK) {
					fprintf(stderr, "Shift-PgDn\n");
				} else {
					special = "kN";
				}
				break;
			case GDK_Tab:
				if (modifiers & GDK_SHIFT_MASK) {
					special = "kB";
				} else {
					normal = g_strdup("\t");
					normal_length = 1;
				}
				break;
			/* The default is to just send the string. */
			default:
				if (event->string != NULL) {
					normal = g_strdup(event->string);
					normal_length = strlen(normal);
				}
				break;
		}
		/* If we got normal characters, send them to the child. */
		if (normal != NULL) {
			vte_terminal_send(terminal, normal, normal_length);
			g_free(normal);
			normal = NULL;
		}
		/* If the key maps to characters, send them to the child. */
		if (special != NULL) {
			termcap = terminal->termcap;
			tterm = terminal->terminal;
			normal = vte_termcap_find_string_length(termcap,
								tterm,
								special,
								&normal_length);
			special = g_strdup_printf(normal, 1);
			vte_terminal_send(terminal, special, strlen(special));
			g_free(special);
		}
		return TRUE;
	}
	return FALSE;
}

/* Read and handle a pointing device buttonpress event. */
static gint
vte_terminal_button_press(GtkWidget *widget, GdkEventButton *event)
{
	fprintf(stderr, "button pressed\n");
	if (event->type == GDK_BUTTON_PRESS) {
		if (!GTK_WIDGET_HAS_FOCUS(widget)) {
			gtk_widget_grab_focus(widget);
		}
		return TRUE;
	}
	return FALSE;
}

/* Handle receiving or losing focus. */
static gint
vte_terminal_focus_in(GtkWidget *widget, GdkEventFocus *event)
{
	g_return_val_if_fail(GTK_IS_WIDGET(widget), 0);
	GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);
	return TRUE;
}

static gint
vte_terminal_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
	g_return_val_if_fail(GTK_WIDGET(widget), 0);
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
	return TRUE;
}

/* Set the fontset used for rendering text into the widget. */
static void
vte_terminal_set_fontset(VteTerminal *terminal, const char *xlfds)
{
	guint width, height, ascent, descent;
	GtkWidget *widget;
	XFontStruct **font_struct_list, font_struct;
	char **missing_charset_list, *def_string;
	int missing_charset_count;
	char **font_name_list;

	g_return_if_fail(terminal != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);

	/* Choose default font metrics.  I like '10x20' as a terminal font. */
	if (xlfds == NULL) {
		xlfds = "10x20";
	}
	width = 10;
	height = 20;
	descent = 0;
	ascent = height - descent;

	/* Load the font set, freeing another one if we loaded one before. */
	if (terminal->fontset) {
		XFreeFontSet(GDK_DISPLAY(), terminal->fontset);
	}
	terminal->fontset = XCreateFontSet(GDK_DISPLAY(),
					   xlfds,
					   &missing_charset_list,
					   &missing_charset_count,
					   &def_string);
	g_return_if_fail(terminal->fontset != NULL);
	XFreeStringList(missing_charset_list);
	missing_charset_list = NULL;
	/* Read the font metrics. */
	if (XFontsOfFontSet(terminal->fontset,
			    &font_struct_list,
			    &font_name_list)) {
		if (font_struct_list) {
			if (font_struct_list[0]) {
				font_struct = font_struct_list[0][0];
				width = font_struct.max_bounds.width;
				ascent = font_struct.max_bounds.ascent;
				descent = font_struct.max_bounds.descent;
				height = ascent + descent;
			}
		}
		XFreeStringList(font_name_list);
		font_name_list = NULL;
	}

	/* Now save the values. */
	terminal->char_width = width;
	terminal->char_height = height;
	terminal->char_ascent = ascent;
	terminal->char_descent = descent;

	/* Emit a signal that the font changed. */
	g_signal_emit_by_name(terminal,
			      "char_size_changed",
			      terminal->char_width,
			      terminal->char_height);
}

/* A comparison function which helps sort quarks. */
static gint
vte_compare_direct(gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

/* Read and refresh our perception of the size of the PTY. */
static void
vte_terminal_pty_size_get(VteTerminal *terminal)
{
	struct winsize size;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(terminal->pty_master != -1);
	/* Use an ioctl to read the size of the terminal. */
	if (ioctl(terminal->pty_master, TIOCGWINSZ, &size) != 0) {
		g_warning("Error reading PTY size, assuming defaults: %s.",
			  strerror(errno));
		terminal->row_count = 10;
		terminal->column_count = 60;
	} else {
		terminal->row_count = size.ws_row;
		terminal->column_count = size.ws_col;
	}
}

/* Set the size of the PTY. */
static void
vte_terminal_pty_size_set(VteTerminal *terminal, guint columns, guint rows)
{
	struct winsize size;
	size.ws_row = rows;
	size.ws_col = columns;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(terminal->pty_master != -1);
	/* Try to set the terminal size. */
	if (ioctl(terminal->pty_master, TIOCSWINSZ, &size) != 0) {
		g_warning("Error setting PTY size: %s.", strerror(errno));
	}
	/* Read the terminal size, in case something went awry. */
	vte_terminal_pty_size_get(terminal);
}

/* Redraw the widget. */
static void
vte_handle_scroll(VteTerminal *terminal)
{
	long dy, adj;
	GtkWidget *widget;
	struct _VteScreen *screen;
	/* Sanity checks. */
	g_return_if_fail(GTK_IS_WIDGET(terminal));
	widget = GTK_WIDGET(terminal);
	screen = terminal->screen;
	if (GTK_WIDGET_REALIZED(widget) == FALSE) {
		return;
	}
	/* This may generate multiple redraws, so freeze it while we do them. */
	gdk_window_freeze_updates(widget->window);
	/* Read the new adjustment value and save the difference. */
	adj = floor(gtk_adjustment_get_value(terminal->adjustment));
	dy = screen->delta - adj;
	screen->delta = adj;
	if (dy != 0) {
		/* Scroll whatever's already in the window to avoid redrawing
		 * as much as possible. */
		gdk_window_scroll(widget->window,
				  0, dy * terminal->char_height);
		/* Trigger an expose on newly-exposed areas. */
		if (dy > 0) {
			vte_invalidate_cells(terminal,
					     0,
					     terminal->column_count,
					     screen->cursor_current.row - dy,
					     dy);
		} else {
			vte_invalidate_cells(terminal,
					     0,
					     terminal->column_count,
					     0,
					     -dy);
		}
	}
	/* Let the refreshing begin. */
	gdk_window_thaw_updates(widget->window);
}

/* Set the adjustment objects used by the terminal widget. */
static void
vte_terminal_set_scroll_adjustment(VteTerminal *terminal,
				   GtkAdjustment *adjustment)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (adjustment != NULL) {
		/* Add a reference to the new adjustment object. */
		g_object_ref(adjustment);
		/* Get rid of the old adjustment object. */
		if (terminal->adjustment != NULL) {
			/* Disconnect our signal handlers from this object. */
			g_signal_handlers_disconnect_by_func(terminal->adjustment,
					 		     G_CALLBACK(vte_handle_scroll),
							     terminal);
			g_object_unref(terminal->adjustment);
		}
		/* Set the new adjustment object. */
		terminal->adjustment = adjustment;
		g_signal_connect_swapped(terminal->adjustment,
					 "value_changed",
					 G_CALLBACK(vte_handle_scroll),
					 terminal);
		g_signal_connect_swapped(terminal->adjustment,
					 "changed",
					 G_CALLBACK(vte_handle_scroll),
					 terminal);
	}
}

/* Set the type of terminal we're emulating. */
static void
vte_terminal_set_emulation(VteTerminal *terminal, const char *emulation)
{
	const char *code, *value;
	char *tmp;
	int i;

	/* Set the emulation type, for reference. */
	if (emulation == NULL) {
		emulation = "xterm";
	}
	terminal->terminal = g_quark_to_string(g_quark_from_string(emulation));
#ifdef VTE_DEBUG
	g_print("Setting emulation to `%s'...", emulation);
#endif

	/* Create a trie to hold the control sequences. */
	if (terminal->trie) {
		vte_trie_free(terminal->trie);
	}
	terminal->trie = vte_trie_new();

	/* Create a tree to hold the handlers. */
	if (terminal->sequences) {
		g_tree_destroy(terminal->sequences);
	}
	terminal->sequences = g_tree_new(vte_compare_direct);
	for (i = 0; i < G_N_ELEMENTS(vte_sequence_handlers); i++) {
		if (vte_sequence_handlers[i].handler != NULL) {
			code = vte_sequence_handlers[i].code;
			g_tree_insert(terminal->sequences,
				      GINT_TO_POINTER(g_quark_from_string(code)),
				      vte_sequence_handlers[i].handler);
		}
	}

	/* Load the known capability strings from the termcap structure into
	 * the trie for recognition. */
	for (i = 0;
	     vte_terminal_capability_strings[i].capability != NULL;
	     i++) {
		code = vte_terminal_capability_strings[i].capability;
		tmp = vte_termcap_find_string(terminal->termcap,
					      terminal->terminal,
					      code);
		if ((tmp != NULL) && (tmp[0] != '\0')) {
			vte_trie_add(terminal->trie, tmp, strlen(tmp),
				     vte_terminal_capability_strings[i].capability,
				     0);
		}
		g_free(tmp);
	}

	/* Add emulator-specific sequences. */
	for (i = 0; vte_xterm_capability_strings[i].value != NULL; i++) {
		code = vte_xterm_capability_strings[i].code;
		value = vte_xterm_capability_strings[i].value;
		vte_trie_add(terminal->trie, code, strlen(code), value, 0);
	}
#ifdef VTE_DEBUG
	g_print("\n");
#endif
}

/* Set the path to the termcap file we read, and read it in. */
static void
vte_terminal_set_termcap(VteTerminal *terminal, const char *path)
{
	if (path == NULL) {
		path = "/etc/termcap";
	}
	terminal->termcap_path = g_quark_to_string(g_quark_from_string(path));
#ifdef VTE_DEBUG
	g_print("Loading termcap `%s'...", terminal->termcap_path);
#endif
	if (terminal->termcap) {
		vte_termcap_free(terminal->termcap);
	}
	terminal->termcap = vte_termcap_new(path);
#ifdef VTE_DEBUG
	g_print("\n");
#endif
	vte_terminal_set_emulation(terminal, terminal->terminal);
}

/* Initialize the terminal widget after the base widget stuff is initialized.
 * We need to create a new psuedo-terminal pair, read in the termcap file, and
 * set ourselves up to do the interpretation of sequences. */
static void
vte_terminal_init(VteTerminal *terminal)
{
	GtkAdjustment *adjustment;
	GIOChannel *channel;
	int i;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(terminal), GTK_CAN_FOCUS);

	/* Initialize data members with settings from the environment and
	 * structures to use for these. */
	terminal->shell = g_strdup(getenv("SHELL") ?: "/bin/sh");
	terminal->pty_master = -1;
	terminal->pty_pid = -1;
	terminal->pending = NULL;
	terminal->n_pending = 0;
	terminal->palette_initialized = FALSE;
	terminal->keypad = VTE_KEYPAD_NORMAL;
	adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0));

	vte_terminal_set_termcap(terminal, NULL);
	vte_terminal_set_emulation(terminal, NULL);
	vte_terminal_set_encoding(terminal, NULL);

	terminal->normal_screen.row_data = g_array_new(FALSE, TRUE,
						       sizeof(GArray *));
	terminal->normal_screen.cursor_current.row = 0;
	terminal->normal_screen.cursor_current.col = 0;
	terminal->normal_screen.cursor_saved.row = 0;
	terminal->normal_screen.cursor_saved.col = 0;
	terminal->normal_screen.cursor_visible = TRUE;
	terminal->normal_screen.delta = 0;
	terminal->normal_screen.insert = FALSE;

	terminal->alternate_screen.row_data = g_array_new(FALSE, TRUE,
							  sizeof(GArray*));
	terminal->alternate_screen.cursor_current.row = 0;
	terminal->alternate_screen.cursor_current.col = 0;
	terminal->alternate_screen.cursor_saved.row = 0;
	terminal->alternate_screen.cursor_saved.col = 0;
	terminal->alternate_screen.cursor_visible = TRUE;
	terminal->alternate_screen.delta = 0;
	terminal->alternate_screen.insert = FALSE;

	terminal->screen = &terminal->alternate_screen;
	vte_terminal_set_default_attributes(terminal);

	terminal->screen = &terminal->normal_screen;
	vte_terminal_set_default_attributes(terminal);

	vte_terminal_set_scroll_adjustment(terminal, adjustment);

	/* Start up the shell. */
	terminal->pty_master = vte_pty_open(&terminal->pty_pid,
					    terminal->shell);
	i = fcntl(terminal->pty_master, F_GETFL);
	fcntl(terminal->pty_master, F_SETFL, i | O_NONBLOCK);
	channel = g_io_channel_unix_new(terminal->pty_master);
	terminal->pty_input = g_io_add_watch_full(channel,
						  G_PRIORITY_LOW,
						  G_IO_IN | G_IO_HUP,
						  vte_terminal_io_read,
						  terminal,
						  NULL);

	/* Set the PTY window size based on the terminal type. */
	vte_terminal_pty_size_set(terminal,
				  vte_termcap_find_numeric(terminal->termcap,
					  		   terminal->terminal,
							   "co") ?: 60,
				  vte_termcap_find_numeric(terminal->termcap,
					  		   terminal->terminal,
							   "li") ?: 18);

	/* Set the font. */
	vte_terminal_set_fontset(terminal, NULL);
}

/* Tell GTK+ how much space we need. */
static void
vte_terminal_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	VteTerminal *terminal;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);

	requisition->width = terminal->char_width * terminal->column_count;
	requisition->height = terminal->char_height * terminal->row_count;
}

/* Accept a given size from GTK+. */
static void
vte_terminal_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	VteTerminal *terminal;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);

	/* Set our allocation to match the structure. */
	widget->allocation = *allocation;

	/* Calculate how many rows and columns we should display. */
	terminal->column_count = allocation->width / terminal->char_width;
	terminal->row_count = allocation->height / terminal->char_height;

	/* Set the size of the pseudo-terminal. */
	vte_terminal_pty_size_set(terminal,
				  terminal->column_count,
				  terminal->row_count);

	/* Resize the GDK window. */
	if (widget->window != NULL) {
		gdk_window_move_resize(widget->window,
				       allocation->x,
				       allocation->y,
				       allocation->width,
				       allocation->height);
	}

	/* Adjust the adjustments. */
	vte_terminal_adjust_adjustments(terminal);
}

/* The window is being destroyed. */
static void
vte_terminal_unrealize(GtkWidget *widget)
{
	VteTerminal *terminal;
	GArray *array;
	int i;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);

	/* Unmap the widget if it hasn't been already. */
	if (GTK_WIDGET_MAPPED(widget)) {
		gtk_widget_unmap(widget);
	}

	/* Remove the GDK window. */
	if (widget->window != NULL) {
		gdk_window_destroy(widget->window);
		widget->window = NULL;
	}

	/* Mark that we no longer have a GDK window. */
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);

	/* Free some of our strings. */
	terminal->termcap_path = NULL;
	terminal->shell = NULL;
	terminal->terminal = NULL;

	/* Shut down the child terminal. */
	close(terminal->pty_master);
	terminal->pty_master = -1;
	if (terminal->pty_pid > 0) {
		kill(-terminal->pty_pid, SIGHUP);
	}
	terminal->pty_pid = 0;

	/* Stop watching for input from the child. */
	if (terminal->pty_input != -1) {
		gtk_input_remove(terminal->pty_input);
		terminal->pty_input = -1;
	}

	/* Discard any pending data. */
	g_free(terminal->pending);
	terminal->pending = NULL;

	/* Clean up emulation structures. */
	g_tree_destroy(terminal->sequences);
	terminal->sequences= NULL;
	vte_termcap_free(terminal->termcap);
	terminal->termcap = NULL;
	vte_trie_free(terminal->trie);
	terminal->trie = NULL;

	/* Clear the output histories. */
	for (i = 0; i < terminal->normal_screen.row_data->len; i++) {
		array = g_array_index(terminal->normal_screen.row_data,
				      GArray*,
				      i);
		g_array_free(array, TRUE);
	}
	g_array_free(terminal->normal_screen.row_data, TRUE);
	terminal->normal_screen.row_data = NULL;

	for (i = 0; i < terminal->alternate_screen.row_data->len; i++) {
		array = g_array_index(terminal->alternate_screen.row_data,
				      GArray*,
				      i);
		g_array_free(array, TRUE);
	}
	g_array_free(terminal->alternate_screen.row_data, TRUE);
	terminal->alternate_screen.row_data = NULL;
}

/* Handle realizing the widget.  Most of this is copy-paste from GGAD. */
static void
vte_terminal_realize(GtkWidget *widget)
{
	VteTerminal *terminal = NULL;
	GdkWindowAttr attributes;
	GdkColor black = {0, 0, 0};
	int attributes_mask = 0;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);

	/* Create a GDK window for the widget. */
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = 0;
	attributes.y = 0;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual(widget);
	attributes.colormap = gtk_widget_get_colormap(widget);
	attributes.event_mask = gtk_widget_get_events(widget) |
				GDK_EXPOSURE_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK;
	attributes.cursor = gdk_cursor_new(GDK_XTERM);
	attributes_mask = GDK_WA_X |
			  GDK_WA_Y |
			  GDK_WA_VISUAL |
			  GDK_WA_COLORMAP |
			  GDK_WA_CURSOR;
	widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
					&attributes,
					attributes_mask);
	gdk_window_move_resize(widget->window,
			       widget->allocation.x,
			       widget->allocation.y,
			       widget->allocation.width,
			       widget->allocation.height);
	gdk_window_set_user_data(widget->window, widget);
	gdk_window_show(widget->window);

	/* Set up styles, backgrounds, and whatnot. */
	widget->style = gtk_style_attach(widget->style, widget->window);
	gtk_style_set_background(widget->style,
				 widget->window,
				 GTK_STATE_NORMAL);
	gdk_window_set_background(widget->window, &black);

	/* Set the realized flag. */
	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

	/* Grab input focus. */
	gtk_widget_grab_focus(widget);
}

/* Find the character in the given "virtual" position. */
struct vte_charcell *
vte_terminal_find_charcell(VteTerminal *terminal, long row, long col)
{
	GArray *rowdata;
	struct vte_charcell *ret = NULL;
	struct _VteScreen *screen;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	screen = terminal->screen;
	if (screen->row_data->len > row) {
		rowdata = g_array_index(screen->row_data, GArray*, row);
		if (rowdata->len > col) {
			ret = &g_array_index(rowdata, struct vte_charcell, col);
		}
	}
	return ret;
}

/* Draw the widget. */
static void
vte_terminal_paint(GtkWidget *widget, GdkRectangle *area)
{
	VteTerminal *terminal = NULL;
	struct _VteScreen *screen;
	Display *display;
	GdkDrawable *gdrawable;
	Drawable drawable;
	GC gc;
	struct vte_charcell *cell;
	int row, drow, col, dcol, row_stop, col_stop, x_offs = 0, y_offs = 0;
	int fore, back, width, height, ascent, descent;
	long delta;
	XwcTextItem textitem;
#ifdef HAVE_XFT
	GdkColormap *gcolormap;
	Colormap colormap;
	GdkVisual *gvisual;
	Visual *visual;
	XftFont *ftfont;
	XftDraw *ftdraw;
	XftColor ftcolor;
	XRenderColor rcolor;
	gboolean use_xft;
#endif

	/* Make a few sanity checks. */
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	g_return_if_fail(area != NULL);
	terminal = VTE_TERMINAL(widget);
	if (!GTK_WIDGET_DRAWABLE(widget)) {
		return;
	}
	screen = terminal->screen;

	/* Set up the default palette. */
	vte_terminal_set_default_palette(terminal);

	/* Get the X11 structures we need for the drawing area. */
	gdk_window_get_internal_paint_info(widget->window, &gdrawable,
					   &x_offs, &y_offs);
	display = gdk_x11_drawable_get_xdisplay(gdrawable);
	drawable = gdk_x11_drawable_get_xid(gdrawable);
	gc = XCreateGC(display, drawable, 0, NULL);

#ifdef HAVE_XFT
	use_xft = FALSE;
	if (getenv("VTE_USE_XFT") != NULL) {
		if (atol(getenv("VTE_USE_XFT")) > 0) {
			use_xft = TRUE;
		}
	}
	if (use_xft) {
		gcolormap = gdk_drawable_get_colormap(widget->window);
		colormap = gdk_x11_colormap_get_xcolormap(gcolormap);
		gvisual = gtk_widget_get_visual(widget);
		visual = gdk_x11_visual_get_xvisual(gvisual);
		/* FIXME: do this when we change the font, to avoid the
		 * need to do this for each repaint. */
		ftfont = XftFontOpen(display,
				     gdk_x11_get_default_screen(),
				     XFT_FAMILY, XftTypeString, "mono",
				     XFT_SIZE, XftTypeDouble, 14.0,
				     0);
		if (ftfont == NULL) {
			g_warning("Error allocating font, disabling Xft.");
			use_xft = FALSE;
		}
		ftdraw = XftDrawCreate(display,
				       drawable,
				       visual,
				       colormap);
		if (ftdraw == NULL) {
			g_warning("Error allocating draw, disabling Xft.");
			use_xft = FALSE;
		}
	}
#endif

	/* Keep local copies of rendering information. */
	width = terminal->char_width;
	height = terminal->char_height;
	ascent = terminal->char_ascent;
	descent = terminal->char_descent;
	delta = screen->delta;

	/* Paint the background for this area, using a filled rectangle.  We
	 * have to do this even when the GDK background matches, otherwise
	 * we may miss character removals before an area is re-exposed. */
	XSetForeground(display, gc, terminal->palette[0].pixel);
	XFillRectangle(display, drawable, gc,
		       area->x - x_offs,
		       area->y - y_offs,
		       area->width,
		       area->height);

	/* Now we're ready to draw the text.  Iterate over the rows we
	 * need to draw. */
	row = area->y / height;
	row_stop = (area->y + area->height + height - 1) / height;
	while (row < row_stop) {
		/* Get the row data for the row we want to display, taking
		 * scrolling into account. */
		drow = row + delta;
		col = area->x / width;
		col_stop = (area->x + area->width + width - 1) / width;
		while (col < col_stop) {
			/* Get the character cell's contents. */
			cell = vte_terminal_find_charcell(terminal, drow, col);
			if (cell != NULL) {
				/* If this column is zero-width, backtrack
				 * until we find the multi-column character
				 * which renders into this column. */
				if (cell->columns == 0) {
					/* Search for a suitable cell. */
					for (dcol = col - 1;
					     dcol >= 0;
					     dcol--) {
						cell = vte_terminal_find_charcell(terminal, drow, dcol);
						if (cell->columns > 0) {
							break;
						}
					}
					/* If we didn't find anything, bail. */
					if (dcol < 0) {
						continue;
					}
				}
				/* Determine what the foreground and background
				 * colors for rendering text should be. */
				if (cell->reverse) {
					fore = cell->back;
					back = cell->fore;
				} else {
					fore = cell->fore;
					back = cell->back;
				}
				if (cell->invisible) {
					fore = back;
				}
				if (cell->bold) {
					fore += 8;
				}
				if (cell->standout) {
					back += 8;
				}

				/* Set the textitem's fields. */
				textitem.chars = &cell->c;
				textitem.nchars = 1;
				textitem.delta = 0;
				textitem.font_set = terminal->fontset;

				/* Paint the background for the cell. */
				XSetForeground(display, gc,
					       terminal->palette[back].pixel);
				XFillRectangle(display, drawable, gc,
					       col * width - x_offs,
					       row * height - y_offs,
					       cell->columns * width,
					       height);
#if HAVE_XFT
				if (use_xft) {
					/* FIXME: save the colors when we do
					 * palette initialization. */
					rcolor.red = terminal->palette[fore].red;
					rcolor.green = terminal->palette[fore].green;
					rcolor.blue = terminal->palette[fore].blue;
					rcolor.alpha = 0xffff;
					if (XftColorAllocValue(display,
							       visual,
							       colormap,
							       &rcolor,
							       &ftcolor)) {
						/* FIXME: need to do the proper
						 * encoding changes in here if
						 * wchar_t isn't suitable.
						 * Also this will probably be
						 * hideously slow. */
						XftChar32 ftc;
						ftc = cell->c;
						XftDrawString32(ftdraw,
								&ftcolor,
								ftfont,
								col * width -
								x_offs,
								row * height -
								y_offs + ascent,
								&ftc, 1);
						XftColorFree(display,
							     visual,
							     colormap,
							     &ftcolor);
					} else {
						g_warning("Unable to allocate color, not rendering.");
					}
				} else {
#endif
					/* Draw the text.  We've handled bold,
					 * standout and reverse already, but we
					 * need to handle half, and maybe
					 * blink, if we decide to be evil. */
					XSetForeground(display, gc,
						       terminal->palette[fore].pixel);
					XwcDrawText(display, drawable, gc,
						    col * width - x_offs,
						    row * height - y_offs + ascent,
						    &textitem, 1);
#if HAVE_XFT
				}
#endif
				/* FX */
				if (cell->underline) {
					XDrawLine(display, drawable, gc,
						  col * width - x_offs,
						  row * height - y_offs + height - 1,
						  col * width - x_offs + width - 1,
						  row * height - y_offs + height - 1);
				}
				col += cell->columns;
			} else {
				/* Skip to the next column. */
				col++;
			}
		}
		row++;
	}

	if (terminal->screen->cursor_visible) {
		/* Draw the insertion cursor in the foreground color for this
		 * cell, shrinking it by one pixel to keep from overflowing
		 * into the next character cell. */
		col = screen->cursor_current.col;
		row = screen->cursor_current.row;
		cell = vte_terminal_find_charcell(terminal, row - delta, col);
		XSetForeground(display, gc,
			       cell ?
			       terminal->palette[cell->fore].pixel :
			       terminal->palette[screen->defaults.fore].pixel);
		XFillRectangle(display, drawable, gc,
			       col * width - x_offs,
			       (row - delta) * height - y_offs,
			       width - 1,
			       height - 1);
		/* If we have a character in this spot, draw it in the reverse
		 * of the normal color. */
		if (cell != NULL) {
			/* Draw the text reversed.  FIXME: handle half, bold,
			 * standout, blink. */
			XSetForeground(display, gc,
				       cell ?
				       terminal->palette[cell->back].pixel :
				       terminal->palette[screen->defaults.back].pixel);
			textitem.chars = &cell->c;
			textitem.nchars = 1;
			textitem.delta = 0;
			textitem.font_set = terminal->fontset;
			XwcDrawText(display, drawable, gc,
				    col * width - x_offs,
				    row * height - y_offs + ascent,
				    &textitem, 1);
		}
	}

	/* Done with various structures. */
#ifdef HAVE_XFT
	if (use_xft) {
		if (ftdraw != NULL) {
			XftDrawDestroy(ftdraw);
		}
		if (ftfont != NULL) {
			XftFontClose(display, ftfont);
		}
	}
#endif
	XFreeGC(display, gc);
}

/* Handle an expose event by painting the exposed area. */
static gint
vte_terminal_expose(GtkWidget *widget, GdkEventExpose *event)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(widget), 0);
	if (event->window == widget->window) {
		vte_terminal_paint(widget, &event->area);
	} else {
		g_assert_not_reached();
	}
	return TRUE;
}

/* Initialize methods. */
static void
vte_terminal_class_init(VteTerminalClass *klass, gconstpointer data)
{
	GtkWidgetClass *widget_class;
	widget_class = GTK_WIDGET_CLASS(klass);
	/* Override some of the default handlers. */
	widget_class->realize = vte_terminal_realize;
	widget_class->expose_event = vte_terminal_expose;
	widget_class->key_press_event = vte_terminal_key_press;
	widget_class->button_press_event = vte_terminal_button_press;
	widget_class->focus_in_event = vte_terminal_focus_in;
	widget_class->focus_out_event = vte_terminal_focus_out;
	widget_class->unrealize = vte_terminal_unrealize;
	widget_class->size_request = vte_terminal_size_request;
	widget_class->size_allocate = vte_terminal_size_allocate;
	klass->eof_signal =
		g_signal_new("eof",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->set_window_title_signal =
		g_signal_new("set_window_title",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__STRING,
			     G_TYPE_NONE, 1, G_TYPE_STRING);
	klass->set_icon_title_signal =
		g_signal_new("set_icon_title",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__STRING,
			     G_TYPE_NONE, 1, G_TYPE_STRING);
	klass->char_size_changed_signal =
		g_signal_new("char_size_changed",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__UINT_UINT,
			     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
}

GtkType
vte_terminal_get_type(void)
{
	static GtkType terminal_type = 0;
	static const GTypeInfo terminal_info = {
		sizeof(VteTerminalClass),
		(GBaseInitFunc)NULL,
		(GBaseFinalizeFunc)NULL,

		(GClassInitFunc)vte_terminal_class_init,
		(GClassFinalizeFunc)NULL,
		(gconstpointer)NULL,

		sizeof(VteTerminal),
		0,
		(GInstanceInitFunc)vte_terminal_init,

		(GTypeValueTable*)NULL,
	};

	if (terminal_type == 0) {
		terminal_type = g_type_register_static(GTK_TYPE_WIDGET,
						       "VteTerminal",
						       &terminal_info,
						       0);
	}

	return terminal_type;
}
