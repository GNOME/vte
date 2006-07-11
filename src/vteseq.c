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


#include "../config.h"

#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

#include <glib.h>

#include "vte.h"
#include "vte-private.h"
#include "vteseq.h"
#include "vtetc.h"




/* Prototype all handlers... */
#define VTE_SEQUENCE_HANDLER(name) \
	static gboolean name(VteTerminal *terminal, \
			     const char *match, \
			     GQuark match_quark, \
			     GValueArray *params);
#include "vteseq-list.h"
#undef VTE_SEQUENCE_HANDLER

/* These two handlers are accessed from vte.c */
gboolean
_vte_sequence_handler_bl(VteTerminal *terminal, const char *match, GQuark match_quark, GValueArray *params)
{
	return vte_sequence_handler_bl(terminal, match, match_quark, params);
}
gboolean
_vte_sequence_handler_sf(VteTerminal *terminal, const char *match, GQuark match_quark, GValueArray *params)
{
	return vte_sequence_handler_sf(terminal, match, match_quark, params);
}





/* FUNCTIONS WE USE */



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
		if (rowdata->cells->len > col) {
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

	while (array->len < final_size) {
		g_array_append_vals(array, item, 1);
	}
}






/* Emit a "deiconify-window" signal. */
static void
vte_terminal_emit_deiconify_window(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `deiconify-window'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "deiconify-window");
}

/* Emit a "iconify-window" signal. */
static void
vte_terminal_emit_iconify_window(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `iconify-window'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "iconify-window");
}

/* Emit an "icon-title-changed" signal. */
static void
vte_terminal_emit_icon_title_changed(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `icon-title-changed'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "icon-title-changed");
}

/* Emit a "window-title-changed" signal. */
static void
vte_terminal_emit_window_title_changed(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `window-title-changed'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "window-title-changed");
}

/* Emit a "raise-window" signal. */
static void
vte_terminal_emit_raise_window(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `raise-window'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "raise-window");
}

/* Emit a "lower-window" signal. */
static void
vte_terminal_emit_lower_window(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `lower-window'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "lower-window");
}

/* Emit a "maximize-window" signal. */
static void
vte_terminal_emit_maximize_window(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `maximize-window'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "maximize-window");
}

/* Emit a "refresh-window" signal. */
static void
vte_terminal_emit_refresh_window(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `refresh-window'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "refresh-window");
}

/* Emit a "restore-window" signal. */
static void
vte_terminal_emit_restore_window(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `restore-window'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "restore-window");
}

/* Emit a "move-window" signal.  (Pixels.) */
static void
vte_terminal_emit_move_window(VteTerminal *terminal, guint x, guint y)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `move-window'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "move-window", x, y);
}

