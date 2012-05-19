/*
 * Copyright (C) 2001-2004 Red Hat, Inc.
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

/* A fake char cell size */
#define CHAR_WIDTH (8)
#define CHAR_HEIGHT (16)

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
/* FIXMEchpe: unify this with vte_buffer_find_charcell in vte.c */
static VteCell *
vte_buffer_find_charcell_writable(VteBuffer *buffer,
                                  glong col,
                                  glong row)
{
	VteRowData *rowdata;
	VteCell *ret = NULL;
	VteScreen *screen;

	screen = buffer->pvt->screen;
	if (_vte_ring_contains (screen->row_data, row)) {
		rowdata = _vte_ring_index_writable (screen->row_data, row);
		ret = _vte_row_data_get_writable (rowdata, col);
	}
	return ret;
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
vte_buffer_ucs4_to_utf8 (VteBuffer *buffer,
                         const guchar *in)
{
	gchar *out = NULL;
	guchar *buf = NULL, *bufptr = NULL;
	gsize inlen, outlen;
	VteConv conv;

	conv = _vte_conv_open ("UTF-8", VTE_CONV_GUNICHAR_TYPE);

	if (conv != VTE_INVALID_CONV) {
		inlen = vte_unichar_strlen ((gunichar *) in) * sizeof (gunichar);
		outlen = (inlen * VTE_UTF8_BPC) + 1;

		_vte_byte_array_set_minimum_size (buffer->pvt->conv_buffer, outlen);
		buf = bufptr = buffer->pvt->conv_buffer->data;

		if (_vte_conv (conv, &in, &inlen, &buf, &outlen) == (size_t) -1) {
			_vte_debug_print (VTE_DEBUG_IO,
					  "Error converting %ld string bytes (%s), skipping.\n",
					  (long) _vte_byte_array_length (buffer->pvt->outgoing),
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
vte_parse_color (const char *spec, GdkRGBA *rgba)
{
	gchar *spec_copy = (gchar *) spec;
	gboolean retval = FALSE;
        GdkColor color;

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

	retval = gdk_color_parse (spec_copy, &color);
	if (spec_copy != spec)
		g_free (spec_copy);

        if (!retval)
                return FALSE;

        rgba->red = color.red / 65535.;
        rgba->green = color.green / 65535.;
        rgba->blue = color.blue / 65535.;
        rgba->alpha = 1.;

        return TRUE;
}

/* Some common functions */

static void
_vte_buffer_home_cursor (VteBuffer *buffer)
{
	VteScreen *screen;
	screen = buffer->pvt->screen;
	screen->cursor_current.row = screen->insert_delta;
	screen->cursor_current.col = 0;
}

/* Clear the entire screen. */
static void
_vte_buffer_clear_screen (VteBuffer *buffer)
{
	long i, initial, row;
	VteScreen *screen;
	screen = buffer->pvt->screen;
	initial = screen->insert_delta;
	row = screen->cursor_current.row - screen->insert_delta;
	initial = _vte_ring_next(screen->row_data);
	/* Add a new screen's worth of rows. */
	for (i = 0; i < buffer->pvt->row_count; i++)
		_vte_buffer_ring_append (buffer, TRUE);
	/* Move the cursor and insertion delta to the first line in the
	 * newly-cleared area and scroll if need be. */
	screen->insert_delta = initial;
	screen->cursor_current.row = row + screen->insert_delta;
	_vte_buffer_view_adjust_adjustments(buffer);
	/* Redraw everything. */
	_vte_buffer_view_invalidate_all(buffer);
	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Clear the current line. */
static void
_vte_buffer_clear_current_line (VteBuffer *buffer)
{
	VteRowData *rowdata;
	VteScreen *screen;

	screen = buffer->pvt->screen;

	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = _vte_ring_index_writable (screen->row_data, screen->cursor_current.row);
		g_assert(rowdata != NULL);
		/* Remove it. */
		_vte_row_data_shrink (rowdata, 0);
		/* Add enough cells to the end of the line to fill out the row. */
		_vte_row_data_fill (rowdata, &screen->fill_defaults, buffer->pvt->column_count);
		rowdata->attr.soft_wrapped = 0;
		/* Repaint this row. */
		_vte_buffer_view_invalidate_cells(buffer,
				      0, buffer->pvt->column_count,
				      screen->cursor_current.row, 1);
	}

	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Clear above the current line. */
static void
_vte_buffer_clear_above_current (VteBuffer *buffer)
{
	VteRowData *rowdata;
	long i;
	VteScreen *screen;
	screen = buffer->pvt->screen;
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
			_vte_row_data_fill (rowdata, &screen->fill_defaults, buffer->pvt->column_count);
			rowdata->attr.soft_wrapped = 0;
			/* Repaint the row. */
			_vte_buffer_view_invalidate_cells(buffer,
					0, buffer->pvt->column_count, i, 1);
		}
	}
	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Scroll the text, but don't move the cursor.  Negative = up, positive = down. */
static void
_vte_buffer_scroll_text (VteBuffer *buffer,
                         int scroll_amount)
{
	long start, end, i;
	VteScreen *screen;

	screen = buffer->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = start + buffer->pvt->row_count - 1;
	}

	while (_vte_ring_next(screen->row_data) <= end)
		_vte_buffer_ring_append (buffer, FALSE);

	if (scroll_amount > 0) {
		for (i = 0; i < scroll_amount; i++) {
			_vte_buffer_ring_remove (buffer, end);
			_vte_buffer_ring_insert (buffer, start, TRUE);
		}
	} else {
		for (i = 0; i < -scroll_amount; i++) {
			_vte_buffer_ring_remove (buffer, start);
			_vte_buffer_ring_insert (buffer, end, TRUE);
		}
	}

	/* Update the display. */
	_vte_buffer_view_scroll_region(buffer, start, end - start + 1,
				   scroll_amount);

	/* Adjust the scrollbars if necessary. */
	_vte_buffer_view_adjust_adjustments(buffer);

	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_inserted_flag = TRUE;
	buffer->pvt->text_deleted_flag = TRUE;
}

static gboolean
vte_buffer_termcap_string_same_as_for (VteBuffer *buffer,
					 const char  *cap_str,
					 const char  *cap_other)
{
	char *other_str;
	gboolean ret;

	other_str = _vte_termcap_find_string(buffer->pvt->termcap,
					     buffer->pvt->emulation,
					     cap_other);

	ret = other_str && (g_ascii_strcasecmp(cap_str, other_str) == 0);

	g_free (other_str);

	return ret;
}

/* Set icon/window titles. */
static void
vte_sequence_handler_set_title_internal(VteBuffer *buffer,
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
			title = vte_buffer_ucs4_to_utf8(buffer, g_value_get_pointer (value));
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
				g_free (buffer->pvt->window_title_changed);
				buffer->pvt->window_title_changed = g_strdup (validated);
			}

			if (icon_title) {
				g_free (buffer->pvt->icon_title_changed);
				buffer->pvt->icon_title_changed = g_strdup (validated);
			}

			g_free (validated);
			g_free(title);
		}
	}
}

/* Toggle a terminal mode. */
static void
vte_sequence_handler_set_mode_internal(VteBuffer *buffer,
				       long setting, gboolean value)
{
	switch (setting) {
	case 2:		/* keyboard action mode (?) */
		break;
	case 4:		/* insert/overtype mode */
		buffer->pvt->screen->insert_mode = value;
		break;
	case 12:	/* send/receive mode (local echo) */
		buffer->pvt->screen->sendrecv_mode = value;
		break;
	case 20:	/* automatic newline / normal linefeed mode */
		buffer->pvt->screen->linefeed_mode = value;
		break;
	default:
		break;
	}
}


/*
 * Sequence handling boilerplate
 */

/* Typedef the handle type */
typedef void (*VteSequenceHandler) (VteBuffer *buffer, GValueArray *params);

/* Prototype all handlers... */
#define VTE_SEQUENCE_HANDLER(name) \
	static void name (VteBuffer *buffer, GValueArray *params);
#include "vteseq-list.h"
#undef VTE_SEQUENCE_HANDLER


/* Call another handler, offsetting any long arguments by the given
 * increment value. */
static void
vte_sequence_handler_offset(VteBuffer *buffer,
			    GValueArray *params,
			    int increment,
			    VteSequenceHandler handler)
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
	handler (buffer, params);
}

/* Call another function a given number of times, or once. */
static void
vte_sequence_handler_multiple_limited(VteBuffer *buffer,
                                      GValueArray *params,
                                      VteSequenceHandler handler,
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
		handler (buffer, NULL);
}

static void
vte_sequence_handler_multiple(VteBuffer *buffer,
                              GValueArray *params,
                              VteSequenceHandler handler)
{
        vte_sequence_handler_multiple_limited(buffer, params, handler, G_MAXUSHORT);
}

