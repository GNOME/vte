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

#include <limits.h>
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

#include <glib.h>

#include "vte.h"
#include "vte-private.h"
#include "vtetc.h"

#define BEL "\007"



/* FUNCTIONS WE USE */



static void
display_control_sequence(const char *name, GValueArray *params)
{
#ifdef VTE_DEBUG
	guint i;
	long l;
	const char *s;
	const gunichar *w;
	GValue *value;
	g_printerr("%s(", name);
	if (params != NULL) {
		for (i = 0; i < params->n_values; i++) {
			value = g_value_array_get_nth(params, i);
			if (i > 0) {
				g_printerr(", ");
			}
			if (G_VALUE_HOLDS_LONG(value)) {
				l = g_value_get_long(value);
				g_printerr("%ld", l);
			} else
			if (G_VALUE_HOLDS_STRING(value)) {
				s = g_value_get_string(value);
				g_printerr("\"%s\"", s);
			} else
			if (G_VALUE_HOLDS_POINTER(value)) {
				w = g_value_get_pointer(value);
				g_printerr("\"%ls\"", (const wchar_t*) w);
			}
		}
	}
	g_printerr(")\n");
#endif
}


/* A couple are duplicated from vte.c, to keep them static... */

/* Find the character an the given position in the backscroll buffer. */
static struct vte_charcell *
vte_terminal_find_charcell(VteTerminal *terminal, glong col, glong row)
{
	VteRowData *rowdata;
	struct vte_charcell *ret = NULL;
	VteScreen *screen;
	g_assert(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	if (_vte_ring_contains(screen->row_data, row)) {
		rowdata = _vte_ring_index(screen->row_data, VteRowData *, row);
		if ((glong) rowdata->cells->len > col) {
			ret = &g_array_index(rowdata->cells,
					     struct vte_charcell,
					     col);
		}
	}
	return ret;
}

/* Append a single item to a GArray a given number of times. Centralizing all
 * of the places we do this may let me do something more clever later.
 * Dupped from vte.c. */
static void
vte_g_array_fill(GArray *array, gpointer item, guint final_size)
{
	g_assert(array != NULL);
	if (array->len >= final_size) {
		return;
	}
	g_assert(item != NULL);

	final_size -= array->len;
	do {
		g_array_append_vals(array, item, 1);
	} while (--final_size);
}

/* Insert a blank line at an arbitrary position. */
static void
vte_insert_line_internal(VteTerminal *terminal, glong position)
{
	VteRowData *row, *old_row;
	old_row = terminal->pvt->free_row;
	/* Pad out the line data to the insertion point. */
	while (_vte_ring_next(terminal->pvt->screen->row_data) < position) {
		if (old_row) {
			row = _vte_reset_row_data (terminal, old_row, TRUE);
		} else {
			row = _vte_new_row_data_sized(terminal, TRUE);
		}
		old_row = _vte_ring_append(terminal->pvt->screen->row_data, row);
	}
	/* If we haven't inserted a line yet, insert a new one. */
	if (old_row) {
		row = _vte_reset_row_data (terminal, old_row, TRUE);
	} else {
		row = _vte_new_row_data_sized(terminal, TRUE);
	}
	if (_vte_ring_next(terminal->pvt->screen->row_data) >= position) {
		old_row = _vte_ring_insert(terminal->pvt->screen->row_data,
				 position, row);
	} else {
		old_row =_vte_ring_append(terminal->pvt->screen->row_data, row);
	}
	terminal->pvt->free_row = old_row;
}

/* Remove a line at an arbitrary position. */
static void
vte_remove_line_internal(VteTerminal *terminal, glong position)
{
	if (_vte_ring_next(terminal->pvt->screen->row_data) > position) {
		if (terminal->pvt->free_row)
			_vte_free_row_data (terminal->pvt->free_row);

		terminal->pvt->free_row = _vte_ring_remove(
				terminal->pvt->screen->row_data,
				position,
				FALSE);
	}
}

/* Check how long a string of unichars is.  Slow version. */
static gssize
vte_unichar_strlen(gunichar *c)
{
	int i;
	for (i = 0; c[i] != 0; i++) ;
	return i;
}

/* Convert a wide character string to a multibyte string */
static gchar *
vte_ucs4_to_utf8 (VteTerminal *terminal, const guchar *in)
{
	gchar *out = NULL;
	guchar *buf = NULL, *bufptr = NULL;
	gsize inlen, outlen;
	VteConv conv;

	conv = _vte_conv_open ("UTF-8", VTE_CONV_GUNICHAR_TYPE);

	if (conv != VTE_INVALID_CONV) {
		inlen = vte_unichar_strlen ((gunichar *) in) * sizeof (gunichar);
		outlen = (inlen * VTE_UTF8_BPC) + 1;

		_vte_buffer_set_minimum_size (terminal->pvt->conv_buffer, outlen);
		buf = bufptr = terminal->pvt->conv_buffer->bytes;

		if (_vte_conv (conv, &in, &inlen, &buf, &outlen) == (size_t) -1) {
			_vte_debug_print (VTE_DEBUG_IO,
					  "Error converting %ld string bytes (%s), skipping.\n",
					  (long) _vte_buffer_length (terminal->pvt->outgoing),
					  g_strerror (errno));
			bufptr = NULL;
		} else {
			out = g_strndup ((gchar *) bufptr, buf - bufptr);
		}
	}

	_vte_conv_close (conv);

	return out;
}

static gboolean
vte_parse_color (const char *spec, GdkColor *color)
{
	gchar *spec_copy = (gchar *) spec;
	gboolean retval = FALSE;

	/* gdk_color_parse doesnt handle all XParseColor formats.  It only
	 * supports the #RRRGGGBBB format, not the rgb:RRR/GGG/BBB format.
	 * See: man XParseColor */

	if (g_ascii_strncasecmp (spec_copy, "rgb:", 4) == 0) {
		gchar *cur, *ptr;

		spec_copy = g_strdup (spec);
		cur = spec_copy;
		ptr = spec_copy + 3;

		*cur++ = '#';
		while (*ptr++)
			if (*ptr != '/')
				*cur++ = *ptr;
		*cur++ = '\0';
	}

	retval = gdk_color_parse (spec_copy, color);

	if (spec_copy != spec)
		g_free (spec_copy);

	return retval;
}






/* Emit a "deiconify-window" signal. */
static void
vte_terminal_emit_deiconify_window(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `deiconify-window'.\n");
	g_signal_emit_by_name(terminal, "deiconify-window");
}

/* Emit a "iconify-window" signal. */
static void
vte_terminal_emit_iconify_window(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `iconify-window'.\n");
	g_signal_emit_by_name(terminal, "iconify-window");
}

/* Emit a "raise-window" signal. */
static void
vte_terminal_emit_raise_window(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `raise-window'.\n");
	g_signal_emit_by_name(terminal, "raise-window");
}

/* Emit a "lower-window" signal. */
static void
vte_terminal_emit_lower_window(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `lower-window'.\n");
	g_signal_emit_by_name(terminal, "lower-window");
}

/* Emit a "maximize-window" signal. */
static void
vte_terminal_emit_maximize_window(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `maximize-window'.\n");
	g_signal_emit_by_name(terminal, "maximize-window");
}

/* Emit a "refresh-window" signal. */
static void
vte_terminal_emit_refresh_window(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `refresh-window'.\n");
	g_signal_emit_by_name(terminal, "refresh-window");
}

/* Emit a "restore-window" signal. */
static void
vte_terminal_emit_restore_window(VteTerminal *terminal)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `restore-window'.\n");
	g_signal_emit_by_name(terminal, "restore-window");
}

/* Emit a "move-window" signal.  (Pixels.) */
static void
vte_terminal_emit_move_window(VteTerminal *terminal, guint x, guint y)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `move-window'.\n");
	g_signal_emit_by_name(terminal, "move-window", x, y);
}

