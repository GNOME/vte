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
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <iconv.h>
#include <langinfo.h>
#include <math.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangox.h>
#include "caps.h"
#include "marshal.h"
#include "pty.h"
#include "termcap.h"
#include "ring.h"
#include "trie.h"
#include "vte.h"
#include "vteaccess.h"
#include <X11/Xlib.h>
#ifdef HAVE_XFT
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>
#endif

#define VTE_TAB_WIDTH	8
#define VTE_LINE_WIDTH	1
#define VTE_DEF_FG	16
#define VTE_DEF_BG	17
#define VTE_SATURATION_MAX 10000
#define VTE_SCROLLBACK_MIN 100
#define VTE_DEFAULT_EMULATION "xterm-color"
#define VTE_DEFAULT_CURSOR GDK_XTERM
#define VTE_MOUSING_CURSOR GDK_LEFT_PTR

/* The structure we use to hold characters we're supposed to display -- this
 * includes any supported visible attributes. */
struct vte_charcell {
	wchar_t c;		/* The wide character. */
	guint16 columns: 2;	/* Number of visible columns (as determined
				   by wcwidth(c)). */
	guint16 fore: 5;	/* Indices in the color palette for the */
	guint16 back: 5;	/* foreground and background of the cell. */
	guint16 reverse: 1;	/* Single-bit attributes. */
	guint16 invisible: 1;
	guint16 bold: 1;
	guint16 standout: 1;
	guint16 underline: 1;
	guint16 half: 1;
	guint16 blink: 1;
	guint16 alternate: 1;
};

/* The terminal's keypad state.  A terminal can either be using the normal
 * keypad, or the "application" keypad.  Arrow key sequences, for example,
 * are really only defined for "application" mode. */
typedef enum {
	VTE_KEYPAD_NORMAL,
	VTE_KEYPAD_APPLICATION,
} VteKeypad;

typedef struct _VteScreen VteScreen;

typedef struct _VteWordCharRange {
	wchar_t start, end;
} VteWordCharRange;

/* Terminal private data. */
struct _VteTerminalPrivate {
	/* Emulation setup data. */
	struct vte_termcap *termcap;	/* termcap storage */
	struct vte_trie *trie;		/* control sequence trie */
	const char *termcap_path;	/* path to termcap file */
	const char *terminal;		/* terminal type to emulate */
	GTree *sequences;		/* sequence handlers, keyed by GQuark
					   based on the sequence name */
	struct {			/* boolean termcap flags */
		gboolean am;
	} flags;

	/* PTY handling data. */
	const char *shell;		/* shell we started */
	int pty_master;			/* pty master descriptor */
	GIOChannel *pty_input;		/* master input watch */
	GIOChannel *pty_output;		/* master output watch */
	pid_t pty_pid;			/* pid of child using pty slave */
	const char *encoding;		/* the pty's encoding */

	/* Input data queues. */
	iconv_t incoming_conv;		/* narrow/wide conversion state */
	unsigned char *incoming;	/* pending output characters */
	size_t n_incoming;
	gboolean processing;
	guint processing_tag;

	/* Output data queue. */
	unsigned char *outgoing;	/* pending input characters */
	size_t n_outgoing;
	iconv_t outgoing_conv_wide;
	iconv_t outgoing_conv_utf8;

	/* Data used when rendering the text. */
	struct {
		guint16 red, green, blue;
		unsigned long pixel;
#ifdef HAVE_XFT
		XRenderColor rcolor;
		XftColor ftcolor;
#endif
	} palette[18];
	XFontSet fontset;

#ifdef HAVE_XFT
	XftFont *ftfont;
	gboolean use_xft;
#endif
	PangoFontDescription *fontdesc;
	PangoLayout *layout;
	gboolean use_pango;

	/* Emulation state. */
	VteKeypad keypad;

	/* Screen data.  We support the normal screen, and an alternate
	 * screen, which seems to be a DEC-specific feature. */
	struct _VteScreen {
		VteRing *row_data;	/* row data, arranged as a GArray of
					   vte_charcell structures */
		struct {
			long row, col;
		} cursor_current, cursor_saved;
					/* the current and saved positions of
					   the [insertion] cursor */
		gboolean cursor_visible;
		gboolean insert;	/* insert mode */
		struct {
			int start, end;
		} scrolling_region;	/* the region we scroll in */
		gboolean scrolling_restricted;
		long scroll_delta;	/* scroll offset */
		long insert_delta;	/* insertion offset */
		struct vte_charcell defaults;	/* default characteristics
						   for insertion of any new
						   characters */
	} normal_screen, alternate_screen, *screen;

	gboolean has_selection;
	char *selection;
	enum {
		selection_type_char,
		selection_type_word,
		selection_type_line,
	} selection_type;
	struct {
		double x, y;
	} selection_origin, selection_last;
	struct {
		long x, y;
	} selection_start, selection_end;

	/* Options. */
	GArray *word_chars;

	gboolean scroll_on_output;
	gboolean scroll_on_keystroke;
	long scrollback_lines;

	gboolean alt_sends_escape;

	gboolean audible_bell;

	gboolean cursor_blinks;
	gint cursor_blink_tag;
	guint last_keypress_time;

	gboolean bg_transparent;
	gboolean bg_transparent_update_pending;
	guint bg_transparent_update_tag;
	GdkAtom bg_transparent_atom;
	GdkWindow *bg_transparent_window;
	GdkPixbuf *bg_transparent_image;
	GtkWidget *bg_toplevel;

	GdkPixbuf *bg_image;

	long bg_saturation;	/* out of VTE_SATURATION_MAX */

	GtkIMContext *im_context;
	char *im_preedit;
	int im_preedit_cursor;

	VteTerminalEraseBinding backspace_binding, delete_binding;

	gboolean xterm_font_tweak;

	gboolean mouse_send_xy_on_click;
	gboolean mouse_send_xy_on_button;
	gboolean mouse_hilite_tracking;
	gboolean mouse_cell_motion_tracking;
	gboolean mouse_all_motion_tracking;
	GdkCursor *mouse_default_cursor,
		  *mouse_mousing_cursor,
		  *mouse_inviso_cursor;
	guint mouse_last_button;
	gboolean mouse_autohide;
};

/* A function which can handle a terminal control sequence. */
typedef void (*VteTerminalSequenceHandler)(VteTerminal *terminal,
					   const char *match,
					   GQuark match_quark,
					   GValueArray *params);
static void vte_terminal_insert_char(GtkWidget *widget, wchar_t c);
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
static gboolean vte_terminal_io_read(GIOChannel *channel,
				     GdkInputCondition condition,
				     gpointer data);
static gboolean vte_terminal_io_write(GIOChannel *channel,
				      GdkInputCondition condition,
				      gpointer data);
static GdkFilterReturn vte_terminal_filter_property_changes(GdkXEvent *xevent,
							    GdkEvent *event,
							    gpointer data);
static void vte_terminal_queue_background_update(VteTerminal *terminal);

/* Free a no-longer-used row data array. */
static void
vte_free_row_data(gpointer freeing, gpointer data)
{
	if (freeing) {
		g_array_free((GArray*)freeing, FALSE);
	}
}

/* Allocate a new line. */
static GArray *
vte_new_row_data(void)
{
	return g_array_new(FALSE, FALSE, sizeof(struct vte_charcell));
}

/* Reset defaults for character insertion. */
static void
vte_terminal_set_default_attributes(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->defaults.fore = VTE_DEF_FG;
	terminal->pvt->screen->defaults.back = VTE_DEF_BG;
	terminal->pvt->screen->defaults.reverse = 0;
	terminal->pvt->screen->defaults.bold = 0;
	terminal->pvt->screen->defaults.invisible = 0;
	terminal->pvt->screen->defaults.standout = 0;
	terminal->pvt->screen->defaults.underline = 0;
	terminal->pvt->screen->defaults.half = 0;
	terminal->pvt->screen->defaults.blink = 0;
	/* Alternate charset isn't an attribute, though we treat it as one.
	 * terminal->pvt->screen->defaults.alternate = 0; */
}

/* Cause certain cells to be updated. */
static void
vte_invalidate_cells(VteTerminal *terminal,
		     glong column_start, gint column_count,
		     glong row_start, gint row_count)
{
	GdkRectangle rect;
	GtkWidget *widget;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);
	if (!GTK_WIDGET_REALIZED(widget)) {
		return;
	}

	/* Subtract the scrolling offset from the row start so that the
	 * resulting rectangle is relative to the visible portion of the
	 * buffer. */
	row_start -= terminal->pvt->screen->scroll_delta;

	/* Clamp the start values to reasonable numbers. */
	column_start = (column_start > 0) ? column_start : 0;
	row_start = (row_start > 0) ? row_start : 0;

	/* Convert the column and row start and end to pixel values
	 * by multiplying by the size of a character cell. */
	rect.x = column_start * terminal->char_width;
	rect.width = column_count * terminal->char_width;
	rect.y = row_start * terminal->char_height;
	rect.height = row_count * terminal->char_height;

	/* Invalidate the rectangle. */
	gdk_window_invalidate_rect(widget->window, &rect, TRUE);
}

/* Find the character in the given "virtual" position. */
static struct vte_charcell *
vte_terminal_find_charcell(VteTerminal *terminal, long row, long col)
{
	GArray *rowdata;
	struct vte_charcell *ret = NULL;
	VteScreen *screen;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	screen = terminal->pvt->screen;
	if (vte_ring_contains(screen->row_data, row)) {
		rowdata = vte_ring_index(screen->row_data, GArray*, row);
		if (rowdata->len > col) {
			ret = &g_array_index(rowdata, struct vte_charcell, col);
		}
	}
	return ret;
}

/* Cause the cursor to be redrawn. */
static void
vte_invalidate_cursor_once(gpointer data)
{
	VteTerminal *terminal;
	VteScreen *screen;
	struct vte_charcell *cell;
	size_t preedit_length;
	int columns;

	if (!VTE_IS_TERMINAL(data)) {
		return;
	}
	terminal = VTE_TERMINAL(data);

	if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
		if (terminal->pvt->im_preedit != NULL) {
			preedit_length = strlen(terminal->pvt->im_preedit);
		} else {
			preedit_length = 0;
		}

		screen = terminal->pvt->screen;
		columns = 1;
		cell = vte_terminal_find_charcell(terminal,
				     		  screen->cursor_current.row,
				     		  screen->cursor_current.col);
		if (cell != NULL) {
			columns = cell->columns;
		}

		vte_invalidate_cells(terminal,
				     screen->cursor_current.col,
				     columns + preedit_length,
				     screen->cursor_current.row,
				     1);
#ifdef VTE_DEBUG
#ifdef VTE_DEBUG_DRAW_CURSOR
	fprintf(stderr, "Invalidating cursor at (%d,%d-%d).\n",
		screen->cursor_current.row,
		screen->cursor_current.col,
		screen->cursor_current.col + columns + preedit_length);
#endif
#endif
	}
}

/* Invalidate the cursor repeatedly. */
static gboolean
vte_invalidate_cursor_periodic(gpointer data)
{
	VteTerminal *terminal;
	GtkSettings *settings;
	gint blink_cycle = 1000;

	g_return_val_if_fail(VTE_IS_TERMINAL(data), FALSE);
	if (!GTK_WIDGET_REALIZED(GTK_WIDGET(data))) {
		return TRUE;
	}

	terminal = VTE_TERMINAL(data);
	vte_invalidate_cursor_once(data);

	settings = gtk_widget_get_settings(GTK_WIDGET(data));
	if (G_IS_OBJECT(settings)) {
		g_object_get(G_OBJECT(settings), "gtk-cursor-blink-time",
			     &blink_cycle, NULL);
	}

	terminal->pvt->cursor_blink_tag = g_timeout_add(blink_cycle / 2,
							vte_invalidate_cursor_periodic,
							terminal);

	return FALSE;
}

/* Emit a "selection_changed" signal. */
static void
vte_terminal_emit_selection_changed(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "selection-changed");
}

/* Emit a "contents_changed" signal. */
static void
vte_terminal_emit_contents_changed(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "contents-changed");
}

/* Emit a "cursor_moved" signal. */
static void
vte_terminal_emit_cursor_moved(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "cursor-moved");
}

/* Emit an "icon-title-changed" signal. */
static void
vte_terminal_emit_icon_title_changed(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "icon-title-changed");
}

/* Emit a "window-title-changed" signal. */
static void
vte_terminal_emit_window_title_changed(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "window-title-changed");
}

/* Emit a "deiconify-window" signal. */
static void
vte_terminal_emit_deiconify_window(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "deiconify-window");
}

/* Emit a "iconify-window" signal. */
static void
vte_terminal_emit_iconify_window(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "iconify-window");
}

/* Emit a "raise-window" signal. */
static void
vte_terminal_emit_raise_window(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "raise-window");
}

/* Emit a "lower-window" signal. */
static void
vte_terminal_emit_lower_window(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "lower-window");
}

/* Emit a "maximize-window" signal. */
static void
vte_terminal_emit_maximize_window(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "maximize-window");
}

/* Emit a "refresh-window" signal. */
static void
vte_terminal_emit_refresh_window(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "refresh-window");
}

/* Emit a "restore-window" signal. */
static void
vte_terminal_emit_restore_window(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "restore-window");
}

/* Emit a "eof" signal. */
static void
vte_terminal_emit_eof(VteTerminal *terminal)
{
	g_signal_emit_by_name(terminal, "eof");
}

/* Emit a "char-size-changed" signal. */
static void
vte_terminal_emit_char_size_changed(VteTerminal *terminal,
				    guint width, guint height)
{
	g_signal_emit_by_name(terminal, "char-size-changed",
			      width, height);
}

/* Emit a "resize-window" signal.  (Pixels.) */
static void
vte_terminal_emit_resize_window(VteTerminal *terminal,
				guint width, guint height)
{
	g_signal_emit_by_name(terminal, "resize-window", width, height);
}

/* Emit a "move-window" signal.  (Pixels.) */
static void
vte_terminal_emit_move_window(VteTerminal *terminal, guint x, guint y)
{
	g_signal_emit_by_name(terminal, "move-window", x, y);
}

/* Deselect anything which is selected and refresh the screen if needed. */
static void
vte_terminal_deselect_all(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->has_selection) {
		terminal->pvt->has_selection = FALSE;
		vte_terminal_emit_selection_changed (terminal);
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     0, terminal->row_count);
	}
}

/* Update the adjustment field of the widget.  This function should be called
 * whenever we add rows to the history or switch screens. */
static void
vte_terminal_adjust_adjustments(VteTerminal *terminal)
{
	gboolean changed;
	long delta, next;
	long page_size;
	long rows;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(terminal->pvt->screen != NULL);
	g_return_if_fail(terminal->pvt->screen->row_data != NULL);

	/* Adjust the vertical, uh, adjustment. */
	changed = FALSE;

	/* The lower value should be the first row in the buffer. */
	delta = vte_ring_delta(terminal->pvt->screen->row_data);
#ifdef VTE_DEBUG
	fprintf(stderr, "Changing adjustment values "
		"(delta = %ld, scroll = %ld).\n",
		delta, terminal->pvt->screen->scroll_delta);
#endif
	if (terminal->adjustment->lower != delta) {
		terminal->adjustment->lower = delta;
		changed = TRUE;
	}

	/* The upper value is the number of rows which might be visible.  (Add
	 * one to the cursor offset because it's zero-based.) */
	next = vte_ring_delta(terminal->pvt->screen->row_data) +
	       vte_ring_length(terminal->pvt->screen->row_data);
	rows = MAX(next,
		   terminal->pvt->screen->cursor_current.row + 1);
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

	/* Set the scrollbar adjustment to where the screen wants it to be. */
	if (floor(gtk_adjustment_get_value(terminal->adjustment)) !=
	    terminal->pvt->screen->scroll_delta) {
		gtk_adjustment_set_value(terminal->adjustment,
					 terminal->pvt->screen->scroll_delta);
		changed = TRUE;
	}

	/* If anything changed, signal that there was a change. */
	if (changed == TRUE) {
#ifdef VTE_DEBUG
		fprintf(stderr, "Changed adjustment values "
			"(delta = %ld, scroll = %ld).\n",
			delta, terminal->pvt->screen->scroll_delta);
#endif
		vte_terminal_emit_contents_changed(terminal);
		gtk_adjustment_changed(terminal->adjustment);
	}
}

/* Scroll up or down in the current screen. */
static void
vte_terminal_scroll_pages(VteTerminal *terminal, gint pages)
{
	glong destination;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
#ifdef VTE_DEBUG
	fprintf(stderr, "Scrolling %d pages.\n", pages);
#endif
	/* Calculate the ideal position where we want to be before clamping. */
	destination = floor(gtk_adjustment_get_value(terminal->adjustment));
	destination += (pages * terminal->row_count);
	/* Can't scroll past data we have. */
	destination = MIN(destination,
			  terminal->adjustment->upper - terminal->row_count);
	/* Can't scroll up past zero. */
	destination = MAX(terminal->adjustment->lower, destination);
	/* Tell the scrollbar to adjust itself. */
	gtk_adjustment_set_value(terminal->adjustment, destination);
	gtk_adjustment_changed(terminal->adjustment);
}

/* Scroll so that the scroll delta is the insertion delta. */
static void
vte_terminal_scroll_to_bottom(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (floor(gtk_adjustment_get_value(terminal->adjustment)) !=
	    terminal->pvt->screen->insert_delta) {
		gtk_adjustment_set_value(terminal->adjustment,
					 terminal->pvt->screen->insert_delta);
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
	while (vte_ring_next(terminal->pvt->screen->row_data) < position) {
		array = vte_new_row_data();
		vte_ring_append(terminal->pvt->screen->row_data, array);
	}
	/* If we haven't inserted a line yet, insert a new one. */
	array = vte_new_row_data();
	if (vte_ring_next(terminal->pvt->screen->row_data) >= position) {
		vte_ring_insert(terminal->pvt->screen->row_data, position, array);
	} else {
		vte_ring_append(terminal->pvt->screen->row_data, array);
	}
}

/* Remove a line at an arbitrary position. */
static void
vte_remove_line_int(VteTerminal *terminal, long position)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (vte_ring_next(terminal->pvt->screen->row_data) > position) {
		vte_ring_remove(terminal->pvt->screen->row_data, position, TRUE);
	}
}

/* Change the encoding used for the terminal to the given codeset, or the
 * locale default if NULL is passed in. */
static void
vte_terminal_set_encoding(VteTerminal *terminal, const char *codeset)
{
	const char *old_codeset;
	GQuark encoding_quark;
	iconv_t conv;
	char *ibuf, *obuf, *obufptr;
	size_t icount, ocount;

	old_codeset = terminal->pvt->encoding;

	if (codeset == NULL) {
		codeset = nl_langinfo(CODESET);
	}

	/* Set up the conversion for incoming-to-wchars. */
	if (terminal->pvt->incoming_conv != NULL) {
		iconv_close(terminal->pvt->incoming_conv);
	}
	terminal->pvt->incoming_conv = iconv_open("WCHAR_T", codeset);

	/* Set up the conversions for wchar/utf-8 to outgoing. */
	if (terminal->pvt->outgoing_conv_wide != NULL) {
		iconv_close(terminal->pvt->outgoing_conv_wide);
	}
	terminal->pvt->outgoing_conv_wide = iconv_open(codeset, "WCHAR_T");

	if (terminal->pvt->outgoing_conv_utf8 != NULL) {
		iconv_close(terminal->pvt->outgoing_conv_utf8);
	}
	terminal->pvt->outgoing_conv_utf8 = iconv_open(codeset, "UTF-8");

	/* Set the terminal's encoding to the new value. */
	encoding_quark = g_quark_from_string(codeset);
	terminal->pvt->encoding = g_quark_to_string(encoding_quark);

	/* Convert any buffered output bytes. */
	if (terminal->pvt->n_outgoing > 0) {
		icount = terminal->pvt->n_outgoing;
		ibuf = terminal->pvt->outgoing;
		ocount = icount * VTE_UTF8_BPC + 1;
		obuf = obufptr = g_malloc(ocount);
		conv = iconv_open(codeset, old_codeset);
		if (conv != NULL) {
			if (iconv(conv, &ibuf, &icount, &obuf, &ocount) == -1) {
				/* Darn, it failed.  Leave it alone. */
				g_free(obufptr);
#ifdef VTE_DEBUG
				fprintf(stderr, "Error converting %ld pending "
					"output bytes (%s) skipping.\n",
					(long) terminal->pvt->n_outgoing,
					strerror(errno));
#endif
			} else {
				g_free(terminal->pvt->outgoing);
				terminal->pvt->outgoing = obufptr;
				terminal->pvt->n_outgoing = obuf - obufptr;
#ifdef VTE_DEBUG
				fprintf(stderr, "Converted %ld pending "
					"output bytes.\n",
					(long) terminal->pvt->n_outgoing);
#endif
			}
			iconv_close(conv);
		}
	}

#ifdef VTE_DEBUG
	fprintf(stderr, "Set terminal encoding to `%s'.\n",
		terminal->pvt->encoding);
#endif
}

/* End alternate character set. */
static void
vte_sequence_handler_ae(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->defaults.alternate = 0;
}

/* Add a line at the current cursor position. */
static void
vte_sequence_handler_al(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	GtkWidget *widget;
	long start, end;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);
	screen = terminal->pvt->screen;
	if (screen->scrolling_restricted) {
		start = screen->insert_delta + screen->scrolling_region.start;
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		start = screen->insert_delta;
		end = screen->insert_delta + terminal->row_count - 1;
	}
	vte_remove_line_int(terminal, end);
	vte_insert_line_int(terminal, screen->cursor_current.row);
	screen->cursor_current.row++;
	if (start - screen->insert_delta < terminal->row_count / 2) {
#if 0
		gdk_window_scroll(widget->window,
				  0,
				  terminal->char_height);
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     0, start + 2);
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     end, terminal->row_count);
#else
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     start, end - start + 1);
#endif
	} else {
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     start, end - start + 1);
	}
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

/* Start using alternate character set. */
static void
vte_sequence_handler_as(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->defaults.alternate = 1;
}

/* Beep. */
static void
vte_sequence_handler_bl(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	Display *display;
	GdkDrawable *gdrawable;
	Drawable drawable;
	GC gc;
	gint x_offs, y_offs;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	if (!(terminal->pvt->audible_bell) &&
	    GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
		gdk_window_get_internal_paint_info(GTK_WIDGET(terminal)->window,
						   &gdrawable,
						   &x_offs,
						   &y_offs);
		display = gdk_x11_drawable_get_xdisplay(gdrawable);
		drawable = gdk_x11_drawable_get_xid(gdrawable);
		gc = XCreateGC(display, drawable, 0, NULL);

		XSetForeground(display, gc,
			       terminal->pvt->palette[VTE_DEF_FG].pixel);
		XFillRectangle(display, drawable, gc,
			       x_offs, y_offs,
			       terminal->column_count * terminal->char_width,
			       terminal->row_count * terminal->char_height);
		gdk_window_process_all_updates();

		vte_invalidate_cells(terminal,
				     0,
				     terminal->column_count,
				     terminal->pvt->screen->scroll_delta,
				     terminal->row_count);
		gdk_window_process_all_updates();
	} else {
		gdk_beep();
	}
}