static void
vte_sequence_handler_multiple_r(VteBuffer *buffer,
                                GValueArray *params,
                                VteSequenceHandler handler)
{
        vte_sequence_handler_multiple_limited(buffer, params, handler,
                                              buffer->pvt->column_count - buffer->pvt->screen->cursor_current.col);
}

/* Manipulate certain terminal attributes. */
static void
vte_sequence_handler_decset_internal(VteBuffer *buffer,
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
		VteSequenceHandler reset, set;
	} settings[] = {
		/* 1: Application/normal cursor keys. */
		{1, NULL, &buffer->pvt->cursor_mode, NULL,
		 GINT_TO_POINTER(VTE_KEYMODE_NORMAL),
		 GINT_TO_POINTER(VTE_KEYMODE_APPLICATION),
		 NULL, NULL,},
		/* 2: disallowed, we don't do VT52. */
		{2, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 3: disallowed, window size is set by user. */
		{3, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 4: Smooth scroll. */
		{4, &buffer->pvt->smooth_scroll, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 5: Reverse video. */
		{5, &buffer->pvt->screen->reverse_mode, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 6: Origin mode: when enabled, cursor positioning is
		 * relative to the scrolling region. */
		{6, &buffer->pvt->screen->origin_mode, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 7: Wraparound mode. */
		{7, &buffer->pvt->flags.am, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 8: disallowed, keyboard repeat is set by user. */
		{8, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 9: Send-coords-on-click. */
		{9, NULL, &buffer->pvt->mouse_tracking_mode, NULL,
		 GINT_TO_POINTER(0),
		 GINT_TO_POINTER(MOUSE_TRACKING_SEND_XY_ON_CLICK),
		 NULL, NULL,},
		/* 12: disallowed, cursor blinks is set by user. */
		{12, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 18: print form feed. */
		/* 19: set print extent to full screen. */
		/* 25: Cursor visible. */
		{25, &buffer->pvt->cursor_visible, NULL, NULL,
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
		{42, &buffer->pvt->nrc_mode, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 44: Margin bell. */
		{44, &buffer->pvt->margin_bell, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 47: Alternate screen. */
		{47, NULL, NULL, (gpointer) &buffer->pvt->screen,
		 &buffer->pvt->normal_screen,
		 &buffer->pvt->alternate_screen,
		 NULL, NULL,},
		/* 66: Keypad mode. */
		{66, &buffer->pvt->keypad_mode, NULL, NULL,
		 GINT_TO_POINTER(VTE_KEYMODE_NORMAL),
		 GINT_TO_POINTER(VTE_KEYMODE_APPLICATION),
		 NULL, NULL,},
		/* 67: disallowed, backspace key policy is set by user. */
		{67, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 1000: Send-coords-on-button. */
		{1000, NULL, &buffer->pvt->mouse_tracking_mode, NULL,
		 GINT_TO_POINTER(0),
		 GINT_TO_POINTER(MOUSE_TRACKING_SEND_XY_ON_BUTTON),
		 NULL, NULL,},
		/* 1001: Hilite tracking. */
		{1001, NULL, &buffer->pvt->mouse_tracking_mode, NULL,
		 GINT_TO_POINTER(0),
		 GINT_TO_POINTER(MOUSE_TRACKING_HILITE_TRACKING),
		 NULL, NULL,},
		/* 1002: Cell motion tracking. */
		{1002, NULL, &buffer->pvt->mouse_tracking_mode, NULL,
		 GINT_TO_POINTER(0),
		 GINT_TO_POINTER(MOUSE_TRACKING_CELL_MOTION_TRACKING),
		 NULL, NULL,},
		/* 1003: All motion tracking. */
		{1003, NULL, &buffer->pvt->mouse_tracking_mode, NULL,
		 GINT_TO_POINTER(0),
		 GINT_TO_POINTER(MOUSE_TRACKING_ALL_MOTION_TRACKING),
		 NULL, NULL,},
		/* 1010/rxvt: disallowed, scroll-on-output is set by user. */
		{1010, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 1011/rxvt: disallowed, scroll-on-keypress is set by user. */
		{1011, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
                /* 1015/urxvt: Extended mouse coordinates. */
                {1015, &buffer->pvt->mouse_urxvt_extension, NULL, NULL,
                 GINT_TO_POINTER(FALSE),
                 GINT_TO_POINTER(TRUE),
                 NULL, NULL,},
		/* 1035: disallowed, don't know what to do with it. */
		{1035, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 1036: Meta-sends-escape. */
		{1036, &buffer->pvt->meta_sends_escape, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 1037: disallowed, delete key policy is set by user. */
		{1037, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
		/* 1047: Use alternate screen buffer. */
		{1047, NULL, NULL, (gpointer) &buffer->pvt->screen,
		 &buffer->pvt->normal_screen,
		 &buffer->pvt->alternate_screen,
		 NULL, NULL,},
		/* 1048: Save/restore cursor position. */
		{1048, NULL, NULL, NULL,
		 NULL,
		 NULL,
		 vte_sequence_handler_rc,
		 vte_sequence_handler_sc,},
		/* 1049: Use alternate screen buffer, saving the cursor
		 * position. */
		{1049, NULL, NULL, (gpointer) &buffer->pvt->screen,
		 &buffer->pvt->normal_screen,
		 &buffer->pvt->alternate_screen,
		 vte_sequence_handler_rc,
		 vte_sequence_handler_sc,},
		/* 1051: Sun function key mode. */
		{1051, NULL, NULL, (gpointer) &buffer->pvt->sun_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
		/* 1052: HP function key mode. */
		{1052, NULL, NULL, (gpointer) &buffer->pvt->hp_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
		/* 1060: Legacy function key mode. */
		{1060, NULL, NULL, (gpointer) &buffer->pvt->legacy_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
		/* 1061: VT220 function key mode. */
		{1061, NULL, NULL, (gpointer) &buffer->pvt->vt220_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
		/* 2004: Bracketed paste mode. */
		{2004, &buffer->pvt->screen->bracketed_paste_mode, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
	};

        VteView *terminal;

        terminal = buffer->pvt->terminal;
        /* FIXMEchpe cope with NULL here !!! */
        
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
			p = g_hash_table_lookup(buffer->pvt->dec_saved,
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
			g_hash_table_insert(buffer->pvt->dec_saved,
					    GINT_TO_POINTER(setting),
					    GINT_TO_POINTER(set));
		}
		/* Change the current setting to match the new/saved value. */
		if (!save) {
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Setting %d to %s.\n",
					setting, set ? "set" : "unset");
			if (settings[i].set && set) {
				settings[i].set (buffer, NULL);
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
				settings[i].reset (buffer, NULL);
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
#if 0		/* 3: disallowed, window size is set by user. */
	case 3:
		_vte_buffer_emit_resize_window(buffer,
						set ? 132 : 80,
						buffer->pvt->row_count);
		/* Request a resize and redraw. */
		_vte_buffer_view_invalidate_all(buffer);
		break;
#endif
	case 5:
		/* Repaint everything in reverse mode. */
                _vte_buffer_view_invalidate_all(buffer);
		break;
	case 6:
		/* Reposition the cursor in its new home position. */
		buffer->pvt->screen->cursor_current.col = 0;
		buffer->pvt->screen->cursor_current.row =
			buffer->pvt->screen->insert_delta;
		break;
	case 47:
	case 1047:
	case 1049:
		/* Clear the alternate screen if we're switching
		 * to it, and home the cursor. */
		if (set) {
			_vte_buffer_clear_screen (buffer);
			_vte_buffer_home_cursor (buffer);
		}
		/* Reset scrollbars and repaint everything. */
		gtk_adjustment_set_value(terminal->pvt->vadjustment,
					 buffer->pvt->screen->scroll_delta);
		vte_buffer_set_scrollback_lines(buffer,
				buffer->pvt->scrollback_lines);
                _vte_buffer_queue_contents_changed(buffer);
                _vte_buffer_view_invalidate_all(buffer);
		break;
	case 9:
	case 1000:
	case 1001:
	case 1002:
	case 1003:
		/* Make the pointer visible. */
                _vte_view_set_pointer_visible(buffer->pvt->terminal, TRUE);
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
vte_sequence_handler_ae (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->alternate_charset = FALSE;
}

/* Add a line at the current cursor position. */
static void
vte_sequence_handler_al (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	long start, end, param, i;
	GValue *value;

	/* Find out which part of the screen we're messing with. */
	screen = buffer->pvt->screen;
	start = screen->cursor_current.row;
	if (screen->scrolling_restricted) {
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		end = screen->insert_delta + buffer->pvt->row_count - 1;
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
		_vte_buffer_ring_remove (buffer, end);
                _vte_buffer_ring_insert (buffer, start, TRUE);
		/* Adjust the scrollbars if necessary. */
                _vte_buffer_view_adjust_adjustments(buffer);
	}

	/* Update the display. */
        _vte_buffer_view_scroll_region(buffer, start, end - start + 1, param);

	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Add N lines at the current cursor position. */
static void
vte_sequence_handler_AL (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_al (buffer, params);
}

/* Start using alternate character set. */
static void
vte_sequence_handler_as (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->alternate_charset = TRUE;
}

/* Beep. */
static void
vte_sequence_handler_bl (VteBuffer *buffer, GValueArray *params)
{
        _vte_buffer_emit_bell(buffer, VTE_BELL_AUDIBLE);
        /* FIXMEchpe: also emit visual bell here?? */
}

/* Backtab. */
static void
vte_sequence_handler_bt (VteBuffer *buffer, GValueArray *params)
{
	long newcol;

	/* Calculate which column is the previous tab stop. */
	newcol = buffer->pvt->screen->cursor_current.col;

	if (buffer->pvt->tabstops != NULL) {
		/* Find the next tabstop. */
		while (newcol > 0) {
			newcol--;
			if (_vte_buffer_get_tabstop(buffer,
						     newcol % buffer->pvt->column_count)) {
				break;
			}
		}
	}

	/* Warp the cursor. */
	_vte_debug_print(VTE_DEBUG_PARSE,
			"Moving cursor to column %ld.\n", (long)newcol);
	buffer->pvt->screen->cursor_current.col = newcol;
}

/* Clear from the cursor position to the beginning of the line. */
static void
vte_sequence_handler_cb (VteBuffer *buffer, GValueArray *params)
{
	VteRowData *rowdata;
	long i;
	VteScreen *screen;
	VteCell *pcell;
	screen = buffer->pvt->screen;

	/* Get the data for the row which the cursor points to. */
	rowdata = _vte_buffer_ensure_row(buffer);
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
        _vte_buffer_view_invalidate_cells(buffer,
			      0, screen->cursor_current.col+1,
			      screen->cursor_current.row, 1);

	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Clear to the right of the cursor and below the current line. */
static void
vte_sequence_handler_cd (VteBuffer *buffer, GValueArray *params)
{
	VteRowData *rowdata;
	glong i;
	VteScreen *screen;

	screen = buffer->pvt->screen;
	/* If the cursor is actually on the screen, clear the rest of the
	 * row the cursor is on and all of the rows below the cursor. */
	i = screen->cursor_current.row;
	if (i < _vte_ring_next(screen->row_data)) {
		/* Get the data for the row we're clipping. */
		rowdata = _vte_ring_index_writable (screen->row_data, i);
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
	     i < screen->insert_delta + buffer->pvt->row_count;
	     i++) {
		/* Retrieve the row's data, creating it if necessary. */
		if (_vte_ring_contains (screen->row_data, i)) {
			rowdata = _vte_ring_index_writable (screen->row_data, i);
			g_assert(rowdata != NULL);
		} else {
			rowdata = _vte_buffer_ring_append (buffer, FALSE);
		}
		/* Pad out the row. */
		_vte_row_data_fill (rowdata, &screen->fill_defaults, buffer->pvt->column_count);
		rowdata->attr.soft_wrapped = 0;
		/* Repaint this row. */
                _vte_buffer_view_invalidate_cells(buffer,
				      0, buffer->pvt->column_count,
				      i, 1);
	}

	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Clear from the cursor position to the end of the line. */
static void
vte_sequence_handler_ce (VteBuffer *buffer, GValueArray *params)
{
	VteRowData *rowdata;
	VteScreen *screen;

	screen = buffer->pvt->screen;
	/* Get the data for the row which the cursor points to. */
	rowdata = _vte_buffer_ensure_row(buffer);
	g_assert(rowdata != NULL);
	/* Remove the data at the end of the array until the current column
	 * is the end of the array. */
	if ((glong) _vte_row_data_length (rowdata) > screen->cursor_current.col) {
		_vte_row_data_shrink (rowdata, screen->cursor_current.col);
		/* We've modified the display.  Make a note of it. */
		buffer->pvt->text_deleted_flag = TRUE;
	}
	if (screen->fill_defaults.attr.back != VTE_DEF_BG) {
		/* Add enough cells to fill out the row. */
		_vte_row_data_fill (rowdata, &screen->fill_defaults, buffer->pvt->column_count);
	}
	rowdata->attr.soft_wrapped = 0;
	/* Repaint this row. */
        _vte_buffer_view_invalidate_cells(buffer,
			      screen->cursor_current.col,
			      buffer->pvt->column_count -
			      screen->cursor_current.col,
			      screen->cursor_current.row, 1);
}

/* Move the cursor to the given column (horizontal position). */
static void
vte_sequence_handler_ch (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	GValue *value;
	long val;

	screen = buffer->pvt->screen;
	/* We only care if there's a parameter in there. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = CLAMP(g_value_get_long(value),
				    0,
				    buffer->pvt->column_count - 1);
			/* Move the cursor. */
			screen->cursor_current.col = val;
			_vte_buffer_cleanup_tab_fragments_at_cursor (buffer);
		}
	}
}

/* Clear the screen and home the cursor. */
static void
vte_sequence_handler_cl (VteBuffer *buffer, GValueArray *params)
{
	_vte_buffer_clear_screen (buffer);
        _vte_buffer_home_cursor (buffer);

	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Move the cursor to the given position. */
static void
vte_sequence_handler_cm (VteBuffer *buffer, GValueArray *params)
{
	GValue *row, *col;
	long rowval, colval, origin;
	VteScreen *screen;

	screen = buffer->pvt->screen;

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
			rowval = CLAMP(rowval, 0, buffer->pvt->row_count - 1);
		}
		if (params->n_values >= 2) {
			col = g_value_array_get_nth(params, 1);
			if (G_VALUE_HOLDS_LONG(col)) {
				colval = g_value_get_long(col);
				colval = CLAMP(colval, 0, buffer->pvt->column_count - 1);
			}
		}
	}
	screen->cursor_current.row = rowval + screen->insert_delta;
	screen->cursor_current.col = colval;
	_vte_buffer_cleanup_tab_fragments_at_cursor (buffer);
}

/* Carriage return. */
static void
vte_sequence_handler_cr (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->cursor_current.col = 0;
}

/* Restrict scrolling and updates to a subset of the visible lines. */
static void
vte_sequence_handler_cs (VteBuffer *buffer, GValueArray *params)
{
	long start=-1, end=-1, rows;
	GValue *value;
	VteScreen *screen;

        _vte_buffer_home_cursor (buffer);

	/* We require two parameters.  Anything less is a reset. */
	screen = buffer->pvt->screen;
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
	rows = buffer->pvt->row_count;
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
}

/* Restrict scrolling and updates to a subset of the visible lines, because
 * GNU Emacs is special. */
static void
vte_sequence_handler_cS (VteBuffer *buffer, GValueArray *params)
{
	long start=0, end=buffer->pvt->row_count-1, rows;
	GValue *value;
	VteScreen *screen;

	/* We require four parameters. */
	screen = buffer->pvt->screen;
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
	rows = buffer->pvt->row_count;
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
vte_sequence_handler_ct (VteBuffer *buffer, GValueArray *params)
{
        _vte_buffer_clear_tabstops(buffer);
}

/* Move the cursor to the lower left-hand corner. */
static void
vte_sequence_handler_cursor_lower_left (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	long row;
	screen = buffer->pvt->screen;
	row = MAX(0, buffer->pvt->row_count - 1);
	screen->cursor_current.row = screen->insert_delta + row;
	screen->cursor_current.col = 0;
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static void
vte_sequence_handler_cursor_next_line (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->cursor_current.col = 0;
	vte_sequence_handler_DO (buffer, params);
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static void
vte_sequence_handler_cursor_preceding_line (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->cursor_current.col = 0;
	vte_sequence_handler_UP (buffer, params);
}

/* Move the cursor to the given row (vertical position). */
static void
vte_sequence_handler_cv (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	GValue *value;
	long val, origin;
	screen = buffer->pvt->screen;
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
			val = CLAMP(val, 0, buffer->pvt->row_count - 1);
			screen->cursor_current.row = screen->insert_delta + val;
		}
	}
}

/* Delete a character at the current cursor position. */
static void
vte_sequence_handler_dc (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	VteRowData *rowdata;
	long col;

	screen = buffer->pvt->screen;

	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		long len;
		/* Get the data for the row which the cursor points to. */
		rowdata = _vte_ring_index_writable (screen->row_data, screen->cursor_current.row);
		g_assert(rowdata != NULL);
		col = screen->cursor_current.col;
		len = _vte_row_data_length (rowdata);
		/* Remove the column. */
		if (col < len) {
			_vte_row_data_remove (rowdata, col);
			if (screen->fill_defaults.attr.back != VTE_DEF_BG) {
				_vte_row_data_fill (rowdata, &screen->fill_defaults, buffer->pvt->column_count);
				len = buffer->pvt->column_count;
			}
			/* Repaint this row. */
			_vte_buffer_view_invalidate_cells(buffer,
					col, len - col,
					screen->cursor_current.row, 1);
		}
	}

	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Delete N characters at the current cursor position. */
static void
vte_sequence_handler_DC (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_multiple_r(buffer, params, vte_sequence_handler_dc);
}

/* Delete a line at the current cursor position. */
static void
vte_sequence_handler_dl (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	long start, end, param, i;
	GValue *value;

	/* Find out which part of the screen we're messing with. */
	screen = buffer->pvt->screen;
	start = screen->cursor_current.row;
	if (screen->scrolling_restricted) {
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		end = screen->insert_delta + buffer->pvt->row_count - 1;
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
		_vte_buffer_ring_remove (buffer, start);
		_vte_buffer_ring_insert (buffer, end, TRUE);
		/* Adjust the scrollbars if necessary. */
                _vte_buffer_view_adjust_adjustments(buffer);
	}

	/* Update the display. */
        _vte_buffer_view_scroll_region(buffer, start, end - start + 1, -param);

	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Delete N lines at the current cursor position. */
static void
vte_sequence_handler_DL (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_dl (buffer, params);
}

/* Cursor down, no scrolling. */
static void
vte_sequence_handler_do (VteBuffer *buffer, GValueArray *params)
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

	/* Move the cursor down. */
	screen->cursor_current.row = MIN(screen->cursor_current.row + 1, end);
}

/* Cursor down, no scrolling. */
static void
vte_sequence_handler_DO (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_multiple(buffer, params, vte_sequence_handler_do);
}

/* Start using alternate character set. */
static void
vte_sequence_handler_eA (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_ae (buffer, params);
}

/* Erase characters starting at the cursor position (overwriting N with
 * spaces, but not moving the cursor). */
static void
vte_sequence_handler_ec (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	VteRowData *rowdata;
	GValue *value;
	VteCell *cell;
	long col, i, count;

	screen = buffer->pvt->screen;

	/* If we got a parameter, use it. */
	count = 1;
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			count = g_value_get_long(value);
		}
	}

	/* Clear out the given number of characters. */
	rowdata = _vte_buffer_ensure_row(buffer);
	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		g_assert(rowdata != NULL);
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
		_vte_buffer_view_invalidate_cells(buffer,
				      screen->cursor_current.col, count,
				      screen->cursor_current.row, 1);
	}

	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* End insert mode. */
static void
vte_sequence_handler_ei (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->insert_mode = FALSE;
}

/* Form-feed / next-page. */
static void
vte_sequence_handler_form_feed (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_index (buffer, params);
}

/* Move from status line. */
static void
vte_sequence_handler_fs (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->status_line = FALSE;
}

/* Move the cursor to the home position. */
static void
vte_sequence_handler_ho (VteBuffer *buffer, GValueArray *params)
{
	_vte_buffer_home_cursor (buffer);
}

/* Move the cursor to a specified position. */
static void
vte_sequence_handler_horizontal_and_vertical_position (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_offset(buffer, params, -1, vte_sequence_handler_cm);
}

/* Insert a character. */
static void
vte_sequence_handler_ic (VteBuffer *buffer, GValueArray *params)
{
	VteVisualPosition save;
	VteScreen *screen;

	screen = buffer->pvt->screen;

	save = screen->cursor_current;

	_vte_buffer_insert_char(buffer, ' ', TRUE, TRUE);

	screen->cursor_current = save;
}

/* Insert N characters. */
static void
vte_sequence_handler_IC (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_multiple_r(buffer, params, vte_sequence_handler_ic);
}

/* Begin insert mode. */
static void
vte_sequence_handler_im (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->insert_mode = TRUE;
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_index (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_sf (buffer, params);
}

/* Send me a backspace key sym, will you?  Guess that the application meant
 * to send the cursor back one position. */
static void
vte_sequence_handler_kb (VteBuffer *buffer, GValueArray *params)
{
	/* Move the cursor left. */
	vte_sequence_handler_le (buffer, params);
}

/* Keypad mode end. */
static void
vte_sequence_handler_ke (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->keypad_mode = VTE_KEYMODE_NORMAL;
}

/* Keypad mode start. */
static void
vte_sequence_handler_ks (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->keypad_mode = VTE_KEYMODE_APPLICATION;
}

/* Cursor left. */
static void
vte_sequence_handler_le (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;

	screen = buffer->pvt->screen;
	if (screen->cursor_current.col > 0) {
		/* There's room to move left, so do so. */
		screen->cursor_current.col--;
		_vte_buffer_cleanup_tab_fragments_at_cursor (buffer);
	} else {
		if (buffer->pvt->flags.bw) {
			/* Wrap to the previous line. */
			screen->cursor_current.col = buffer->pvt->column_count - 1;
			if (screen->scrolling_restricted) {
				vte_sequence_handler_sr (buffer, params);
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
vte_sequence_handler_LE (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_multiple(buffer, params, vte_sequence_handler_le);
}

/* Move the cursor to the lower left corner of the display. */
static void
vte_sequence_handler_ll (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	screen = buffer->pvt->screen;
	screen->cursor_current.row = MAX(screen->insert_delta,
					 screen->insert_delta +
					 buffer->pvt->row_count - 1);
	screen->cursor_current.col = 0;
}

/* Blink on. */
static void
vte_sequence_handler_mb (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->defaults.attr.blink = 1;
}

/* Bold on. */
static void
vte_sequence_handler_md (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->defaults.attr.bold = 1;
	buffer->pvt->screen->defaults.attr.half = 0;
}

/* End modes. */
static void
vte_sequence_handler_me (VteBuffer *buffer, GValueArray *params)
{
	_vte_screen_set_default_attributes(buffer->pvt->screen);
}

/* Half-bright on. */
static void
vte_sequence_handler_mh (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->defaults.attr.half = 1;
	buffer->pvt->screen->defaults.attr.bold = 0;
}

/* Invisible on. */
static void
vte_sequence_handler_mk (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->defaults.attr.invisible = 1;
}

/* Protect on. */
static void
vte_sequence_handler_mp (VteBuffer *buffer, GValueArray *params)
{
	/* unused; bug 499893
	buffer->pvt->screen->defaults.attr.protect = 1;
	 */
}

/* Reverse on. */
static void
vte_sequence_handler_mr (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->defaults.attr.reverse = 1;
}

/* Cursor right. */
static void
vte_sequence_handler_nd (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	screen = buffer->pvt->screen;
	if ((screen->cursor_current.col + 1) < buffer->pvt->column_count) {
		/* There's room to move right. */
		screen->cursor_current.col++;
	}
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static void
vte_sequence_handler_next_line (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->cursor_current.col = 0;
	vte_sequence_handler_DO (buffer, params);
}

/* No-op. */
static void
vte_sequence_handler_noop (VteBuffer *buffer, GValueArray *params)
{
}

/* Carriage return command(?). */
static void
vte_sequence_handler_nw (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_cr (buffer, params);
}

/* Restore cursor (position). */
static void
vte_sequence_handler_rc (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	screen = buffer->pvt->screen;
	screen->cursor_current.col = screen->cursor_saved.col;
	screen->cursor_current.row = CLAMP(screen->cursor_saved.row +
					   screen->insert_delta,
					   screen->insert_delta,
					   screen->insert_delta +
					   buffer->pvt->row_count - 1);
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_reverse_index (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_sr (buffer, params);
}

/* Cursor right N characters. */
static void
vte_sequence_handler_RI (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_multiple_r(buffer, params, vte_sequence_handler_nd);
}

/* Save cursor (position). */
static void
vte_sequence_handler_sc (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	screen = buffer->pvt->screen;
	screen->cursor_saved.col = screen->cursor_current.col;
	screen->cursor_saved.row = CLAMP(screen->cursor_current.row -
					 screen->insert_delta,
					 0, buffer->pvt->row_count - 1);
}

/* Scroll the text down, but don't move the cursor. */
static void
vte_sequence_handler_scroll_down (VteBuffer *buffer, GValueArray *params)
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

	_vte_buffer_scroll_text (buffer, val);
}

/* change color in the palette */
static void
vte_sequence_handler_change_color (VteBuffer *buffer, GValueArray *params)
{
	gchar **pairs, *str = NULL;
	GValue *value;
	GdkRGBA color;
	guint idx, i;

	if (params != NULL && params->n_values > 0) {
		value = g_value_array_get_nth (params, 0);

		if (G_VALUE_HOLDS_STRING (value))
			str = g_value_dup_string (value);
		else if (G_VALUE_HOLDS_POINTER (value))
			str = vte_buffer_ucs4_to_utf8(buffer, g_value_get_pointer (value));

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
				buffer->pvt->palette[idx] = color;
                                VTE_PALETTE_SET_OVERRIDE(buffer->pvt->palette_set, idx);
			} else if (strcmp (pairs[i + 1], "?") == 0) {
				gchar buf[128];
				g_snprintf (buf, sizeof (buf),
					    _VTE_CAP_OSC "4;%u;rgb:%04x/%04x/%04x" BEL, idx,
					    (guint) (buffer->pvt->palette[idx].red * 65535.),
                                            (guint) (buffer->pvt->palette[idx].green * 65535.),
                                            (guint) (buffer->pvt->palette[idx].blue * 65535.));
				vte_buffer_feed_child (buffer, buf, -1);
			}
		}

		g_free (str);
		g_strfreev (pairs);

		/* emit the refresh as the palette has changed and previous
		 * renders need to be updated. */
		_vte_buffer_emit_refresh_window (buffer);
	}
}

/* Scroll the text up, but don't move the cursor. */
static void
vte_sequence_handler_scroll_up (VteBuffer *buffer, GValueArray *params)
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

	_vte_buffer_scroll_text (buffer, -val);
}

/* Standout end. */
static void
vte_sequence_handler_se (VteBuffer *buffer, GValueArray *params)
{
	char *standout;

	/* Standout may be mapped to another attribute, so attempt to do
	 * the Right Thing here.
	 *
	 * If the standout sequence is the same as another sequence, do what
	 * we'd do for that other sequence instead. */

	standout = _vte_termcap_find_string(buffer->pvt->termcap,
					    buffer->pvt->emulation,
					    "so");
	g_assert(standout != NULL);

	if (vte_buffer_termcap_string_same_as_for (buffer, standout, "mb") /* blink */   ||
	    vte_buffer_termcap_string_same_as_for (buffer, standout, "md") /* bold */    ||
	    vte_buffer_termcap_string_same_as_for (buffer, standout, "mh") /* half */    ||
	    vte_buffer_termcap_string_same_as_for (buffer, standout, "mr") /* reverse */ ||
	    vte_buffer_termcap_string_same_as_for (buffer, standout, "us") /* underline */)
	{
		vte_sequence_handler_me (buffer, params);
	} else {
		/* Otherwise just set standout mode. */
		buffer->pvt->screen->defaults.attr.standout = 0;
	}

	g_free(standout);
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_sf (VteBuffer *buffer, GValueArray *params)
{
	_vte_buffer_cursor_down (buffer);
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_SF (VteBuffer *buffer, GValueArray *params)
{
	/* XXX implement this directly in _vte_view_cursor_down */
	vte_sequence_handler_multiple(buffer, params, vte_sequence_handler_sf);
}

/* Standout start. */
static void
vte_sequence_handler_so (VteBuffer *buffer, GValueArray *params)
{
	char *standout;

	/* Standout may be mapped to another attribute, so attempt to do
	 * the Right Thing here.
	 *
	 * If the standout sequence is the same as another sequence, do what
	 * we'd do for that other sequence instead. */

	standout = _vte_termcap_find_string(buffer->pvt->termcap,
					    buffer->pvt->emulation,
					    "so");
	g_assert(standout != NULL);

	if (vte_buffer_termcap_string_same_as_for (buffer, standout, "mb") /* blink */)
		vte_sequence_handler_mb (buffer, params);
	else if (vte_buffer_termcap_string_same_as_for (buffer, standout, "md") /* bold */)
		vte_sequence_handler_md (buffer, params);
	else if (vte_buffer_termcap_string_same_as_for (buffer, standout, "mh") /* half */)
		vte_sequence_handler_mh (buffer, params);
	else if (vte_buffer_termcap_string_same_as_for (buffer, standout, "mr") /* reverse */)
		vte_sequence_handler_mr (buffer, params);
	else if (vte_buffer_termcap_string_same_as_for (buffer, standout, "us") /* underline */)
		vte_sequence_handler_us (buffer, params);
	else {
		/* Otherwise just set standout mode. */
		buffer->pvt->screen->defaults.attr.standout = 1;
	}

	g_free(standout);
}

/* Cursor up, scrolling if need be. */
static void
vte_sequence_handler_sr (VteBuffer *buffer, GValueArray *params)
{
	long start, end;
	VteScreen *screen;

	screen = buffer->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->scrolling_region.start + screen->insert_delta;
		end = screen->scrolling_region.end + screen->insert_delta;
	} else {
		start = buffer->pvt->screen->insert_delta;
		end = start + buffer->pvt->row_count - 1;
	}

	if (screen->cursor_current.row == start) {
		/* If we're at the top of the scrolling region, add a
		 * line at the top to scroll the bottom off. */
		_vte_buffer_ring_remove (buffer, end);
		_vte_buffer_ring_insert (buffer, start, TRUE);
		/* Update the display. */
                _vte_buffer_view_scroll_region(buffer, start, end - start + 1, 1);
                _vte_buffer_view_invalidate_cells(buffer,
				      0, buffer->pvt->column_count,
				      start, 2);
	} else {
		/* Otherwise, just move the cursor up. */
		screen->cursor_current.row--;
	}
	/* Adjust the scrollbars if necessary. */
        _vte_buffer_view_adjust_adjustments(buffer);
	/* We modified the display, so make a note of it. */
	buffer->pvt->text_modified_flag = TRUE;
}

/* Cursor up, with scrolling. */
static void
vte_sequence_handler_SR (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_multiple(buffer, params, vte_sequence_handler_sr);
}

/* Set tab stop in the current column. */
static void
vte_sequence_handler_st (VteBuffer *buffer, GValueArray *params)
{
	if (buffer->pvt->tabstops == NULL) {
		buffer->pvt->tabstops = g_hash_table_new(NULL, NULL);
	}
	_vte_buffer_set_tabstop(buffer,
				buffer->pvt->screen->cursor_current.col);
}

/* Tab. */
static void
vte_sequence_handler_ta (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	long old_len, newcol, col;

	/* Calculate which column is the next tab stop. */
	screen = buffer->pvt->screen;
	newcol = col = screen->cursor_current.col;

	g_assert (col >= 0);

	if (buffer->pvt->tabstops != NULL) {
		/* Find the next tabstop. */
		for (newcol++; newcol < VTE_TAB_MAX; newcol++) {
			if (_vte_buffer_get_tabstop(buffer, newcol)) {
				break;
			}
		}
	}

	/* If we have no tab stops or went past the end of the line, stop
	 * at the right-most column. */
	if (newcol >= buffer->pvt->column_count) {
		newcol = buffer->pvt->column_count - 1;
	}

	/* but make sure we don't move cursor back (bug #340631) */
	if (col < newcol) {
		VteRowData *rowdata = _vte_buffer_ensure_row (buffer);

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
			if (!found) {
				VteCell *cell = _vte_row_data_get_writable (rowdata, col);
				VteCell tab = *cell;
				tab.attr.columns = newcol - col;
				tab.c = '\t';
				/* Check if it fits in columns */
				if (tab.attr.columns == newcol - col) {
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
		}

		_vte_buffer_view_invalidate_cells(buffer,
				screen->cursor_current.col,
				newcol - screen->cursor_current.col,
				screen->cursor_current.row, 1);
		screen->cursor_current.col = newcol;
	}
}

/* Clear tabs selectively. */
static void
vte_sequence_handler_tab_clear (VteBuffer *buffer, GValueArray *params)
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
		_vte_buffer_clear_tabstop(buffer,
					  buffer->pvt->screen->cursor_current.col);
	} else
	if (param == 3) {
                _vte_buffer_clear_tabstops(buffer);
	}
}

/* Move to status line. */
static void
vte_sequence_handler_ts (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->status_line = TRUE;
	buffer->pvt->screen->status_line_changed = TRUE;
	g_string_truncate(buffer->pvt->screen->status_line_contents, 0);
}

/* Underline this character and move right. */
static void
vte_sequence_handler_uc (VteBuffer *buffer, GValueArray *params)
{
	VteCell *cell;
	int column;
	VteScreen *screen;

	screen = buffer->pvt->screen;
	column = screen->cursor_current.col;
	cell = vte_buffer_find_charcell_writable(buffer, column, screen->cursor_current.row);
	while ((cell != NULL) && (cell->attr.fragment) && (column > 0)) {
		column--;
                cell = vte_buffer_find_charcell_writable(buffer, column, screen->cursor_current.row);
	}
	if (cell != NULL) {
		/* Set this character to be underlined. */
		cell->attr.underline = 1;
		/* Cause the character to be repainted. */
                _vte_buffer_view_invalidate_cells(buffer,
				      column, cell->attr.columns,
				      screen->cursor_current.row, 1);
		/* Move the cursor right. */
		vte_sequence_handler_nd (buffer, params);
	}

	/* We've modified the display without changing the text.  Make a note
	 * of it. */
	buffer->pvt->text_modified_flag = TRUE;
}

/* Underline end. */
static void
vte_sequence_handler_ue (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->defaults.attr.underline = 0;
}

/* Cursor up, no scrolling. */
static void
vte_sequence_handler_up (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	long start, end;

	screen = buffer->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = start + buffer->pvt->row_count - 1;
	}

	screen->cursor_current.row = MAX(screen->cursor_current.row - 1, start);
}

/* Cursor up N lines, no scrolling. */
static void
vte_sequence_handler_UP (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_multiple(buffer, params, vte_sequence_handler_up);
}

/* Underline start. */
static void
vte_sequence_handler_us (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->screen->defaults.attr.underline = 1;
}

/* Visible bell. */
static void
vte_sequence_handler_vb (VteBuffer *buffer, GValueArray *params)
{
        _vte_buffer_emit_bell(buffer, VTE_BELL_VISUAL);
}

/* Cursor visible. */
static void
vte_sequence_handler_ve (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->cursor_visible = TRUE;
}

/* Vertical tab. */
static void
vte_sequence_handler_vertical_tab (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_index (buffer, params);
}

/* Cursor invisible. */
static void
vte_sequence_handler_vi (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->cursor_visible = FALSE;
}

/* Cursor standout. */
static void
vte_sequence_handler_vs (VteBuffer *buffer, GValueArray *params)
{
	buffer->pvt->cursor_visible = TRUE; /* FIXME: should be *more*
						 visible. */
}

/* Handle ANSI color setting and related stuffs (SGR). */
static void
vte_sequence_handler_character_attributes (VteBuffer *buffer, GValueArray *params)
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
			_vte_screen_set_default_attributes(buffer->pvt->screen);
			break;
		case 1:
			buffer->pvt->screen->defaults.attr.bold = 1;
			buffer->pvt->screen->defaults.attr.half = 0;
			break;
		case 2:
			buffer->pvt->screen->defaults.attr.half = 1;
			buffer->pvt->screen->defaults.attr.bold = 0;
			break;
		case 4:
			buffer->pvt->screen->defaults.attr.underline = 1;
			break;
		case 5:
			buffer->pvt->screen->defaults.attr.blink = 1;
			break;
		case 7:
			buffer->pvt->screen->defaults.attr.reverse = 1;
			break;
		case 8:
			buffer->pvt->screen->defaults.attr.invisible = 1;
			break;
		case 9:
			buffer->pvt->screen->defaults.attr.strikethrough = 1;
			break;
		case 21: /* Error in old versions of linux console. */
		case 22: /* ECMA 48. */
			buffer->pvt->screen->defaults.attr.bold = 0;
			buffer->pvt->screen->defaults.attr.half = 0;
			break;
		case 24:
			buffer->pvt->screen->defaults.attr.underline = 0;
			break;
		case 25:
			buffer->pvt->screen->defaults.attr.blink = 0;
			break;
		case 27:
			buffer->pvt->screen->defaults.attr.reverse = 0;
			break;
		case 28:
			buffer->pvt->screen->defaults.attr.invisible = 0;
			break;
		case 29:
			buffer->pvt->screen->defaults.attr.strikethrough = 0;
			break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
			buffer->pvt->screen->defaults.attr.fore = param - 30;
			break;
		case 38:
		{
			/* The format looks like: ^[[38;5;COLORNUMBERm,
			   so look for COLORNUMBER here. */
			if ((i + 2) < params->n_values){
				GValue *value1, *value2;
				long param1, param2;
				value1 = g_value_array_get_nth(params, i + 1);
				value2 = g_value_array_get_nth(params, i + 2);
				if (G_UNLIKELY (!(G_VALUE_HOLDS_LONG(value1) && G_VALUE_HOLDS_LONG(value2))))
					break;
				param1 = g_value_get_long(value1);
				param2 = g_value_get_long(value2);
				if (G_LIKELY (param1 == 5 && param2 >= 0 && param2 < 256))
					buffer->pvt->screen->defaults.attr.fore = param2;
				i += 2;
			}
			break;
		}
		case 39:
			/* default foreground */
			buffer->pvt->screen->defaults.attr.fore = VTE_DEF_FG;
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			buffer->pvt->screen->defaults.attr.back = param - 40;
			break;
		case 48:
		{
			/* The format looks like: ^[[48;5;COLORNUMBERm,
			   so look for COLORNUMBER here. */
			if ((i + 2) < params->n_values){
				GValue *value1, *value2;
				long param1, param2;
				value1 = g_value_array_get_nth(params, i + 1);
				value2 = g_value_array_get_nth(params, i + 2);
				if (G_UNLIKELY (!(G_VALUE_HOLDS_LONG(value1) && G_VALUE_HOLDS_LONG(value2))))
					break;
				param1 = g_value_get_long(value1);
				param2 = g_value_get_long(value2);
				if (G_LIKELY (param1 == 5 && param2 >= 0 && param2 < 256))
					buffer->pvt->screen->defaults.attr.back = param2;
				i += 2;
			}
			break;
		}
		case 49:
			/* default background */
			buffer->pvt->screen->defaults.attr.back = VTE_DEF_BG;
			break;
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
			buffer->pvt->screen->defaults.attr.fore = param - 90 + VTE_COLOR_BRIGHT_OFFSET;
			break;
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
			buffer->pvt->screen->defaults.attr.back = param - 100 + VTE_COLOR_BRIGHT_OFFSET;
			break;
		}
	}
	/* If we had no parameters, default to the defaults. */
	if (i == 0) {
		_vte_screen_set_default_attributes(buffer->pvt->screen);
	}
	/* Save the new colors. */
	buffer->pvt->screen->color_defaults.attr.fore =
		buffer->pvt->screen->defaults.attr.fore;
	buffer->pvt->screen->color_defaults.attr.back =
		buffer->pvt->screen->defaults.attr.back;
	buffer->pvt->screen->fill_defaults.attr.fore =
		buffer->pvt->screen->defaults.attr.fore;
	buffer->pvt->screen->fill_defaults.attr.back =
		buffer->pvt->screen->defaults.attr.back;
}

/* Move the cursor to the given column, 1-based. */
static void
vte_sequence_handler_cursor_character_absolute (VteBuffer *buffer, GValueArray *params)
{
	VteScreen *screen;
	GValue *value;
	long val;

	screen = buffer->pvt->screen;

        val = 0;
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = CLAMP(g_value_get_long(value),
				    1, buffer->pvt->column_count) - 1;
		}
	}

        screen->cursor_current.col = val;
	_vte_buffer_cleanup_tab_fragments_at_cursor (buffer);
}

/* Move the cursor to the given position, 1-based. */
static void
vte_sequence_handler_cursor_position (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_offset(buffer, params, -1, vte_sequence_handler_cm);
}

/* Request terminal attributes. */
static void
vte_sequence_handler_request_terminal_parameters (VteBuffer *buffer, GValueArray *params)
{
	vte_buffer_feed_child(buffer, "\e[?x", -1);
}

/* Request terminal attributes. */
static void
vte_sequence_handler_return_terminal_status (VteBuffer *buffer, GValueArray *params)
{
	vte_buffer_feed_child(buffer, "", 0);
}

/* Send primary device attributes. */
static void
vte_sequence_handler_send_primary_device_attributes (VteBuffer *buffer, GValueArray *params)
{
	/* Claim to be a VT220 with only national character set support. */
	vte_buffer_feed_child(buffer, "\e[?62;9;c", -1);
}

/* Send terminal ID. */
static void
vte_sequence_handler_return_terminal_id (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_send_primary_device_attributes (buffer, params);
}

/* Send secondary device attributes. */
static void
vte_sequence_handler_send_secondary_device_attributes (VteBuffer *buffer, GValueArray *params)
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
	vte_buffer_feed_child(buffer, buf, -1);
}

/* Set one or the other. */
static void
vte_sequence_handler_set_icon_title (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_set_title_internal(buffer, params, TRUE, FALSE);
}

static void
vte_sequence_handler_set_window_title (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_set_title_internal(buffer, params, FALSE, TRUE);
}

/* Set both the window and icon titles to the same string. */
static void
vte_sequence_handler_set_icon_and_window_title (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_set_title_internal(buffer, params, TRUE, TRUE);
}

/* Restrict the scrolling region. */
static void
vte_sequence_handler_set_scrolling_region (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_offset(buffer, params, -1, vte_sequence_handler_cs);
}

static void
vte_sequence_handler_set_scrolling_region_from_start (VteBuffer *buffer, GValueArray *params)
{
	GValue value = {0};

	g_value_init (&value, G_TYPE_LONG);
	g_value_set_long (&value, 0); /* Out of range means start/end */

	g_value_array_insert (params, 0, &value);

	vte_sequence_handler_offset(buffer, params, -1, vte_sequence_handler_cs);
}

static void
vte_sequence_handler_set_scrolling_region_to_end (VteBuffer *buffer, GValueArray *params)
{
	GValue value = {0};

	g_value_init (&value, G_TYPE_LONG);
	g_value_set_long (&value, 0); /* Out of range means start/end */

	g_value_array_insert (params, 1, &value);

	vte_sequence_handler_offset(buffer, params, -1, vte_sequence_handler_cs);
}

/* Set the application or normal keypad. */
static void
vte_sequence_handler_application_keypad (VteBuffer *buffer, GValueArray *params)
{
	_vte_debug_print(VTE_DEBUG_KEYBOARD,
			"Entering application keypad mode.\n");
	buffer->pvt->keypad_mode = VTE_KEYMODE_APPLICATION;
}

static void
vte_sequence_handler_normal_keypad (VteBuffer *buffer, GValueArray *params)
{
	_vte_debug_print(VTE_DEBUG_KEYBOARD,
			"Leaving application keypad mode.\n");
	buffer->pvt->keypad_mode = VTE_KEYMODE_NORMAL;
}

/* Move the cursor. */
static void
vte_sequence_handler_character_position_absolute (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_offset(buffer, params, -1, vte_sequence_handler_ch);
}
static void
vte_sequence_handler_line_position_absolute (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_offset(buffer, params, -1, vte_sequence_handler_cv);
}

/* Set certain terminal attributes. */
static void
vte_sequence_handler_set_mode (VteBuffer *buffer, GValueArray *params)
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
		vte_sequence_handler_set_mode_internal(buffer, setting, TRUE);
	}
}

/* Unset certain terminal attributes. */
static void
vte_sequence_handler_reset_mode (VteBuffer *buffer, GValueArray *params)
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
		vte_sequence_handler_set_mode_internal(buffer, setting, FALSE);
	}
}

/* Set certain terminal attributes. */
static void
vte_sequence_handler_decset (VteBuffer *buffer, GValueArray *params)
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
		vte_sequence_handler_decset_internal(buffer, setting, FALSE, FALSE, TRUE);
	}
}

/* Unset certain terminal attributes. */
static void
vte_sequence_handler_decreset (VteBuffer *buffer, GValueArray *params)
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
		vte_sequence_handler_decset_internal(buffer, setting, FALSE, FALSE, FALSE);
	}
}

/* Erase a specified number of characters. */
static void
vte_sequence_handler_erase_characters (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_ec (buffer, params);
}

/* Erase certain lines in the display. */
static void
vte_sequence_handler_erase_in_display (VteBuffer *buffer, GValueArray *params)
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
		vte_sequence_handler_cd (buffer, NULL);
		break;
	case 1:
		/* Clear above the current line. */
		_vte_buffer_clear_above_current (buffer);
		/* Clear everything to the left of the cursor, too. */
		/* FIXME: vttest. */
		vte_sequence_handler_cb (buffer, NULL);
		break;
	case 2:
		/* Clear the entire screen. */
		_vte_buffer_clear_screen (buffer);
		break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Erase certain parts of the current line in the display. */
static void
vte_sequence_handler_erase_in_line (VteBuffer *buffer, GValueArray *params)
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
		vte_sequence_handler_ce (buffer, NULL);
		break;
	case 1:
		/* Clear to start of the line. */
		vte_sequence_handler_cb (buffer, NULL);
		break;
	case 2:
		/* Clear the entire line. */
		_vte_buffer_clear_current_line (buffer);
		break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Perform a full-bore reset. */
static void
vte_sequence_handler_full_reset (VteBuffer *buffer, GValueArray *params)
{
	vte_buffer_reset(buffer, TRUE, TRUE);
}

/* Insert a specified number of blank characters. */
static void
vte_sequence_handler_insert_blank_characters (VteBuffer *buffer, GValueArray *params)
{
	vte_sequence_handler_IC (buffer, params);
}

/* Insert a certain number of lines below the current cursor. */
static void
vte_sequence_handler_insert_lines (VteBuffer *buffer, GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param, end, row, i, limit;
	screen = buffer->pvt->screen;

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
		end = screen->insert_delta + buffer->pvt->row_count - 1;
	}

	/* Only allow to insert as many lines as there are between this row
         * and the end of the scrolling region. See bug #676090.
         */
        limit = end - row + 1;
        param = MIN (param, limit);

	for (i = 0; i < param; i++) {
		/* Clear a line off the end of the region and add one to the
		 * top of the region. */
		_vte_buffer_ring_remove (buffer, end);
		_vte_buffer_ring_insert (buffer, row, TRUE);
	}
	/* Update the display. */
        _vte_buffer_view_scroll_region(buffer, row, end - row + 1, param);
	/* Adjust the scrollbars if necessary. */
        _vte_buffer_view_adjust_adjustments(buffer);
	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_inserted_flag = TRUE;
}

/* Delete certain lines from the scrolling region. */
static void
vte_sequence_handler_delete_lines (VteBuffer *buffer, GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param, end, row, i, limit;

	screen = buffer->pvt->screen;
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
		end = screen->insert_delta + buffer->pvt->row_count - 1;
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
		_vte_buffer_ring_remove (buffer, row);
		_vte_buffer_ring_insert (buffer, end, TRUE);
	}
	/* Update the display. */
        _vte_buffer_view_scroll_region(buffer, row, end - row + 1, -param);
	/* Adjust the scrollbars if necessary. */
        _vte_buffer_view_adjust_adjustments(buffer);
	/* We've modified the display.  Make a note of it. */
	buffer->pvt->text_deleted_flag = TRUE;
}

/* Set the terminal encoding. */
static void
vte_sequence_handler_local_charset (VteBuffer *buffer, GValueArray *params)
{
	const char *locale_encoding;
	g_get_charset(&locale_encoding);
	vte_buffer_set_encoding(buffer, locale_encoding);
}

static void
vte_sequence_handler_utf_8_charset (VteBuffer *buffer, GValueArray *params)
{
	vte_buffer_set_encoding(buffer, "UTF-8");
}

/* Device status reports. The possible reports are the cursor position and
 * whether or not we're okay. */
static void
vte_sequence_handler_device_status_report (VteBuffer *buffer, GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param;
	char buf[128];

	screen = buffer->pvt->screen;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			param = g_value_get_long(value);
			switch (param) {
			case 5:
				/* Send a thumbs-up sequence. */
				vte_buffer_feed_child(buffer, _VTE_CAP_CSI "0n", -1);
				break;
			case 6:
				/* Send the cursor position. */
				g_snprintf(buf, sizeof(buf),
					   _VTE_CAP_CSI "%ld;%ldR",
					   screen->cursor_current.row + 1 -
					   screen->insert_delta,
					   screen->cursor_current.col + 1);
				vte_buffer_feed_child(buffer, buf, -1);
				break;
			default:
				break;
			}
		}
	}
}

/* DEC-style device status reports. */
static void
vte_sequence_handler_dec_device_status_report (VteBuffer *buffer, GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param;
	char buf[128];

	screen = buffer->pvt->screen;

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
				vte_buffer_feed_child(buffer, buf, -1);
				break;
			case 15:
				/* Send printer status -- 10 = ready,
				 * 11 = not ready.  We don't print. */
				vte_buffer_feed_child(buffer, _VTE_CAP_CSI "?11n", -1);
				break;
			case 25:
				/* Send UDK status -- 20 = locked,
				 * 21 = not locked.  I don't even know what
				 * that means, but punt anyway. */
				vte_buffer_feed_child(buffer, _VTE_CAP_CSI "?20n", -1);
				break;
			case 26:
				/* Send keyboard status.  50 = no locator. */
				vte_buffer_feed_child(buffer, _VTE_CAP_CSI "?50n", -1);
				break;
			default:
				break;
			}
		}
	}
}

/* Restore a certain terminal attribute. */
static void
vte_sequence_handler_restore_mode (VteBuffer *buffer, GValueArray *params)
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
		vte_sequence_handler_decset_internal(buffer, setting, TRUE, FALSE, FALSE);
	}
}

/* Save a certain terminal attribute. */
static void
vte_sequence_handler_save_mode (VteBuffer *buffer, GValueArray *params)
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
		vte_sequence_handler_decset_internal(buffer, setting, FALSE, TRUE, FALSE);
	}
}

