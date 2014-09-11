/*
 * Copyright (C) 2001-2004 Red Hat, Inc.
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

#include <limits.h>
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

#include <glib.h>

#include "vte.h"
#include "vte-private.h"

#define BEL "\007"
#define ST _VTE_CAP_ST

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

		_vte_byte_array_set_minimum_size (terminal->pvt->conv_buffer, outlen);
		buf = bufptr = terminal->pvt->conv_buffer->data;

		if (_vte_conv (conv, &in, &inlen, &buf, &outlen) == (size_t) -1) {
			_vte_debug_print (VTE_DEBUG_IO,
					  "Error converting %ld string bytes (%s), skipping.\n",
					  (long) _vte_byte_array_length (terminal->pvt->outgoing),
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
vte_parse_color (const char *spec, PangoColor *color)
{
	gchar *spec_copy = (gchar *) spec;
	gboolean retval = FALSE;
        GdkColor gdk_color;

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

	retval = gdk_color_parse (spec_copy, &gdk_color);

	if (spec_copy != spec)
		g_free (spec_copy);

        if (retval) {
                color->red = gdk_color.red;
                color->green = gdk_color.green;
                color->blue = gdk_color.blue;
        }

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

/* Emit a "resize-window" signal.  (Grid size.) */
static void
vte_terminal_emit_resize_window(VteTerminal *terminal,
				guint columns, guint rows)
{
	_vte_debug_print(VTE_DEBUG_SIGNALS,
			"Emitting `resize-window'.\n");
	g_signal_emit_by_name(terminal, "resize-window", columns, rows);
}


/* Some common functions */

/* In Xterm, upon printing a character in the last column the cursor doesn't
 * advance.  It's special cased that printing the following letter will first
 * wrap to the next row.
 *
 * As a rule of thumb, escape sequences that move the cursor (e.g. cursor up)
 * or immediately update the visible contents (e.g. clear in line) disable
 * this special mode, whereas escape sequences with no immediate visible
 * effect (e.g. color change) leave this special mode on.  There are
 * exceptions of course (e.g. scroll up).
 *
 * In VTE, a different technical approach is used.  The cursor is advanced to
 * the invisible column on the right, but it's set back to the visible
 * rightmost column whenever necessary (that is, before handling any of the
 * sequences that disable the special cased mode in xterm).  (Bug 731155.)
 */
static void
_vte_terminal_ensure_cursor_is_onscreen (VteTerminal *terminal)
{
        if (G_UNLIKELY (terminal->pvt->screen->cursor_current.col >= terminal->pvt->column_count))
                terminal->pvt->screen->cursor_current.col = terminal->pvt->column_count - 1;
}

static void
_vte_terminal_home_cursor (VteTerminal *terminal)
{
        long origin;
	VteScreen *screen;
	screen = terminal->pvt->screen;

        if (screen->origin_mode &&
            screen->scrolling_restricted) {
                origin = screen->scrolling_region.start;
        } else {
                origin = 0;
        }

	screen->cursor_current.row = screen->insert_delta + origin;
	screen->cursor_current.col = 0;
}

/* Clear the entire screen. */
static void
_vte_terminal_clear_screen (VteTerminal *terminal)
{
	long i, initial, row;
	VteScreen *screen;
	screen = terminal->pvt->screen;
	initial = screen->insert_delta;
	row = screen->cursor_current.row - screen->insert_delta;
	initial = _vte_ring_next(screen->row_data);
	/* Add a new screen's worth of rows. */
	for (i = 0; i < terminal->pvt->row_count; i++)
		_vte_terminal_ring_append (terminal, TRUE);
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
		rowdata = _vte_ring_index_writable (screen->row_data, screen->cursor_current.row);
		g_assert(rowdata != NULL);
		/* Remove it. */
		_vte_row_data_shrink (rowdata, 0);
		/* Add enough cells to the end of the line to fill out the row. */
		_vte_row_data_fill (rowdata, &screen->fill_defaults, terminal->pvt->column_count);
		rowdata->attr.soft_wrapped = 0;
		/* Repaint this row. */
		_vte_invalidate_cells(terminal,
				      0, terminal->pvt->column_count,
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
			/* Get the data for the row we're erasing. */
			rowdata = _vte_ring_index_writable (screen->row_data, i);
			g_assert(rowdata != NULL);
			/* Remove it. */
			_vte_row_data_shrink (rowdata, 0);
			/* Add new cells until we fill the row. */
			_vte_row_data_fill (rowdata, &screen->fill_defaults, terminal->pvt->column_count);
			rowdata->attr.soft_wrapped = 0;
			/* Repaint the row. */
			_vte_invalidate_cells(terminal,
					0, terminal->pvt->column_count, i, 1);
		}
	}
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Scroll the text, but don't move the cursor.  Negative = up, positive = down. */
static void
_vte_terminal_scroll_text (VteTerminal *terminal, int scroll_amount)
{
	long start, end, i;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = start + terminal->pvt->row_count - 1;
	}

	while (_vte_ring_next(screen->row_data) <= end)
		_vte_terminal_ring_append (terminal, FALSE);

	if (scroll_amount > 0) {
		for (i = 0; i < scroll_amount; i++) {
			_vte_terminal_ring_remove (terminal, end);
			_vte_terminal_ring_insert (terminal, start, TRUE);
		}
	} else {
		for (i = 0; i < -scroll_amount; i++) {
			_vte_terminal_ring_remove (terminal, start);
			_vte_terminal_ring_insert (terminal, end, TRUE);
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


/* Call another function a given number of times, or once. */
static void
vte_sequence_handler_multiple_limited(VteTerminal *terminal,
                                      GValueArray *params,
                                      VteTerminalSequenceHandler handler,
                                      glong max)
{
	long val = 1;
	int i;
	GValue *value;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = g_value_get_long(value);
			val = CLAMP(val, 1, max);	/* FIXME: vttest. */
		}
	}
	for (i = 0; i < val; i++)
		handler (terminal, NULL);
}

static void
vte_sequence_handler_multiple_r(VteTerminal *terminal,
                                GValueArray *params,
                                VteTerminalSequenceHandler handler)
{
        vte_sequence_handler_multiple_limited(terminal, params, handler,
                                              terminal->pvt->column_count - terminal->pvt->screen->cursor_current.col);
}

static void
vte_reset_mouse_smooth_scroll_delta(VteTerminal *terminal,
                                    GValueArray *params)
{
	terminal->pvt->mouse_smooth_scroll_delta = 0.;
}

struct decset_t {
        gint16 setting;
        /* offset in VteTerminalPrivate (> 0) or VteScreen (< 0) */
        gint16 boffset;
        gint16 ioffset;
        gint16 poffset;
        gint16 fvalue;
        gint16 tvalue;
        VteTerminalSequenceHandler reset, set;
};

static int
decset_cmp(const void *va,
           const void *vb)
{
        const struct decset_t *a = va;
        const struct decset_t *b = vb;

        return a->setting < b->setting ? -1 : a->setting > b->setting;
}