/* Clear from the cursor position to the beginning of the line. */
static void
vte_sequence_handler_cb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GArray *rowdata;
	long i;
	VteScreen *screen;
	struct vte_charcell *pcell;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	if (vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = vte_ring_index(screen->row_data,
					 GArray*,
					 screen->cursor_current.row);
		/* Clear the data up to the current column. */
		for (i = 0;
		     (i < screen->cursor_current.col) && (i < rowdata->len);
		     i++) {
			pcell = &g_array_index(rowdata, struct vte_charcell, i);
			if (pcell != NULL) {
				memset(pcell, sizeof(*pcell), 0);
				pcell->fore = VTE_DEF_FG;
				pcell->back = VTE_DEF_BG;
				pcell->c = ' ';
				pcell->columns = wcwidth(pcell->c);
			}
		}
		/* Repaint this row. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     screen->cursor_current.row, 1);
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
	long i;
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear data in the rows
	 * below the cursor. */
	for (i = screen->cursor_current.row + 1;
	     i < vte_ring_next(screen->row_data);
	     i++) {
		/* Get the data for the row we're removing. */
		rowdata = vte_ring_index(screen->row_data, GArray*, i);
		/* Remove it. */
		while ((rowdata != NULL) && (rowdata->len > 0)) {
			g_array_remove_index(rowdata, rowdata->len - 1);
		}
		/* Repaint this row. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     i, 1);
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
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	if (vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = vte_ring_index(screen->row_data, GArray*,
					 screen->cursor_current.row);
		/* Remove the data at the end of the array. */
		while (rowdata->len > screen->cursor_current.col) {
			g_array_remove_index(rowdata, rowdata->len - 1);
		}
		/* Repaint this row. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     screen->cursor_current.row, 1);
	}
}

/* Move the cursor to the given column (horizontal position). */
static void
vte_sequence_handler_ch(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	GValue *value;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* We only care if there's a parameter in there. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			/* Move the cursor. */
			screen->cursor_current.col = g_value_get_long(value);
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
	long rowval, colval;
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* We need at least two parameters. */
	if ((params != NULL) && (params->n_values >= 2)) {
		/* The first is the row, the second is the column. */
		row = g_value_array_get_nth(params, 0);
		col = g_value_array_get_nth(params, 1);
		if (G_VALUE_HOLDS_LONG(row) &&
		    G_VALUE_HOLDS_LONG(col)) {
			rowval = g_value_get_long(row);
			colval = g_value_get_long(col);
			rowval = MAX(0, rowval);
			rowval = MIN(rowval, terminal->row_count - 1);
			colval = MAX(0, colval);
			colval = MIN(colval, terminal->column_count - 1);
			screen->cursor_current.row = rowval +
						     screen->insert_delta;
			screen->cursor_current.col = colval;
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
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	if (vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = vte_ring_index(screen->row_data, GArray*,
					 screen->cursor_current.row);
		/* Remove it. */
		while (rowdata->len > 0) {
			g_array_remove_index(rowdata, rowdata->len - 1);
		}
		/* Repaint this row. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     screen->cursor_current.row, 1);
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
	terminal->pvt->screen->cursor_current.col = 0;
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
		terminal->pvt->screen->scrolling_restricted = FALSE;
		return;
	}
	/* Extract the two values. */
	value = g_value_array_get_nth(params, 0);
	start = g_value_get_long(value);
	value = g_value_array_get_nth(params, 1);
	end = g_value_get_long(value);
	/* Set the right values. */
	terminal->pvt->screen->scrolling_region.start = start;
	terminal->pvt->screen->scrolling_region.end = end;
	terminal->pvt->screen->scrolling_restricted = TRUE;
	/* Special case -- run wild, run free. */
	rows = terminal->row_count;
	if ((terminal->pvt->screen->scrolling_region.start == 0) &&
	    (terminal->pvt->screen->scrolling_region.end == rows - 1)) {
		terminal->pvt->screen->scrolling_restricted = FALSE;
	}
}

/* Move the cursor to the given row (vertical position). */
static void
vte_sequence_handler_cv(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	GValue *value;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* We only care if there's a parameter in there. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			/* Move the cursor. */
			screen->cursor_current.row = g_value_get_long(value);
		}
	}
}

/* Delete a character at the current cursor position. */
static void
vte_sequence_handler_dc(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	GArray *rowdata;
	long col;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;

	if (vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = vte_ring_index(screen->row_data,
					 GArray*,
					 screen->cursor_current.row);
		col = screen->cursor_current.col;
		/* Remove the column. */
		if (col < rowdata->len) {
			g_array_remove_index(rowdata, col);
		}
		/* Repaint this row. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     screen->cursor_current.row, 1);
	}
}

/* Delete N characters at the current cursor position. */
static void
vte_sequence_handler_DC(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_sequence_handler_multiple(terminal, match, match_quark, params,
				      vte_sequence_handler_dc);
}

/* Delete a line at the current cursor position. */
static void
vte_sequence_handler_dl(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	long end;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	if (screen->scrolling_restricted) {
		end = screen->insert_delta + screen->scrolling_region.end;
	} else {
		end = screen->insert_delta + terminal->row_count - 1;
	}
	vte_remove_line_int(terminal, screen->cursor_current.row);
	vte_insert_line_int(terminal, end);
	/* Repaint the entire screen. */
	vte_invalidate_cells(terminal,
			     0,
			     terminal->column_count,
			     terminal->pvt->screen->insert_delta,
			     terminal->row_count);
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

/* Make sure we have enough rows and columns to hold data at the current
 * cursor position. */
static void
vte_terminal_ensure_cursor(VteTerminal *terminal)
{
	GArray *array;
	VteScreen *screen;
	struct vte_charcell cell;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	screen = terminal->pvt->screen;

	while (screen->cursor_current.row >= vte_ring_next(screen->row_data)) {
		array = vte_new_row_data();
		vte_ring_append(screen->row_data, array);
	}

	array = vte_ring_index(screen->row_data,
			       GArray*,
			       screen->cursor_current.row);

	if (array != NULL) {
		/* Add enough characters to fill out the row. */
		memset(&cell, 0, sizeof(cell));
		cell.fore = VTE_DEF_FG;
		cell.back = VTE_DEF_BG;
		cell.c = ' ';
		cell.columns = wcwidth(cell.c);
		while (array->len < screen->cursor_current.col) {
			array = g_array_append_val(array, cell);
		}
		/* Add one more cell to the end of the line to get
		 * one for the column. */
		array = g_array_append_val(array, cell);
	}
}

static void
vte_terminal_scroll_insertion(VteTerminal *terminal)
{
	long rows, delta;
	VteScreen *screen;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;

	/* Make sure that the bottom row is visible, and that it's in
	 * the buffer (even if it's empty).  This usually causes the
	 * top row to become a history-only row. */
	rows = MAX(vte_ring_next(screen->row_data),
		   screen->cursor_current.row + 1);
	delta = MAX(0, rows - terminal->row_count);

	/* Adjust the insert delta and scroll if needed. */
	if (delta != screen->insert_delta) {
		vte_terminal_ensure_cursor(terminal);
		screen->insert_delta = delta;
		/* Update scroll bar adjustments. */
		vte_terminal_adjust_adjustments(terminal);
	}
}

/* Scroll forward. */
static void
vte_sequence_handler_do(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GtkWidget *widget;
	long col, row, start, end;
	VteScreen *screen;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);
	screen = terminal->pvt->screen;

	start = screen->scrolling_region.start + screen->insert_delta;
	end = screen->scrolling_region.end + screen->insert_delta;
	col = screen->cursor_current.col;
	row = screen->cursor_current.row;

	if (screen->scrolling_restricted) {
		if (row == end) {
			/* If we're at the end of the scrolling region, add a
			 * line at the bottom to scroll the top off. */
			vte_remove_line_int(terminal, start);
			vte_insert_line_int(terminal, end);
			if ((terminal->pvt->bg_image == NULL) &&
			    (!terminal->pvt->bg_transparent)) {
#if 0
				/* Scroll the window. */
				gdk_window_scroll(widget->window,
						  0,
						  -terminal->char_height);
				/* We need to redraw the last row of the
				 * scrolling region, and anything beyond. */
				vte_invalidate_cells(terminal,
						     0, terminal->column_count,
						     end, terminal->row_count);
				/* Also redraw anything above the scrolling
				 * region. */
				vte_invalidate_cells(terminal,
						     0, terminal->column_count,
						     0, start);
#else
				vte_invalidate_cells(terminal,
						     0,
						     terminal->column_count,
						     start,
						     end - start + 1);
#endif
			} else {
				/* If we have a background image, we need to
				 * redraw the entire window. */
				vte_invalidate_cells(terminal,
						     0,
						     terminal->column_count,
						     start,
						     end - start + 1);
			}
		} else {
			/* Otherwise, just move the cursor down. */
			screen->cursor_current.row++;
		}
	} else {
		/* Move the cursor down. */
		screen->cursor_current.row++;

		/* Adjust the insert delta so that the row the cursor is on
		 * is viewable if the insert delta is equal to the scrolling
		 * delta. */
		vte_terminal_scroll_insertion(terminal);
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

/* Start using alternate character set. */
static void
vte_sequence_handler_eA(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_sequence_handler_ae(terminal, match, match_quark, params);
}

/* Erase characters starting at the cursor position (overwriting N with
 * spaces, but not moving the cursor). */
static void
vte_sequence_handler_ec(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	GArray *rowdata;
	GValue *value;
	struct vte_charcell *cell;
	long col, i, count;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
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
	if (vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = vte_ring_index(screen->row_data,
					 GArray*,
					 screen->cursor_current.row);
		/* Write over the same characters. */
		for (i = 0; i < count; i++) {
			col = screen->cursor_current.col + i;
			if ((col < rowdata->len) && (col >= 0)) {
				cell = &g_array_index(rowdata,
						      struct vte_charcell,
						      col);
				memset(cell, sizeof(*cell), 0);
				cell->fore = VTE_DEF_FG;
				cell->back = VTE_DEF_BG;
				cell->c = ' ';
				cell->columns = wcwidth(cell->c);
			}
		}
		/* Repaint this row. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     screen->cursor_current.row, 1);
	}
}

/* End insert mode. */
static void
vte_sequence_handler_ei(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->insert = FALSE;
}

/* Move the cursor to the home position. */
static void
vte_sequence_handler_ho(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	screen->cursor_current.row = screen->insert_delta;
	screen->cursor_current.col = 0;
}

/* Insert a character. */
static void
vte_sequence_handler_ic(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	long row, col;
	VteScreen *screen;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;

	row = screen->cursor_current.row;
	col = screen->cursor_current.col;

	vte_terminal_insert_char(GTK_WIDGET(terminal), ' ');

	screen->cursor_current.row = row;
	screen->cursor_current.col = col;
}

/* Insert N characters. */
static void
vte_sequence_handler_IC(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, match, match_quark, params,
				      vte_sequence_handler_ic);
}

/* Begin insert mode. */
static void
vte_sequence_handler_im(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->insert = TRUE;
}

/* Send me a backspace key sym, will you?  Guess that the application meant
 * to send the cursor back one position. */
static void
vte_sequence_handler_kb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	screen->cursor_current.col = MAX(0, screen->cursor_current.col - 1);
}

/* Keypad mode end. */
static void
vte_sequence_handler_ke(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->keypad = VTE_KEYPAD_NORMAL;
}

/* Keypad mode start. */
static void
vte_sequence_handler_ks(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->keypad = VTE_KEYPAD_APPLICATION;
}

/* Cursor left. */
static void
vte_sequence_handler_le(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_kb(terminal, match, match_quark, params);
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
	terminal->pvt->screen->defaults.blink = 1;
}

/* Bold on. */
static void
vte_sequence_handler_md(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->defaults.bold = 1;
}

/* End modes. */
static void
vte_sequence_handler_me(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_terminal_set_default_attributes(terminal);
}

/* Invisible on. */
static void
vte_sequence_handler_mk(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->defaults.invisible = 1;
}

/* Reverse on. */
static void
vte_sequence_handler_mr(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->defaults.reverse = 1;
}

/* Cursor right. */
static void
vte_sequence_handler_nd(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	screen->cursor_current.col++;
}

/* Restore cursor (position). */
static void
vte_sequence_handler_rc(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	screen->cursor_current.col = screen->cursor_saved.col;
	screen->cursor_current.row = screen->cursor_saved.row +
				     screen->insert_delta;
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
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	screen->cursor_saved.col = screen->cursor_current.col;
	screen->cursor_saved.row = screen->cursor_current.row -
				   screen->insert_delta;
}

/* Standout end. */
static void
vte_sequence_handler_se(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->defaults.standout = 0;
}

/* Standout start. */
static void
vte_sequence_handler_so(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->defaults.standout = 1;
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
	/* Calculate which column is the next tab stop. */
	newcol = terminal->pvt->screen->cursor_current.col;
	do {
		newcol++;
	} while ((newcol % VTE_TAB_WIDTH) != 0);
	/* Wrap to the next line if need be.  FIXME: check if we're supposed
	 * to wrap to the next line. */
	if (newcol >= terminal->column_count) {
		terminal->pvt->screen->cursor_current.col = 0;
		vte_sequence_handler_do(terminal, match, match_quark, params);
	} else {
		terminal->pvt->screen->cursor_current.col = newcol;
	}
}

/* Terminal usage ends. */
static void
vte_sequence_handler_te(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* I think this is a no-op. */
}

/* Terminal usage starts. */
static void
vte_sequence_handler_ts(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* I think this is a no-op. */
}

/* Underline end. */
static void
vte_sequence_handler_ue(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->defaults.underline = 0;
}

/* Cursor up, scrolling if need be. */
static void
vte_sequence_handler_up(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GtkWidget *widget;
	long col, row, start, end;
	VteScreen *screen;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);
	screen = terminal->pvt->screen;

	col = screen->cursor_current.col;
	row = screen->cursor_current.row;

	if (screen->scrolling_restricted) {
		start = screen->scrolling_region.start + screen->insert_delta;
		end = screen->scrolling_region.end + screen->insert_delta;
		if (row == start) {
			/* If we're at the top of the scrolling region, add a
			 * line at the top to scroll the bottom off. */
			vte_remove_line_int(terminal, end);
			vte_insert_line_int(terminal, start);
			if ((terminal->pvt->bg_image == NULL) &&
			    (!terminal->pvt->bg_transparent)) {
#if 0
				/* Scroll the window. */
				gdk_window_scroll(widget->window,
						  0,
						  terminal->char_height);
				/* We need to redraw the first row of the
				 * scrolling region, and anything above. */
				vte_invalidate_cells(terminal,
						     0, terminal->column_count,
						     0, start + 1);
				/* Also redraw anything below the scrolling
				 * region. */
				vte_invalidate_cells(terminal,
						     0, terminal->column_count,
						     end, terminal->row_count);
#endif
				vte_invalidate_cells(terminal,
						     0, terminal->column_count,
						     start, end - start + 1);
			} else {
				/* If we have a background image, we need to
				 * redraw the entire window. */
				vte_invalidate_cells(terminal,
						     0, terminal->column_count,
						     start, end - start + 1);
			}
		} else {
			/* Otherwise, just move the cursor up. */
			screen->cursor_current.row--;
			row = screen->cursor_current.row;
		}
	} else {
		start = terminal->pvt->screen->insert_delta;
		end = start + terminal->row_count - 1;
		if (row == start) {
			/* Insert a blank line and remove one from the bottom,
			 * to simulate a proper scroll without screwing up the
			 * history. */
			vte_remove_line_int(terminal, end);
			vte_insert_line_int(terminal, start);
			if ((terminal->pvt->bg_image == NULL) &&
			    (!terminal->pvt->bg_transparent)) {
#if 0
				/* Scroll the window. */
				gdk_window_scroll(widget->window,
						  0,
						  terminal->char_height);
				/* We need to redraw the bottom row. */
				vte_invalidate_cells(terminal,
						     0,
						     terminal->column_count,
						     terminal->row_count - 1,
						     1);
#else
				vte_invalidate_cells(terminal,
						     0,
						     terminal->column_count,
						     screen->scroll_delta,
						     terminal->row_count);
#endif
			} else {
				/* If we have a background image, we need to
				 * redraw the entire window. */
				vte_invalidate_cells(terminal,
						     0,
						     terminal->column_count,
						     screen->scroll_delta,
						     terminal->row_count);
			}
		} else {
			/* Move the cursor up. */
			screen->cursor_current.row--;
			row = screen->cursor_current.row;
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
	terminal->pvt->screen->defaults.underline = 1;
}

/* Cursor visible. */
static void
vte_sequence_handler_ve(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->cursor_visible = TRUE;
}

/* Cursor invisible. */
static void
vte_sequence_handler_vi(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->cursor_visible = FALSE;
}

/* Cursor standout. */
static void
vte_sequence_handler_vs(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->cursor_visible = TRUE; /* FIXME: should be
							 *more* visible. */
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
				terminal->pvt->screen->defaults.bold = 1;
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
			case 21: /* one of these is the linux console */
			case 22: /* one of these is ecma, i forget which */
				terminal->pvt->screen->defaults.bold = 0;
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
				/* default foreground, underscore */
				terminal->pvt->screen->defaults.fore = VTE_DEF_FG;
				terminal->pvt->screen->defaults.underline = 1;
				break;
			case 39:
				/* default foreground, no underscore */
				terminal->pvt->screen->defaults.fore = VTE_DEF_FG;
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
				terminal->pvt->screen->defaults.fore = param - 90;
				break;
			case 100:
			case 101:
			case 102:
			case 103:
			case 104:
			case 105:
			case 106:
			case 107:
				terminal->pvt->screen->defaults.back = param - 100;
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
	long i;
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	for (i = screen->insert_delta; i < screen->cursor_current.row; i++) {
		if (vte_ring_next(screen->row_data) > i) {
			/* Get the data for the row we're erasing. */
			rowdata = vte_ring_index(screen->row_data, GArray*, i);
			/* Remove it. */
			while (rowdata->len > 0) {
				g_array_remove_index(rowdata, rowdata->len - 1);
			}
			/* Repaint this row. */
			vte_invalidate_cells(terminal,
					     0, terminal->column_count,
					     i, 1);
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
	long i;
	VteScreen *screen;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	for (i = screen->insert_delta;
	     i < screen->insert_delta + terminal->row_count;
	     i++) {
		if (vte_ring_next(screen->row_data) > i) {
			/* Get the data for the row we're removing. */
			rowdata = vte_ring_index(screen->row_data, GArray*, i);
			/* Remove it. */
			while (rowdata->len > 0) {
				g_array_remove_index(rowdata, rowdata->len - 1);
			}
			/* Repaint this row. */
			vte_invalidate_cells(terminal,
					     0, terminal->column_count,
					     i, 1);
		}
	}
}

/* Move the cursor to the given column, 1-based. */
static void
vte_sequence_handler_cursor_character_absolute(VteTerminal *terminal,
					       const char *match,
					       GQuark match_quark,
					       GValueArray *params)
{
	vte_sequence_handler_offset(terminal, match, match_quark, params,
				    -1, vte_sequence_handler_ch);
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
vte_sequence_handler_set_title_int(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params,
				   const char *signal)
{
	GValue *value;
	iconv_t conv;
	char *inbuf = NULL, *outbuf = NULL, *outbufptr = NULL;
	size_t inbuf_len, outbuf_len;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* Get the string parameter's value. */
	value = g_value_array_get_nth(params, 0);
	if (value) {
		if (G_VALUE_HOLDS_LONG(value)) {
			/* Convert the long to a string. */
			outbufptr = g_strdup_printf("%ld",
						    g_value_get_long(value));
		} else
		if (G_VALUE_HOLDS_STRING(value)) {
			/* Copy the string into the buffer. */
			outbufptr = g_value_dup_string(value);
		} else
		if (G_VALUE_HOLDS_POINTER(value)) {
			/* Convert the wide-character string into a
			 * multibyte string. */
			conv = iconv_open("UTF-8", "WCHAR_T");
			inbuf = g_value_get_pointer(value);
			inbuf_len = wcslen((wchar_t*)inbuf) * sizeof(wchar_t);
			outbuf_len = (inbuf_len * VTE_UTF8_BPC) + 1;
			outbuf = outbufptr = g_malloc0(outbuf_len);
			if (iconv(conv, &inbuf, &inbuf_len,
				  &outbuf, &outbuf_len) == -1) {
#ifdef VTE_DEBUG
				fprintf(stderr, "Error converting %ld title "
					"bytes (%s), skipping.\n",
					(long) terminal->pvt->n_outgoing,
					strerror(errno));
#endif
				g_free(outbufptr);
				outbufptr = NULL;
			}
		}
		if (outbufptr != NULL) {
			/* Emit the signal */
			if (strcmp(signal, "window_title_changed") == 0) {
				g_free(terminal->window_title);
				terminal->window_title = outbufptr;
				vte_terminal_emit_window_title_changed(terminal);
			}
			else if (strcmp (signal, "icon_title_changed") == 0) {
				g_free (terminal->icon_title);
				terminal->icon_title = outbufptr;
				vte_terminal_emit_icon_title_changed(terminal);
			}
		}
	}
}

/* Set one or the other. */
static void
vte_sequence_handler_set_icon_title(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params)
{
	vte_sequence_handler_set_title_int(terminal, match, match_quark,
					   params, "icon_title_changed");
}
static void
vte_sequence_handler_set_window_title(VteTerminal *terminal,
				      const char *match,
				      GQuark match_quark,
				      GValueArray *params)
{
	vte_sequence_handler_set_title_int(terminal, match, match_quark,
					   params, "window_title_changed");
}

/* Set both the window and icon titles to the same string. */
static void
vte_sequence_handler_set_icon_and_window_title(VteTerminal *terminal,
						  const char *match,
						  GQuark match_quark,
						  GValueArray *params)
{
	vte_sequence_handler_set_title_int(terminal, match, match_quark,
					   params, "icon_title_changed");
	vte_sequence_handler_set_title_int(terminal, match, match_quark,
					   params, "window_title_changed");
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

/* Show or hide the pointer. */
static void
vte_terminal_set_pointer_visible(VteTerminal *terminal, gboolean visible)
{
	GdkCursor *cursor = NULL;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (visible || !terminal->pvt->mouse_autohide) {
		if (terminal->pvt->mouse_send_xy_on_click ||
		    terminal->pvt->mouse_send_xy_on_button ||
		    terminal->pvt->mouse_hilite_tracking ||
		    terminal->pvt->mouse_cell_motion_tracking ||
		    terminal->pvt->mouse_all_motion_tracking) {
#ifdef VTE_DEBUG
			fprintf(stderr, "Setting mousing cursor.\n");
#endif
			cursor = terminal->pvt->mouse_mousing_cursor;
		} else {
#ifdef VTE_DEBUG
			fprintf(stderr, "Setting default mouse cursor.\n");
#endif
			cursor = terminal->pvt->mouse_default_cursor;
		}
	} else {
#ifdef VTE_DEBUG
		fprintf(stderr, "Setting to invisible cursor.\n");
#endif
		cursor = terminal->pvt->mouse_inviso_cursor;
	}
	if (cursor) {
		if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
			gdk_window_set_cursor((GTK_WIDGET(terminal))->window,
					      cursor);
		}
	}
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
	GtkWidget *widget;
	long param;
	int i;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);
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
				terminal->pvt->keypad = set ?
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
				terminal->pvt->screen->defaults.reverse = set;
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
#ifdef VTE_DEBUG
				fprintf(stderr, "Setting send-coords-on-click "
					"to %s.\n", set ? "ON" : "OFF");
#endif
				terminal->pvt->mouse_send_xy_on_click = set;
				break;
			case 25:
				terminal->pvt->screen->cursor_visible = set;
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
				terminal->pvt->screen = set ?
							&terminal->pvt->alternate_screen :
							&terminal->pvt->normal_screen;
				/* Fixup the scrollbars. */
				vte_terminal_adjust_adjustments(terminal);
				/* Force the screen to be redrawn. */
				vte_invalidate_cells(terminal,
						     0,
						     terminal->column_count,
						     terminal->pvt->screen->scroll_delta,
						     terminal->row_count);
				break;
			case 1000:
#ifdef VTE_DEBUG
				fprintf(stderr, "Setting send-coords-on-button "
					"to %s.\n", set ? "ON" : "OFF");
#endif
				terminal->pvt->mouse_send_xy_on_button = set;
				break;
			case 1001:
#ifdef VTE_DEBUG
				fprintf(stderr, "Setting hilite-tracking "
					"to %s.\n", set ? "ON" : "OFF");
#endif
				terminal->pvt->mouse_hilite_tracking = set;
				break;
			case 1002:
#ifdef VTE_DEBUG
				fprintf(stderr, "Setting cell-tracking "
					"to %s.\n", set ? "ON" : "OFF");
#endif
				terminal->pvt->mouse_cell_motion_tracking = set;
				break;
			case 1003:
#ifdef VTE_DEBUG
				fprintf(stderr, "Setting all-tracking "
					"to %s.\n", set ? "ON" : "OFF");
#endif
				terminal->pvt->mouse_all_motion_tracking = set;
			default:
				break;
		}
		vte_terminal_set_pointer_visible(terminal, TRUE);
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
	terminal->pvt->keypad = VTE_KEYPAD_APPLICATION;
}

static void
vte_sequence_handler_normal_keypad(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->keypad = VTE_KEYPAD_NORMAL;
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
	VteScreen *screen;
	long param, end, row;
	int i;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* The default is one. */
	param = 1;
	/* Extract any parameters. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		param = g_value_get_long(value);
	}
	row = screen->cursor_current.row;
	end = screen->scrolling_region.end + screen->insert_delta;
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
			     row, end - row + 1);
}

/* Delete certain lines from the scrolling region. */
static void
vte_sequence_handler_delete_lines(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param, end, row;
	int i;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;
	/* The default is one. */
	param = 1;
	/* Extract any parameters. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		param = g_value_get_long(value);
	}
	/* Clear the lines which we need to clear. */
	row = screen->cursor_current.row;
	end = screen->insert_delta + screen->scrolling_region.end;
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
			     row, end - row + 1);
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
vte_sequence_handler_local_charset(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
#ifdef VTE_DEFAULT_ISO_8859_1
	vte_terminal_set_encoding(terminal, "ISO-8859-1");
#else
	if (strcmp(nl_langinfo(CODESET), "UTF-8") == 0) {
		vte_terminal_set_encoding(terminal, "ISO-8859-1");
	} else {
		vte_terminal_set_encoding(terminal, nl_langinfo(CODESET));
	}
#endif
}

static void
vte_sequence_handler_utf_8_charset(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_terminal_set_encoding(terminal, "UTF-8");
}

/* Device status reports. The possible reports are the cursor position and
 * whether or not we're okay. */
static void
vte_sequence_handler_device_status_report(VteTerminal *terminal,
					  const char *match,
					  GQuark match_quark,
					  GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param;
	char buf[LINE_MAX];

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		param = g_value_get_long(value);
		switch (param) {
			case 5:
				/* Send a thumbs-up sequence. */
				snprintf(buf, sizeof(buf),
					 "%s%dn", VTE_CAP_CSI, 0);
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			case 6:
				/* Send the cursor position. */
				snprintf(buf, sizeof(buf),
					 "%s%ld;%ldR", VTE_CAP_CSI,
					 screen->cursor_current.row + 1 -
					 screen->insert_delta,
					 screen->cursor_current.col + 1);
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			default:
				break;
		}
	}
}

/* DEC-style device status reports. */
static void
vte_sequence_handler_dec_device_status_report(VteTerminal *terminal,
					      const char *match,
					      GQuark match_quark,
					      GValueArray *params)
{
	GValue *value;
	VteScreen *screen;
	long param;
	char buf[LINE_MAX];

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	screen = terminal->pvt->screen;

	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		param = g_value_get_long(value);
		switch (param) {
			case 6:
				/* Send the cursor position. */
				snprintf(buf, sizeof(buf),
					 "%s?%ld;%ldR", VTE_CAP_CSI,
					 screen->cursor_current.row + 1 -
					 screen->insert_delta,
					 screen->cursor_current.col + 1);
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			case 15:
				/* Send printer status -- 10 = ready,
				 * 11 = not ready.  We don't print. */
				snprintf(buf, sizeof(buf),
					 "%s?%dn", VTE_CAP_CSI, 11);
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			case 25:
				/* Send UDK status -- 20 = locked,
				 * 21 = not locked.  I don't even know what
				 * that means, but punt anyway. */
				snprintf(buf, sizeof(buf),
					 "%s?%dn", VTE_CAP_CSI, 20);
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			case 26:
				/* Send keyboard status.  50 = no locator. */
				snprintf(buf, sizeof(buf),
					 "%s?%dn", VTE_CAP_CSI, 50);
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			default:
				break;
		}
	}
}

/* Window manipulation control sequences.  Most of these are considered
 * bad ideas, but they're implemented as signals which the application
 * is free to ignore, so they're harmless. */
static void
vte_sequence_handler_window_manipulation(VteTerminal *terminal,
					 const char *match,
					 GQuark match_quark,
					 GValueArray *params)
{
	VteScreen *screen;
	GValue *value;
	GtkWidget *widget;
	Display *display;
	char buf[LINE_MAX];
	long param, arg1, arg2;
	guint width, height;
	int i;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
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
				fprintf(stderr, "Deiconifying window.\n");
#endif
				vte_terminal_emit_deiconify_window(terminal);
				break;
			case 2:
#ifdef VTE_DEBUG
				fprintf(stderr, "Iconifying window.\n");
#endif
				vte_terminal_emit_iconify_window(terminal);
				break;
			case 3:
				if ((arg1 != -1) && (arg2 != -2)) {
#ifdef VTE_DEBUG
					fprintf(stderr, "Moving window to %ld,%ld.\n", arg1, arg2);
#endif
					vte_terminal_emit_move_window(terminal, arg1, arg2);
					i += 2;
				}
				break;
			case 4:
				if ((arg1 != -1) && (arg2 != -1)) {
#ifdef VTE_DEBUG
					fprintf(stderr, "Resizing window (%ldx%ld pixels).\n",
						arg2, arg1);
#endif
					vte_terminal_emit_resize_window(terminal,
									arg2,
									arg1);
					i += 2;
				}
				break;
			case 5:
#ifdef VTE_DEBUG
				fprintf(stderr, "Raising window.\n");
#endif
				vte_terminal_emit_raise_window(terminal);
				break;
			case 6:
#ifdef VTE_DEBUG
				fprintf(stderr, "Lowering window.\n");
#endif
				vte_terminal_emit_lower_window(terminal);
				break;
			case 7:
#ifdef VTE_DEBUG
				fprintf(stderr, "Refreshing window.\n");
#endif
				vte_invalidate_cells(terminal,
						     0, terminal->column_count,
						     0, terminal->row_count);
				vte_terminal_emit_refresh_window(terminal);
				break;
			case 8:
				if ((arg1 != -1) && (arg2 != -1)) {
#ifdef VTE_DEBUG
					fprintf(stderr, "Resizing window (%ld columns, %ld rows).\n",
						arg2, arg1);
#endif
					vte_terminal_emit_resize_window(terminal,
									arg2 * terminal->char_width,
									arg1 * terminal->char_height);
					i += 2;
				}
				break;
			case 9:
				switch (arg1) {
					case 0:
#ifdef VTE_DEBUG
						fprintf(stderr, "Restoring window.\n");
#endif
						vte_terminal_emit_restore_window(terminal);
						break;
					case 1:
#ifdef VTE_DEBUG
						fprintf(stderr, "Maximizing window.\n");
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
					 "%s%dt", VTE_CAP_CSI,
					 1 + !GTK_WIDGET_MAPPED(widget));
#ifdef VTE_DEBUG
				fprintf(stderr, "Reporting window state %s.\n",
					GTK_WIDGET_MAPPED(widget) ?
					"non-iconified" : "iconified");
#endif
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			case 13:
				/* Send window location, in pixels. */
				gdk_window_get_origin(widget->window,
						      &width, &height);
				snprintf(buf, sizeof(buf),
					 "%s%d;%dt", VTE_CAP_CSI,
					 width, height);
#ifdef VTE_DEBUG
				fprintf(stderr, "Reporting window location"
					"(%d,%d).\n",
					width, height);
#endif
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			case 14:
				/* Send window size, in pixels. */
				gdk_drawable_get_size(widget->window,
						      &width, &height);
				snprintf(buf, sizeof(buf),
					 "%s%d;%dt", VTE_CAP_CSI,
					 height, width);
#ifdef VTE_DEBUG
				fprintf(stderr, "Reporting window size "
					"(%dx%d).\n", width, height);
#endif
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			case 18:
				/* Send widget size, in cells. */
#ifdef VTE_DEBUG
				fprintf(stderr, "Reporting widget size.\n");
#endif
				snprintf(buf, sizeof(buf),
					 "%s%ld;%ldt", VTE_CAP_CSI,
					 terminal->row_count,
					 terminal->column_count);
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			case 19:
#ifdef VTE_DEBUG
				fprintf(stderr, "Reporting screen size.\n");
#endif
				display = gdk_x11_drawable_get_xdisplay(widget->window);
				i = gdk_x11_get_default_screen();
				snprintf(buf, sizeof(buf),
					 "%s%ld;%ldt", VTE_CAP_CSI,
					 DisplayHeight(display, i) /
					 terminal->char_height,
					 DisplayWidth(display, i) /
					 terminal->char_width);
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			case 20:
				/* Report the icon title. */
#ifdef VTE_DEBUG
				fprintf(stderr, "Reporting icon title.\n");
#endif
				snprintf(buf, sizeof(buf),
					 "%sL%s%s",
					 VTE_CAP_OSC,
					 terminal->icon_title,
					 VTE_CAP_ST);
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			case 21:
				/* Report the window title. */
#ifdef VTE_DEBUG
				fprintf(stderr, "Reporting window title.\n");
#endif
				snprintf(buf, sizeof(buf),
					 "%sL%s%s",
					 VTE_CAP_OSC,
					 terminal->window_title,
					 VTE_CAP_ST);
				vte_terminal_feed_child(terminal,
							buf, strlen(buf));
				break;
			default:
				if (param >= 24) {
#ifdef VTE_DEBUG
					fprintf(stderr, "Resizing to %ld rows.\n",
						param);
#endif
					/* Resize to the specified number of
					 * rows. */
					vte_terminal_emit_resize_window(terminal,
									terminal->column_count * terminal->char_width,
									param * terminal->char_height);
				}
				break;
		}
	}
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
	{"ae", vte_sequence_handler_ae},
	{"as", vte_sequence_handler_as},

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

	{"dc", vte_sequence_handler_dc},
	{"DC", vte_sequence_handler_DC},
	{"dl", vte_sequence_handler_dl},
	{"DL", vte_sequence_handler_DL},
	{"dm", NULL},
	{"do", vte_sequence_handler_do},
	{"DO", vte_sequence_handler_DO},
	{"ds", NULL},

	{"eA", vte_sequence_handler_eA},
	{"ec", vte_sequence_handler_ec},
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
	{"ic", vte_sequence_handler_ic},
	{"IC", vte_sequence_handler_IC},
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
	{"kb", vte_sequence_handler_kb},
	{"kB", NULL},
	{"kC", NULL},
	{"kd", NULL},
	{"kD", NULL},
	{"ke", vte_sequence_handler_ke},
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
	{"ks", vte_sequence_handler_ks},
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

	{"nd", vte_sequence_handler_nd},
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
	{"te", vte_sequence_handler_te},
	{"ti", NULL},
	{"ts", vte_sequence_handler_ts},

	{"uc", NULL},
	{"ue", vte_sequence_handler_ue},
	{"up", vte_sequence_handler_up},
	{"UP", vte_sequence_handler_UP},
	{"us", vte_sequence_handler_us},

	{"vb", NULL},
	{"ve", vte_sequence_handler_ve},
	{"vi", vte_sequence_handler_vi},
	{"vs", vte_sequence_handler_vs},

	{"wi", NULL},

	{"XF", NULL},

	{"character-attributes", vte_sequence_handler_character_attributes},

	{"cursor-backward", vte_sequence_handler_le},
	{"cursor-forward", vte_sequence_handler_RI},
	{"cursor-up", vte_sequence_handler_UP},
	{"cursor-down", vte_sequence_handler_DO},
	{"cursor-character-absolute", vte_sequence_handler_cursor_character_absolute},
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
	{"iso8859-1-character-set", vte_sequence_handler_local_charset},
	{"utf-8-character-set", vte_sequence_handler_utf_8_charset},
	{"character-position-absolute", vte_sequence_handler_character_position_absolute},
	{"line-position-absolute", vte_sequence_handler_line_position_absolute},
	{"device-status-report", vte_sequence_handler_device_status_report},
	{"dec-device-status-report", vte_sequence_handler_dec_device_status_report},
	{"window-manipulation", vte_sequence_handler_window_manipulation},
};

/* Create the basic widget.  This more or less creates and initializes a
 * GtkWidget and clears out the rest of the data which is specific to our
 * widget class. */
GtkWidget *
vte_terminal_new(void)
{
	return GTK_WIDGET(g_object_new(vte_terminal_get_type(), NULL));
}

/* Set a given set of colors as the palette. */
void
vte_terminal_set_colors(VteTerminal *terminal,
			const GdkColor *foreground,
			const GdkColor *background,
			const GdkColor *palette,
			size_t palette_size)
{
	int i;
	GdkColor color;
	GtkWidget *widget;
	Display *display;
	GdkColormap *gcolormap;
	Colormap colormap;
	GdkVisual *gvisual;
	Visual *visual;
	const GdkColor *proposed;
	int bright;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(palette_size >= 8);
	g_return_if_fail((palette_size == 8) || (palette_size == 16));
	if (foreground == NULL) {
		foreground = &palette[7];
	}
	if (background == NULL) {
		background = &palette[0];
	}

	memset(&color, 0, sizeof(color));

	widget = NULL;
	display = NULL;
	gcolormap = NULL;
	colormap = 0;
	gvisual = NULL;
	visual = NULL;

	/* Initialize each item in the palette. */
	for (i = 0; i < G_N_ELEMENTS(terminal->pvt->palette); i++) {
		if (i == VTE_DEF_FG) {
			proposed = foreground;
		} else
		if (i == VTE_DEF_BG) {
			proposed = background;
		} else {
			proposed = &palette[i % palette_size];
		}

		/* Get X11 attributes used by GDK for the widget. */
		if (widget == NULL) {
			widget = GTK_WIDGET(terminal);
			display = GDK_DISPLAY();
			gcolormap = gtk_widget_get_colormap(widget);
			colormap = gdk_x11_colormap_get_xcolormap(gcolormap);
			gvisual = gtk_widget_get_visual(widget);
			visual = gdk_x11_visual_get_xvisual(gvisual);
		}

		/* Allocate a color from the colormap. */
		color = *proposed;

		/* If we're guessing about the second half, check how much
		 * brighter we could make this entry. */
		if ((i != VTE_DEF_FG) &&
		    (i != VTE_DEF_BG) &&
		    (i >= palette_size)) {
			bright = 0xffff;
			bright = MIN(bright, 0xffff - color.red);
			bright = MIN(bright, 0xffff - color.green);
			bright = MIN(bright, 0xffff - color.blue);
			bright = MIN(bright, 0xc000);
			color.red += bright;
			color.green += bright;
			color.blue += bright;
		}

		/* Get an Xlib color. */
		gdk_rgb_find_color(gcolormap, &color); /* fill in pixel */
		terminal->pvt->palette[i].red = color.red;
		terminal->pvt->palette[i].green = color.green;
		terminal->pvt->palette[i].blue = color.blue;
		terminal->pvt->palette[i].pixel = color.pixel;

#ifdef HAVE_XFT
		if (terminal->pvt->use_xft) {
			/* Get an Xft color. */
			terminal->pvt->palette[i].rcolor.red = color.red;
			terminal->pvt->palette[i].rcolor.green = color.green;
			terminal->pvt->palette[i].rcolor.blue = color.blue;
			terminal->pvt->palette[i].rcolor.alpha = 0xffff;

			/* FIXME this should probably use a color from the
			 * color cube. */
			if (!XftColorAllocValue(display,
						visual,
						colormap,
						&terminal->pvt->palette[i].rcolor,
						&terminal->pvt->palette[i].ftcolor)) {
				terminal->pvt->use_xft = FALSE;
			}
		}
#endif
	}

	/* This may have changed the default background color, so trigger
	 * a repaint. */
	vte_invalidate_cells(terminal,
			     0,
			     terminal->column_count,
			     terminal->pvt->screen->scroll_delta,
			     terminal->row_count);
}

/* Reset palette defaults for character colors. */
void
vte_terminal_set_default_colors(VteTerminal *terminal)
{
	GdkColor colors[8], fg, bg;
	int i;
	guint16 red, green, blue;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	/* Generate a default palette. */
	for (i = 0; i < G_N_ELEMENTS(colors); i++) {
		blue = (i & 4) ? 0xc000 : 0;
		green = (i & 2) ? 0xc000 : 0;
		red = (i & 1) ? 0xc000 : 0;
		colors[i].blue = blue;
		colors[i].green = green;
		colors[i].red = red;
	}

	/* Set the default background to look the same as color 7, and the
	 * default background to look like color 0. */
	fg = colors[7];
	bg = colors[0];

	vte_terminal_set_colors(terminal, &fg, &bg,
				colors, G_N_ELEMENTS(colors));
}

/* Insert a single character into the stored data array. */
static void
vte_terminal_insert_char(GtkWidget *widget, wchar_t c)
{
	VteTerminal *terminal;
	GArray *array;
	struct vte_charcell cell, *pcell;
	int columns, i;
	long col;
	VteScreen *screen;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);
	screen = terminal->pvt->screen;

#ifdef VTE_DEBUG
#ifdef VTE_DEBUG_INSERT
	fprintf(stderr, "Inserting %ld, delta = %ld.\n", (long)c,
		(long)screen->insert_delta);
#endif
#endif

	/* Figure out how many columns this character should occupy. */
	columns = wcwidth(c);

	/* FIXME: find why this can happen, and stop it. */
	if (columns < 0) {
		g_warning("Character %5ld is %d columns wide, guessing 1.\n",
			  c, columns);
		columns = 1;
	}

	/* If we're autowrapping here, do it. */
	col = terminal->pvt->screen->cursor_current.col;
	if ((col >= terminal->column_count) && terminal->pvt->flags.am) {
		terminal->pvt->screen->cursor_current.col = 0;
		terminal->pvt->screen->cursor_current.row++;
	}

	/* Make sure we have enough rows to hold this data. */
	vte_terminal_ensure_cursor(terminal);

	/* Get a handle on the array for the insertion row. */
	array = vte_ring_index(screen->row_data,
			       GArray*,
			       screen->cursor_current.row);

	/* Read the deltas. */
	for (i = 0; i < columns; i++) {
		col = terminal->pvt->screen->cursor_current.col;

		/* Make sure we have enough columns in this row. */
		if (array->len <= col) {
			/* Add enough characters to fill out the row. */
			memset(&cell, 0, sizeof(cell));
			cell.fore = VTE_DEF_FG;
			cell.back = VTE_DEF_BG;
			cell.c = ' ';
			cell.columns = wcwidth(cell.c);
			while (array->len < col) {
				array = g_array_append_val(array, cell);
			}
			/* Add one more cell to the end of the line to get
			 * it into the column, and use it. */
			array = g_array_append_val(array, cell);
			pcell = &g_array_index(array,
					       struct vte_charcell,
					       col);
		} else {
			/* If we're in insert mode, insert a new cell here
			 * and use it. */
			if (screen->insert) {
				memset(&cell, 0, sizeof(cell));
				cell.fore = VTE_DEF_FG;
				cell.back = VTE_DEF_BG;
				cell.c = ' ';
				cell.columns = wcwidth(cell.c);
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
		pcell->fore = terminal->pvt->screen->defaults.fore;
		pcell->back = terminal->pvt->screen->defaults.back;
		pcell->reverse = terminal->pvt->screen->defaults.reverse;
		pcell->invisible = terminal->pvt->screen->defaults.invisible;
		pcell->half = terminal->pvt->screen->defaults.half;
		pcell->underline = terminal->pvt->screen->defaults.underline;
		pcell->bold = terminal->pvt->screen->defaults.bold;
		pcell->standout = terminal->pvt->screen->defaults.standout;
		pcell->alternate = terminal->pvt->screen->defaults.alternate;

		/* Signal that this part of the window needs drawing. */
		if (terminal->pvt->screen->insert) {
			vte_invalidate_cells(terminal,
					     col - 1,
					     terminal->column_count - col + 1,
					     screen->cursor_current.row,
					     2);
		} else {
			vte_invalidate_cells(terminal,
					     col - 1, 3,
					     screen->cursor_current.row, 2);
		}

		/* And take a step to the to the right, making sure we redraw
		 * both where the cursor was moved from. */
		vte_invalidate_cursor_once(terminal);
		screen->cursor_current.col++;
	}

	/* Redraw where the cursor has moved to. */
	vte_invalidate_cursor_once(terminal);

#ifdef VTE_DEBUG
#ifdef VTE_DEBUG_INSERT
	fprintf(stderr, "Insertion delta = %ld.\n", (long)screen->insert_delta);
#endif
#endif
}

#ifdef VTE_DEBUG
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
#endif

/* Handle a terminal control sequence and its parameters. */
static void
vte_terminal_handle_sequence(GtkWidget *widget,
			     const char *match_s,
			     GQuark match,
			     GValueArray *params)
{
	VteTerminal *terminal;
	VteTerminalSequenceHandler handler;
	VteScreen *screen;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);
	screen = terminal->pvt->screen;

	/* This may generate multiple redraws, so freeze it while we do them. */
	gdk_window_freeze_updates(widget->window);

	/* Signal that the cursor's current position needs redrawing. */
	vte_invalidate_cells(terminal,
			     screen->cursor_current.col - 1, 3,
			     screen->cursor_current.row, 1);

	/* Find the handler for this control sequence. */
	handler = g_tree_lookup(terminal->pvt->sequences, GINT_TO_POINTER(match));
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
			     screen->cursor_current.row, 1);

	/* Let the updating begin. */
	gdk_window_thaw_updates(widget->window);
}

/* Start up a command in a slave PTY. */
pid_t
vte_terminal_fork_command(VteTerminal *terminal, const char *command,
			  const char **argv)
{
	const char **env_add;
	char *term, *colorterm;
	int i;
	pid_t pid;

	/* Start up the command and get the PTY of the master. */
	env_add = g_malloc0(sizeof(char*) * 3);
	term = g_strdup_printf("TERM=%s", terminal->pvt->terminal);
	colorterm = g_strdup("COLORTERM=" PACKAGE);
	env_add[0] = term;
	env_add[1] = colorterm;
	env_add[2] = NULL;
	terminal->pvt->pty_master = vte_pty_open(&pid,
						 env_add,
						 command ?:
						 terminal->pvt->shell,
						 argv);
	g_free(term);
	g_free(colorterm);
	g_free((char**)env_add);

	/* If we started the process, set up to listen for its output. */
	if (pid != -1) {
		/* Set this as the child's pid. */
		terminal->pvt->pty_pid = pid;

		/* Set the pty to be non-blocking. */
		i = fcntl(terminal->pvt->pty_master, F_GETFL);
		fcntl(terminal->pvt->pty_master, F_SETFL, i | O_NONBLOCK);

		/* Open a channel to listen for input on. */
		terminal->pvt->pty_input =
			g_io_channel_unix_new(terminal->pvt->pty_master);
		terminal->pvt->pty_output = NULL;
		g_io_add_watch_full(terminal->pvt->pty_input,
				    G_PRIORITY_LOW,
				    G_IO_IN | G_IO_HUP,
				    vte_terminal_io_read,
				    terminal,
				    NULL);
		g_io_channel_unref(terminal->pvt->pty_input);
	}

	/* Return the pid to the caller. */
	return pid;
}

/* Handle an EOF from the client. */
static void
vte_terminal_eof(GIOChannel *channel, gpointer data)
{
	VteTerminal *terminal;

	g_return_if_fail(VTE_IS_TERMINAL(data));
	terminal = VTE_TERMINAL(data);

	/* Close the connections to the child -- note that the source channel
	 * has already been dereferenced. */
	if (channel == terminal->pvt->pty_input) {
		terminal->pvt->pty_input = NULL;
	}

	/* Emit a signal that we read an EOF. */
	vte_terminal_emit_eof(terminal);
}

/* Reset the input method context. */
static void
vte_terminal_im_reset(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
		gtk_im_context_reset(terminal->pvt->im_context);
		if (terminal->pvt->im_preedit != NULL) {
			g_free(terminal->pvt->im_preedit);
			terminal->pvt->im_preedit = NULL;
		}
	}
}

/* Free a parameter array.  Most of the GValue elements can clean up after
 * themselves, but we're using gpointers to hold wide character strings, and
 * we need to free those ourselves. */
static void
free_params_array(GValueArray *params)
{
	int i;
	GValue *value;
	gpointer ptr;
	if (params != NULL) {
		for (i = 0; i < params->n_values; i++) {
			value = g_value_array_get_nth(params, i);
			if (G_VALUE_HOLDS_POINTER(value)) {
				ptr = g_value_get_pointer(value);
				if (ptr != NULL) {
					g_free(ptr);
				}
				g_value_set_pointer(value, NULL);
			}
		}
		g_value_array_free(params);
	}
}

/* Process incoming data, first converting it to wide characters, and then
 * processing escape sequences. */
static gboolean
vte_terminal_process_incoming(gpointer data)
{
	GValueArray *params = NULL;
	VteTerminal *terminal;
	VteScreen *screen;
	long cursor_row, cursor_col;
	GtkWidget *widget;
	GdkRectangle rect;
	char *ibuf, *obuf, *obufptr, *ubuf, *ubufptr;
	size_t icount, ocount, ucount;
	wchar_t *wbuf, c;
	int wcount, start, end, i;
	const char *match, *encoding;
	iconv_t unconv;
	GQuark quark;
	const wchar_t *next;
	gboolean leftovers, inserted, again, bottom;

	g_return_val_if_fail(GTK_IS_WIDGET(data), FALSE);
	g_return_val_if_fail(VTE_IS_TERMINAL(data), FALSE);
	widget = GTK_WIDGET(data);
	terminal = VTE_TERMINAL(data);
	bottom = (terminal->pvt->screen->insert_delta ==
		  terminal->pvt->screen->scroll_delta);

#ifdef VTE_DEBUG
	fprintf(stderr, "Handler processing %d bytes.\n",
		terminal->pvt->n_incoming);
#endif

	/* We should only be called when there's data to process. */
	g_assert(terminal->pvt->n_incoming > 0);

	/* Try to convert the data into wide characters. */
	ocount = sizeof(wchar_t) * terminal->pvt->n_incoming;
	obuf = obufptr = g_malloc(ocount);
	icount = terminal->pvt->n_incoming;
	ibuf = terminal->pvt->incoming;

	/* Convert the data to wide characters. */
	if (iconv(terminal->pvt->incoming_conv, &ibuf, &icount,
		  &obuf, &ocount) == -1) {
		/* No dice.  Try again when we have more data. */
		if ((icount > VTE_UTF8_BPC) &&
		    (icount < terminal->pvt->n_incoming)) {
			/* We barfed on something that had a high bit, so
			 * discard it. */
			start = terminal->pvt->n_incoming - icount;
			if (terminal->pvt->incoming[start] > 128) {
				/* Count the number of non-ascii chars. */
				for (end = start; end < terminal->pvt->n_incoming; end++) {
					/* If we're in UTF-8, just discard any
					 * bytes that claim to be part of this character. */
					if ((end > start) &&
					    (strcmp(terminal->pvt->encoding, "UTF-8") == 0) &&
					    ((terminal->pvt->incoming[end] & 0xc0) == 0xc0)) {
					    
						break;
					}
					if (terminal->pvt->incoming[end] < 128) {
						break;
					}
				}
				/* Be conservative about discarding data. */
				g_warning("Invalid multibyte sequence detected.  Munging up %d bytes of data.", end - start);
				/* Remove the offending bytes. */
				for (i = start; i < end; i++) {
#ifdef VTE_DEBUG
					fprintf(stderr, "Nuking byte %d/%02x.\n",
						terminal->pvt->incoming[i],
						terminal->pvt->incoming[i]);
#endif
					terminal->pvt->incoming[i] = '?';
				}
				/* Try again right away. */
				terminal->pvt->processing = (terminal->pvt->n_incoming > 0);
				if (terminal->pvt->processing == FALSE) {
					terminal->pvt->processing_tag = -1;
				}
				return terminal->pvt->processing;
			}
		}
#ifdef VTE_DEBUG
		fprintf(stderr, "Error converting %ld incoming data "
			"bytes: %s, leaving for later.\n",
			(long) terminal->pvt->n_incoming, strerror(errno));
#endif
		terminal->pvt->processing = FALSE;
		terminal->pvt->processing_tag = -1;
		return terminal->pvt->processing;
	}

	/* Store the current encoding. */
	encoding = terminal->pvt->encoding;

	/* Compute the number of wide characters we got. */
	wcount = (obuf - obufptr) / sizeof(wchar_t);
	wbuf = (wchar_t*) obufptr;

	/* Save the current cursor position. */
	screen = terminal->pvt->screen;
	cursor_row = screen->cursor_current.row;
	cursor_col = screen->cursor_current.col;

	/* Try initial substrings. */
	start = 0;
	inserted = leftovers = FALSE;
	while ((start < wcount) && !leftovers) {
		/* Check if the first character is part of a control
		 * sequence. */
		vte_trie_match(terminal->pvt->trie,
			       &wbuf[start],
			       1,
			       &match,
			       &next,
			       &quark,
			       &params);
		/* Next now points to the next character in the buffer we're
		 * uncertain about.  The match string falls into one of three
		 * classes, but only one of them is ambiguous, and we want to
		 * clear that up if possible. */
		if ((match != NULL) && (match[0] == '\0')) {
#ifdef VTE_DEBUG
			fprintf(stderr, "Ambiguous sequence  at %d of %d.  "
				"Resolving.\n", start, wcount);
#endif
			/* Try to match the *entire* string.  This will set
			 * "next" to a more useful value. */
			free_params_array(params);
			params = NULL;
			vte_trie_match(terminal->pvt->trie,
				       &wbuf[start],
				       wcount - start,
				       &match,
				       &next,
				       &quark,
				       &params);
			/* Now check just the number of bytes we know about
			 * to determine what we're doing in this iteration. */
			if (match == NULL) {
#ifdef VTE_DEBUG
				fprintf(stderr,
					"Looks like a sequence at %d, "
					"length = %d.\n", start,
					next - (wbuf + start));
#endif
				free_params_array(params);
				params = NULL;
				vte_trie_match(terminal->pvt->trie,
					       &wbuf[start],
					       next - (wbuf + start),
					       &match,
					       &next,
					       &quark,
					       &params);
			}
#ifdef VTE_DEBUG
			if ((match != NULL) && (match[0] != '\0')) {
				fprintf(stderr,
					"Ambiguity resolved -- sequence at %d, "
					"length = %d.\n", start,
					next - (wbuf + start));
			}
			if ((match != NULL) && (match[0] == '\0')) {
				int i;
				fprintf(stderr,
					"Ambiguity resolved -- incomplete `");
				for (i = 0; i < wcount; i++) {
					if (i == start) {
						fprintf(stderr, "=>");
					} else
					if (i == (next - wbuf)) {
						fprintf(stderr, "<=");
					}
					if ((wbuf[i] < 32) || (wbuf[i] > 127)) {
						fprintf(stderr, "{%ld}",
							(long) wbuf[i]);
					} else {
						fprintf(stderr, "%lc",
							(wint_t) wbuf[i]);
					}
				}
				if (i == (next - wbuf)) {
					fprintf(stderr, "<=");
				}
				fprintf(stderr, "' at %d.\n", start);
			}
			if (match == NULL) {
				fprintf(stderr, "Ambiguity resolved -- "
					"plain data (%d).\n", start);
			}
#endif
		}

#ifdef VTE_DEBUG
		else {
			if ((match != NULL) && (match[0] != '\0')) {
				fprintf(stderr,
					"Sequence (%d).\n", next - wbuf);
			}
			if ((match != NULL) && (match[0] == '\0')) {
				fprintf(stderr,
					"Incomplete (%d).\n", next - wbuf);
			}
			if (match == NULL) {
				fprintf(stderr,
					"Plain data (%d).\n", next - wbuf);
			}
		}
#endif
		/* We're in one of three possible situations now.
		 * First, the match string is a non-empty string and next
		 * points to the first character which isn't part of this
		 * sequence. */
		if ((match != NULL) && (match[0] != '\0')) {
			vte_invalidate_cursor_once(terminal);
			vte_terminal_handle_sequence(GTK_WIDGET(terminal),
						     match,
						     quark,
						     params);
			/* Skip over the proper number of wide chars. */
			start = (next - wbuf);
			/* Check if the encoding's changed. If it has, we need
			 * to force our caller to call us again to parse the
			 * rest of the data. */
			if (strcmp(encoding, terminal->pvt->encoding)) {
				leftovers = TRUE;
			}
			inserted = TRUE;
		} else
		/* Second, we have a NULL match, and next points the very
		 * next character in the buffer.  Insert the character we're
		 * which we're currently examining. */
		if (match == NULL) {
			c = wbuf[start];
#ifdef VTE_DEBUG
			if (c > 255) {
				fprintf(stderr, "%ld\n", (long) c);
			} else {
				if (c > 127) {
					fprintf(stderr, "%ld = ", (long) c);
				}
				if (c < 32) {
					fprintf(stderr, "^%lc\n",
						(wint_t)c + 64);
				} else {
					fprintf(stderr, "`%lc'\n", (wint_t)c);
				}
			}
#endif
			vte_terminal_insert_char(widget, c);
			inserted = TRUE;
			start++;
		} else {
			/* Case three: the read broke in the middle of a
			 * control sequence, so we're undecided with no more
			 * data to consult. If we have data following the
			 * middle of the sequence, then it's just garbage data,
			 * and for compatibility, we should discard it. */
			if (wbuf + wcount > next) {
#ifdef VTE_DEBUG
				fprintf(stderr, "Invalid control sequence, "
					"discarding %d characters.\n",
					next - (wbuf + start));
#endif
				/* Discard. */
				start = next - wbuf;
			} else {
				/* Pause processing and wait for more data. */
				leftovers = TRUE;
			}
		}
		/* Free any parameters we don't care about any more. */
		free_params_array(params);
		params = NULL;
	}

	if (leftovers) {
		/* There are leftovers, so convert them back to the terminal's
		 * old encoding and save them for later. */
		unconv = iconv_open(encoding, "WCHAR_T");
		if (unconv != NULL) {
			icount = sizeof(wchar_t) * (wcount - start);
			ibuf = (char*) &wbuf[start];
			ucount = VTE_UTF8_BPC * (wcount - start) + 1;
			ubuf = ubufptr = g_malloc(ucount);
			if (iconv(unconv, &ibuf, &icount,
				  &ubuf, &ucount) != -1) {
				/* Store it. */
				if (terminal->pvt->incoming) {
					g_free(terminal->pvt->incoming);
				}
				terminal->pvt->incoming = ubufptr;
				terminal->pvt->n_incoming = ubuf - ubufptr;
				*ubuf = '\0';
				/* If we're doing this because the encoding
				 * was changed out from under us, we need to
				 * keep trying to process the incoming data. */
				if (strcmp(encoding, terminal->pvt->encoding)) {
					again = TRUE;
				} else {
					again = FALSE;
				}
			} else {
#ifdef VTE_DEBUG
				fprintf(stderr, "Error unconverting %ld "
					"pending input bytes (%s), dropping.\n",
					(long) (sizeof(wchar_t) * (wcount - start)),
					strerror(errno));
#endif
				if (terminal->pvt->incoming) {
					g_free(terminal->pvt->incoming);
				}
				terminal->pvt->incoming = NULL;
				terminal->pvt->n_incoming = 0;
				g_free(ubufptr);
				again = FALSE;
			}
			iconv_close(unconv);
		} else {
			/* Discard the data, we can't use it. */
			if (terminal->pvt->incoming != NULL) {
				g_free(terminal->pvt->incoming);
			}
			terminal->pvt->incoming = NULL;
			terminal->pvt->n_incoming = 0;
			again = FALSE;
		}
	} else {
		/* No leftovers, clean out the data. */
		terminal->pvt->n_incoming = 0;
		g_free(terminal->pvt->incoming);
		terminal->pvt->incoming = NULL;
		again = FALSE;
	}
	g_free(obufptr);

	if (inserted) {
		/* Keep the cursor on-screen if we scroll on output, or if
		 * we're currently at the bottom of the buffer. */
		vte_terminal_scroll_insertion(terminal);
		if (terminal->pvt->scroll_on_output || bottom) {
			vte_terminal_scroll_to_bottom(terminal);
		}

		/* The cursor moved, so force it to be redrawn. */
		vte_invalidate_cursor_once(terminal);

		/* Deselect any existing selection. */
		vte_terminal_deselect_all(terminal);
	}

	if (inserted || (screen != terminal->pvt->screen)) {
		/* Signal that the visible contents changed. */
		vte_terminal_emit_contents_changed(terminal);
	}

	if ((cursor_row != terminal->pvt->screen->cursor_current.row) ||
	    (cursor_col != terminal->pvt->screen->cursor_current.col)) {
		/* Signal that the cursor moved. */
		vte_terminal_emit_cursor_moved(terminal);
	}

	/* Tell the input method where the cursor is. */
	rect.x = terminal->pvt->screen->cursor_current.col *
		 terminal->char_width;
	rect.width = terminal->char_width;
	rect.y = terminal->pvt->screen->cursor_current.row *
		 terminal->char_height;
	rect.height = terminal->char_height;
	gtk_im_context_set_cursor_location(terminal->pvt->im_context, &rect);

#ifdef VTE_DEBUG
	fprintf(stderr, "%d bytes left to process.\n",
		terminal->pvt->n_incoming);
#endif
	/* Decide if we're going to keep on processing data, and if not,
	 * note that our source tag is about to become invalid. */
	terminal->pvt->processing = again && (terminal->pvt->n_incoming > 0);
	if (terminal->pvt->processing == FALSE) {
		terminal->pvt->processing_tag = -1;
	}
#ifdef VTE_DEBUG
	if (terminal->pvt->processing) {
		fprintf(stderr, "Leaving processing handler on.\n");
	} else {
		fprintf(stderr, "Turning processing handler off.\n");
	}
#endif
	return terminal->pvt->processing;
}

/* Read and handle data from the child. */
static gboolean
vte_terminal_io_read(GIOChannel *channel,
		     GdkInputCondition condition,
		     gpointer data)
{
	VteTerminal *terminal;
	GtkWidget *widget;
	char *buf;
	size_t bufsize;
	int bcount, fd;
	gboolean empty, eof, leave_open = TRUE;

	g_return_val_if_fail(VTE_IS_TERMINAL(data), TRUE);
	widget = GTK_WIDGET(data);
	terminal = VTE_TERMINAL(data);

	/* Check that the channel is still open. */
	fd = g_io_channel_unix_get_fd(channel);

	/* Allocate a buffer to hold whatever data's available. */
	bufsize = terminal->pvt->n_incoming + LINE_MAX;
	buf = g_malloc0(bufsize);
	if (terminal->pvt->n_incoming > 0) {
		memcpy(buf, terminal->pvt->incoming, terminal->pvt->n_incoming);
	}
	empty = (terminal->pvt->n_incoming == 0);

	/* Read some more data in from this channel. */
	bcount = 0;
	if (condition & G_IO_IN) {
		bcount = read(fd, buf + terminal->pvt->n_incoming,
			      bufsize - terminal->pvt->n_incoming);
	}
	eof = FALSE;
	if (condition & G_IO_HUP) {
		eof = TRUE;
	}

	/* Catch errors. */
	leave_open = TRUE;
	switch (bcount) {
		case 0:
			/* EOF */
			eof = TRUE;
			break;
		case -1:
			switch (errno) {
				case EIO: /* Fake an EOF. */
					eof = TRUE;
					break;
				case EAGAIN:
				case EBUSY:
					leave_open = TRUE;
					break;
				default:
					g_warning("Error reading from child: "
						  "%s.\n", strerror(errno));
					leave_open = TRUE;
					break;
			}
			break;
		default:
			break;
	}

	/* If we got data, modify the pending buffer. */
	if (bcount >= 0) {
		if (terminal->pvt->incoming != NULL) {
			g_free(terminal->pvt->incoming);
		}
		terminal->pvt->incoming = buf;
		terminal->pvt->n_incoming += bcount;
	} else {
		g_free(buf);
	}

	/* If we have data to process, schedule some time to process it. */
	if (!terminal->pvt->processing && (terminal->pvt->n_incoming > 0)) {
#ifdef VTE_DEBUG
		fprintf(stderr, "Queuing handler to process bytes.\n");
#endif
		terminal->pvt->processing = TRUE;
		terminal->pvt->processing_tag =
			g_idle_add(vte_terminal_process_incoming, terminal);
	}

	/* If we detected an eof condition, signal one. */
	if (eof) {
		vte_terminal_eof(channel, terminal);
		leave_open = FALSE;
	}

	/* If there's more data coming, return TRUE, otherwise return FALSE. */
	return leave_open;
}

/* Render some UTF-8 text. */
void
vte_terminal_feed(VteTerminal *terminal, const char *data, size_t length)
{
	char *buf;
	gboolean empty;

	/* Allocate space for old and new data. */
	buf = g_malloc(terminal->pvt->n_incoming + length + 1);
	empty = (terminal->pvt->n_incoming == 0);

	/* If we got data, modify the pending buffer. */
	if (length >= 0) {
		if (terminal->pvt->n_incoming > 0) {
			memcpy(buf, terminal->pvt->incoming,
			       terminal->pvt->n_incoming);
			g_free(terminal->pvt->incoming);
		}
		memcpy(buf + terminal->pvt->n_incoming,
		       data, length);
		terminal->pvt->incoming = buf;
		terminal->pvt->n_incoming += length;
	} else {
		g_free(buf);
	}

	/* If we didn't have data before, but we do now, process it. */
	if (!terminal->pvt->processing && (terminal->pvt->n_incoming > 0)) {
#ifdef VTE_DEBUG
		fprintf(stderr, "Queuing handler to process bytes.\n");
#endif
		terminal->pvt->processing = TRUE;
		terminal->pvt->processing_tag = g_idle_add(vte_terminal_process_incoming,
							   terminal);
	}
}

/* Send wide characters to the child. */
static gboolean
vte_terminal_io_write(GIOChannel *channel,
		      GdkInputCondition condition,
		      gpointer data)
{
	VteTerminal *terminal;
	ssize_t count;
	int fd;
	gboolean leave_open;

	g_return_val_if_fail(VTE_IS_TERMINAL(data), FALSE);
	terminal = VTE_TERMINAL(data);

	fd = g_io_channel_unix_get_fd(channel);

	count = write(fd, terminal->pvt->outgoing, terminal->pvt->n_outgoing);
	if (count != -1) {
#ifdef VTE_DEBUG
		int i;
		for (i = 0; i < count; i++) {
			fprintf(stderr, "Wrote %c%c\n",
				terminal->pvt->outgoing[i] > 32 ?  ' ' : '^',
				terminal->pvt->outgoing[i] > 32 ?
				terminal->pvt->outgoing[i] :
				terminal->pvt->outgoing[i]  + 64);
		}
#endif
		memmove(terminal->pvt->outgoing,
			terminal->pvt->outgoing + count,
			terminal->pvt->n_outgoing - count);
		terminal->pvt->n_outgoing -= count;
	}

	if (terminal->pvt->n_outgoing == 0) {
		if (channel == terminal->pvt->pty_output) {
			terminal->pvt->pty_output = NULL;
		}
		leave_open = FALSE;
	} else {
		leave_open = TRUE;
	}

	return leave_open;
}

/* Convert some arbitrarily-encoded data to send to the child. */
static void
vte_terminal_send(VteTerminal *terminal, const char *encoding,
		  const void *data, size_t length)
{
	size_t icount, ocount;
	char *ibuf, *obuf, *obufptr;
	char *outgoing;
	size_t n_outgoing;
	iconv_t *conv;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_assert((strcmp(encoding, "UTF-8") == 0) ||
		 (strcmp(encoding, "WCHAR_T") == 0));

	conv = NULL;
	if (strcmp(encoding, "UTF-8") == 0) {
		conv = &terminal->pvt->outgoing_conv_utf8;
	}
	if (strcmp(encoding, "WCHAR_T") == 0) {
		conv = &terminal->pvt->outgoing_conv_wide;
	}
	g_assert(conv != NULL);
	g_assert(*conv != NULL);

	icount = length;
	ibuf = (char *) data;
	ocount = ((length + 1) * VTE_UTF8_BPC) + 1;
	obuf = obufptr = g_malloc0(ocount);

	if (iconv(*conv, &ibuf, &icount, &obuf, &ocount) == -1) {
		g_warning("Error (%s) converting data for child, dropping.\n",
			  strerror(errno));
	} else {
		n_outgoing = terminal->pvt->n_outgoing + (obuf - obufptr);
		outgoing = g_realloc(terminal->pvt->outgoing, n_outgoing);
		/* Move some data around. */
		memcpy(outgoing + terminal->pvt->n_outgoing,
		       obufptr, obuf - obufptr);
		/* Save the new outgoing buffer. */
		terminal->pvt->n_outgoing = n_outgoing;
		terminal->pvt->outgoing = outgoing;
		/* If we need to start waiting for the child pty to become
		 * available for writing, set that up here. */
		if (terminal->pvt->pty_output == NULL) {
			terminal->pvt->pty_output = g_io_channel_unix_new(terminal->pvt->pty_master);
			g_io_add_watch_full(terminal->pvt->pty_output,
					    G_PRIORITY_HIGH,
					    G_IO_OUT,
					    vte_terminal_io_write,
					    terminal,
					    NULL);
			g_io_channel_unref(terminal->pvt->pty_output);
		}
	}
	g_free(obufptr);
	return;
}

/* Send a chunk of UTF-8 text to the child. */
void
vte_terminal_feed_child(VteTerminal *terminal, const char *text, size_t length)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (length == (size_t)-1) {
		length = strlen(text);
	}
	vte_terminal_im_reset(terminal);
	vte_terminal_send(terminal, "UTF-8", text, length);
}

/* Send text from the input method to the child. */
static void
vte_terminal_im_commit(GtkIMContext *im_context, gchar *text, gpointer data)
{
	g_return_if_fail(VTE_IS_TERMINAL(data));
#ifdef VTE_DEBUG
	fprintf(stderr, "Input method committed `%s'.\n", text);
#endif
	vte_terminal_send(VTE_TERMINAL(data), "UTF-8", text, strlen(text));
}

/* We've started pre-editing. */
static void
vte_terminal_im_preedit_start(GtkIMContext *im_context, gpointer data)
{
	VteTerminal *terminal;

	g_return_if_fail(VTE_IS_TERMINAL(data));
	terminal = VTE_TERMINAL(data);
	g_return_if_fail(GTK_IS_IM_CONTEXT(im_context));

#ifdef VTE_DEBUG
	fprintf(stderr, "Input method pre-edit started.\n");
#endif
}

/* We've stopped pre-editing. */
static void
vte_terminal_im_preedit_end(GtkIMContext *im_context, gpointer data)
{
	VteTerminal *terminal;

	g_return_if_fail(VTE_IS_TERMINAL(data));
	terminal = VTE_TERMINAL(data);
	g_return_if_fail(GTK_IS_IM_CONTEXT(im_context));

#ifdef VTE_DEBUG
	fprintf(stderr, "Input method pre-edit ended.\n");
#endif
}

/* The pre-edit string changed. */
static void
vte_terminal_im_preedit_changed(GtkIMContext *im_context, gpointer data)
{
	gchar *str;
	PangoAttrList *attrs;
	VteTerminal *terminal;
	gint cursor;

	g_return_if_fail(VTE_IS_TERMINAL(data));
	terminal = VTE_TERMINAL(data);
	g_return_if_fail(GTK_IS_IM_CONTEXT(im_context));

	gtk_im_context_get_preedit_string(im_context, &str, &attrs, &cursor);
#ifdef VTE_DEBUG
	fprintf(stderr, "Input method pre-edit changed (%s,%d).\n",
		str, cursor);
#endif

	vte_invalidate_cursor_once(terminal);

	pango_attr_list_unref(attrs);
	if (terminal->pvt->im_preedit != NULL) {
		g_free(terminal->pvt->im_preedit);
	}
	terminal->pvt->im_preedit = str;
	terminal->pvt->im_preedit_cursor = cursor;

	vte_invalidate_cursor_once(terminal);
}

/* Handle the toplevel being reconfigured. */
static gboolean
vte_terminal_configure_toplevel(GtkWidget *widget, GdkEventConfigure *event,
				gpointer data)
{
#ifdef VTE_DEBUG
	fprintf(stderr, "Top level parent configured.\n");
#endif
	g_return_val_if_fail(GTK_IS_WIDGET(widget), FALSE);
	g_return_val_if_fail(GTK_WIDGET_TOPLEVEL(widget), FALSE);
	g_return_val_if_fail(VTE_IS_TERMINAL(data), FALSE);
	vte_terminal_queue_background_update(VTE_TERMINAL(data));
	return FALSE;
}

/* Handle a hierarchy-changed signal. */
static void
vte_terminal_hierarchy_changed(GtkWidget *widget, GtkWidget *old_toplevel,
			       gpointer data)
{
	VteTerminal *terminal;
	GtkWidget *toplevel;

#ifdef VTE_DEBUG
	fprintf(stderr, "Hierarchy changed.\n");
#endif

	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);

	if (GTK_IS_WIDGET(old_toplevel)) {
		g_signal_handlers_disconnect_by_func(G_OBJECT(old_toplevel),
						     vte_terminal_configure_toplevel,
						     terminal);
	}

	toplevel = gtk_widget_get_toplevel(widget);
	if (GTK_IS_WIDGET(toplevel)) {
		g_signal_connect(G_OBJECT(toplevel), "configure-event",
				 G_CALLBACK(vte_terminal_configure_toplevel),
				 terminal);
	}
}

/* Read and handle a keypress event. */
static gint
vte_terminal_key_press(GtkWidget *widget, GdkEventKey *event)
{
	VteTerminal *terminal;
	GdkModifierType modifiers;
	const PangoFontDescription *rofontdesc;
	PangoFontDescription *fontdesc;
	struct vte_termcap *termcap;
	const char *tterm;
	unsigned char *normal = NULL;
	size_t normal_length = 0;
	int i;
	unsigned char *special = NULL;
	struct termios tio;
	struct timeval tv;
	struct timezone tz;
	gboolean scrolled = FALSE, steal = FALSE;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(VTE_IS_TERMINAL(widget), FALSE);
	terminal = VTE_TERMINAL(widget);

	/* If it's a keypress, record that we got the event, in case the
	 * input method takes the event from us. */
	if (event->type == GDK_KEY_PRESS) {
		if (gettimeofday(&tv, &tz) == 0) {
			terminal->pvt->last_keypress_time =
				(tv.tv_sec * 1000) + (tv.tv_usec / 1000);
		}
		/* Read the modifiers. */
		if (gdk_event_get_state((GdkEvent*)event,
					&modifiers) == FALSE) {
			modifiers = 0;
		}
#ifdef VTE_DEBUG
		fprintf(stderr, "Keypress, modifiers=%d, keyval=%d, "
			"string=`%s'.\n", modifiers, event->keyval,
			event->string);
#endif
		/* Determine if we want to steal this keysym from the input
		 * method.  Ideally the answer would be "no" always. */
		steal = FALSE;
		if (modifiers & GDK_SHIFT_MASK) {
			switch (event->keyval) {
				case GDK_KP_Add:
				case GDK_KP_Subtract:
					steal = TRUE;
#ifdef VTE_DEBUG
				fprintf(stderr, "Hiding key from input method.\n");
#endif
					break;
				default:
					steal = FALSE;
			}
		}
		/* Hide the mouse cursor. */
		vte_terminal_set_pointer_visible(terminal, FALSE);
	}

	/* Let the input method at this one first. */
	if (!steal) {
		if (gtk_im_context_filter_keypress(terminal->pvt->im_context,
						   event)) {
			return TRUE;
		}
	}

	/* Now figure out what to send to the child. */
	if (event->type == GDK_KEY_PRESS) {
		/* Map the key to a sequence name if we can. */
		switch (event->keyval) {
			case GDK_BackSpace:
				switch (terminal->pvt->backspace_binding) {
					case VTE_ERASE_ASCII_BACKSPACE:
						normal = g_strdup("");
						normal_length = 1;
						break;
					case VTE_ERASE_ASCII_DELETE:
						normal = g_strdup("");
						normal_length = 1;
						break;
					case VTE_ERASE_DELETE_SEQUENCE:
						special = "kD";
						break;
					/* Use the tty's erase character. */
					case VTE_ERASE_AUTO:
					default:
						if (tcgetattr(terminal->pvt->pty_master,
							      &tio) != -1) {
							normal = g_strdup_printf("%c",
										 tio.c_cc[VERASE]);
							normal_length = 1;
						}
						break;
				}
				break;
			case GDK_Delete:
				switch (terminal->pvt->delete_binding) {
					case VTE_ERASE_ASCII_BACKSPACE:
						normal = g_strdup("");
						normal_length = 1;
						break;
					case VTE_ERASE_ASCII_DELETE:
						normal = g_strdup("");
						normal_length = 1;
						break;
					case VTE_ERASE_DELETE_SEQUENCE:
					case VTE_ERASE_AUTO:
					default:
						special = "kD";
						break;
				}
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
				special = "F1";
				break;
			case GDK_F12:
				special = "F2";
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
			case GDK_KP_Page_Up:
			case GDK_Page_Up:
				if (modifiers & GDK_SHIFT_MASK) {
					vte_terminal_scroll_pages(terminal, -1);
					scrolled = TRUE;
				} else {
					special = "kP";
				}
				break;
			case GDK_KP_Page_Down:
			case GDK_Page_Down:
				if (modifiers & GDK_SHIFT_MASK) {
					vte_terminal_scroll_pages(terminal, 1);
					scrolled = TRUE;
				} else {
					special = "kN";
				}
				break;
			case GDK_KP_Tab:
			case GDK_Tab:
				if (modifiers & GDK_SHIFT_MASK) {
					special = "kB";
				} else {
					normal = g_strdup("\t");
					normal_length = 1;
				}
				break;
			case GDK_KP_Space:
			case GDK_space:
				if (modifiers & GDK_CONTROL_MASK) {
					/* Ctrl-Space sends NUL?!?  Madness! */
					normal = g_strdup("");
					normal_length = 1;
				} else {
					normal = g_strdup(" ");
					normal_length = 1;
				}
				break;
			/* Let Shift +/- tweak the font, like XTerm does. */
			case GDK_KP_Add:
			case GDK_KP_Subtract:
				if ((modifiers & GDK_SHIFT_MASK) &&
				    terminal->pvt->xterm_font_tweak) {
					rofontdesc = vte_terminal_get_font(terminal);
					if (rofontdesc != NULL) {
						fontdesc = pango_font_description_copy(rofontdesc);
						i = pango_font_description_get_size(fontdesc);
						if (event->keyval == GDK_KP_Add) {
							i += PANGO_SCALE;
						}
						if (event->keyval == GDK_KP_Subtract) {
							i = MAX(PANGO_SCALE,
								i - PANGO_SCALE);
						}
						pango_font_description_set_size(fontdesc, i);
						vte_terminal_set_font(terminal, fontdesc);
						pango_font_description_free(fontdesc);
					}
					break;
				}
			/* The default is to just send the string. */
			default:
				if (event->string != NULL) {
					normal = g_strdup(event->string);
					normal_length = strlen(normal);
					if (modifiers & GDK_CONTROL_MASK) {
						/* Replace characters which have
						 * "control" counterparts with
						 * those counterparts. */
						for (i = 0;
						     i < normal_length;
						     i++) {
							if ((normal[i] > 64) &&
							    (normal[i] < 97)) {
								normal[i] ^= 0x40;
							}
						}
					}
				}
				break;
		}
		/* If we got normal characters, send them to the child. */
		if (normal != NULL) {
			if (terminal->pvt->alt_sends_escape &&
			    (normal_length > 0) &&
			    (modifiers & GDK_MOD1_MASK)) {
				vte_terminal_send(terminal, "UTF-8", "", 1);
			}
			vte_terminal_send(terminal, "UTF-8",
					  normal, normal_length);
			g_free(normal);
		} else
		/* If the key maps to characters, send them to the child. */
		if (special != NULL) {
			termcap = terminal->pvt->termcap;
			tterm = terminal->pvt->terminal;
			normal = vte_termcap_find_string_length(termcap,
								tterm,
								special,
								&normal_length);
			special = g_strdup_printf(normal, 1);
			vte_terminal_send(terminal, "UTF-8",
					  special, strlen(special));
			g_free(special);
		}
		/* Keep the cursor on-screen. */
		if (!scrolled &&
		    ((normal != NULL) || (special != NULL)) &&
		    terminal->pvt->scroll_on_keystroke) {
			vte_terminal_scroll_to_bottom(terminal);
		}
		return TRUE;
	}
	return FALSE;
}

/* Check if a particular character is part of a "word" or not. */
gboolean
vte_terminal_is_word_char(VteTerminal *terminal, gunichar c)
{
	int i;
	VteWordCharRange *range;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	if (terminal->pvt->word_chars == NULL) {
		return FALSE;
	}
	/* FIXME: if a gunichar isn't a wchar_t, we're probably screwed, so
	 * should we convert from UCS-4 to WCHAR_T or something here?  (Is a
	 * gunichar even a UCS-4 character)?  Or should we convert to UTF-8
	 * and then to WCHAR_T?  Aaaargh. */
	for (i = 0; i < terminal->pvt->word_chars->len; i++) {
		range = &g_array_index(terminal->pvt->word_chars,
				       VteWordCharRange,
				       i);
		if ((c >= range->start) && (c <= range->end)) {
			return TRUE;
		}
	}
	return FALSE;
}

/* Check if the characters in the given block are in the same class (word vs.
 * non-word characters). */
static gboolean
vte_uniform_class(VteTerminal *terminal, long row, long scol, long ecol)
{
	struct vte_charcell *pcell = NULL;
	long col;
	gboolean word_char;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	if ((pcell = vte_terminal_find_charcell(terminal, row, scol)) != NULL) {
		word_char = vte_terminal_is_word_char(terminal, pcell->c);
		for (col = scol + 1; col <= ecol; col++) {
			pcell = vte_terminal_find_charcell(terminal, row, col);
			if (pcell == NULL) {
				return FALSE;
			}
			if (word_char != vte_terminal_is_word_char(terminal,
								   pcell->c)) {
				return FALSE;
			}
		}
		return TRUE;
	}
	return FALSE;
}

/* Check if a cell is selected or not. */
static gboolean
vte_cell_is_selected(VteTerminal *terminal, long row, long col)
{
	long scol, ecol;

	/* If there's nothing selected, it's an easy question to answer. */
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	if (!terminal->pvt->has_selection) {
		return FALSE;
	}

	/* Sort the two columns, for the cases where the selection is
	 * entirely within a single line. */
	scol = MIN(terminal->pvt->selection_start.x,
		   terminal->pvt->selection_end.x);
	ecol = MAX(terminal->pvt->selection_start.x,
		   terminal->pvt->selection_end.x);

	switch (terminal->pvt->selection_type) {
		case selection_type_char:
			/* A cell is selected if it's on the line where the
			 * selected area starts, or the line where it ends,
			 * or any of the lines in between. */
			if ((row > terminal->pvt->selection_start.y) &&
			    (row < terminal->pvt->selection_end.y)) {
				return TRUE;
			} else
			/* It's also selected if the selection is confined to
			 * one line and the character lies between the start
			 * and end columns (which may not be in the more obvious
			 * of two possible orders). */
			if ((terminal->pvt->selection_start.y == row) &&
			    (terminal->pvt->selection_end.y == row)) {
				if ((col >= scol) && (col < ecol)) {
					return TRUE;
				}
			} else
			/* It's also selected if it's on the line where the
			 * selected area starts and it's after the start column,
			 * or on the line where the selection ends, after the
			 * last selected column. */
			if ((row == terminal->pvt->selection_start.y) &&
			    (col >= terminal->pvt->selection_start.x)) {
				return TRUE;
			} else
			if ((row == terminal->pvt->selection_end.y) &&
			    (col < terminal->pvt->selection_end.x)) {
				return TRUE;
			}
			break;
		case selection_type_word:
			/* A cell is selected if it's on the line where the
			 * selected area starts, or the line where it ends,
			 * or any of the lines in between. */
			if ((row > terminal->pvt->selection_start.y) &&
			    (row < terminal->pvt->selection_end.y)) {
				return TRUE;
			} else
			/* It's also selected if the selection is confined to
			 * one line and the character lies between the start
			 * and end columns (which may not be in the more obvious
			 * of two possible orders). */
			if ((terminal->pvt->selection_start.y == row) &&
			    (terminal->pvt->selection_end.y == row)) {
				if ((col >= scol) && (col <= ecol)) {
					return TRUE;
				} else
				/* If the character is before the beginning of
				 * the region, it's also selected if it and
				 * everything else in between belongs to the
				 * same character class. */
				if (col < scol) {
					if (vte_uniform_class(terminal,
							      row,
							      col,
							      scol)) {
						return TRUE;
					}
				} else
				if (col > ecol) {
					if (vte_uniform_class(terminal,
							      row,
							      ecol,
							      col)) {
						return TRUE;
					}
				}
			} else
			/* It's also selected if it's on the line where the
			 * selected area starts and it's after the start column,
			 * or on the line where the selection ends, after the
			 * last selected column. */
			if (row == terminal->pvt->selection_start.y) {
				if (col >= terminal->pvt->selection_start.x) {
					return TRUE;
				} else
				if (vte_uniform_class(terminal,
						      row,
						      col,
						      terminal->pvt->selection_start.x)) {
					return TRUE;
				}
			} else
			if (row == terminal->pvt->selection_end.y) {
				if (col < terminal->pvt->selection_end.x) {
					return TRUE;
				} else
				if (vte_uniform_class(terminal,
						      row,
						      terminal->pvt->selection_end.x,
						      col)) {
					return TRUE;
				}
			}
			break;
		case selection_type_line:
			/* A cell is selected if it's on the line where the
			 * selected area starts, or the line where it ends,
			 * or any of the lines in between. */
			if ((row >= terminal->pvt->selection_start.y) &&
			    (row <= terminal->pvt->selection_end.y)) {
				return TRUE;
			}
			break;
		default:
			break;
	}
	return FALSE;
}

/* Once we get text data, actually paste it in. */
static void
vte_terminal_paste_cb(GtkClipboard *clipboard, const gchar *text, gpointer data)
{
	VteTerminal *terminal;
	g_return_if_fail(VTE_IS_TERMINAL(data));
	terminal = VTE_TERMINAL(data);
	if (text != NULL) {
		vte_terminal_im_reset(terminal);
		vte_terminal_send(terminal, "UTF-8", text, strlen(text));
	}
}

/* Read and handle a motion event. */
static gint
vte_terminal_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
	VteTerminal *terminal;
	struct {
		long x, y;
	} o, p, q, origin, last;
	long delta, top, height, w, h;

	g_return_val_if_fail(VTE_IS_TERMINAL(widget), FALSE);
	terminal = VTE_TERMINAL(widget);

	/* Show the cursor. */
	vte_terminal_set_pointer_visible(terminal, TRUE);

	/* Handle a drag event. */
	if (terminal->pvt->mouse_last_button == 1) {
#ifdef VTE_DEBUG
		fprintf(stderr, "Drag.\n");
#endif
		vte_terminal_emit_selection_changed (terminal);
		terminal->pvt->has_selection = TRUE;

		w = terminal->char_width;
		h = terminal->char_height;
		origin.x = (terminal->pvt->selection_origin.x + w / 2) / w;
		origin.y = (terminal->pvt->selection_origin.y) / h;
		last.x = (event->x + w / 2) / w;
		last.y = (event->y) / h;
		o.x = (terminal->pvt->selection_last.x + w / 2) / w;
		o.y = (terminal->pvt->selection_last.y) / h;

		terminal->pvt->selection_last.x = event->x;
		terminal->pvt->selection_last.y = event->y;

		if (last.y > origin.y) {
			p.x = origin.x;
			p.y = origin.y;
			q.x = last.x;
			q.y = last.y;
		} else
		if (last.y < origin.y) {
			p.x = last.x;
			p.y = last.y;
			q.x = origin.x;
			q.y = origin.y;
		} else
		if (last.x > origin.x) {
			p.x = origin.x;
			p.y = origin.y;
			q.x = last.x;
			q.y = last.y;
		} else {
			p.x = last.x;
			p.y = last.y;
			q.x = origin.x;
			q.y = origin.y;
		}

		delta = terminal->pvt->screen->scroll_delta;

		terminal->pvt->selection_start.x = p.x;
		terminal->pvt->selection_start.y = p.y + delta;
		terminal->pvt->selection_end.x = q.x;
		terminal->pvt->selection_end.y = q.y + delta;

		top = MIN(o.y, MIN(p.y, q.y));
		height = MAX(o.y, MAX(p.y, q.y)) - top + 1;

#ifdef VTE_DEBUG
		fprintf(stderr, "selection is (%ld,%ld) to (%ld,%ld)\n",
			terminal->pvt->selection_start.x,
			terminal->pvt->selection_start.y,
			terminal->pvt->selection_end.x,
			terminal->pvt->selection_end.y);
		fprintf(stderr, "repainting rows %ld to %ld\n", top, top + height);
#endif

		vte_invalidate_cells(terminal, 0, terminal->column_count,
				     top + delta, height);
	} else {
#ifdef VTE_DEBUG
		fprintf(stderr, "Mouse move.\n");
#endif
	}

	return FALSE;
}

/* Note that the clipboard has cleared. */
static void
vte_terminal_clear_cb(GtkClipboard *clipboard, gpointer owner)
{
	VteTerminal *terminal;
	g_return_if_fail(VTE_IS_TERMINAL(owner));
	terminal = VTE_TERMINAL(owner);
	if (terminal->pvt->has_selection) {
		vte_terminal_deselect_all(terminal);
	}
}

/* Supply the selected text to the clipboard. */
static void
vte_terminal_copy_cb(GtkClipboard *clipboard, GtkSelectionData *data,
		     guint info, gpointer owner)
{
	VteTerminal *terminal;
	g_return_if_fail(VTE_IS_TERMINAL(owner));
	terminal = VTE_TERMINAL(owner);
	if (terminal->pvt->selection != NULL) {
		gtk_selection_data_set_text(data, terminal->pvt->selection, -1);
	}
}

/* Place the selected text onto the clipboard.  Do this asynchronously so that
 * we get notified when the selection we placed on the clipboard is replaced. */
static void
vte_terminal_copy(VteTerminal *terminal, GdkAtom board)
{
	GtkClipboard *clipboard;
	GtkWidget *widget;
	long x, y;
	VteScreen *screen;
	struct vte_charcell *pcell;
	wchar_t *buffer;
	size_t length;
	char *ibuf, *obuf, *obufptr;
	size_t icount, ocount;
	iconv_t conv;
	GtkTargetEntry targets[] = {
		{"UTF8_STRING", 0, 0},
		{"COMPOUND_TEXT", 0, 0},
		{"TEXT", 0, 0},
		{"STRING", 0, 0},
	};

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);
	clipboard = gtk_clipboard_get(board);

	/* Build a buffer with the selected wide chars. */
	screen = terminal->pvt->screen;
	buffer = g_malloc((terminal->column_count + 1) *
			  terminal->row_count * sizeof(wchar_t));
	length = 0;
	for (y = screen->scroll_delta;
	     y < terminal->row_count + screen->scroll_delta;
	     y++) {
		x = 0;
		do {
			pcell = vte_terminal_find_charcell(terminal, y, x);
			if (vte_cell_is_selected(terminal, y, x)) {
				if (pcell != NULL) {
					if (pcell->columns > 0) {
						buffer[length++] = pcell->c;
					}
				} else {
					if (x < terminal->column_count) {
						buffer[length++] = '\n';
					}
				}
			}
			x++;
		} while (pcell != NULL);
	}
	/* Now convert it all to UTF-8. */
	if (length > 0) {
		icount = sizeof(wchar_t) * length;
		ibuf = (char*) buffer;
		ocount = (terminal->column_count + 1) *
			 terminal->row_count * sizeof(wchar_t);
		obuf = obufptr = g_malloc0(ocount);
		conv = iconv_open("UTF-8", "WCHAR_T");
		if (conv) {
			if (iconv(conv, &ibuf, &icount,
				  &obuf, &ocount) != -1) {
#ifdef VTE_DEBUG
				fprintf(stderr, "Passing `%*s' to clipboard.\n",
					obuf - obufptr, obufptr);
#endif
				if (terminal->pvt->selection != NULL) {
					g_free(terminal->pvt->selection);
				}
				terminal->pvt->selection = g_strndup(obufptr,
								     obuf -
								     obufptr);
				gtk_clipboard_set_with_owner(clipboard,
							     targets,
							     G_N_ELEMENTS(targets),
							     vte_terminal_copy_cb,
							     vte_terminal_clear_cb,
							     G_OBJECT(terminal));
			} else {
				g_warning("Conversion error in copy (%s), "
					  "dropping.", strerror(errno));
			}
			iconv_close(conv);
		} else {
			g_warning("Error initializing for conversion.");
		}
		g_free(obufptr);
	}
	g_free(buffer);
}