/* Perform a screen alignment test -- fill all visible cells with the
 * letter "E". */
static void
vte_sequence_handler_screen_alignment_test (VteBuffer *buffer, GValueArray *params)
{
	long row;
	VteRowData *rowdata;
	VteScreen *screen;
	VteCell cell;

	screen = buffer->pvt->screen;

	for (row = buffer->pvt->screen->insert_delta;
	     row < buffer->pvt->screen->insert_delta + buffer->pvt->row_count;
	     row++) {
		/* Find this row. */
		while (_vte_ring_next(screen->row_data) <= row)
			_vte_buffer_ring_append (buffer, FALSE);
                _vte_buffer_view_adjust_adjustments(buffer);
		rowdata = _vte_ring_index_writable (screen->row_data, row);
		g_assert(rowdata != NULL);
		/* Clear this row. */
		_vte_row_data_shrink (rowdata, 0);

		_vte_buffer_emit_text_deleted(buffer);
		/* Fill this row. */
		cell.c = 'E';
		cell.attr = basic_cell.cell.attr;
		cell.attr.columns = 1;
		_vte_row_data_fill (rowdata, &cell, buffer->pvt->column_count);
		_vte_buffer_emit_text_inserted(buffer);
	}
	_vte_buffer_view_invalidate_all(buffer);

	/* We modified the display, so make a note of it for completeness. */
	buffer->pvt->text_modified_flag = TRUE;
}