/* Emit a "resize-window" signal.  (Pixels.) */
static void
vte_terminal_emit_resize_window(VteTerminal *terminal,
				guint width, guint height)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `resize-window'.\n");
	g_signal_emit_by_name(terminal, "resize-window", width, height);
}


/* Some common functions */

static void
_vte_terminal_home_cursor (VteTerminal *terminal)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_current.row = screen->insert_delta;
	screen->cursor_current.col = 0;
}

/* Clear the entire screen. */
static void
_vte_terminal_clear_screen (VteTerminal *terminal)
{
	VteRowData *rowdata, *old_row;
	long i, initial, row;
	VteScreen *screen;
	screen = terminal->pvt->screen;
	initial = screen->insert_delta;
	row = screen->cursor_current.row - screen->insert_delta;
	/* Add a new screen's worth of rows. */
	old_row = terminal->pvt->free_row;
	for (i = 0; i < terminal->row_count; i++) {
		/* Add a new row */
		if (i == 0) {
			initial = _vte_ring_next(screen->row_data);
		}
		if (old_row) {
			rowdata = _vte_reset_row_data (terminal, old_row, TRUE);
		} else {
			rowdata = _vte_new_row_data_sized(terminal, TRUE);
		}
		old_row = _vte_ring_append(screen->row_data, rowdata);
	}
	terminal->pvt->free_row = old_row;
	/* Move the cursor and insertion delta to the first line in the
	 * newly-cleared area and scroll if need be. */
	screen->insert_delta = initial;
	screen->cursor_current.row = row + screen->insert_delta;
	_vte_terminal_adjust_adjustments(terminal);
	/* Redraw everything. */
	_vte_invalidate_all(terminal);
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Clear the current line. */
static void
_vte_terminal_clear_current_line (VteTerminal *terminal)
{
	VteRowData *rowdata;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = _vte_ring_index(screen->row_data, VteRowData *,
					  screen->cursor_current.row);
		g_assert(rowdata != NULL);
		/* Remove it. */
		if (rowdata->cells->len > 0) {
			g_array_set_size(rowdata->cells, 0);
		}
		/* Add enough cells to the end of the line to fill out the
		 * row. */
		vte_g_array_fill(rowdata->cells,
				 &screen->fill_defaults,
				 terminal->column_count);
		rowdata->soft_wrapped = 0;
		/* Repaint this row. */
		_vte_invalidate_cells(terminal,
				      0, terminal->column_count,
				      screen->cursor_current.row, 1);
	}

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Clear above the current line. */
static void
_vte_terminal_clear_above_current (VteTerminal *terminal)
{
	VteRowData *rowdata;
	long i;
	VteScreen *screen;
	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	for (i = screen->insert_delta; i < screen->cursor_current.row; i++) {
		if (_vte_ring_next(screen->row_data) > i) {
			guint len;
			/* Get the data for the row we're erasing. */
			rowdata = _vte_ring_index(screen->row_data,
						  VteRowData *, i);
			g_assert(rowdata != NULL);
			/* Remove it. */
			len = rowdata->cells->len;
			if (len > 0) {
				g_array_set_size(rowdata->cells, 0);
			}
			/* Add new cells until we fill the row. */
			vte_g_array_fill(rowdata->cells,
					 &screen->fill_defaults,
					 terminal->column_count);
			rowdata->soft_wrapped = 0;
			/* Repaint the row. */
			_vte_invalidate_cells(terminal,
					0, terminal->column_count, i, 1);
		}
	}
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Scroll the text, but don't move the cursor.  Negative = up, positive = down. */
static void
_vte_terminal_scroll_text (VteTerminal *terminal, int scroll_amount)
{
	VteRowData *row, *old_row;
	long start, end, i;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = start + terminal->row_count - 1;
	}

	old_row = terminal->pvt->free_row;
	while (_vte_ring_next(screen->row_data) <= end) {
		if (old_row) {
			row = _vte_reset_row_data (terminal, old_row, FALSE);
		} else {
			row = _vte_new_row_data_sized(terminal, FALSE);
		}
		old_row = _vte_ring_append(terminal->pvt->screen->row_data, row);
	}
	terminal->pvt->free_row = old_row;
	if (scroll_amount > 0) {
		for (i = 0; i < scroll_amount; i++) {
			vte_remove_line_internal(terminal, end);
			vte_insert_line_internal(terminal, start);
		}
	} else {
		for (i = 0; i < -scroll_amount; i++) {
			vte_remove_line_internal(terminal, start);
			vte_insert_line_internal(terminal, end);
		}
	}

	/* Update the display. */
	_vte_terminal_scroll_region(terminal, start, end - start + 1,
				   scroll_amount);

	/* Adjust the scrollbars if necessary. */
	_vte_terminal_adjust_adjustments(terminal);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_inserted_flag = TRUE;
	terminal->pvt->text_deleted_flag = TRUE;
}

static gboolean
vte_terminal_termcap_string_same_as_for (VteTerminal *terminal,
					 const char  *cap_str,
					 const char  *cap_other)
{
	char *other_str;
	gboolean ret;

	other_str = _vte_termcap_find_string(terminal->pvt->termcap,
					     terminal->pvt->emulation,
					     cap_other);

	ret = other_str && (g_ascii_strcasecmp(cap_str, other_str) == 0);

	g_free (other_str);

	return ret;
}

/* Set icon/window titles. */
static void
vte_sequence_handler_set_title_internal(VteTerminal *terminal,
					GValueArray *params,
					gboolean icon_title,
					gboolean window_title)
{
	GValue *value;
	char *title = NULL;

	if (icon_title == FALSE && window_title == FALSE)
		return;

	/* Get the string parameter's value. */
	value = g_value_array_get_nth(params, 0);
	if (value) {
		if (G_VALUE_HOLDS_LONG(value)) {
			/* Convert the long to a string. */
			title = g_strdup_printf("%ld", g_value_get_long(value));
		} else
		if (G_VALUE_HOLDS_STRING(value)) {
			/* Copy the string into the buffer. */
			title = g_value_dup_string(value);
		} else
		if (G_VALUE_HOLDS_POINTER(value)) {
			title = vte_ucs4_to_utf8 (terminal, g_value_get_pointer (value));
		}
		if (title != NULL) {
			char *p, *validated;
			const char *end;

			/* Validate the text. */
			g_utf8_validate(title, strlen(title), &end);
			validated = g_strndup(title, end - title);

			/* No control characters allowed. */
			for (p = validated; *p != '\0'; p++) {
				if ((*p & 0x1f) == *p) {
					*p = ' ';
				}
			}

			/* Emit the signal */
			if (window_title) {
				g_free (terminal->pvt->window_title_changed);
				terminal->pvt->window_title_changed = g_strdup (validated);
			}

			if (icon_title) {
				g_free (terminal->pvt->icon_title_changed);
				terminal->pvt->icon_title_changed = g_strdup (validated);
			}

			g_free (validated);
			g_free(title);
		}
	}
}

/* Toggle a terminal mode. */
static void
vte_sequence_handler_set_mode_internal(VteTerminal *terminal,
				       long setting, gboolean value)
{
	switch (setting) {
	case 2:		/* keyboard action mode (?) */
		break;
	case 4:		/* insert/overtype mode */
		terminal->pvt->screen->insert_mode = value;
		break;
	case 12:	/* send/receive mode (local echo) */
		terminal->pvt->screen->sendrecv_mode = value;
		break;
	case 20:	/* automatic newline / normal linefeed mode */
		terminal->pvt->screen->linefeed_mode = value;
		break;
	default:
		break;
	}
}


/*
 * Sequence handling boilerplate
 */

/* Typedef the handle type */
typedef void (*VteTerminalSequenceHandler) (VteTerminal *terminal, GValueArray *params);

/* Prototype all handlers... */
#define VTE_SEQUENCE_HANDLER(name) \
	static void name (VteTerminal *terminal, GValueArray *params);
#include "vteseq-list.h"
#undef VTE_SEQUENCE_HANDLER


/* Call another handler, offsetting any long arguments by the given
 * increment value. */
static void
vte_sequence_handler_offset(VteTerminal *terminal,
			    GValueArray *params,
			    int increment,
			    VteTerminalSequenceHandler handler)
{
	guint i;
	long val;
	GValue *value;
	/* Decrement the parameters and let the _cs handler deal with it. */
	for (i = 0; (params != NULL) && (i < params->n_values); i++) {
		value = g_value_array_get_nth(params, i);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = g_value_get_long(value);
			val += increment;
			g_value_set_long(value, val);
		}
	}
	handler (terminal, params);
}

/* Call another function a given number of times, or once. */
static void
vte_sequence_handler_multiple(VteTerminal *terminal,
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
			val = MAX(val, 1);	/* FIXME: vttest. */
		}
	}
	for (i = 0; i < val; i++)
		handler (terminal, NULL);
}


/* Manipulate certain terminal attributes. */
static void
vte_sequence_handler_decset_internal(VteTerminal *terminal,
				     int setting,
				     gboolean restore,
				     gboolean save,
				     gboolean set)
{
	gboolean recognized = FALSE;
	gpointer p;
	guint i;
	struct {
		int setting;
		gboolean *bvalue;
		gint *ivalue;
		gpointer *pvalue;
		gpointer fvalue;
		gpointer tvalue;
		VteTerminalSequenceHandler reset, set;
	} settings[] = {
		/* 1: Application/normal cursor keys. */
		{1, NULL, &terminal->pvt->cursor_mode, NULL,
		 GINT_TO_POINTER(VTE_KEYMODE_NORMAL),
		 GINT_TO_POINTER(VTE_KEYMODE_APPLICATION),
		 NULL, NULL,},
		/* 2: disallowed, we don't do VT52. */
		{2, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 3: disallowed, window size is set by user. */
		{3, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 4: Smooth scroll. */
		{4, &terminal->pvt->smooth_scroll, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 5: Reverse video. */
		{5, &terminal->pvt->screen->reverse_mode, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 6: Origin mode: when enabled, cursor positioning is
		 * relative to the scrolling region. */
		{6, &terminal->pvt->screen->origin_mode, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 7: Wraparound mode. */
		{7, &terminal->pvt->flags.am, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 8: disallowed, keyboard repeat is set by user. */
		{8, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 9: Send-coords-on-click. */
		{9, NULL, &terminal->pvt->mouse_tracking_mode, NULL,
		 GINT_TO_POINTER(0),
		 GINT_TO_POINTER(MOUSE_TRACKING_SEND_XY_ON_CLICK),
		 NULL, NULL,},
		/* 12: disallowed, cursor blinks is set by user. */
		{12, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 18: print form feed. */
		/* 19: set print extent to full screen. */
		/* 25: Cursor visible. */
		{25, &terminal->pvt->cursor_visible, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 30/rxvt: disallowed, scrollbar visibility is set by user. */
		{30, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 35/rxvt: disallowed, fonts set by user. */
		{35, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 38: enter Tektronix mode. */
		/* 40: disallowed, the user sizes dynamically. */
		{40, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 41: more(1) fix. */
		/* 42: Enable NLS replacements. */
		{42, &terminal->pvt->nrc_mode, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 44: Margin bell. */
		{44, &terminal->pvt->margin_bell, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 47: Alternate screen. */
		{47, NULL, NULL, (gpointer) &terminal->pvt->screen,
		 &terminal->pvt->normal_screen,
		 &terminal->pvt->alternate_screen,
		 NULL, NULL,},
		/* 66: Keypad mode. */
		{66, &terminal->pvt->keypad_mode, NULL, NULL,
		 GINT_TO_POINTER(VTE_KEYMODE_NORMAL),
		 GINT_TO_POINTER(VTE_KEYMODE_APPLICATION),
		 NULL, NULL,},
		/* 67: disallowed, backspace key policy is set by user. */
		{67, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 1000: Send-coords-on-button. */
		{1000, NULL, &terminal->pvt->mouse_tracking_mode, NULL,
		 GINT_TO_POINTER(0),
		 GINT_TO_POINTER(MOUSE_TRACKING_SEND_XY_ON_BUTTON),
		 NULL, NULL,},
		/* 1001: Hilite tracking. */
		{1001, NULL, &terminal->pvt->mouse_tracking_mode, NULL,
		 GINT_TO_POINTER(0),
		 GINT_TO_POINTER(MOUSE_TRACKING_HILITE_TRACKING),
		 NULL, NULL,},
		/* 1002: Cell motion tracking. */
		{1002, NULL, &terminal->pvt->mouse_tracking_mode, NULL,
		 GINT_TO_POINTER(0),
		 GINT_TO_POINTER(MOUSE_TRACKING_CELL_MOTION_TRACKING),
		 NULL, NULL,},
		/* 1003: All motion tracking. */
		{1003, NULL, &terminal->pvt->mouse_tracking_mode, NULL,
		 GINT_TO_POINTER(0),
		 GINT_TO_POINTER(MOUSE_TRACKING_ALL_MOTION_TRACKING),
		 NULL, NULL,},
		/* 1010/rxvt: disallowed, scroll-on-output is set by user. */
		{1010, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 1011/rxvt: disallowed, scroll-on-keypress is set by user. */
		{1011, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 1035: disallowed, don't know what to do with it. */
		{1035, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 1036: Meta-sends-escape. */
		{1036, &terminal->pvt->meta_sends_escape, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 1037: disallowed, delete key policy is set by user. */
		{1037, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 1047: Use alternate screen buffer. */
		{1047, NULL, NULL, (gpointer) &terminal->pvt->screen,
		 &terminal->pvt->normal_screen,
		 &terminal->pvt->alternate_screen,
		 NULL, NULL,},
		/* 1048: Save/restore cursor position. */
		{1048, NULL, NULL, NULL,
		 NULL,
		 NULL,
		 vte_sequence_handler_rc,
		 vte_sequence_handler_sc,},
		/* 1049: Use alternate screen buffer, saving the cursor
		 * position. */
		{1049, NULL, NULL, (gpointer) &terminal->pvt->screen,
		 &terminal->pvt->normal_screen,
		 &terminal->pvt->alternate_screen,
		 vte_sequence_handler_rc,
		 vte_sequence_handler_sc,},
		/* 1051: Sun function key mode. */
		{1051, NULL, NULL, (gpointer) &terminal->pvt->sun_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
		/* 1052: HP function key mode. */
		{1052, NULL, NULL, (gpointer) &terminal->pvt->hp_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
		/* 1060: Legacy function key mode. */
		{1060, NULL, NULL, (gpointer) &terminal->pvt->legacy_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
		/* 1061: VT220 function key mode. */
		{1061, NULL, NULL, (gpointer) &terminal->pvt->vt220_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
	};

	/* Handle the setting. */
	for (i = 0; i < G_N_ELEMENTS(settings); i++)
	if (settings[i].setting == setting) {
		recognized = TRUE;
		/* Handle settings we want to ignore. */
		if ((settings[i].fvalue == settings[i].tvalue) &&
		    (settings[i].set == NULL) &&
		    (settings[i].reset == NULL)) {
			continue;
		}

		/* Read the old setting. */
		if (restore) {
			p = g_hash_table_lookup(terminal->pvt->dec_saved,
						GINT_TO_POINTER(setting));
			set = (p != NULL);
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Setting %d was %s.\n",
					setting, set ? "set" : "unset");
		}
		/* Save the current setting. */
		if (save) {
			if (settings[i].bvalue) {
				set = *(settings[i].bvalue) != FALSE;
			} else
			if (settings[i].ivalue) {
				set = *(settings[i].ivalue) ==
				      GPOINTER_TO_INT(settings[i].tvalue);
			} else
			if (settings[i].pvalue) {
				set = *(settings[i].pvalue) ==
				      settings[i].tvalue;
			}
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Setting %d is %s, saving.\n",
					setting, set ? "set" : "unset");
			g_hash_table_insert(terminal->pvt->dec_saved,
					    GINT_TO_POINTER(setting),
					    GINT_TO_POINTER(set));
		}
		/* Change the current setting to match the new/saved value. */
		if (!save) {
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Setting %d to %s.\n",
					setting, set ? "set" : "unset");
			if (settings[i].set && set) {
				settings[i].set (terminal, NULL);
			}
			if (settings[i].bvalue) {
				*(settings[i].bvalue) = set;
			} else
			if (settings[i].ivalue) {
				*(settings[i].ivalue) = set ?
					GPOINTER_TO_INT(settings[i].tvalue) :
					GPOINTER_TO_INT(settings[i].fvalue);
			} else
			if (settings[i].pvalue) {
				*(settings[i].pvalue) = set ?
					settings[i].tvalue :
					settings[i].fvalue;
			}
			if (settings[i].reset && !set) {
				settings[i].reset (terminal, NULL);
			}
		}
	}

	/* Do whatever's necessary when the setting changes. */
	switch (setting) {
	case 1:
		_vte_debug_print(VTE_DEBUG_KEYBOARD, set ?
				"Entering application cursor mode.\n" :
				"Leaving application cursor mode.\n");
		break;
	case 3:
		vte_terminal_emit_resize_window(terminal,
						(set ? 132 : 80) *
						terminal->char_width +
						VTE_PAD_WIDTH * 2,
						terminal->row_count *
						terminal->char_height +
						VTE_PAD_WIDTH * 2);
		/* Request a resize and redraw. */
		_vte_invalidate_all(terminal);
		break;
	case 5:
		/* Repaint everything in reverse mode. */
		_vte_invalidate_all(terminal);
		break;
	case 6:
		/* Reposition the cursor in its new home position. */
		terminal->pvt->screen->cursor_current.col = 0;
		terminal->pvt->screen->cursor_current.row =
			terminal->pvt->screen->insert_delta;
		break;
	case 47:
	case 1047:
	case 1049:
		/* Clear the alternate screen if we're switching
		 * to it, and home the cursor. */
		if (set) {
			_vte_terminal_clear_screen (terminal);
			_vte_terminal_home_cursor (terminal);
		}
		/* Reset scrollbars and repaint everything. */
		terminal->adjustment->value =
			terminal->pvt->screen->scroll_delta;
		vte_terminal_set_scrollback_lines(terminal,
				terminal->pvt->scrollback_lines);
		_vte_terminal_queue_contents_changed(terminal);
		_vte_invalidate_all (terminal);
		break;
	case 9:
	case 1000:
	case 1001:
	case 1002:
	case 1003:
		/* Make the pointer visible. */
		_vte_terminal_set_pointer_visible(terminal, TRUE);
		break;
	case 66:
		_vte_debug_print(VTE_DEBUG_KEYBOARD, set ?
				"Entering application keypad mode.\n" :
				"Leaving application keypad mode.\n");
		break;
	case 1051:
		_vte_debug_print(VTE_DEBUG_KEYBOARD, set ?
				"Entering Sun fkey mode.\n" :
				"Leaving Sun fkey mode.\n");
		break;
	case 1052:
		_vte_debug_print(VTE_DEBUG_KEYBOARD, set ?
				"Entering HP fkey mode.\n" :
				"Leaving HP fkey mode.\n");
		break;
	case 1060:
		_vte_debug_print(VTE_DEBUG_KEYBOARD, set ?
				"Entering Legacy fkey mode.\n" :
				"Leaving Legacy fkey mode.\n");
		break;
	case 1061:
		_vte_debug_print(VTE_DEBUG_KEYBOARD, set ?
				"Entering VT220 fkey mode.\n" :
				"Leaving VT220 fkey mode.\n");
		break;
	default:
		break;
	}

	if (!recognized) {
		_vte_debug_print (VTE_DEBUG_MISC,
				  "DECSET/DECRESET mode %d not recognized, ignoring.\n",
				  setting);
	}
}




/* THE HANDLERS */


/* End alternate character set. */
static void
vte_sequence_handler_ae (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->alternate_charset = FALSE;
}

/* Add a line at the current cursor position. */
static void
vte_sequence_handler_al (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	VteRowData *rowdata;
	long start, end, param, i;
	GValue *value;

	/* Find out which part of the screen we're messing with. */
	screen = terminal->pvt->screen;
	start = screen->cursor_current.row;
	if (screen->scrolling_restricted) {
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		end = screen->insert_delta + terminal->row_count - 1;
	}

	/* Extract any parameters. */
	param = 1;
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			param = g_value_get_long(value);
		}
	}

	/* Insert the right number of lines. */
	for (i = 0; i < param; i++) {
		/* Clear a line off the end of the region and add one to the
		 * top of the region. */
		vte_remove_line_internal(terminal, end);
		vte_insert_line_internal(terminal, start);
		/* Get the data for the new row. */
		rowdata = _vte_ring_index(screen->row_data,
					  VteRowData *, start);
		g_assert(rowdata != NULL);
		/* Add enough cells to it so that it has the default columns. */
		vte_g_array_fill(rowdata->cells, &screen->fill_defaults,
				 terminal->column_count);
		/* Adjust the scrollbars if necessary. */
		_vte_terminal_adjust_adjustments(terminal);
	}

	/* Update the display. */
	_vte_terminal_scroll_region(terminal, start, end - start + 1, param);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Add N lines at the current cursor position. */
static void
vte_sequence_handler_AL (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_al (terminal, params);
}

/* Start using alternate character set. */
static void
vte_sequence_handler_as (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->alternate_charset = TRUE;
}

/* Beep. */
static void
vte_sequence_handler_bl (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_beep (terminal);
	g_signal_emit_by_name(terminal, "beep");
}

/* Backtab. */
static void
vte_sequence_handler_bt (VteTerminal *terminal, GValueArray *params)
{
	long newcol;

	/* Calculate which column is the previous tab stop. */
	newcol = terminal->pvt->screen->cursor_current.col;

	if (terminal->pvt->tabstops != NULL) {
		/* Find the next tabstop. */
		while (newcol >= 0) {
			if (_vte_terminal_get_tabstop(terminal,
						     newcol % terminal->column_count)) {
				break;
			}
			newcol--;
		}
	}

	/* If we have no tab stops, stop at the first column. */
	if (newcol <= 0) {
		newcol = 0;
	}

	/* Warp the cursor. */
	_vte_debug_print(VTE_DEBUG_PARSE,
			"Moving cursor to column %ld.\n", (long)newcol);
	terminal->pvt->screen->cursor_current.col = newcol;
}

/* Clear from the cursor position to the beginning of the line. */
static void
vte_sequence_handler_cb (VteTerminal *terminal, GValueArray *params)
{
	VteRowData *rowdata;
	long i;
	VteScreen *screen;
	struct vte_charcell *pcell;
	screen = terminal->pvt->screen;

	/* Get the data for the row which the cursor points to. */
	rowdata = _vte_terminal_ensure_row(terminal);
	/* Clear the data up to the current column with the default
	 * attributes.  If there is no such character cell, we need
	 * to add one. */
	for (i = 0; i <= screen->cursor_current.col; i++) {
		if (i < (glong) rowdata->cells->len) {
			/* Muck with the cell in this location. */
			pcell = &g_array_index(rowdata->cells,
					       struct vte_charcell,
					       i);
			*pcell = screen->color_defaults;
		} else {
			/* Add new cells until we have one here. */
			g_array_append_val(rowdata->cells,
					   screen->color_defaults);
		}
	}
	/* Repaint this row. */
	_vte_invalidate_cells(terminal,
			      0, screen->cursor_current.col+1,
			      screen->cursor_current.row, 1);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Clear to the right of the cursor and below the current line. */
static void
vte_sequence_handler_cd (VteTerminal *terminal, GValueArray *params)
{
	VteRowData *rowdata;
	glong i;
	VteScreen *screen;

	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear the rest of the
	 * row the cursor is on and all of the rows below the cursor. */
	i = screen->cursor_current.row;
	if (i < _vte_ring_next(screen->row_data)) {
		/* Get the data for the row we're clipping. */
		rowdata = _vte_ring_index(screen->row_data, VteRowData *, i);
		/* Clear everything to the right of the cursor. */
		if ((rowdata != NULL) &&
		    ((glong) rowdata->cells->len > screen->cursor_current.col)) {
			g_array_set_size(rowdata->cells,
					 screen->cursor_current.col);
		}
	}
	/* Now for the rest of the lines. */
	for (i = screen->cursor_current.row + 1;
	     i < _vte_ring_next(screen->row_data);
	     i++) {
		/* Get the data for the row we're removing. */
		rowdata = _vte_ring_index(screen->row_data, VteRowData *, i);
		/* Remove it. */
		if ((rowdata != NULL) && (rowdata->cells->len > 0)) {
			g_array_set_size(rowdata->cells, 0);
		}
	}
	/* Now fill the cleared areas. */
	for (i = screen->cursor_current.row;
	     i < screen->insert_delta + terminal->row_count;
	     i++) {
		/* Retrieve the row's data, creating it if necessary. */
		if (_vte_ring_contains(screen->row_data, i)) {
			rowdata = _vte_ring_index(screen->row_data,
						  VteRowData *, i);
			g_assert(rowdata != NULL);
		} else {
			if (terminal->pvt->free_row) {
				rowdata = _vte_reset_row_data (terminal,
						terminal->pvt->free_row,
						FALSE);
			} else {
				rowdata = _vte_new_row_data(terminal);
			}
			terminal->pvt->free_row =
				_vte_ring_append(screen->row_data, rowdata);
		}
		/* Pad out the row. */
		vte_g_array_fill(rowdata->cells,
				 &screen->fill_defaults,
				 terminal->column_count);
		rowdata->soft_wrapped = 0;
		/* Repaint this row. */
		_vte_invalidate_cells(terminal,
				      0, terminal->column_count,
				      i, 1);
	}

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Clear from the cursor position to the end of the line. */
static void
vte_sequence_handler_ce (VteTerminal *terminal, GValueArray *params)
{
	VteRowData *rowdata;
	VteScreen *screen;

	screen = terminal->pvt->screen;
	/* Get the data for the row which the cursor points to. */
	rowdata = _vte_terminal_ensure_row(terminal);
	g_assert(rowdata != NULL);
	/* Remove the data at the end of the array until the current column
	 * is the end of the array. */
	if ((glong) rowdata->cells->len > screen->cursor_current.col) {
		g_array_set_size(rowdata->cells, screen->cursor_current.col);
		/* We've modified the display.  Make a note of it. */
		terminal->pvt->text_deleted_flag = TRUE;
	}
	if (screen->fill_defaults.attr.back != VTE_DEF_BG) {
		/* Add enough cells to fill out the row. */
		vte_g_array_fill(rowdata->cells,
				 &screen->fill_defaults,
				 terminal->column_count);
	}
	rowdata->soft_wrapped = 0;
	/* Repaint this row. */
	_vte_invalidate_cells(terminal,
			      screen->cursor_current.col,
			      terminal->column_count -
			      screen->cursor_current.col,
			      screen->cursor_current.row, 1);
}

/* Move the cursor to the given column (horizontal position). */
static void
vte_sequence_handler_ch (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	GValue *value;
	long val;

	screen = terminal->pvt->screen;
	/* We only care if there's a parameter in there. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = CLAMP(g_value_get_long(value),
				    0,
				    terminal->column_count - 1);
			/* Move the cursor. */
			screen->cursor_current.col = val;
			_vte_terminal_cleanup_tab_fragments_at_cursor (terminal);
		}
	}
}

/* Clear the screen and home the cursor. */
static void
vte_sequence_handler_cl (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_clear_screen (terminal);
	_vte_terminal_home_cursor (terminal);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Move the cursor to the given position. */
static void
vte_sequence_handler_cm (VteTerminal *terminal, GValueArray *params)
{
	GValue *row, *col;
	long rowval, colval, origin;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	/* We need at least two parameters. */
	rowval = colval = 0;
	if (params != NULL && params->n_values >= 1) {
		/* The first is the row, the second is the column. */
		row = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(row)) {
			if (screen->origin_mode &&
			    screen->scrolling_restricted) {
				origin = screen->scrolling_region.start;
			} else {
				origin = 0;
			}
			rowval = g_value_get_long(row) + origin;
			rowval = CLAMP(rowval, 0, terminal->row_count - 1);
		}
		if (params->n_values >= 2) {
			col = g_value_array_get_nth(params, 1);
			if (G_VALUE_HOLDS_LONG(col)) {
				colval = g_value_get_long(col);
				colval = CLAMP(colval, 0, terminal->column_count - 1);
			}
		}
	}
	screen->cursor_current.row = rowval + screen->insert_delta;
	screen->cursor_current.col = colval;
	_vte_terminal_cleanup_tab_fragments_at_cursor (terminal);
}

/* Carriage return. */
static void
vte_sequence_handler_cr (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
}

/* Restrict scrolling and updates to a subset of the visible lines. */
static void
vte_sequence_handler_cs (VteTerminal *terminal, GValueArray *params)
{
	long start=-1, end=-1, rows;
	GValue *value;
	VteScreen *screen;

	/* We require two parameters.  Anything less is a reset. */
	screen = terminal->pvt->screen;
	if ((params == NULL) || (params->n_values < 2)) {
		screen->scrolling_restricted = FALSE;
		return;
	}
	/* Extract the two values. */
	value = g_value_array_get_nth(params, 0);
	if (G_VALUE_HOLDS_LONG(value)) {
		start = g_value_get_long(value);
	}
	value = g_value_array_get_nth(params, 1);
	if (G_VALUE_HOLDS_LONG(value)) {
		end = g_value_get_long(value);
	}
	/* Catch garbage. */
	rows = terminal->row_count;
	if (start <= 0 || start >= rows) {
		start = 0;
	}
	if (end <= 0 || end >= rows) {
		end = rows - 1;
	}
	/* Set the right values. */
	screen->scrolling_region.start = start;
	screen->scrolling_region.end = end;
	screen->scrolling_restricted = TRUE;
	/* Special case -- run wild, run free. */
	if (screen->scrolling_region.start == 0 &&
	    screen->scrolling_region.end == rows - 1) {
		screen->scrolling_restricted = FALSE;
	}
	screen->cursor_current.row = screen->insert_delta + start;
	screen->cursor_current.col = 0;
}

/* Restrict scrolling and updates to a subset of the visible lines, because
 * GNU Emacs is special. */
static void
vte_sequence_handler_cS (VteTerminal *terminal, GValueArray *params)
{
	long start=0, end=terminal->row_count-1, rows;
	GValue *value;
	VteScreen *screen;

	/* We require four parameters. */
	screen = terminal->pvt->screen;
	if ((params == NULL) || (params->n_values < 2)) {
		screen->scrolling_restricted = FALSE;
		return;
	}
	/* Extract the two parameters we care about, encoded as the number
	 * of lines above and below the scrolling region, respectively. */
	value = g_value_array_get_nth(params, 1);
	if (G_VALUE_HOLDS_LONG(value)) {
		start = g_value_get_long(value);
	}
	value = g_value_array_get_nth(params, 2);
	if (G_VALUE_HOLDS_LONG(value)) {
		end -= g_value_get_long(value);
	}
	/* Set the right values. */
	screen->scrolling_region.start = start;
	screen->scrolling_region.end = end;
	screen->scrolling_restricted = TRUE;
	/* Special case -- run wild, run free. */
	rows = terminal->row_count;
	if ((screen->scrolling_region.start == 0) &&
	    (screen->scrolling_region.end == rows - 1)) {
		screen->scrolling_restricted = FALSE;
	}
	/* Clamp the cursor to the scrolling region. */
	screen->cursor_current.row = CLAMP(screen->cursor_current.row,
					   screen->insert_delta + start,
					   screen->insert_delta + end);
}

/* Clear all tab stops. */
static void
vte_sequence_handler_ct (VteTerminal *terminal, GValueArray *params)
{
	if (terminal->pvt->tabstops != NULL) {
		g_hash_table_destroy(terminal->pvt->tabstops);
		terminal->pvt->tabstops = NULL;
	}
}

/* Move the cursor to the lower left-hand corner. */
static void
vte_sequence_handler_cursor_lower_left (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	long row;
	screen = terminal->pvt->screen;
	row = MAX(0, terminal->row_count - 1);
	screen->cursor_current.row = screen->insert_delta + row;
	screen->cursor_current.col = 0;
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static void
vte_sequence_handler_cursor_next_line (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	vte_sequence_handler_DO (terminal, params);
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static void
vte_sequence_handler_cursor_preceding_line (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	vte_sequence_handler_UP (terminal, params);
}

/* Move the cursor to the given row (vertical position). */
static void
vte_sequence_handler_cv (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	GValue *value;
	long val, origin;
	screen = terminal->pvt->screen;
	/* We only care if there's a parameter in there. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			/* Move the cursor. */
			if (screen->origin_mode &&
			    screen->scrolling_restricted) {
				origin = screen->scrolling_region.start;
			} else {
				origin = 0;
			}
			val = g_value_get_long(value) + origin;
			val = CLAMP(val, 0, terminal->row_count - 1);
			screen->cursor_current.row = screen->insert_delta + val;
		}
	}
}

/* Delete a character at the current cursor position. */
static void
vte_sequence_handler_dc (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	VteRowData *rowdata;
	long col;

	screen = terminal->pvt->screen;

	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		long len;
		/* Get the data for the row which the cursor points to. */
		rowdata = _vte_ring_index(screen->row_data,
					  VteRowData *,
					  screen->cursor_current.row);
		g_assert(rowdata != NULL);
		col = screen->cursor_current.col;
		len = rowdata->cells->len;
		/* Remove the column. */
		if (col < len) {
			g_array_remove_index(rowdata->cells, col);
			if (screen->fill_defaults.attr.back != VTE_DEF_BG) {
				vte_g_array_fill (rowdata->cells,
						&screen->fill_defaults,
						terminal->column_count);
				len = terminal->column_count;
			}
			/* Repaint this row. */
			_vte_invalidate_cells(terminal,
					col, len - col,
					screen->cursor_current.row, 1);
		}
	}

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Delete N characters at the current cursor position. */
static void
vte_sequence_handler_DC (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, params, vte_sequence_handler_dc);
}

/* Delete a line at the current cursor position. */
static void
vte_sequence_handler_dl (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	long start, end, param, i;
	GValue *value;

	/* Find out which part of the screen we're messing with. */
	screen = terminal->pvt->screen;
	start = screen->cursor_current.row;
	if (screen->scrolling_restricted) {
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		end = screen->insert_delta + terminal->row_count - 1;
	}

	/* Extract any parameters. */
	param = 1;
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			param = g_value_get_long(value);
		}
	}

	/* Delete the right number of lines. */
	for (i = 0; i < param; i++) {
		/* Clear a line off the end of the region and add one to the
		 * top of the region. */
		vte_remove_line_internal(terminal, start);
		vte_insert_line_internal(terminal, end);
		/* Adjust the scrollbars if necessary. */
		_vte_terminal_adjust_adjustments(terminal);
	}

	/* Update the display. */
	_vte_terminal_scroll_region(terminal, start, end - start + 1, -param);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Delete N lines at the current cursor position. */
static void
vte_sequence_handler_DL (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_dl (terminal, params);
}

/* Cursor down, no scrolling. */
static void
vte_sequence_handler_do (VteTerminal *terminal, GValueArray *params)
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

	/* Move the cursor down. */
	screen->cursor_current.row = MIN(screen->cursor_current.row + 1, end);
}

/* Cursor down, no scrolling. */
static void
vte_sequence_handler_DO (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, params, vte_sequence_handler_do);
}

/* Start using alternate character set. */
static void
vte_sequence_handler_eA (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_ae (terminal, params);
}

/* Erase characters starting at the cursor position (overwriting N with
 * spaces, but not moving the cursor). */
static void
vte_sequence_handler_ec (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	VteRowData *rowdata;
	GValue *value;
	struct vte_charcell *cell;
	long col, i, count;

	screen = terminal->pvt->screen;

	/* If we got a parameter, use it. */
	count = 1;
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			count = g_value_get_long(value);
		}
	}

	/* Clear out the given number of characters. */
	rowdata = _vte_terminal_ensure_row(terminal);
	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		g_assert(rowdata != NULL);
		/* Write over the characters.  (If there aren't enough, we'll
		 * need to create them.) */
		for (i = 0; i < count; i++) {
			col = screen->cursor_current.col + i;
			if (col >= 0) {
				if (col < (glong) rowdata->cells->len) {
					/* Replace this cell with the current
					 * defaults. */
					cell = &g_array_index(rowdata->cells,
							      struct vte_charcell,
							      col);
					*cell = screen->color_defaults;
				} else {
					/* Add new cells until we have one here. */
					vte_g_array_fill(rowdata->cells,
							 &screen->color_defaults,
							 col);
				}
			}
		}
		/* Repaint this row. */
		_vte_invalidate_cells(terminal,
				      screen->cursor_current.col, count,
				      screen->cursor_current.row, 1);
	}

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* End insert mode. */
static void
vte_sequence_handler_ei (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->insert_mode = FALSE;
}

/* Form-feed / next-page. */
static void
vte_sequence_handler_form_feed (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_index (terminal, params);
}

/* Move from status line. */
static void
vte_sequence_handler_fs (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->status_line = FALSE;
}

/* Move the cursor to the home position. */
static void
vte_sequence_handler_ho (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_home_cursor (terminal);
}

/* Move the cursor to a specified position. */
static void
vte_sequence_handler_horizontal_and_vertical_position (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_offset(terminal, params, -1, vte_sequence_handler_cm);
}

/* Insert a character. */
static void
vte_sequence_handler_ic (VteTerminal *terminal, GValueArray *params)
{
	struct vte_cursor_position save;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	save = screen->cursor_current;

	_vte_terminal_insert_char(terminal, ' ', TRUE, TRUE);

	screen->cursor_current = save;
}

/* Insert N characters. */
static void
vte_sequence_handler_IC (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, params, vte_sequence_handler_ic);
}

/* Begin insert mode. */
static void
vte_sequence_handler_im (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->insert_mode = TRUE;
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_index (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_sf (terminal, params);
}

/* Send me a backspace key sym, will you?  Guess that the application meant
 * to send the cursor back one position. */
static void
vte_sequence_handler_kb (VteTerminal *terminal, GValueArray *params)
{
	/* Move the cursor left. */
	vte_sequence_handler_le (terminal, params);
}

/* Keypad mode end. */
static void
vte_sequence_handler_ke (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->keypad_mode = VTE_KEYMODE_NORMAL;
}

/* Keypad mode start. */
static void
vte_sequence_handler_ks (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->keypad_mode = VTE_KEYMODE_APPLICATION;
}

/* Cursor left. */
static void
vte_sequence_handler_le (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;

	screen = terminal->pvt->screen;
	if (screen->cursor_current.col > 0) {
		/* There's room to move left, so do so. */
		screen->cursor_current.col--;
		_vte_terminal_cleanup_tab_fragments_at_cursor (terminal);
	} else {
		if (terminal->pvt->flags.bw) {
			/* Wrap to the previous line. */
			screen->cursor_current.col = terminal->column_count - 1;
			if (screen->scrolling_restricted) {
				vte_sequence_handler_sr (terminal, params);
			} else {
				screen->cursor_current.row = MAX(screen->cursor_current.row - 1,
								 screen->insert_delta);
			}
		} else {
			/* Stick to the first column. */
			screen->cursor_current.col = 0;
		}
	}
}

/* Move the cursor left N columns. */
static void
vte_sequence_handler_LE (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, params, vte_sequence_handler_le);
}

/* Move the cursor to the lower left corner of the display. */
static void
vte_sequence_handler_ll (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_current.row = MAX(screen->insert_delta,
					 screen->insert_delta +
					 terminal->row_count - 1);
	screen->cursor_current.col = 0;
}

/* Blink on. */
static void
vte_sequence_handler_mb (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->defaults.attr.blink = 1;
}

/* Bold on. */
static void
vte_sequence_handler_md (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->defaults.attr.bold = 1;
	terminal->pvt->screen->defaults.attr.half = 0;
}

/* End modes. */
static void
vte_sequence_handler_me (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_set_default_attributes(terminal);
}

/* Half-bright on. */
static void
vte_sequence_handler_mh (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->defaults.attr.half = 1;
	terminal->pvt->screen->defaults.attr.bold = 0;
}

/* Invisible on. */
static void
vte_sequence_handler_mk (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->defaults.attr.invisible = 1;
}

/* Protect on. */
static void
vte_sequence_handler_mp (VteTerminal *terminal, GValueArray *params)
{
	/* unused; bug 499893
	terminal->pvt->screen->defaults.attr.protect = 1;
	 */
}

/* Reverse on. */
static void
vte_sequence_handler_mr (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->defaults.attr.reverse = 1;
}

/* Cursor right. */
static void
vte_sequence_handler_nd (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	if ((screen->cursor_current.col + 1) < terminal->column_count) {
		/* There's room to move right. */
		screen->cursor_current.col++;
	}
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static void
vte_sequence_handler_next_line (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	vte_sequence_handler_DO (terminal, params);
}

/* No-op. */
static void
vte_sequence_handler_noop (VteTerminal *terminal, GValueArray *params)
{
}

/* Carriage return command(?). */
static void
vte_sequence_handler_nw (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_cr (terminal, params);
}

/* Restore cursor (position). */
static void
vte_sequence_handler_rc (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_current.col = screen->cursor_saved.col;
	screen->cursor_current.row = CLAMP(screen->cursor_saved.row +
					   screen->insert_delta,
					   screen->insert_delta,
					   screen->insert_delta +
					   terminal->row_count - 1);
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_reverse_index (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_sr (terminal, params);
}

/* Cursor right N characters. */
static void
vte_sequence_handler_RI (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, params, vte_sequence_handler_nd);
}

/* Save cursor (position). */
static void
vte_sequence_handler_sc (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_saved.col = screen->cursor_current.col;
	screen->cursor_saved.row = CLAMP(screen->cursor_current.row -
					 screen->insert_delta,
					 0, terminal->row_count - 1);
}

/* Scroll the text down, but don't move the cursor. */
static void
vte_sequence_handler_scroll_down (VteTerminal *terminal, GValueArray *params)
{
	long val = 1;
	GValue *value;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = g_value_get_long(value);
			val = MAX(val, 1);
		}
	}

	_vte_terminal_scroll_text (terminal, val);
}

/* change color in the palette */
static void
vte_sequence_handler_change_color (VteTerminal *terminal, GValueArray *params)
{
	gchar **pairs, *str = NULL;
	GValue *value;
	GdkColor color;
	guint idx, i;

	if (params != NULL && params->n_values > 0) {
		value = g_value_array_get_nth (params, 0);

		if (G_VALUE_HOLDS_STRING (value))
			str = g_value_dup_string (value);
		else if (G_VALUE_HOLDS_POINTER (value))
			str = vte_ucs4_to_utf8 (terminal, g_value_get_pointer (value));

		if (! str)
			return;

		pairs = g_strsplit (str, ";", 0);
		if (! pairs) {
			g_free (str);
			return;
		}

		for (i = 0; pairs[i] && pairs[i + 1]; i += 2) {
			idx = strtoul (pairs[i], (char **) NULL, 10);

			if (idx >= VTE_DEF_FG)
				continue;

			if (vte_parse_color (pairs[i + 1], &color)) {
				terminal->pvt->palette[idx].red = color.red;
				terminal->pvt->palette[idx].green = color.green;
				terminal->pvt->palette[idx].blue = color.blue;
			} else if (strcmp (pairs[i + 1], "?") == 0) {
				gchar buf[128];
				g_snprintf (buf, sizeof (buf),
					    _VTE_CAP_OSC "4;%u;rgb:%04x/%04x/%04x" BEL, idx,
					    terminal->pvt->palette[idx].red,
					    terminal->pvt->palette[idx].green,
					    terminal->pvt->palette[idx].blue);
				vte_terminal_feed_child (terminal, buf, -1);
			}
		}

		g_free (str);
		g_strfreev (pairs);

		/* emit the refresh as the palette has changed and previous
		 * renders need to be updated. */
		vte_terminal_emit_refresh_window (terminal);
	}
}

/* Scroll the text up, but don't move the cursor. */
static void
vte_sequence_handler_scroll_up (VteTerminal *terminal, GValueArray *params)
{
	long val = 1;
	GValue *value;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = g_value_get_long(value);
			val = MAX(val, 1);
		}
	}

	_vte_terminal_scroll_text (terminal, -val);
}

/* Standout end. */
static void
vte_sequence_handler_se (VteTerminal *terminal, GValueArray *params)
{
	char *standout;

	/* Standout may be mapped to another attribute, so attempt to do
	 * the Right Thing here.
	 *
	 * If the standout sequence is the same as another sequence, do what
	 * we'd do for that other sequence instead. */

	standout = _vte_termcap_find_string(terminal->pvt->termcap,
					    terminal->pvt->emulation,
					    "so");
	g_assert(standout != NULL);

	if (vte_terminal_termcap_string_same_as_for (terminal, standout, "mb") /* blink */   ||
	    vte_terminal_termcap_string_same_as_for (terminal, standout, "md") /* bold */    ||
	    vte_terminal_termcap_string_same_as_for (terminal, standout, "mh") /* half */    ||
	    vte_terminal_termcap_string_same_as_for (terminal, standout, "mr") /* reverse */ ||
	    vte_terminal_termcap_string_same_as_for (terminal, standout, "us") /* underline */)
	{
		vte_sequence_handler_me (terminal, params);
	} else {
		/* Otherwise just set standout mode. */
		terminal->pvt->screen->defaults.attr.standout = 0;
	}

	g_free(standout);
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_sf (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_cursor_down (terminal);
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_SF (VteTerminal *terminal, GValueArray *params)
{
	/* XXX implement this directly in _vte_terminal_cursor_down */
	vte_sequence_handler_multiple(terminal, params, vte_sequence_handler_sf);
}

/* Standout start. */
static void
vte_sequence_handler_so (VteTerminal *terminal, GValueArray *params)
{
	char *standout;

	/* Standout may be mapped to another attribute, so attempt to do
	 * the Right Thing here.
	 *
	 * If the standout sequence is the same as another sequence, do what
	 * we'd do for that other sequence instead. */

	standout = _vte_termcap_find_string(terminal->pvt->termcap,
					    terminal->pvt->emulation,
					    "so");
	g_assert(standout != NULL);

	if (vte_terminal_termcap_string_same_as_for (terminal, standout, "mb") /* blink */)
		vte_sequence_handler_mb (terminal, params);
	else if (vte_terminal_termcap_string_same_as_for (terminal, standout, "md") /* bold */)
		vte_sequence_handler_md (terminal, params);
	else if (vte_terminal_termcap_string_same_as_for (terminal, standout, "mh") /* half */)
		vte_sequence_handler_mh (terminal, params);
	else if (vte_terminal_termcap_string_same_as_for (terminal, standout, "mr") /* reverse */)
		vte_sequence_handler_mr (terminal, params);
	else if (vte_terminal_termcap_string_same_as_for (terminal, standout, "us") /* underline */)
		vte_sequence_handler_us (terminal, params);
	else {
		/* Otherwise just set standout mode. */
		terminal->pvt->screen->defaults.attr.standout = 1;
	}

	g_free(standout);
}

/* Cursor up, scrolling if need be. */
static void
vte_sequence_handler_sr (VteTerminal *terminal, GValueArray *params)
{
	long start, end;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->scrolling_region.start + screen->insert_delta;
		end = screen->scrolling_region.end + screen->insert_delta;
	} else {
		start = terminal->pvt->screen->insert_delta;
		end = start + terminal->row_count - 1;
	}

	if (screen->cursor_current.row == start) {
		/* If we're at the top of the scrolling region, add a
		 * line at the top to scroll the bottom off. */
		vte_remove_line_internal(terminal, end);
		vte_insert_line_internal(terminal, start);
		/* Update the display. */
		_vte_terminal_scroll_region(terminal, start, end - start + 1, 1);
		_vte_invalidate_cells(terminal,
				      0, terminal->column_count,
				      start, 2);
	} else {
		/* Otherwise, just move the cursor up. */
		screen->cursor_current.row--;
	}
	/* Adjust the scrollbars if necessary. */
	_vte_terminal_adjust_adjustments(terminal);
	/* We modified the display, so make a note of it. */
	terminal->pvt->text_modified_flag = TRUE;
}

/* Cursor up, with scrolling. */
static void
vte_sequence_handler_SR (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, params, vte_sequence_handler_sr);
}

/* Set tab stop in the current column. */
static void
vte_sequence_handler_st (VteTerminal *terminal, GValueArray *params)
{
	if (terminal->pvt->tabstops == NULL) {
		terminal->pvt->tabstops = g_hash_table_new(NULL, NULL);
	}
	_vte_terminal_set_tabstop(terminal,
				 terminal->pvt->screen->cursor_current.col);
}

/* Tab. */
static void
vte_sequence_handler_ta (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	long newcol, col;

	/* Calculate which column is the next tab stop. */
	screen = terminal->pvt->screen;
	newcol = col = screen->cursor_current.col;

	g_assert (col >= 0);

	if (terminal->pvt->tabstops != NULL) {
		/* Find the next tabstop. */
		for (newcol++; newcol < VTE_TAB_MAX; newcol++) {
			if (_vte_terminal_get_tabstop(terminal, newcol)) {
				break;
			}
		}
	}

	/* If we have no tab stops or went past the end of the line, stop
	 * at the right-most column. */
	if (newcol >= terminal->column_count) {
		newcol = terminal->column_count - 1;
	}

	/* but make sure we don't move cursor back (bug #340631) */
	if (col < newcol) {
		VteRowData *rowdata = _vte_terminal_ensure_row (terminal);

		/* Smart tab handling: bug 353610
		 *
		 * If we currently don't have any cells in the space this
		 * tab creates, we try to make the tab character copyable,
		 * by appending a single tab char with lots of fragment
		 * cells following it.
		 *
		 * Otherwise, just append empty cells that will show up
		 * as a space each.
		 */

		/* Get rid of trailing empty cells: bug 545924 */
		if ((glong) rowdata->cells->len > col)
		{
			struct vte_charcell *cell;
			guint i;
			for (i = rowdata->cells->len; (glong) i > col; i--) {
				cell = &g_array_index(rowdata->cells,
						      struct vte_charcell, i - 1);
				if (cell->attr.fragment || cell->c != 0)
					break;
			}
			g_array_set_size(rowdata->cells, i);
		}

		if ((glong) rowdata->cells->len <= col)
		  {
		    struct vte_charcell cell;

		    vte_g_array_fill (rowdata->cells,
				      &screen->fill_defaults,
				      col);

		    cell.attr = screen->fill_defaults.attr;
		    cell.attr.invisible = 1; /* FIXME: bug 499944 */
		    cell.attr.columns = newcol - col;
		    if (cell.attr.columns != newcol - col)
		      {
		        /* number of columns doesn't fit one cell. skip
			 * fancy tabs
			 */
		       goto fallback_tab;
		      }
		    cell.c = '\t';
		    g_array_append_vals(rowdata->cells, &cell, 1);

		    cell.attr = screen->fill_defaults.attr;
		    cell.attr.fragment = 1;
		    vte_g_array_fill (rowdata->cells,
				      &cell,
				newcol);
		  }
		else
		  {
		  fallback_tab:
		    vte_g_array_fill (rowdata->cells,
				      &screen->fill_defaults,
				      newcol);
		  }

		_vte_invalidate_cells (terminal,
				screen->cursor_current.col,
				newcol - screen->cursor_current.col,
				screen->cursor_current.row, 1);
		screen->cursor_current.col = newcol;
	}
}

/* Clear tabs selectively. */
static void
vte_sequence_handler_tab_clear (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	long param = 0;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			param = g_value_get_long(value);
		}
	}
	if (param == 0) {
		_vte_terminal_clear_tabstop(terminal,
					   terminal->pvt->screen->cursor_current.col);
	} else
	if (param == 3) {
		if (terminal->pvt->tabstops != NULL) {
			g_hash_table_destroy(terminal->pvt->tabstops);
			terminal->pvt->tabstops = NULL;
		}
	}
}

/* Move to status line. */
static void
vte_sequence_handler_ts (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->status_line = TRUE;
	terminal->pvt->screen->status_line_changed = TRUE;
	g_string_truncate(terminal->pvt->screen->status_line_contents, 0);
}

/* Underline this character and move right. */
static void
vte_sequence_handler_uc (VteTerminal *terminal, GValueArray *params)
{
	struct vte_charcell *cell;
	int column;
	VteScreen *screen;

	screen = terminal->pvt->screen;
	column = screen->cursor_current.col;
	cell = vte_terminal_find_charcell(terminal,
					  column,
					  screen->cursor_current.row);
	while ((cell != NULL) && (cell->attr.fragment) && (column > 0)) {
		column--;
		cell = vte_terminal_find_charcell(terminal,
						  column,
						  screen->cursor_current.row);
	}
	if (cell != NULL) {
		/* Set this character to be underlined. */
		cell->attr.underline = 1;
		/* Cause the character to be repainted. */
		_vte_invalidate_cells(terminal,
				      column, cell->attr.columns,
				      screen->cursor_current.row, 1);
		/* Move the cursor right. */
		vte_sequence_handler_nd (terminal, params);
	}

	/* We've modified the display without changing the text.  Make a note
	 * of it. */
	terminal->pvt->text_modified_flag = TRUE;
}

/* Underline end. */
static void
vte_sequence_handler_ue (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->defaults.attr.underline = 0;
}

/* Cursor up, no scrolling. */
static void
vte_sequence_handler_up (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	long start, end;

	screen = terminal->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = start + terminal->row_count - 1;
	}

	screen->cursor_current.row = MAX(screen->cursor_current.row - 1, start);
}

/* Cursor up N lines, no scrolling. */
static void
vte_sequence_handler_UP (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, params, vte_sequence_handler_up);
}

/* Underline start. */
static void
vte_sequence_handler_us (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->defaults.attr.underline = 1;
}

/* Visible bell. */
static void
vte_sequence_handler_vb (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_visible_beep (terminal);
}

/* Cursor visible. */
static void
vte_sequence_handler_ve (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->cursor_visible = TRUE;
}

/* Vertical tab. */
static void
vte_sequence_handler_vertical_tab (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_index (terminal, params);
}

/* Cursor invisible. */
static void
vte_sequence_handler_vi (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->cursor_visible = FALSE;
}

/* Cursor standout. */
static void
vte_sequence_handler_vs (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->cursor_visible = TRUE; /* FIXME: should be *more*
						 visible. */
}

/* Handle ANSI color setting and related stuffs (SGR). */
static void
vte_sequence_handler_character_attributes (VteTerminal *terminal, GValueArray *params)
{
	unsigned int i;
	GValue *value;
	long param;
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
			_vte_terminal_set_default_attributes(terminal);
			break;
		case 1:
			terminal->pvt->screen->defaults.attr.bold = 1;
			terminal->pvt->screen->defaults.attr.half = 0;
			break;
		case 2:
			terminal->pvt->screen->defaults.attr.half = 1;
			terminal->pvt->screen->defaults.attr.bold = 0;
			break;
		case 4:
			terminal->pvt->screen->defaults.attr.underline = 1;
			break;
		case 5:
			terminal->pvt->screen->defaults.attr.blink = 1;
			break;
		case 7:
			terminal->pvt->screen->defaults.attr.reverse = 1;
			break;
		case 8:
			terminal->pvt->screen->defaults.attr.invisible = 1;
			break;
		case 9:
			terminal->pvt->screen->defaults.attr.strikethrough = 1;
			break;
		case 21: /* Error in old versions of linux console. */
		case 22: /* ECMA 48. */
			terminal->pvt->screen->defaults.attr.bold = 0;
			terminal->pvt->screen->defaults.attr.half = 0;
			break;
		case 24:
			terminal->pvt->screen->defaults.attr.underline = 0;
			break;
		case 25:
			terminal->pvt->screen->defaults.attr.blink = 0;
			break;
		case 27:
			terminal->pvt->screen->defaults.attr.reverse = 0;
			break;
		case 28:
			terminal->pvt->screen->defaults.attr.invisible = 0;
			break;
		case 29:
			terminal->pvt->screen->defaults.attr.strikethrough = 0;
			break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
			terminal->pvt->screen->defaults.attr.fore = param - 30;
			break;
		case 38:
		{
			GValue *value1;
			long param1;
			/* The format looks like: ^[[38;5;COLORNUMBERm,
			   so look for COLORNUMBER here. */
			if ((i + 2) < params->n_values){
				value1 = g_value_array_get_nth(params, i + 2);
				if (!G_VALUE_HOLDS_LONG(value1)) {
					break;
				}
				param1 = g_value_get_long(value1);
				terminal->pvt->screen->defaults.attr.fore = param1;
				i += 2;
			}
			break;
		}
		case 39:
			/* default foreground, no underscore */
			terminal->pvt->screen->defaults.attr.fore = VTE_DEF_FG;
			/* By ECMA 48, this underline off has no business
			   being here, but the Linux console specifies it. */
			terminal->pvt->screen->defaults.attr.underline = 0;
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			terminal->pvt->screen->defaults.attr.back = param - 40;
			break;
		case 48:
		{
			GValue *value1;
			long param1;
			/* The format looks like: ^[[48;5;COLORNUMBERm,
			   so look for COLORNUMBER here. */
			if ((i + 2) < params->n_values){
				value1 = g_value_array_get_nth(params, i + 2);
				if (!G_VALUE_HOLDS_LONG(value1)) {
					break;
				}
				param1 = g_value_get_long(value1);
				terminal->pvt->screen->defaults.attr.back = param1;
				i += 2;
			}
			break;
		}
		case 49:
			/* default background */
			terminal->pvt->screen->defaults.attr.back = VTE_DEF_BG;
			break;
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
			terminal->pvt->screen->defaults.attr.fore = param - 90 + VTE_COLOR_BRIGHT_OFFSET;
			break;
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
			terminal->pvt->screen->defaults.attr.back = param - 100 + VTE_COLOR_BRIGHT_OFFSET;
			break;
		}
	}
	/* If we had no parameters, default to the defaults. */
	if (i == 0) {
		_vte_terminal_set_default_attributes(terminal);
	}
	/* Save the new colors. */
	terminal->pvt->screen->color_defaults.attr.fore =
		terminal->pvt->screen->defaults.attr.fore;
	terminal->pvt->screen->color_defaults.attr.back =
		terminal->pvt->screen->defaults.attr.back;
	terminal->pvt->screen->fill_defaults.attr.fore =
		terminal->pvt->screen->defaults.attr.fore;
	terminal->pvt->screen->fill_defaults.attr.back =
		terminal->pvt->screen->defaults.attr.back;
}

/* Move the cursor to the given column, 1-based. */
static void
vte_sequence_handler_cursor_character_absolute (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	GValue *value;
	long val;

	screen = terminal->pvt->screen;

        val = 0;
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = CLAMP(g_value_get_long(value),
				    1, terminal->column_count) - 1;
		}
	}

        screen->cursor_current.col = val;
	_vte_terminal_cleanup_tab_fragments_at_cursor (terminal);
}

/* Move the cursor to the given position, 1-based. */
static void
vte_sequence_handler_cursor_position (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_offset(terminal, params, -1, vte_sequence_handler_cm);
}

/* Request terminal attributes. */
static void
vte_sequence_handler_request_terminal_parameters (VteTerminal *terminal, GValueArray *params)
{
	vte_terminal_feed_child(terminal, "\e[?x", -1);
}

/* Request terminal attributes. */
static void
vte_sequence_handler_return_terminal_status (VteTerminal *terminal, GValueArray *params)
{
	vte_terminal_feed_child(terminal, "", 0);
}

/* Send primary device attributes. */
static void
vte_sequence_handler_send_primary_device_attributes (VteTerminal *terminal, GValueArray *params)
{
	/* Claim to be a VT220 with only national character set support. */
	vte_terminal_feed_child(terminal, "\e[?62;9;c", -1);
}

/* Send terminal ID. */
static void
vte_sequence_handler_return_terminal_id (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_send_primary_device_attributes (terminal, params);
}

/* Send secondary device attributes. */
static void
vte_sequence_handler_send_secondary_device_attributes (VteTerminal *terminal, GValueArray *params)
{
	char **version;
	char buf[128];
	long ver = 0, i;
	/* Claim to be a VT220, more or less.  The '>' in the response appears
	 * to be undocumented. */
	version = g_strsplit(VERSION, ".", 0);
	if (version != NULL) {
		for (i = 0; version[i] != NULL; i++) {
			ver = ver * 100;
			ver += atol(version[i]);
		}
		g_strfreev(version);
	}
	g_snprintf(buf, sizeof (buf), _VTE_CAP_ESC "[>1;%ld;0c", ver);
	vte_terminal_feed_child(terminal, buf, -1);
}

/* Set one or the other. */
static void
vte_sequence_handler_set_icon_title (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_set_title_internal(terminal, params, TRUE, FALSE);
}

static void
vte_sequence_handler_set_window_title (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_set_title_internal(terminal, params, FALSE, TRUE);
}

/* Set both the window and icon titles to the same string. */
static void
vte_sequence_handler_set_icon_and_window_title (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_set_title_internal(terminal, params, TRUE, TRUE);
}

/* Restrict the scrolling region. */
static void
vte_sequence_handler_set_scrolling_region (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_offset(terminal, params, -1, vte_sequence_handler_cs);
}

/* Set the application or normal keypad. */
static void
vte_sequence_handler_application_keypad (VteTerminal *terminal, GValueArray *params)
{
	_vte_debug_print(VTE_DEBUG_KEYBOARD,
			"Entering application keypad mode.\n");
	terminal->pvt->keypad_mode = VTE_KEYMODE_APPLICATION;
}

static void
vte_sequence_handler_normal_keypad (VteTerminal *terminal, GValueArray *params)
{
	_vte_debug_print(VTE_DEBUG_KEYBOARD,
			"Leaving application keypad mode.\n");
	terminal->pvt->keypad_mode = VTE_KEYMODE_NORMAL;
}

/* Move the cursor. */
static void
vte_sequence_handler_character_position_absolute (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_offset(terminal, params, -1, vte_sequence_handler_ch);
}
static void
vte_sequence_handler_line_position_absolute (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_offset(terminal, params, -1, vte_sequence_handler_cv);
}

/* Set certain terminal attributes. */
static void
vte_sequence_handler_set_mode (VteTerminal *terminal, GValueArray *params)
{
	guint i;
	long setting;
	GValue *value;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		vte_sequence_handler_set_mode_internal(terminal, setting, TRUE);
	}
}

/* Unset certain terminal attributes. */
static void
vte_sequence_handler_reset_mode (VteTerminal *terminal, GValueArray *params)
{
	guint i;
	long setting;
	GValue *value;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		vte_sequence_handler_set_mode_internal(terminal, setting, FALSE);
	}
}

/* Set certain terminal attributes. */
static void
vte_sequence_handler_decset (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	long setting;
	guint i;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		vte_sequence_handler_decset_internal(terminal, setting, FALSE, FALSE, TRUE);
	}
}

/* Unset certain terminal attributes. */
static void
vte_sequence_handler_decreset (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	long setting;
	guint i;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		vte_sequence_handler_decset_internal(terminal, setting, FALSE, FALSE, FALSE);
	}
}

/* Erase a specified number of characters. */
static void
vte_sequence_handler_erase_characters (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_ec (terminal, params);
}

/* Erase certain lines in the display. */
static void
vte_sequence_handler_erase_in_display (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	long param;
	guint i;
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
		vte_sequence_handler_cd (terminal, NULL);
		break;
	case 1:
		/* Clear above the current line. */
		_vte_terminal_clear_above_current (terminal);
		/* Clear everything to the left of the cursor, too. */
		/* FIXME: vttest. */
		vte_sequence_handler_cb (terminal, NULL);
		break;
	case 2:
		/* Clear the entire screen. */
		_vte_terminal_clear_screen (terminal);
		break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Erase certain parts of the current line in the display. */
static void
vte_sequence_handler_erase_in_line (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	long param;
	guint i;
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
		vte_sequence_handler_ce (terminal, NULL);
		break;
	case 1:
		/* Clear to start of the line. */
		vte_sequence_handler_cb (terminal, NULL);
		break;
	case 2:
		/* Clear the entire line. */
		_vte_terminal_clear_current_line (terminal);
		break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Perform a full-bore reset. */
static void
vte_sequence_handler_full_reset (VteTerminal *terminal, GValueArray *params)
{
	vte_terminal_reset(terminal, TRUE, TRUE);
}

/* Insert a specified number of blank characters. */
static void
vte_sequence_handler_insert_blank_characters (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_IC (terminal, params);
}

/* Insert a certain number of lines below the current cursor. */
static void
vte_sequence_handler_insert_lines (VteTerminal *terminal, GValueArray *params)
{
	VteRowData *rowdata;
	GValue *value;
	VteScreen *screen;
	long param, end, row;
	int i;
	screen = terminal->pvt->screen;
	/* The default is one. */
	param = 1;
	/* Extract any parameters. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			param = g_value_get_long(value);
		}
	}
	/* Find the region we're messing with. */
	row = screen->cursor_current.row;
	if (screen->scrolling_restricted) {
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		end = screen->insert_delta + terminal->row_count - 1;
	}
	/* Insert the new lines at the cursor. */
	for (i = 0; i < param; i++) {
		/* Clear a line off the end of the region and add one to the
		 * top of the region. */
		vte_remove_line_internal(terminal, end);
		vte_insert_line_internal(terminal, row);
		/* Get the data for the new row. */
		rowdata = _vte_ring_index(screen->row_data, VteRowData *, row);
		g_assert(rowdata != NULL);
		/* Add enough cells to it so that it has the default colors. */
		vte_g_array_fill(rowdata->cells,
				 &screen->fill_defaults,
				 terminal->column_count);
	}
	/* Update the display. */
	_vte_terminal_scroll_region(terminal, row, end - row + 1, param);
	/* Adjust the scrollbars if necessary. */
	_vte_terminal_adjust_adjustments(terminal);
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_inserted_flag = TRUE;
}

/* Delete certain lines from the scrolling region. */
static void
vte_sequence_handler_delete_lines (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	VteRowData *rowdata;
	VteScreen *screen;
	long param, end, row;
	int i;

	screen = terminal->pvt->screen;
	/* The default is one. */
	param = 1;
	/* Extract any parameters. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			param = g_value_get_long(value);
		}
	}
	/* Find the region we're messing with. */
	row = screen->cursor_current.row;
	if (screen->scrolling_restricted) {
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		end = screen->insert_delta + terminal->row_count - 1;
	}
	/* Clear them from below the current cursor. */
	for (i = 0; i < param; i++) {
		/* Insert a line at the end of the region and remove one from
		 * the top of the region. */
		vte_remove_line_internal(terminal, row);
		vte_insert_line_internal(terminal, end);
		/* Get the data for the new row. */
		rowdata = _vte_ring_index(screen->row_data, VteRowData *, end);
		g_assert(rowdata != NULL);
		/* Add enough cells to it so that it has the default colors. */
		vte_g_array_fill(rowdata->cells,
				 &screen->fill_defaults,
				 terminal->column_count);
	}
	/* Update the display. */
	_vte_terminal_scroll_region(terminal, row, end - row + 1, -param);
	/* Adjust the scrollbars if necessary. */
	_vte_terminal_adjust_adjustments(terminal);
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Set the terminal encoding. */
static void
vte_sequence_handler_local_charset (VteTerminal *terminal, GValueArray *params)
{
	G_CONST_RETURN char *locale_encoding;
	g_get_charset(&locale_encoding);
	vte_terminal_set_encoding(terminal, locale_encoding);
}

static void
vte_sequence_handler_utf_8_charset (VteTerminal *terminal, GValueArray *params)
{
	vte_terminal_set_encoding(terminal, "UTF-8");
}

/* Device status reports. The possible reports are the cursor position and
 * whether or not we're okay. */
static void
vte_sequence_handler_device_status_report (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param;
	char buf[128];

	screen = terminal->pvt->screen;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			param = g_value_get_long(value);
			switch (param) {
			case 5:
				/* Send a thumbs-up sequence. */
				vte_terminal_feed_child(terminal, _VTE_CAP_CSI "0n", -1);
				break;
			case 6:
				/* Send the cursor position. */
				g_snprintf(buf, sizeof(buf),
					   _VTE_CAP_CSI "%ld;%ldR",
					   screen->cursor_current.row + 1 -
					   screen->insert_delta,
					   screen->cursor_current.col + 1);
				vte_terminal_feed_child(terminal, buf, -1);
				break;
			default:
				break;
			}
		}
	}
}

/* DEC-style device status reports. */
static void
vte_sequence_handler_dec_device_status_report (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param;
	char buf[128];

	screen = terminal->pvt->screen;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			param = g_value_get_long(value);
			switch (param) {
			case 6:
				/* Send the cursor position. */
				g_snprintf(buf, sizeof(buf),
					   _VTE_CAP_CSI "?%ld;%ldR",
					   screen->cursor_current.row + 1 -
					   screen->insert_delta,
					   screen->cursor_current.col + 1);
				vte_terminal_feed_child(terminal, buf, -1);
				break;
			case 15:
				/* Send printer status -- 10 = ready,
				 * 11 = not ready.  We don't print. */
				vte_terminal_feed_child(terminal, _VTE_CAP_CSI "?11n", -1);
				break;
			case 25:
				/* Send UDK status -- 20 = locked,
				 * 21 = not locked.  I don't even know what
				 * that means, but punt anyway. */
				vte_terminal_feed_child(terminal, _VTE_CAP_CSI "?20n", -1);
				break;
			case 26:
				/* Send keyboard status.  50 = no locator. */
				vte_terminal_feed_child(terminal, _VTE_CAP_CSI "?50n", -1);
				break;
			default:
				break;
			}
		}
	}
}

/* Restore a certain terminal attribute. */
static void
vte_sequence_handler_restore_mode (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	long setting;
	guint i;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		vte_sequence_handler_decset_internal(terminal, setting, TRUE, FALSE, FALSE);
	}
}

/* Save a certain terminal attribute. */
static void
vte_sequence_handler_save_mode (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	long setting;
	guint i;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		vte_sequence_handler_decset_internal(terminal, setting, FALSE, TRUE, FALSE);
	}
}

/* Perform a screen alignment test -- fill all visible cells with the
 * letter "E". */
static void
vte_sequence_handler_screen_alignment_test (VteTerminal *terminal, GValueArray *params)
{
	long row;
	VteRowData *rowdata, *old_row;
	VteScreen *screen;
	struct vte_charcell cell;

	screen = terminal->pvt->screen;

	for (row = terminal->pvt->screen->insert_delta;
	     row < terminal->pvt->screen->insert_delta + terminal->row_count;
	     row++) {
		/* Find this row. */
		old_row = terminal->pvt->free_row;
		while (_vte_ring_next(screen->row_data) <= row) {
			if (old_row) {
				rowdata = _vte_reset_row_data (terminal, old_row, FALSE);
			} else {
				rowdata = _vte_new_row_data(terminal);
			}
			old_row = _vte_ring_append(screen->row_data, rowdata);
		}
		terminal->pvt->free_row = old_row;
		_vte_terminal_adjust_adjustments(terminal);
		rowdata = _vte_ring_index(screen->row_data, VteRowData *, row);
		g_assert(rowdata != NULL);
		/* Clear this row. */
		if (rowdata->cells->len > 0) {
			g_array_set_size(rowdata->cells, 0);
		}
		_vte_terminal_emit_text_deleted(terminal);
		/* Fill this row. */
		cell.c = 'E';
		cell.attr = screen->basic_defaults.attr;
		cell.attr.columns = 1;
		vte_g_array_fill(rowdata->cells, &cell, terminal->column_count);
		_vte_terminal_emit_text_inserted(terminal);
	}
	_vte_invalidate_all(terminal);

	/* We modified the display, so make a note of it for completeness. */
	terminal->pvt->text_modified_flag = TRUE;
}

/* Perform a soft reset. */
static void
vte_sequence_handler_soft_reset (VteTerminal *terminal, GValueArray *params)
{
	vte_terminal_reset(terminal, FALSE, FALSE);
}

/* Window manipulation control sequences.  Most of these are considered
 * bad ideas, but they're implemented as signals which the application
 * is free to ignore, so they're harmless. */
static void
vte_sequence_handler_window_manipulation (VteTerminal *terminal, GValueArray *params)
{
	GdkScreen *gscreen;
	VteScreen *screen;
	GValue *value;
	GtkWidget *widget;
	char buf[128];
	long param, arg1, arg2;
	gint width, height;
	guint i;

	widget = &terminal->widget;
	screen = terminal->pvt->screen;

	for (i = 0; ((params != NULL) && (i < params->n_values)); i++) {
		arg1 = arg2 = -1;
		if (i + 1 < params->n_values) {
			value = g_value_array_get_nth(params, i + 1);
			if (G_VALUE_HOLDS_LONG(value)) {
				arg1 = g_value_get_long(value);
			}
		}
		if (i + 2 < params->n_values) {
			value = g_value_array_get_nth(params, i + 2);
			if (G_VALUE_HOLDS_LONG(value)) {
				arg2 = g_value_get_long(value);
			}
		}
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		param = g_value_get_long(value);
		switch (param) {
		case 1:
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Deiconifying window.\n");
			vte_terminal_emit_deiconify_window(terminal);
			break;
		case 2:
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Iconifying window.\n");
			vte_terminal_emit_iconify_window(terminal);
			break;
		case 3:
			if ((arg1 != -1) && (arg2 != -2)) {
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Moving window to "
						"%ld,%ld.\n", arg1, arg2);
				vte_terminal_emit_move_window(terminal,
							      arg1, arg2);
				i += 2;
			}
			break;
		case 4:
			if ((arg1 != -1) && (arg2 != -1)) {
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Resizing window "
						"(to %ldx%ld pixels).\n",
						arg2, arg1);
				vte_terminal_emit_resize_window(terminal,
								arg2 +
								VTE_PAD_WIDTH * 2,
								arg1 +
								VTE_PAD_WIDTH * 2);
				i += 2;
			}
			break;
		case 5:
			_vte_debug_print(VTE_DEBUG_PARSE, "Raising window.\n");
			vte_terminal_emit_raise_window(terminal);
			break;
		case 6:
			_vte_debug_print(VTE_DEBUG_PARSE, "Lowering window.\n");
			vte_terminal_emit_lower_window(terminal);
			break;
		case 7:
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Refreshing window.\n");
			_vte_invalidate_all(terminal);
			vte_terminal_emit_refresh_window(terminal);
			break;
		case 8:
			if ((arg1 != -1) && (arg2 != -1)) {
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Resizing window "
						"(to %ld columns, %ld rows).\n",
						arg2, arg1);
				vte_terminal_emit_resize_window(terminal,
								arg2 * terminal->char_width +
								VTE_PAD_WIDTH * 2,
								arg1 * terminal->char_height +
								VTE_PAD_WIDTH * 2);
				i += 2;
			}
			break;
		case 9:
			switch (arg1) {
			case 0:
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Restoring window.\n");
				vte_terminal_emit_restore_window(terminal);
				break;
			case 1:
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Maximizing window.\n");
				vte_terminal_emit_maximize_window(terminal);
				break;
			default:
				break;
			}
			i++;
			break;
		case 11:
			/* If we're unmapped, then we're iconified. */
			g_snprintf(buf, sizeof(buf),
				   _VTE_CAP_CSI "%dt",
				   1 + !GTK_WIDGET_MAPPED(widget));
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting window state %s.\n",
					GTK_WIDGET_MAPPED(widget) ?
					"non-iconified" : "iconified");
			vte_terminal_feed_child(terminal, buf, -1);
			break;
		case 13:
			/* Send window location, in pixels. */
			gdk_window_get_origin(widget->window,
					      &width, &height);
			g_snprintf(buf, sizeof(buf),
				   _VTE_CAP_CSI "3;%d;%dt",
				   width + VTE_PAD_WIDTH, height + VTE_PAD_WIDTH);
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting window location"
					"(%d++,%d++).\n",
					width, height);
			vte_terminal_feed_child(terminal, buf, -1);
			break;
		case 14:
			/* Send window size, in pixels. */
			g_snprintf(buf, sizeof(buf),
				   _VTE_CAP_CSI "4;%d;%dt",
				   widget->allocation.height - 2 * VTE_PAD_WIDTH,
				   widget->allocation.width - 2 * VTE_PAD_WIDTH);
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting window size "
					"(%dx%dn",
					width - 2 * VTE_PAD_WIDTH,
					height - 2 * VTE_PAD_WIDTH);
			vte_terminal_feed_child(terminal, buf, -1);
			break;
		case 18:
			/* Send widget size, in cells. */
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting widget size.\n");
			g_snprintf(buf, sizeof(buf),
				   _VTE_CAP_CSI "8;%ld;%ldt",
				   terminal->row_count,
				   terminal->column_count);
			vte_terminal_feed_child(terminal, buf, -1);
			break;
		case 19:
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting screen size.\n");
			gscreen = gtk_widget_get_screen(widget);
			height = gdk_screen_get_height(gscreen);
			width = gdk_screen_get_width(gscreen);
			g_snprintf(buf, sizeof(buf),
				   _VTE_CAP_CSI "9;%ld;%ldt",
				   height / terminal->char_height,
				   width / terminal->char_width);
			vte_terminal_feed_child(terminal, buf, -1);
			break;
		case 20:
			/* Report the icon title. */
			_vte_debug_print(VTE_DEBUG_PARSE,
				"Reporting icon title.\n");
			g_snprintf (buf, sizeof (buf),
				    _VTE_CAP_OSC "L%s" _VTE_CAP_ST,
				    terminal->icon_title);
			vte_terminal_feed_child(terminal, buf, -1);
			break;
		case 21:
			/* Report the window title. */
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting window title.\n");
			g_snprintf (buf, sizeof (buf),
				    _VTE_CAP_OSC "l%s" _VTE_CAP_ST,
				    terminal->window_title);
			vte_terminal_feed_child(terminal, buf, -1);
			break;
		default:
			if (param >= 24) {
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Resizing to %ld rows.\n",
					       	param);
				/* Resize to the specified number of
				 * rows. */
				vte_terminal_emit_resize_window(terminal,
								terminal->column_count * terminal->char_width +
								VTE_PAD_WIDTH * 2,
								param * terminal->char_height +
								VTE_PAD_WIDTH * 2);
			}
			break;
		}
	}
}

/* Change the color of the cursor */
static void
vte_sequence_handler_change_cursor_color (VteTerminal *terminal, GValueArray *params)
{
	gchar *name = NULL;
	GValue *value;
	GdkColor color;

	if (params != NULL && params->n_values > 0) {
		value = g_value_array_get_nth (params, 0);

		if (G_VALUE_HOLDS_STRING (value))
			name = g_value_dup_string (value);
		else if (G_VALUE_HOLDS_POINTER (value))
			name = vte_ucs4_to_utf8 (terminal, g_value_get_pointer (value));

		if (! name)
			return;

		if (vte_parse_color (name, &color))
			vte_terminal_set_color_cursor (terminal, &color);
		else if (strcmp (name, "?") == 0) {
			gchar buf[128];
			g_snprintf (buf, sizeof (buf),
				    _VTE_CAP_OSC "12;rgb:%04x/%04x/%04x" BEL,
				    terminal->pvt->palette[VTE_CUR_BG].red,
				    terminal->pvt->palette[VTE_CUR_BG].green,
				    terminal->pvt->palette[VTE_CUR_BG].blue);
			vte_terminal_feed_child (terminal, buf, -1);
		}

		g_free (name);
	}
}


/* Lookup tables */

#define VTE_SEQUENCE_HANDLER(name) name

static const struct vteseq_2_struct *
vteseq_2_lookup (register const char *str, register unsigned int len);
#include"vteseq-2.c"

static const struct vteseq_n_struct *
vteseq_n_lookup (register const char *str, register unsigned int len);
#include"vteseq-n.c"

#undef VTE_SEQUENCE_HANDLER

static VteTerminalSequenceHandler
_vte_sequence_get_handler (const char *name)
{
	int len = strlen (name);

	if (G_UNLIKELY (len < 2)) {
		return NULL;
	} else if (len == 2) {
		const struct vteseq_2_struct *seqhandler;
		seqhandler = vteseq_2_lookup (name, 2);
		return seqhandler ? seqhandler->handler : NULL;
	} else {
		const struct vteseq_n_struct *seqhandler;
		seqhandler = vteseq_n_lookup (name, len);
		return seqhandler ? seqhandler->handler : NULL;
	}
}


/* Handle a terminal control sequence and its parameters. */
void
_vte_terminal_handle_sequence(VteTerminal *terminal,
			      const char *match_s,
			      GQuark match G_GNUC_UNUSED,
			      GValueArray *params)
{
	VteTerminalSequenceHandler handler;

	_VTE_DEBUG_IF(VTE_DEBUG_PARSE)
		display_control_sequence(match_s, params);

	/* Find the handler for this control sequence. */
	handler = _vte_sequence_get_handler (match_s);

	if (handler != NULL) {
		/* Let the handler handle it. */
		handler (terminal, params);
	} else {
		_vte_debug_print (VTE_DEBUG_MISC,
				  "No handler for control sequence `%s' defined.\n",
				  match_s);
	}
}