/* Paste from the given clipboard. */
static void
vte_terminal_paste(VteTerminal *terminal, GdkAtom board)
{
	GtkClipboard *clipboard;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	clipboard = gtk_clipboard_get(board);
	if (clipboard != NULL) {
		gtk_clipboard_request_text(clipboard,
					   vte_terminal_paste_cb,
					   terminal);
	}
}

/* Send a button down or up notification. */
static void
vte_terminal_send_mouse_button(VteTerminal *terminal, GdkEventButton *event)
{
	unsigned char cb = 0, cx = 0, cy = 0;
	char buf[LINE_MAX];
	GdkModifierType modifiers;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* Read the modifiers. */
	if (gdk_event_get_state((GdkEvent*)event, &modifiers) == FALSE) {
		modifiers = 0;
	}
	/* Encode the button information in cb. */
	if (event->type == GDK_BUTTON_PRESS) {
		switch (event->button) {
			case 1:
				cb = 0;
				break;
			case 2:
				cb = 1;
				break;
			case 3:
				cb = 2;
				break;
			case 4:
				cb = 64;	/* FIXME: check */
				break;
			case 5:
				cb = 65;	/* FIXME: check */
				break;
		}
	}
	if (event->type == GDK_BUTTON_RELEASE) {
		cb = 3;
	}
	cb |= 32;
	/* Encode the modifiers. */
	if (modifiers & GDK_SHIFT_MASK) {
		cb |= 4;
	}
	if (modifiers & GDK_MOD1_MASK) {
		cb |= 8;
	}
	if (modifiers & GDK_CONTROL_MASK) {
		cb |= 16;
	}
	/* Encode the cursor coordinates. */
	cx = 32 + 1 + (event->x / terminal->char_width);
	cy = 32 + 1 + (event->y / terminal->char_height);
	/* Send the event to the child. */
	snprintf(buf, sizeof(buf), "%sM%c%c%c", VTE_CAP_CSI, cb, cx, cy);
	vte_terminal_feed_child(terminal, buf, strlen(buf));
}