/* Emit a "resize-window" signal.  (Pixels.) */
static void
vte_terminal_emit_resize_window(VteTerminal *terminal,
				guint width, guint height)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `resize-window'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "resize-window", width, height);
}


/* Insert a blank line at an arbitrary position. */
static void
vte_insert_line_internal(VteTerminal *terminal, glong position)
{
	VteRowData *row;
	/* Pad out the line data to the insertion point. */
	while (_vte_ring_next(terminal->pvt->screen->row_data) < position) {
		row = _vte_new_row_data_sized(terminal, TRUE);
		_vte_ring_append(terminal->pvt->screen->row_data, row);
	}
	/* If we haven't inserted a line yet, insert a new one. */
	row = _vte_new_row_data_sized(terminal, TRUE);
	if (_vte_ring_next(terminal->pvt->screen->row_data) >= position) {
		_vte_ring_insert(terminal->pvt->screen->row_data,
				 position, row);
	} else {
		_vte_ring_append(terminal->pvt->screen->row_data, row);
	}
}

/* Remove a line at an arbitrary position. */
static void
vte_remove_line_internal(VteTerminal *terminal, glong position)
{
	if (_vte_ring_next(terminal->pvt->screen->row_data) > position) {
		_vte_ring_remove(terminal->pvt->screen->row_data,
				 position, TRUE);
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
/* Call another function, offsetting any long arguments by the given
 * increment value. */
static gboolean
vte_sequence_handler_offset(VteTerminal *terminal,
			    const char *match,
			    GQuark match_quark,
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
	return handler(terminal, match, match_quark, params);
}

/* Call another function a given number of times, or once. */
static gboolean
vte_sequence_handler_multiple(VteTerminal *terminal,
			      const char *match,
			      GQuark match_quark,
			      GValueArray *params,
			      VteTerminalSequenceHandler handler)
{
	long val = 1;
	int i, again;
	GValue *value;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			val = g_value_get_long(value);
			val = MAX(val, 1);	/* FIXME: vttest. */
		}
	}
	again = 0;
	for (i = 0; i < val; i++) {
		if (handler(terminal, match, match_quark, NULL)) {
			again++;
		}
	}
	return (again > 0);
}

/* Scroll the text, but don't move the cursor.  Negative = up,
 * positive = down. */
static gboolean
vte_sequence_handler_scroll_up_or_down(VteTerminal *terminal, int scroll_amount)
{
	GtkWidget *widget;
	VteRowData *row;
	long start, end, i;
	VteScreen *screen;

	widget = GTK_WIDGET(terminal);
	screen = terminal->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = start + terminal->row_count - 1;
	}

	while (_vte_ring_next(screen->row_data) <= end) {
		row = _vte_new_row_data_sized(terminal, FALSE);
		_vte_ring_append(terminal->pvt->screen->row_data, row);
	}
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
	_vte_terminal_adjust_adjustments(terminal, FALSE);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_inserted_count++;
	terminal->pvt->text_deleted_count++;

	return FALSE;
}

/* Set icon/window titles. */
static gboolean
vte_sequence_handler_set_title_internal(VteTerminal *terminal,
					const char *match,
					GQuark match_quark,
					GValueArray *params,
					const char *signal)
{
	GValue *value;
	VteConv conv;
	char *inbuf = NULL, *outbuf = NULL, *outbufptr = NULL, *title = NULL;
	gsize inbuf_len, outbuf_len;
	gboolean ret = FALSE;

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
			/* Convert the unicode-character string into a
			 * multibyte string. */
			conv = _vte_conv_open("UTF-8", VTE_CONV_GUNICHAR_TYPE);
			inbuf = g_value_get_pointer(value);
			inbuf_len = vte_unichar_strlen((gunichar*)inbuf) *
				    sizeof(gunichar);
			outbuf_len = (inbuf_len * VTE_UTF8_BPC) + 1;
			_vte_buffer_set_minimum_size(terminal->pvt->conv_buffer,
						     outbuf_len);
			outbuf = outbufptr = terminal->pvt->conv_buffer->bytes;
			if (conv != ((VteConv) -1)) {
				if (_vte_conv(conv, &inbuf, &inbuf_len,
					      &outbuf, &outbuf_len) == -1) {
#ifdef VTE_DEBUG
					if (_vte_debug_on(VTE_DEBUG_IO)) {
						fprintf(stderr, "Error "
							"converting %ld title "
							"bytes (%s), "
							"skipping.\n",
							(long) _vte_buffer_length(terminal->pvt->outgoing),
							strerror(errno));
					}
#endif
					outbufptr = NULL;
				} else {
					title = g_strndup(outbufptr,
							  outbuf - outbufptr);
				}
				_vte_conv_close(conv);
			}
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
			if (strcmp(signal, "window") == 0) {
				g_free(terminal->window_title);
				terminal->window_title = g_strdup(validated);
				gdk_window_set_title (GTK_WIDGET (terminal)->window, validated);
				vte_terminal_emit_window_title_changed(terminal);
			} else
			if (strcmp(signal, "icon") == 0) {
				g_free (terminal->icon_title);
				terminal->icon_title = g_strdup(validated);
				gdk_window_set_icon_name (GTK_WIDGET (terminal)->window, validated);
				vte_terminal_emit_icon_title_changed(terminal);
			}
			g_free(validated);
			g_free(title);

			ret = TRUE;
		}
	}
	return ret;
}

/* Manipulate certain terminal attributes. */
static gboolean
vte_sequence_handler_decset_internal(VteTerminal *terminal,
				     int setting,
				     gboolean restore,
				     gboolean save,
				     gboolean set)
{
	gboolean recognized = FALSE, again = FALSE;
	gpointer p;
	int i;
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
		{9, &terminal->pvt->mouse_send_xy_on_click, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
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
		{47, NULL, NULL, (gpointer*) &terminal->pvt->screen,
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
		{1000, &terminal->pvt->mouse_send_xy_on_button, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 1001: Hilite tracking. */
		{1001, &terminal->pvt->mouse_hilite_tracking, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 1002: Cell motion tracking. */
		{1002, &terminal->pvt->mouse_cell_motion_tracking, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
		/* 1003: All motion tracking. */
		{1003, &terminal->pvt->mouse_all_motion_tracking, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
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
		{1047, NULL, NULL, (gpointer*) &terminal->pvt->screen,
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
		{1049, NULL, NULL, (gpointer*) &terminal->pvt->screen,
		 &terminal->pvt->normal_screen,
		 &terminal->pvt->alternate_screen,
		 vte_sequence_handler_rc,
		 vte_sequence_handler_sc,},
		/* 1051: Sun function key mode. */
		{1051, NULL, NULL, (gpointer*) &terminal->pvt->sun_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
		/* 1052: HP function key mode. */
		{1052, NULL, NULL, (gpointer*) &terminal->pvt->hp_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
		/* 1060: Legacy function key mode. */
		{1060, NULL, NULL, (gpointer*) &terminal->pvt->legacy_fkey_mode,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL},
		/* 1061: VT220 function key mode. */
		{1061, NULL, NULL, (gpointer*) &terminal->pvt->vt220_fkey_mode,
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
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Setting %d was %s.\n",
					setting, set ? "set" : "unset");
			}
#endif
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
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Setting %d is %s, saving.\n",
					setting, set ? "set" : "unset");
			}
#endif
			g_hash_table_insert(terminal->pvt->dec_saved,
					    GINT_TO_POINTER(setting),
					    GINT_TO_POINTER(set));
		}
		/* Change the current setting to match the new/saved value. */
		if (!save) {
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Setting %d to %s.\n",
					setting, set ? "set" : "unset");
			}
#endif
			if (settings[i].set && set) {
				settings[i].set(terminal, NULL, 0, NULL);
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
				settings[i].reset(terminal, NULL, 0, NULL);
			}
		}
	}

	/* Do whatever's necessary when the setting changes. */
	switch (setting) {
	case 1:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_KEYBOARD)) {
			if (set) {
				fprintf(stderr, "Entering application cursor mode.\n");
			} else {
				fprintf(stderr, "Leaving application cursor mode.\n");
			}
		}
#endif
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
		again = TRUE;
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
	case 25:
	case 1048:
		/* Repaint the cell the cursor is in. */
		_vte_invalidate_cursor_once(terminal, FALSE);
		break;
	case 47:
	case 1047:
	case 1049:
		/* Clear the alternate screen if we're switching
		 * to it, and home the cursor. */
		if (set) {
			vte_sequence_handler_clear_screen(terminal,
							  NULL,
							  0,
							  NULL);
			vte_sequence_handler_ho(terminal,
						NULL,
						0,
						NULL);
		}
		/* Reset scrollbars and repaint everything. */
		_vte_terminal_adjust_adjustments(terminal, TRUE);
		_vte_invalidate_all(terminal);
		/* Clear the matching view. */
		_vte_terminal_match_contents_clear(terminal);
		/* Notify viewers that the contents have changed. */
		_vte_terminal_emit_contents_changed(terminal);
		break;
	case 9:
	case 1000:
	case 1001:
	case 1002:
	case 1003:
		/* Reset all of the options except the one which was
		 * just toggled. */
		switch (setting) {
		case 9:
			terminal->pvt->mouse_send_xy_on_button = FALSE; /* 1000 */
			terminal->pvt->mouse_hilite_tracking = FALSE; /* 1001 */
			terminal->pvt->mouse_cell_motion_tracking = FALSE; /* 1002 */
			terminal->pvt->mouse_all_motion_tracking = FALSE; /* 1003 */
			break;
		case 1000:
			terminal->pvt->mouse_send_xy_on_click = FALSE; /* 9 */
			terminal->pvt->mouse_hilite_tracking = FALSE; /* 1001 */
			terminal->pvt->mouse_cell_motion_tracking = FALSE; /* 1002 */
			terminal->pvt->mouse_all_motion_tracking = FALSE; /* 1003 */
			break;
		case 1001:
			terminal->pvt->mouse_send_xy_on_click = FALSE; /* 9 */
			terminal->pvt->mouse_send_xy_on_button = FALSE; /* 1000 */
			terminal->pvt->mouse_cell_motion_tracking = FALSE; /* 1002 */
			terminal->pvt->mouse_all_motion_tracking = FALSE; /* 1003 */
			break;
		case 1002:
			terminal->pvt->mouse_send_xy_on_click = FALSE; /* 9 */
			terminal->pvt->mouse_send_xy_on_button = FALSE; /* 1000 */
			terminal->pvt->mouse_hilite_tracking = FALSE; /* 1001 */
			terminal->pvt->mouse_all_motion_tracking = FALSE; /* 1003 */
			break;
		case 1003:
			terminal->pvt->mouse_send_xy_on_click = FALSE; /* 9 */
			terminal->pvt->mouse_send_xy_on_button = FALSE; /* 1000 */
			terminal->pvt->mouse_hilite_tracking = FALSE; /* 1001 */
			terminal->pvt->mouse_cell_motion_tracking = FALSE; /* 1002 */
			break;
		}
		/* Make the pointer visible. */
		_vte_terminal_set_pointer_visible(terminal, TRUE);
		break;
	case 66:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_KEYBOARD)) {
			if (set) {
				fprintf(stderr, "Entering application keypad mode.\n");
			} else {
				fprintf(stderr, "Leaving application keypad mode.\n");
			}
		}
#endif
		break;
	case 1051:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_KEYBOARD)) {
			if (set) {
				fprintf(stderr, "Entering Sun fkey mode.\n");
			} else {
				fprintf(stderr, "Leaving Sun fkey mode.\n");
			}
		}
#endif
		break;
	case 1052:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_KEYBOARD)) {
			if (set) {
				fprintf(stderr, "Entering HP fkey mode.\n");
			} else {
				fprintf(stderr, "Leaving HP fkey mode.\n");
			}
		}
#endif
		break;
	case 1060:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_KEYBOARD)) {
			if (set) {
				fprintf(stderr, "Entering Legacy fkey mode.\n");
			} else {
				fprintf(stderr, "Leaving Legacy fkey mode.\n");
			}
		}
#endif
		break;
	case 1061:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_KEYBOARD)) {
			if (set) {
				fprintf(stderr, "Entering VT220 fkey mode.\n");
			} else {
				fprintf(stderr, "Leaving VT220 fkey mode.\n");
			}
		}
#endif
		break;
	default:
		break;
	}
#ifdef VTE_DEBUG
	if (!recognized) {
		g_warning("DECSET/DECRESET mode %d not recognized, ignoring.\n",
			  setting);
	}
#endif
	return again;
}

/* Toggle a terminal mode. */
static gboolean
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
	return FALSE;
}



















/* THE HANDLERS */









/* End alternate character set. */
static gboolean
vte_sequence_handler_ae(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.alternate = 0;
	return FALSE;
}

/* Add a line at the current cursor position. */
static gboolean
vte_sequence_handler_al(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
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
		param = g_value_get_long(value);
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
		_vte_terminal_adjust_adjustments(terminal, FALSE);
	}

	/* Update the display. */
	_vte_terminal_scroll_region(terminal, start, end - start + 1, param);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Add N lines at the current cursor position. */
static gboolean
vte_sequence_handler_AL(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_al(terminal, match, match_quark, params);
}

/* Start using alternate character set. */
static gboolean
vte_sequence_handler_as(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.alternate = 1;
	return FALSE;
}

static void
vte_terminal_beep(VteTerminal *terminal)
{
	GdkDisplay *display;

	g_assert(VTE_IS_TERMINAL(terminal));
	display = gtk_widget_get_display(GTK_WIDGET(terminal));
	gdk_display_beep(display);
}

/* Beep. */
static gboolean
vte_sequence_handler_bl(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	if (terminal->pvt->audible_bell) {
		/* Feep. */
		vte_terminal_beep(terminal);
	}
	if (terminal->pvt->visible_bell) {
		/* Visual bell. */
		vte_sequence_handler_vb(terminal, match, match_quark, params);
	}
	return FALSE;
}

/* Backtab. */
static gboolean
vte_sequence_handler_bt(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
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
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_PARSE)) {
		fprintf(stderr, "Moving cursor to column %ld.\n", (long)newcol);
	}
#endif
	terminal->pvt->screen->cursor_current.col = newcol;
	return FALSE;
}

/* Clear from the cursor position to the beginning of the line. */
static gboolean
vte_sequence_handler_cb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteRowData *rowdata;
	long i;
	VteScreen *screen;
	struct vte_charcell *pcell;
	screen = terminal->pvt->screen;

	/* Get the data for the row which the cursor points to. */
	_vte_terminal_ensure_cursor(terminal, FALSE);
	rowdata = _vte_ring_index(screen->row_data,
				  VteRowData *,
				  screen->cursor_current.row);
	/* Clear the data up to the current column with the default
	 * attributes.  If there is no such character cell, we need
	 * to add one. */
	for (i = 0; i <= screen->cursor_current.col; i++) {
		if (i < rowdata->cells->len) {
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
			      0, terminal->column_count,
			      screen->cursor_current.row, 1);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Clear to the right of the cursor and below the current line. */
static gboolean
vte_sequence_handler_cd(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteRowData *rowdata;
	long i;
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
		    (rowdata->cells->len > screen->cursor_current.col)) {
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
			rowdata = _vte_new_row_data(terminal);
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
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Clear from the cursor position to the end of the line. */
static gboolean
vte_sequence_handler_ce(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteRowData *rowdata;
	VteScreen *screen;

	screen = terminal->pvt->screen;
	/* Get the data for the row which the cursor points to. */
	_vte_terminal_ensure_cursor(terminal, FALSE);
	rowdata = _vte_ring_index(screen->row_data, VteRowData *,
				  screen->cursor_current.row);
	g_assert(rowdata != NULL);
	/* Remove the data at the end of the array until the current column
	 * is the end of the array. */
	if (rowdata->cells->len > screen->cursor_current.col) {
		g_array_set_size(rowdata->cells, screen->cursor_current.col);
	}
	/* Add enough cells to the end of the line to fill out the row. */
	vte_g_array_fill(rowdata->cells,
			 &screen->fill_defaults,
			 terminal->column_count);
	rowdata->soft_wrapped = 0;
	/* Repaint this row. */
	_vte_invalidate_cells(terminal,
			      0, terminal->column_count,
			      screen->cursor_current.row, 1);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Move the cursor to the given column (horizontal position). */
static gboolean
vte_sequence_handler_ch(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
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
		}
	}
	return FALSE;
}

/* Clear the screen and home the cursor. */
static gboolean
vte_sequence_handler_cl(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_clear_screen(terminal, NULL, 0, NULL);
	vte_sequence_handler_ho(terminal, NULL, 0, NULL);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Move the cursor to the given position. */
static gboolean
vte_sequence_handler_cm(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GValue *row, *col;
	long rowval, colval, origin;
	VteScreen *screen;

	screen = terminal->pvt->screen;
	/* We need at least two parameters. */
	if ((params != NULL) && (params->n_values >= 2)) {
		/* The first is the row, the second is the column. */
		row = g_value_array_get_nth(params, 0);
		col = g_value_array_get_nth(params, 1);
		if (G_VALUE_HOLDS_LONG(row) &&
		    G_VALUE_HOLDS_LONG(col)) {
			if (screen->origin_mode &&
			    screen->scrolling_restricted) {
				origin = screen->scrolling_region.start;
			} else {
				origin = 0;
			}
			rowval = g_value_get_long(row) + origin;
			colval = g_value_get_long(col);
			rowval = CLAMP(rowval, 0, terminal->row_count - 1);
			colval = CLAMP(colval, 0, terminal->column_count - 1);
			screen->cursor_current.row = rowval +
						     screen->insert_delta;
			screen->cursor_current.col = colval;
		}
	}
	return FALSE;
}

/* Clear the current line. */
static gboolean
vte_sequence_handler_clear_current_line(VteTerminal *terminal,
					const char *match,
					GQuark match_quark,
					GValueArray *params)
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
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Carriage return. */
static gboolean
vte_sequence_handler_cr(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	return FALSE;
}

/* Restrict scrolling and updates to a subset of the visible lines. */
static gboolean
vte_sequence_handler_cs(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	long start, end, rows;
	GValue *value;
	VteScreen *screen;

	/* We require two parameters.  Anything less is a reset. */
	screen = terminal->pvt->screen;
	if ((params == NULL) || (params->n_values < 2)) {
		screen->scrolling_restricted = FALSE;
		return FALSE;
	}
	/* Extract the two values. */
	value = g_value_array_get_nth(params, 0);
	start = g_value_get_long(value);
	value = g_value_array_get_nth(params, 1);
	end = g_value_get_long(value);
	/* Catch garbage. */
	rows = terminal->row_count;
	if ((start <= 0) || (start >= rows)) {
		start = 0;
	}
	if ((end <= 0) || (end >= rows)) {
		end = rows - 1;
	}
	/* Set the right values. */
	screen->scrolling_region.start = start;
	screen->scrolling_region.end = end;
	screen->scrolling_restricted = TRUE;
	/* Special case -- run wild, run free. */
	if ((screen->scrolling_region.start == 0) &&
	    (screen->scrolling_region.end == rows - 1)) {
		screen->scrolling_restricted = FALSE;
	}
	/* Clamp the cursor to the scrolling region. */
	screen->cursor_current.row = CLAMP(screen->cursor_current.row,
					   screen->insert_delta + start,
					   screen->insert_delta + end);
	_vte_terminal_ensure_cursor(terminal, TRUE);
	return FALSE;
}

/* Restrict scrolling and updates to a subset of the visible lines, because
 * GNU Emacs is special. */
static gboolean
vte_sequence_handler_cS(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	long start, end, rows;
	GValue *value;
	VteScreen *screen;

	/* We require four parameters. */
	screen = terminal->pvt->screen;
	if ((params == NULL) || (params->n_values < 2)) {
		screen->scrolling_restricted = FALSE;
		return FALSE;
	}
	/* Extract the two parameters we care about, encoded as the number
	 * of lines above and below the scrolling region, respectively. */
	value = g_value_array_get_nth(params, 1);
	start = g_value_get_long(value);
	value = g_value_array_get_nth(params, 2);
	end = (terminal->row_count - 1) - g_value_get_long(value);
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
	_vte_terminal_ensure_cursor(terminal, TRUE);
	return FALSE;
}

/* Clear all tab stops. */
static gboolean
vte_sequence_handler_ct(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	if (terminal->pvt->tabstops != NULL) {
		g_hash_table_destroy(terminal->pvt->tabstops);
		terminal->pvt->tabstops = NULL;
	}
	return FALSE;
}

/* Move the cursor to the lower left-hand corner. */
static gboolean
vte_sequence_handler_cursor_lower_left(VteTerminal *terminal,
				       const char *match,
				       GQuark match_quark,
				       GValueArray *params)
{
	VteScreen *screen;
	long row;
	screen = terminal->pvt->screen;
	row = MAX(0, terminal->row_count - 1);
	screen->cursor_current.row = screen->insert_delta + row;
	screen->cursor_current.col = 0;
	_vte_terminal_ensure_cursor(terminal, TRUE);
	return FALSE;
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static gboolean
vte_sequence_handler_cursor_next_line(VteTerminal *terminal,
				      const char *match,
				      GQuark match_quark,
				      GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	return vte_sequence_handler_DO(terminal, match, match_quark, params);
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static gboolean
vte_sequence_handler_cursor_preceding_line(VteTerminal *terminal,
					   const char *match,
					   GQuark match_quark,
					   GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	return vte_sequence_handler_UP(terminal, match, match_quark, params);
}

/* Move the cursor to the given row (vertical position). */
static gboolean
vte_sequence_handler_cv(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
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
	return FALSE;
}

/* Delete a character at the current cursor position. */
static gboolean
vte_sequence_handler_dc(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	VteRowData *rowdata;
	long col;

	screen = terminal->pvt->screen;

	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = _vte_ring_index(screen->row_data,
					  VteRowData *,
					  screen->cursor_current.row);
		g_assert(rowdata != NULL);
		col = screen->cursor_current.col;
		/* Remove the column. */
		if (col < rowdata->cells->len) {
			g_array_remove_index(rowdata->cells, col);
		}
		/* Add new cells until we have enough to fill the row. */
		vte_g_array_fill(rowdata->cells,
				 &screen->color_defaults,
				 terminal->column_count);
		/* Repaint this row. */
		_vte_invalidate_cells(terminal,
				      0, terminal->column_count,
				      screen->cursor_current.row, 1);
	}

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Delete N characters at the current cursor position. */
static gboolean
vte_sequence_handler_DC(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_multiple(terminal, match, match_quark,
					     params, vte_sequence_handler_dc);
}

/* Delete a line at the current cursor position. */
static gboolean
vte_sequence_handler_dl(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
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
		param = g_value_get_long(value);
	}

	/* Delete the right number of lines. */
	for (i = 0; i < param; i++) {
		/* Clear a line off the end of the region and add one to the
		 * top of the region. */
		vte_remove_line_internal(terminal, start);
		vte_insert_line_internal(terminal, end);
		/* Adjust the scrollbars if necessary. */
		_vte_terminal_adjust_adjustments(terminal, FALSE);
	}

	/* Update the display. */
	_vte_terminal_scroll_region(terminal, start, end - start + 1, -param);

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Delete N lines at the current cursor position. */
static gboolean
vte_sequence_handler_DL(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_dl(terminal, match, match_quark, params);
}

/* Cursor down, no scrolling. */
static gboolean
vte_sequence_handler_do(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GtkWidget *widget;
	long start, end;
	VteScreen *screen;

	widget = GTK_WIDGET(terminal);
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
	return FALSE;
}

/* Cursor down, no scrolling. */
static gboolean
vte_sequence_handler_DO(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_multiple(terminal, match, match_quark,
					     params, vte_sequence_handler_do);
}

/* Start using alternate character set. */
static gboolean
vte_sequence_handler_eA(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_ae(terminal, match, match_quark, params);
}

/* Erase characters starting at the cursor position (overwriting N with
 * spaces, but not moving the cursor). */
static gboolean
vte_sequence_handler_ec(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
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
	_vte_terminal_ensure_cursor(terminal, TRUE);
	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = _vte_ring_index(screen->row_data,
					  VteRowData *,
					  screen->cursor_current.row);
		g_assert(rowdata != NULL);
		/* Write over the characters.  (If there aren't enough, we'll
		 * need to create them.) */
		for (i = 0; i < count; i++) {
			col = screen->cursor_current.col + i;
			if (col >= 0) {
				if (col < rowdata->cells->len) {
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
				      0, terminal->column_count,
				      screen->cursor_current.row, 1);
	}

	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* End insert mode. */
static gboolean
vte_sequence_handler_ei(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->insert_mode = FALSE;
	return FALSE;
}

/* Form-feed / next-page. */
static gboolean
vte_sequence_handler_form_feed(VteTerminal *terminal,
			       const char *match,
			       GQuark match_quark,
			       GValueArray *params)
{
	return vte_sequence_handler_index(terminal, match, match_quark, params);
}

/* Move from status line. */
static gboolean
vte_sequence_handler_fs(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->status_line = FALSE;
	return FALSE;
}

/* Move the cursor to the home position. */
static gboolean
vte_sequence_handler_ho(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_current.row = screen->insert_delta;
	screen->cursor_current.col = 0;
	return FALSE;
}

/* Move the cursor to a specified position. */
static gboolean
vte_sequence_handler_horizontal_and_vertical_position(VteTerminal *terminal,
						      const char *match,
						      GQuark match_quark,
						      GValueArray *params)
{
	return vte_sequence_handler_offset(terminal, match, match_quark, params,
					   -1, vte_sequence_handler_cm);
}

/* Insert a character. */
static gboolean
vte_sequence_handler_ic(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct vte_cursor_position save;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	save = screen->cursor_current;

	_vte_terminal_insert_char(terminal, ' ', TRUE, TRUE, TRUE, TRUE, 0);

	screen->cursor_current = save;

	return FALSE;
}

/* Insert N characters. */
static gboolean
vte_sequence_handler_IC(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_multiple(terminal, match, match_quark,
					     params, vte_sequence_handler_ic);
}

/* Begin insert mode. */
static gboolean
vte_sequence_handler_im(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->insert_mode = TRUE;
	return FALSE;
}

/* Cursor down, with scrolling. */
static gboolean
vte_sequence_handler_index(VteTerminal *terminal,
			   const char *match,
			   GQuark match_quark,
			   GValueArray *params)
{
	return vte_sequence_handler_sf(terminal, match, match_quark, params);
}

/* Send me a backspace key sym, will you?  Guess that the application meant
 * to send the cursor back one position. */
static gboolean
vte_sequence_handler_kb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	/* Move the cursor left. */
	return vte_sequence_handler_le(terminal, match, match_quark, params);
}

/* Keypad mode end. */
static gboolean
vte_sequence_handler_ke(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->keypad_mode = VTE_KEYMODE_NORMAL;
	return FALSE;
}

/* Keypad mode start. */
static gboolean
vte_sequence_handler_ks(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->keypad_mode = VTE_KEYMODE_APPLICATION;
	return FALSE;
}

/* Cursor left. */
static gboolean
vte_sequence_handler_le(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;

	screen = terminal->pvt->screen;
	if (screen->cursor_current.col > 0) {
		/* There's room to move left, so do so. */
		screen->cursor_current.col--;
	} else {
		if (terminal->pvt->flags.bw) {
			/* Wrap to the previous line. */
			screen->cursor_current.col = terminal->column_count - 1;
			if (screen->scrolling_restricted) {
				vte_sequence_handler_sr(terminal, match, match_quark, params);
			} else {
				screen->cursor_current.row = MAX(screen->cursor_current.row - 1,
								 screen->insert_delta);
			}
		} else {
			/* Stick to the first column. */
			screen->cursor_current.col = 0;
		}
	}
	return FALSE;
}

/* Move the cursor left N columns. */
static gboolean
vte_sequence_handler_LE(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_multiple(terminal, match, match_quark,
					     params, vte_sequence_handler_le);
}

/* Move the cursor to the lower left corner of the display. */
static gboolean
vte_sequence_handler_ll(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_current.row = MAX(screen->insert_delta,
					 screen->insert_delta +
					 terminal->row_count - 1);
	screen->cursor_current.col = 0;
	return FALSE;
}

/* Blink on. */
static gboolean
vte_sequence_handler_mb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.blink = 1;
	return FALSE;
}

/* Bold on. */
static gboolean
vte_sequence_handler_md(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.bold = 1;
	terminal->pvt->screen->defaults.half = 0;
	return FALSE;
}

/* End modes. */
static gboolean
vte_sequence_handler_me(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	_vte_terminal_set_default_attributes(terminal);
	return FALSE;
}

/* Half-bright on. */
static gboolean
vte_sequence_handler_mh(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.half = 1;
	terminal->pvt->screen->defaults.bold = 0;
	return FALSE;
}

/* Invisible on. */
static gboolean
vte_sequence_handler_mk(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.invisible = 1;
	return FALSE;
}

/* Protect on. */
static gboolean
vte_sequence_handler_mp(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.protect = 1;
	return FALSE;
}

/* Reverse on. */
static gboolean
vte_sequence_handler_mr(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.reverse = 1;
	return FALSE;
}

/* Cursor right. */
static gboolean
vte_sequence_handler_nd(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	if ((screen->cursor_current.col + 1) < terminal->column_count) {
		/* There's room to move right. */
		screen->cursor_current.col++;
	}
	return FALSE;
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static gboolean
vte_sequence_handler_next_line(VteTerminal *terminal,
			       const char *match,
			       GQuark match_quark,
			       GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	return vte_sequence_handler_DO(terminal, match, match_quark, params);
}

/* No-op. */
static gboolean
vte_sequence_handler_noop(VteTerminal *terminal,
			  const char *match,
			  GQuark match_quark,
			  GValueArray *params)
{
	return FALSE;
}

/* Carriage return command(?). */
static gboolean
vte_sequence_handler_nw(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_cr(terminal, match, match_quark, params);
}

/* Restore cursor (position). */
static gboolean
vte_sequence_handler_rc(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_current.col = screen->cursor_saved.col;
	screen->cursor_current.row = CLAMP(screen->cursor_saved.row +
					   screen->insert_delta,
					   screen->insert_delta,
					   screen->insert_delta +
					   terminal->row_count - 1);
	return FALSE;
}

/* Cursor down, with scrolling. */
static gboolean
vte_sequence_handler_reverse_index(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	return vte_sequence_handler_sr(terminal, match, match_quark, params);
}

/* Cursor right N characters. */
static gboolean
vte_sequence_handler_RI(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_multiple(terminal, match, match_quark,
					     params, vte_sequence_handler_nd);
}

/* Save cursor (position). */
static gboolean
vte_sequence_handler_sc(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_saved.col = screen->cursor_current.col;
	screen->cursor_saved.row = CLAMP(screen->cursor_current.row -
					 screen->insert_delta,
					 0, terminal->row_count - 1);
	return FALSE;
}

/* Scroll the text down one line, but don't move the cursor. */
static gboolean
vte_sequence_handler_scroll_down_one(VteTerminal *terminal,
				     const char *match,
				     GQuark match_quark,
				     GValueArray *params)
{
	return vte_sequence_handler_scroll_up_or_down(terminal, 1);
}

/* Scroll the text down, but don't move the cursor. */
static gboolean
vte_sequence_handler_scroll_down(VteTerminal *terminal,
				 const char *match,
				 GQuark match_quark,
				 GValueArray *params)
{
	return vte_sequence_handler_multiple(terminal, match, match_quark,
					     params, vte_sequence_handler_scroll_down_one);
}

/* Scroll the text up one line, but don't move the cursor. */
static gboolean
vte_sequence_handler_scroll_up_one(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	return vte_sequence_handler_scroll_up_or_down(terminal, -1);
}

/* Scroll the text up, but don't move the cursor. */
static gboolean
vte_sequence_handler_scroll_up(VteTerminal *terminal,
			       const char *match,
			       GQuark match_quark,
			       GValueArray *params)
{
	return vte_sequence_handler_multiple(terminal, match, match_quark,
					     params, vte_sequence_handler_scroll_up_one);
}

/* Standout end. */
static gboolean
vte_sequence_handler_se(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	char *bold, *underline, *standout, *reverse, *half, *blink;

	/* Standout may be mapped to another attribute, so attempt to do
	 * the Right Thing here. */
	standout = _vte_termcap_find_string(terminal->pvt->termcap,
					    terminal->pvt->emulation,
					    "so");
	g_assert(standout != NULL);
	blink = _vte_termcap_find_string(terminal->pvt->termcap,
					 terminal->pvt->emulation,
					 "mb");
	bold = _vte_termcap_find_string(terminal->pvt->termcap,
					terminal->pvt->emulation,
					"md");
	half = _vte_termcap_find_string(terminal->pvt->termcap,
					terminal->pvt->emulation,
					"mh");
	reverse = _vte_termcap_find_string(terminal->pvt->termcap,
					   terminal->pvt->emulation,
					   "mr");
	underline = _vte_termcap_find_string(terminal->pvt->termcap,
					     terminal->pvt->emulation,
					     "us");

	/* If the standout sequence is the same as another sequence, do what
	 * we'd do for that other sequence instead. */
	if (blink && (g_ascii_strcasecmp(standout, blink) == 0)) {
		vte_sequence_handler_me(terminal, match, match_quark, params);
	} else
	if (bold && (g_ascii_strcasecmp(standout, bold) == 0)) {
		vte_sequence_handler_me(terminal, match, match_quark, params);
	} else
	if (half && (g_ascii_strcasecmp(standout, half) == 0)) {
		vte_sequence_handler_me(terminal, match, match_quark, params);
	} else
	if (reverse && (g_ascii_strcasecmp(standout, reverse) == 0)) {
		vte_sequence_handler_me(terminal, match, match_quark, params);
	} else
	if (underline && (g_ascii_strcasecmp(standout, underline) == 0)) {
		vte_sequence_handler_ue(terminal, match, match_quark, params);
	} else {
		/* Otherwise just set standout mode. */
		terminal->pvt->screen->defaults.standout = 0;
	}

	if (blink) {
		g_free(blink);
	}
	if (bold) {
		g_free(bold);
	}
	if (half) {
		g_free(half);
	}
	if (reverse) {
		g_free(reverse);
	}
	if (underline) {
		g_free(underline);
	}
	g_free(standout);
	return FALSE;
}

/* Cursor down, with scrolling. */
static gboolean
vte_sequence_handler_sf(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GtkWidget *widget;
	VteRowData *row;
	long start, end;
	VteScreen *screen;

	widget = GTK_WIDGET(terminal);
	screen = terminal->pvt->screen;

	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = start + terminal->row_count - 1;
	}

	if (screen->cursor_current.row == end) {
		if (screen->scrolling_restricted) {
			if (start == screen->insert_delta) {
				/* Scroll this line into the scrollback
				 * buffer by inserting a line at the next
				 * line and scrolling the area up. */
				row = _vte_new_row_data_sized(terminal, TRUE);
				screen->insert_delta++;
				screen->scroll_delta++;
				screen->cursor_current.row++;
				/* update start and end, as they are relative
				 * to insert_delta. */
				start++;
				end++;
				_vte_ring_insert_preserve(terminal->pvt->screen->row_data,
							  screen->cursor_current.row,
							  row);
				/* This may generate multiple redraws, so
				 * disable fast scrolling for now. */
				terminal->pvt->scroll_lock_count++;
				gdk_window_freeze_updates(widget->window);
				/* Force the areas below the region to be
				 * redrawn -- they've moved. */
				_vte_terminal_scroll_region(terminal, start,
							    end - start + 1, 1);
				/* Force scroll. */
				_vte_terminal_ensure_cursor(terminal, FALSE);
				_vte_terminal_adjust_adjustments(terminal, TRUE);
				/* Allow updates again. */
				gdk_window_thaw_updates(widget->window);
				terminal->pvt->scroll_lock_count--;
			} else {
				/* If we're at the bottom of the scrolling
				 * region, add a line at the top to scroll the
				 * bottom off. */
				vte_remove_line_internal(terminal, start);
				vte_insert_line_internal(terminal, end);
				/* This may generate multiple redraws, so
				 * disable fast scrolling for now. */
				terminal->pvt->scroll_lock_count++;
				gdk_window_freeze_updates(widget->window);
				/* Update the display. */
				_vte_terminal_scroll_region(terminal, start,
							   end - start + 1, -1);
				_vte_invalidate_cells(terminal,
						      0, terminal->column_count,
						      end - 2, 2);
				/* Allow updates again. */
				gdk_window_thaw_updates(widget->window);
				terminal->pvt->scroll_lock_count--;
			}
		} else {
			/* Scroll up with history. */
			screen->cursor_current.row++;
			_vte_terminal_update_insert_delta(terminal);
		}
	} else {
		/* Otherwise, just move the cursor down. */
		screen->cursor_current.row++;
		_vte_terminal_ensure_cursor(terminal, TRUE);
	}
	/* Adjust the scrollbars if necessary. */
	_vte_terminal_adjust_adjustments(terminal, FALSE);
	return FALSE;
}

/* Cursor down, with scrolling. */
static gboolean
vte_sequence_handler_SF(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_multiple(terminal, match, match_quark,
					     params, vte_sequence_handler_sf);
}

/* Standout start. */
static gboolean
vte_sequence_handler_so(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	char *bold, *underline, *standout, *reverse, *half, *blink;

	/* Standout may be mapped to another attribute, so attempt to do
	 * the Right Thing here. */
	standout = _vte_termcap_find_string(terminal->pvt->termcap,
				     terminal->pvt->emulation,
				    "so");
	g_assert(standout != NULL);
	blink = _vte_termcap_find_string(terminal->pvt->termcap,
					 terminal->pvt->emulation,
					 "mb");
	bold = _vte_termcap_find_string(terminal->pvt->termcap,
					terminal->pvt->emulation,
					"md");
	half = _vte_termcap_find_string(terminal->pvt->termcap,
					terminal->pvt->emulation,
					"mh");
	reverse = _vte_termcap_find_string(terminal->pvt->termcap,
					   terminal->pvt->emulation,
					   "mr");
	underline = _vte_termcap_find_string(terminal->pvt->termcap,
					     terminal->pvt->emulation,
					     "us");

	/* If the standout sequence is the same as another sequence, do what
	 * we'd do for that other sequence instead. */
	if (blink && (g_ascii_strcasecmp(standout, blink) == 0)) {
		vte_sequence_handler_mb(terminal, match, match_quark, params);
	} else
	if (bold && (g_ascii_strcasecmp(standout, bold) == 0)) {
		vte_sequence_handler_md(terminal, match, match_quark, params);
	} else
	if (half && (g_ascii_strcasecmp(standout, half) == 0)) {
		vte_sequence_handler_mh(terminal, match, match_quark, params);
	} else
	if (reverse && (g_ascii_strcasecmp(standout, reverse) == 0)) {
		vte_sequence_handler_mr(terminal, match, match_quark, params);
	} else
	if (underline && (g_ascii_strcasecmp(standout, underline) == 0)) {
		vte_sequence_handler_us(terminal, match, match_quark, params);
	} else {
		/* Otherwise just set standout mode. */
		terminal->pvt->screen->defaults.standout = 1;
	}

	if (blink) {
		g_free(blink);
	}
	if (bold) {
		g_free(bold);
	}
	if (half) {
		g_free(half);
	}
	if (reverse) {
		g_free(reverse);
	}
	if (underline) {
		g_free(underline);
	}
	g_free(standout);
	return FALSE;
}

/* Cursor up, scrolling if need be. */
static gboolean
vte_sequence_handler_sr(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GtkWidget *widget;
	long start, end;
	VteScreen *screen;

	widget = GTK_WIDGET(terminal);
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
	_vte_terminal_adjust_adjustments(terminal, FALSE);
	/* We modified the display, so make a note of it. */
	terminal->pvt->text_modified_flag = TRUE;
	return FALSE;
}

/* Cursor up, with scrolling. */
static gboolean
vte_sequence_handler_SR(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_multiple(terminal, match, match_quark,
					     params, vte_sequence_handler_sr);
}

/* Set tab stop in the current column. */
static gboolean
vte_sequence_handler_st(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	if (terminal->pvt->tabstops == NULL) {
		terminal->pvt->tabstops = g_hash_table_new(g_direct_hash,
							   g_direct_equal);
	}
	_vte_terminal_set_tabstop(terminal,
				 terminal->pvt->screen->cursor_current.col);
	return FALSE;
}

/* Tab. */
static gboolean
vte_sequence_handler_ta(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	long newcol;

	/* Calculate which column is the next tab stop. */
	newcol = terminal->pvt->screen->cursor_current.col;

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

	terminal->pvt->screen->cursor_current.col = newcol;
	return FALSE;
}

/* Clear tabs selectively. */
static gboolean
vte_sequence_handler_tab_clear(VteTerminal *terminal,
			       const char *match,
			       GQuark match_quark,
			       GValueArray *params)
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
	return FALSE;
}

/* Move to status line. */
static gboolean
vte_sequence_handler_ts(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->status_line = TRUE;
	g_string_truncate(terminal->pvt->screen->status_line_contents, 0);
	_vte_terminal_emit_status_line_changed(terminal);
	return FALSE;
}

/* Underline this character and move right. */
static gboolean
vte_sequence_handler_uc(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct vte_charcell *cell;
	int column;
	VteScreen *screen;

	screen = terminal->pvt->screen;
	column = screen->cursor_current.col;
	cell = vte_terminal_find_charcell(terminal,
					  column,
					  screen->cursor_current.row);
	while ((cell != NULL) && (cell->fragment) && (column > 0)) {
		column--;
		cell = vte_terminal_find_charcell(terminal,
						  column,
						  screen->cursor_current.row);
	}
	if (cell != NULL) {
		/* Set this character to be underlined. */
		cell->underline = 1;
		/* Cause the character to be repainted. */
		_vte_invalidate_cells(terminal,
				      column, cell->columns,
				      screen->cursor_current.row, 1);
		/* Move the cursor right. */
		vte_sequence_handler_nd(terminal, match, match_quark, params);
	}

	/* We've modified the display without changing the text.  Make a note
	 * of it. */
	terminal->pvt->text_modified_flag = TRUE;
	return FALSE;
}

/* Underline end. */
static gboolean
vte_sequence_handler_ue(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.underline = 0;
	return FALSE;
}

/* Cursor up, no scrolling. */
static gboolean
vte_sequence_handler_up(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
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
	return FALSE;
}

/* Cursor up N lines, no scrolling. */
static gboolean
vte_sequence_handler_UP(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	return vte_sequence_handler_multiple(terminal, match, match_quark,
					     params, vte_sequence_handler_up);
}

/* Underline start. */
static gboolean
vte_sequence_handler_us(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.underline = 1;
	return FALSE;
}

/* Visible bell. */
static gboolean
vte_sequence_handler_vb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GtkWidget *widget;
	gint width, height, state;

	widget = GTK_WIDGET(terminal);
	if (GTK_WIDGET_REALIZED(widget)) {
		gdk_drawable_get_size(widget->window, &width, &height);
		state = GTK_WIDGET_STATE(widget);
		/* Fill the screen with the default foreground color, and then
		 * repaint everything, to provide visual bell. */
		gdk_draw_rectangle(widget->window,
				   widget->style->fg_gc[state],
				   TRUE,
				   0, 0,
				   width, height);
		gdk_window_process_updates(widget->window, TRUE);
		/* Force the repaint. */
		_vte_invalidate_all(terminal);
		gdk_window_process_updates(widget->window, TRUE);
	}
	return FALSE;
}

/* Cursor visible. */
static gboolean
vte_sequence_handler_ve(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->cursor_visible = TRUE;
	return FALSE;
}

/* Vertical tab. */
static gboolean
vte_sequence_handler_vertical_tab(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	return vte_sequence_handler_index(terminal, match, match_quark, params);
}

/* Cursor invisible. */
static gboolean
vte_sequence_handler_vi(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->cursor_visible = FALSE;
	return FALSE;
}

/* Cursor standout. */
static gboolean
vte_sequence_handler_vs(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->cursor_visible = TRUE; /* FIXME: should be *more*
						 visible. */
	return FALSE;
}

/* Handle ANSI color setting and related stuffs (SGR). */
static gboolean
vte_sequence_handler_character_attributes(VteTerminal *terminal,
					  const char *match,
					  GQuark match_quark,
					  GValueArray *params)
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
			terminal->pvt->screen->defaults.bold = 1;
			terminal->pvt->screen->defaults.half = 0;
			break;
		case 2:
			terminal->pvt->screen->defaults.half = 1;
			terminal->pvt->screen->defaults.bold = 0;
			break;
		case 4:
			terminal->pvt->screen->defaults.underline = 1;
			break;
		case 5:
			terminal->pvt->screen->defaults.blink = 1;
			break;
		case 7:
			terminal->pvt->screen->defaults.reverse = 1;
			break;
		case 8:
			terminal->pvt->screen->defaults.invisible = 1;
			break;
		case 9:
			terminal->pvt->screen->defaults.strikethrough = 1;
			break;
		case 21: /* Error in old versions of linux console. */
		case 22: /* ECMA 48. */
			terminal->pvt->screen->defaults.bold = 0;
			terminal->pvt->screen->defaults.half = 0;
			break;
		case 24:
			terminal->pvt->screen->defaults.underline = 0;
			break;
		case 25:
			terminal->pvt->screen->defaults.blink = 0;
			break;
		case 27:
			terminal->pvt->screen->defaults.reverse = 0;
			break;
		case 28:
			terminal->pvt->screen->defaults.invisible = 0;
			break;
		case 29:
			terminal->pvt->screen->defaults.strikethrough = 0;
			break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
			terminal->pvt->screen->defaults.fore = param - 30;
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
 				terminal->pvt->screen->defaults.fore = param1;
 				i += 2;
 			}
 			
 			break;
 		}
		case 39:
			/* default foreground, no underscore */
			terminal->pvt->screen->defaults.fore = VTE_DEF_FG;
			/* By ECMA 48, this underline off has no business
			   being here, but the Linux console specifies it. */
			terminal->pvt->screen->defaults.underline = 0;
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			terminal->pvt->screen->defaults.back = param - 40;
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
  				terminal->pvt->screen->defaults.back = param1;
  				i += 2;
  			}
  			break;
  			
  		}
		case 49:
			/* default background */
			terminal->pvt->screen->defaults.back = VTE_DEF_BG;
			break;
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
			terminal->pvt->screen->defaults.fore = param - 90 + VTE_COLOR_BRIGHT_OFFSET;
			break;
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
			terminal->pvt->screen->defaults.back = param - 100 + VTE_COLOR_BRIGHT_OFFSET;
			break;
		}
	}
	/* If we had no parameters, default to the defaults. */
	if (i == 0) {
		_vte_terminal_set_default_attributes(terminal);
	}
	/* Save the new colors. */
	terminal->pvt->screen->color_defaults.fore =
		terminal->pvt->screen->defaults.fore;
	terminal->pvt->screen->color_defaults.back =
		terminal->pvt->screen->defaults.back;
	terminal->pvt->screen->fill_defaults.fore =
		terminal->pvt->screen->defaults.fore;
	terminal->pvt->screen->fill_defaults.back =
		terminal->pvt->screen->defaults.back;
	return FALSE;
}

/* Clear above the current line. */
static gboolean
vte_sequence_handler_clear_above_current(VteTerminal *terminal,
					 const char *match,
					 GQuark match_quark,
					 GValueArray *params)
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
			rowdata = _vte_ring_index(screen->row_data,
						  VteRowData *, i);
			g_assert(rowdata != NULL);
			/* Remove it. */
			if (rowdata->cells->len > 0) {
				g_array_set_size(rowdata->cells, 0);
			}
			/* Add new cells until we fill the row. */
			vte_g_array_fill(rowdata->cells,
					 &screen->fill_defaults,
					 terminal->column_count);
			rowdata->soft_wrapped = 0;
			/* Repaint the row. */
			_vte_invalidate_cells(terminal,
					      0, terminal->column_count,
					      i, 1);
		}
	}
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Clear the entire screen. */
static gboolean
vte_sequence_handler_clear_screen(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	VteRowData *rowdata;
	long i, initial, row;
	VteScreen *screen;
	screen = terminal->pvt->screen;
	initial = screen->insert_delta;
	row = screen->cursor_current.row - screen->insert_delta;
	/* Add a new screen's worth of rows. */
	for (i = 0; i < terminal->row_count; i++) {
		/* Add a new row */
		if (i == 0) {
			initial = _vte_ring_next(screen->row_data);
		}
		rowdata = _vte_new_row_data_sized(terminal, TRUE);
		_vte_ring_append(screen->row_data, rowdata);
	}
	/* Move the cursor and insertion delta to the first line in the
	 * newly-cleared area and scroll if need be. */
	screen->insert_delta = initial;
	screen->cursor_current.row = row + screen->insert_delta;
	_vte_terminal_adjust_adjustments(terminal, FALSE);
	/* Redraw everything. */
	_vte_invalidate_all(terminal);
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Move the cursor to the given column, 1-based. */
static gboolean
vte_sequence_handler_cursor_character_absolute(VteTerminal *terminal,
					       const char *match,
					       GQuark match_quark,
					       GValueArray *params)
{
	return vte_sequence_handler_offset(terminal, match, match_quark, params,
					   -1, vte_sequence_handler_ch);
}

/* Move the cursor to the given position, 1-based. */
static gboolean
vte_sequence_handler_cursor_position(VteTerminal *terminal,
				     const char *match,
				     GQuark match_quark,
				     GValueArray *params)
{
	return vte_sequence_handler_offset(terminal, match, match_quark, params,
					   -1, vte_sequence_handler_cm);
}

/* Request terminal attributes. */
static gboolean
vte_sequence_handler_request_terminal_parameters(VteTerminal *terminal,
						 const char *match,
						 GQuark match_quark,
						 GValueArray *params)
{
	vte_terminal_feed_child(terminal, "[?x", -1);
	return FALSE;
}

/* Request terminal attributes. */
static gboolean
vte_sequence_handler_return_terminal_status(VteTerminal *terminal,
					    const char *match,
					    GQuark match_quark,
					    GValueArray *params)
{
	vte_terminal_feed_child(terminal, "", -1);
	return FALSE;
}

/* Send primary device attributes. */
static gboolean
vte_sequence_handler_send_primary_device_attributes(VteTerminal *terminal,
						    const char *match,
						    GQuark match_quark,
						    GValueArray *params)
{
	/* Claim to be a VT220 with only national character set support. */
	vte_terminal_feed_child(terminal, "[?62;9;c", -1);
	return FALSE;
}

/* Send terminal ID. */
static gboolean
vte_sequence_handler_return_terminal_id(VteTerminal *terminal,
					const char *match,
					GQuark match_quark,
					GValueArray *params)
{
	return vte_sequence_handler_send_primary_device_attributes(terminal,
								   match,
								   match_quark,
								   params);
}

/* Send secondary device attributes. */
static gboolean
vte_sequence_handler_send_secondary_device_attributes(VteTerminal *terminal,
						      const char *match,
						      GQuark match_quark,
						      GValueArray *params)
{
	char **version, *ret;
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
	ret = g_strdup_printf(_VTE_CAP_ESC "[>1;%ld;0c", ver);
	vte_terminal_feed_child(terminal, ret, -1);
	g_free(ret);
	return FALSE;
}

/* Set one or the other. */
static gboolean
vte_sequence_handler_set_icon_title(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params)
{
	return vte_sequence_handler_set_title_internal(terminal,
						       match, match_quark,
						       params, "icon");
}
static gboolean
vte_sequence_handler_set_window_title(VteTerminal *terminal,
				      const char *match,
				      GQuark match_quark,
				      GValueArray *params)
{
	return vte_sequence_handler_set_title_internal(terminal,
						       match, match_quark,
						       params, "window");
}

/* Set both the window and icon titles to the same string. */
static gboolean
vte_sequence_handler_set_icon_and_window_title(VteTerminal *terminal,
						  const char *match,
						  GQuark match_quark,
						  GValueArray *params)
{
	int again;
	again = 0;
	if (vte_sequence_handler_set_title_internal(terminal,
						    match, match_quark,
						    params, "icon")) {
		again++;
	}
	if (vte_sequence_handler_set_title_internal(terminal,
						    match, match_quark,
						    params, "window")) {
		again++;
	}
	return (again > 0);
}

/* Restrict the scrolling region. */
static gboolean
vte_sequence_handler_set_scrolling_region(VteTerminal *terminal,
					  const char *match,
					  GQuark match_quark,
					  GValueArray *params)
{
	return vte_sequence_handler_offset(terminal, match, match_quark, params,
					   -1, vte_sequence_handler_cs);
}

/* Set the application or normal keypad. */
static gboolean
vte_sequence_handler_application_keypad(VteTerminal *terminal,
					const char *match,
					GQuark match_quark,
					GValueArray *params)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_KEYBOARD)) {
		fprintf(stderr, "Entering application keypad mode.\n");
	}
#endif
	terminal->pvt->keypad_mode = VTE_KEYMODE_APPLICATION;
	return FALSE;
}

static gboolean
vte_sequence_handler_normal_keypad(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_KEYBOARD)) {
		fprintf(stderr, "Leaving application keypad mode.\n");
	}
#endif
	terminal->pvt->keypad_mode = VTE_KEYMODE_NORMAL;
	return FALSE;
}

/* Move the cursor. */
static gboolean
vte_sequence_handler_character_position_absolute(VteTerminal *terminal,
						 const char *match,
						 GQuark match_quark,
						 GValueArray *params)
{
	return vte_sequence_handler_offset(terminal, match, match_quark, params,
					   -1, vte_sequence_handler_ch);
}
static gboolean
vte_sequence_handler_line_position_absolute(VteTerminal *terminal,
					    const char *match,
					    GQuark match_quark,
					    GValueArray *params)
{
	return vte_sequence_handler_offset(terminal, match, match_quark, params,
					   -1, vte_sequence_handler_cv);
}

/* Set certain terminal attributes. */
static gboolean
vte_sequence_handler_set_mode(VteTerminal *terminal,
			      const char *match,
			      GQuark match_quark,
			      GValueArray *params)
{
	int i, again;
	long setting;
	GValue *value;
	if ((params == NULL) || (params->n_values == 0)) {
		return FALSE;
	}
	again = 0;
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		if (vte_sequence_handler_set_mode_internal(terminal, setting,
							   TRUE)) {
			again++;
		}
	}
	return (again > 0);
}

/* Unset certain terminal attributes. */
static gboolean
vte_sequence_handler_reset_mode(VteTerminal *terminal,
				const char *match,
				GQuark match_quark,
				GValueArray *params)
{
	int i, again;
	long setting;
	GValue *value;
	if ((params == NULL) || (params->n_values == 0)) {
		return FALSE;
	}
	again = 0;
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		if (vte_sequence_handler_set_mode_internal(terminal, setting,
							   FALSE)) {
			again++;
		}
	}
	return (again > 0);
}

/* Set certain terminal attributes. */
static gboolean
vte_sequence_handler_decset(VteTerminal *terminal,
			    const char *match,
			    GQuark match_quark,
			    GValueArray *params)
{
	GValue *value;
	long setting;
	int i, again;
	if ((params == NULL) || (params->n_values == 0)) {
		return FALSE;
	}
	again = 0;
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		if (vte_sequence_handler_decset_internal(terminal, setting,
							 FALSE, FALSE, TRUE)) {
			again++;
		}
	}
	return (again > 0);
}

/* Unset certain terminal attributes. */
static gboolean
vte_sequence_handler_decreset(VteTerminal *terminal,
			      const char *match,
			      GQuark match_quark,
			      GValueArray *params)
{
	GValue *value;
	long setting;
	int i, again;
	if ((params == NULL) || (params->n_values == 0)) {
		return FALSE;
	}
	again = 0;
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		if (vte_sequence_handler_decset_internal(terminal, setting,
							 FALSE, FALSE, FALSE)) {
			again++;
		}
	}
	return (again > 0);
}

/* Erase a specified number of characters. */
static gboolean
vte_sequence_handler_erase_characters(VteTerminal *terminal,
				      const char *match,
				      GQuark match_quark,
				      GValueArray *params)
{
	return vte_sequence_handler_ec(terminal, match, match_quark, params);
}

/* Erase certain lines in the display. */
static gboolean
vte_sequence_handler_erase_in_display(VteTerminal *terminal,
				      const char *match,
				      GQuark match_quark,
				      GValueArray *params)
{
	GValue *value;
	long param;
	int i;
	gboolean again;
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
	again = FALSE;
	switch (param) {
	case 0:
		/* Clear below the current line. */
		again = vte_sequence_handler_cd(terminal, NULL, 0, NULL);
		break;
	case 1:
		/* Clear above the current line. */
		again = vte_sequence_handler_clear_above_current(terminal,
								 NULL,
								 0,
								 NULL);
		/* Clear everything to the left of the cursor, too. */
		/* FIXME: vttest. */
		again = vte_sequence_handler_cb(terminal, NULL, 0, NULL) ||
			again;
		break;
	case 2:
		/* Clear the entire screen. */
		again = vte_sequence_handler_clear_screen(terminal,
							  NULL,
							  0,
							  NULL);
		break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return again;
}

/* Erase certain parts of the current line in the display. */
static gboolean
vte_sequence_handler_erase_in_line(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	GValue *value;
	long param;
	int i;
	gboolean again;
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
	again = FALSE;
	switch (param) {
	case 0:
		/* Clear to end of the line. */
		again = vte_sequence_handler_ce(terminal, NULL, 0, NULL);
		break;
	case 1:
		/* Clear to start of the line. */
		again = vte_sequence_handler_cb(terminal, NULL, 0, NULL);
		break;
	case 2:
		/* Clear the entire line. */
		again = vte_sequence_handler_clear_current_line(terminal,
								NULL, 0, NULL);
		break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return again;
}

/* Perform a full-bore reset. */
static gboolean
vte_sequence_handler_full_reset(VteTerminal *terminal,
				const char *match,
				GQuark match_quark,
				GValueArray *params)
{
	vte_terminal_reset(terminal, TRUE, TRUE);
	return FALSE;
}

/* Insert a specified number of blank characters. */
static gboolean
vte_sequence_handler_insert_blank_characters(VteTerminal *terminal,
					     const char *match,
					     GQuark match_quark,
					     GValueArray *params)
{
	return vte_sequence_handler_IC(terminal, match, match_quark, params);
}

/* Insert a certain number of lines below the current cursor. */
static gboolean
vte_sequence_handler_insert_lines(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
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
		param = g_value_get_long(value);
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
	_vte_terminal_adjust_adjustments(terminal, FALSE);
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_inserted_count++;
	return FALSE;
}

/* Delete certain lines from the scrolling region. */
static gboolean
vte_sequence_handler_delete_lines(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
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
		param = g_value_get_long(value);
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
	_vte_terminal_adjust_adjustments(terminal, FALSE);
	/* We've modified the display.  Make a note of it. */
	terminal->pvt->text_deleted_count++;
	return FALSE;
}

/* Set the terminal encoding. */
static gboolean
vte_sequence_handler_local_charset(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	G_CONST_RETURN char *locale_encoding;
	g_get_charset(&locale_encoding);
	vte_terminal_set_encoding(terminal, locale_encoding);
	return FALSE;
}

static gboolean
vte_sequence_handler_utf_8_charset(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	vte_terminal_set_encoding(terminal, "UTF-8");
	return FALSE;
}

/* Device status reports. The possible reports are the cursor position and
 * whether or not we're okay. */
static gboolean
vte_sequence_handler_device_status_report(VteTerminal *terminal,
					  const char *match,
					  GQuark match_quark,
					  GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param;
	char buf[LINE_MAX];

	screen = terminal->pvt->screen;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		param = g_value_get_long(value);
		switch (param) {
		case 5:
			/* Send a thumbs-up sequence. */
			snprintf(buf, sizeof(buf), "%s%dn", _VTE_CAP_CSI, 0);
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		case 6:
			/* Send the cursor position. */
			snprintf(buf, sizeof(buf),
				 "%s%ld;%ldR", _VTE_CAP_CSI,
				 screen->cursor_current.row + 1 -
				 screen->insert_delta,
				 screen->cursor_current.col + 1);
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		default:
			break;
		}
	}
	return FALSE;
}

/* DEC-style device status reports. */
static gboolean
vte_sequence_handler_dec_device_status_report(VteTerminal *terminal,
					      const char *match,
					      GQuark match_quark,
					      GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param;
	char buf[LINE_MAX];

	screen = terminal->pvt->screen;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		param = g_value_get_long(value);
		switch (param) {
		case 6:
			/* Send the cursor position. */
			snprintf(buf, sizeof(buf),
				 "%s?%ld;%ldR", _VTE_CAP_CSI,
				 screen->cursor_current.row + 1 -
				 screen->insert_delta,
				 screen->cursor_current.col + 1);
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		case 15:
			/* Send printer status -- 10 = ready,
			 * 11 = not ready.  We don't print. */
			snprintf(buf, sizeof(buf), "%s?%dn", _VTE_CAP_CSI, 11);
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		case 25:
			/* Send UDK status -- 20 = locked,
			 * 21 = not locked.  I don't even know what
			 * that means, but punt anyway. */
			snprintf(buf, sizeof(buf), "%s?%dn", _VTE_CAP_CSI, 20);
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		case 26:
			/* Send keyboard status.  50 = no locator. */
			snprintf(buf, sizeof(buf), "%s?%dn", _VTE_CAP_CSI, 50);
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		default:
			break;
		}
	}
	return FALSE;
}

/* Restore a certain terminal attribute. */
static gboolean
vte_sequence_handler_restore_mode(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	GValue *value;
	long setting;
	int i, again;
	if ((params == NULL) || (params->n_values == 0)) {
		return FALSE;
	}
	again = 0;
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		if (vte_sequence_handler_decset_internal(terminal, setting,
						         TRUE, FALSE, FALSE)) {
			again++;
		}
	}
	return (again > 0);
}

/* Save a certain terminal attribute. */
static gboolean
vte_sequence_handler_save_mode(VteTerminal *terminal,
			       const char *match,
			       GQuark match_quark,
			       GValueArray *params)
{
	GValue *value;
	long setting;
	int i, again;
	if ((params == NULL) || (params->n_values == 0)) {
		return FALSE;
	}
	again = 0;
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		if (vte_sequence_handler_decset_internal(terminal, setting,
						         FALSE, TRUE, FALSE)) {
			again++;
		}
	}
	return (again > 0);
}

/* Perform a screen alignment test -- fill all visible cells with the
 * letter "E". */
static gboolean
vte_sequence_handler_screen_alignment_test(VteTerminal *terminal,
					   const char *match,
					   GQuark match_quark,
					   GValueArray *params)
{
	long row;
	VteRowData *rowdata;
	VteScreen *screen;
	struct vte_charcell cell;

	screen = terminal->pvt->screen;

	for (row = terminal->pvt->screen->insert_delta;
	     row < terminal->pvt->screen->insert_delta + terminal->row_count;
	     row++) {
		/* Find this row. */
		while (_vte_ring_next(screen->row_data) <= row) {
			rowdata = _vte_new_row_data(terminal);
			_vte_ring_append(screen->row_data, rowdata);
		}
		_vte_terminal_adjust_adjustments(terminal, TRUE);
		rowdata = _vte_ring_index(screen->row_data, VteRowData *, row);
		g_assert(rowdata != NULL);
		/* Clear this row. */
		if (rowdata->cells->len > 0) {
			g_array_set_size(rowdata->cells, 0);
		}
		_vte_terminal_emit_text_deleted(terminal);
		/* Fill this row. */
		cell = screen->basic_defaults;
		cell.c = 'E';
		cell.columns = 1;
		cell.empty = 0;
		vte_g_array_fill(rowdata->cells, &cell, terminal->column_count);
		_vte_terminal_emit_text_inserted(terminal);
	}
	_vte_invalidate_all(terminal);

	/* We modified the display, so make a note of it for completeness. */
	terminal->pvt->text_modified_flag = TRUE;
	return FALSE;
}

/* Perform a soft reset. */
static gboolean
vte_sequence_handler_soft_reset(VteTerminal *terminal,
				const char *match,
				GQuark match_quark,
				GValueArray *params)
{
	vte_terminal_reset(terminal, FALSE, FALSE);
	return FALSE;
}

/* Window manipulation control sequences.  Most of these are considered
 * bad ideas, but they're implemented as signals which the application
 * is free to ignore, so they're harmless. */
static gboolean
vte_sequence_handler_window_manipulation(VteTerminal *terminal,
					 const char *match,
					 GQuark match_quark,
					 GValueArray *params)
{
	GdkScreen *gscreen;
	VteScreen *screen;
	GValue *value;
	GtkWidget *widget;
	char buf[LINE_MAX];
	long param, arg1, arg2;
	guint width, height, i;

	widget = GTK_WIDGET(terminal);
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
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Deiconifying window.\n");
			}
#endif
			vte_terminal_emit_deiconify_window(terminal);
			break;
		case 2:
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Iconifying window.\n");
			}
#endif
			vte_terminal_emit_iconify_window(terminal);
			break;
		case 3:
			if ((arg1 != -1) && (arg2 != -2)) {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_PARSE)) {
					fprintf(stderr, "Moving window to "
						"%ld,%ld.\n", arg1, arg2);
				}
#endif
				vte_terminal_emit_move_window(terminal,
							      arg1, arg2);
				i += 2;
			}
			break;
		case 4:
			if ((arg1 != -1) && (arg2 != -1)) {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_PARSE)) {
					fprintf(stderr, "Resizing window "
						"(to %ldx%ld pixels).\n",
						arg2, arg1);
				}
#endif
				vte_terminal_emit_resize_window(terminal,
								arg2 +
								VTE_PAD_WIDTH * 2,
								arg1 +
								VTE_PAD_WIDTH * 2);
				i += 2;
			}
			break;
		case 5:
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Raising window.\n");
			}
#endif
			vte_terminal_emit_raise_window(terminal);
			break;
		case 6:
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Lowering window.\n");
			}
#endif
			vte_terminal_emit_lower_window(terminal);
			break;
		case 7:
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Refreshing window.\n");
			}
#endif
			_vte_invalidate_all(terminal);
			vte_terminal_emit_refresh_window(terminal);
			break;
		case 8:
			if ((arg1 != -1) && (arg2 != -1)) {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_PARSE)) {
					fprintf(stderr, "Resizing window "
						"(to %ld columns, %ld rows).\n",
						arg2, arg1);
				}
#endif
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
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_PARSE)) {
					fprintf(stderr, "Restoring window.\n");
				}
#endif
				vte_terminal_emit_restore_window(terminal);
				break;
			case 1:
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_PARSE)) {
					fprintf(stderr, "Maximizing window.\n");
				}
#endif
				vte_terminal_emit_maximize_window(terminal);
				break;
			default:
				break;
			}
			i++;
			break;
		case 11:
			/* If we're unmapped, then we're iconified. */
			snprintf(buf, sizeof(buf),
				 "%s%dt", _VTE_CAP_CSI,
				 1 + !GTK_WIDGET_MAPPED(widget));
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting window state %s.\n",
					GTK_WIDGET_MAPPED(widget) ?
					"non-iconified" : "iconified");
			}
#endif
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		case 13:
			/* Send window location, in pixels. */
			gdk_window_get_origin(widget->window,
					      &width, &height);
			snprintf(buf, sizeof(buf),
				 "%s%d;%dt", _VTE_CAP_CSI,
				 width + VTE_PAD_WIDTH, height + VTE_PAD_WIDTH);
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting window location"
					"(%d++,%d++).\n",
					width, height);
			}
#endif
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		case 14:
			/* Send window size, in pixels. */
			gdk_drawable_get_size(widget->window,
					      &width, &height);
			snprintf(buf, sizeof(buf),
				 "%s%d;%dt", _VTE_CAP_CSI,
				 height - 2 * VTE_PAD_WIDTH,
				 width - 2 * VTE_PAD_WIDTH);
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting window size "
					"(%dx%dn",
					width - 2 * VTE_PAD_WIDTH,
					height - 2 * VTE_PAD_WIDTH);
			}
#endif
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		case 18:
			/* Send widget size, in cells. */
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting widget size.\n");
			}
#endif
			snprintf(buf, sizeof(buf),
				 "%s%ld;%ldt", _VTE_CAP_CSI,
				 terminal->row_count,
				 terminal->column_count);
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		case 19:
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting screen size.\n");
			}
#endif
			if (gtk_widget_has_screen(widget)) {
				gscreen = gtk_widget_get_screen(widget);
			} else {
				gscreen = gdk_display_get_default_screen(gtk_widget_get_display(widget));
			}
			height = gdk_screen_get_height(gscreen);
			width = gdk_screen_get_width(gscreen);
			snprintf(buf, sizeof(buf),
				 "%s%ld;%ldt", _VTE_CAP_CSI,
				 height / terminal->char_height,
				 width / terminal->char_width);
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		case 20:
			/* Report the icon title. */
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting icon title.\n");
			}
#endif
			snprintf(buf, sizeof(buf),
				 "%sL%s%s",
				 _VTE_CAP_OSC,
				 "Terminal",
				 _VTE_CAP_ST);
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		case 21:
			/* Report the window title. */
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting window title.\n");
			}
#endif
			snprintf(buf, sizeof(buf),
				 "%sL%s%s",
				 _VTE_CAP_OSC,
				 "Terminal",
				 _VTE_CAP_ST);
			vte_terminal_feed_child(terminal, buf, strlen(buf));
			break;
		default:
			if (param >= 24) {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_PARSE)) {
					fprintf(stderr, "Resizing to %ld rows.\n",
						param);
				}
#endif
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
	return TRUE;
}

/* Complain that we got an escape sequence that's actually a keystroke. */
static gboolean
vte_sequence_handler_complain_key(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	g_warning(_("Got unexpected (key?) sequence `%s'."),
		  match ? match : "???");
	return FALSE;
}