/* Manipulate certain terminal attributes. */
static void
vte_sequence_handler_decset_internal(VteTerminal *terminal,
				     int setting,
				     gboolean restore,
				     gboolean save,
				     gboolean set)
{
	static const struct decset_t const settings[] = {
#define PRIV_OFFSET(member) (G_STRUCT_OFFSET(VteTerminalPrivate, member))
#define SCREEN_OFFSET(member) (-G_STRUCT_OFFSET(VteScreen, member))
		/* 1: Application/normal cursor keys. */
		{1, 0, PRIV_OFFSET(cursor_mode), 0,
		 VTE_KEYMODE_NORMAL,
		 VTE_KEYMODE_APPLICATION,
		 NULL, NULL,},
		/* 2: disallowed, we don't do VT52. */
		{2, 0, 0, 0, 0, 0, NULL, NULL,},
                /* 3: DECCOLM set/reset to and from 132/80 columns */
                {3, 0, 0, 0,
                 FALSE,
                 TRUE,
                 NULL, NULL,},
		/* 5: Reverse video. */
		{5, SCREEN_OFFSET(reverse_mode), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
		/* 6: Origin mode: when enabled, cursor positioning is
		 * relative to the scrolling region. */
		{6, SCREEN_OFFSET(origin_mode), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
		/* 7: Wraparound mode. */
                {7, PRIV_OFFSET(autowrap), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
		/* 8: disallowed, keyboard repeat is set by user. */
		{8, 0, 0, 0, 0, 0, NULL, NULL,},
		/* 9: Send-coords-on-click. */
		{9, 0, PRIV_OFFSET(mouse_tracking_mode), 0,
		 0,
		 MOUSE_TRACKING_SEND_XY_ON_CLICK,
		 vte_reset_mouse_smooth_scroll_delta,
		 vte_reset_mouse_smooth_scroll_delta,},
		/* 12: disallowed, cursor blinks is set by user. */
		{12, 0, 0, 0, 0, 0, NULL, NULL,},
		/* 18: print form feed. */
		/* 19: set print extent to full screen. */
		/* 25: Cursor visible. */
		{25, PRIV_OFFSET(cursor_visible), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
		/* 30/rxvt: disallowed, scrollbar visibility is set by user. */
		{30, 0, 0, 0, 0, 0, NULL, NULL,},
		/* 35/rxvt: disallowed, fonts set by user. */
		{35, 0, 0, 0, 0, 0, NULL, NULL,},
		/* 38: enter Tektronix mode. */
                /* 40: Enable DECCOLM mode. */
                {40, PRIV_OFFSET(deccolm_mode), 0, 0,
                 FALSE,
                 TRUE,
                 NULL, NULL,},
		/* 41: more(1) fix. */
		/* 42: Enable NLS replacements. */
		{42, PRIV_OFFSET(nrc_mode), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
		/* 44: Margin bell. */
		{44, PRIV_OFFSET(margin_bell), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
		/* 47: Alternate screen. */
		{47, 0, 0, PRIV_OFFSET(screen),
		 PRIV_OFFSET(normal_screen),
                 PRIV_OFFSET(alternate_screen),
		 NULL, NULL,},
		/* 66: Keypad mode. */
		{66, PRIV_OFFSET(keypad_mode), 0, 0,
		 VTE_KEYMODE_NORMAL,
		 VTE_KEYMODE_APPLICATION,
		 NULL, NULL,},
		/* 67: disallowed, backspace key policy is set by user. */
		{67, 0, 0, 0, 0, 0, NULL, NULL,},
		/* 1000: Send-coords-on-button. */
		{1000, 0, PRIV_OFFSET(mouse_tracking_mode), 0,
		 0,
		 MOUSE_TRACKING_SEND_XY_ON_BUTTON,
		 vte_reset_mouse_smooth_scroll_delta,
		 vte_reset_mouse_smooth_scroll_delta,},
		/* 1001: Hilite tracking. */
		{1001, 0, PRIV_OFFSET(mouse_tracking_mode), 0,
		 (0),
		 (MOUSE_TRACKING_HILITE_TRACKING),
		 vte_reset_mouse_smooth_scroll_delta,
		 vte_reset_mouse_smooth_scroll_delta,},
		/* 1002: Cell motion tracking. */
		{1002, 0, PRIV_OFFSET(mouse_tracking_mode), 0,
		 (0),
		 (MOUSE_TRACKING_CELL_MOTION_TRACKING),
		 vte_reset_mouse_smooth_scroll_delta,
		 vte_reset_mouse_smooth_scroll_delta,},
		/* 1003: All motion tracking. */
		{1003, 0, PRIV_OFFSET(mouse_tracking_mode), 0,
		 (0),
		 (MOUSE_TRACKING_ALL_MOTION_TRACKING),
		 vte_reset_mouse_smooth_scroll_delta,
		 vte_reset_mouse_smooth_scroll_delta,},
		/* 1006: Extended mouse coordinates. */
		{1006, PRIV_OFFSET(mouse_xterm_extension), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
		/* 1007: Alternate screen scroll. */
		{1007, PRIV_OFFSET(alternate_screen_scroll), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
		/* 1010/rxvt: disallowed, scroll-on-output is set by user. */
		{1010, 0, 0, 0, 0, 0, NULL, NULL,},
		/* 1011/rxvt: disallowed, scroll-on-keypress is set by user. */
		{1011, 0, 0, 0, 0, 0, NULL, NULL,},
		/* 1015/urxvt: Extended mouse coordinates. */
		{1015, PRIV_OFFSET(mouse_urxvt_extension), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
		/* 1035: disallowed, don't know what to do with it. */
		{1035, 0, 0, 0, 0, 0, NULL, NULL,},
		/* 1036: Meta-sends-escape. */
		{1036, PRIV_OFFSET(meta_sends_escape), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
		/* 1037: disallowed, delete key policy is set by user. */
		{1037, 0, 0, 0, 0, 0, NULL, NULL,},
		/* 1047: Use alternate screen buffer. */
		{1047, 0, 0, PRIV_OFFSET(screen),
		 PRIV_OFFSET(normal_screen),
		 PRIV_OFFSET(alternate_screen),
		 NULL, NULL,},
		/* 1048: Save/restore cursor position. */
		{1048, 0, 0, 0,
		 0,
		 0,
                 vte_sequence_handler_restore_cursor,
                 vte_sequence_handler_save_cursor,},
		/* 1049: Use alternate screen buffer, saving the cursor
		 * position. */
		{1049, 0, 0, PRIV_OFFSET(screen),
		 PRIV_OFFSET(normal_screen),
		 PRIV_OFFSET(alternate_screen),
                 vte_sequence_handler_restore_cursor,
                 vte_sequence_handler_save_cursor,},
		/* 2004: Bracketed paste mode. */
		{2004, PRIV_OFFSET(bracketed_paste_mode), 0, 0,
		 FALSE,
		 TRUE,
		 NULL, NULL,},
#undef PRIV_OFFSET
#undef SCREEN_OFFSET
	};
        struct decset_t key;
        struct decset_t *found;

	/* Handle the setting. */
        key.setting = setting;
        found = bsearch(&key, settings, G_N_ELEMENTS(settings), sizeof(settings[0]), decset_cmp);
        if (!found) {
		_vte_debug_print (VTE_DEBUG_MISC,
				  "DECSET/DECRESET mode %d not recognized, ignoring.\n",
				  setting);
                return;
	}

        key = *found;
        do {
                gboolean *bvalue = NULL;
                gint *ivalue = NULL;
                gpointer *pvalue = NULL, pfvalue = NULL, ptvalue = NULL;
                gpointer p;

		/* Handle settings we want to ignore. */
		if ((key.fvalue == key.tvalue) &&
		    (key.set == NULL) &&
		    (key.reset == NULL)) {
			break;
		}

#define STRUCT_MEMBER_P(type,total_offset) \
                (type) (total_offset >= 0 ? G_STRUCT_MEMBER_P(terminal->pvt, total_offset) : G_STRUCT_MEMBER_P(terminal->pvt->screen, -total_offset))

                if (key.boffset) {
                        bvalue = STRUCT_MEMBER_P(gboolean*, key.boffset);
                } else if (key.ioffset) {
                        ivalue = STRUCT_MEMBER_P(int*, key.ioffset);
                } else if (key.poffset) {
                        pvalue = STRUCT_MEMBER_P(gpointer*, key.poffset);
                        pfvalue = STRUCT_MEMBER_P(gpointer, key.fvalue);
                        ptvalue = STRUCT_MEMBER_P(gpointer, key.tvalue);
                }
#undef STRUCT_MEMBER_P

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
			if (bvalue) {
				set = *(bvalue) != FALSE;
			} else
			if (ivalue) {
                                set = *(ivalue) == (int)key.tvalue;
			} else
			if (pvalue) {
				set = *(pvalue) == ptvalue;
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
			if (key.set && set) {
				key.set (terminal, NULL);
			}
			if (bvalue) {
				*(bvalue) = set;
			} else
			if (ivalue) {
                                *(ivalue) = set ? (int)key.tvalue : (int)key.fvalue;
			} else
			if (pvalue) {
                                *(pvalue) = set ? ptvalue : pfvalue;
			}
			if (key.reset && !set) {
				key.reset (terminal, NULL);
			}
		}
	} while (0);

	/* Do whatever's necessary when the setting changes. */
	switch (setting) {
	case 1:
		_vte_debug_print(VTE_DEBUG_KEYBOARD, set ?
				"Entering application cursor mode.\n" :
				"Leaving application cursor mode.\n");
		break;
	case 3:
                /* 3: DECCOLM set/reset to 132/80 columns mode, clear screen and cursor home */
                if (terminal->pvt->deccolm_mode) {
                        vte_terminal_emit_resize_window(terminal,
                                                        set ? 132 : 80,
                                                        terminal->pvt->row_count);
                        _vte_terminal_clear_screen(terminal);
                        _vte_terminal_home_cursor(terminal);
                }
		break;
	case 5:
		/* Repaint everything in reverse mode. */
		_vte_invalidate_all(terminal);
		break;
	case 6:
		/* Reposition the cursor in its new home position. */
                _vte_terminal_home_cursor (terminal);
		break;
	case 47:
	case 1047:
	case 1049:
		/* Clear the alternate screen if we're switching
		 * to it, and home the cursor. */
		if (set) {
			_vte_terminal_set_default_attributes (terminal);
			_vte_terminal_clear_screen (terminal);
			_vte_terminal_home_cursor (terminal);
		}
		/* Reset scrollbars and repaint everything. */
		gtk_adjustment_set_value(terminal->pvt->vadjustment,
					 terminal->pvt->screen->scroll_delta);
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
	default:
		break;
	}
}




/* THE HANDLERS */


/* End alternate character set. */
static void
vte_sequence_handler_alternate_character_set_end (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->alternate_charset = FALSE;
}

/* Start using alternate character set. */
static void
vte_sequence_handler_alternate_character_set_start (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->alternate_charset = TRUE;
}

/* Beep. */
static void
vte_sequence_handler_bell (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_beep (terminal);
	g_signal_emit_by_name(terminal, "bell");
}

/* Backtab. */
static void
vte_sequence_handler_cursor_back_tab (VteTerminal *terminal, GValueArray *params)
{
	long newcol;

	/* Calculate which column is the previous tab stop. */
	newcol = terminal->pvt->screen->cursor_current.col;

	if (terminal->pvt->tabstops != NULL) {
		/* Find the next tabstop. */
		while (newcol > 0) {
			newcol--;
			if (_vte_terminal_get_tabstop(terminal,
						     newcol % terminal->pvt->column_count)) {
				break;
			}
		}
	}

	/* Warp the cursor. */
	_vte_debug_print(VTE_DEBUG_PARSE,
			"Moving cursor to column %ld.\n", (long)newcol);
	terminal->pvt->screen->cursor_current.col = newcol;
}

/* Clear from the cursor position (inclusive!) to the beginning of the line. */
static void
_vte_sequence_handler_cb (VteTerminal *terminal, GValueArray *params)
{
	VteRowData *rowdata;
	long i;
	VteScreen *screen;
	VteCell *pcell;
	screen = terminal->pvt->screen;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	/* Get the data for the row which the cursor points to. */
	rowdata = _vte_terminal_ensure_row(terminal);
        /* Clean up Tab/CJK fragments. */
        _vte_terminal_cleanup_fragments (terminal, 0, screen->cursor_current.col + 1);
	/* Clear the data up to the current column with the default
	 * attributes.  If there is no such character cell, we need
	 * to add one. */
	for (i = 0; i <= screen->cursor_current.col; i++) {
		if (i < (glong) _vte_row_data_length (rowdata)) {
			/* Muck with the cell in this location. */
			pcell = _vte_row_data_get_writable (rowdata, i);
			*pcell = screen->color_defaults;
		} else {
			/* Add new cells until we have one here. */
			_vte_row_data_append (rowdata, &screen->color_defaults);
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
_vte_sequence_handler_cd (VteTerminal *terminal, GValueArray *params)
{
	VteRowData *rowdata;
	glong i;
	VteScreen *screen;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear the rest of the
	 * row the cursor is on and all of the rows below the cursor. */
	i = screen->cursor_current.row;
	if (i < _vte_ring_next(screen->row_data)) {
		/* Get the data for the row we're clipping. */
		rowdata = _vte_ring_index_writable (screen->row_data, i);
                /* Clean up Tab/CJK fragments. */
                if ((glong) _vte_row_data_length (rowdata) > screen->cursor_current.col)
                        _vte_terminal_cleanup_fragments (terminal, screen->cursor_current.col, _vte_row_data_length (rowdata));
		/* Clear everything to the right of the cursor. */
		if (rowdata)
			_vte_row_data_shrink (rowdata, screen->cursor_current.col);
	}
	/* Now for the rest of the lines. */
	for (i = screen->cursor_current.row + 1;
	     i < _vte_ring_next(screen->row_data);
	     i++) {
		/* Get the data for the row we're removing. */
		rowdata = _vte_ring_index_writable (screen->row_data, i);
		/* Remove it. */
		if (rowdata)
			_vte_row_data_shrink (rowdata, 0);
	}
	/* Now fill the cleared areas. */
	for (i = screen->cursor_current.row;
	     i < screen->insert_delta + terminal->pvt->row_count;
	     i++) {
		/* Retrieve the row's data, creating it if necessary. */
		if (_vte_ring_contains (screen->row_data, i)) {
			rowdata = _vte_ring_index_writable (screen->row_data, i);
			g_assert(rowdata != NULL);
		} else {
			rowdata = _vte_terminal_ring_append (terminal, FALSE);
		}
		/* Pad out the row. */
		if (screen->fill_defaults.attr.back != VTE_DEFAULT_BG) {
			_vte_row_data_fill (rowdata, &screen->fill_defaults, terminal->pvt->column_count);
		}
		rowdata->attr.soft_wrapped = 0;
		/* Repaint this row. */
		_vte_invalidate_cells(terminal,
				      0, terminal->pvt->column_count,
				      i, 1);
	}

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Clear from the cursor position to the end of the line. */
static void
_vte_sequence_handler_ce (VteTerminal *terminal, GValueArray *params)
{
	VteRowData *rowdata;
	VteScreen *screen;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	screen = terminal->pvt->screen;
	/* Get the data for the row which the cursor points to. */
	rowdata = _vte_terminal_ensure_row(terminal);
	g_assert(rowdata != NULL);
	if ((glong) _vte_row_data_length (rowdata) > screen->cursor_current.col) {
                /* Clean up Tab/CJK fragments. */
                _vte_terminal_cleanup_fragments (terminal, screen->cursor_current.col, _vte_row_data_length (rowdata));
                /* Remove the data at the end of the array until the current column
                 * is the end of the array. */
		_vte_row_data_shrink (rowdata, screen->cursor_current.col);
		/* We've modified the display.  Make a note of it. */
		terminal->pvt->text_deleted_flag = TRUE;
	}
	if (screen->fill_defaults.attr.back != VTE_DEFAULT_BG) {
		/* Add enough cells to fill out the row. */
		_vte_row_data_fill (rowdata, &screen->fill_defaults, terminal->pvt->column_count);
	}
	rowdata->attr.soft_wrapped = 0;
	/* Repaint this row. */
	_vte_invalidate_cells(terminal,
			      screen->cursor_current.col,
			      terminal->pvt->column_count -
			      screen->cursor_current.col,
			      screen->cursor_current.row, 1);
}

/* Move the cursor to the given column (horizontal position), 1-based. */
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
                        val = CLAMP(g_value_get_long(value) - 1,
				    0,
				    terminal->pvt->column_count - 1);
		}
	}

        screen->cursor_current.col = val;
}

/* Move the cursor to the given position, 1-based. */
static void
vte_sequence_handler_cursor_position (VteTerminal *terminal, GValueArray *params)
{
	GValue *row, *col;
        long rowval, colval, origin, rowmax;
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
                                rowmax = screen->scrolling_region.end;
			} else {
				origin = 0;
                                rowmax = terminal->pvt->row_count - 1;
			}
                        rowval = g_value_get_long(row) - 1 + origin;
                        rowval = CLAMP(rowval, origin, rowmax);
		}
		if (params->n_values >= 2) {
			col = g_value_array_get_nth(params, 1);
			if (G_VALUE_HOLDS_LONG(col)) {
                                colval = g_value_get_long(col) - 1;
				colval = CLAMP(colval, 0, terminal->pvt->column_count - 1);
			}
		}
	}
	screen->cursor_current.row = rowval + screen->insert_delta;
	screen->cursor_current.col = colval;
}

/* Carriage return. */
static void
vte_sequence_handler_carriage_return (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
}

/* Restrict scrolling and updates to a subset of the visible lines. */
static void
vte_sequence_handler_set_scrolling_region (VteTerminal *terminal, GValueArray *params)
{
	long start=-1, end=-1, rows;
	GValue *value;
	VteScreen *screen;

	/* We require two parameters.  Anything less is a reset. */
	screen = terminal->pvt->screen;
	if ((params == NULL) || (params->n_values < 2)) {
		screen->scrolling_restricted = FALSE;
                _vte_terminal_home_cursor (terminal);
		return;
	}
	/* Extract the two values. */
	value = g_value_array_get_nth(params, 0);
	if (G_VALUE_HOLDS_LONG(value)) {
                start = g_value_get_long(value) - 1;
	}
	value = g_value_array_get_nth(params, 1);
	if (G_VALUE_HOLDS_LONG(value)) {
                end = g_value_get_long(value) - 1;
	}
	rows = terminal->pvt->row_count;
        /* A (1-based) value of 0 means default. */
        if (start == -1) {
		start = 0;
	}
        if (end == -1) {
                end = rows - 1;
        }
        /* Bail out on garbage, require at least 2 rows, as per xterm. */
        if (start < 0 || start >= rows - 1 || end < start + 1) {
                return;
        }
        if (end >= rows) {
		end = rows - 1;
	}

	/* Set the right values. */
	screen->scrolling_region.start = start;
	screen->scrolling_region.end = end;
	screen->scrolling_restricted = TRUE;
	if (screen->scrolling_region.start == 0 &&
	    screen->scrolling_region.end == rows - 1) {
		/* Special case -- run wild, run free. */
		screen->scrolling_restricted = FALSE;
	} else {
		/* Maybe extend the ring -- bug 710483 */
		while (_vte_ring_next(screen->row_data) < screen->insert_delta + rows)
			_vte_ring_insert(screen->row_data, _vte_ring_next(screen->row_data));
	}

        _vte_terminal_home_cursor (terminal);
}

/* Move the cursor to the beginning of the Nth next line, no scrolling. */
static void
vte_sequence_handler_cursor_next_line (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
        vte_sequence_handler_cursor_down (terminal, params);
}

/* Move the cursor to the beginning of the Nth previous line, no scrolling. */
static void
vte_sequence_handler_cursor_preceding_line (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
        vte_sequence_handler_cursor_up (terminal, params);
}

/* Move the cursor to the given row (vertical position), 1-based. */
static void
vte_sequence_handler_line_position_absolute (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	GValue *value;
        long val = 1, origin, rowmax;
	screen = terminal->pvt->screen;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
                        val = g_value_get_long(value);
		}
	}

        if (screen->origin_mode &&
            screen->scrolling_restricted) {
                origin = screen->scrolling_region.start;
                rowmax = screen->scrolling_region.end;
        } else {
                origin = 0;
                rowmax = terminal->pvt->row_count - 1;
        }
        val = val - 1 + origin;
        val = CLAMP(val, origin, rowmax);
        screen->cursor_current.row = screen->insert_delta + val;
}

/* Delete a character at the current cursor position. */
static void
_vte_sequence_handler_dc (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	VteRowData *rowdata;
	long col;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	screen = terminal->pvt->screen;

	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		long len;
		/* Get the data for the row which the cursor points to. */
		rowdata = _vte_ring_index_writable (screen->row_data, screen->cursor_current.row);
		g_assert(rowdata != NULL);
		col = screen->cursor_current.col;
		len = _vte_row_data_length (rowdata);
		/* Remove the column. */
		if (col < len) {
                        /* Clean up Tab/CJK fragments. */
                        _vte_terminal_cleanup_fragments (terminal, col, col + 1);
			_vte_row_data_remove (rowdata, col);
			if (screen->fill_defaults.attr.back != VTE_DEFAULT_BG) {
				_vte_row_data_fill (rowdata, &screen->fill_defaults, terminal->pvt->column_count);
				len = terminal->pvt->column_count;
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
vte_sequence_handler_delete_characters (VteTerminal *terminal, GValueArray *params)
{
        vte_sequence_handler_multiple_r(terminal, params, _vte_sequence_handler_dc);
}

/* Cursor down N lines, no scrolling. */
static void
vte_sequence_handler_cursor_down (VteTerminal *terminal, GValueArray *params)
{
        long end;
	VteScreen *screen;
        GValue *value;
        long val;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	screen = terminal->pvt->screen;

	if (screen->scrolling_restricted) {
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
                end = screen->insert_delta + terminal->pvt->row_count - 1;
	}

        val = 1;
        if (params != NULL && params->n_values >= 1) {
                value = g_value_array_get_nth(params, 0);
                if (G_VALUE_HOLDS_LONG(value)) {
                        val = CLAMP(g_value_get_long(value),
                                    1, terminal->pvt->row_count);
                }
        }

        screen->cursor_current.row = MIN(screen->cursor_current.row + val, end);
}

/* Erase characters starting at the cursor position (overwriting N with
 * spaces, but not moving the cursor). */
static void
vte_sequence_handler_erase_characters (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	VteRowData *rowdata;
	GValue *value;
	VteCell *cell;
	long col, i, count;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

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
                /* Clean up Tab/CJK fragments. */
                _vte_terminal_cleanup_fragments (terminal, screen->cursor_current.col, screen->cursor_current.col + count);
		/* Write over the characters.  (If there aren't enough, we'll
		 * need to create them.) */
		for (i = 0; i < count; i++) {
			col = screen->cursor_current.col + i;
			if (col >= 0) {
				if (col < (glong) _vte_row_data_length (rowdata)) {
					/* Replace this cell with the current
					 * defaults. */
					cell = _vte_row_data_get_writable (rowdata, col);
					*cell = screen->color_defaults;
				} else {
					/* Add new cells until we have one here. */
					_vte_row_data_fill (rowdata, &screen->color_defaults, col + 1);
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

/* Form-feed / next-page. */
static void
vte_sequence_handler_form_feed (VteTerminal *terminal, GValueArray *params)
{
        vte_sequence_handler_line_feed (terminal, params);
}

/* Insert a blank character. */
static void
_vte_sequence_handler_insert_character (VteTerminal *terminal, GValueArray *params)
{
	VteVisualPosition save;
	VteScreen *screen;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	screen = terminal->pvt->screen;

	save = screen->cursor_current;

	_vte_terminal_insert_char(terminal, ' ', TRUE, TRUE);

	screen->cursor_current = save;
}

/* Insert N blank characters. */
/* TODOegmont: Insert them in a single run, so that we call _vte_terminal_cleanup_fragments only once. */
static void
vte_sequence_handler_insert_blank_characters (VteTerminal *terminal, GValueArray *params)
{
        vte_sequence_handler_multiple_r(terminal, params, _vte_sequence_handler_insert_character);
}

/* Cursor down 1 line, with scrolling. */
static void
vte_sequence_handler_index (VteTerminal *terminal, GValueArray *params)
{
        vte_sequence_handler_line_feed (terminal, params);
}

/* Cursor left. */
static void
vte_sequence_handler_backspace (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	screen = terminal->pvt->screen;
	if (screen->cursor_current.col > 0) {
		/* There's room to move left, so do so. */
		screen->cursor_current.col--;
	}
}

/* Cursor left N columns. */
static void
vte_sequence_handler_cursor_backward (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
        GValue *value;
        long val;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

        screen = terminal->pvt->screen;

        val = 1;
        if (params != NULL && params->n_values >= 1) {
                value = g_value_array_get_nth(params, 0);
                if (G_VALUE_HOLDS_LONG(value)) {
                        val = MAX(g_value_get_long(value), 1);
                }
        }
        screen->cursor_current.col = MAX(screen->cursor_current.col - val, 0);
}

/* Cursor right N columns. */
static void
vte_sequence_handler_cursor_forward (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
        GValue *value;
        long val;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	screen = terminal->pvt->screen;

        val = 1;
        if (params != NULL && params->n_values >= 1) {
                value = g_value_array_get_nth(params, 0);
                if (G_VALUE_HOLDS_LONG(value)) {
                        val = CLAMP(g_value_get_long(value),
                                    1, terminal->pvt->column_count);
                }
        }
        /* The cursor can be further to the right, don't move in that case. */
        if (screen->cursor_current.col < terminal->pvt->column_count) {
		/* There's room to move right. */
                screen->cursor_current.col = MIN(screen->cursor_current.col + val,
                                                 terminal->pvt->column_count - 1);
	}
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static void
vte_sequence_handler_next_line (VteTerminal *terminal, GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	_vte_terminal_cursor_down (terminal);
}

/* No-op. */
static void
vte_sequence_handler_linux_console_cursor_attributes (VteTerminal *terminal, GValueArray *params)
{
}

/* Restore cursor (position). */
static void
vte_sequence_handler_restore_cursor (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_current.col = screen->cursor_saved.col;
	screen->cursor_current.row = CLAMP(screen->cursor_saved.row +
					   screen->insert_delta,
					   screen->insert_delta,
					   screen->insert_delta +
					   terminal->pvt->row_count - 1);

        _vte_terminal_ensure_cursor_is_onscreen(terminal);
}

/* Save cursor (position). */
static void
vte_sequence_handler_save_cursor (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_saved.col = screen->cursor_current.col;
	screen->cursor_saved.row = CLAMP(screen->cursor_current.row -
					 screen->insert_delta,
					 0, terminal->pvt->row_count - 1);
}

/* Scroll the text down N lines, but don't move the cursor. */
static void
vte_sequence_handler_scroll_down (VteTerminal *terminal, GValueArray *params)
{
	long val = 1;
	GValue *value;

        /* No _vte_terminal_ensure_cursor_is_onscreen() here as per xterm */

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = g_value_get_long(value);
			val = MAX(val, 1);
		}
	}

	_vte_terminal_scroll_text (terminal, val);
}

/* Internal helper for changing color in the palette */
static void
vte_sequence_handler_change_color_internal (VteTerminal *terminal, GValueArray *params,
					    const char *terminator)
{
	gchar **pairs, *str = NULL;
	GValue *value;
	PangoColor color;
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

			if (idx >= VTE_DEFAULT_FG)
				continue;

			if (vte_parse_color (pairs[i + 1], &color)) {
                                _vte_terminal_set_color_internal(terminal, idx, VTE_COLOR_SOURCE_ESCAPE, &color);
			} else if (strcmp (pairs[i + 1], "?") == 0) {
				gchar buf[128];
				PangoColor *c = _vte_terminal_get_color(terminal, idx);
				g_assert(c != NULL);
				g_snprintf (buf, sizeof (buf),
					    _VTE_CAP_OSC "4;%u;rgb:%04x/%04x/%04x%s",
					    idx, c->red, c->green, c->blue, terminator);
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

/* Change color in the palette, BEL terminated */
static void
vte_sequence_handler_change_color_bel (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_color_internal (terminal, params, BEL);
}

/* Change color in the palette, ST terminated */
static void
vte_sequence_handler_change_color_st (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_color_internal (terminal, params, ST);
}

/* Reset color in the palette */
static void
vte_sequence_handler_reset_color (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
        guint i;
	long idx;

	if (params != NULL && params->n_values > 0) {
		for (i = 0; i < params->n_values; i++) {
			value = g_value_array_get_nth (params, i);

			if (!G_VALUE_HOLDS_LONG (value))
				continue;
			idx = g_value_get_long (value);
			if (idx < 0 || idx >= VTE_DEFAULT_FG)
				continue;

			_vte_terminal_set_color_internal(terminal, idx, VTE_COLOR_SOURCE_ESCAPE, NULL);
		}
	} else {
		for (idx = 0; idx < VTE_DEFAULT_FG; idx++) {
			_vte_terminal_set_color_internal(terminal, idx, VTE_COLOR_SOURCE_ESCAPE, NULL);
		}
	}
}

/* Scroll the text up N lines, but don't move the cursor. */
static void
vte_sequence_handler_scroll_up (VteTerminal *terminal, GValueArray *params)
{
	long val = 1;
	GValue *value;

        /* No _vte_terminal_ensure_cursor_is_onscreen() here as per xterm */

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = g_value_get_long(value);
			val = MAX(val, 1);
		}
	}

	_vte_terminal_scroll_text (terminal, -val);
}

/* Cursor down 1 line, with scrolling. */
static void
vte_sequence_handler_line_feed (VteTerminal *terminal, GValueArray *params)
{
        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	_vte_terminal_cursor_down (terminal);
}

/* Cursor up 1 line, with scrolling. */
static void
vte_sequence_handler_reverse_index (VteTerminal *terminal, GValueArray *params)
{
	long start, end;
	VteScreen *screen;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	screen = terminal->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->scrolling_region.start + screen->insert_delta;
		end = screen->scrolling_region.end + screen->insert_delta;
	} else {
		start = terminal->pvt->screen->insert_delta;
		end = start + terminal->pvt->row_count - 1;
	}

	if (screen->cursor_current.row == start) {
		/* If we're at the top of the scrolling region, add a
		 * line at the top to scroll the bottom off. */
		_vte_terminal_ring_remove (terminal, end);
		_vte_terminal_ring_insert (terminal, start, TRUE);
		/* Update the display. */
		_vte_terminal_scroll_region(terminal, start, end - start + 1, 1);
		_vte_invalidate_cells(terminal,
				      0, terminal->pvt->column_count,
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

/* Set tab stop in the current column. */
static void
vte_sequence_handler_tab_set (VteTerminal *terminal, GValueArray *params)
{
	if (terminal->pvt->tabstops == NULL) {
		terminal->pvt->tabstops = g_hash_table_new(NULL, NULL);
	}
	_vte_terminal_set_tabstop(terminal,
				 terminal->pvt->screen->cursor_current.col);
}

/* Tab. */
static void
vte_sequence_handler_tab (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	long old_len, newcol, col;

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
	if (newcol >= terminal->pvt->column_count) {
		newcol = terminal->pvt->column_count - 1;
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

		old_len = _vte_row_data_length (rowdata);
		_vte_row_data_fill (rowdata, &screen->fill_defaults, newcol);

		/* Insert smart tab if there's nothing in the line after
		 * us.  Though, there may be empty cells (with non-default
		 * background color for example.
		 *
		 * Notable bugs here: 545924 and 597242 */
		{
			glong i;
			gboolean found = FALSE;
			for (i = old_len; i > col; i--) {
				const VteCell *cell = _vte_row_data_get (rowdata, i - 1);
				if (cell->attr.fragment || cell->c != 0) {
					found = TRUE;
					break;
				}
			}
			/* Nothing found on the line after us, turn this into
			 * a smart tab */
			if (!found && newcol - col <= VTE_TAB_WIDTH_MAX) {
				VteCell *cell = _vte_row_data_get_writable (rowdata, col);
				VteCell tab = *cell;
				tab.attr.columns = newcol - col;
				tab.c = '\t';
				/* Save tab char */
				*cell = tab;
				/* And adjust the fragments */
				for (i = col + 1; i < newcol; i++) {
					cell = _vte_row_data_get_writable (rowdata, i);
					cell->c = '\t';
					cell->attr.columns = 1;
					cell->attr.fragment = 1;
				}
			}
		}

		_vte_invalidate_cells (terminal,
				screen->cursor_current.col,
				newcol - screen->cursor_current.col,
				screen->cursor_current.row, 1);
		screen->cursor_current.col = newcol;
	}
}

static void
vte_sequence_handler_cursor_forward_tabulation (VteTerminal *terminal, GValueArray *params)
{
        vte_sequence_handler_multiple_r(terminal, params, vte_sequence_handler_tab);
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

/* Cursor up N lines, no scrolling. */
static void
vte_sequence_handler_cursor_up (VteTerminal *terminal, GValueArray *params)
{
	VteScreen *screen;
	long start;
        GValue *value;
        long val;

        _vte_terminal_ensure_cursor_is_onscreen(terminal);

	screen = terminal->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
	} else {
		start = screen->insert_delta;
	}

        val = 1;
        if (params != NULL && params->n_values >= 1) {
                value = g_value_array_get_nth(params, 0);
                if (G_VALUE_HOLDS_LONG(value)) {
                        val = CLAMP(g_value_get_long(value),
                                    1, terminal->pvt->row_count);
                }
        }

        screen->cursor_current.row = MAX(screen->cursor_current.row - val, start);
}

/* Vertical tab. */
static void
vte_sequence_handler_vertical_tab (VteTerminal *terminal, GValueArray *params)
{
        vte_sequence_handler_line_feed (terminal, params);
}

/* Parse parameters of SGR 38 or 48, starting at @index within @params.
 * Returns the color index, or -1 on error.
 * Increments @index to point to the last consumed parameter (not beyond). */
static gint32
vte_sequence_parse_sgr_38_48_parameters (GValueArray *params, unsigned int *index)
{
	if (*index < params->n_values) {
		GValue *value0, *value1, *value2, *value3;
		long param0, param1, param2, param3;
		value0 = g_value_array_get_nth(params, *index);
		if (G_UNLIKELY (!G_VALUE_HOLDS_LONG(value0)))
			return -1;
		param0 = g_value_get_long(value0);
		switch (param0) {
		case 2:
			if (G_UNLIKELY (*index + 3 >= params->n_values))
				return -1;
			value1 = g_value_array_get_nth(params, *index + 1);
			value2 = g_value_array_get_nth(params, *index + 2);
			value3 = g_value_array_get_nth(params, *index + 3);
			if (G_UNLIKELY (!(G_VALUE_HOLDS_LONG(value1) && G_VALUE_HOLDS_LONG(value2) && G_VALUE_HOLDS_LONG(value3))))
				return -1;
			param1 = g_value_get_long(value1);
			param2 = g_value_get_long(value2);
			param3 = g_value_get_long(value3);
			if (G_UNLIKELY (param1 < 0 || param1 >= 256 || param2 < 0 || param2 >= 256 || param3 < 0 || param3 >= 256))
				return -1;
			*index += 3;
			return VTE_RGB_COLOR | (param1 << 16) | (param2 << 8) | param3;
		case 5:
			if (G_UNLIKELY (*index + 1 >= params->n_values))
				return -1;
			value1 = g_value_array_get_nth(params, *index + 1);
			if (G_UNLIKELY (!G_VALUE_HOLDS_LONG(value1)))
				return -1;
			param1 = g_value_get_long(value1);
			if (G_UNLIKELY (param1 < 0 || param1 >= 256))
				return -1;
			*index += 1;
			return param1;
		}
	}
	return -1;
}

/* Handle ANSI color setting and related stuffs (SGR).
 * @params contains the values split at semicolons, with sub arrays splitting at colons
 * wherever colons were encountered. */
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
		value = g_value_array_get_nth(params, i);
		/* If this parameter is a GValueArray, it can be a fully colon separated 38 or 48
		 * (see below for details). */
		if (G_UNLIKELY (G_VALUE_HOLDS_BOXED(value))) {
			GValueArray *subvalues = g_value_get_boxed(value);
			GValue *value0;
			long param0;
			gint32 color;
			unsigned int index = 1;

			value0 = g_value_array_get_nth(subvalues, 0);
			if (G_UNLIKELY (!G_VALUE_HOLDS_LONG(value0)))
				continue;
			param0 = g_value_get_long(value0);
			if (G_UNLIKELY (param0 != 38 && param0 != 48))
				continue;
			color = vte_sequence_parse_sgr_38_48_parameters(subvalues, &index);
			/* Bail out on additional colon-separated values. */
			if (G_UNLIKELY (index != subvalues->n_values - 1))
				continue;
			if (G_LIKELY (color != -1)) {
				if (param0 == 38) {
					terminal->pvt->screen->defaults.attr.fore = color;
				} else {
					terminal->pvt->screen->defaults.attr.back = color;
				}
			}
			continue;
		}
		/* If this parameter is not a GValueArray and not a number either, skip it. */
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
			break;
		case 2:
			terminal->pvt->screen->defaults.attr.dim = 1;
			break;
		case 3:
			terminal->pvt->screen->defaults.attr.italic = 1;
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
			terminal->pvt->screen->defaults.attr.dim = 0;
			break;
		case 23:
			terminal->pvt->screen->defaults.attr.italic = 0;
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
			terminal->pvt->screen->defaults.attr.fore = VTE_LEGACY_COLORS_OFFSET + param - 30;
			break;
		case 38:
		case 48:
		{
			/* The format looks like:
			 * - 256 color indexed palette:
			 *   - ^[[38;5;INDEXm
			 *   - ^[[38;5:INDEXm
			 *   - ^[[38:5:INDEXm
			 * - true colors:
			 *   - ^[[38;2;RED;GREEN;BLUEm
			 *   - ^[[38;2:RED:GREEN:BLUEm
			 *   - ^[[38:2:RED:GREEN:BLUEm
			 * See bug 685759 for details.
			 * The fully colon versions were handled above separately. The code is reached
			 * if the first separator is a semicolon. */
			if ((i + 1) < params->n_values) {
				gint32 color;
				GValue *value1 = g_value_array_get_nth(params, ++i);
				if (G_VALUE_HOLDS_LONG(value1)) {
					/* Only semicolons as separators. */
					color = vte_sequence_parse_sgr_38_48_parameters(params, &i);
				} else if (G_VALUE_HOLDS_BOXED(value1)) {
					/* The first separator was a semicolon, the rest are colons. */
					GValueArray *subvalues = g_value_get_boxed(value1);
					unsigned int index = 0;
					color = vte_sequence_parse_sgr_38_48_parameters(subvalues, &index);
					/* Bail out on additional colon-separated values. */
					if (G_UNLIKELY (index != subvalues->n_values - 1))
						break;
				} else {
					break;
				}
				if (G_LIKELY (color != -1)) {
					if (param == 38) {
						terminal->pvt->screen->defaults.attr.fore = color;
					} else {
						terminal->pvt->screen->defaults.attr.back = color;
					}
				}
			}
			break;
		}
		case 39:
			/* default foreground */
			terminal->pvt->screen->defaults.attr.fore = VTE_DEFAULT_FG;
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			terminal->pvt->screen->defaults.attr.back = VTE_LEGACY_COLORS_OFFSET + param - 40;
			break;
	     /* case 48: was handled above at 38 to avoid code duplication */
		case 49:
			/* default background */
			terminal->pvt->screen->defaults.attr.back = VTE_DEFAULT_BG;
			break;
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
			terminal->pvt->screen->defaults.attr.fore = VTE_LEGACY_COLORS_OFFSET + param - 90 + VTE_COLOR_BRIGHT_OFFSET;
			break;
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
			terminal->pvt->screen->defaults.attr.back = VTE_LEGACY_COLORS_OFFSET + param - 100 + VTE_COLOR_BRIGHT_OFFSET;
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

/* Move the cursor to the given column in the top row, 1-based. */
static void
vte_sequence_handler_cursor_position_top_row (VteTerminal *terminal, GValueArray *params)
{
        GValue value = {0};

        g_value_init (&value, G_TYPE_LONG);
        g_value_set_long (&value, 1);

        g_value_array_insert (params, 0, &value);

        vte_sequence_handler_cursor_position(terminal, params);
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

static void
vte_sequence_handler_set_current_directory_uri (VteTerminal *terminal, GValueArray *params)
{
        GValue *value;
        char *uri, *filename;

        uri = NULL;
        if (params != NULL && params->n_values > 0) {
                value = g_value_array_get_nth(params, 0);

                if (G_VALUE_HOLDS_POINTER(value)) {
                        uri = vte_ucs4_to_utf8 (terminal, g_value_get_pointer (value));
                } else if (G_VALUE_HOLDS_STRING(value)) {
                        /* Copy the string into the buffer. */
                        uri = g_value_dup_string(value);
                }
        }

        /* Validate URI */
        if (uri && uri[0]) {
                filename = g_filename_from_uri (uri, NULL, NULL);
                if (filename == NULL) {
                        /* invalid URI */
                        g_free (uri);
                        uri = NULL;
                } else {
                        g_free (filename);
                }
        }

        g_free(terminal->pvt->current_directory_uri_changed);
        terminal->pvt->current_directory_uri_changed = uri;
}

static void
vte_sequence_handler_set_current_file_uri (VteTerminal *terminal, GValueArray *params)
{
        GValue *value;
        char *uri, *filename;

        uri = NULL;
        if (params != NULL && params->n_values > 0) {
                value = g_value_array_get_nth(params, 0);

                if (G_VALUE_HOLDS_POINTER(value)) {
                        uri = vte_ucs4_to_utf8 (terminal, g_value_get_pointer (value));
                } else if (G_VALUE_HOLDS_STRING(value)) {
                        /* Copy the string into the buffer. */
                        uri = g_value_dup_string(value);
                }
        }

        /* Validate URI */
        if (uri && uri[0]) {
                filename = g_filename_from_uri (uri, NULL, NULL);
                if (filename == NULL) {
                        /* invalid URI */
                        g_free (uri);
                        uri = NULL;
                } else {
                        g_free (filename);
                }
        }

        g_free(terminal->pvt->current_file_uri_changed);
        terminal->pvt->current_file_uri_changed = uri;
}

/* Restrict the scrolling region. */
static void
vte_sequence_handler_set_scrolling_region_from_start (VteTerminal *terminal, GValueArray *params)
{
	GValue value = {0};

	g_value_init (&value, G_TYPE_LONG);
        g_value_set_long (&value, 0);  /* A missing value is treated as 0 */

	g_value_array_insert (params, 0, &value);

        vte_sequence_handler_set_scrolling_region (terminal, params);
}

static void
vte_sequence_handler_set_scrolling_region_to_end (VteTerminal *terminal, GValueArray *params)
{
	GValue value = {0};

	g_value_init (&value, G_TYPE_LONG);
        g_value_set_long (&value, 0);  /* A missing value is treated as 0 */

	g_value_array_insert (params, 1, &value);

        vte_sequence_handler_set_scrolling_region (terminal, params);
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

/* Same as cursor_character_absolute, not widely supported. */
static void
vte_sequence_handler_character_position_absolute (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_cursor_character_absolute (terminal, params);
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
                _vte_sequence_handler_cd (terminal, NULL);
		break;
	case 1:
		/* Clear above the current line. */
		_vte_terminal_clear_above_current (terminal);
		/* Clear everything to the left of the cursor, too. */
		/* FIXME: vttest. */
                _vte_sequence_handler_cb (terminal, NULL);
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
                _vte_sequence_handler_ce (terminal, NULL);
		break;
	case 1:
		/* Clear to start of the line. */
                _vte_sequence_handler_cb (terminal, NULL);
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

/* Insert a certain number of lines below the current cursor. */
static void
vte_sequence_handler_insert_lines (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param, end, row, i, limit;
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
		end = screen->insert_delta + terminal->pvt->row_count - 1;
	}

	/* Only allow to insert as many lines as there are between this row
         * and the end of the scrolling region. See bug #676090.
         */
        limit = end - row + 1;
        param = MIN (param, limit);

	for (i = 0; i < param; i++) {
		/* Clear a line off the end of the region and add one to the
		 * top of the region. */
		_vte_terminal_ring_remove (terminal, end);
		_vte_terminal_ring_insert (terminal, row, TRUE);
	}
        terminal->pvt->screen->cursor_current.col = 0;
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
	VteScreen *screen;
	long param, end, row, i, limit;

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
		end = screen->insert_delta + terminal->pvt->row_count - 1;
	}

        /* Only allow to delete as many lines as there are between this row
         * and the end of the scrolling region. See bug #676090.
         */
        limit = end - row + 1;
        param = MIN (param, limit);

	/* Clear them from below the current cursor. */
	for (i = 0; i < param; i++) {
		/* Insert a line at the end of the region and remove one from
		 * the top of the region. */
		_vte_terminal_ring_remove (terminal, row);
		_vte_terminal_ring_insert (terminal, end, TRUE);
	}
        terminal->pvt->screen->cursor_current.col = 0;
	/* Update the display. */
	_vte_terminal_scroll_region(terminal, row, end - row + 1, -param);
	/* Adjust the scrollbars if necessary. */
	_vte_terminal_adjust_adjustments(terminal);
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_flag = TRUE;
}

/* Set the terminal encoding. */
static void
vte_sequence_handler_default_character_set (VteTerminal *terminal, GValueArray *params)
{
	G_CONST_RETURN char *locale_encoding;
	g_get_charset(&locale_encoding);
	vte_terminal_set_encoding(terminal, locale_encoding, NULL);
}

static void
vte_sequence_handler_utf_8_character_set (VteTerminal *terminal, GValueArray *params)
{
	vte_terminal_set_encoding(terminal, NULL /* UTF-8 */, NULL);
}

/* Device status reports. The possible reports are the cursor position and
 * whether or not we're okay. */
static void
vte_sequence_handler_device_status_report (VteTerminal *terminal, GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
        long param, rowval, origin, rowmax;
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
                                if (screen->origin_mode &&
                                    screen->scrolling_restricted) {
                                        origin = screen->scrolling_region.start;
                                        rowmax = screen->scrolling_region.end;
                                } else {
                                        origin = 0;
                                        rowmax = terminal->pvt->row_count - 1;
                                }
                                rowval = screen->cursor_current.row - screen->insert_delta - origin;
                                rowval = CLAMP(rowval, 0, rowmax);
				g_snprintf(buf, sizeof(buf),
					   _VTE_CAP_CSI "%ld;%ldR",
                                           rowval + 1,
                                           CLAMP(screen->cursor_current.col + 1,
                                                 1, terminal->pvt->column_count));
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
        long param, rowval, origin, rowmax;
	char buf[128];

	screen = terminal->pvt->screen;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			param = g_value_get_long(value);
			switch (param) {
			case 6:
				/* Send the cursor position. */
                                if (screen->origin_mode &&
                                    screen->scrolling_restricted) {
                                        origin = screen->scrolling_region.start;
                                        rowmax = screen->scrolling_region.end;
                                } else {
                                        origin = 0;
                                        rowmax = terminal->pvt->row_count - 1;
                                }
                                rowval = screen->cursor_current.row - screen->insert_delta - origin;
                                rowval = CLAMP(rowval, 0, rowmax);
				g_snprintf(buf, sizeof(buf),
					   _VTE_CAP_CSI "?%ld;%ldR",
                                           rowval + 1,
                                           CLAMP(screen->cursor_current.col + 1,
                                                 1, terminal->pvt->column_count));
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
	VteRowData *rowdata;
	VteScreen *screen;
	VteCell cell;

	screen = terminal->pvt->screen;

	for (row = terminal->pvt->screen->insert_delta;
	     row < terminal->pvt->screen->insert_delta + terminal->pvt->row_count;
	     row++) {
		/* Find this row. */
		while (_vte_ring_next(screen->row_data) <= row)
			_vte_terminal_ring_append (terminal, FALSE);
		_vte_terminal_adjust_adjustments(terminal);
		rowdata = _vte_ring_index_writable (screen->row_data, row);
		g_assert(rowdata != NULL);
		/* Clear this row. */
		_vte_row_data_shrink (rowdata, 0);

		_vte_terminal_emit_text_deleted(terminal);
		/* Fill this row. */
		cell.c = 'E';
		cell.attr = basic_cell.cell.attr;
		cell.attr.columns = 1;
		_vte_row_data_fill (rowdata, &cell, terminal->pvt->column_count);
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
	GValue *value;
	GtkWidget *widget;
	char buf[128];
	long param, arg1, arg2;
	gint width, height;
	guint i;

	widget = &terminal->widget;

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
						"(to %ldx%ld pixels, grid size %ldx%ld).\n",
                                                 arg2, arg1,
                                                 arg2 / terminal->pvt->char_width,
                                                 arg1 / terminal->pvt->char_height);
				vte_terminal_emit_resize_window(terminal,
                                                                arg2 / terminal->pvt->char_width,
                                                                arg1 / terminal->pvt->char_height);
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
				vte_terminal_emit_resize_window(terminal, arg2, arg1);
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
				   1 + !gtk_widget_get_mapped(widget));
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting window state %s.\n",
					gtk_widget_get_mapped(widget) ?
					"non-iconified" : "iconified");
			vte_terminal_feed_child(terminal, buf, -1);
			break;
		case 13:
			/* Send window location, in pixels. */
			gdk_window_get_origin(gtk_widget_get_window(widget),
					      &width, &height);
			g_snprintf(buf, sizeof(buf),
				   _VTE_CAP_CSI "3;%d;%dt",
				   width + terminal->pvt->padding.left,
                                   height + terminal->pvt->padding.top);
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
                                   (int)(terminal->pvt->row_count * terminal->pvt->char_height),
                                   (int)(terminal->pvt->column_count * terminal->pvt->char_width));
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting window size "
					"(%dx%d)\n",
                                         (int)(terminal->pvt->row_count * terminal->pvt->char_height),
                                         (int)(terminal->pvt->column_count * terminal->pvt->char_width));

			vte_terminal_feed_child(terminal, buf, -1);
			break;
		case 18:
			/* Send widget size, in cells. */
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting widget size.\n");
			g_snprintf(buf, sizeof(buf),
				   _VTE_CAP_CSI "8;%ld;%ldt",
				   terminal->pvt->row_count,
				   terminal->pvt->column_count);
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
				   height / terminal->pvt->char_height,
				   width / terminal->pvt->char_width);
			vte_terminal_feed_child(terminal, buf, -1);
			break;
		case 20:
			/* Report a static icon title, since the real
			   icon title should NEVER be reported, as it
			   creates a security vulnerability.  See
			   http://marc.info/?l=bugtraq&m=104612710031920&w=2
			   and CVE-2003-0070. */
			_vte_debug_print(VTE_DEBUG_PARSE,
				"Reporting fake icon title.\n");
			/* never use terminal->pvt->icon_title here! */
			g_snprintf (buf, sizeof (buf),
				    _VTE_CAP_OSC "LTerminal" _VTE_CAP_ST);
			vte_terminal_feed_child(terminal, buf, -1);
			break;
		case 21:
			/* Report a static window title, since the real
			   window title should NEVER be reported, as it
			   creates a security vulnerability.  See
			   http://marc.info/?l=bugtraq&m=104612710031920&w=2
			   and CVE-2003-0070. */
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting fake window title.\n");
			/* never use terminal->pvt->window_title here! */
			g_snprintf (buf, sizeof (buf),
				    _VTE_CAP_OSC "lTerminal" _VTE_CAP_ST);
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
								terminal->pvt->column_count,
								param);
			}
			break;
		}
	}
}

/* Internal helper for setting/querying special colors */
static void
vte_sequence_handler_change_special_color_internal (VteTerminal *terminal, GValueArray *params,
						    int index, int index_fallback, int osc,
						    const char *terminator)
{
	gchar *name = NULL;
	GValue *value;
	PangoColor color;

	if (params != NULL && params->n_values > 0) {
		value = g_value_array_get_nth (params, 0);

		if (G_VALUE_HOLDS_STRING (value))
			name = g_value_dup_string (value);
		else if (G_VALUE_HOLDS_POINTER (value))
			name = vte_ucs4_to_utf8 (terminal, g_value_get_pointer (value));

		if (! name)
			return;

		if (vte_parse_color (name, &color))
			_vte_terminal_set_color_internal(terminal, index, VTE_COLOR_SOURCE_ESCAPE, &color);
		else if (strcmp (name, "?") == 0) {
			gchar buf[128];
			PangoColor *c = _vte_terminal_get_color(terminal, index);
			if (c == NULL && index_fallback != -1)
				c = _vte_terminal_get_color(terminal, index_fallback);
			g_assert(c != NULL);
			g_snprintf (buf, sizeof (buf),
				    _VTE_CAP_OSC "%d;rgb:%04x/%04x/%04x%s",
				    osc, c->red, c->green, c->blue, terminator);
			vte_terminal_feed_child (terminal, buf, -1);
		}

		g_free (name);
	}
}

/* Change the default foreground cursor, BEL terminated */
static void
vte_sequence_handler_change_foreground_color_bel (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_special_color_internal (terminal, params,
							    VTE_DEFAULT_FG, -1, 10, BEL);
}

/* Change the default foreground cursor, ST terminated */
static void
vte_sequence_handler_change_foreground_color_st (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_special_color_internal (terminal, params,
							    VTE_DEFAULT_FG, -1, 10, ST);
}

/* Reset the default foreground color */
static void
vte_sequence_handler_reset_foreground_color (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_set_color_internal(terminal, VTE_DEFAULT_FG, VTE_COLOR_SOURCE_ESCAPE, NULL);
}

/* Change the default background cursor, BEL terminated */
static void
vte_sequence_handler_change_background_color_bel (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_special_color_internal (terminal, params,
							    VTE_DEFAULT_BG, -1, 11, BEL);
}

/* Change the default background cursor, ST terminated */
static void
vte_sequence_handler_change_background_color_st (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_special_color_internal (terminal, params,
							    VTE_DEFAULT_BG, -1, 11, ST);
}

/* Reset the default background color */
static void
vte_sequence_handler_reset_background_color (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_set_color_internal(terminal, VTE_DEFAULT_BG, VTE_COLOR_SOURCE_ESCAPE, NULL);
}

/* Change the color of the cursor, BEL terminated */
static void
vte_sequence_handler_change_cursor_color_bel (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_special_color_internal (terminal, params,
							    VTE_CURSOR_BG, VTE_DEFAULT_FG, 12, BEL);
}

/* Change the color of the cursor, ST terminated */
static void
vte_sequence_handler_change_cursor_color_st (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_special_color_internal (terminal, params,
							    VTE_CURSOR_BG, VTE_DEFAULT_FG, 12, ST);
}

/* Reset the color of the cursor */
static void
vte_sequence_handler_reset_cursor_color (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_set_color_internal(terminal, VTE_CURSOR_BG, VTE_COLOR_SOURCE_ESCAPE, NULL);
}

/* Change the highlight background color, BEL terminated */
static void
vte_sequence_handler_change_highlight_background_color_bel (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_special_color_internal (terminal, params,
							    VTE_HIGHLIGHT_BG, VTE_DEFAULT_FG, 17, BEL);
}

/* Change the highlight background color, ST terminated */
static void
vte_sequence_handler_change_highlight_background_color_st (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_special_color_internal (terminal, params,
							    VTE_HIGHLIGHT_BG, VTE_DEFAULT_FG, 17, ST);
}

/* Reset the highlight background color */
static void
vte_sequence_handler_reset_highlight_background_color (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_set_color_internal(terminal, VTE_HIGHLIGHT_BG, VTE_COLOR_SOURCE_ESCAPE, NULL);
}

/* Change the highlight foreground color, BEL terminated */
static void
vte_sequence_handler_change_highlight_foreground_color_bel (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_special_color_internal (terminal, params,
							    VTE_HIGHLIGHT_FG, VTE_DEFAULT_BG, 19, BEL);
}

/* Change the highlight foreground color, ST terminated */
static void
vte_sequence_handler_change_highlight_foreground_color_st (VteTerminal *terminal, GValueArray *params)
{
	vte_sequence_handler_change_special_color_internal (terminal, params,
							    VTE_HIGHLIGHT_FG, VTE_DEFAULT_BG, 19, ST);
}

/* Reset the highlight foreground color */
static void
vte_sequence_handler_reset_highlight_foreground_color (VteTerminal *terminal, GValueArray *params)
{
	_vte_terminal_set_color_internal(terminal, VTE_HIGHLIGHT_FG, VTE_COLOR_SOURCE_ESCAPE, NULL);
}


/* Lookup tables */

#define VTE_SEQUENCE_HANDLER(name) name

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
	} else {
		const struct vteseq_n_struct *seqhandler;
		seqhandler = vteseq_n_lookup (name, len);
		return seqhandler ? seqhandler->handler : NULL;
	}
}


/* Handle a terminal control sequence and its parameters. */
void
_vte_terminal_handle_sequence(VteTerminal *terminal,
			      const char *match,
			      GValueArray *params)
{
	VteTerminalSequenceHandler handler;

	_VTE_DEBUG_IF(VTE_DEBUG_PARSE)
		display_control_sequence(match, params);

	/* Find the handler for this control sequence. */
	handler = _vte_sequence_get_handler (match);

	if (handler != NULL) {
		/* Let the handler handle it. */
		handler (terminal, params);
	} else {
		_vte_debug_print (VTE_DEBUG_MISC,
				  "No handler for control sequence `%s' defined.\n",
				  match);
	}
}