/* Read and handle a pointing device buttonpress event. */
static gint
vte_terminal_button_press(GtkWidget *widget, GdkEventButton *event)
{
	VteTerminal *terminal;
	long height, width, delta;

	g_return_val_if_fail(VTE_IS_TERMINAL(widget), FALSE);
	terminal = VTE_TERMINAL(widget);
	height = terminal->char_height;
	width = terminal->char_width;
	delta = terminal->pvt->screen->scroll_delta;
	vte_terminal_set_pointer_visible(terminal, TRUE);

	if (event->type == GDK_BUTTON_PRESS) {
#ifdef VTE_DEBUG
		fprintf(stderr, "button %d pressed at (%lf,%lf)\n",
			event->button, event->x, event->y);
#endif
		if ((terminal->pvt->mouse_send_xy_on_button) ||
		    (terminal->pvt->mouse_send_xy_on_click)) {
			vte_terminal_send_mouse_button(terminal, event);
		}
		terminal->pvt->mouse_last_button = event->button;
		if (event->button == 1) {
			if (!GTK_WIDGET_HAS_FOCUS(widget)) {
				gtk_widget_grab_focus(widget);
			}
			vte_terminal_deselect_all(terminal);
			terminal->pvt->selection_origin.x = event->x;
			terminal->pvt->selection_origin.y = event->y;
			terminal->pvt->selection_type = selection_type_char;
			return TRUE;
		}
		if (event->button == 2) {
			vte_terminal_paste(terminal, GDK_SELECTION_PRIMARY);
			return TRUE;
		}
	}
	if (event->type == GDK_2BUTTON_PRESS) {
#ifdef VTE_DEBUG
		fprintf(stderr, "button %d double-clicked at (%lf,%lf)\n",
			event->button, event->x, event->y);
#endif
		if (event->button == 1) {
			if (!GTK_WIDGET_HAS_FOCUS(widget)) {
				gtk_widget_grab_focus(widget);
			}
			vte_terminal_deselect_all(terminal);
			terminal->pvt->has_selection = TRUE;
			terminal->pvt->selection_origin.x = event->x;
			terminal->pvt->selection_origin.y = event->y;
			terminal->pvt->selection_start.x = event->x / width;
			terminal->pvt->selection_start.y = event->y / height +
							   delta;
			terminal->pvt->selection_end =
			terminal->pvt->selection_start;
			terminal->pvt->selection_type = selection_type_word;
			vte_invalidate_cells(terminal,
					     0,
					     terminal->column_count,
					     terminal->pvt->selection_start.y,
					     1);
			return TRUE;
		}
	}
	if (event->type == GDK_3BUTTON_PRESS) {
#ifdef VTE_DEBUG
		fprintf(stderr, "button %d triple-clicked at (%lf,%lf)\n",
			event->button, event->x, event->y);
#endif
		if (event->button == 1) {
			if (!GTK_WIDGET_HAS_FOCUS(widget)) {
				gtk_widget_grab_focus(widget);
			}
			vte_terminal_deselect_all(terminal);
			terminal->pvt->has_selection = TRUE;
			terminal->pvt->selection_origin.x = event->x;
			terminal->pvt->selection_origin.y = event->y;
			terminal->pvt->selection_start.x = event->x / width;
			terminal->pvt->selection_start.y = event->y / height +
							   delta;
			terminal->pvt->selection_end =
			terminal->pvt->selection_start;
			terminal->pvt->selection_type = selection_type_line;
			vte_invalidate_cells(terminal,
					     0,
					     terminal->column_count,
					     terminal->pvt->selection_start.y,
					     1);
			return TRUE;
		}
	}
	return FALSE;
}