/* LOOKUP */

static const VteTerminalSequenceHandler vte_sequence_handlers[] = {
  NULL,
#define VTE_SEQUENCE_HANDLER(name) name,
#include "vteseq-list.h"
#undef VTE_SEQUENCE_HANDLER
  NULL
};

enum {
  NULL_handler,
#define VTE_SEQUENCE_HANDLER(name) name##_index,
#include "vteseq-list.h"
#undef VTE_SEQUENCE_HANDLER
  _dummy_handler
};

#include "vteseq-table.h"

VteTerminalSequenceHandler
_vte_sequence_get_handler (const char *code,
			   GQuark quark)
{
	int len = strlen (code);

	/* bsearch in the right table */
	if (len == 2) {
		int min = 0, max = G_N_ELEMENTS (vte_sequence_handlers_two) - 1;
		do {
			int i = (min + max) / 2;
			int res = memcmp (vte_sequence_handlers_two[i].code, code, 2);
			if (res < 0)
				min = i + 1;
			else
				max = i;
		} while (min < max);

		if (!memcmp (vte_sequence_handlers_two[min].code, code, 2))
			return vte_sequence_handlers[vte_sequence_handlers_two[min].handler];
		else
			return NULL;
	} else {
		int min = 0, max = G_N_ELEMENTS (vte_sequence_handlers_others) - 1;
		do {
			int i = (min + max) / 2;
			int res = (int)vte_sequence_handlers_others[i].len - len;
			if (!res)
				res = strcmp (vte_sequence_handlers_others[i].code, code);
			if (res < 0)
				min = i + 1;
			else
				max = i;
		} while (min < max);

		if (vte_sequence_handlers_others[min].len == len &&
		    !strcmp (vte_sequence_handlers_others[min].code, code))
			return vte_sequence_handlers[vte_sequence_handlers_others[min].handler];
		else
			return NULL;
	}
}