/* Perform a soft reset. */
static void
vte_sequence_handler_soft_reset (VteBuffer *buffer, GValueArray *params)
{
	vte_buffer_reset(buffer, FALSE, FALSE);
}

/* Window manipulation control sequences.  Most of these are considered
 * bad ideas, but they're implemented as signals which the application
 * is free to ignore, so they're harmless. */
static void
vte_sequence_handler_window_manipulation (VteBuffer *buffer, GValueArray *params)
{
        VteView *terminal;
	GdkScreen *gscreen;
	GValue *value;
	GtkWidget *widget;
	char buf[128];
	long param, arg1, arg2;
	gint width, height;
	guint i;

        terminal = buffer->pvt->terminal;
        /* FIXMEchpe cope with NULL terminal */

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
			_vte_buffer_emit_deiconify_window(buffer);
			break;
		case 2:
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Iconifying window.\n");
			_vte_buffer_emit_iconify_window(buffer);
			break;
		case 3:
			if ((arg1 != -1) && (arg2 != -2)) {
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Moving window to "
						"%ld,%ld.\n", arg1, arg2);
				_vte_buffer_emit_move_window(buffer,
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
				_vte_buffer_emit_resize_window(buffer,
								arg2 / CHAR_WIDTH,
								arg1 / CHAR_HEIGHT);
				i += 2;
			}
			break;
		case 5:
			_vte_debug_print(VTE_DEBUG_PARSE, "Raising window.\n");
			_vte_buffer_emit_raise_window(buffer);
			break;
		case 6:
			_vte_debug_print(VTE_DEBUG_PARSE, "Lowering window.\n");
			_vte_buffer_emit_lower_window(buffer);
			break;
		case 7:
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Refreshing window.\n");
                        _vte_buffer_view_invalidate_all(buffer);
			_vte_buffer_emit_refresh_window(buffer);
			break;
		case 8:
			if ((arg1 != -1) && (arg2 != -1)) {
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Resizing window "
						"(to %ld columns, %ld rows).\n",
						arg2, arg1);
				_vte_buffer_emit_resize_window(buffer,
								arg2,
								arg1);
				i += 2;
			}
			break;
		case 9:
			switch (arg1) {
			case 0:
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Restoring window.\n");
				_vte_buffer_emit_restore_window(buffer);
				break;
			case 1:
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Maximizing window.\n");
				_vte_buffer_emit_maximize_window(buffer);
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
			vte_buffer_feed_child(buffer, buf, -1);
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
			vte_buffer_feed_child(buffer, buf, -1);
			break;
		case 14:
			/* Send window size, in pixels. */
			g_snprintf(buf, sizeof(buf),
				   _VTE_CAP_CSI "4;%ld;%ldt",
				   buffer->pvt->row_count * CHAR_HEIGHT,
                                   buffer->pvt->column_count * CHAR_WIDTH);
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting window size "
					"(%ldx%ldn",
                                         buffer->pvt->row_count * CHAR_HEIGHT,
                                         buffer->pvt->column_count * CHAR_WIDTH);
                        vte_buffer_feed_child(buffer, buf, -1);
			break;
		case 18:
			/* Send widget size, in cells. */
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting widget size.\n");
			g_snprintf(buf, sizeof(buf),
				   _VTE_CAP_CSI "8;%ld;%ldt",
				   buffer->pvt->row_count,
				   buffer->pvt->column_count);
			vte_buffer_feed_child(buffer, buf, -1);
			break;
		case 19:
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting screen size.\n");
			gscreen = gtk_widget_get_screen(widget);
			height = gdk_screen_get_height(gscreen);
			width = gdk_screen_get_width(gscreen);
			g_snprintf(buf, sizeof(buf),
				   _VTE_CAP_CSI "9;%ld;%ldt",
				   (glong)height / CHAR_HEIGHT,
				   (glong)width / CHAR_WIDTH);
			vte_buffer_feed_child(buffer, buf, -1);
			break;
		case 20:
			/* Report a static icon title, since the real
			   icon title should NEVER be reported, as it
			   creates a security vulnerability.  See
			   http://marc.info/?l=bugtraq&m=104612710031920&w=2
			   and CVE-2003-0070. */
			_vte_debug_print(VTE_DEBUG_PARSE,
				"Reporting fake icon title.\n");
			/* never use buffer->pvt->icon_title here! */
			g_snprintf (buf, sizeof (buf),
				    _VTE_CAP_OSC "LTerminal" _VTE_CAP_ST);
			vte_buffer_feed_child(buffer, buf, -1);
			break;
		case 21:
			/* Report a static window title, since the real
			   window title should NEVER be reported, as it
			   creates a security vulnerability.  See
			   http://marc.info/?l=bugtraq&m=104612710031920&w=2
			   and CVE-2003-0070. */
			_vte_debug_print(VTE_DEBUG_PARSE,
					"Reporting fake window title.\n");
			/* never use buffer->pvt->window_title here! */
			g_snprintf (buf, sizeof (buf),
				    _VTE_CAP_OSC "lTerminal" _VTE_CAP_ST);
			vte_buffer_feed_child(buffer, buf, -1);
			break;
		default:
			if (param >= 24) {
				_vte_debug_print(VTE_DEBUG_PARSE,
						"Resizing to %ld rows.\n",
					       	param);
				/* Resize to the specified number of
				 * rows. */
				_vte_buffer_emit_resize_window(buffer,
								buffer->pvt->column_count,
								param);
			}
			break;
		}
	}
}