/* Read and handle a pointing device buttonrelease event. */
static gint
vte_terminal_button_release(GtkWidget *widget, GdkEventButton *event)
{
	VteTerminal *terminal;

	g_return_val_if_fail(VTE_IS_TERMINAL(widget), FALSE);
	terminal = VTE_TERMINAL(widget);
	vte_terminal_set_pointer_visible(terminal, TRUE);


	if (event->type == GDK_BUTTON_RELEASE) {
#ifdef VTE_DEBUG
		fprintf(stderr, "button %d released at (%lf,%lf)\n",
			event->button, event->x, event->y);
#endif
		if (terminal->pvt->mouse_send_xy_on_button) {
			vte_terminal_send_mouse_button(terminal, event);
		}
		if (event->button == 1) {
			vte_terminal_copy(terminal, GDK_SELECTION_PRIMARY);
		}
		terminal->pvt->mouse_last_button = 0;
	}

	return FALSE;
}

/* Handle receiving or losing focus. */
static gint
vte_terminal_focus_in(GtkWidget *widget, GdkEventFocus *event)
{
	g_return_val_if_fail(GTK_IS_WIDGET(widget), 0);
	GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);
	gtk_im_context_focus_in((VTE_TERMINAL(widget))->pvt->im_context);
	vte_invalidate_cursor_once(VTE_TERMINAL(widget));
	return TRUE;
}

static gint
vte_terminal_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
	g_return_val_if_fail(GTK_WIDGET(widget), 0);
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
	gtk_im_context_focus_out((VTE_TERMINAL(widget))->pvt->im_context);
	vte_invalidate_cursor_once(VTE_TERMINAL(widget));
	return TRUE;
}

static void
vte_terminal_font_complain(const char *font,
			   char **missing_charset_list,
			   int missing_charset_count)
{
	int i;
	char *charsets, *tmp;
	if ((missing_charset_count > 0) && (missing_charset_list != NULL)) {
		charsets = NULL;
		for (i = 0; i < missing_charset_count; i++) {
			if (charsets == NULL) {
				tmp = g_strdup(missing_charset_list[i]);
			} else {
				tmp = g_strconcat(charsets, ", ",
						  missing_charset_list[i],
						  NULL);
				g_free(charsets);
			}
			charsets = tmp;
			tmp = NULL;
		}
		g_warning("Warning: using fontset \"%s\", which is missing "
			  "these character sets: %s.\n", font, charsets);
		g_free(charsets);
	}
}

static int
xft_weight_from_pango_weight (int weight)
{
	/* cut-and-pasted from Pango */
	if (weight < (PANGO_WEIGHT_NORMAL + PANGO_WEIGHT_LIGHT) / 2)
		return XFT_WEIGHT_LIGHT;
	else if (weight < (PANGO_WEIGHT_NORMAL + 600) / 2)
		return XFT_WEIGHT_MEDIUM;
	else if (weight < (600 + PANGO_WEIGHT_BOLD) / 2)
		return XFT_WEIGHT_DEMIBOLD;
	else if (weight < (PANGO_WEIGHT_BOLD + PANGO_WEIGHT_ULTRABOLD) / 2)
		return XFT_WEIGHT_BOLD;
	else
		return XFT_WEIGHT_BLACK;
}

static int
xft_slant_from_pango_style (int style)
{
	switch (style) {
	case PANGO_STYLE_NORMAL:
		return XFT_SLANT_ROMAN;
		break;
	case PANGO_STYLE_ITALIC:
		return XFT_SLANT_ITALIC;
		break;
	case PANGO_STYLE_OBLIQUE:
		return XFT_SLANT_OBLIQUE;
		break;
	}

	return XFT_SLANT_ROMAN;
}

#ifdef HAVE_XFT
/* Create an Xft pattern from a Pango font description. */
static XftPattern *
xft_pattern_from_pango_font_description(const PangoFontDescription *font_desc)
{
	XftPattern *pattern;
	const char *family = "mono";
	int pango_mask = 0;
	int weight, style;
	double size = 14.0;

	if (font_desc != NULL) {
		pango_mask = pango_font_description_get_set_fields (font_desc);
	}

	pattern = XftPatternCreate ();

	/* Set the family for the pattern, or use a sensible default. */
	if (pango_mask & PANGO_FONT_MASK_FAMILY) {
		family = pango_font_description_get_family (font_desc);
	}
	XftPatternAddString (pattern, XFT_FAMILY, family);

	/* Set the font size for the pattern, or use a sensible default. */
	if (pango_mask & PANGO_FONT_MASK_SIZE) {
		size = (double) pango_font_description_get_size (font_desc);
		size /= (double) PANGO_SCALE;
	}
	XftPatternAddDouble (pattern, XFT_SIZE, size);

	/* There aren'ty any fallbacks for these, so just omit them from the
	 * pattern if they're not set in the pango font. */
	if (pango_mask & PANGO_FONT_MASK_WEIGHT) {
		weight = pango_font_description_get_weight (font_desc);
		XftPatternAddInteger (pattern, XFT_WEIGHT,
				      xft_weight_from_pango_weight (weight));
	}
	if (pango_mask & PANGO_FONT_MASK_STYLE) {
		style = pango_font_description_get_style (font_desc);
		XftPatternAddInteger (pattern, XFT_SLANT,
				      xft_slant_from_pango_style (style));
	}

	return pattern;
}
#endif

static char *
xlfd_from_pango_font_description(const PangoFontDescription *fontdesc)
{
	int weighti, stylei, stretchi, size, count;
	const char *weight, *style, *stretch, *family;
	char *ret = NULL;
	char **fonts;

	family = pango_font_description_get_family(fontdesc);
	weighti = pango_font_description_get_weight(fontdesc);
	if (weighti > ((PANGO_WEIGHT_NORMAL + PANGO_WEIGHT_BOLD) / 2)) {
		weight = "bold";
	} else {
		weight = "medium";
	}
	stylei = pango_font_description_get_style(fontdesc);
	switch (stylei) {
		case PANGO_STYLE_ITALIC:
			style = "i";
			break;
		case PANGO_STYLE_OBLIQUE:
			style = "o";
			break;
		default:
			style = "r";
			break;
	}
	stretchi = pango_font_description_get_stretch(fontdesc);
	if (stretchi <= PANGO_STRETCH_CONDENSED) {
		stretch = "condensed";
	} else
	if (stretchi <= PANGO_STRETCH_SEMI_CONDENSED) {
		stretch = "semicondensed";
	} else {
		stretch = "normal";
	}

	size = pango_font_description_get_size(fontdesc);

	ret = g_strdup_printf("-*-%s*-%s-%s-%s--%d-*-*-*-*-*-*-*",
			      family, weight, style, stretch,
			      PANGO_PIXELS(size));
	fonts = XListFonts(GDK_DISPLAY(), ret, 1, &count);
	if (fonts != NULL) {
#ifdef VTE_DEBUG
		int i;
		char *desc = pango_font_description_to_string(fontdesc);
		for (i = 0; i < count; i++) {
			fprintf(stderr, "Font `%s' matched `%s'.\n",
				desc, fonts[i]);
		}
		g_free(desc);
#endif
		XFreeFontNames(fonts);
	}
	if (count > 0) {
		return ret;
	} else {
		g_free(ret);
		return NULL;
	}
}

/* Set the fontset used for rendering text into the widget. */
void
vte_terminal_set_font(VteTerminal *terminal,
		      const PangoFontDescription *font_desc)
{
	long width, height, ascent, descent;
	GtkWidget *widget;
	XFontStruct **font_struct_list, font_struct;
	char *xlfds;
	char **missing_charset_list = NULL, *def_string = NULL;
	int missing_charset_count = 0;
	char **font_name_list = NULL;

	g_return_if_fail(terminal != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);

	/* Choose default font metrics.  I like '10x20' as a terminal font. */
	width = 10;
	height = 20;
	descent = 0;
	ascent = height - descent;

	if (terminal->pvt->use_pango) {
		/* Create the layout if we don't have one yet. */
		if (terminal->pvt->layout == NULL) {
			terminal->pvt->layout = pango_layout_new(gdk_pango_context_get());
		}
		/* Free the old font description. */
		if (terminal->pvt->fontdesc != NULL) {
			pango_font_description_free(terminal->pvt->fontdesc);
			terminal->pvt->fontdesc = NULL;
		}

		/* Create the new font description. */
#ifdef VTE_DEBUG
		if (font_desc != NULL) {
			char *tmp;
			tmp = pango_font_description_to_string (font_desc);
			fprintf (stderr, "Using pango font \"%s\".\n", tmp);
			g_free (tmp);
		} else {
			fprintf (stderr, "Using default pango font.\n");
		}
#endif
		if (font_desc != NULL) {
			terminal->pvt->fontdesc = pango_font_description_copy(font_desc);
		} else {
			terminal->pvt->fontdesc = pango_font_description_from_string("Luxi Mono 8");
		}

		/* Try to load the described font. */
		if (terminal->pvt->fontdesc != NULL) {
			PangoFont *font = NULL;
			PangoFontDescription *desc = NULL;
			PangoContext *pcontext = NULL;
			PangoFontMetrics *pmetrics = NULL;
			PangoLanguage *lang = NULL;
			pcontext = gdk_pango_context_get();
			font = pango_context_load_font(pcontext,
						       terminal->pvt->fontdesc);
			if (PANGO_IS_FONT(font)) {
				/* We got a font, reset the description so that
				 * it describes this font, and read metrics. */
				desc = pango_font_describe(font);
				pango_font_description_free(terminal->pvt->fontdesc);
				terminal->pvt->fontdesc = desc;

				pango_layout_set_font_description(terminal->pvt->layout, desc);
				lang = pango_context_get_language(pcontext);
				pmetrics = pango_font_get_metrics(font, lang);
				g_object_unref(G_OBJECT(font));
			}
			/* Pull character cell size info from the metrics. */
			if (pmetrics != NULL) {
				ascent = howmany(pango_font_metrics_get_ascent(pmetrics), PANGO_SCALE);
				descent = howmany(pango_font_metrics_get_descent(pmetrics), PANGO_SCALE);
				width = howmany(pango_font_metrics_get_approximate_char_width(pmetrics), PANGO_SCALE);
				height = ascent + descent;
				pango_font_metrics_unref(pmetrics);
			}
		}
	}

#ifdef HAVE_XFT
	if (terminal->pvt->use_xft) {
		XftFont *new_font;
		XftPattern *pattern;
		XftPattern *matched_pattern;
		XftResult result;

#ifdef VTE_DEBUG
		if (font_desc) {
			char *tmp;
			tmp = pango_font_description_to_string (font_desc);
			fprintf (stderr, "Using pango font \"%s\".\n", tmp);
			g_free (tmp);
		} else {
			fprintf (stderr, "Using default pango font.\n");
		}
#endif

		pattern = xft_pattern_from_pango_font_description(font_desc);

		/* Xft is on a lot of crack here - it fills in "result" when it
		 * feels like it, and leaves it uninitialized the rest of the
		 * time.  Whether it's filled in is impossible to determine
		 * afaict.  We don't care about its value anyhow.  */
		result = 0xffff; /* some bogus value to help in debugging */
		matched_pattern = XftFontMatch (GDK_DISPLAY(),
						gdk_x11_get_default_screen(),
						pattern, &result);

#ifdef VTE_DEBUG
		if (matched_pattern != NULL) {
			char buf[256];
			if (!XftNameUnparse (matched_pattern, buf, sizeof(buf)-1)) {
				buf[0] = '\0';
			}
			buf[sizeof(buf) - 1] = '\0';
			fprintf (stderr, "Matched pattern \"%s\".\n", buf);
		}

		switch (result) {
			case XftResultMatch:
			       fprintf(stderr, "matched.\n");
			       break;
			case XftResultNoMatch:
			       fprintf(stderr, "no match.\n");
			       break;
			case XftResultTypeMismatch:
			       fprintf(stderr, "type mismatch.\n");
			       break;
			case XftResultNoId:
			       fprintf(stderr, "no ID.\n");
			       break;
			default:
			       fprintf(stderr, "undefined/bogus result.\n");
			       break;
		}
#endif

		if (matched_pattern != NULL) {
			/* More Xft crackrock - it appears to "adopt"
			 * matched_pattern as new_font->pattern; whether it
			 * does this reliably or not, or does another
			 * unpredictable bogosity like the "result" field
			 * above, I don't know.
			 */
			new_font = XftFontOpenPattern(GDK_DISPLAY(),
						      matched_pattern);
		} else {
			new_font = NULL;
		}

		if (new_font == NULL) {
			char buf[256];
			if (!XftNameUnparse(matched_pattern, buf, sizeof(buf)-1)) {
				buf[0] = '\0';
			}
			buf[sizeof(buf) - 1] = '\0';

			g_warning("Failed to load Xft font pattern \"%s\", "
				  "falling back to default font.", buf);

			/* Try to use the deafult font. */
			new_font = XftFontOpen(GDK_DISPLAY(),
					       gdk_x11_get_default_screen(),
					       XFT_FAMILY, XftTypeString,
					       "mono",
					       XFT_SIZE, XftTypeDouble, 14.0,
					       0);
		}
		if (new_font == NULL) {
			g_warning("Failed to load default Xft font.");
		}

		g_assert (pattern != new_font->pattern);
		XftPatternDestroy (pattern);
		if ((matched_pattern != NULL) &&
		    (matched_pattern != new_font->pattern)) {
			XftPatternDestroy (matched_pattern);
		}

		if (new_font) {
			char buf[256];
			if (!XftNameUnparse (new_font->pattern, buf, sizeof(buf)-1)) {
				buf[0] = '\0';
			}
			buf[sizeof(buf) - 1] = '\0';

#ifdef VTE_DEBUG
			fprintf (stderr, "Opened new font `%s'.\n", buf);
#endif

			/* Dispose of any previously-opened font. */
			if (terminal->pvt->ftfont != NULL) {
				XftFontClose(GDK_DISPLAY(),
					     terminal->pvt->ftfont);
			}
			terminal->pvt->ftfont = new_font;
		}

		/* Read the metrics for the current font. */
		if (terminal->pvt->ftfont != NULL) {
			ascent = terminal->pvt->ftfont->ascent;
			descent = terminal->pvt->ftfont->descent;
			height = terminal->pvt->ftfont->height;
			width = terminal->pvt->ftfont->max_advance_width;
		} else {
			g_warning("Error allocating Xft font, disabling Xft.");
			terminal->pvt->use_xft = FALSE;
		}
	}
#endif

	if (!terminal->pvt->use_xft && !terminal->pvt->use_pango) {
		/* Load the font set, freeing another one if we loaded one
		 * before. */
#ifdef VTE_DEBUG
		if (font_desc) {
			char *tmp;
			tmp = pango_font_description_to_string (font_desc);
			fprintf (stderr, "Using pango font \"%s\".\n", tmp);
			g_free (tmp);
		} else {
			fprintf (stderr, "Using default pango font.\n");
		}
#endif
		if (font_desc) {
			xlfds = xlfd_from_pango_font_description(font_desc);
		} else {
			xlfds = "-misc-fixed-medium-r-normal--12-*-*-*-*-*-*-*";
		}
		if (xlfds == NULL) {
			xlfds = "-misc-fixed-medium-r-normal--12-*-*-*-*-*-*-*";
		}
		if (terminal->pvt->fontset != NULL) {
			XFreeFontSet(GDK_DISPLAY(), terminal->pvt->fontset);
		}
		terminal->pvt->fontset = XCreateFontSet(GDK_DISPLAY(),
							xlfds,
							&missing_charset_list,
							&missing_charset_count,
							&def_string);
		if (terminal->pvt->fontset != NULL) {
			vte_terminal_font_complain(xlfds, missing_charset_list,
						   missing_charset_count);
		} else {
			g_warning("Failed to load font set \"%s\", "
				  "falling back to default font.", xlfds);
			if (missing_charset_list != NULL) {
				XFreeStringList(missing_charset_list);
				missing_charset_list = NULL;
			}
			terminal->pvt->fontset = XCreateFontSet(GDK_DISPLAY(),
								"fixed",
								&missing_charset_list,
								&missing_charset_count,
								&def_string);
			if (terminal->pvt->fontset == NULL) {
				g_warning("Failed to load default font, "
					  "crashing or behaving abnormally.\n");
			} else {
				vte_terminal_font_complain(xlfds,
							   missing_charset_list,
							   missing_charset_count);
			}
		}
		if (missing_charset_list != NULL) {
			XFreeStringList(missing_charset_list);
			missing_charset_list = NULL;
		}
		g_return_if_fail(terminal->pvt->fontset != NULL);
		/* Read the font metrics. */
		if (XFontsOfFontSet(terminal->pvt->fontset,
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
			font_name_list = NULL;
		}
	}

	/* Now save the values. */
	terminal->char_width = width;
	terminal->char_height = height;
	terminal->char_ascent = ascent;
	terminal->char_descent = descent;

	/* Emit a signal that the font changed. */
	vte_terminal_emit_char_size_changed(terminal,
					    terminal->char_width,
					    terminal->char_height);

	/* Attempt to resize. */
	if (GTK_WIDGET_REALIZED(widget)) {
		gtk_widget_queue_resize(widget);
	}

	/* Make sure the entire window gets repainted. */
	vte_invalidate_cells(terminal,
			     0, terminal->column_count,
			     0, terminal->row_count);
}

void
vte_terminal_set_font_from_string(VteTerminal *terminal, const char *name)
{
	PangoFontDescription *font_desc;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(name != NULL);
	g_return_if_fail(strlen(name) > 0);

	font_desc = pango_font_description_from_string(name);
	vte_terminal_set_font(terminal, font_desc);
	pango_font_description_free(font_desc);
}

const PangoFontDescription *
vte_terminal_get_font(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	return terminal->pvt->fontdesc;
}

/* A comparison function which helps sort quarks. */
static gint
vte_compare_direct(gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

/* Read and refresh our perception of the size of the PTY. */
static void
vte_terminal_get_size(VteTerminal *terminal)
{
	struct winsize size;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->pty_master != -1) {
		/* Use an ioctl to read the size of the terminal. */
		if (ioctl(terminal->pvt->pty_master, TIOCGWINSZ, &size) != 0) {
			g_warning("Error reading PTY size, using defaults: "
				  "%s.", strerror(errno));
		} else {
			terminal->row_count = size.ws_row;
			terminal->column_count = size.ws_col;
		}
	}
}

/* Set the size of the PTY. */
void
vte_terminal_set_size(VteTerminal *terminal, long columns, long rows)
{
	struct winsize size;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->pty_master != -1) {
		memset(&size, 0, sizeof(size));
		size.ws_row = rows;
		size.ws_col = columns;
		/* Try to set the terminal size. */
		if (ioctl(terminal->pvt->pty_master, TIOCSWINSZ, &size) != 0) {
			g_warning("Error setting PTY size: %s.",
				  strerror(errno));
		}
	} else {
		terminal->row_count = rows;
		terminal->column_count = columns;
	}
	/* Read the terminal size, in case something went awry. */
	vte_terminal_get_size(terminal);
}

/* Redraw the widget. */
static void
vte_handle_scroll(VteTerminal *terminal)
{
	long dy, adj;
	GtkWidget *widget;
	VteScreen *screen;
	/* Sanity checks. */
	g_return_if_fail(GTK_IS_WIDGET(terminal));
	widget = GTK_WIDGET(terminal);
	screen = terminal->pvt->screen;
	if (GTK_WIDGET_REALIZED(widget) == FALSE) {
		return;
	}
	/* This may generate multiple redraws, so freeze it while we do them. */
	gdk_window_freeze_updates(widget->window);
	/* Read the new adjustment value and save the difference. */
	adj = floor(gtk_adjustment_get_value(terminal->adjustment));
	dy = screen->scroll_delta - adj;
	screen->scroll_delta = adj;
	if (dy != 0) {
		if ((terminal->pvt->bg_image == NULL) &&
		    (!terminal->pvt->bg_transparent)) {
			/* Scroll whatever's already in the window to avoid
			 * redrawing as much as possible -- any exposed area
			 * will be exposed for us by the windowing system
			 * and GDK. */
			gdk_window_scroll(widget->window,
					  0, dy * terminal->char_height);
		} else {
			/* If we have a background image, we need to redraw
			 * the entire window. */
			vte_invalidate_cells(terminal,
					     0,
					     terminal->column_count,
					     screen->scroll_delta,
					     terminal->row_count);
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
	char *stripped;
	size_t stripped_length;
	GQuark quark;
	char *tmp;
	int i;

	/* Set the emulation type, for reference. */
	if (emulation == NULL) {
		emulation = VTE_DEFAULT_EMULATION;
	}
	quark = g_quark_from_string(emulation);
	terminal->pvt->terminal = g_quark_to_string(quark);
#ifdef VTE_DEBUG
	fprintf(stderr, "Setting emulation to `%s'...", emulation);
#endif

	/* Create a trie to hold the control sequences. */
	if (terminal->pvt->trie) {
		vte_trie_free(terminal->pvt->trie);
	}
	terminal->pvt->trie = vte_trie_new();

	/* Create a tree to hold the handlers. */
	if (terminal->pvt->sequences) {
		g_tree_destroy(terminal->pvt->sequences);
	}
	terminal->pvt->sequences = g_tree_new(vte_compare_direct);
	for (i = 0; i < G_N_ELEMENTS(vte_sequence_handlers); i++) {
		if (vte_sequence_handlers[i].handler != NULL) {
			code = vte_sequence_handlers[i].code;
			g_tree_insert(terminal->pvt->sequences,
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
		tmp = vte_termcap_find_string(terminal->pvt->termcap,
					      terminal->pvt->terminal,
					      code);
		if ((tmp != NULL) && (tmp[0] != '\0')) {
			vte_termcap_strip(tmp, &stripped, &stripped_length);
			vte_trie_add(terminal->pvt->trie,
				     stripped, stripped_length,
				     vte_terminal_capability_strings[i].capability,
				     0);
			g_free(stripped);
		}
		g_free(tmp);
	}

	/* Add emulator-specific sequences. */
	for (i = 0; vte_xterm_capability_strings[i].value != NULL; i++) {
		code = vte_xterm_capability_strings[i].code;
		value = vte_xterm_capability_strings[i].value;
		vte_termcap_strip(code, &stripped, &stripped_length);
		vte_trie_add(terminal->pvt->trie, stripped, stripped_length,
			     value, 0);
		g_free(stripped);
	}
#ifdef VTE_DEBUG
	fprintf(stderr, "Trie contents:\n");
	vte_trie_print(terminal->pvt->trie);
	fprintf(stderr, "\n");
#endif

	/* Read emulation flags. */
	terminal->pvt->flags.am = vte_termcap_find_boolean(terminal->pvt->termcap,
							   terminal->pvt->terminal,
							   "am");

	/* Resize to the given default. */
	vte_terminal_set_size(terminal,
			      vte_termcap_find_numeric(terminal->pvt->termcap,
						       terminal->pvt->terminal,
						       "co") ?: 80,
			      vte_termcap_find_numeric(terminal->pvt->termcap,
						       terminal->pvt->terminal,
						       "li") ?: 24);
}

/* Set the path to the termcap file we read, and read it in. */
static void
vte_terminal_set_termcap(VteTerminal *terminal, const char *path)
{
	struct stat st;
	char path_default[PATH_MAX];

	if (path == NULL) {
		snprintf(path_default, sizeof(path_default),
			 DATADIR "/" PACKAGE "/termcap/%s",
			 terminal->pvt->terminal ?: VTE_DEFAULT_EMULATION);
		if (stat(path_default, &st) == 0) {
			path = path_default;
		} else {
			path = "/etc/termcap";
		}
	}
	terminal->pvt->termcap_path = g_quark_to_string(g_quark_from_string(path));
#ifdef VTE_DEBUG
	fprintf(stderr, "Loading termcap `%s'...", terminal->pvt->termcap_path);
#endif
	if (terminal->pvt->termcap) {
		vte_termcap_free(terminal->pvt->termcap);
	}
	terminal->pvt->termcap = vte_termcap_new(path);
#ifdef VTE_DEBUG
	fprintf(stderr, "\n");
#endif
	vte_terminal_set_emulation(terminal, terminal->pvt->terminal);
}

static void
vte_terminal_reset_rowdata(VteRing **ring, long lines)
{
	VteRing *new_ring;
	GArray *row;
	long i, next;
	new_ring = vte_ring_new(lines, vte_free_row_data, NULL);
	if (*ring) {
		next = vte_ring_next(*ring);
		for (i = vte_ring_delta(*ring); i < next; i++) {
			row = vte_ring_index(*ring, GArray*, i);
			vte_ring_append(new_ring, row);
		}
		vte_ring_free(*ring, FALSE);
	}
	*ring = new_ring;
}

/* Initialize the terminal widget after the base widget stuff is initialized.
 * We need to create a new psuedo-terminal pair, read in the termcap file, and
 * set ourselves up to do the interpretation of sequences. */
static void
vte_terminal_init(VteTerminal *terminal, gpointer *klass)
{
	struct _VteTerminalPrivate *pvt;
	struct passwd *pwd;
	GtkAdjustment *adjustment;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(terminal), GTK_CAN_FOCUS);

	/* Initialize data members with settings from the environment and
	 * structures to use for these. */
	pvt = terminal->pvt = g_malloc0(sizeof(*terminal->pvt));
	if (pvt->shell == NULL) {
		pwd = getpwuid(getuid());
		if (pwd != NULL) {
			pvt->shell = pwd->pw_shell;
#ifdef VTE_DEBUG
			fprintf(stderr, "Using user's shell (%s).\n",
				pvt->shell);
#endif
		}
	}
	if (pvt->shell == NULL) {
		pvt->shell = "/bin/sh";
#ifdef VTE_DEBUG
		fprintf(stderr, "Using hard-coded default shell (%s).\n",
			pvt->shell);
#endif
	}
	pvt->shell = g_quark_to_string(g_quark_from_string(pvt->shell));
	pvt->pty_master = -1;
	pvt->pty_pid = -1;
	pvt->incoming = NULL;
	pvt->n_incoming = 0;
	pvt->processing = FALSE;
	pvt->processing_tag = -1;
	pvt->outgoing = NULL;
	pvt->n_outgoing = 0;
	pvt->keypad = VTE_KEYPAD_NORMAL;

	vte_terminal_set_word_chars(terminal, "-a-zA-Z0-9");

	pvt->scroll_on_output = FALSE;
	pvt->scroll_on_keystroke = TRUE;
	pvt->scrollback_lines = 0;
	pvt->alt_sends_escape = TRUE;
	pvt->audible_bell = TRUE;
	pvt->bg_transparent = FALSE;
	pvt->bg_transparent_update_pending = FALSE;
	pvt->bg_transparent_update_tag = -1;
	pvt->bg_transparent_atom = 0;
	pvt->bg_transparent_window = NULL;
	pvt->bg_transparent_image = NULL;
	pvt->bg_toplevel = NULL;
	pvt->bg_saturation = 0.4 * VTE_SATURATION_MAX;
	pvt->bg_image = NULL;

	pvt->cursor_blinks = FALSE;
	pvt->cursor_blink_tag = g_timeout_add(0,
		 			      vte_invalidate_cursor_periodic,
					      terminal);
	pvt->last_keypress_time = 0;

	pvt->has_selection = FALSE;
	pvt->selection = NULL;
	pvt->selection_start.x = 0;
	pvt->selection_start.y = 0;
	pvt->selection_end.x = 0;
	pvt->selection_end.y = 0;

	pvt->fontset = NULL;

	/* Decide if we're going to use Pango (pangox) for rendering. */
	pvt->fontdesc = NULL;
	pvt->layout = NULL;
	pvt->use_pango = TRUE;
	if (pvt->use_pango) {
		if (getenv("VTE_USE_PANGO") != NULL) {
			if (atol(getenv("VTE_USE_PANGO")) == 0) {
				pvt->use_pango = FALSE;
			}
		}
	}

#ifdef HAVE_XFT
	/* Try to use Xft if the user requests it.  Provide both the original
	 * variable we consulted (which we should stop consulting at some
	 * point) and the one GTK itself uses. */
	pvt->ftfont = NULL;
	pvt->use_xft = FALSE;
	if (getenv("VTE_USE_XFT") != NULL) {
		if (atol(getenv("VTE_USE_XFT")) != 0) {
			pvt->use_xft = TRUE;
		}
	}
	if (!pvt->use_xft) {
		if (getenv("GDK_USE_XFT") != NULL) {
			if (atol(getenv("GDK_USE_XFT")) != 0) {
				pvt->use_xft = TRUE;
			}
		}
	}
#endif

	/* Set the default font. */
	vte_terminal_set_font(terminal, NULL);

	/* Load the termcap data and set up the emulation and default
	 * terminal encoding. */
	vte_terminal_set_termcap(terminal, NULL);
	vte_terminal_set_emulation(terminal, NULL);
	vte_terminal_set_encoding(terminal, NULL);

	/* Initialize the screen history. */
	vte_terminal_reset_rowdata(&pvt->normal_screen.row_data,
				   VTE_SCROLLBACK_MIN);
	pvt->normal_screen.cursor_current.row = 0;
	pvt->normal_screen.cursor_current.col = 0;
	pvt->normal_screen.cursor_saved.row = 0;
	pvt->normal_screen.cursor_saved.col = 0;
	pvt->normal_screen.cursor_visible = TRUE;
	pvt->normal_screen.insert_delta = 0;
	pvt->normal_screen.scroll_delta = 0;
	pvt->normal_screen.insert = FALSE;

	vte_terminal_reset_rowdata(&pvt->alternate_screen.row_data,
				   VTE_SCROLLBACK_MIN);
	pvt->alternate_screen.cursor_current.row = 0;
	pvt->alternate_screen.cursor_current.col = 0;
	pvt->alternate_screen.cursor_saved.row = 0;
	pvt->alternate_screen.cursor_saved.col = 0;
	pvt->alternate_screen.cursor_visible = TRUE;
	pvt->alternate_screen.insert_delta = 0;
	pvt->alternate_screen.scroll_delta = 0;
	pvt->alternate_screen.insert = FALSE;

	pvt->screen = &terminal->pvt->alternate_screen;
	vte_terminal_set_default_attributes(terminal);

	pvt->screen = &terminal->pvt->normal_screen;
	vte_terminal_set_default_attributes(terminal);

	/* Set up the default palette. */
	vte_terminal_set_default_colors(terminal);

	/* Set up an adjustment to control scrolling. */
	adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0));
	vte_terminal_set_scroll_adjustment(terminal, adjustment);

	/* Set up hierarchy change notifications. */
	g_signal_connect(G_OBJECT(terminal), "hierarchy-changed",
			 G_CALLBACK(vte_terminal_hierarchy_changed),
			 NULL);

	/* Set up input method support. */
	pvt->im_context = NULL;

	/* Set backspace/delete bindings. */
	vte_terminal_set_backspace_binding(terminal, VTE_ERASE_AUTO);
	vte_terminal_set_delete_binding(terminal, VTE_ERASE_AUTO);

	/* Set mouse pointer settings. */
	pvt->mouse_send_xy_on_click = FALSE;
	pvt->mouse_send_xy_on_button = FALSE;
	pvt->mouse_hilite_tracking = FALSE;
	pvt->mouse_cell_motion_tracking = FALSE;
	pvt->mouse_all_motion_tracking = FALSE;
	pvt->mouse_last_button = 0;
	pvt->mouse_default_cursor = NULL;
	pvt->mouse_mousing_cursor = NULL;
	pvt->mouse_inviso_cursor = NULL;
	pvt->mouse_autohide = FALSE;

	/* Set various other settings. */
	pvt->xterm_font_tweak = FALSE;
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

#ifdef VTE_DEBUG
	fprintf(stderr, "Initial size request is %dx%d.\n",
		requisition->width, requisition->height);
#endif
}

/* Accept a given size from GTK+. */
static void
vte_terminal_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	VteTerminal *terminal;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);

#ifdef VTE_DEBUG
	fprintf(stderr, "Sizing window to %dx%d (%ldx%ld).\n",
		allocation->width, allocation->height,
		allocation->width / terminal->char_width,
		allocation->height / terminal->char_height);
#endif

	/* Set our allocation to match the structure. */
	widget->allocation = *allocation;

	/* Set the size of the pseudo-terminal. */
	vte_terminal_set_size(terminal,
			      allocation->width / terminal->char_width,
			      allocation->height / terminal->char_height);

	/* Resize the GDK window. */
	if (widget->window != NULL) {
		gdk_window_move_resize(widget->window,
				       allocation->x,
				       allocation->y,
				       allocation->width,
				       allocation->height);
		vte_terminal_queue_background_update(terminal);
	}

	/* Adjust the adjustments. */
	vte_terminal_adjust_adjustments(terminal);
}

/* The window is being destroyed. */
static void
vte_terminal_unrealize(GtkWidget *widget)
{
	VteTerminal *terminal;
	Display *display;
	GdkColormap *gcolormap;
	Colormap colormap;
	GdkVisual *gvisual;
	Visual *visual;
	int i;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);

	/* Shut down input methods. */
	g_object_unref(G_OBJECT(terminal->pvt->im_context));
	terminal->pvt->im_context = NULL;

#ifdef HAVE_XFT
	/* Clean up after Xft. */
	display = gdk_x11_drawable_get_xdisplay(widget->window);
	gvisual = gtk_widget_get_visual(widget);
	visual = gdk_x11_visual_get_xvisual(gvisual);
	gcolormap = gtk_widget_get_colormap(widget);
	colormap = gdk_x11_colormap_get_xcolormap(gcolormap);
	for (i = 0; i < G_N_ELEMENTS(terminal->pvt->palette); i++) {
		XftColorFree(display,
			     visual,
			     colormap,
			     &terminal->pvt->palette[i].ftcolor);
	}
	if (terminal->pvt->ftfont != NULL) {
		XftFontClose(display, terminal->pvt->ftfont);
		terminal->pvt->ftfont = NULL;
	}
#endif

	/* Clean up after Pango. */
	if (terminal->pvt->layout != NULL) {
		g_object_unref(G_OBJECT(terminal->pvt->layout));
		terminal->pvt->layout = NULL;
	}
	if (terminal->pvt->fontdesc != NULL) {
		pango_font_description_free(terminal->pvt->fontdesc);
		terminal->pvt->fontdesc = NULL;
	}

	/* Disconnect any filters which might be watching for X window
	 * pixmap changes. */
	if (terminal->pvt->bg_transparent) {
		gdk_window_remove_filter(terminal->pvt->bg_transparent_window,
					 vte_terminal_filter_property_changes,
					 terminal);
	}
	gdk_window_remove_filter(widget->window,
				 vte_terminal_filter_property_changes,
				 terminal);

	/* Unmap the widget if it hasn't been already. */
	if (GTK_WIDGET_MAPPED(widget)) {
		gtk_widget_unmap(widget);
	}

	/* Deallocate the cursors. */
	gdk_cursor_unref(terminal->pvt->mouse_default_cursor);
	terminal->pvt->mouse_default_cursor = NULL;
	gdk_cursor_unref(terminal->pvt->mouse_mousing_cursor);
	terminal->pvt->mouse_mousing_cursor = NULL;
	gdk_cursor_unref(terminal->pvt->mouse_inviso_cursor);
	terminal->pvt->mouse_inviso_cursor = NULL;

	/* Remove the GDK window. */
	if (widget->window != NULL) {
		gdk_window_destroy(widget->window);
		widget->window = NULL;
	}

	/* Mark that we no longer have a GDK window. */
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
}