/* Change the color of the cursor */
static void
vte_sequence_handler_change_cursor_color (VteBuffer *buffer, GValueArray *params)
{
	gchar *name = NULL;
	GValue *value;
	GdkRGBA color;

	if (params != NULL && params->n_values > 0) {
		value = g_value_array_get_nth (params, 0);

		if (G_VALUE_HOLDS_STRING (value))
			name = g_value_dup_string (value);
		else if (G_VALUE_HOLDS_POINTER (value))
			name = vte_buffer_ucs4_to_utf8(buffer, g_value_get_pointer (value));

		if (! name)
			return;

		if (vte_parse_color (name, &color))
			_vte_view_set_effect_color(buffer->pvt->terminal, VTE_CUR_BG, &color,
                                                       VTE_EFFECT_COLOR, TRUE);
		else if (strcmp (name, "?") == 0) {
			gchar buf[128];
			g_snprintf (buf, sizeof (buf),
				    _VTE_CAP_OSC "12;rgb:%04x/%04x/%04x" BEL,
                                    (guint) (buffer->pvt->palette[VTE_CUR_BG].red * 65535.),
                                    (guint) (buffer->pvt->palette[VTE_CUR_BG].green * 65535.),
                                    (guint) (buffer->pvt->palette[VTE_CUR_BG].blue * 65535.));
			vte_buffer_feed_child (buffer, buf, -1);
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

static VteSequenceHandler
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
_vte_buffer_handle_sequence(VteBuffer *buffer,
                            const char *match_s,
                            GQuark match G_GNUC_UNUSED,
                            GValueArray *params)
{
	VteSequenceHandler handler;

	_VTE_DEBUG_IF(VTE_DEBUG_PARSE)
		display_control_sequence(match_s, params);

	/* Find the handler for this control sequence. */
	handler = _vte_sequence_get_handler (match_s);

	if (handler != NULL) {
		/* Let the handler handle it. */
		handler (buffer, params);
	} else {
		_vte_debug_print (VTE_DEBUG_MISC,
				  "No handler for control sequence `%s' defined.\n",
				  match_s);
	}
}