/* Perform final cleanups for the widget before it's freed. */
static void
vte_terminal_finalize(GObject *object)
{
	VteTerminal *terminal;
	GtkWidget *toplevel;
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_return_if_fail(VTE_IS_TERMINAL(object));
	terminal = VTE_TERMINAL(object);
	object_class = G_OBJECT_GET_CLASS(G_OBJECT(object));
	widget_class = g_type_class_peek(GTK_TYPE_WIDGET);

	/* Remove the blink timeout function. */
	if (terminal->pvt->cursor_blink_tag != -1) {
		g_source_remove(terminal->pvt->cursor_blink_tag);
	}

	/* Remove idle handlers. */
	if (terminal->pvt->processing_tag != -1) {
		g_source_remove(terminal->pvt->processing_tag);
	}
	if (terminal->pvt->bg_transparent_update_tag != -1) {
		g_source_remove(terminal->pvt->bg_transparent_update_tag);
	}

	/* Disconnect from toplevel window configure events. */
	toplevel = gtk_widget_get_toplevel(GTK_WIDGET(object));
	if ((toplevel != NULL) && (G_OBJECT(toplevel) != G_OBJECT(object))) {
		g_signal_handlers_disconnect_by_func(toplevel,
						     vte_terminal_configure_toplevel,
						     terminal);
	}

	/* Free any selected text. */
	if (terminal->pvt->selection != NULL) {
		g_free(terminal->pvt->selection);
		terminal->pvt->selection = NULL;
	}

	/* Shut down the child terminal. */
	close(terminal->pvt->pty_master);
	terminal->pvt->pty_master = -1;
	if (terminal->pvt->pty_pid > 0) {
		kill(-terminal->pvt->pty_pid, SIGHUP);
	}
	terminal->pvt->pty_pid = 0;

	/* Clear some of our strings. */
	terminal->pvt->termcap_path = NULL;
	terminal->pvt->shell = NULL;
	terminal->pvt->terminal = NULL;

	/* Stop watching for input from the child. */
	if (terminal->pvt->pty_input != NULL) {
		g_io_channel_unref(terminal->pvt->pty_input);
		terminal->pvt->pty_input = NULL;
	}
	if (terminal->pvt->pty_output != NULL) {
		g_io_channel_unref(terminal->pvt->pty_output);
		terminal->pvt->pty_output = NULL;
	}

	/* Discard any pending data. */
	g_free(terminal->pvt->incoming);
	terminal->pvt->incoming = NULL;
	terminal->pvt->n_incoming = 0;

	/* Clean up emulation structures. */
	g_tree_destroy(terminal->pvt->sequences);
	terminal->pvt->sequences= NULL;
	vte_termcap_free(terminal->pvt->termcap);
	terminal->pvt->termcap = NULL;
	vte_trie_free(terminal->pvt->trie);
	terminal->pvt->trie = NULL;

	/* Clear the output histories. */
	vte_ring_free(terminal->pvt->normal_screen.row_data, TRUE);
	terminal->pvt->normal_screen.row_data = NULL;
	vte_ring_free(terminal->pvt->alternate_screen.row_data, TRUE);
	terminal->pvt->alternate_screen.row_data = NULL;

	/* Free strings. */
	g_free(terminal->window_title);
	terminal->window_title = NULL;
	g_free(terminal->icon_title);
	terminal->icon_title = NULL;

	/* Free background images. */
	if (terminal->pvt->bg_image != NULL) {
		g_object_unref(G_OBJECT(terminal->pvt->bg_image));
		terminal->pvt->bg_image = NULL;
	}
	if (terminal->pvt->bg_transparent_image != NULL) {
		g_object_unref(G_OBJECT(terminal->pvt->bg_transparent_image));
		terminal->pvt->bg_transparent_image = NULL;
	}

	/* Free the word chars array. */
	g_array_free(terminal->pvt->word_chars, FALSE);

	/* Call the inherited finalize() method. */
	if (G_OBJECT_CLASS(widget_class)->finalize) {
		(G_OBJECT_CLASS(widget_class))->finalize(object);
	}
}

/* Handle realizing the widget.  Most of this is copy-paste from GGAD. */
static void
vte_terminal_realize(GtkWidget *widget)
{
	VteTerminal *terminal = NULL;
	GdkWindowAttr attributes;
	GdkPixmap *pixmap;
	GdkColor black = {0,};
	int attributes_mask = 0;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);

	/* Create the stock cursors. */
	terminal->pvt->mouse_default_cursor =
		gdk_cursor_new(VTE_DEFAULT_CURSOR);
	terminal->pvt->mouse_mousing_cursor =
		gdk_cursor_new(VTE_MOUSING_CURSOR);

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
				GDK_POINTER_MOTION_MASK |
				GDK_BUTTON1_MOTION_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK;
	attributes.cursor = terminal->pvt->mouse_default_cursor;
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

	/* Set the realized flag. */
	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

	/* Add a filter to watch for property changes. */
	gdk_window_add_filter(widget->window,
			      vte_terminal_filter_property_changes,
			      terminal);

	/* Set up input method support.  FIXME: do we need to handle the
	 * "retrieve-surrounding" and "delete-surrounding" events? */
	terminal->pvt->im_context = gtk_im_multicontext_new();
	gtk_im_context_set_client_window(terminal->pvt->im_context,
					 widget->window);
	g_signal_connect(G_OBJECT(terminal->pvt->im_context), "commit",
			 GTK_SIGNAL_FUNC(vte_terminal_im_commit), terminal);
	g_signal_connect(G_OBJECT(terminal->pvt->im_context), "preedit-start",
			 GTK_SIGNAL_FUNC(vte_terminal_im_preedit_start),
			 terminal);
	g_signal_connect(G_OBJECT(terminal->pvt->im_context), "preedit-changed",
			 GTK_SIGNAL_FUNC(vte_terminal_im_preedit_changed),
			 terminal);
	g_signal_connect(G_OBJECT(terminal->pvt->im_context), "preedit-end",
			 GTK_SIGNAL_FUNC(vte_terminal_im_preedit_end),
			 terminal);
	gtk_im_context_set_use_preedit(terminal->pvt->im_context, TRUE);

	/* Create our invisible cursor. */
	pixmap = gdk_pixmap_new(widget->window, 1, 1, 1);
	terminal->pvt->mouse_inviso_cursor =
		gdk_cursor_new_from_pixmap(pixmap, pixmap,
					   &black, &black, 0, 0);
	g_object_unref(G_OBJECT(pixmap));
	pixmap = NULL;

	/* Grab input focus. */
	gtk_widget_grab_focus(widget);
}

static void
vte_terminal_determine_colors(VteTerminal *terminal,
			      const struct vte_charcell *cell, gboolean reverse,
			      int *fore, int *back)
{
	/* Determine what the foreground and background colors for rendering
	 * text should be. */
	if (reverse) {
		*fore = cell ? cell->back : VTE_DEF_BG;
		*back = cell ? cell->fore : VTE_DEF_FG;
	} else {
		*fore = cell ? cell->fore : VTE_DEF_FG;
		*back = cell ? cell->back : VTE_DEF_BG;
	}

	/* Handle invisible, bold, and standout text by adjusting colors. */
	if (cell && cell->invisible) {
		*fore = *back;
	}
	if (cell && cell->bold) {
		if ((*fore != VTE_DEF_FG) && (*fore != VTE_DEF_BG)) {
			*fore += 8;
		} else {
			/* Aaargh. We have to do *something*. */
			*fore = 15;
		}
	}
	if (cell && cell->standout) {
		if ((*back != VTE_DEF_FG) && (*back != VTE_DEF_BG)) {
			*back += 8;
		}
	}
}

/* Draw a particular character on the screen. */
static void
vte_terminal_draw_char(VteTerminal *terminal,
		       VteScreen *screen,
		       struct vte_charcell *cell,
		       long col,
		       long row,
		       long x,
		       long y,
		       long x_offs,
		       long y_offs,
		       long width,
		       long height,
		       long ascent,
		       long descent,
		       Display *display,
		       GdkDrawable *gdrawable,
		       Drawable drawable,
		       GdkColormap *gcolormap,
		       Colormap colormap,
		       GdkVisual *gvisual,
		       Visual *visual,
		       GdkGC *ggc,
		       GC gc,
		       PangoLayout *layout,
#ifdef HAVE_XFT
		       XftDraw *ftdraw,
#endif
		       gboolean cursor)
{
	int fore, back, dcol;
	long xcenter, ycenter, xright, ybottom;
	char utf8_buf[7] = {0,};
	gboolean drawn, reverse;
	PangoAttribute *attr;
	PangoAttrList *attrlist;
	XwcTextItem textitem;

#ifdef VTE_DEBUG
#ifdef VTE_DEBUG_DRAW
	fprintf(stderr, "Drawing %ld/%d at (%d,%d), ",
		cell ? cell->c : 0, cell ? cell->columns : 0, x, y + ascent);
#endif
#endif

	/* Determine what the foreground and background colors for rendering
	 * text should be. */
	reverse = cell && cell->reverse;
	reverse = reverse ^ vte_cell_is_selected(terminal, row, col);
	reverse = reverse || cursor;
	vte_terminal_determine_colors(terminal, cell, reverse, &fore, &back);

	/* Paint the background for the cell. */
	if ((back != VTE_DEF_BG) && GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
		XSetForeground(display, gc, terminal->pvt->palette[back].pixel);
		XFillRectangle(display, drawable, gc, x, y, width, height);
	}

	/* If there's no data, bug out here. */
	if (cell == NULL) {
#ifdef VTE_DEBUG
#ifdef VTE_DEBUG_DRAW
		fprintf(stderr, " skipping.\n");
#endif
#endif
		return;
	}

	/* If this column is zero-width, backtrack until we find the
	 * multi-column character which renders into this column. */
	dcol = col;
	if (cell->columns == 0) {
		/* Search for a suitable cell. */
		for (dcol = col - 1; dcol >= 0; dcol--) {
			cell = vte_terminal_find_charcell(terminal,
							  row, dcol);
			if (cell->columns > 0) {
				break;
			}
		}
		/* If we didn't find anything, bail. */
		if (dcol < 0) {
#ifdef VTE_DEBUG
#ifdef VTE_DEBUG_DRAW
			fprintf(stderr, " skipping.\n");
#endif
#endif
			return;
		}
	}
	x -= (col - dcol) * width;
	width += ((col - dcol) * width);
	drawn = FALSE;

#ifdef VTE_DEBUG
#ifdef VTE_DEBUG_DRAW
	fprintf(stderr, "adjusted to %ld/%d at (%d,%d).\n",
		cell->c, cell->columns, x, y + ascent);
#endif
#endif

	/* If the character is drawn in the alternate graphic font, do the
	 * drawing ourselves. */
	if (cell->alternate) {
		xright = x + width;
		ybottom = y + height;
		xcenter = (x + xright) / 2;
		ycenter = (y + ybottom) / 2;

		/* Draw the alternate charset data. */
		XSetForeground(display, gc, terminal->pvt->palette[fore].pixel);
		switch (cell->c) {
			case 95:
				/* drawing a blank */
				break;
			case 96:  /* ` */
			case 97:  /* a */
			case 98:  /* b */
			case 99:  /* c */
			case 100: /* d */
			case 101: /* e */
			case 102: /* f */
			case 103: /* g */
			case 104: /* h */
			case 105: /* i */
				g_warning("Alternate character `%lc' not "
					  "implemented, ignoring.\n",
					  (wint_t) cell->c);
				break;
			case 106: /* j */
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       ycenter,
					       xcenter - x + VTE_LINE_WIDTH,
					       VTE_LINE_WIDTH);
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       y,
					       VTE_LINE_WIDTH,
					       ycenter - y + VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 107: /* k */
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       ycenter,
					       xcenter - x + VTE_LINE_WIDTH,
					       VTE_LINE_WIDTH);
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       ycenter,
					       VTE_LINE_WIDTH,
					       ybottom - ycenter);
				drawn = TRUE;
				break;
			case 108: /* l */
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       ycenter,
					       xright - xcenter,
					       VTE_LINE_WIDTH);
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       ycenter,
					       VTE_LINE_WIDTH,
					       ybottom - ycenter);
				drawn = TRUE;
				break;
			case 109: /* m */
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       ycenter,
					       xright - xcenter,
					       VTE_LINE_WIDTH);
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       y,
					       VTE_LINE_WIDTH,
					       ycenter - y + VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 110: /* n */
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       y,
					       VTE_LINE_WIDTH,
					       height);
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       ycenter,
					       width,
					       VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 111: /* o */
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       y,
					       width,
					       VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 112: /* p */
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       (y + ycenter) / 2,
					       width,
					       VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 113: /* q */
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       ycenter,
					       width,
					       VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 114: /* r */
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       (ycenter + ybottom) / 2,
					       width,
					       VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 115: /* s */
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       ybottom,
					       width,
					       VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 116: /* t */
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       y,
					       VTE_LINE_WIDTH,
					       height);
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       ycenter,
					       xright - xcenter,
					       VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 117: /* u */
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       y,
					       VTE_LINE_WIDTH,
					       height);
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       ycenter,
					       xcenter - x + VTE_LINE_WIDTH,
					       VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 118: /* v */
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       y,
					       VTE_LINE_WIDTH,
					       ycenter - y + VTE_LINE_WIDTH);
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       ycenter,
					       width,
					       VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 119: /* w */
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       ycenter,
					       VTE_LINE_WIDTH,
					       ybottom - ycenter);
				XFillRectangle(display,
					       drawable,
					       gc,
					       x,
					       ycenter,
					       width,
					       VTE_LINE_WIDTH);
				drawn = TRUE;
				break;
			case 120: /* x */
				XFillRectangle(display,
					       drawable,
					       gc,
					       xcenter,
					       y,
					       VTE_LINE_WIDTH,
					       height);
				drawn = TRUE;
				break;
			default:
				break;
		}
	}

#if HAVE_XFT
	/* If we haven't drawn anything, try to draw the text using Xft. */
	if (!drawn && terminal->pvt->use_xft) {
		XftChar32 ftc;
		ftc = cell->c;
		XftDrawString32(ftdraw,
				&terminal->pvt->palette[fore].ftcolor,
				terminal->pvt->ftfont,
				x, y + ascent, &ftc, 1);
		drawn = TRUE;
	}
#endif

	/* If we haven't drawn anything, try to draw the text using Pango. */
	if (!drawn && terminal->pvt->use_pango) {
		attrlist = pango_attr_list_new();
		attr = pango_attr_foreground_new(terminal->pvt->palette[fore].red,
						 terminal->pvt->palette[fore].green,
						 terminal->pvt->palette[fore].blue);
		if (back != VTE_DEF_BG) {
			attr = pango_attr_background_new(terminal->pvt->palette[back].red,
							 terminal->pvt->palette[back].green,
							 terminal->pvt->palette[back].blue);
			pango_attr_list_insert(attrlist, attr);
		}
		g_unichar_to_utf8(cell->c, utf8_buf);
		pango_layout_set_text(layout, utf8_buf, -1);
		pango_layout_set_attributes(layout, attrlist);
		XSetForeground(display, gc, terminal->pvt->palette[fore].pixel);
		pango_x_render_layout(display,
				      drawable,
				      gc,
				      layout,
				      x,
				      y);
		pango_layout_set_attributes(layout, NULL);
		pango_attr_list_unref(attrlist);
		attrlist = NULL;
		drawn = TRUE;
	}

	/* If we haven't managed to draw anything yet, try to draw the text
	 * using Xlib. */
	if (!drawn) {
		/* Set the textitem's fields. */
		textitem.chars = &cell->c;
		textitem.nchars = 1;
		textitem.delta = 0;
		textitem.font_set = terminal->pvt->fontset;

		/* Draw the text.  We've handled bold,
		 * standout and reverse already, but we
		 * need to handle half, and maybe
		 * blink, if we decide to be evil. */
		XSetForeground(display, gc, terminal->pvt->palette[fore].pixel);
		XwcDrawText(display, drawable, gc, x, y + ascent, &textitem, 1);
		drawn = TRUE;
	}

	/* SFX */
	if (cell->underline) {
		XDrawLine(display, drawable, gc,
			  x, y + height - 1, x + width - 1, y + height - 1);
	}
}

/* Draw the widget. */
static void
vte_terminal_paint(GtkWidget *widget, GdkRectangle *area)
{
	VteTerminal *terminal = NULL;
	GtkSettings *settings = NULL;
	PangoLayout *layout = NULL;
	VteScreen *screen;
	Display *display;
	GdkDrawable *gdrawable;
	Drawable drawable;
	GdkColormap *gcolormap;
	Colormap colormap;
	GdkVisual *gvisual;
	Visual *visual;
	GdkGC *ggc;
	GC gc;
	struct vte_charcell *cell, im_cell;
	int row, drow, col, row_stop, col_stop, x_offs = 0, y_offs = 0, columns;
	char *preedit;
	long width, height, ascent, descent, delta;
	struct timezone tz;
	struct timeval tv;
	guint daytime;
	gint blink_cycle = 1000;
	int i, len;
	gboolean blink;
#ifdef HAVE_XFT
	XftDraw *ftdraw = NULL;
#endif

	/* Make a few sanity checks. */
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	g_return_if_fail(area != NULL);
	terminal = VTE_TERMINAL(widget);
	if (!GTK_WIDGET_DRAWABLE(widget)) {
		return;
	}
	screen = terminal->pvt->screen;

	/* Determine if blinking text should be shown. */
	if (terminal->pvt->cursor_blinks) {
		if (gettimeofday(&tv, &tz) == 0) {
			settings = gtk_widget_get_settings(widget);
			if (G_IS_OBJECT(settings)) {
				g_object_get(G_OBJECT(settings),
					     "gtk-cursor-blink-time",
					     &blink_cycle, NULL);
			}
			daytime = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
			if (daytime >= terminal->pvt->last_keypress_time) {
				daytime -= terminal->pvt->last_keypress_time;
			}
			daytime = daytime % blink_cycle;
			blink = daytime < (blink_cycle / 2);
		} else {
			blink = TRUE;
		}
	} else {
		blink = TRUE;
	}

	/* Get the X11 structures we need for the drawing area. */
	gdk_window_get_internal_paint_info(widget->window, &gdrawable,
					   &x_offs, &y_offs);
	display = gdk_x11_drawable_get_xdisplay(gdrawable);
	drawable = gdk_x11_drawable_get_xid(gdrawable);
	ggc = gdk_gc_new(gdrawable);
	gc = XCreateGC(display, drawable, 0, NULL);
	gcolormap = gdk_drawable_get_colormap(widget->window);
	colormap = gdk_x11_colormap_get_xcolormap(gcolormap);
	gvisual = gtk_widget_get_visual(widget);
	visual = gdk_x11_visual_get_xvisual(gvisual);

	/* Create a new pango layout in the correct font. */
	if (terminal->pvt->use_pango) {
		layout = terminal->pvt->layout;

		if (layout == NULL) {
			g_warning("Error allocating layout, disabling Pango.");
			terminal->pvt->use_pango = FALSE;
		}
	}

#ifdef HAVE_XFT
	/* Create a new XftDraw context. */
	if (terminal->pvt->use_xft) {
		ftdraw = XftDrawCreate(display, drawable, visual, colormap);
		if (ftdraw == NULL) {
			g_warning("Error allocating draw, disabling Xft.");
			terminal->pvt->use_xft = FALSE;
		}
	}
#endif

	/* Keep local copies of rendering information. */
	width = terminal->char_width;
	height = terminal->char_height;
	ascent = terminal->char_ascent;
	descent = terminal->char_descent;
	delta = screen->scroll_delta;

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
			columns = 1;
			if ((cell != NULL) && (cell->columns > 1)) {
				columns = cell->columns;
			}
#ifdef VTE_DEBUG
#ifdef VTE_DEBUG_DRAW
			if (cell != NULL) {
				if (cell->c > 256) {
					fprintf(stderr,
						"Drawing %5ld/%d at "
						"(r%d,c%d).\n",
						cell->c, cell->columns,
						drow, col);
				} else {
					fprintf(stderr,
						"Drawing %5ld/%d (`%c') at "
						"(r%d,c%d).\n",
						cell->c, cell->columns, cell->c,
						drow, col);
				}
			}
#endif
#endif
			/* Draw the character. */
			vte_terminal_draw_char(terminal, screen, cell,
					       col,
					       drow,
					       col * width - x_offs,
					       row * height - y_offs,
					       x_offs,
					       y_offs,
					       columns * width,
					       height,
					       ascent, descent,
					       display,
					       gdrawable, drawable,
					       gcolormap, colormap,
					       gvisual, visual,
					       ggc, gc,
					       layout,
#ifdef HAVE_XFT
					       ftdraw,
#endif
					       FALSE);
			if (cell == NULL) {
				/* Skip to the next column. */
				col++;
			} else {
				if (cell->columns != 0) {
					col += cell->columns;
				} else {
					col++;
				}
			}
		}
		row++;
	}

	/* Draw the cursor if it's visible. */
	if (terminal->pvt->screen->cursor_visible) {
		/* Get the character under the cursor. */
		col = screen->cursor_current.col;
		if (terminal->pvt->im_preedit != NULL) {
			preedit = terminal->pvt->im_preedit;
			for (i = 0; i < terminal->pvt->im_preedit_cursor; i++) {
				col += wcwidth(g_utf8_get_char(preedit));
				preedit = g_utf8_next_char(preedit);
			}
		}
		drow = screen->cursor_current.row;
		row = drow - delta;
		cell = vte_terminal_find_charcell(terminal, drow, col);
		columns = 1;
		if ((cell != NULL) && (cell->columns > 1)) {
			columns = cell->columns;
		}

		/* Draw the cursor. */
		delta = screen->scroll_delta;
		if (GTK_WIDGET_HAS_FOCUS(GTK_WIDGET(terminal))) {
			/* Draw it as a character, possibly reversed. */
#ifdef VTE_DEBUG
#ifdef VTE_DEBUG_DRAW_CURSOR
			fprintf(stderr, "Drawing the cursor (%s).\n",
				blink ? "on" : "off");
			if (cell != NULL) {
				if (cell->c > 256) {
					fprintf(stderr,
						"Drawing %5ld/%d at "
						"(r%d,c%d).\n",
						cell->c, cell->columns,
						drow, col);
				} else {
					fprintf(stderr,
						"Drawing %5ld/%d (`%c') at "
						"(r%d,c%d).\n",
						cell->c, cell->columns, cell->c,
						drow, col);
				}
			}
#endif
#endif
			vte_terminal_draw_char(terminal, screen, cell,
					       col,
					       drow,
					       col * width - x_offs,
					       row * height - y_offs,
					       x_offs,
					       y_offs,
					       columns * width,
					       height,
					       ascent, descent,
					       display,
					       gdrawable, drawable,
					       gcolormap, colormap,
					       gvisual, visual,
					       ggc, gc,
					       layout,
#ifdef HAVE_XFT
					       ftdraw,
#endif
					       blink);
		} else {
			/* Draw it as a rectangle. */
			guint fore;
			fore = cell ? cell->fore : VTE_DEF_FG;
			XSetForeground(display, gc,
				       terminal->pvt->palette[fore].pixel);
			XDrawRectangle(display, drawable, gc,
				       col * width - x_offs,
				       row * height - y_offs,
				       columns * width - 1,
				       height - 1);
		}
	}

	/* Draw the pre-edit string (if one exists) over the cursor. */
	if (terminal->pvt->im_preedit) {
		drow = screen->cursor_current.row;
		row = drow - delta;
		memset(&im_cell, 0, sizeof(im_cell));
		im_cell.fore = screen->defaults.fore;
		im_cell.back = screen->defaults.back;
		im_cell.columns = 1;

		/* If the pre-edit string won't fit on the screen, drop initial
		 * characters until it does. */
		preedit = terminal->pvt->im_preedit;
		len = g_utf8_strlen(preedit, -1);
		col = screen->cursor_current.col;
		while ((col + len) > terminal->column_count) {
			preedit = g_utf8_next_char(preedit);
		}
		len = g_utf8_strlen(preedit, -1);
		col = screen->cursor_current.col;

		/* Draw the preedit string, one character at a time.  Fill the
		 * background to prevent double-draws. */
		for (i = 0; i < len; i++) {
			im_cell.c = g_utf8_get_char(preedit);
#ifdef VTE_DEBUG
#ifdef VTE_DEBUG_DRAW
			fprintf(stderr, "Drawing preedit[%d] = %lc.\n",
				i, im_cell.c);
#endif
#endif
			XSetForeground(display, gc,
				       terminal->pvt->palette[im_cell.back].pixel);
			XFillRectangle(display,
				       drawable,
				       gc,
				       col * width - x_offs,
				       row * height - y_offs,
				       width,
				       height);
			vte_terminal_draw_char(terminal, screen, &im_cell,
					       col,
					       drow,
					       col * width - x_offs,
					       row * height - y_offs,
					       x_offs,
					       y_offs,
					       width,
					       height,
					       ascent, descent,
					       display,
					       gdrawable, drawable,
					       gcolormap, colormap,
					       gvisual, visual,
					       ggc, gc,
					       layout,
#ifdef HAVE_XFT
					       ftdraw,
#endif
					       FALSE);
			col += wcwidth(im_cell.c);
			preedit = g_utf8_next_char(preedit);
		}
		if (len > 0) {
			/* Draw a rectangle around the pre-edit string, to help
			 * distinguish it from other text. */
			XSetForeground(display, gc,
				       terminal->pvt->palette[im_cell.fore].pixel);
			XDrawRectangle(display,
				       drawable,
				       gc,
				       screen->cursor_current.col * width - x_offs,
				       row * height - y_offs,
				       (col - screen->cursor_current.col) * width - 1,
				       height - 1);
		}
	}

	/* Done with various structures. */
#ifdef HAVE_XFT
	if (ftdraw != NULL) {
		XftDrawDestroy(ftdraw);
	}
#endif
	g_object_unref(G_OBJECT(ggc));
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

static gboolean
vte_terminal_scroll(GtkWidget *widget, GdkEventScroll *event)
{
	GtkAdjustment *adj;
	gdouble new_value;

	adj = VTE_TERMINAL (widget)->adjustment;

	switch (event->direction) {
	case GDK_SCROLL_UP:
		new_value = adj->value - adj->page_increment / 2;
		break;
	case GDK_SCROLL_DOWN:
		new_value = adj->value + adj->page_increment / 2;
		break;
	default:
		return FALSE;
	}

	new_value = CLAMP (new_value, adj->lower, adj->upper - adj->page_size);
	gtk_adjustment_set_value (adj, new_value);

	return TRUE;
}

/* Create a new accessible object associated with ourselves, and return
 * it to the caller. */
static AtkObject *
vte_terminal_get_accessible(GtkWidget *widget)
{
	AtkObject *access;
	g_return_val_if_fail(VTE_IS_TERMINAL(widget), NULL);
	access = vte_terminal_accessible_new(VTE_TERMINAL(widget));
	return access;
}

/* Initialize methods. */
static void
vte_terminal_class_init(VteTerminalClass *klass, gconstpointer data)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	/* Override some of the default handlers. */
	gobject_class->finalize = vte_terminal_finalize;
	widget_class->realize = vte_terminal_realize;
	widget_class->scroll_event = vte_terminal_scroll;
	widget_class->expose_event = vte_terminal_expose;
	widget_class->key_press_event = vte_terminal_key_press;
	widget_class->button_press_event = vte_terminal_button_press;
	widget_class->button_release_event = vte_terminal_button_release;
	widget_class->motion_notify_event = vte_terminal_motion_notify;
	widget_class->focus_in_event = vte_terminal_focus_in;
	widget_class->focus_out_event = vte_terminal_focus_out;
	widget_class->unrealize = vte_terminal_unrealize;
	widget_class->size_request = vte_terminal_size_request;
	widget_class->size_allocate = vte_terminal_size_allocate;
	widget_class->get_accessible = vte_terminal_get_accessible;

	klass->eof_signal =
		g_signal_new("eof",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->window_title_changed_signal =
		g_signal_new("window-title-changed",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->icon_title_changed_signal =
		g_signal_new("icon-title-changed",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->char_size_changed_signal =
		g_signal_new("char-size-changed",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__UINT_UINT,
			     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
	klass->selection_changed_signal =
		g_signal_new ("selection-changed",
			      G_OBJECT_CLASS_TYPE(klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL,
			      NULL,
			      _vte_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	klass->contents_changed_signal =
		g_signal_new("contents-changed",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->cursor_moved_signal =
		g_signal_new("cursor-moved",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->deiconify_window_signal =
		g_signal_new("deiconify-window",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->iconify_window_signal =
		g_signal_new("iconify-window",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->raise_window_signal =
		g_signal_new("raise-window",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->lower_window_signal =
		g_signal_new("lower-window",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->refresh_window_signal =
		g_signal_new("refresh-window",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->restore_window_signal =
		g_signal_new("restore-window",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->maximize_window_signal =
		g_signal_new("maximize-window",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->resize_window_signal =
		g_signal_new("resize-window",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__UINT_UINT,
			     G_TYPE_NONE, 0);
	klass->move_window_signal =
		g_signal_new("move-window",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__UINT_UINT,
			     G_TYPE_NONE, 0);
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

/* External access functions. */
void
vte_terminal_set_audible_bell(VteTerminal *terminal, gboolean audible)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->audible_bell = audible;
}

void
vte_terminal_set_scroll_on_output(VteTerminal *terminal, gboolean scroll)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->scroll_on_output = scroll;
}

void
vte_terminal_set_scroll_on_keystroke(VteTerminal *terminal, gboolean scroll)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->scroll_on_keystroke = scroll;
}

void
vte_terminal_copy_clipboard(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_terminal_copy(terminal, GDK_SELECTION_CLIPBOARD);
}

void
vte_terminal_paste_clipboard(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_terminal_paste(terminal, GDK_SELECTION_CLIPBOARD);
}

/* Append the menu items for our input method context to the given shell. */
void
vte_terminal_im_append_menuitems(VteTerminal *terminal, GtkMenuShell *menushell)
{
	GtkIMMulticontext *context;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	context = GTK_IM_MULTICONTEXT(terminal->pvt->im_context);
	gtk_im_multicontext_append_menuitems(context, menushell);
}

/* Set up whatever background we want. */
static void
vte_terminal_setup_background(VteTerminal *terminal,
			      gboolean refresh_transparent)
{
	long i, pixel_count;
	GtkWidget *widget;
	guchar *pixels, *oldpixels;
	GdkColormap *colormap = NULL;
	GdkPixbuf *pixbuf = NULL;
	GdkPixmap *pixmap = NULL;
	GdkBitmap *bitmap = NULL;
	GdkColor bgcolor;
	GdkWindow *window = NULL;
	GdkAtom atom, prop_type;
	Pixmap *prop_data = NULL;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);
	if (!GTK_WIDGET_REALIZED(widget)) {
#ifdef VTE_DEBUG
		fprintf(stderr, "Can not set background image without window.\n");
#endif
		return;
	}

#ifdef VTE_DEBUG
	fprintf(stderr, "Setting up background image.\n");
#endif

	/* Set the default background color. */
	bgcolor.red = terminal->pvt->palette[VTE_DEF_BG].red;
	bgcolor.green = terminal->pvt->palette[VTE_DEF_BG].green;
	bgcolor.blue = terminal->pvt->palette[VTE_DEF_BG].blue;
	bgcolor.pixel = terminal->pvt->palette[VTE_DEF_BG].pixel;

	gdk_window_set_background(widget->window, &bgcolor);

	if (terminal->pvt->bg_transparent) {
#ifdef VTE_DEBUG
		fprintf(stderr, "Setting up background transparent.\n");
#endif
		/* If we don't have a root pixmap, try to fetch one regardless
		 * of what the caller told us to do. */
		if (terminal->pvt->bg_transparent_image == NULL) {
			refresh_transparent = TRUE;
		}
		/* If we need a new copy of the desktop, get it. */
		if (refresh_transparent) {
			guint width, height, pwidth, pheight;

#ifdef VTE_DEBUG
			fprintf(stderr, "Fetching new background pixmap.\n");
#endif
			gdk_error_trap_push();

			/* Retrieve the window and its property which
			 * we're watching. */
			window = terminal->pvt->bg_transparent_window;
			atom = terminal->pvt->bg_transparent_atom;
			prop_data = NULL;

			/* Read the pixmap property off of the window. */
			gdk_property_get(window, atom, 0, 0, 10, FALSE,
					 &prop_type, NULL, NULL,
					 (guchar**)&prop_data);

			/* If we got something, try to create a pixmap we
			 * can mess with. */
			pixbuf = NULL;
			if ((prop_type == GDK_TARGET_PIXMAP) &&
			    (prop_data != NULL) &&
			    (prop_data[0] != 0)) {
				/* Create a pixmap from the window we're
				 * watching, which is foreign because we
				 * didn't create it. */
				gdk_drawable_get_size(window, &width, &height);
				pixmap = gdk_pixmap_foreign_new(prop_data[0]);
				/* If we got a pixmap, create a pixbuf for
				 * us to work with. */
				if (GDK_IS_PIXMAP(pixmap)) {
					gdk_drawable_get_size(GDK_DRAWABLE(pixmap), &pwidth, &pheight);
					colormap = gdk_drawable_get_colormap(window);
					pixbuf = gdk_pixbuf_get_from_drawable(NULL,
									      pixmap,
									      colormap,
									      0, 0,
									      0, 0,
									      MIN(width, pwidth),
									      MIN(pheight, height));
					/* Get rid of the pixmap. */
					g_object_unref(G_OBJECT(pixmap));
					pixmap = NULL;
				}
			}
			gdk_error_trap_pop();

			/* Get rid of any previous snapshot we've got, and
			 * save the new one in its place. */
			if (GDK_IS_PIXBUF(terminal->pvt->bg_transparent_image)) {
				g_object_unref(G_OBJECT(terminal->pvt->bg_transparent_image));
			}
			terminal->pvt->bg_transparent_image = pixbuf;
			pixbuf = NULL;
		}

		/* Get a copy of the root image which we can manipulate. */
		if (GDK_IS_PIXBUF(terminal->pvt->bg_transparent_image)) {
			pixbuf = gdk_pixbuf_copy(terminal->pvt->bg_transparent_image);
		}

		/* Rotate the copy of the image left or up to compensate for
		 * our window not having the same origin. */
		if (GDK_IS_PIXBUF(pixbuf)) {
			guint width, height;
			gint x, y;

			width = gdk_pixbuf_get_width(pixbuf);
			height = gdk_pixbuf_get_height(pixbuf);

			/* Determine how far we should shift the origin. */
			gdk_window_get_origin(widget->window, &x, &y);
			while (x < 0) {
				x += width;
			}
			while (y < 0) {
				y += height;
			}
			x %= width;
			y %= height;

			/* Copy the picture data. */
			oldpixels = gdk_pixbuf_get_pixels(pixbuf);
			pixels = g_malloc(height *
					  gdk_pixbuf_get_rowstride(pixbuf) *
					  2);
			memcpy(pixels,
			       oldpixels,
			       gdk_pixbuf_get_rowstride(pixbuf) * height);
			memcpy(pixels +
			       gdk_pixbuf_get_rowstride(pixbuf) * height,
			       oldpixels,
			       gdk_pixbuf_get_rowstride(pixbuf) * height);
			memcpy(oldpixels,
			       pixels + gdk_pixbuf_get_rowstride(pixbuf) * y +
			       (gdk_pixbuf_get_bits_per_sample(pixbuf) *
				gdk_pixbuf_get_n_channels(pixbuf) * x) / 8,
			       gdk_pixbuf_get_rowstride(pixbuf) * height);
			g_free(pixels);
		}
	} else
	if (GDK_IS_PIXBUF(terminal->pvt->bg_image)) {
		/* If we need to desaturate the image, create a copy we can
		 * safely modify.  Otherwise just ref the one we were passed. */
#ifdef VTE_DEBUG
		fprintf(stderr, "Applying new background pixbuf.\n");
#endif
		if (terminal->pvt->bg_saturation != VTE_SATURATION_MAX) {
			pixbuf = gdk_pixbuf_copy(terminal->pvt->bg_image);
		} else {
			pixbuf = terminal->pvt->bg_image;
			g_object_ref(G_OBJECT(pixbuf));
		}
	}

	/* Desaturate the image if we need to. */
	if (GDK_IS_PIXBUF(pixbuf)) {
#ifdef VTE_DEBUG
		fprintf(stderr, "Desaturating background.\n");
#endif
		if (terminal->pvt->bg_saturation != VTE_SATURATION_MAX) {
			pixels = gdk_pixbuf_get_pixels(pixbuf);
			pixel_count = gdk_pixbuf_get_height(pixbuf) *
				      gdk_pixbuf_get_rowstride(pixbuf);
			for (i = 0; i < pixel_count; i++) {
				pixels[i] = pixels[i]
					    * terminal->pvt->bg_saturation
					    / VTE_SATURATION_MAX;
			}
		}
	}

	if (GDK_IS_PIXBUF(pixbuf)) {
#ifdef VTE_DEBUG
		fprintf(stderr, "Setting final background.\n");
#endif
		/* Render the modified image into a pixmap/bitmap pair. */
		colormap = gdk_drawable_get_colormap(widget->window);
		gdk_pixbuf_render_pixmap_and_mask_for_colormap(pixbuf,
							       colormap,
							       &pixmap,
							       &bitmap,
							       0);

		/* Get rid of the pixbuf, which is no longer useful. */
		g_object_unref(G_OBJECT(pixbuf));
		pixbuf = NULL;

		/* Set the pixmap as the window background, and then get unref
		 * it (the drawable should keep a ref). */
		if (GDK_IS_PIXMAP(pixmap)) {
			/* Set the pixmap as the window background. */
			gdk_window_set_back_pixmap(widget->window,
						   pixmap,
						   FALSE);
			g_object_unref(G_OBJECT(pixmap));
			pixmap = NULL;
		}
		if (GDK_IS_DRAWABLE(bitmap)) {
			g_object_unref(G_OBJECT(bitmap));
			bitmap = NULL;
		}
	}

	/* Force a redraw for everything. */
	vte_invalidate_cells(terminal,
			     0,
			     terminal->column_count,
			     terminal->pvt->screen->scroll_delta,
			     terminal->row_count);
}

void
vte_terminal_set_background_saturation(VteTerminal *terminal, double saturation)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->bg_saturation = saturation * VTE_SATURATION_MAX;
#ifdef VTE_DEBUG
	fprintf(stderr, "Setting background saturation to %ld/%ld.\n",
		terminal->pvt->bg_saturation, (long) VTE_SATURATION_MAX);
#endif
	vte_terminal_queue_background_update(terminal);
}

/* Set up the background, grabbing a new copy of the transparency background,
 * if possible. */
static gboolean
vte_terminal_update_transparent(gpointer data)
{
	VteTerminal *terminal;
	g_return_val_if_fail(VTE_IS_TERMINAL(data), FALSE);
	terminal = VTE_TERMINAL(data);
	if (terminal->pvt->bg_transparent_update_pending == FALSE) {
		return FALSE;
	}
	vte_terminal_setup_background(terminal, TRUE);
	terminal->pvt->bg_transparent_update_pending = FALSE;
	terminal->pvt->bg_transparent_update_tag = -1;
	return FALSE;
}

/* Queue an update of the background image, to be done as soon as we can
 * get to it.  Just bail if there's already an update pending, so that if
 * opaque move tries to screw us, we don't end up with an insane backlog
 * of updates after the user finishes moving us. */
static void
vte_terminal_queue_background_update(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->bg_transparent_update_pending = TRUE;
	terminal->pvt->bg_transparent_update_tag = g_idle_add(vte_terminal_update_transparent,
								      terminal);
}

/* Watch for property change events. */
static GdkFilterReturn
vte_terminal_filter_property_changes(GdkXEvent *xevent, GdkEvent *event,
				     gpointer data)
{
	VteTerminal *terminal;
	GdkWindow *window;
	XEvent *xev;
	GdkAtom atom;

	xev = (XEvent*) xevent;

	/* If we aren't realized, then we have no need for this information. */
	if (VTE_IS_TERMINAL(data) && GTK_WIDGET_REALIZED(GTK_WIDGET(data))) {
		terminal = VTE_TERMINAL(data);
	} else {
		return GDK_FILTER_CONTINUE;
	}

	/* We only care about property changes to the pixmap ID on the root
	 * window, so we should ignore everything else. */
	switch (xev->type) {
	case PropertyNotify:
#ifdef VTE_DEBUG
		fprintf(stderr, "Property changed.\n");
#endif
		atom = terminal->pvt->bg_transparent_atom;
		if (xev->xproperty.atom == gdk_x11_atom_to_xatom(atom)) {
#ifdef VTE_DEBUG
			fprintf(stderr, "Property atom matches.\n");
#endif
			window = terminal->pvt->bg_transparent_window;
			if (xev->xproperty.window == GDK_DRAWABLE_XID(window)) {
#ifdef VTE_DEBUG
				fprintf(stderr, "Property window matches.\n");
#endif
				/* The attribute we care about changed in the
				 * window we care about, so update the
				 * background image to the new snapshot. */
				vte_terminal_queue_background_update(terminal);
			}
		}
	}
	return GDK_FILTER_CONTINUE;
}

/* Turn background "transparency" on or off. */
void
vte_terminal_set_background_transparent(VteTerminal *terminal, gboolean setting)
{
	GdkWindow *window;
	GdkAtom atom;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
#ifdef VTE_DEBUG
	fprintf(stderr, "Turning background transparency %s.\n",
		setting ? "on" : "off");
#endif
	terminal->pvt->bg_transparent = setting;

	/* To be "transparent", we treat the _XROOTPMAP_ID attribute of the
	 * root window as a picture of what's beneath us, and use that as
	 * the background.  It's a little tricky because we need to "scroll"
	 * the image to match our window position. */
	window = gdk_get_default_root_window();
	if (setting) {
		/* Get the window and property name we'll be watching for
		 * changes in. */
		atom = gdk_atom_intern("_XROOTPMAP_ID", TRUE);
		terminal->pvt->bg_transparent_window = window;
		terminal->pvt->bg_transparent_atom = atom;
		/* Add a filter to watch for this property changing. */
		gdk_window_add_filter(window,
				      vte_terminal_filter_property_changes,
				      terminal);
		gdk_window_set_events(window,
				      gdk_window_get_events(window) |
				      GDK_PROPERTY_CHANGE_MASK);
		/* Remove a background image, if we have one. */
		if (GDK_IS_PIXBUF(terminal->pvt->bg_image)) {
			g_object_unref(G_OBJECT(terminal->pvt->bg_image));
			terminal->pvt->bg_image = NULL;
		}
	} else {
		/* Remove the watch filter in case it was added before. */
		gdk_window_remove_filter(window,
					 vte_terminal_filter_property_changes,
					 terminal);
	}
	/* Update the background. */
	vte_terminal_queue_background_update(terminal);
}

void
vte_terminal_set_background_image(VteTerminal *terminal, GdkPixbuf *image)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

#ifdef VTE_DEBUG
	fprintf(stderr, "%s background image.\n",
		GDK_IS_PIXBUF(image) ? "Setting" : "Clearing");
#endif

	/* Get a ref to the new image if there is one.  Do it here just in
	 * case we're actually given the same one we're already using. */
	if (GDK_IS_PIXBUF(image)) {
		g_object_ref(G_OBJECT(image));
	}

	/* Unref the previous background image. */
	if (GDK_IS_PIXBUF(terminal->pvt->bg_image)) {
		g_object_unref(G_OBJECT(terminal->pvt->bg_image));
	}

	/* Set the new background. */
	terminal->pvt->bg_image = image;

	/* Turn off transparency and finish setting things up. */
	if (terminal->pvt->bg_transparent) {
		vte_terminal_set_background_transparent(terminal, FALSE);
	}
	vte_terminal_queue_background_update(terminal);
}

/* Set the background image using just a file.  It's more efficient for a
 * caller to pass us an already-desaturated pixbuf if we've got multiple
 * instances going, but this is handy for the single-widget case. */
void
vte_terminal_set_background_image_file(VteTerminal *terminal, const char *path)
{
	GdkPixbuf *image;
	GError *error = NULL;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(path != NULL);
	g_return_if_fail(strlen(path) > 0);
#ifdef VTE_DEBUG
	fprintf(stderr, "Loading background image from `%s'.\n", path);
#endif
	image = gdk_pixbuf_new_from_file(path, &error);
	if ((image != NULL) && (error == NULL)) {
		vte_terminal_set_background_image(terminal, image);
		g_object_unref(G_OBJECT(image));
	} else {
		/* Set "no image" as the background. */
		vte_terminal_set_background_image(terminal, NULL);
		/* FIXME: do something better with the error. */
		g_error_free(error);
	}
}

/* Check if we're the current owner of the clipboard. */
gboolean
vte_terminal_get_has_selection(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->has_selection;
}

/* Tell the caller if we're [planning on] using Xft for rendering. */
gboolean
vte_terminal_get_using_xft(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->use_xft;
}

/* Toggle the cursor blink setting. */
void
vte_terminal_set_cursor_blinks(VteTerminal *terminal, gboolean blink)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->cursor_blinks = blink;
}

/* Set the length of the scrollback buffers. */
void
vte_terminal_set_scrollback_lines(VteTerminal *terminal, long lines)
{
	long old_delta = 0, new_delta = 0, delta;
	VteScreen *screens[2];
	int i;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	/* If we're being asked to resize to the same size, just save ourselves
	 * the trouble, nod our heads, and smile. */
	if ((terminal->pvt->scrollback_lines != 0) &&
	    (terminal->pvt->scrollback_lines == lines)) {
		return;
	}

	/* We need to resize both scrollback buffers, and this beats copying
	 * and pasting the same code twice. */
	screens[0] = &terminal->pvt->normal_screen;
	screens[1] = &terminal->pvt->alternate_screen;
	for (i = 0; i < G_N_ELEMENTS(screens); i++) {
		/* Resize the buffers, but keep track of where the last data
		 * in the buffer is so that we can compensate for it being
		 * moved.  We track the end of the data instead of the start
		 * so that the visible portion of the buffer doesn't change. */
		old_delta = 0;
		if (screens[i]->row_data != NULL) {
			old_delta = vte_ring_next(screens[i]->row_data);
		}
		vte_terminal_reset_rowdata(&screens[i]->row_data, lines);
		new_delta = vte_ring_next(screens[i]->row_data);
		delta = (new_delta - old_delta);
		screens[i]->cursor_current.row += delta;
		screens[i]->cursor_saved.row += delta;
		screens[i]->scroll_delta += delta;
		screens[i]->insert_delta += delta;
	}

	/* Adjust the scrollbars to the new locations. */
	vte_terminal_adjust_adjustments(terminal);
	vte_invalidate_cells(terminal,
			     0, terminal->column_count,
			     0, terminal->row_count);
}

/* Get a snapshot of what's in the visible part of the window. */
VteTerminalSnapshot *
vte_terminal_get_snapshot(VteTerminal *terminal)
{
	VteTerminalSnapshot *ret;
	int row, column, x;
	struct vte_charcell *cell;
	int fore, back;

	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);

	ret = g_malloc0(sizeof(VteTerminalSnapshot));

	/* Save the cursor position and visibility. */
	ret->cursor.x = terminal->pvt->screen->cursor_current.col;
	ret->cursor.y = terminal->pvt->screen->cursor_current.row -
			terminal->pvt->screen->insert_delta;
	ret->cursor_visible = terminal->pvt->screen->cursor_visible;

	/* Save the window size. */
	ret->rows = terminal->row_count;
	ret->columns = terminal->column_count;

	/* Save the window contents. */
	ret->contents = g_malloc0(sizeof(struct VteTerminalSnapshotCell*) *
				  (ret->rows + 1));
	for (row = 0; row < ret->rows; row++) {
		ret->contents[row] = g_malloc0(sizeof(struct VteTerminalSnapshotCell) *
					       (ret->columns + 1));
		column = x = 0;
		while (column < ret->columns) {
			cell = vte_terminal_find_charcell(terminal,
							  row + terminal->pvt->screen->scroll_delta,
							  x++);
			if (cell == NULL) {
				break;
			}
			if (cell->columns == 0) {
				continue;
			}

			/* Get the text. FIXME: convert from wchar_t to
			 * gunichar when they're not interchangeable. */
#ifdef VTE_DEBUG
			fprintf(stderr, "%lc", (wint_t) cell->c);
#endif
			ret->contents[row][column].c = cell->c;

			/* Get text attributes which aren't represented as
			 * colors. */
			ret->contents[row][column].attributes.underline =
				cell->underline;
			ret->contents[row][column].attributes.alternate =
				cell->alternate;

			/* Get text colors. */
			vte_terminal_determine_colors(terminal, cell, FALSE,
						      &fore, &back);

			ret->contents[row][column].attributes.foreground.red =
				terminal->pvt->palette[fore].red;
			ret->contents[row][column].attributes.foreground.green =
				terminal->pvt->palette[fore].green;
			ret->contents[row][column].attributes.foreground.blue =
				terminal->pvt->palette[fore].blue;

			ret->contents[row][column].attributes.background.red =
				terminal->pvt->palette[back].red;
			ret->contents[row][column].attributes.background.green =
				terminal->pvt->palette[back].green;
			ret->contents[row][column].attributes.background.blue =
				terminal->pvt->palette[back].blue;

			column++;
		}
	}
	ret->contents[row] = NULL;

	return ret;
}

void
vte_terminal_free_snapshot(VteTerminalSnapshot *snapshot)
{
	int row;
	g_return_if_fail(snapshot != NULL);
	for (row = 0; snapshot->contents[row] != NULL; row++) {
		memset(snapshot->contents[row], 0,
		       sizeof(snapshot->contents[row][0]) * snapshot->columns);
		g_free(snapshot->contents[row]);
	}
	g_free(snapshot->contents);
	memset(snapshot, 0, sizeof(*snapshot));
	g_free(snapshot);
}

/* Set the list of characters we consider to be parts of words.  Everything
 * else will be a non-word character, and we'll use transitions between the
 * two sets when doing selection-by-words. */
void
vte_terminal_set_word_chars(VteTerminal *terminal, const char *spec)
{
	iconv_t conv;
	wchar_t *wbuf;
	char *ibuf, *ibufptr, *obuf, *obufptr;
	size_t ilen, olen;
	VteWordCharRange range;
	int i;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* Allocate a new range array. */
	if (terminal->pvt->word_chars != NULL) {
		g_array_free(terminal->pvt->word_chars, FALSE);
	}
	terminal->pvt->word_chars = g_array_new(FALSE, TRUE,
						sizeof(VteWordCharRange));
	/* Convert the spec from UTF-8 to a string of wchar_t. */
	conv = iconv_open("WCHAR_T", "UTF-8");
	if (conv == NULL) {
		/* Aaargh.  We're screwed. */
		g_warning("iconv_open() failed setting word characters");
		return;
	}
	ilen = strlen(spec);
	ibuf = ibufptr = g_strdup(spec);
	olen = (ilen + 1) * sizeof(wchar_t);
	obuf = obufptr = g_malloc0(sizeof(wchar_t) * (strlen(spec) + 1));
	wbuf = (wchar_t*) obuf;
	wbuf[ilen] = '\0';
	iconv(conv, &ibuf, &ilen, &obuf, &olen);
	for (i = 0; i < ((obuf - obufptr) / sizeof(wchar_t)); i++) {
		/* The hyphen character. */
		if (wbuf[i] == '-') {
			range.start = wbuf[i];
			range.end = wbuf[i];
			g_array_append_val(terminal->pvt->word_chars, range);
#ifdef VTE_DEBUG
			fprintf(stderr, "Word charset includes hyphen.\n");
#endif
			continue;
		}
		/* A single character, not the start of a range. */
		if ((wbuf[i] != '-') && (wbuf[i + 1] != '-')) {
			range.start = wbuf[i];
			range.end = wbuf[i];
			g_array_append_val(terminal->pvt->word_chars, range);
#ifdef VTE_DEBUG
			fprintf(stderr, "Word charset includes `%lc'.\n",
				(wint_t) wbuf[i]);
#endif
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
#ifdef VTE_DEBUG
			fprintf(stderr, "Word charset includes range from "
				"`%lc' to `%lc'.\n", (wint_t) wbuf[i],
				(wint_t) wbuf[i + 2]);
#endif
			i += 2;
			continue;
		}
	}
	g_free(ibufptr);
	g_free(obufptr);
}

void
vte_terminal_set_backspace_binding(VteTerminal *terminal,
				   VteTerminalEraseBinding binding)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* FIXME: should we set the pty mode to match? */
	terminal->pvt->backspace_binding = binding;
}

void
vte_terminal_set_delete_binding(VteTerminal *terminal,
				VteTerminalEraseBinding binding)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->delete_binding = binding;
}

void
vte_terminal_set_mouse_autohide(VteTerminal *terminal, gboolean setting)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->mouse_autohide = setting;
}

gboolean
vte_terminal_get_mouse_autohide(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->mouse_autohide;
}
