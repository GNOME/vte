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
#include <math.h>
#include <pwd.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangox.h>
#include "buffer.h"
#include "caps.h"
#include "debug.h"
#include "iso2022.h"
#include "keymap.h"
#include "marshal.h"
#include "pty.h"
#include "reaper.h"
#include "ring.h"
#include "termcap.h"
#include "table.h"
#include "vte.h"
#include "vteaccess.h"
#include <X11/Xlib.h>
#ifdef HAVE_XFT
#include <X11/Xft/Xft.h>
#ifdef HAVE_XFT2
#include <fontconfig/fontconfig.h>
#endif
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifndef HAVE_WINT_T
typedef gunichar wint_t;
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext(PACKAGE, String)
#else
#define _(String) String
#define bindtextdomain(package,dir)
#endif

#define VTE_PAD_WIDTH			1
#define VTE_TAB_WIDTH			8
#define VTE_LINE_WIDTH			1
#define VTE_COLOR_SET_SIZE		8
#define VTE_COLOR_PLAIN_OFFSET		0
#define VTE_COLOR_BRIGHT_OFFSET		8
#define VTE_COLOR_DIM_OFFSET            16
#define VTE_DEF_FG			24
#define VTE_DEF_BG			25
#define VTE_BOLD_FG			26
#define VTE_DIM_FG                      27
#define VTE_SATURATION_MAX		10000
#define VTE_SCROLLBACK_MIN		100
#define VTE_DEFAULT_EMULATION		"xterm"
#define VTE_DEFAULT_CURSOR		GDK_XTERM
#define VTE_MOUSING_CURSOR		GDK_LEFT_PTR
#define VTE_DRAW_MAX_LENGTH		88
#define VTE_TAB_MAX			999
#define VTE_X_FIXED			"-*-fixed-medium-r-normal-*-20-*"
#define VTE_REPRESENTATIVE_CHARACTERS	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
					"abcdefgjijklmnopqrstuvwxyz" \
					"0123456789./+@"
#define VTE_REPRESENTATIVE_WIDER_CHARACTER	'M'
#define VTE_REPRESENTATIVE_NARROWER_CHARACTER	'l'
#define VTE_ADJUSTMENT_PRIORITY		G_PRIORITY_DEFAULT_IDLE
#define VTE_INPUT_RETRY_PRIORITY	G_PRIORITY_HIGH
#define VTE_INPUT_PRIORITY		G_PRIORITY_DEFAULT_IDLE
#define VTE_CHILD_INPUT_PRIORITY	G_PRIORITY_DEFAULT_IDLE
#define VTE_CHILD_OUTPUT_PRIORITY	G_PRIORITY_HIGH
#define VTE_FX_PRIORITY			G_PRIORITY_DEFAULT_IDLE
#define VTE_REGCOMP_FLAGS		REG_EXTENDED
#define VTE_REGEXEC_FLAGS		0
#define VTE_INPUT_CHUNK_SIZE		0x1000

/* The structure we use to hold characters we're supposed to display -- this
 * includes any supported visible attributes. */
struct vte_charcell {
	gunichar c;		/* The Unicode character. */
	guint32 columns: 11;	/* Number of visible columns (as determined
				   by g_unicode_iswide(c)).  Use as many bits
				   as possible without making this structure
				   grow any larger. */
	guint32 fragment: 1;	/* The nth fragment of a wide character. */
	guint32 fore: 5;	/* Indices in the color palette for the */
	guint32 back: 5;	/* foreground and background of the cell. */
	guint32 standout: 1;	/* Single-bit attributes. */
	guint32 underline: 1;
	guint32 strikethrough: 1;
	guint32 reverse: 1;
	guint32 blink: 1;
	guint32 half: 1;
	guint32 bold: 1;
	guint32 invisible: 1;
	guint32 protect: 1;
	guint32 alternate: 1;
};

/* A match regex, with a tag. */
struct vte_match_regex {
	regex_t reg;
	gint tag;
};

/* A drawing request record, for simplicity. */
struct vte_draw_item {
	gunichar c;
	guint16 columns;
	guint16 xpad;
};

/* The terminal's keypad/cursor state.  A terminal can either be using the
 * normal keypad, or the "application" keypad. */
typedef enum _VteKeymode {
	VTE_KEYMODE_NORMAL,
	VTE_KEYMODE_APPLICATION
} VteKeymode;

typedef struct _VteScreen VteScreen;

typedef struct _VteWordCharRange {
	gunichar start, end;
} VteWordCharRange;

/* Terminal private data. */
struct _VteTerminalPrivate {
	/* Emulation setup data. */
	struct _vte_termcap *termcap;	/* termcap storage */
	struct _vte_table *table;		/* control sequence table */
	const char *termcap_path;	/* path to termcap file */
	const char *emulation;		/* terminal type to emulate */
	GTree *sequences;		/* sequence handlers, keyed by GQuark
					   based on the sequence name */
	struct vte_terminal_flags {	/* boolean termcap flags */
		gboolean am;
		gboolean bw;
		gboolean ul;
	} flags;
	int keypad_mode, cursor_mode;	/* these would be VteKeymodes, but we
					   need to guarantee its type */
	gboolean sun_fkey_mode;
	gboolean hp_fkey_mode;
	gboolean legacy_fkey_mode;
	gboolean vt220_fkey_mode;
	int fkey;			/* this would be a VteFKey, but we
					   need to guarantee its type */
	GHashTable *dec_saved;
	int default_column_count, default_row_count;	/* default sizes */
	GTree *unichar_wc_map;		/* mapping between gunichars and wide
					   characters */

	/* PTY handling data. */
	const char *shell;		/* shell we started */
	int pty_master;			/* pty master descriptor */
	GIOChannel *pty_input;		/* master input watch */
	guint pty_input_source;
	GIOChannel *pty_output;		/* master output watch */
	guint pty_output_source;
	pid_t pty_pid;			/* pid of child using pty slave */

	/* Input data queues. */
	const char *encoding;		/* the pty's encoding */
	struct _vte_iso2022 *substitutions;
	GIConv incoming_conv;		/* narrow/unichar conversion state */
	struct _vte_buffer *incoming;	/* pending output characters */
	gboolean processing;
	gint processing_tag;

	/* Output data queue. */
	struct _vte_buffer *outgoing;	/* pending input characters */
	GIConv outgoing_conv_wide;
	GIConv outgoing_conv_utf8;

	/* IConv buffer. */
	struct _vte_buffer *conv_buffer;

	/* Screen data.  We support the normal screen, and an alternate
	 * screen, which seems to be a DEC-specific feature. */
	struct _VteScreen {
		VteRing *row_data;	/* row data, arranged as a GArray of
					   vte_charcell structures */
		struct vte_cursor_position {
			long row, col;
		} cursor_current, cursor_saved;
					/* the current and saved positions of
					   the [insertion] cursor -- current is
					   absolute, saved is relative to the
					   insertion delta */
		gboolean reverse_mode;	/* reverse mode */
		gboolean origin_mode;	/* origin mode */
		gboolean insert_mode;	/* insert mode */
		struct vte_scrolling_region {
			int start, end;
		} scrolling_region;	/* the region we scroll in */
		gboolean scrolling_restricted;
		long scroll_delta;	/* scroll offset */
		long insert_delta;	/* insertion offset */
		struct vte_charcell defaults;	/* default characteristics
						   for insertion of any new
						   characters */
		struct vte_charcell color_defaults;	/* original defaults
							   plus the current
							   fore/back */
		struct vte_charcell fill_defaults;	/* original defaults
							   plust the current
							   fore/back with no
							   character data */
		struct vte_charcell basic_defaults;	/* original defaults */
		gboolean status_line;
		GString *status_line_contents;
	} normal_screen, alternate_screen, *screen;

	/* Selection information. */
	GArray *word_chars;
	gboolean has_selection;
	gboolean selecting;
	gboolean selecting_restart;
	gboolean selecting_had_delta;
	char *selection;
	enum vte_selection_type {
		selection_type_char,
		selection_type_word,
		selection_type_line
	} selection_type;
	struct selection_event_coords {
		double x, y;
	} selection_origin, selection_last, selection_restart_origin;
	struct selection_cell_coords {
		long x, y;
	} selection_start, selection_end;

	/* Miscellaneous options. */
	VteTerminalEraseBinding backspace_binding, delete_binding;
	gboolean meta_sends_escape;
	gboolean audible_bell;
	gboolean visible_bell;
	gboolean margin_bell;
	guint bell_margin;
	gboolean allow_bold;
	gboolean nrc_mode;
	gboolean smooth_scroll;
	GHashTable *tabstops;

	/* Scrolling options. */
	gboolean scroll_on_output;
	gboolean scroll_on_keystroke;
	long scrollback_lines;

	/* Cursor blinking. */
	int cursor_force_fg;
	gboolean cursor_blinks;
	gint cursor_blink_tag;
	gint cursor_blink_timeout;
	gboolean cursor_visible;

	/* Input device options. */
	guint last_keypress_time;
	gboolean mouse_send_xy_on_click;
	gboolean mouse_send_xy_on_button;
	gboolean mouse_hilite_tracking;
	gboolean mouse_cell_motion_tracking;
	gboolean mouse_all_motion_tracking;
	guint mouse_last_button;
	gdouble mouse_last_x, mouse_last_y;
	gboolean mouse_autohide;
	guint mouse_autoscroll_tag;

	/* State variables for handling match checks. */
	char *match_contents;
	GArray *match_attributes;
	GArray *match_regexes;
	struct {
		long row, column;
	} match_start, match_end;

	/* Data used when rendering the text which does not require server
	 * resources and which can be kept after unrealizing. */
	PangoFontDescription *fontdesc;

	/* GtkSettings that we are monitoring. */
	GtkSettings *connected_settings;

	/* Data used when rendering the text which reflects server resources
	 * and data, which should be dropped when unrealizing and (re)created
	 * when realizing. */
	PangoContext *pcontext;
	XFontSet fontset;
	GTree *fontpaddingl, *fontpaddingr;
	enum VteRenderMethod {
		VteRenderXlib = 0,
		VteRenderPangoX = 1,
		VteRenderPango = 2,
#ifdef HAVE_XFT
		VteRenderXft1 = 3,
		VteRenderXft2 = 4,
#endif
	} render_method;
#ifdef HAVE_XFT
	XftFont *ftfont;
#endif
	gboolean palette_initialized;
	struct vte_palette_entry {
		guint16 red, green, blue;
		unsigned long pixel;
#ifdef HAVE_XFT
		XRenderColor rcolor;
		XftColor ftcolor;
		gboolean ftcolor_allocated;
#endif
	} palette[VTE_DIM_FG + 1];
	XwcTextItem xlib_textitem[VTE_DRAW_MAX_LENGTH];
	wchar_t xlib_wcitem[VTE_DRAW_MAX_LENGTH];
#ifdef HAVE_XFT2
	XftCharSpec xft_textitem[VTE_DRAW_MAX_LENGTH];
#endif

	/* Mouse cursors. */
	GdkCursor *mouse_default_cursor,
		  *mouse_mousing_cursor,
		  *mouse_inviso_cursor;

	/* Input method support. */
	GtkIMContext *im_context;
	char *im_preedit;
	int im_preedit_cursor;

	/* Our accessible peer. */
	AtkObject *accessible;

	/* Adjustment updates pending. */
	gboolean adjustment_changed_tag;

	/* Background images/"transparency". */
	gboolean bg_transparent;
	gboolean bg_transparent_update_pending;
	guint bg_transparent_update_tag;
	GdkAtom bg_transparent_atom;
	GdkWindow *bg_transparent_window;
	GdkPixbuf *bg_transparent_image;
	GdkPixbuf *bg_image;
	long bg_saturation;	/* out of VTE_SATURATION_MAX */
};

/* A function which can handle a terminal control sequence. */
typedef void (*VteTerminalSequenceHandler)(VteTerminal *terminal,
					   const char *match,
					   GQuark match_quark,
					   GValueArray *params);
static void vte_terminal_set_termcap(VteTerminal *terminal, const char *path,
				     gboolean reset);
static void vte_terminal_setup_background(VteTerminal *terminal,
					  gboolean refresh_transparent);
static void vte_terminal_ensure_cursor(VteTerminal *terminal, gboolean current);
static void vte_terminal_paste(VteTerminal *terminal, GdkAtom board);
static void vte_terminal_insert_char(VteTerminal *terminal, gunichar c,
				     gboolean force_insert_mode,
				     gboolean invalidate_cells,
				     gboolean paint_cells,
				     gint forced_width);
static void vte_sequence_handler_clear_screen(VteTerminal *terminal,
					      const char *match,
					      GQuark match_quark,
					      GValueArray *params);
static void vte_sequence_handler_do(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_DO(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_ho(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_le(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_LE(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_nd(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_sf(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_sr(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_ue(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_up(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_UP(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_us(VteTerminal *terminal,
				    const char *match,
				    GQuark match_quark,
				    GValueArray *params);
static void vte_sequence_handler_vb(VteTerminal *terminal,
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
static void vte_terminal_match_hilite_clear(VteTerminal *terminal);
static void vte_terminal_queue_background_update(VteTerminal *terminal);
static void vte_terminal_queue_adjustment_changed(VteTerminal *terminal);

/* Free a no-longer-used row data array. */
static void
vte_free_row_data(gpointer freeing, gpointer data)
{
	if (freeing) {
		g_array_free((GArray*)freeing, TRUE);
	}
}

/* Append a single item to a GArray a given number of times. Centralizing all
 * of the places we do this may let me do something more clever later. */
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

/* Allocate a new line. */
static GArray *
vte_new_row_data(VteTerminal *terminal)
{
	return g_array_new(FALSE, FALSE, sizeof(struct vte_charcell));
}

/* Allocate a new line of a given size. */
static GArray *
vte_new_row_data_sized(VteTerminal *terminal, gboolean fill)
{
	GArray *row;
	row = g_array_sized_new(FALSE, FALSE,
				sizeof(struct vte_charcell),
				terminal->column_count);
	if (fill) {
		vte_g_array_fill(row,
				 &terminal->pvt->screen->fill_defaults,
				 terminal->column_count);
	}
	return row;
}

/* Check how long a string of unichars is.  Slow version. */
static gssize
vte_unicode_strlen(gunichar *c)
{
	int i;
	for (i = 0; c[i] != 0; i++) ;
	return i;
}

/* Convert a gunichar to a wchar_t for use with X. */
static wchar_t
vte_wc_from_unichar(VteTerminal *terminal, gunichar c)
{
#ifdef __STDC_ISO_10646__
	return (wchar_t) c;
#else
	gpointer original, result;
	char *local, utf8_buf[VTE_UTF8_BPC];
	const char *localr;
	wchar_t wc_buf[VTE_UTF8_BPC];
	int ret;
	gsize length, bytes_read, bytes_written;
	mbstate_t state;
	GError *error = NULL;
	/* Check the cache. */
	if (g_tree_lookup_extended(terminal->pvt->unichar_wc_map,
				   GINT_TO_POINTER(c),
				   &original,
				   &result)) {
		return GPOINTER_TO_INT(c);
	}
	/* Convert the character to a locally-encoded mbs. */
	length = g_unichar_to_utf8(c, utf8_buf);
	local = g_locale_from_utf8(utf8_buf, length,
				   &bytes_read, &bytes_written, &error);
	if (error == NULL) {
		/* Convert from an mbs to a (single-character) wcs. */
		memset(&state, 0, sizeof(state));
		localr = local;
		ret = mbsrtowcs(wc_buf, &localr, bytes_written, &state);
		if (ret == 1) {
			g_tree_insert(terminal->pvt->unichar_wc_map,
				      GINT_TO_POINTER(c),
				      GINT_TO_POINTER(wc_buf[0]));
			return wc_buf[0];
		}
	}
	/* Punt. */
	if (error != NULL) {
		g_printerr("g_locale_from_utf8(%d): %s", error->code,
			   error->message);
		g_error_free(error);
	}
	return (wchar_t) c;
#endif
}

/* Guess at how many columns a character takes up. */
static gssize
vte_unichar_width(VteTerminal *terminal, gunichar c)
{
	gssize width;
	width = g_unichar_isdefined(c) ? (g_unichar_iswide(c) ? 2 : 1) : -1;
	width = CLAMP(width, 1, 2);
	return width;
}

/* Avoid driving myself nuts on the differing semantics of Pango and PangoX. */
static PangoContext *
vte_terminal_get_pango_context(VteTerminal *terminal)
{
	switch (terminal->pvt->render_method) {
	case VteRenderPangoX:
		return pango_x_get_context(GDK_DISPLAY());
		break;
	case VteRenderPango:
	default:
		return gtk_widget_get_pango_context(GTK_WIDGET(terminal));
		break;
	}
}

static void
vte_terminal_maybe_unref_pango_context(VteTerminal *terminal, PangoContext *ctx)
{
	switch (terminal->pvt->render_method) {
	case VteRenderPangoX:
		g_object_unref(G_OBJECT(ctx));
		break;
	case VteRenderPango:
	default:
		/* no-op */
		break;
	}
}

/* Reset defaults for character insertion. */
static void
vte_terminal_set_default_attributes(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->screen->defaults.c = ' ';
	terminal->pvt->screen->defaults.columns = 1;
	terminal->pvt->screen->defaults.fragment = 0;
	terminal->pvt->screen->defaults.fore = VTE_DEF_FG;
	terminal->pvt->screen->defaults.back = VTE_DEF_BG;
	terminal->pvt->screen->defaults.reverse = 0;
	terminal->pvt->screen->defaults.bold = 0;
	terminal->pvt->screen->defaults.invisible = 0;
	terminal->pvt->screen->defaults.protect = 0;
	terminal->pvt->screen->defaults.standout = 0;
	terminal->pvt->screen->defaults.underline = 0;
	terminal->pvt->screen->defaults.strikethrough = 0;
	terminal->pvt->screen->defaults.half = 0;
	terminal->pvt->screen->defaults.blink = 0;
	/* Alternate charset isn't an attribute, though we treat it as one.
	 * terminal->pvt->screen->defaults.alternate = 0; */
	terminal->pvt->screen->basic_defaults = terminal->pvt->screen->defaults;
	terminal->pvt->screen->color_defaults = terminal->pvt->screen->defaults;
	terminal->pvt->screen->fill_defaults = terminal->pvt->screen->defaults;
	terminal->pvt->screen->fill_defaults.c = '\0';
	terminal->pvt->screen->fill_defaults.columns = 1;
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
	rect.x = column_start * terminal->char_width + VTE_PAD_WIDTH;
	rect.width = column_count * terminal->char_width;
	rect.y = row_start * terminal->char_height + VTE_PAD_WIDTH;
	rect.height = row_count * terminal->char_height;

	/* Invalidate the rectangle. */
	gdk_window_invalidate_rect(widget->window, &rect, FALSE);
}

/* Redraw the entire visible portion of the window. */
static void
vte_invalidate_all(VteTerminal *terminal)
{
	GdkRectangle rect;
	GtkWidget *widget;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (!GTK_IS_WIDGET(terminal)) {
	       return;
	}
	widget = GTK_WIDGET(terminal);
	if (!GTK_WIDGET_REALIZED(widget)) {
		return;
	}

	/* Expose the entire widget area. */
	rect.x = 0;
	rect.y = 0;
	rect.width = terminal->column_count * terminal->char_width +
		     2 * VTE_PAD_WIDTH;
	rect.height = terminal->row_count * terminal->char_height +
		      2 * VTE_PAD_WIDTH;
	gdk_window_invalidate_rect((GTK_WIDGET(terminal))->window, &rect, FALSE);
}

/* Scroll a rectangular region up or down by a fixed number of lines. */
static void
vte_terminal_scroll_region(VteTerminal *terminal,
			   long row, glong count, glong delta)
{
	GtkWidget *widget;
	gboolean repaint = TRUE;

	if ((delta == 0) || (count == 0)) {
		/* Shenanigans! */
		return;
	}

	/* We only do this if we're scrolling the entire window. */
	if (!terminal->pvt->bg_transparent &&
	    (terminal->pvt->bg_image == NULL) &&
	    (row == 0) &&
	    (count == terminal->row_count)) {
		widget = GTK_WIDGET(terminal);
		gdk_window_scroll(widget->window,
				  0, delta * terminal->char_height);
		repaint = FALSE;
	}

	if (repaint) {
		/* We have to repaint the entire area. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     row, count);
	}
}

/* Find the character in the given "virtual" position. */
static struct vte_charcell *
vte_terminal_find_charcell(VteTerminal *terminal, glong col, glong row)
{
	GArray *rowdata;
	struct vte_charcell *ret = NULL;
	VteScreen *screen;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	screen = terminal->pvt->screen;
	if (_vte_ring_contains(screen->row_data, row)) {
		rowdata = _vte_ring_index(screen->row_data, GArray*, row);
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
	gssize preedit_length;
	int column, columns;

	if (!VTE_IS_TERMINAL(data)) {
		return;
	}
	terminal = VTE_TERMINAL(data);

	if (terminal->pvt->cursor_visible &&
	    GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
		if (terminal->pvt->im_preedit != NULL) {
			preedit_length = strlen(terminal->pvt->im_preedit);
		} else {
			preedit_length = 0;
		}

		screen = terminal->pvt->screen;
		column = screen->cursor_current.col;
		columns = 1;
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
			columns = cell->columns;
		}
		vte_invalidate_cells(terminal,
				     column, columns + preedit_length,
				     screen->cursor_current.row, 1);
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_UPDATES)) {
			fprintf(stderr, "Invalidating cursor at (%ld,%ld-%ld)."
				"\n", screen->cursor_current.row,
				screen->cursor_current.col,
				screen->cursor_current.col +
				columns + preedit_length - 1);
		}
#endif
	}
}

/* Invalidate the cursor repeatedly. */
static gboolean
vte_invalidate_cursor_periodic(gpointer data)
{
	VteTerminal *terminal;
	GtkWidget *widget;
	GtkSettings *settings;
	gint blink_cycle = 1000;

	g_return_val_if_fail(VTE_IS_TERMINAL(data), FALSE);
	widget = GTK_WIDGET(data);
	if (!GTK_WIDGET_REALIZED(widget)) {
		return TRUE;
	}
	if (!GTK_WIDGET_HAS_FOCUS(widget)) {
		return TRUE;
	}

	terminal = VTE_TERMINAL(widget);
	if (terminal->pvt->cursor_blinks) {
		vte_invalidate_cursor_once(terminal);
	}

	settings = gtk_widget_get_settings(GTK_WIDGET(data));
	if (G_IS_OBJECT(settings)) {
		g_object_get(G_OBJECT(settings), "gtk-cursor-blink-time",
			     &blink_cycle, NULL);
	}

	if (terminal->pvt->cursor_blink_timeout != blink_cycle) {
		terminal->pvt->cursor_blink_tag = g_timeout_add_full(G_PRIORITY_LOW,
								     blink_cycle / 2,
								     vte_invalidate_cursor_periodic,
								     terminal,
								     NULL);
		terminal->pvt->cursor_blink_timeout = blink_cycle;
		return FALSE;
	} else {
		return TRUE;
	}
}

/* Emit a "selection_changed" signal. */
static void
vte_terminal_emit_selection_changed(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `selection-changed'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "selection-changed");
}

/* Emit a "commit" signal. */
static void
vte_terminal_emit_commit(VteTerminal *terminal, gchar *text, guint length)
{
	char *wrapped = NULL;
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `commit' of %d bytes.\n", length);
	}
#endif
	if (length == -1) {
		length = strlen(text);
		wrapped = text;
	} else {
		wrapped = g_malloc0(length + 1);
		memcpy(wrapped, text, length);
	}
	g_signal_emit_by_name(terminal, "commit", wrapped, length);
	if (wrapped != text) {
		g_free(wrapped);
	}
}

/* Emit an "emulation-changed" signal. */
static void
vte_terminal_emit_emulation_changed(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `emulation-changed'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "emulation-changed");
}

/* Emit an "encoding-changed" signal. */
static void
vte_terminal_emit_encoding_changed(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `encoding-changed'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "encoding-changed");
}

/* Emit a "child-exited" signal. */
static void
vte_terminal_emit_child_exited(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `child-exited'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "child-exited");
}

/* Emit a "contents_changed" signal. */
static void
vte_terminal_emit_contents_changed(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `contents-changed'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "contents-changed");
}

/* Emit a "cursor_moved" signal. */
static void
vte_terminal_emit_cursor_moved(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `cursor-moved'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "cursor-moved");
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

/* Emit a "eof" signal. */
static void
vte_terminal_emit_eof(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `eof'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "eof");
}

/* Emit a "char-size-changed" signal. */
static void
vte_terminal_emit_char_size_changed(VteTerminal *terminal,
				    guint width, guint height)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `char-size-changed'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "char-size-changed",
			      width, height);
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

/* Emit a "status-line-changed" signal. */
static void
vte_terminal_emit_status_line_changed(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `status-line-changed'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "status-line-changed");
}

/* Emit an "increase-font-size" signal. */
static void
vte_terminal_emit_increase_font_size(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `increase-font-size'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "increase-font-size");
}

/* Emit a "decrease-font-size" signal. */
static void
vte_terminal_emit_decrease_font_size(VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Emitting `decrease-font-size'.\n");
	}
#endif
	g_signal_emit_by_name(terminal, "decrease-font-size");
}

/* Deselect anything which is selected and refresh the screen if needed. */
static void
vte_terminal_deselect_all(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->has_selection) {
		terminal->pvt->has_selection = FALSE;
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
			fprintf(stderr, "Deselecting all text.\n");
		}
#endif
		vte_terminal_emit_selection_changed(terminal);
		vte_invalidate_all(terminal);
	}
}

/* Reset the set of tab stops to the default. */
static void
vte_terminal_set_tabstop(VteTerminal *terminal, int column)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->tabstops != NULL) {
		/* Just set a non-NULL pointer for this column number. */
		g_hash_table_insert(terminal->pvt->tabstops,
				    GINT_TO_POINTER(2 * column + 1),
				    terminal);
	}
}

/* Remove a tabstop. */
static void
vte_terminal_clear_tabstop(VteTerminal *terminal, int column)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->tabstops != NULL) {
		/* Remove a tab stop from the hash table. */
		g_hash_table_remove(terminal->pvt->tabstops,
				    GINT_TO_POINTER(2 * column + 1));
	}
}

/* Check if we have a tabstop at a given position. */
static gboolean
vte_terminal_get_tabstop(VteTerminal *terminal, int column)
{
	gpointer hash;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	if (terminal->pvt->tabstops != NULL) {
		hash = g_hash_table_lookup(terminal->pvt->tabstops,
					   GINT_TO_POINTER(2 * column + 1));
		return (hash != NULL);
	} else {
		return FALSE;
	}
}

/* Reset the set of tab stops to the default. */
static void
vte_terminal_set_default_tabstops(VteTerminal *terminal)
{
	int i, width;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->tabstops != NULL) {
		g_hash_table_destroy(terminal->pvt->tabstops);
	}
	terminal->pvt->tabstops = g_hash_table_new(g_direct_hash,
						   g_direct_equal);
	width = _vte_termcap_find_numeric(terminal->pvt->termcap,
					  terminal->pvt->emulation,
					  "it");
	if (width == 0) {
		width = VTE_TAB_WIDTH;
	}
	for (i = 0; i <= VTE_TAB_MAX; i += width) {
		vte_terminal_set_tabstop(terminal, i);
	}
}

/* Clear the cache of the screen contents we keep. */
static void
vte_terminal_match_contents_clear(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->match_contents != NULL) {
		g_free(terminal->pvt->match_contents);
		terminal->pvt->match_contents = NULL;;
	}
	if (terminal->pvt->match_attributes != NULL) {
		g_array_free(terminal->pvt->match_attributes, TRUE);
		terminal->pvt->match_attributes = NULL;
	}
	vte_terminal_match_hilite_clear(terminal);
}

/* Refresh the cache of the screen contents we keep. */
static gboolean
always_selected(VteTerminal *terminal, glong row, glong column, gpointer data)
{
	return TRUE;
}
static void
vte_terminal_match_contents_refresh(VteTerminal *terminal)
{
	GArray *array;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_terminal_match_contents_clear(terminal);
	array = g_array_new(FALSE, TRUE, sizeof(struct vte_char_attributes));
	terminal->pvt->match_contents = vte_terminal_get_text(terminal,
							      always_selected,
							      NULL,
							      array);
	terminal->pvt->match_attributes = array;
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
	int i;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	for (i = 0; i < terminal->pvt->match_regexes->len; i++) {
		regex = &g_array_index(terminal->pvt->match_regexes,
				       struct vte_match_regex,
				       i);
		/* Unless this is a hole, clean it up. */
		if (regex->tag >= 0) {
			regfree(&regex->reg);
			memset(&regex->reg, 0, sizeof(regex->reg));
			regex->tag = -1;
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
	if (terminal->pvt->match_regexes->len > tag) {
		/* The tag is an index, so find the corresponding struct. */
		regex = &g_array_index(terminal->pvt->match_regexes,
				       struct vte_match_regex,
				       tag);
		/* If it's already been removed, return. */
		if (regex->tag < 0) {
			return;
		}
		/* Remove this item and leave a hole in its place. */
		regfree(&regex->reg);
		memset(&regex->reg, 0, sizeof(regex->reg));
		regex->tag = -1;
	}
	vte_terminal_match_hilite_clear(terminal);
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
 */
int
vte_terminal_match_add(VteTerminal *terminal, const char *match)
{
	struct vte_match_regex new_regex, *regex;
	int ret;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	memset(&new_regex, 0, sizeof(new_regex));
	ret = regcomp(&new_regex.reg, match, VTE_REGCOMP_FLAGS);
	if (ret != 0) {
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

/* Check if a given cell on the screen contains part of a matched string.  If
 * it does, return the string, and store the match tag in the optional tag
 * argument. */
static char *
vte_terminal_match_check_internal(VteTerminal *terminal,
				  long column, glong row,
				  int *tag, int *start, int *end)
{
	int i, j, ret, offset;
	struct vte_match_regex *regex = NULL;
	struct vte_char_attributes *attr = NULL;
	gssize coffset;
	regmatch_t matches[256];
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		fprintf(stderr, "Checking for match at (%ld,%ld).\n",
			row, column);
	}
#endif
	if (tag != NULL) {
		*tag = -1;
	}
	if (start != NULL) {
		*start = 0;
	}
	if (end != NULL) {
		*end = 0;
	}
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	if (terminal->pvt->match_contents == NULL) {
		vte_terminal_match_contents_refresh(terminal);
	}
	/* Map the pointer position to a portion of the string. */
	for (offset = terminal->pvt->match_attributes->len - 1;
	     offset >= 0;
	     offset--) {
		attr = &g_array_index(terminal->pvt->match_attributes,
				      struct vte_char_attributes,
				      offset);
		if ((row == attr->row) &&
		    (column == attr->column) &&
		    (terminal->pvt->match_contents[offset] != ' ')) {
			break;
		}
	}
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		if (offset < 0) {
			fprintf(stderr, "Cursor is not on a character.\n");
		} else {
			fprintf(stderr, "Cursor is on character %d.\n", offset);
		}
	}
#endif

	/* If the pointer isn't on a matchable character, bug out. */
	if (offset < 0) {
		return NULL;
	}

	/* If the pointer is on a newline, bug out. */
	if (g_ascii_isspace(terminal->pvt->match_contents[offset])) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Cursor is on whitespace.\n");
		}
#endif
		return NULL;
	}

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
		coffset = 0;
		ret = regexec(&regex->reg,
			      terminal->pvt->match_contents + coffset,
			      G_N_ELEMENTS(matches),
			      matches,
			      VTE_REGEXEC_FLAGS);
		while (ret == 0) {
			for (j = 0;
			     (j < G_N_ELEMENTS(matches)) &&
			     (matches[j].rm_so != -1);
			     j++) {
				/* The offsets should be "sane". */
				g_assert(matches[j].rm_so + coffset <
					 terminal->pvt->match_attributes->len);
				g_assert(matches[j].rm_eo + coffset <=
					 terminal->pvt->match_attributes->len);
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_MISC)) {
					char *match;
					struct vte_char_attributes *sattr, *eattr;
					match = g_strndup(terminal->pvt->match_contents + matches[j].rm_so + coffset,
							  matches[j].rm_eo - matches[j].rm_so);
					sattr = &g_array_index(terminal->pvt->match_attributes,
							       struct vte_char_attributes,
							       matches[j].rm_so + coffset);
					eattr = &g_array_index(terminal->pvt->match_attributes,
							       struct vte_char_attributes,
							       matches[j].rm_eo + coffset - 1);
					fprintf(stderr, "Match %d `%s' from %d(%ld,%ld) to %d(%ld,%ld) (%d).\n",
						j, match,
						matches[j].rm_so + coffset,
						sattr->column,
						sattr->row,
						matches[j].rm_eo + coffset - 1,
						eattr->column,
						eattr->row,
						offset);
					g_free(match);

				}
#endif
				/* Snip off any final newlines. */
				while ((matches[j].rm_eo > matches[j].rm_so) &&
				       (terminal->pvt->match_contents[coffset + matches[j].rm_eo - 1] == '\n')) {
					matches[j].rm_eo--;
				}
				/* If the pointer is in this substring,
				 * then we're done. */
				if ((offset >= (matches[j].rm_so + coffset)) &&
				    (offset < (matches[j].rm_eo + coffset))) {
					if (tag != NULL) {
						*tag = regex->tag;
					}
					if (start != NULL) {
						*start = coffset +
							 matches[j].rm_so;
					}
					if (end != NULL) {
						*end = coffset +
						       matches[j].rm_eo - 1;
					}
					return g_strndup(terminal->pvt->match_contents + coffset + matches[j].rm_so,
							 matches[j].rm_eo - matches[j].rm_so);
				}
			}
			/* Skip past the beginning of this match to
			 * look for more. */
			coffset += (matches[0].rm_so + 1);
			ret = regexec(&regex->reg,
				      terminal->pvt->match_contents + coffset,
				      G_N_ELEMENTS(matches),
				      matches,
				      VTE_REGEXEC_FLAGS);
		}
	}
	return NULL;
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
 * match exists, the text string is returned and if @tag is not NULL, the number
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
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		fprintf(stderr, "Checking for match at (%ld,%ld).\n",
			row, column);
	}
#endif
	ret = vte_terminal_match_check_internal(terminal,
						column, row + delta,
						tag, NULL, NULL);
#ifdef VTE_DEBUG
	if ((ret != NULL) && _vte_debug_on(VTE_DEBUG_EVENTS)) {
		fprintf(stderr, "Matched `%s'.\n", ret);
	}
#endif
	return ret;
}

/* Emit an adjustment changed signal on our adjustment object. */
static gboolean
vte_terminal_emit_adjustment_changed(gpointer data)
{
	VteTerminal *terminal;
	terminal = VTE_TERMINAL(data);
	if (terminal->pvt->adjustment_changed_tag) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Emitting adjustment_changed.\n");
		}
#endif
		terminal->pvt->adjustment_changed_tag = 0;
		gtk_adjustment_changed(terminal->adjustment);
	}
	return FALSE;
}

/* Queue an adjustment-changed signal to be delivered when convenient. */
static void
vte_terminal_queue_adjustment_changed(VteTerminal *terminal)
{
	if (terminal->pvt->adjustment_changed_tag == 0) {
		terminal->pvt->adjustment_changed_tag =
				g_idle_add_full(VTE_ADJUSTMENT_PRIORITY,
						vte_terminal_emit_adjustment_changed,
						terminal,
						NULL);
	} else {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Swallowing duplicate "
				"adjustment-changed signal.\n");
		}
#endif
	}
}

/* Update the adjustment field of the widget.  This function should be called
 * whenever we add rows to or remove rows from the history or switch screens. */
static void
vte_terminal_adjust_adjustments(VteTerminal *terminal, gboolean immediate)
{
	VteScreen *screen;
	gboolean changed;
	long delta;
	long rows;

	g_return_if_fail(terminal->pvt->screen != NULL);
	g_return_if_fail(terminal->pvt->screen->row_data != NULL);

	/* Adjust the vertical, uh, adjustment. */
	changed = FALSE;

	/* The lower value should be the first row in the buffer. */
	screen = terminal->pvt->screen;
	delta = _vte_ring_delta(screen->row_data);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_IO)) {
		fprintf(stderr, "Changing adjustment values "
			"(delta = %ld, scroll = %ld).\n",
			delta, screen->scroll_delta);
	}
#endif
	if (terminal->adjustment->lower != delta) {
		terminal->adjustment->lower = delta;
		changed = TRUE;
	}

	/* Snap the insert delta and the cursor position to be in the visible
	 * area.  Leave the scrolling delta alone because it will be updated
	 * when the adjustment changes. */
	screen->insert_delta = MAX(screen->insert_delta, delta);
	screen->cursor_current.row = MAX(screen->cursor_current.row,
					 screen->insert_delta);

	/* The upper value is the number of rows which might be visible.  (Add
	 * one to the cursor offset because it's zero-based.) */
	rows = MAX(_vte_ring_next(terminal->pvt->screen->row_data),
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
	if (terminal->adjustment->page_size != terminal->row_count) {
		terminal->adjustment->page_size = terminal->row_count;
		changed = TRUE;
	}

	/* Clicking in the empty area should scroll one screen, so set the
	 * page size to the number of visible rows. */
	if (terminal->adjustment->page_increment != terminal->row_count) {
		terminal->adjustment->page_increment = terminal->row_count;
		changed = TRUE;
	}

	/* Set the scrollbar adjustment to where the screen wants it to be. */
	if (floor(terminal->adjustment->value) !=
	    terminal->pvt->screen->scroll_delta) {
		/* This emits a "value-changed" signal, so no need to screw
		 * with anything else for just this. */
		gtk_adjustment_set_value(terminal->adjustment,
					 terminal->pvt->screen->scroll_delta);
	}

	/* If anything changed, signal that there was a change. */
	if (changed == TRUE) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_IO)) {
			fprintf(stderr, "Changed adjustment values "
				"(delta = %ld, scroll = %ld).\n",
				delta, terminal->pvt->screen->scroll_delta);
		}
#endif
		if (immediate) {
			gtk_adjustment_changed(terminal->adjustment);
		} else {
			vte_terminal_queue_adjustment_changed(terminal);
		}
	}
}

/* Scroll up or down in the current screen. */
static void
vte_terminal_scroll_pages(VteTerminal *terminal, gint pages)
{
	glong destination;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_IO)) {
		fprintf(stderr, "Scrolling %d pages.\n", pages);
	}
#endif
	/* Calculate the ideal position where we want to be before clamping. */
	destination = floor(gtk_adjustment_get_value(terminal->adjustment));
	destination += (pages * terminal->row_count);
	/* Can't scroll past data we have. */
	destination = CLAMP(destination,
			    terminal->adjustment->lower,
			    terminal->adjustment->upper - terminal->row_count);
	/* Tell the scrollbar to adjust itself. */
	gtk_adjustment_set_value(terminal->adjustment, destination);
	/* Clear dingus match set. */
	vte_terminal_match_contents_clear(terminal);
	/* Notify viewers that the contents have changed. */
	vte_terminal_emit_contents_changed(terminal);
}

/* Scroll so that the scroll delta is the insertion delta. */
static void
vte_terminal_maybe_scroll_to_bottom(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (floor(gtk_adjustment_get_value(terminal->adjustment)) !=
	    terminal->pvt->screen->insert_delta) {
		gtk_adjustment_set_value(terminal->adjustment,
					 terminal->pvt->screen->insert_delta);
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
			val = MAX(val, 1);	/* FIXME: vttest. */
		}
	}
	for (i = 0; i < val; i++) {
		handler(terminal, match, match_quark, NULL);
	}
}

/* Insert a blank line at an arbitrary position. */
static void
vte_insert_line_internal(VteTerminal *terminal, glong position)
{
	GArray *array;
	/* Pad out the line data to the insertion point. */
	while (_vte_ring_next(terminal->pvt->screen->row_data) < position) {
		array = vte_new_row_data_sized(terminal, TRUE);
		_vte_ring_append(terminal->pvt->screen->row_data, array);
	}
	/* If we haven't inserted a line yet, insert a new one. */
	array = vte_new_row_data_sized(terminal, TRUE);
	if (_vte_ring_next(terminal->pvt->screen->row_data) >= position) {
		_vte_ring_insert(terminal->pvt->screen->row_data,
				 position, array);
	} else {
		_vte_ring_append(terminal->pvt->screen->row_data, array);
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

/**
 * vte_terminal_set_encoding:
 * @terminal: a #VteTerminal
 * @codeset: a valid #gconv target
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
	const char *old_codeset;
	GQuark encoding_quark;
	GIConv conv, new_iconv, new_oconvw, new_oconvu;
	char *ibuf, *obuf, *obufptr;
	gsize icount, ocount;

	old_codeset = terminal->pvt->encoding;
	if (codeset == NULL) {
		g_get_charset(&codeset);
	}

	/* Open new conversions. */
	new_iconv = g_iconv_open(_vte_table_wide_encoding(), codeset);
	if (new_iconv == ((GIConv) -1)) {
		g_warning(_("Unable to convert characters from %s to %s."),
			  codeset, _vte_table_wide_encoding());
		if (terminal->pvt->encoding != NULL) {
			/* Keep the current encoding. */
			return;
		}
	}
	new_oconvw = g_iconv_open(codeset, _vte_table_wide_encoding());
	if (new_oconvw == ((GIConv) -1)) {
		g_warning(_("Unable to convert characters from %s to %s."),
			  _vte_table_wide_encoding(), codeset);
		if (new_iconv != ((GIConv) -1)) {
			g_iconv_close(new_iconv);
		}
		if (terminal->pvt->encoding != NULL) {
			/* Keep the current encoding. */
			return;
		}
	}
	new_oconvu = g_iconv_open(codeset, "UTF-8");
	if (new_oconvu == ((GIConv) -1)) {
		g_warning(_("Unable to convert characters from %s to %s."),
			  "UTF-8", codeset);
		if (new_iconv != ((GIConv) -1)) {
			g_iconv_close(new_iconv);
		}
		if (new_oconvw != ((GIConv) -1)) {
			g_iconv_close(new_oconvw);
		}
		if (terminal->pvt->encoding != NULL) {
			/* Keep the current encoding. */
			return;
		}
	}

	if (new_oconvu == ((GIConv) -1)) {
		codeset = _vte_table_narrow_encoding();
		new_iconv = g_iconv_open(_vte_table_wide_encoding(), codeset);
		if (new_iconv == ((GIConv) -1)) {
			g_error(_("Unable to convert characters from %s to %s."),
				codeset, _vte_table_wide_encoding());
		}
		new_oconvw = g_iconv_open(codeset, _vte_table_wide_encoding());
		if (new_oconvw == ((GIConv) -1)) {
			g_error(_("Unable to convert characters from %s to %s."),
				_vte_table_wide_encoding(), codeset);
		}
		new_oconvu = g_iconv_open(codeset, "UTF-8");
		if (new_oconvu == ((GIConv) -1)) {
			g_error(_("Unable to convert characters from %s to %s."),
				"UTF-8", codeset);
		}
	}

	/* Set up the conversion for incoming-to-gunichar. */
	if (terminal->pvt->incoming_conv != ((GIConv) -1)) {
		g_iconv_close(terminal->pvt->incoming_conv);
	}
	terminal->pvt->incoming_conv = new_iconv;

	/* Set up the conversions for gunichar/utf-8 to outgoing. */
	if (terminal->pvt->outgoing_conv_wide != ((GIConv) -1)) {
		g_iconv_close(terminal->pvt->outgoing_conv_wide);
	}
	terminal->pvt->outgoing_conv_wide = new_oconvw;

	if (terminal->pvt->outgoing_conv_utf8 != ((GIConv) -1)) {
		g_iconv_close(terminal->pvt->outgoing_conv_utf8);
	}
	terminal->pvt->outgoing_conv_utf8 = new_oconvu;

	/* Set the terminal's encoding to the new value. */
	encoding_quark = g_quark_from_string(codeset);
	terminal->pvt->encoding = g_quark_to_string(encoding_quark);

	/* Convert any buffered output bytes. */
	if ((_vte_buffer_length(terminal->pvt->outgoing) > 0) &&
	    (old_codeset != NULL)) {
		icount = _vte_buffer_length(terminal->pvt->outgoing);
		ibuf = terminal->pvt->incoming->bytes;
		ocount = icount * VTE_UTF8_BPC + 1;
		_vte_buffer_set_minimum_size(terminal->pvt->conv_buffer,
					     ocount);
		obuf = obufptr = terminal->pvt->conv_buffer->bytes;
		conv = g_iconv_open(codeset, old_codeset);
		if (conv != ((GIConv) -1)) {
			if (g_iconv(conv, &ibuf, &icount, &obuf, &ocount) == -1) {
				/* Darn, it failed.  Leave it alone. */
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_IO)) {
					fprintf(stderr, "Error converting %ld "
						"pending output bytes (%s) "
						"skipping.\n",
						(long) _vte_buffer_length(terminal->pvt->outgoing),
						strerror(errno));
				}
#endif
			} else {
				_vte_buffer_clear(terminal->pvt->outgoing);
				_vte_buffer_append(terminal->pvt->outgoing,
						   obufptr, obuf - obufptr);
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_IO)) {
					fprintf(stderr, "Converted %ld pending "
						"output bytes.\n",
						(long) _vte_buffer_length(terminal->pvt->outgoing));
				}
#endif
			}
			g_iconv_close(conv);
		}
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_IO)) {
		fprintf(stderr, "Set terminal encoding to `%s'.\n",
			terminal->pvt->encoding);
	}
#endif
	vte_terminal_emit_encoding_changed(terminal);
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

/* End alternate character set. */
static void
vte_sequence_handler_ae(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
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
	GArray *rowdata;
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
		rowdata = _vte_ring_index(screen->row_data, GArray*, start);
		/* Add enough cells to it so that it has the default colors. */
		vte_g_array_fill(rowdata, &screen->fill_defaults,
				 terminal->column_count);
		/* Adjust the scrollbars if necessary. */
		vte_terminal_adjust_adjustments(terminal, FALSE);
	}

	/* Update the display. */
	vte_terminal_scroll_region(terminal, start, end - start + 1, param);
}

/* Add N lines at the current cursor position. */
static void
vte_sequence_handler_AL(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_al(terminal, match, match_quark, params);
}

/* Start using alternate character set. */
static void
vte_sequence_handler_as(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.alternate = 1;
}

/* Beep. */
static void
vte_sequence_handler_bl(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	if (terminal->pvt->audible_bell) {
		/* Feep. */
		gdk_beep();
	}
	if (terminal->pvt->visible_bell) {
		/* Visual bell. */
		vte_sequence_handler_vb(terminal, match, match_quark, params);
	}
}

/* Backtab. */
static void
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
		for (newcol += (terminal->column_count - 1);
		     newcol >= 0;
		     newcol--) {
			if (vte_terminal_get_tabstop(terminal,
						     newcol % terminal->column_count)) {
				break;
			}
		}
	}

	/* If we have no tab stops, stop right here. */
	if (newcol <= 0) {
		return;
	}

	/* Warp the cursor. */
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_PARSE)) {
		fprintf(stderr, "Moving cursor to column %ld.\n", (long)newcol);
	}
#endif
	terminal->pvt->screen->cursor_current.col = newcol;
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
	screen = terminal->pvt->screen;

	/* Get the data for the row which the cursor points to. */
	vte_terminal_ensure_cursor(terminal, FALSE);
	rowdata = _vte_ring_index(screen->row_data,
				  GArray*,
				  screen->cursor_current.row);
	/* Clear the data up to the current column with the default
	 * attributes.  If there is no such character cell, we need
	 * to add one. */
	for (i = 0; i <= screen->cursor_current.col; i++) {
		if (i < rowdata->len) {
			/* Muck with the cell in this location. */
			pcell = &g_array_index(rowdata,
					       struct vte_charcell,
					       i);
			*pcell = screen->color_defaults;
		} else {
			/* Add new cells until we have one here. */
			g_array_append_val(rowdata, screen->color_defaults);
		}
	}
	/* Repaint this row. */
	vte_invalidate_cells(terminal,
			     0, terminal->column_count,
			     screen->cursor_current.row, 1);
}

/* Clear to the right of the cursor and below the current line. */
static void
vte_sequence_handler_cd(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	GArray *rowdata;
	long i;
	VteScreen *screen;

	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear the rest of the
	 * row the cursor is on and all of the rows below the cursor. */
	i = screen->cursor_current.row;
	if (i < _vte_ring_next(screen->row_data)) {
		/* Get the data for the row we're clipping. */
		rowdata = _vte_ring_index(screen->row_data, GArray*, i);
		/* Clear everything to the right of the cursor. */
		if ((rowdata != NULL) &&
		    (rowdata->len > screen->cursor_current.col)) {
			g_array_set_size(rowdata,
					 screen->cursor_current.col);
		}
	}
	/* Now for the rest of the lines. */
	for (i = screen->cursor_current.row + 1;
	     i < _vte_ring_next(screen->row_data);
	     i++) {
		/* Get the data for the row we're removing. */
		rowdata = _vte_ring_index(screen->row_data, GArray*, i);
		/* Remove it. */
		if ((rowdata != NULL) && (rowdata->len > 0)) {
			g_array_set_size(rowdata, 0);
		}
	}
	/* Now fill the cleared areas. */
	for (i = screen->cursor_current.row;
	     i < screen->insert_delta + terminal->row_count;
	     i++) {
		/* Retrieve the row's data, creating it if necessary. */
		if (_vte_ring_contains(screen->row_data, i)) {
			rowdata = _vte_ring_index(screen->row_data, GArray*, i);
		} else {
			rowdata = vte_new_row_data(terminal);
			_vte_ring_append(screen->row_data, rowdata);
		}
		/* Pad out the row. */
		vte_g_array_fill(rowdata,
				 &screen->fill_defaults,
				 terminal->column_count);
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

	screen = terminal->pvt->screen;
	/* Get the data for the row which the cursor points to. */
	vte_terminal_ensure_cursor(terminal, FALSE);
	rowdata = _vte_ring_index(screen->row_data, GArray*,
				  screen->cursor_current.row);
	/* Remove the data at the end of the array until the current column
	 * is the end of the array. */
	if (rowdata->len > screen->cursor_current.col) {
		g_array_set_size(rowdata, screen->cursor_current.col);
	}
	/* Add enough cells to the end of the line to fill out the row. */
	vte_g_array_fill(rowdata,
			 &screen->fill_defaults,
			 terminal->column_count);
	/* Repaint this row. */
	vte_invalidate_cells(terminal,
			     0, terminal->column_count,
			     screen->cursor_current.row, 1);
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
			rowval = CLAMP(rowval, 0, terminal->row_count - 1);
			colval = CLAMP(colval, 0, terminal->column_count - 1);
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

	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = _vte_ring_index(screen->row_data, GArray*,
					  screen->cursor_current.row);
		/* Remove it. */
		if (rowdata->len > 0) {
			g_array_set_size(rowdata, 0);
		}
		/* Add enough cells to the end of the line to fill out the
		 * row. */
		vte_g_array_fill(rowdata,
				 &screen->fill_defaults,
				 terminal->column_count);
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

	/* We require two parameters.  Anything less is a reset. */
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

/* Restrict scrolling and updates to a subset of the visible lines, because
 * GNU Emacs is special. */
static void
vte_sequence_handler_cS(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	long start, end, rows;
	GValue *value;

	/* We require four parameters. */
	if ((params == NULL) || (params->n_values < 2)) {
		terminal->pvt->screen->scrolling_restricted = FALSE;
		return;
	}
	/* Extract the two parameters we care about, encoded as the number
	 * of lines above and below the scrolling region, respectively. */
	value = g_value_array_get_nth(params, 1);
	start = g_value_get_long(value);
	value = g_value_array_get_nth(params, 2);
	end = (terminal->row_count - 1) - g_value_get_long(value);
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

/* Clear all tab stops. */
static void
vte_sequence_handler_ct(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	if (terminal->pvt->tabstops != NULL) {
		g_hash_table_destroy(terminal->pvt->tabstops);
		terminal->pvt->tabstops = NULL;
	}
}

/* Move the cursor to the lower left-hand corner. */
static void
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
	vte_terminal_ensure_cursor(terminal, TRUE);
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static void
vte_sequence_handler_cursor_next_line(VteTerminal *terminal,
				      const char *match,
				      GQuark match_quark,
				      GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	vte_sequence_handler_DO(terminal, match, match_quark, params);
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static void
vte_sequence_handler_cursor_preceding_line(VteTerminal *terminal,
					   const char *match,
					   GQuark match_quark,
					   GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	vte_sequence_handler_UP(terminal, match, match_quark, params);
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
	long val;
	screen = terminal->pvt->screen;
	/* We only care if there's a parameter in there. */
	if ((params != NULL) && (params->n_values > 0)) {
		value = g_value_array_get_nth(params, 0);
		if (G_VALUE_HOLDS_LONG(value)) {
			/* Move the cursor. */
			val = CLAMP(g_value_get_long(value),
				    0,
				    terminal->row_count - 1);
			screen->cursor_current.row = screen->insert_delta + val;
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

	screen = terminal->pvt->screen;

	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = _vte_ring_index(screen->row_data,
					  GArray*,
					  screen->cursor_current.row);
		col = screen->cursor_current.col;
		/* Remove the column. */
		if (col < rowdata->len) {
			g_array_remove_index(rowdata, col);
		}
		/* Add new cells until we have enough to fill the row. */
		vte_g_array_fill(rowdata,
				 &screen->color_defaults,
				 terminal->column_count);
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
		vte_terminal_adjust_adjustments(terminal, FALSE);
	}

	/* Update the display. */
	vte_terminal_scroll_region(terminal, start, end - start + 1, -param);
}

/* Delete N lines at the current cursor position. */
static void
vte_sequence_handler_DL(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_dl(terminal, match, match_quark, params);
}

/* Make sure we have enough rows and columns to hold data at the current
 * cursor position. */
static void
vte_terminal_ensure_cursor(VteTerminal *terminal, gboolean current)
{
	GArray *array;
	VteScreen *screen;
	gboolean readjust = FALSE, fill = FALSE;

	/* Must make sure we're in a sane area. */
	screen = terminal->pvt->screen;

	/* Figure out how many rows we need to add. */
	fill = (terminal->pvt->screen->defaults.back != VTE_DEF_BG);
	while (screen->cursor_current.row >= _vte_ring_next(screen->row_data)) {
		/* Create a new row. */
		if (fill) {
			array = vte_new_row_data_sized(terminal, TRUE);
		} else {
			array = vte_new_row_data(terminal);
		}
		_vte_ring_append(screen->row_data, array);
		readjust = TRUE;
	}
	if (readjust) {
		vte_terminal_adjust_adjustments(terminal, FALSE);
	}

	/* Find the row the cursor is in. */
	array = _vte_ring_index(screen->row_data,
			        GArray*,
			        screen->cursor_current.row);
	if ((array->len <= screen->cursor_current.col) &&
	    (array->len < terminal->column_count)) {
		/* Set up defaults we'll use when adding new cells. */
		if (current) {
			/* Add new cells until we have one here. */
			vte_g_array_fill(array,
					 &screen->color_defaults,
					 screen->cursor_current.col + 1);
		} else {
			/* Add enough cells at the end to make sure we have
			 * enough for all visible columns. */
			vte_g_array_fill(array,
					 &screen->basic_defaults,
					 screen->cursor_current.col + 1);
		}
	}
}

/* Update the insert delta so that the screen which includes it also
 * includes the end of the buffer. */
static void
vte_terminal_update_insert_delta(VteTerminal *terminal)
{
	long delta;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	/* Make sure that the bottom row is visible, and that it's in
	 * the buffer (even if it's empty).  This usually causes the
	 * top row to become a history-only row. */
	delta = MAX(screen->insert_delta,
		    screen->cursor_current.row - (terminal->row_count - 1));
	delta = MAX(delta, _vte_ring_delta(screen->row_data));

	/* Adjust the insert delta and scroll if needed. */
	if (delta != screen->insert_delta) {
		vte_terminal_ensure_cursor(terminal, FALSE);
		vte_terminal_adjust_adjustments(terminal, TRUE);
		screen->insert_delta = delta;
	}
}

/* Cursor down, no scrolling. */
static void
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
}

/* Cursor down, no scrolling. */
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
	vte_terminal_ensure_cursor(terminal, TRUE);
	if (_vte_ring_next(screen->row_data) > screen->cursor_current.row) {
		/* Get the data for the row which the cursor points to. */
		rowdata = _vte_ring_index(screen->row_data,
					  GArray*,
					  screen->cursor_current.row);
		/* Write over the characters.  (If there aren't enough, we'll
		 * need to create them.) */
		for (i = 0; i < count; i++) {
			col = screen->cursor_current.col + i;
			if (col >= 0) {
				if (col < rowdata->len) {
					/* Replace this cell with the current
					 * defaults. */
					cell = &g_array_index(rowdata,
							      struct vte_charcell,
							      col);
					*cell = screen->color_defaults;
				} else {
					/* Add new cells until we have one here. */
					vte_g_array_fill(rowdata,
							 &screen->color_defaults,
							 col);
				}
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
	terminal->pvt->screen->insert_mode = FALSE;
}

/* Move from status line. */
static void
vte_sequence_handler_fs(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->status_line = FALSE;
}

/* Move the cursor to the home position. */
static void
vte_sequence_handler_ho(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	VteScreen *screen;
	screen = terminal->pvt->screen;
	screen->cursor_current.row = screen->insert_delta;
	screen->cursor_current.col = 0;
}

/* Move the cursor to a specified position. */
static void
vte_sequence_handler_horizontal_and_vertical_position(VteTerminal *terminal,
						      const char *match,
						      GQuark match_quark,
						      GValueArray *params)
{
	vte_sequence_handler_offset(terminal, match, match_quark, params,
				    -1, vte_sequence_handler_cm);
}

/* Insert a character. */
static void
vte_sequence_handler_ic(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	struct vte_cursor_position save;
	VteScreen *screen;

	screen = terminal->pvt->screen;

	save = screen->cursor_current;

	vte_terminal_insert_char(terminal, ' ', TRUE, TRUE, TRUE, -1);

	screen->cursor_current = save;
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
	terminal->pvt->screen->insert_mode = TRUE;
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_index(VteTerminal *terminal,
			   const char *match,
			   GQuark match_quark,
			   GValueArray *params)
{
	vte_sequence_handler_sf(terminal, match, match_quark, params);
}

/* Send me a backspace key sym, will you?  Guess that the application meant
 * to send the cursor back one position. */
static void
vte_sequence_handler_kb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	/* Move the cursor left. */
	vte_sequence_handler_le(terminal, match, match_quark, params);
}

/* Keypad mode end. */
static void
vte_sequence_handler_ke(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->keypad_mode = VTE_KEYMODE_NORMAL;
}

/* Keypad mode start. */
static void
vte_sequence_handler_ks(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->keypad_mode = VTE_KEYMODE_APPLICATION;
}

/* Cursor left. */
static void
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
			screen->cursor_current.row = MAX(screen->cursor_current.row - 1,
							 screen->insert_delta);
		} else {
			/* Stick to the first column. */
			screen->cursor_current.col = 0;
		}
	}
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

/* Move the cursor to the lower left corner of the display. */
static void
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
}

/* Blink on. */
static void
vte_sequence_handler_mb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.blink = 1;
}

/* Bold on. */
static void
vte_sequence_handler_md(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.bold = 1;
	terminal->pvt->screen->defaults.half = 0;
}

/* End modes. */
static void
vte_sequence_handler_me(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_terminal_set_default_attributes(terminal);
}

/* Half-bright on. */
static void
vte_sequence_handler_mh(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.half = 1;
	terminal->pvt->screen->defaults.bold = 0;
}

/* Invisible on. */
static void
vte_sequence_handler_mk(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.invisible = 1;
}

/* Protect on. */
static void
vte_sequence_handler_mp(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.protect = 1;
}

/* Reverse on. */
static void
vte_sequence_handler_mr(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
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
	screen = terminal->pvt->screen;
	if ((screen->cursor_current.col + 1) < terminal->column_count) {
		/* There's room to move right. */
		screen->cursor_current.col++;
	}
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
static void
vte_sequence_handler_next_line(VteTerminal *terminal,
			       const char *match,
			       GQuark match_quark,
			       GValueArray *params)
{
	terminal->pvt->screen->cursor_current.col = 0;
	vte_sequence_handler_DO(terminal, match, match_quark, params);
}

/* No-op. */
static void
vte_sequence_handler_noop(VteTerminal *terminal,
			  const char *match,
			  GQuark match_quark,
			  GValueArray *params)
{
}

/* Carriage return command(?). */
static void
vte_sequence_handler_nw(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_cr(terminal, match, match_quark, params);
}

/* Restore cursor (position). */
static void
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
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_reverse_index(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	vte_sequence_handler_sr(terminal, match, match_quark, params);
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
	screen = terminal->pvt->screen;
	screen->cursor_saved.col = screen->cursor_current.col;
	screen->cursor_saved.row = CLAMP(screen->cursor_current.row -
					 screen->insert_delta,
					 0, terminal->row_count - 1);
}

/* Standout end. */
static void
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
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_sf(VteTerminal *terminal,
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

	if (screen->cursor_current.row == end) {
		if (screen->scrolling_restricted) {
			/* If we're at the bottom of the scrolling region, add a
			 * line at the top to scroll the bottom off. */
			vte_remove_line_internal(terminal, start);
			vte_insert_line_internal(terminal, end);
			/* Update the display. */
			vte_terminal_scroll_region(terminal, start,
						   end - start + 1, -1);
		} else {
			/* Scroll up with history. */
			screen->cursor_current.row++;
			vte_terminal_update_insert_delta(terminal);
		}
	} else {
		/* Otherwise, just move the cursor down. */
		screen->cursor_current.row++;
		vte_terminal_ensure_cursor(terminal, TRUE);
	}
	/* Adjust the scrollbars if necessary. */
	vte_terminal_adjust_adjustments(terminal, FALSE);
}

/* Cursor down, with scrolling. */
static void
vte_sequence_handler_SF(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, match, match_quark, params,
				      vte_sequence_handler_sf);
}

/* Standout start. */
static void
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
}

/* Cursor up, scrolling if need be. */
static void
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
		vte_terminal_scroll_region(terminal, start, end - start + 1, 1);
	} else {
		/* Otherwise, just move the cursor up. */
		screen->cursor_current.row--;
	}
	/* Adjust the scrollbars if necessary. */
	vte_terminal_adjust_adjustments(terminal, FALSE);
}

/* Cursor up, with scrolling. */
static void
vte_sequence_handler_SR(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	vte_sequence_handler_multiple(terminal, match, match_quark, params,
				      vte_sequence_handler_sr);
}

/* Set tab stop in the current column. */
static void
vte_sequence_handler_st(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	if (terminal->pvt->tabstops == NULL) {
		terminal->pvt->tabstops = g_hash_table_new(g_direct_hash,
							   g_direct_equal);
	}
	vte_terminal_set_tabstop(terminal,
				 terminal->pvt->screen->cursor_current.col);
}

/* Tab. */
static void
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
			if (vte_terminal_get_tabstop(terminal, newcol)) {
				break;
			}
		}
	}

	/* If we have no tab stops or went past the end of the line, stop
	 * at the right-most column. */
	if (newcol >= terminal->column_count) {
		newcol = terminal->column_count - 1;
	}

#if 0
	/* Insert a tab character with the right width. */
	vte_terminal_insert_char(terminal, '\t',
				 FALSE, FALSE, FALSE,
				 newcol -
				 terminal->pvt->screen->cursor_current.col);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_PARSE)) {
		fprintf(stderr, "Inserting tab.\n");
	}
#endif
#else
	terminal->pvt->screen->cursor_current.col = newcol;
#endif
}

/* Clear tabs selectively. */
static void
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
		vte_terminal_clear_tabstop(terminal,
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
vte_sequence_handler_ts(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->status_line = TRUE;
	g_string_truncate(terminal->pvt->screen->status_line_contents, 0);
	vte_terminal_emit_status_line_changed(terminal);
}

/* Underline this character and move right. */
static void
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
		vte_invalidate_cells(terminal,
				     column, cell->columns,
				     screen->cursor_current.row, 1);
		/* Move the cursor right. */
		vte_sequence_handler_nd(terminal, match, match_quark, params);
	}
}

/* Underline end. */
static void
vte_sequence_handler_ue(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->screen->defaults.underline = 0;
}

/* Cursor up, no scrolling. */
static void
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
}

/* Cursor up N lines, no scrolling. */
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
	terminal->pvt->screen->defaults.underline = 1;
}

/* Visible bell. */
static void
vte_sequence_handler_vb(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	Display *display;
	GdkDrawable *gdrawable;
	GtkWidget *widget;
	Drawable drawable;
	GC gc;
	gint x_offs, y_offs;

	widget = GTK_WIDGET(terminal);
	if (GTK_WIDGET_REALIZED(widget)) {
		/* Fill the screen with the default foreground color, and then
		 * repaint everything, to provide visual bell. */
		gdk_window_get_internal_paint_info(widget->window,
						   &gdrawable,
						   &x_offs,
						   &y_offs);
		display = gdk_x11_drawable_get_xdisplay(gdrawable);
		drawable = gdk_x11_drawable_get_xid(gdrawable);
		gc = XCreateGC(display, drawable, 0, NULL);

		XSetForeground(display, gc,
			       terminal->pvt->palette[VTE_BOLD_FG].pixel);
		XFillRectangle(display, drawable, gc,
			       x_offs, y_offs,
			       terminal->column_count * terminal->char_width,
			       terminal->row_count * terminal->char_height);
		gdk_window_process_updates(widget->window, TRUE);

		vte_invalidate_all(terminal);
		gdk_window_process_updates(widget->window, TRUE);
	}
}

/* Cursor visible. */
static void
vte_sequence_handler_ve(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->cursor_visible = TRUE;
}

/* Cursor invisible. */
static void
vte_sequence_handler_vi(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->cursor_visible = FALSE;
}

/* Cursor standout. */
static void
vte_sequence_handler_vs(VteTerminal *terminal,
			const char *match,
			GQuark match_quark,
			GValueArray *params)
{
	terminal->pvt->cursor_visible = TRUE; /* FIXME: should be *more*
						 visible. */
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
			/* default foreground, underscore */
			terminal->pvt->screen->defaults.fore = VTE_DEF_FG;
			terminal->pvt->screen->defaults.underline = 1;
			break;
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
	/* Save the new colors. */
	terminal->pvt->screen->color_defaults.fore =
		terminal->pvt->screen->defaults.fore;
	terminal->pvt->screen->color_defaults.back =
		terminal->pvt->screen->defaults.back;
	terminal->pvt->screen->fill_defaults.fore =
		terminal->pvt->screen->defaults.fore;
	terminal->pvt->screen->fill_defaults.back =
		terminal->pvt->screen->defaults.back;
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
	screen = terminal->pvt->screen;
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
	for (i = screen->insert_delta; i < screen->cursor_current.row; i++) {
		if (_vte_ring_next(screen->row_data) > i) {
			/* Get the data for the row we're erasing. */
			rowdata = _vte_ring_index(screen->row_data, GArray*, i);
			/* Remove it. */
			if (rowdata->len > 0) {
				g_array_set_size(rowdata, 0);
			}
			/* Add new cells until we fill the row. */
			vte_g_array_fill(rowdata,
					 &screen->fill_defaults,
					 terminal->column_count);
			/* Repaint the row. */
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
	long i, initial;
	VteScreen *screen;
	screen = terminal->pvt->screen;
	initial = screen->insert_delta;
	/* Add a new screen's worth of rows. */
	for (i = 0; i < terminal->row_count; i++) {
		/* Add a new row */
		if (i == 0) {
			initial = _vte_ring_next(screen->row_data);
		}
		rowdata = vte_new_row_data_sized(terminal, TRUE);
		_vte_ring_append(screen->row_data, rowdata);
	}
	/* Move the cursor and insertion delta to the first line in the
	 * newly-cleared area and scroll if need be. */
	screen->insert_delta = initial;
	screen->cursor_current.row = initial;
	vte_terminal_adjust_adjustments(terminal, FALSE);
	/* Redraw everything. */
	vte_invalidate_all(terminal);
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

/* Request terminal attributes. */
static void
vte_sequence_handler_request_terminal_parameters(VteTerminal *terminal,
						 const char *match,
						 GQuark match_quark,
						 GValueArray *params)
{
	vte_terminal_feed_child(terminal, "[?x", -1);
}

/* Request terminal attributes. */
static void
vte_sequence_handler_return_terminal_status(VteTerminal *terminal,
					    const char *match,
					    GQuark match_quark,
					    GValueArray *params)
{
	vte_terminal_feed_child(terminal, "xterm", -1);
}

/* Send primary device attributes. */
static void
vte_sequence_handler_send_primary_device_attributes(VteTerminal *terminal,
						    const char *match,
						    GQuark match_quark,
						    GValueArray *params)
{
	/* Claim to be a VT220 with only national character set support. */
	vte_terminal_feed_child(terminal, "[?60;9c", -1);
}

/* Send secondary device attributes. */
static void
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
	ret = g_strdup_printf("[>1;%ld;0c", ver);
	vte_terminal_feed_child(terminal, ret, -1);
	g_free(ret);
}

/* Set icon/window titles. */
static void
vte_sequence_handler_set_title_internal(VteTerminal *terminal,
					const char *match,
					GQuark match_quark,
					GValueArray *params,
					const char *signal)
{
	GValue *value;
	GIConv conv;
	char *inbuf = NULL, *outbuf = NULL, *outbufptr = NULL;
	gsize inbuf_len, outbuf_len;
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
			/* Convert the unicode-character string into a
			 * multibyte string. */
			conv = g_iconv_open("UTF-8", _vte_table_wide_encoding());
			inbuf = g_value_get_pointer(value);
			inbuf_len = vte_unicode_strlen((gunichar*)inbuf) *
				    sizeof(gunichar);
			outbuf_len = (inbuf_len * VTE_UTF8_BPC) + 1;
			_vte_buffer_set_minimum_size(terminal->pvt->conv_buffer,
						     outbuf_len);
			outbuf = outbufptr = terminal->pvt->conv_buffer->bytes;
			if (conv != ((GIConv) -1)) {
				if (g_iconv(conv, &inbuf, &inbuf_len,
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
				}
			}
			g_iconv_close(conv);
		}
		if (outbufptr != NULL) {
			/* Emit the signal */
			if (strcmp(signal, "window_title_changed") == 0) {
				g_free(terminal->window_title);
				terminal->window_title = g_strndup(outbufptr,
								   outbuf -
								   outbufptr);
				vte_terminal_emit_window_title_changed(terminal);
			} else
			if (strcmp(signal, "icon_title_changed") == 0) {
				g_free (terminal->icon_title);
				terminal->icon_title = g_strndup(outbufptr,
								 outbuf -
								 outbufptr);
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
	vte_sequence_handler_set_title_internal(terminal, match, match_quark,
						params, "icon_title_changed");
}
static void
vte_sequence_handler_set_window_title(VteTerminal *terminal,
				      const char *match,
				      GQuark match_quark,
				      GValueArray *params)
{
	vte_sequence_handler_set_title_internal(terminal, match, match_quark,
						params, "window_title_changed");
}

/* Set both the window and icon titles to the same string. */
static void
vte_sequence_handler_set_icon_and_window_title(VteTerminal *terminal,
						  const char *match,
						  GQuark match_quark,
						  GValueArray *params)
{
	vte_sequence_handler_set_title_internal(terminal, match, match_quark,
						params, "icon_title_changed");
	vte_sequence_handler_set_title_internal(terminal, match, match_quark,
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
	if (visible || !terminal->pvt->mouse_autohide) {
		if (terminal->pvt->mouse_send_xy_on_click ||
		    terminal->pvt->mouse_send_xy_on_button ||
		    terminal->pvt->mouse_hilite_tracking ||
		    terminal->pvt->mouse_cell_motion_tracking ||
		    terminal->pvt->mouse_all_motion_tracking) {
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_CURSOR)) {
				fprintf(stderr, "Setting mousing cursor.\n");
			}
#endif
			cursor = terminal->pvt->mouse_mousing_cursor;
		} else {
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_CURSOR)) {
				fprintf(stderr, "Setting default mouse "
					"cursor.\n");
			}
#endif
			cursor = terminal->pvt->mouse_default_cursor;
		}
	} else {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_CURSOR)) {
			fprintf(stderr, "Setting to invisible cursor.\n");
		}
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
				     int setting,
				     gboolean restore,
				     gboolean save,
				     gboolean set)
{
	gboolean recognized = FALSE;
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
#if 0
		/* 6: Origin mode. */
		{6, &terminal->pvt->screen->origin_mode, NULL, NULL,
		 GINT_TO_POINTER(FALSE),
		 GINT_TO_POINTER(TRUE),
		 NULL, NULL,},
#else
		{6, NULL, NULL, NULL, NULL, NULL, NULL, NULL,},
#endif
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

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	/* Handle the setting. */
	for (i = 0; i < G_N_ELEMENTS(settings); i++)
	if (settings[i].setting == setting) {
		recognized = TRUE;
		/* Handle settings we want to ignore. */
		if (settings[i].fvalue == settings[i].tvalue) {
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
						terminal->char_width,
						terminal->row_count *
						terminal->char_height);
		/* Request a resize and redraw. */
		vte_invalidate_all(terminal);
		break;
	case 5:
		/* Repaint everything in reverse mode. */
		vte_invalidate_all(terminal);
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
		vte_invalidate_cursor_once(terminal);
		break;
	case 47:
	case 1047:
	case 1049:
		/* Clear the alternate screen if we're switching
		 * to it. */
		if (set) {
			vte_sequence_handler_clear_screen(terminal,
							  NULL,
							  0,
							  NULL);
		}
		/* Reset scrollbars and repaint everything. */
		vte_terminal_adjust_adjustments(terminal, TRUE);
		vte_invalidate_all(terminal);
		/* Clear the matching view. */
		vte_terminal_match_contents_clear(terminal);
		/* Notify viewers that the contents have changed. */
		vte_terminal_emit_contents_changed(terminal);
		break;
	case 9:
	case 1000:
	case 1001:
	case 1002:
	case 1003:
		/* Make the pointer visible. */
		vte_terminal_set_pointer_visible(terminal, TRUE);
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
}

/* Set the application or normal keypad. */
static void
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
}

static void
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
}

/* Move the cursor. */
static void
vte_sequence_handler_character_position_absolute(VteTerminal *terminal,
						 const char *match,
						 GQuark match_quark,
						 GValueArray *params)
{
	vte_sequence_handler_offset(terminal, match, match_quark, params,
				    -1, vte_sequence_handler_ch);
}
static void
vte_sequence_handler_line_position_absolute(VteTerminal *terminal,
					    const char *match,
					    GQuark match_quark,
					    GValueArray *params)
{
	vte_sequence_handler_offset(terminal, match, match_quark, params,
				    -1, vte_sequence_handler_cv);
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
	case 12:	/* send/receive mode (local echo?) */
		break;
	case 20:	/* automatic newline / normal linefeed mode */
		break;
	default:
		break;
	}
}

/* Set certain terminal attributes. */
static void
vte_sequence_handler_set_mode(VteTerminal *terminal,
			      const char *match,
			      GQuark match_quark,
			      GValueArray *params)
{
	int i;
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
		vte_sequence_handler_set_mode_internal(terminal, setting,
						       TRUE);
	}
}

/* Unset certain terminal attributes. */
static void
vte_sequence_handler_reset_mode(VteTerminal *terminal,
			        const char *match,
			        GQuark match_quark,
			        GValueArray *params)
{
	int i;
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
		vte_sequence_handler_set_mode_internal(terminal, setting,
						       FALSE);
	}
}

/* Set certain terminal attributes. */
static void
vte_sequence_handler_decset(VteTerminal *terminal,
			    const char *match,
			    GQuark match_quark,
			    GValueArray *params)
{
	GValue *value;
	long setting;
	int i;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		vte_sequence_handler_decset_internal(terminal, setting,
						     FALSE, FALSE, TRUE);
	}
}

/* Unset certain terminal attributes. */
static void
vte_sequence_handler_decreset(VteTerminal *terminal,
			      const char *match,
			      GQuark match_quark,
			      GValueArray *params)
{
	GValue *value;
	long setting;
	int i;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		vte_sequence_handler_decset_internal(terminal, setting,
						     FALSE, FALSE, FALSE);
	}
}

/* Erase a specified number of characters. */
static void
vte_sequence_handler_erase_characters(VteTerminal *terminal,
				      const char *match,
				      GQuark match_quark,
				      GValueArray *params)
{
	vte_sequence_handler_ec(terminal, match, match_quark, params);
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
		/* Clear everything to the left of the cursor, too. */
		/* FIXME: vttest. */
		vte_sequence_handler_cb(terminal, NULL, 0, NULL);
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

/* Perform a full-bore reset. */
static void
vte_sequence_handler_full_reset(VteTerminal *terminal,
				const char *match,
				GQuark match_quark,
				GValueArray *params)
{
	vte_terminal_reset(terminal, TRUE, TRUE);
}

/* Insert a specified number of blank characters. */
static void
vte_sequence_handler_insert_blank_characters(VteTerminal *terminal,
					     const char *match,
					     GQuark match_quark,
					     GValueArray *params)
{
	vte_sequence_handler_IC(terminal, match, match_quark, params);
}

/* Insert a certain number of lines below the current cursor. */
static void
vte_sequence_handler_insert_lines(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	GArray *rowdata;
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
		rowdata = _vte_ring_index(screen->row_data, GArray*, row);
		/* Add enough cells to it so that it has the default colors. */
		vte_g_array_fill(rowdata,
				 &screen->fill_defaults,
				 terminal->column_count);
	}
	/* Update the display. */
	vte_terminal_scroll_region(terminal, row, end - row + 1, param);
	/* Adjust the scrollbars if necessary. */
	vte_terminal_adjust_adjustments(terminal, FALSE);
}

/* Delete certain lines from the scrolling region. */
static void
vte_sequence_handler_delete_lines(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	GValue *value;
	GArray *rowdata;
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
		rowdata = _vte_ring_index(screen->row_data, GArray*, end);
		/* Add enough cells to it so that it has the default colors. */
		vte_g_array_fill(rowdata,
				 &screen->fill_defaults,
				 terminal->column_count);
	}
	/* Update the display. */
	vte_terminal_scroll_region(terminal, row, end - row + 1, -param);
	/* Adjust the scrollbars if necessary. */
	vte_terminal_adjust_adjustments(terminal, FALSE);
}

/* Set the terminal encoding. */
static void
vte_sequence_handler_local_charset(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
	G_CONST_RETURN char *locale_encoding;
#ifdef VTE_DEFAULT_ISO_8859_1
	vte_terminal_set_encoding(terminal, _vte_table_narrow_encoding());
#else
	g_get_charset(&locale_encoding);
	vte_terminal_set_encoding(terminal, locale_encoding);
#endif
}

static void
vte_sequence_handler_utf_8_charset(VteTerminal *terminal,
				   const char *match,
				   GQuark match_quark,
				   GValueArray *params)
{
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
}

/* Restore a certain terminal attribute. */
static void
vte_sequence_handler_restore_mode(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	GValue *value;
	long setting;
	int i;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		vte_sequence_handler_decset_internal(terminal, setting,
						     TRUE, FALSE, FALSE);
	}
}

/* Save a certain terminal attribute. */
static void
vte_sequence_handler_save_mode(VteTerminal *terminal,
			       const char *match,
			       GQuark match_quark,
			       GValueArray *params)
{
	GValue *value;
	long setting;
	int i;
	if ((params == NULL) || (params->n_values == 0)) {
		return;
	}
	for (i = 0; i < params->n_values; i++) {
		value = g_value_array_get_nth(params, i);
		if (!G_VALUE_HOLDS_LONG(value)) {
			continue;
		}
		setting = g_value_get_long(value);
		vte_sequence_handler_decset_internal(terminal, setting,
						     FALSE, TRUE, FALSE);
	}
}

/* Perform a screen alignment test -- fill all visible cells with the
 * letter "E". */
static void
vte_sequence_handler_screen_alignment_test(VteTerminal *terminal,
					   const char *match,
					   GQuark match_quark,
					   GValueArray *params)
{
	long row;
	GArray *rowdata;
	VteScreen *screen;
	struct vte_charcell cell;

	screen = terminal->pvt->screen;

	for (row = terminal->pvt->screen->insert_delta;
	     row < terminal->pvt->screen->insert_delta + terminal->row_count;
	     row++) {
		/* Find this row. */
		while (_vte_ring_next(screen->row_data) <= row) {
			rowdata = vte_new_row_data(terminal);
			_vte_ring_append(screen->row_data, rowdata);
		}
		vte_terminal_adjust_adjustments(terminal, TRUE);
		rowdata = _vte_ring_index(screen->row_data, GArray*, row);
		/* Clear this row. */
		if (rowdata->len > 0) {
			g_array_set_size(rowdata, 0);
		}
		/* Fill this row. */
		cell = screen->basic_defaults;
		cell.c = 'E';
		cell.columns = 1;
		vte_g_array_fill(rowdata, &cell, terminal->column_count);
	}
	vte_invalidate_all(terminal);
}

/* Perform a soft reset. */
static void
vte_sequence_handler_soft_reset(VteTerminal *terminal,
				const char *match,
				GQuark match_quark,
				GValueArray *params)
{
	vte_terminal_reset(terminal, FALSE, FALSE);
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
								arg2,
								arg1);
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
			vte_invalidate_all(terminal);
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
								arg2 * terminal->char_width,
								arg1 * terminal->char_height);
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
			vte_terminal_feed_child(terminal,
						buf, strlen(buf));
			break;
		case 13:
			/* Send window location, in pixels. */
			gdk_window_get_origin(widget->window,
					      &width, &height);
			snprintf(buf, sizeof(buf),
				 "%s%d;%dt", _VTE_CAP_CSI,
				 width, height);
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting window location"
					"(%d,%d).\n",
					width, height);
			}
#endif
			vte_terminal_feed_child(terminal,
						buf, strlen(buf));
			break;
		case 14:
			/* Send window size, in pixels. */
			gdk_drawable_get_size(widget->window,
					      &width, &height);
			snprintf(buf, sizeof(buf),
				 "%s%d;%dt", _VTE_CAP_CSI,
				 height, width);
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting window size "
					"(%dx%d).\n", width, height);
			}
#endif
			vte_terminal_feed_child(terminal,
						buf, strlen(buf));
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
			vte_terminal_feed_child(terminal,
						buf, strlen(buf));
			break;
		case 19:
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting screen size.\n");
			}
#endif
			display = gdk_x11_drawable_get_xdisplay(widget->window);
			i = gdk_x11_get_default_screen();
			snprintf(buf, sizeof(buf),
				 "%s%ld;%ldt", _VTE_CAP_CSI,
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
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				fprintf(stderr, "Reporting icon title.\n");
			}
#endif
			snprintf(buf, sizeof(buf),
				 "%sL%s%s",
				 _VTE_CAP_OSC,
				 terminal->icon_title ?
				 terminal->icon_title : "",
				 _VTE_CAP_ST);
			vte_terminal_feed_child(terminal,
						buf, strlen(buf));
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
				 terminal->window_title ?
				 terminal->window_title : "",
				 _VTE_CAP_ST);
			vte_terminal_feed_child(terminal,
						buf, strlen(buf));
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
								terminal->column_count * terminal->char_width,
								param * terminal->char_height);
			}
			break;
		}
	}
}

/* Complain that we got an escape sequence that's actually a keystroke. */
static void
vte_sequence_handler_complain_key(VteTerminal *terminal,
				  const char *match,
				  GQuark match_quark,
				  GValueArray *params)
{
	g_warning(_("Got unexpected (key?) sequence `%s'."),
		  match ? match : "???");
}

/* The table of handlers.  Primarily used at initialization time. */
static struct {
	const char *code;
	VteTerminalSequenceHandler handler;
} vte_sequence_handlers[] = {
	{"!1", vte_sequence_handler_complain_key},
	{"!2", vte_sequence_handler_complain_key},
	{"!3", vte_sequence_handler_complain_key},

	{"#1", vte_sequence_handler_complain_key},
	{"#2", vte_sequence_handler_complain_key},
	{"#3", vte_sequence_handler_complain_key},
	{"#4", vte_sequence_handler_complain_key},

	{"%1", vte_sequence_handler_complain_key},
	{"%2", vte_sequence_handler_complain_key},
	{"%3", vte_sequence_handler_complain_key},
	{"%4", vte_sequence_handler_complain_key},
	{"%5", vte_sequence_handler_complain_key},
	{"%6", vte_sequence_handler_complain_key},
	{"%7", vte_sequence_handler_complain_key},
	{"%8", vte_sequence_handler_complain_key},
	{"%9", vte_sequence_handler_complain_key},
	{"%a", vte_sequence_handler_complain_key},
	{"%b", vte_sequence_handler_complain_key},
	{"%c", vte_sequence_handler_complain_key},
	{"%d", vte_sequence_handler_complain_key},
	{"%e", vte_sequence_handler_complain_key},
	{"%f", vte_sequence_handler_complain_key},
	{"%g", vte_sequence_handler_complain_key},
	{"%h", vte_sequence_handler_complain_key},
	{"%i", vte_sequence_handler_complain_key},
	{"%j", vte_sequence_handler_complain_key},

	{"&0", vte_sequence_handler_complain_key},
	{"&1", vte_sequence_handler_complain_key},
	{"&2", vte_sequence_handler_complain_key},
	{"&3", vte_sequence_handler_complain_key},
	{"&4", vte_sequence_handler_complain_key},
	{"&5", vte_sequence_handler_complain_key},
	{"&6", vte_sequence_handler_complain_key},
	{"&7", vte_sequence_handler_complain_key},
	{"&8", vte_sequence_handler_complain_key},
	{"&9", vte_sequence_handler_complain_key},

	{"*0", vte_sequence_handler_complain_key},
	{"*1", vte_sequence_handler_complain_key},
	{"*2", vte_sequence_handler_complain_key},
	{"*3", vte_sequence_handler_complain_key},
	{"*4", vte_sequence_handler_complain_key},
	{"*5", vte_sequence_handler_complain_key},
	{"*6", vte_sequence_handler_complain_key},
	{"*7", vte_sequence_handler_complain_key},
	{"*8", vte_sequence_handler_complain_key},
	{"*9", vte_sequence_handler_complain_key},

	{"@0", vte_sequence_handler_complain_key},
	{"@1", vte_sequence_handler_complain_key},
	{"@2", vte_sequence_handler_complain_key},
	{"@3", vte_sequence_handler_complain_key},
	{"@4", vte_sequence_handler_complain_key},
	{"@5", vte_sequence_handler_complain_key},
	{"@6", vte_sequence_handler_complain_key},
	{"@7", vte_sequence_handler_complain_key},
	{"@8", vte_sequence_handler_complain_key},
	{"@9", vte_sequence_handler_complain_key},

	{"al", vte_sequence_handler_al},
	{"AL", vte_sequence_handler_AL},
	{"ae", vte_sequence_handler_ae},
	{"as", vte_sequence_handler_as},

	{"bc", vte_sequence_handler_le},
	{"bl", vte_sequence_handler_bl},
	{"bt", vte_sequence_handler_bt},

	{"cb", vte_sequence_handler_cb},
	{"cc", vte_sequence_handler_noop},
	{"cd", vte_sequence_handler_cd},
	{"ce", vte_sequence_handler_ce},
	{"ch", vte_sequence_handler_ch},
	{"cl", vte_sequence_handler_cl},
	{"cm", vte_sequence_handler_cm},
	{"cr", vte_sequence_handler_cr},
	{"cs", vte_sequence_handler_cs},
	{"cS", vte_sequence_handler_cS},
	{"ct", vte_sequence_handler_ct},
	{"cv", vte_sequence_handler_cv},

	{"dc", vte_sequence_handler_dc},
	{"DC", vte_sequence_handler_DC},
	{"dl", vte_sequence_handler_dl},
	{"DL", vte_sequence_handler_DL},
	{"dm", vte_sequence_handler_noop},
	{"do", vte_sequence_handler_do},
	{"DO", vte_sequence_handler_DO},
	{"ds", NULL},

	{"eA", vte_sequence_handler_eA},
	{"ec", vte_sequence_handler_ec},
	{"ed", vte_sequence_handler_noop},
	{"ei", vte_sequence_handler_ei},

	{"ff", vte_sequence_handler_noop},
	{"fs", vte_sequence_handler_fs},
	{"F1", vte_sequence_handler_complain_key},
	{"F2", vte_sequence_handler_complain_key},
	{"F3", vte_sequence_handler_complain_key},
	{"F4", vte_sequence_handler_complain_key},
	{"F5", vte_sequence_handler_complain_key},
	{"F6", vte_sequence_handler_complain_key},
	{"F7", vte_sequence_handler_complain_key},
	{"F8", vte_sequence_handler_complain_key},
	{"F9", vte_sequence_handler_complain_key},
	{"FA", vte_sequence_handler_complain_key},
	{"FB", vte_sequence_handler_complain_key},
	{"FC", vte_sequence_handler_complain_key},
	{"FD", vte_sequence_handler_complain_key},
	{"FE", vte_sequence_handler_complain_key},
	{"FF", vte_sequence_handler_complain_key},
	{"FG", vte_sequence_handler_complain_key},
	{"FH", vte_sequence_handler_complain_key},
	{"FI", vte_sequence_handler_complain_key},
	{"FJ", vte_sequence_handler_complain_key},
	{"FK", vte_sequence_handler_complain_key},
	{"FL", vte_sequence_handler_complain_key},
	{"FM", vte_sequence_handler_complain_key},
	{"FN", vte_sequence_handler_complain_key},
	{"FO", vte_sequence_handler_complain_key},
	{"FP", vte_sequence_handler_complain_key},
	{"FQ", vte_sequence_handler_complain_key},
	{"FR", vte_sequence_handler_complain_key},
	{"FS", vte_sequence_handler_complain_key},
	{"FT", vte_sequence_handler_complain_key},
	{"FU", vte_sequence_handler_complain_key},
	{"FV", vte_sequence_handler_complain_key},
	{"FW", vte_sequence_handler_complain_key},
	{"FX", vte_sequence_handler_complain_key},
	{"FY", vte_sequence_handler_complain_key},
	{"FZ", vte_sequence_handler_complain_key},

	{"Fa", vte_sequence_handler_complain_key},
	{"Fb", vte_sequence_handler_complain_key},
	{"Fc", vte_sequence_handler_complain_key},
	{"Fd", vte_sequence_handler_complain_key},
	{"Fe", vte_sequence_handler_complain_key},
	{"Ff", vte_sequence_handler_complain_key},
	{"Fg", vte_sequence_handler_complain_key},
	{"Fh", vte_sequence_handler_complain_key},
	{"Fi", vte_sequence_handler_complain_key},
	{"Fj", vte_sequence_handler_complain_key},
	{"Fk", vte_sequence_handler_complain_key},
	{"Fl", vte_sequence_handler_complain_key},
	{"Fm", vte_sequence_handler_complain_key},
	{"Fn", vte_sequence_handler_complain_key},
	{"Fo", vte_sequence_handler_complain_key},
	{"Fp", vte_sequence_handler_complain_key},
	{"Fq", vte_sequence_handler_complain_key},
	{"Fr", vte_sequence_handler_complain_key},

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

	{"K1", vte_sequence_handler_complain_key},
	{"K2", vte_sequence_handler_complain_key},
	{"K3", vte_sequence_handler_complain_key},
	{"K4", vte_sequence_handler_complain_key},
	{"K5", vte_sequence_handler_complain_key},

	{"k0", vte_sequence_handler_complain_key},
	{"k1", vte_sequence_handler_complain_key},
	{"k2", vte_sequence_handler_complain_key},
	{"k3", vte_sequence_handler_complain_key},
	{"k4", vte_sequence_handler_complain_key},
	{"k5", vte_sequence_handler_complain_key},
	{"k6", vte_sequence_handler_complain_key},
	{"k7", vte_sequence_handler_complain_key},
	{"k8", vte_sequence_handler_complain_key},
	{"k9", vte_sequence_handler_complain_key},
	{"k;", vte_sequence_handler_complain_key},
	{"ka", vte_sequence_handler_complain_key},
	{"kA", vte_sequence_handler_complain_key},
	{"kb", vte_sequence_handler_kb},
	{"kB", vte_sequence_handler_complain_key},
	{"kC", vte_sequence_handler_complain_key},
	{"kd", vte_sequence_handler_complain_key},
	{"kD", vte_sequence_handler_complain_key},
	{"ke", vte_sequence_handler_ke},
	{"kE", vte_sequence_handler_complain_key},
	{"kF", vte_sequence_handler_complain_key},
	{"kh", vte_sequence_handler_complain_key},
	{"kH", vte_sequence_handler_complain_key},
	{"kI", vte_sequence_handler_complain_key},
	{"kl", vte_sequence_handler_complain_key},
	{"kL", vte_sequence_handler_complain_key},
	{"kM", vte_sequence_handler_complain_key},
	{"kN", vte_sequence_handler_complain_key},
	{"kP", vte_sequence_handler_complain_key},
	{"kr", vte_sequence_handler_complain_key},
	{"kR", vte_sequence_handler_complain_key},
	{"ks", vte_sequence_handler_ks},
	{"kS", vte_sequence_handler_complain_key},
	{"kt", vte_sequence_handler_complain_key},
	{"kT", vte_sequence_handler_complain_key},
	{"ku", vte_sequence_handler_complain_key},

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
	{"ll", vte_sequence_handler_ll},
	{"LO", NULL},

	{"mb", vte_sequence_handler_mb},
	{"MC", NULL},
	{"md", vte_sequence_handler_md},
	{"me", vte_sequence_handler_me},
	{"mh", vte_sequence_handler_mh},
	{"mk", vte_sequence_handler_mk},
	{"ML", NULL},
	{"mm", NULL},
	{"mo", NULL},
	{"mp", vte_sequence_handler_mp},
	{"mr", vte_sequence_handler_mr},
	{"MR", NULL},

	{"nd", vte_sequence_handler_nd},
	{"nw", vte_sequence_handler_nw},

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

	{"..sa", NULL},
	{"sa", NULL},
	{"SA", NULL},
	{"sc", vte_sequence_handler_sc},
	{"se", vte_sequence_handler_se},
	{"sf", vte_sequence_handler_sf},
	{"SF", vte_sequence_handler_SF},
	{"so", vte_sequence_handler_so},
	{"sr", vte_sequence_handler_sr},
	{"SR", vte_sequence_handler_SR},
	{"st", vte_sequence_handler_st},
	{"SX", NULL},

	{"ta", vte_sequence_handler_ta},
	{"te", vte_sequence_handler_noop},
	{"ti", vte_sequence_handler_noop},
	{"ts", vte_sequence_handler_ts},

	{"uc", vte_sequence_handler_uc},
	{"ue", vte_sequence_handler_ue},
	{"up", vte_sequence_handler_up},
	{"UP", vte_sequence_handler_UP},
	{"us", vte_sequence_handler_us},

	{"vb", vte_sequence_handler_vb},
	{"ve", vte_sequence_handler_ve},
	{"vi", vte_sequence_handler_vi},
	{"vs", vte_sequence_handler_vs},

	{"wi", NULL},

	{"XF", NULL},

	{"7-bit-controls", NULL},
	{"8-bit-controls", NULL},
	{"ansi-conformance-level-1", NULL},
	{"ansi-conformance-level-2", NULL},
	{"ansi-conformance-level-3", NULL},
	{"application-keypad", vte_sequence_handler_application_keypad},
	{"change-background-colors", NULL},
	{"change-color", NULL},
	{"change-cursor-colors", NULL},
	{"change-font-name", NULL},
	{"change-font-number", NULL},
	{"change-foreground-colors", NULL},
	{"change-highlight-colors", NULL},
	{"change-logfile", NULL},
	{"change-mouse-cursor-background-colors", NULL},
	{"change-mouse-cursor-foreground-colors", NULL},
	{"change-tek-background-colors", NULL},
	{"change-tek-foreground-colors", NULL},
	{"character-attributes", vte_sequence_handler_character_attributes},
	{"character-position-absolute", vte_sequence_handler_character_position_absolute},
	{"cursor-back-tab", vte_sequence_handler_bt},
	{"cursor-backward", vte_sequence_handler_LE},
	{"cursor-character-absolute", vte_sequence_handler_cursor_character_absolute},
	{"cursor-down", vte_sequence_handler_DO},
	{"cursor-forward-tabulation", vte_sequence_handler_ta},
	{"cursor-forward", vte_sequence_handler_RI},
	{"cursor-lower-left", vte_sequence_handler_cursor_lower_left},
	{"cursor-next-line", vte_sequence_handler_cursor_next_line},
	{"cursor-position", vte_sequence_handler_cursor_position},
	{"cursor-preceding-line", vte_sequence_handler_cursor_preceding_line},
	{"cursor-up", vte_sequence_handler_UP},
	{"dec-device-status-report", vte_sequence_handler_dec_device_status_report},
	{"dec-media-copy", NULL},
	{"decreset", vte_sequence_handler_decreset},
	{"decset", vte_sequence_handler_decset},
	{"delete-characters", vte_sequence_handler_DC},
	{"delete-lines", vte_sequence_handler_delete_lines},
	{"device-control-string", NULL},
	{"device-status-report", vte_sequence_handler_device_status_report},
	{"double-height-bottom-half", NULL},
	{"double-height-top-half", NULL},
	{"double-width", NULL},
	{"enable-filter-rectangle", NULL},
	{"enable-locator-reporting", NULL},
	{"end-of-guarded-area", NULL},
	{"erase-characters", vte_sequence_handler_erase_characters},
	{"erase-in-display", vte_sequence_handler_erase_in_display},
	{"erase-in-line", vte_sequence_handler_erase_in_line},
	{"full-reset", vte_sequence_handler_full_reset},
	{"horizontal-and-vertical-position", vte_sequence_handler_horizontal_and_vertical_position},
	{"index", vte_sequence_handler_index},
	{"initiate-hilite-mouse-tracking", NULL},
	{"insert-blank-characters", vte_sequence_handler_insert_blank_characters},
	{"insert-lines", vte_sequence_handler_insert_lines},
	{"invoke-g1-character-set-as-gr", NULL},
	{"invoke-g2-character-set-as-gr", NULL},
	{"invoke-g2-character-set", NULL},
	{"invoke-g3-character-set-as-gr", NULL},
	{"invoke-g3-character-set", NULL},
	{"iso8859-1-character-set", vte_sequence_handler_local_charset},
	{"line-position-absolute", vte_sequence_handler_line_position_absolute},
	{"media-copy", NULL},
	{"memory-lock", NULL},
	{"memory-unlock", NULL},
	{"next-line", vte_sequence_handler_next_line},
	{"normal-keypad", vte_sequence_handler_normal_keypad},
	{"repeat", NULL},
	{"request-locator-position", NULL},
	{"request-terminal-parameters", vte_sequence_handler_request_terminal_parameters},
	{"reset-mode", vte_sequence_handler_reset_mode},
	{"restore-cursor", vte_sequence_handler_rc},
	{"restore-mode", vte_sequence_handler_restore_mode},
	{"return-terminal-status", vte_sequence_handler_return_terminal_status},
	{"return-terminal-id", NULL},
	{"reverse-index", vte_sequence_handler_reverse_index},
	{"save-cursor", vte_sequence_handler_sc},
	{"save-mode", vte_sequence_handler_save_mode},
	{"screen-alignment-test", vte_sequence_handler_screen_alignment_test},
	{"scroll-down", NULL},
	{"scroll-up", NULL},
	{"select-character-protection", NULL},
	{"selective-erase-in-display", NULL},
	{"selective-erase-in-line", NULL},
	{"select-locator-events", NULL},
	{"send-primary-device-attributes", vte_sequence_handler_send_primary_device_attributes},
	{"send-secondary-device-attributes", vte_sequence_handler_send_secondary_device_attributes},
	{"set-conformance-level", NULL},
	{"set-icon-and-window-title", vte_sequence_handler_set_icon_and_window_title},
	{"set-icon-title", vte_sequence_handler_set_icon_title},
	{"set-mode", vte_sequence_handler_set_mode},
	{"set-scrolling-region", vte_sequence_handler_set_scrolling_region},
	{"set-window-title", vte_sequence_handler_set_window_title},
	{"single-shift-g2", NULL},
	{"single-shift-g3", NULL},
	{"single-width", NULL},
	{"soft-reset", vte_sequence_handler_soft_reset},
	{"start-of-guarded-area", NULL},
	{"tab-clear", vte_sequence_handler_tab_clear},
	{"tab-set", vte_sequence_handler_st},
	{"utf-8-character-set", vte_sequence_handler_utf_8_charset},
	{"window-manipulation", vte_sequence_handler_window_manipulation},
};

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
	return GTK_WIDGET(g_object_new(vte_terminal_get_type(), NULL));
}

/* Set up a palette entry with a more-or-less match for the requested color. */
static void
vte_terminal_set_color_internal(VteTerminal *terminal, int entry,
				const GdkColor *proposed)
{
	GtkWidget *widget;
	Display *display;
	GdkColormap *gcolormap;
	Colormap colormap;
	GdkVisual *gvisual;
	Visual *visual;
	GdkColor color;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(entry >= 0);
	g_return_if_fail(entry < G_N_ELEMENTS(terminal->pvt->palette));

	/* Save the requested color. */
	terminal->pvt->palette[entry].red = proposed->red;
	terminal->pvt->palette[entry].green = proposed->green;
	terminal->pvt->palette[entry].blue = proposed->blue;

	/* If we're not realized yet, there's nothing else we can do. */
	widget = GTK_WIDGET(terminal);
	if (!GTK_WIDGET_REALIZED(widget)) {
		return;
	}

	/* Get X11 attributes used by GDK for the widget. */
	display = GDK_DISPLAY();
	gcolormap = gtk_widget_get_colormap(widget);
	colormap = gdk_x11_colormap_get_xcolormap(gcolormap);
	gvisual = gtk_widget_get_visual(widget);
	visual = gdk_x11_visual_get_xvisual(gvisual);

	/* Create a working copy of what we want, which GDK will
	 * adjust below when it fills in the pixel value. */
	color = *proposed;

	/* Get a GDK color. */
	gdk_rgb_find_color(gcolormap, &color); /* fills in pixel */
	terminal->pvt->palette[entry].red = color.red;
	terminal->pvt->palette[entry].green = color.green;
	terminal->pvt->palette[entry].blue = color.blue;
	terminal->pvt->palette[entry].pixel = color.pixel;

#ifdef HAVE_XFT
	/* If we're using Xft, we need to allocate a RenderColor, too. */
	if ((terminal->pvt->render_method == VteRenderXft1) ||
	    (terminal->pvt->render_method == VteRenderXft2)) {
		XRenderColor *rcolor;
		XftColor *ftcolor;

		rcolor = &terminal->pvt->palette[entry].rcolor;
		ftcolor = &terminal->pvt->palette[entry].ftcolor;

		/* Fill the render color in with what we got from GDK,
		 * hopefully so that they match. */
		rcolor->red = color.red;
		rcolor->green = color.green;
		rcolor->blue = color.blue;
		rcolor->alpha = 0xffff;

		/* Try to allocate a color with Xft, otherwise fudge it. */
		if (XftColorAllocValue(display, visual, colormap,
				       rcolor, ftcolor) != 0) {
			terminal->pvt->palette[entry].ftcolor_allocated = TRUE;
		} else {
			ftcolor->color = *rcolor;
			ftcolor->pixel = color.pixel;
			terminal->pvt->palette[entry].ftcolor_allocated = FALSE;
		}
	}
#endif

	/* If we're setting the background color, set the background color
	 * on the widget as well. */
	if ((entry == VTE_DEF_BG)) {
		vte_terminal_setup_background(terminal, FALSE);
	}
}

static void
vte_terminal_generate_bold(const struct vte_palette_entry *foreground,
			   const struct vte_palette_entry *background,
			   double factor,
			   GdkColor *bold)
{
	double fy, fcb, fcr, by, bcb, bcr, r, g, b;
	g_return_if_fail(foreground != NULL);
	g_return_if_fail(background != NULL);
	g_return_if_fail(bold != NULL);
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
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Calculated bold (%d, %d, %d) = (%lf,%lf,%lf)",
			foreground->red, foreground->green, foreground->blue,
			r, g, b);
	}
#endif
	bold->red = CLAMP(r, 0, 0xffff);
	bold->green = CLAMP(g, 0, 0xffff);
	bold->blue = CLAMP(b, 0, 0xffff);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "= (%04x,%04x,%04x).\n",
			bold->red, bold->green, bold->blue);
	}
#endif
}

/**
 * vte_terminal_set_color_bold
 * @terminal: a #VteTerminal
 * @bold: the new bold color
 *
 * Sets the color used to draw bold text in the default foreground color.
 *
 */
void
vte_terminal_set_color_bold(VteTerminal *terminal, const GdkColor *bold)
{
	vte_terminal_set_color_internal(terminal, VTE_BOLD_FG, bold);
}

/**
 * vte_terminal_set_color_dim
 * @terminal: a #VteTerminal
 * @dim: the new dim color
 *
 * Sets the color used to draw dim text in the default foreground color.
 *
 */
void
vte_terminal_set_color_dim(VteTerminal *terminal, const GdkColor *dim)
{
	vte_terminal_set_color_internal(terminal, VTE_DIM_FG, dim);
}

/**
 * vte_terminal_set_color_foreground
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
	vte_terminal_set_color_internal(terminal, VTE_DEF_FG, foreground);
}

/**
 * vte_terminal_set_color_background
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
	vte_terminal_set_color_internal(terminal, VTE_DEF_BG, background);
}

/**
 * vte_terminal_set_colors
 * @terminal: a #VteTerminal
 * @foreground: the new foreground color, or #NULL
 * @background: the new background color, or #NULL
 * @palette: the color palette
 * @palette_size: the number of entries in @palette
 *
 * The terminal widget uses a 28-color model comprised of the default foreground
 * and background colors, the bold foreground color, the dim foreground
 * color, an eight color palette, and bold versions of the eight color palette,
 * and a dim version of the the eight color palette.
 *
 * @palette_size must be either 0, 8, 16, or 24.  If @foreground is NULL and
 * @palette_size is greater than 0, the new foreground color is taken from
 * @palette[7].  If @background is NULL and @palette_size is greater than 0,
 * the new background color is taken from @palette[0].  If
 * @palette_size is 8 or 16, the third (dim) and possibly second (bold)
 * 8-color palette is extrapolated from the new background color and the items
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
	int i;
	GdkColor color;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	g_return_if_fail(palette_size >= 0);
	g_return_if_fail((palette_size == 0) ||
			 (palette_size == 8) ||
			 (palette_size == 16) ||
			 (palette_size == G_N_ELEMENTS(terminal->pvt->palette)));

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
	for (i = 0; (i < G_N_ELEMENTS(terminal->pvt->palette)); i++) {
		switch (i) {
		case VTE_DEF_FG:
			if (foreground != NULL) {
				color = *foreground;
			} else {
				color.red = 0xc000;
				color.blue = 0xc000;
				color.green = 0xc000;
			}
			break;
		case VTE_DEF_BG:
			if (background != NULL) {
				color = *background;
			} else {
				color.red = 0;
				color.blue = 0;
				color.green = 0;
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
		case 0 + 0:
		case 0 + 1:
		case 0 + 2:
		case 0 + 3:
		case 0 + 4:
		case 0 + 5:
		case 0 + 6:
		case 0 + 7:
		case 8 + 0:
		case 8 + 1:
		case 8 + 2:
		case 8 + 3:
		case 8 + 4:
		case 8 + 5:
		case 8 + 6:
		case 8 + 7:
			color.blue = (i & 4) ? 0xc000 : 0;
			color.green = (i & 2) ? 0xc000 : 0;
			color.red = (i & 1) ? 0xc000 : 0;
			if (i > 8) {
				color.blue += 0x3fff;
				color.green += 0x3fff;
				color.red += 0x3fff;
			}
			break;
		case 16 + 0:
		case 16 + 1:
		case 16 + 2:
		case 16 + 3:
		case 16 + 4:
		case 16 + 5:
		case 16 + 6:
		case 16 + 7:
			color.blue = (i & 4) ? 0x8000 : 0;
			color.green = (i & 2) ? 0x8000 : 0;
			color.red = (i & 1) ? 0x8000 : 0;
			break;
		default:
			g_assert_not_reached();
			break;
		}

		/* Override from the supplied palette if there is one. */
		if (i < palette_size) {
			color = palette[i];
		}

		/* Set up the color entry. */
		vte_terminal_set_color_internal(terminal, i, &color);
	}

	/* We may just have changed the default background color, so queue
	 * a repaint of the entire viewable area. */
	vte_invalidate_all(terminal);

	/* Track that we had a color palette set. */
	terminal->pvt->palette_initialized = TRUE;
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
	vte_terminal_set_colors(terminal, NULL, NULL, NULL, 0);
}

/* Insert a single character into the stored data array. */
static void
vte_terminal_insert_char(VteTerminal *terminal, gunichar c,
			 gboolean force_insert_mode, gboolean invalidate_cells,
			 gboolean paint_cells, gint forced_width)
{
	GArray *array;
	struct vte_charcell cell, *pcell;
	int columns, i;
	long col;
	VteScreen *screen;
	gboolean insert, clean;

	screen = terminal->pvt->screen;
	insert = screen->insert_mode || force_insert_mode;
	invalidate_cells = insert || invalidate_cells;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_IO) && _vte_debug_on(VTE_DEBUG_PARSE)) {
		fprintf(stderr, "Inserting %ld %c (%d/%d)(%d), delta = %ld, ",
			(long)c,
			c < 256 ? c : ' ',
			screen->defaults.fore, screen->defaults.back,
			vte_unichar_width(terminal, c),
			(long)screen->insert_delta);
	}
#endif
	/* If this character is destined for the status line, save it. */
	if (terminal->pvt->screen->status_line) {
		g_string_append_unichar(terminal->pvt->screen->status_line_contents,
					c);
		vte_terminal_emit_status_line_changed(terminal);
		return;
	}

	/* Figure out how many columns this character should occupy. */
	if (forced_width == -1) {
		columns = vte_unichar_width(terminal, c);
	} else {
		columns = forced_width;
	}

	/* If we're autowrapping here, do it. */
	col = screen->cursor_current.col;
	if (col + columns > terminal->column_count) {
		if (terminal->pvt->flags.am) {
			/* Wrap. */
			screen->cursor_current.col = 0;
			screen->cursor_current.row++;
		} else {
			/* Don't wrap, stay at the rightmost column. */
			screen->cursor_current.col = terminal->column_count -
						     columns;
		}
	}

	/* Make sure we have enough rows to hold this data. */
	vte_terminal_ensure_cursor(terminal, FALSE);

	/* Get a handle on the array for the insertion row. */
	array = _vte_ring_index(screen->row_data,
			        GArray*,
			        screen->cursor_current.row);

	/* Insert the right number of columns. */
	for (i = 0; i < columns; i++) {
		col = screen->cursor_current.col;

		/* Make sure we have enough columns in this row. */
		if (array->len <= col) {
			/* Add enough cells to fill out the row to at least out
			 * to (and including) the insertion point. */
			if (paint_cells) {
				vte_g_array_fill(array,
						 &screen->color_defaults,
						 col + 1);
			} else {
				vte_g_array_fill(array,
						 &screen->basic_defaults,
						 col + 1);
			}
			clean = FALSE;
		} else {
			/* If we're in insert mode, insert a new cell here
			 * and use it. */
			if (insert) {
				cell = screen->color_defaults;
				g_array_insert_val(array, col, cell);
				clean = FALSE;
			} else {
				/* We're in overtype mode, so we can use the
				 * existing character. */
				clean = TRUE;
			}
		}

		/* Set the character cell's attributes to match the current
		 * defaults, preserving any previous contents. */
		cell = g_array_index(array, struct vte_charcell, col);
		pcell = &g_array_index(array, struct vte_charcell, col);
		*pcell = screen->defaults;
		if (!paint_cells) {
			pcell->fore = cell.fore;
			pcell->back = cell.back;
		}
		pcell->c = cell.c;
		pcell->columns = cell.columns;
		pcell->fragment = cell.fragment;

		/* Now set the character and column count. */
		if (i == 0) {
			/* This is an entire character or the first column of
			 * a multi-column character. */
			if ((pcell->c != 0) &&
			    (c == '_') &&
			    (terminal->pvt->flags.ul)) {
				/* Handle overstrike-style underlining. */
				pcell->underline = 1;
			} else {
				/* Insert the character. */
				pcell->c = c;
				pcell->columns = columns;
				pcell->fragment = 0;
			}
		} else {
			/* This is a continuation cell. */
			pcell->c = c;
			pcell->columns = columns;
			pcell->fragment = 1;
		}

		/* And take a step to the to the right. */
		screen->cursor_current.col++;

		/* Make sure we're not getting random stuff past the right
		 * edge of the screen at this point, because the user can't
		 * see it. */
		if (array->len > terminal->column_count) {
			g_array_set_size(array, terminal->column_count);
		}
	}

	/* Signal that this part of the window needs drawing. */
	if (invalidate_cells) {
		col = screen->cursor_current.col - columns;
		if (insert) {
			vte_invalidate_cells(terminal,
					     col, terminal->column_count - col,
					     screen->cursor_current.row, 1);
		} else {
			vte_invalidate_cells(terminal,
					     col, columns,
					     screen->cursor_current.row, 1);
		}
	}

	/* Make sure the location the cursor is on exists. */
	vte_terminal_ensure_cursor(terminal, FALSE);

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_IO) && _vte_debug_on(VTE_DEBUG_PARSE)) {
		fprintf(stderr, "insertion delta => %ld.\n",
			(long)screen->insert_delta);
	}
#endif
}

#ifdef VTE_DEBUG
static void
display_control_sequence(const char *name, GValueArray *params)
{
	/* Display the control sequence with its parameters, to
	 * help me debug this thing.  I don't have all of the
	 * sequences implemented yet. */
	guint i;
	long l;
	const char *s;
	const gunichar *w;
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
				fprintf(stderr, "\"%ls\"", (const wchar_t*) w);
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
	struct vte_cursor_position position;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);
	screen = terminal->pvt->screen;

	/* This may generate multiple redraws, so freeze it while we do them. */
	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_freeze_updates(widget->window);
	}

	/* Save the cursor's current position for future use. */
	position = screen->cursor_current;

	/* Find the handler for this control sequence. */
	handler = g_tree_lookup(terminal->pvt->sequences, GINT_TO_POINTER(match));
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_PARSE)) {
		display_control_sequence(match_s, params);
	}
#endif
	if (handler != NULL) {
		/* Let the handler handle it. */
		handler(terminal, match_s, match, params);
	} else {
		g_warning(_("No handler for control sequence `%s' defined."),
			  match_s);
	}

	/* Let the updating begin. */
	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_thaw_updates(widget->window);
	}
}

/* Catch a VteReaper child-exited signal, and if it matches the one we're
 * looking for, emit one of our own. */
static void
vte_terminal_catch_child_exited(VteReaper *reaper, int pid, int status,
				VteTerminal *data)
{
	VteTerminal *terminal;
	g_return_if_fail(VTE_IS_TERMINAL(data));
	terminal = VTE_TERMINAL(data);
	if (pid == terminal->pvt->pty_pid) {
		vte_terminal_emit_child_exited(terminal);
	}
}

/**
 * vte_terminal_fork_command:
 * @terminal: a #VteTerminal
 * @command: the name of a binary to run
 * @argv: the argument list to be passed to @command
 * @envv: a list of environment variables to be added to the environment before
 * starting @command
 * @directory: the name of a directory the command should start in, or NULL
 * @lastlog: TRUE if the session should be logged to the lastlog
 * @utmp: TRUE if the session should be logged to the utmp/utmpx log
 * @wtmp: TRUE if the session should be logged to the wtmp/wtmpx log
 *
 * Starts the specified command under a newly-alllocated control
 * pseudo-terminal.  TERM is automatically set to reflect the terminal widget's
 * emulation setting.  If @lastlog, @utmp, or @wtmp are TRUE, logs the session
 * to the specified system log files.
 *
 * Returns: the ID of the new process
 */
pid_t
vte_terminal_fork_command(VteTerminal *terminal, const char *command,
			  char **argv, char **envv, const char *directory,
			  gboolean lastlog, gboolean utmp, gboolean wtmp)
{
	char **env_add;
	int i;
	pid_t pid;
	GtkWidget *widget;

	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	widget = GTK_WIDGET(terminal);

	/* Start up the command and get the PTY of the master. */
	for (i = 0; (envv != NULL) && (envv[i] != NULL); i++) ;

	env_add = g_malloc0(sizeof(char*) * (i + 2));
	if (command == NULL) {
		command = terminal->pvt->shell;
	}

	env_add[0] = g_strdup_printf("TERM=%s", terminal->pvt->emulation);
	for (i = 0; (envv != NULL) && (envv[i] != NULL); i++) {
		env_add[i + 1] = g_strdup(envv[i]);
	}
	env_add[i + 1] = NULL;

	if (terminal->pvt->pty_master != -1) {
		_vte_pty_close(terminal->pvt->pty_master);
	}
	terminal->pvt->pty_master = _vte_pty_open(&pid,
						  env_add,
						  command,
						  argv,
						  directory,
						  terminal->column_count,
						  terminal->row_count,
						  lastlog,
						  utmp,
						  wtmp);

	for (i = 0; env_add[i] != NULL; i++) {
		g_free(env_add[i]);
	}
	g_free(env_add);

	/* If we started the process, set up to listen for its output. */
	if (pid != -1) {
		/* Set this as the child's pid. */
		terminal->pvt->pty_pid = pid;

		/* Catch a child-exited signal from the child pid. */
		g_signal_connect(G_OBJECT(vte_reaper_get()), "child-exited",
				 G_CALLBACK(vte_terminal_catch_child_exited),
				 terminal);

		/* Set the pty to be non-blocking. */
		i = fcntl(terminal->pvt->pty_master, F_GETFL);
		fcntl(terminal->pvt->pty_master, F_SETFL, i | O_NONBLOCK);

		/* Set the PTY size. */
		vte_terminal_set_size(terminal,
				      terminal->column_count,
				      terminal->row_count);
		if (GTK_WIDGET_REALIZED(widget)) {
			gtk_widget_queue_resize(widget);
		}

		/* Open a channel to listen for input on. */
		terminal->pvt->pty_input =
			g_io_channel_unix_new(terminal->pvt->pty_master);
		terminal->pvt->pty_input_source =
			g_io_add_watch_full(terminal->pvt->pty_input,
					    VTE_CHILD_INPUT_PRIORITY,
					    G_IO_IN | G_IO_HUP,
					    vte_terminal_io_read,
					    terminal,
					    NULL);
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
		g_io_channel_unref(terminal->pvt->pty_input);
		terminal->pvt->pty_input = NULL;
		g_source_remove(terminal->pvt->pty_input_source);
		terminal->pvt->pty_input_source = -1;
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
 * themselves, but we're using gpointers to hold unicode character strings, and
 * we need to free those ourselves. */
static void
free_params_array(GValueArray *params)
{
	guint i;
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

/* Process incoming data, first converting it to unicode characters, and then
 * processing control sequences. */
static gboolean
vte_terminal_process_incoming(gpointer data)
{
	GValueArray *params = NULL;
	VteTerminal *terminal;
	VteScreen *screen;
	struct vte_cursor_position cursor;
	struct _vte_iso2022 *substitutions;
	gssize substitution_count;
	GtkWidget *widget;
	GdkRectangle rect;
	GdkPoint bbox_topleft, bbox_bottomright;
	char *ibuf, *obuf, *obufptr, *ubuf, *ubufptr;
	gsize icount, ocount, ucount;
	gunichar *wbuf, c;
	long wcount, start;
	const char *match, *encoding;
	GIConv unconv;
	GQuark quark;
	const gunichar *next;
	gboolean leftovers, modified, again, bottom;

	g_return_val_if_fail(GTK_IS_WIDGET(data), FALSE);
	g_return_val_if_fail(VTE_IS_TERMINAL(data), FALSE);
	widget = GTK_WIDGET(data);
	terminal = VTE_TERMINAL(data);

	bottom = (terminal->pvt->screen->insert_delta ==
		  terminal->pvt->screen->scroll_delta);

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_IO)) {
		fprintf(stderr, "Handler processing %d bytes.\n",
			_vte_buffer_length(terminal->pvt->incoming));
	}
#endif

	/* We should only be called when there's data to process. */
	g_assert(_vte_buffer_length(terminal->pvt->incoming) > 0);

	/* Try to convert the data into unicode characters. */
	ocount = sizeof(gunichar) * _vte_buffer_length(terminal->pvt->incoming);
	_vte_buffer_set_minimum_size(terminal->pvt->conv_buffer, ocount);
	obuf = obufptr = terminal->pvt->conv_buffer->bytes;
	icount = _vte_buffer_length(terminal->pvt->incoming);
	ibuf = terminal->pvt->incoming->bytes;

	/* Convert the data to unicode characters. */
	if (g_iconv(terminal->pvt->incoming_conv, &ibuf, &icount,
		    &obuf, &ocount) == -1) {
		/* No dice.  Try again when we have more data. */
		if ((errno == EILSEQ) && (_vte_buffer_length(terminal->pvt->incoming) >= icount)) {
			/* Discard the offending byte. */
			start = _vte_buffer_length(terminal->pvt->incoming) - icount;
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_IO)) {
				fprintf(stderr, "Error converting %ld incoming "
					"data bytes: %s, discarding byte %ld "
					"(0x%02x) and trying again.\n",
					(long) _vte_buffer_length(terminal->pvt->incoming),
					strerror(errno), start,
					terminal->pvt->incoming->bytes[start]);
			}
#endif
			terminal->pvt->incoming->bytes[start] = '?';
			/* Try again, before we try anything else.  To pull this
			 * off we add ourselves as a higher priority idle
			 * handler, and cause this handler to be dropped. */
			terminal->pvt->processing_tag =
					g_idle_add_full(VTE_INPUT_RETRY_PRIORITY,
							vte_terminal_process_incoming,
							terminal,
						NULL);
			return FALSE;
		}
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_IO)) {
			fprintf(stderr, "Error converting %ld incoming data "
				"bytes: %s, leaving for later.\n",
				(long) _vte_buffer_length(terminal->pvt->incoming),
				strerror(errno));
		}
#endif
		terminal->pvt->processing = FALSE;
		terminal->pvt->processing_tag = 0;
		return terminal->pvt->processing;
	}

	/* Store the current encoding. */
	encoding = terminal->pvt->encoding;

	/* Compute the number of unicode characters we got. */
	wcount = (obuf - obufptr) / sizeof(gunichar);
	wbuf = (gunichar*) obufptr;

	/* Initialize some state info we'll use to decide what to do next. */
	start = 0;
	modified = leftovers = again = FALSE;

	/* Perform ISO-2022 and XTerm national replacement charset
	 * substitutions. */
	if (terminal->pvt->nrc_mode) {
		substitutions = _vte_iso2022_copy(terminal->pvt->substitutions);
		substitution_count = _vte_iso2022_substitute(substitutions,
							     wbuf, wcount, wbuf,
							     terminal->pvt->table);
		if (substitution_count < 0) {
			_vte_iso2022_free(substitutions);
			leftovers = TRUE;
			again = FALSE;
		} else {
			_vte_iso2022_free(terminal->pvt->substitutions);
			terminal->pvt->substitutions = substitutions;
			wcount = substitution_count;
		}
	}

	/* Save the current cursor position. */
	screen = terminal->pvt->screen;
	cursor = screen->cursor_current;

	/* Estimate how much of the screen we'll need to update. */
	bbox_topleft.x = cursor.col;
	bbox_topleft.y = cursor.row;
	bbox_bottomright.x = cursor.col;
	bbox_bottomright.y = cursor.row;

	/* Try initial substrings. */
	while ((start < wcount) && !leftovers) {
		/* Try to match any control sequences. */
		_vte_table_match(terminal->pvt->table,
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
			vte_terminal_handle_sequence(GTK_WIDGET(terminal),
						     match,
						     quark,
						     params);
			/* Skip over the proper number of unicode chars. */
			start = (next - wbuf);
			/* Check if the encoding's changed. If it has, we need
			 * to force our caller to call us again to parse the
			 * rest of the data. */
			if (strcmp(encoding, terminal->pvt->encoding)) {
				leftovers = TRUE;
			}
			modified = TRUE;
		} else
		/* Second, we have a NULL match, and next points the very
		 * next character in the buffer.  Insert the character we're
		 * which we're currently examining. */
		if (match == NULL) {
			c = wbuf[start];
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PARSE)) {
				if (c > 255) {
					fprintf(stderr, "%ld\n", (long) c);
				} else {
					if (c > 127) {
						fprintf(stderr, "%ld = ",
							(long) c);
					}
					if (c < 32) {
						fprintf(stderr, "^%lc\n",
							(wint_t)c + 64);
					} else {
						fprintf(stderr, "`%lc'\n",
							(wint_t)c);
					}
				}
			}
#endif
			if (c != 0) {
				/* Insert the character. */
				vte_terminal_insert_char(terminal, c,
							 FALSE, FALSE, TRUE,
							 -1);
			}
			modified = TRUE;
			start++;
		} else {
			/* Case three: the read broke in the middle of a
			 * control sequence, so we're undecided with no more
			 * data to consult. If we have data following the
			 * middle of the sequence, then it's just garbage data,
			 * and for compatibility, we should discard it. */
			if (wbuf + wcount > next) {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_PARSE)) {
					fprintf(stderr, "Invalid control sequence, "
						"discarding %d characters.\n",
						next - (wbuf + start));
				}
#endif
				/* Discard. */
				start = next - wbuf + 1;
			} else {
				/* Pause processing and wait for more data. */
				leftovers = TRUE;
			}
		}

		/* Add the cell we just moved to the region we need to
		 * refresh for the user. */
		bbox_topleft.x = MIN(bbox_topleft.x,
				     screen->cursor_current.col);
		bbox_topleft.y = MIN(bbox_topleft.y,
				     screen->cursor_current.row);
		bbox_bottomright.x = MAX(bbox_bottomright.x,
					 screen->cursor_current.col);
		bbox_bottomright.y = MAX(bbox_bottomright.y,
					 screen->cursor_current.row);

#ifdef VTE_DEBUG
		/* Some safety checks: ensure the visible parts of the buffer
		 * are all in the buffer. */
		g_assert(screen->insert_delta >=
			 _vte_ring_delta(screen->row_data));
		/* The cursor shouldn't be above or below the addressable
		 * part of the display buffer. */
		g_assert(screen->cursor_current.row >= screen->insert_delta);
#endif

		/* Free any parameters we don't care about any more. */
		free_params_array(params);
		params = NULL;
	}

	/* Clip off any part of the box which isn't already on-screen. */
	bbox_topleft.x = MAX(bbox_topleft.x, 0);
	bbox_topleft.y = MAX(bbox_topleft.y, screen->scroll_delta);
	bbox_bottomright.x = MIN(bbox_bottomright.x,
				 terminal->column_count - 1);
	bbox_bottomright.y = MIN(bbox_bottomright.y,
				 screen->scroll_delta +
				 terminal->row_count);

	/* Update the screen to draw any modified areas.  This includes
	 * the current location of the cursor, so we won't need to redraw
	 * it below. */
	vte_invalidate_cells(terminal,
			     bbox_topleft.x,
			     bbox_bottomright.x - bbox_topleft.x + 1,
			     bbox_topleft.y,
			     bbox_bottomright.y - bbox_topleft.y + 1);

	if (leftovers) {
		/* There are leftovers, so convert them back to the terminal's
		 * old encoding and save them for later.  We can't use the
		 * scratch buffer here because it already holds ibuf. */
		unconv = g_iconv_open(encoding, _vte_table_wide_encoding());
		if (unconv != ((GIConv) -1)) {
			icount = sizeof(gunichar) * (wcount - start);
			ibuf = (char*) &wbuf[start];
			ucount = VTE_UTF8_BPC * (wcount - start) + 1;
			ubuf = ubufptr = g_malloc(ucount);
			if (g_iconv(unconv, &ibuf, &icount,
				    &ubuf, &ucount) != -1) {
				/* Store it. */
				_vte_buffer_clear(terminal->pvt->incoming);
				_vte_buffer_append(terminal->pvt->incoming,
						   ubufptr,
						   ubuf - ubufptr);
				/* If we're doing this because the encoding
				 * was changed out from under us, we need to
				 * keep trying to process the incoming data. */
				if (strcmp(encoding, terminal->pvt->encoding)) {
					again = TRUE;
				}
			} else {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_IO)) {
					fprintf(stderr, "Error unconverting %ld "
						"pending input bytes (%s), dropping.\n",
						(long) (sizeof(gunichar) * (wcount - start)),
						strerror(errno));
				}
#endif
				_vte_buffer_clear(terminal->pvt->incoming);
				again = FALSE;
			}
			g_free(ubufptr);
			g_iconv_close(unconv);
		} else {
			/* Discard the data, we can't use it. */
			_vte_buffer_clear(terminal->pvt->incoming);
			again = FALSE;
		}
	} else {
		/* No leftovers, clean out the data. */
		_vte_buffer_clear(terminal->pvt->incoming);
		again = FALSE;
	}

	if (modified) {
		/* Keep the cursor on-screen if we scroll on output, or if
		 * we're currently at the bottom of the buffer. */
		vte_terminal_update_insert_delta(terminal);
		if (terminal->pvt->scroll_on_output || bottom) {
			vte_terminal_maybe_scroll_to_bottom(terminal);
		}
		/* Deselect any existing selection. */
		vte_terminal_deselect_all(terminal);
	}

	if (modified || (screen != terminal->pvt->screen)) {
		/* Signal that the visible contents changed. */
		vte_terminal_match_contents_clear(terminal);
		/* Notify viewers that the contents have changed. */
		vte_terminal_emit_contents_changed(terminal);
	}

	if ((cursor.col != terminal->pvt->screen->cursor_current.col) ||
	    (cursor.row != terminal->pvt->screen->cursor_current.row)) {
		/* Signal that the cursor moved. */
		vte_terminal_emit_cursor_moved(terminal);
	}

	/* Tell the input method where the cursor is. */
	if (terminal->pvt->im_context) {
		rect.x = terminal->pvt->screen->cursor_current.col *
			 terminal->char_width + VTE_PAD_WIDTH;
		rect.width = terminal->char_width;
		rect.y = (terminal->pvt->screen->cursor_current.row -
			  terminal->pvt->screen->scroll_delta) *
			 terminal->char_height + VTE_PAD_WIDTH;
		rect.height = terminal->char_height;
		gtk_im_context_set_cursor_location(terminal->pvt->im_context,
						   &rect);
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_IO)) {
		fprintf(stderr, "%ld bytes left to process.\n",
			(long) _vte_buffer_length(terminal->pvt->incoming));
	}
#endif
	/* Decide if we're going to keep on processing data, and if not,
	 * note that our source tag is about to become invalid. */
	terminal->pvt->processing = again && (_vte_buffer_length(terminal->pvt->incoming) > 0);
	if (terminal->pvt->processing == FALSE) {
		terminal->pvt->processing_tag = 0;
	}
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_IO)) {
		if (terminal->pvt->processing) {
			fprintf(stderr, "Leaving processing handler on.\n");
		} else {
			fprintf(stderr, "Turning processing handler off.\n");
		}
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
	char buf[VTE_INPUT_CHUNK_SIZE];
	int bcount, fd;
	gboolean eof, leave_open = TRUE;

	g_return_val_if_fail(VTE_IS_TERMINAL(data), TRUE);
	widget = GTK_WIDGET(data);
	terminal = VTE_TERMINAL(data);

	/* If the terminal is selecting, then we need to stop reading input
	 * (for at least a moment) to keep data from scrolling off the top of
	 * our backscroll buffer, but come back later. */
	if (terminal->pvt->selecting) {
		return TRUE;
	}

	/* Check that the channel is still open. */
	fd = g_io_channel_unix_get_fd(channel);

	/* Read some data in from this channel. */
	bcount = 0;
	if (condition & G_IO_IN) {
		bcount = read(fd, buf, sizeof(buf));
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
				g_warning(_("Error reading from child: "
					    "%s."), strerror(errno));
				leave_open = TRUE;
				break;
		}
		break;
	default:
		break;
	}

	/* If we got data, modify the pending buffer. */
	if (bcount >= 0) {
		_vte_buffer_append(terminal->pvt->incoming, buf, bcount);
	} else {
		g_free(buf);
	}

	/* If we have data to process, schedule some time to process it. */
	if (!terminal->pvt->processing &&
	    (_vte_buffer_length(terminal->pvt->incoming) > 0)) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_IO)) {
			fprintf(stderr, "Queuing handler to process bytes.\n");
		}
#endif
		terminal->pvt->processing = TRUE;
		terminal->pvt->processing_tag =
				g_idle_add_full(VTE_INPUT_PRIORITY,
						vte_terminal_process_incoming,
						terminal,
						NULL);
	}

	/* If we detected an eof condition, signal one. */
	if (eof) {
		vte_terminal_eof(channel, terminal);
		leave_open = FALSE;
	}

	/* If there's more data coming, return TRUE, otherwise return FALSE. */
	return leave_open;
}

/**
 * vte_terminal_feed:
 * @terminal: a #VteTerminal
 * @data: a string
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

	/* If we got data, modify the pending buffer. */
	if (length >= 0) {
		_vte_buffer_append(terminal->pvt->incoming, data, length);
	}

	/* If we didn't have data before, but we do now, start processing it. */
	if (!terminal->pvt->processing &&
	    (_vte_buffer_length(terminal->pvt->incoming) > 0)) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_IO)) {
			fprintf(stderr,
				"Queuing handler to process %ld bytes.\n",
				(long) length);
		}
#endif
		terminal->pvt->processing = TRUE;
		terminal->pvt->processing_tag =
				g_idle_add_full(VTE_INPUT_PRIORITY,
						vte_terminal_process_incoming,
						terminal,
						NULL);
	}
}

/* Send locally-encoded characters to the child. */
static gboolean
vte_terminal_io_write(GIOChannel *channel,
		      GdkInputCondition condition,
		      gpointer data)
{
	VteTerminal *terminal;
	gssize count;
	int fd;
	gboolean leave_open;

	g_return_val_if_fail(VTE_IS_TERMINAL(data), FALSE);
	terminal = VTE_TERMINAL(data);

	fd = g_io_channel_unix_get_fd(channel);

	count = write(fd, terminal->pvt->outgoing->bytes,
		      _vte_buffer_length(terminal->pvt->outgoing));
	if (count != -1) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_IO)) {
			int i;
			for (i = 0; i < count; i++) {
				fprintf(stderr, "Wrote %c%c\n",
					((guint8)terminal->pvt->outgoing->bytes[i]) >= 32 ?
					' ' : '^',
					((guint8)terminal->pvt->outgoing->bytes[i]) >= 32 ?
					terminal->pvt->outgoing->bytes[i] :
					((guint8)terminal->pvt->outgoing->bytes[i])  + 64);
			}
		}
#endif
		_vte_buffer_consume(terminal->pvt->outgoing, count);
	}

	if (_vte_buffer_length(terminal->pvt->outgoing) == 0) {
		if (channel == terminal->pvt->pty_output) {
			g_io_channel_unref(terminal->pvt->pty_output);
			terminal->pvt->pty_output = NULL;
			g_source_remove(terminal->pvt->pty_output_source);
			terminal->pvt->pty_output_source = -1;
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
		  const void *data, gssize length)
{
	gssize icount, ocount;
	char *ibuf, *obuf, *obufptr;
	GIConv *conv;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_assert((strcmp(encoding, "UTF-8") == 0) ||
		 (strcmp(encoding, _vte_table_wide_encoding()) == 0));

	conv = NULL;
	if (strcmp(encoding, "UTF-8") == 0) {
		conv = &terminal->pvt->outgoing_conv_utf8;
	}
	if (strcmp(encoding, _vte_table_wide_encoding()) == 0) {
		conv = &terminal->pvt->outgoing_conv_wide;
	}
	g_assert(conv != NULL);
	g_assert(*conv != ((GIConv) -1));

	icount = length;
	ibuf = (char *) data;
	ocount = ((length + 1) * VTE_UTF8_BPC) + 1;
	_vte_buffer_set_minimum_size(terminal->pvt->conv_buffer, ocount);
	obuf = obufptr = terminal->pvt->conv_buffer->bytes;

	if (g_iconv(*conv, &ibuf, &icount, &obuf, &ocount) == -1) {
		g_warning(_("Error (%s) converting data for child, dropping."),
			  strerror(errno));
	} else {
		/* Tell observers that we're sending this to the child. */
		if (obuf - obufptr > 0) {
			vte_terminal_emit_commit(terminal,
						 obufptr, obuf - obufptr);
		}
		/* If there's a place for it to go, add the data to the
		 * outgoing buffer. */
		if (terminal->pvt->pty_master != -1) {
			_vte_buffer_append(terminal->pvt->outgoing,
					   obufptr, obuf - obufptr);
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_KEYBOARD)) {
				while (obufptr < obuf) {
					if ((((guint8) obufptr[0]) < 32) ||
					    (((guint8) obufptr[0]) > 127)) {
						fprintf(stderr,
							"Sending <%02x> "
							"to child.\n",
							obufptr[0]);
					} else {
						fprintf(stderr,
							"Sending '%c' "
							"to child.\n",
							obufptr[0]);
					}
					obufptr++;
				}
			}
#endif
			/* If we need to start waiting for the child pty to
			 * become available for writing, set that up here. */
			if (terminal->pvt->pty_output == NULL) {
				terminal->pvt->pty_output =
					g_io_channel_unix_new(terminal->pvt->pty_master);
				terminal->pvt->pty_output_source =
					g_io_add_watch_full(terminal->pvt->pty_output,
							    VTE_CHILD_OUTPUT_PRIORITY,
							    G_IO_OUT,
							    vte_terminal_io_write,
							    terminal,
							    NULL);
			}
		}
	}
	return;
}

/**
 * vte_terminal_feed_child:
 * @terminal: a #VteTerminal
 * @data: data to send to the child
 * @length: length of @text
 *
 * Sends a block of UTF-8 text to the child as if it were entered by the user
 * at the keyboard.
 *
 */
void
vte_terminal_feed_child(VteTerminal *terminal, const char *data, glong length)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (length == ((gssize)-1)) {
		length = strlen(data);
	}
	vte_terminal_im_reset(terminal);
	if (length > 0) {
		vte_terminal_send(terminal, "UTF-8", data, length);
	}
}

/* Send text from the input method to the child. */
static void
vte_terminal_im_commit(GtkIMContext *im_context, gchar *text, gpointer data)
{
	VteTerminal *terminal;

	g_return_if_fail(VTE_IS_TERMINAL(data));
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		fprintf(stderr, "Input method committed `%s'.\n", text);
	}
#endif
	terminal = VTE_TERMINAL(data);
	vte_terminal_feed_child(terminal, text, -1);
	/* Committed text was committed because the user pressed a key, so
	 * we need to obey the scroll-on-keystroke setting. */
	if (terminal->pvt->scroll_on_keystroke) {
		vte_terminal_maybe_scroll_to_bottom(terminal);
	}
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
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		fprintf(stderr, "Input method pre-edit started.\n");
	}
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
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		fprintf(stderr, "Input method pre-edit ended.\n");
	}
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
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		fprintf(stderr, "Input method pre-edit changed (%s,%d).\n",
			str, cursor);
	}
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
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		fprintf(stderr, "Top level parent configured.\n");
	}
#endif
	g_return_val_if_fail(GTK_IS_WIDGET(widget), FALSE);
	g_return_val_if_fail(GTK_WIDGET_TOPLEVEL(widget), FALSE);
	g_return_val_if_fail(VTE_IS_TERMINAL(data), FALSE);

	/* In case we moved, queue a background image update. */
	if (VTE_TERMINAL(data)->pvt->bg_transparent) {
		vte_terminal_queue_background_update(VTE_TERMINAL(data));
	}

	/* In case we were resized, repaint everything, including any extra
	 * regions which no cell covers. */
	vte_invalidate_all(VTE_TERMINAL(data));

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
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		fprintf(stderr, "Hierarchy changed.\n");
	}
#endif

	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);

	if (GTK_IS_WIDGET(old_toplevel)) {
		g_signal_handlers_disconnect_by_func(G_OBJECT(old_toplevel),
						     (gpointer)vte_terminal_configure_toplevel,
						     terminal);
	}

	toplevel = gtk_widget_get_toplevel(widget);
	if (GTK_IS_WIDGET(toplevel)) {
		g_signal_connect(G_OBJECT(toplevel), "configure-event",
				 G_CALLBACK(vte_terminal_configure_toplevel),
				 terminal);
	}
}

/* Handle a style-changed signal. */
static void
vte_terminal_style_changed(GtkWidget *widget, GtkStyle *style, gpointer data)
{
	VteTerminal *terminal;
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);
	/* If the font we're using is the same as the old default, then we
	 * need to pick up the new default. */
	if (pango_font_description_equal(style->font_desc,
					 widget->style->font_desc) ||
	    (terminal->pvt->fontdesc == NULL)) {
		vte_terminal_set_font(terminal, NULL);
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
	struct timeval tv;
	struct timezone tz;
	gboolean scrolled = FALSE, steal = FALSE, modifier = FALSE, handled,
		 suppress_meta_esc = FALSE;
	VteKeymode keypad_mode = VTE_KEYMODE_NORMAL,
		   cursor_mode = VTE_KEYMODE_NORMAL;
	guint keyval = 0;
	gunichar keychar = 0;
	char keybuf[VTE_UTF8_BPC];

	g_return_val_if_fail(widget != NULL, TRUE);
	g_return_val_if_fail(VTE_IS_TERMINAL(widget), TRUE);
	terminal = VTE_TERMINAL(widget);

	/* If it's a keypress, record that we got the event, in case the
	 * input method takes the event from us. */
	if (event->type == GDK_KEY_PRESS) {
		/* Store a copy of the key. */
		keyval = event->keyval;

		/* If we're in margin bell mode and on the border of the
		 * margin, bell. */
		if (terminal->pvt->margin_bell) {
			if ((terminal->pvt->screen->cursor_current.col +
			     terminal->pvt->bell_margin) ==
			    terminal->column_count) {
				vte_sequence_handler_bl(terminal,
							"bl",
							0,
							NULL);
			}
		}

		/* Log the time of the last keypress. */
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
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Keypress, modifiers=0x%x, "
				"keyval=0x%x, raw string=`%s'.\n",
				modifiers, keyval, event->string);
		}
#endif

		/* Determine what the keypad and modes are. */
		if ((modifiers & VTE_NUMLOCK_MASK) == VTE_NUMLOCK_MASK) {
			keypad_mode = VTE_KEYMODE_NORMAL;
		} else {
			keypad_mode = terminal->pvt->keypad_mode;
		}
		cursor_mode = terminal->pvt->cursor_mode;

		/* Determine if this is just a modifier key. */
		modifier = _vte_keymap_key_is_modifier(keyval);

		/* Unless it's a modifier key, hide the pointer. */
		if (!modifier) {
			vte_terminal_set_pointer_visible(terminal, FALSE);
		}

		/* We steal many keypad keys here. */
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
	}

	/* Let the input method at this one first. */
	if (!steal) {
		if (gtk_im_context_filter_keypress(terminal->pvt->im_context,
						   event)) {
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
				fprintf(stderr, "Keypress taken by IM.\n");
			}
#endif
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
				if (terminal->pvt->pty_master != -1) {
					if (tcgetattr(terminal->pvt->pty_master,
						      &tio) != -1) {
						normal = g_strdup_printf("%c",
									 tio.c_cc[VERASE]);
						normal_length = 1;
					}
				}
				break;
			}
			handled = TRUE;
			suppress_meta_esc = TRUE;
			break;
		case GDK_KP_Delete:
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
			handled = TRUE;
			suppress_meta_esc = TRUE;
			break;
		case GDK_KP_Insert:
		case GDK_Insert:
			if (modifiers & GDK_SHIFT_MASK) {
				vte_terminal_paste(terminal,
						   GDK_SELECTION_PRIMARY);
				handled = TRUE;
				suppress_meta_esc = TRUE;
			}
			break;
		/* Keypad/motion keys. */
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
		/* Let Shift +/- tweak the font, like XTerm does. */
		case GDK_KP_Add:
		case GDK_KP_Subtract:
			if (modifiers & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) {
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
		if (handled == FALSE) {
			_vte_keymap_map(keyval, modifiers,
					terminal->pvt->sun_fkey_mode,
					terminal->pvt->hp_fkey_mode,
					terminal->pvt->legacy_fkey_mode,
					terminal->pvt->vt220_fkey_mode,
					terminal->pvt->cursor_mode == VTE_KEYMODE_APPLICATION,
					terminal->pvt->keypad_mode == VTE_KEYMODE_APPLICATION,
					terminal->pvt->termcap,
					terminal->pvt->emulation ?
					terminal->pvt->emulation : "xterm",
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
		if (!handled && (normal == NULL) && (special == NULL)) {
			/* Convert the keyval to a gunichar. */
			keychar = gdk_keyval_to_unicode(keyval);
			normal_length = 0;
			if (keychar != 0) {
				/* Convert the gunichar to a string. */
				normal_length = g_unichar_to_utf8(keychar,
								  keybuf);
				if (normal_length != 0) {
					normal = g_malloc0(normal_length + 1);
					memcpy(normal, keybuf, normal_length);
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
#ifdef VTE_DEBUG
			if (normal && _vte_debug_on(VTE_DEBUG_EVENTS)) {
				fprintf(stderr, "Keypress, modifiers=0x%x, "
					"keyval=0x%x, cooked string=`%s'.\n",
					modifiers, keyval, normal);
			}
#endif
		}
		/* If we got normal characters, send them to the child. */
		if (normal != NULL) {
			if (terminal->pvt->meta_sends_escape &&
			    !suppress_meta_esc &&
			    (normal_length > 0) &&
			    (modifiers & VTE_META_MASK)) {
				vte_terminal_feed_child(terminal, "", 1);
			}
			if (normal_length > 0) {
				vte_terminal_feed_child(terminal,
							normal, normal_length);
			}
			g_free(normal);
		} else
		/* If the key maps to characters, send them to the child. */
		if (special != NULL) {
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
							  &normal,
							  &normal_length);
			output = g_strdup_printf(normal, 1);
			vte_terminal_feed_child(terminal, output, -1);
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

	return gtk_im_context_filter_keypress(terminal->pvt->im_context, event);
}

/**
 * vte_terminal_is_word_char:
 * @terminal: a #VteTerminal
 * @c: a candidate Unicode code point
 *
 * Checks if a particular character is considered to be part of a word or not,
 * based on the values last passed to vte_terminal_set_word_chars().
 *
 * Returns: TRUE if the character is considered to be part of a word
 */
gboolean
vte_terminal_is_word_char(VteTerminal *terminal, gunichar c)
{
	int i;
	VteWordCharRange *range;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	/* We need an array, even if it's empty. */
	if (terminal->pvt->word_chars == NULL) {
		return FALSE;
	}
	/* Go through each range and check if the character is included. */
	for (i = 0; i < terminal->pvt->word_chars->len; i++) {
		range = &g_array_index(terminal->pvt->word_chars,
				       VteWordCharRange,
				       i);
		if ((c >= range->start) && (c <= range->end)) {
			return TRUE;
		}
	}
	/* Special case:  if there are no ranges, assume the defaults. */
	if (i == 0) {
		return g_unichar_isgraph(c) && (!g_unichar_ispunct(c));
	}
	return FALSE;
}

/* Check if the characters in the given block are in the same class (word vs.
 * non-word characters). */
static gboolean
vte_uniform_class(VteTerminal *terminal, glong row, glong scol, glong ecol)
{
	struct vte_charcell *pcell = NULL;
	long col;
	gboolean word_char;
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	if ((pcell = vte_terminal_find_charcell(terminal, scol, row)) != NULL) {
		word_char = vte_terminal_is_word_char(terminal, pcell->c);
		for (col = scol + 1; col <= ecol; col++) {
			pcell = vte_terminal_find_charcell(terminal, col, row);
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

/* Check if the last visible column of the requested line and the first visible
 * column of the one after it contain non-whitespace characters. */
static gboolean
vte_line_is_wrappable(VteTerminal *terminal, glong row)
{
	struct vte_charcell *acell = NULL, *bcell = NULL;

	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);

	acell = vte_terminal_find_charcell(terminal,
					   terminal->column_count - 1,
					   row);
	bcell = vte_terminal_find_charcell(terminal,
					   0,
					   row + 1);
	if (acell && bcell) {
		return !g_unichar_isspace(acell->c) &&
		       !g_unichar_isspace(bcell->c);
	}
	return FALSE;
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
	if ((ss.y < 0) || (se.y < 0)) {
		return FALSE;
	}

	/* Now it boils down to whether or not the point is between the
	 * begin and endpoint of the selection. */
	return vte_cell_is_between(col, row, ss.x, ss.y, se.x, se.y, TRUE);
}

/* Once we get text data, actually paste it in. */
static void
vte_terminal_paste_cb(GtkClipboard *clipboard, const gchar *text, gpointer data)
{
	VteTerminal *terminal;
	gchar *paste, *p;
	long length;
	g_return_if_fail(VTE_IS_TERMINAL(data));
	terminal = VTE_TERMINAL(data);
	if (text != NULL) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
			fprintf(stderr, "Pasting %d UTF-8 bytes.\n",
				strlen(text));
		}
#endif
		vte_terminal_im_reset(terminal);
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
		vte_terminal_send(terminal, "UTF-8", paste, length);
		g_free(paste);
	}
}

/* Send a button down or up notification. */
static void
vte_terminal_send_mouse_button_internal(VteTerminal *terminal,
					int button,
					double x, double y,
					GdkModifierType modifiers)
{
	unsigned char cb = 0, cx = 0, cy = 0;
	char buf[LINE_MAX];

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

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
		cb = 64;	/* Scroll up. FIXME: check */
		break;
	case 5:
		cb = 65;	/* Scroll down. FIXME: check */
		break;
	}
	cb += 32; /* 32 for normal */

	/* Encode the modifiers. */
	if (modifiers & GDK_SHIFT_MASK) {
		cb |= 4;
	}
	if (modifiers & VTE_META_MASK) {
		cb |= 8;
	}
	if (modifiers & GDK_CONTROL_MASK) {
		cb |= 16;
	}

	/* Encode the cursor coordinates. */
	cx = 32 + 1 + (x / terminal->char_width);
	cy = 32 + 1 + (y / terminal->char_height);

	/* Send the event to the child. */
	snprintf(buf, sizeof(buf), _VTE_CAP_CSI "M%c%c%c", cb, cx, cy);
	vte_terminal_feed_child(terminal, buf, strlen(buf));
}

/* Send a mouse button click/release notification. */
static void
vte_terminal_maybe_send_mouse_button(VteTerminal *terminal,
				     GdkEventButton *event)
{
	GdkModifierType modifiers;

	/* Read the modifiers. */
	if (gdk_event_get_state((GdkEvent*)event, &modifiers) == FALSE) {
		modifiers = 0;
	}

	/* Decide whether or not to do anything. */
	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (!terminal->pvt->mouse_send_xy_on_button &&
		    !terminal->pvt->mouse_send_xy_on_click) {
			return;
		}
		break;
	case GDK_BUTTON_RELEASE:
		if (!terminal->pvt->mouse_send_xy_on_button) {
			return;
		}
		break;
	default:
		return;
		break;
	}

	/* Encode the parameters and send them to the app. */
	vte_terminal_send_mouse_button_internal(terminal,
						(event->type == GDK_BUTTON_PRESS) ?
						event->button : 0,
						event->x - VTE_PAD_WIDTH,
						event->y - VTE_PAD_WIDTH,
						modifiers);
}

/* Send a mouse motion notification. */
static void
vte_terminal_maybe_send_mouse_drag(VteTerminal *terminal, GdkEventMotion *event)
{
	unsigned char cb = 0, cx = 0, cy = 0;
	char buf[LINE_MAX];
	GdkModifierType modifiers;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	/* First determine if we even want to send notification. */
	switch (event->type) {
	case GDK_MOTION_NOTIFY:
		if (!terminal->pvt->mouse_cell_motion_tracking &&
		    !terminal->pvt->mouse_all_motion_tracking) {
			return;
		}
		if (terminal->pvt->mouse_cell_motion_tracking) {
			if (((event->x - VTE_PAD_WIDTH) /
			     terminal->char_width ==
			     terminal->pvt->mouse_last_x /
			     terminal->char_width) &&
			    ((event->y - VTE_PAD_WIDTH) /
			     terminal->char_height ==
			     terminal->pvt->mouse_last_y /
			     terminal->char_height)) {
				return;
			}
		}
		break;
	default:
		return;
		break;
	}


	/* Read the modifiers. */
	if (gdk_event_get_state((GdkEvent*)event, &modifiers) == FALSE) {
		modifiers = 0;
	}

	/* Encode the modifiers. */
	if (modifiers & GDK_SHIFT_MASK) {
		cb |= 4;
	}
	if (modifiers & VTE_META_MASK) {
		cb |= 8;
	}
	if (modifiers & GDK_CONTROL_MASK) {
		cb |= 16;
	}

	/* Encode which button we're being dragged with. */
	switch (terminal->pvt->mouse_last_button) {
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
	cb += 64; /* 32 for normal, 32 for movement */

	/* Encode the cursor coordinates. */
	cx = 32 + 1 + ((event->x - VTE_PAD_WIDTH + terminal->char_width / 2) /
	     terminal->char_width);
	cy = 32 + 1 + ((event->y - VTE_PAD_WIDTH) / terminal->char_height);

	/* Send the event to the child. */
	snprintf(buf, sizeof(buf), "%sM%c%c%c", _VTE_CAP_CSI, cb, cx, cy);
	vte_terminal_feed_child(terminal, buf, strlen(buf));
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
	if ((srow < erow) || ((srow == erow) && (scolumn < ecolumn))) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Repainting (%ld,%ld) to (%ld,%ld).\n",
				srow, scolumn, erow, ecolumn);
		}
#endif
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     srow, erow - srow + 1);
	}
}

/* Update the hilited text if the pointer has moved to a new character cell. */
static void
vte_terminal_match_hilite(VteTerminal *terminal, double x, double y)
{
	int start, end, width, height;
	long rows, rowe;
	char *match;
	struct vte_char_attributes *attr;
	VteScreen *screen;
	long delta;

	width = terminal->char_width;
	height = terminal->char_height;

	/* If the pointer hasn't moved to another character cell, then we
	 * need do nothing. */
	if ((x / width == terminal->pvt->mouse_last_x / width) &&
	    (y / height == terminal->pvt->mouse_last_y / height)) {
		return;
	}

	/* Check for matches. */
	screen = terminal->pvt->screen;
	delta = screen->scroll_delta;
	match = vte_terminal_match_check_internal(terminal,
						  floor(x) / width,
						  floor(y) / height + delta,
						  NULL,
						  &start,
						  &end);

	/* If there are no matches, repaint what we had matched before. */
	if (match == NULL) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "No matches.\n");
		}
#endif
		vte_terminal_match_hilite_clear(terminal);
	} else {
		/* Save the old hilite area. */
		rows = terminal->pvt->match_start.row;
		rowe = terminal->pvt->match_end.row;
		/* Read the new locations. */
		attr = &g_array_index(terminal->pvt->match_attributes,
				      struct vte_char_attributes,
				      start);
		terminal->pvt->match_start.row = attr->row;
		terminal->pvt->match_start.column = attr->column;
		attr = &g_array_index(terminal->pvt->match_attributes,
				      struct vte_char_attributes,
				      end);
		terminal->pvt->match_end.row = attr->row;
		terminal->pvt->match_end.column = attr->column;
		/* Repaint the newly-hilited area. */
		vte_invalidate_cells(terminal,
				     0,
				     terminal->column_count,
				     terminal->pvt->match_start.row,
				     terminal->pvt->match_end.row -
				     terminal->pvt->match_start.row + 1);
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Matched (%ld,%ld) to (%ld,%ld).\n",
				terminal->pvt->match_start.column,
				terminal->pvt->match_start.row,
				terminal->pvt->match_end.column,
				terminal->pvt->match_end.row);
		}
#endif
		/* Repaint what used to be hilited, if anything. */
		vte_invalidate_cells(terminal,
				     0, terminal->column_count,
				     rows, rowe - rows + 1);
	}
}


/* Note that the clipboard has cleared. */
static void
vte_terminal_clear_cb(GtkClipboard *clipboard, gpointer owner)
{
	VteTerminal *terminal;
	g_return_if_fail(VTE_IS_TERMINAL(owner));
	terminal = VTE_TERMINAL(owner);
	if (terminal->pvt->has_selection) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
			fprintf(stderr, "Lost selection.\n");
		}
#endif
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
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
			int i;
			fprintf(stderr, "Setting selection (%d UTF-8 bytes.)\n",
				strlen(terminal->pvt->selection));
			for (i = 0; terminal->pvt->selection[i] != '\0'; i++) {
				fprintf(stderr, "0x%04x\n",
					terminal->pvt->selection[i]);
			}
		}
#endif
		gtk_selection_data_set_text(data, terminal->pvt->selection, -1);
	}
}

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
 * Extracts a view of the visible part of the string.  If @is_selected is not
 * NULL, characters will only be read if @is_selected returns TRUE after being
 * passed the column and row, respectively.  A #vte_char_attributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics.  The
 * entire scrollback buffer is scanned, so it is possible to read the entire
 * contents of the buffer using this function.
 *
 * Returns: a text string which must be freed by the caller.
 */
char *
vte_terminal_get_text_range(VteTerminal *terminal,
			    glong start_row, glong start_col,
			    glong end_row, glong end_col,
			    gboolean(*is_selected)(VteTerminal *, glong, glong,
						   gpointer),
			    gpointer data,
			    GArray *attributes)
{
	long col, row, last_space, last_spacecol,
	     last_nonspace, last_nonspacecol;
	VteScreen *screen;
	struct vte_charcell *pcell = NULL;
	GString *string;
	struct vte_char_attributes attr;
	struct vte_palette_entry fore, back, *palette;

	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);
	g_return_val_if_fail(is_selected != NULL, NULL);
	screen = terminal->pvt->screen;

	string = g_string_new("");
	memset(&attr, 0, sizeof(attr));

	palette = terminal->pvt->palette;
	for (row = start_row; row <= end_row; row++) {
		col = (row == start_row) ? start_col : 0;
		last_space = last_nonspace = -1;
		last_spacecol = last_nonspacecol = -1;
		attr.row = row;
		pcell = NULL;
		do {
			/* If it's not part of a multi-column character,
			 * and passes the selection criterion, add it to
			 * the selection. */
			attr.column = col;
			pcell = vte_terminal_find_charcell(terminal, col, row);
			if (pcell == NULL) {
				/* No more characters on this line. */
				break;
			}
			if (!pcell->fragment &&
			    is_selected(terminal, col, row, data)) {
				/* Store the attributes of this character. */
				fore = palette[pcell->fore];
				back = palette[pcell->back];
				attr.fore.red = fore.red;
				attr.fore.green = fore.green;
				attr.fore.blue = fore.blue;
				attr.back.red = back.red;
				attr.back.green = back.green;
				attr.back.blue = back.blue;
				attr.underline = pcell->underline;
				attr.strikethrough = pcell->strikethrough;
				attr.alternate = pcell->alternate;
				/* Store the character. */
				string = g_string_append_unichar(string,
								 pcell->c);
				/* Record whether or not this was
				 * whitespace. */
				if ((pcell->c == ' ') || (pcell->c == '\0')) {
					last_space = string->len - 1;
					last_spacecol = col;
				} else {
					last_nonspace = string->len - 1;
					last_nonspacecol = col;
				}
			}
			/* If we added a character to the string, record its
			 * attributes, one per byte. */
			if (attributes) {
				vte_g_array_fill(attributes,
						 &attr, string->len);
			}
			/* If we're on the last line, and have just looked in
			 * the last column, stop. */
			if ((row == end_row) && (col == end_col)) {
				break;
			}
			col++;
		} while (pcell != NULL);
		/* If the last thing we saw was a space, and we stopped at the
		 * right edge of the selected area, trim the trailing spaces
		 * off of the line. */
		if ((last_space != -1) &&
		    (last_nonspace != -1) &&
		    (last_space > last_nonspace)) {
			/* Check for non-space after this point on the line. */
			col = MAX(0, last_spacecol);
			do {
				/* Check that we have data here. */
				pcell = vte_terminal_find_charcell(terminal,
								   col, row);
				/* Stop if we ran out of data. */
				if (pcell == NULL) {
					break;
				}
				/* Skip over fragments. */
				if (pcell->fragment) {
					continue;
				}
				/* Check whether or not it holds whitespace. */
				if ((pcell->c != ' ') && (pcell->c != '\0')) {
					/* It holds non-whitespace, stop. */
					break;
				}
				col++;
			} while (pcell != NULL);
			/* If pcell is NULL, then there was no printing
			 * character to the right of the endpoint, so truncate
			 * the string at the end of the printing chars. */
			if (pcell == NULL) {
				g_string_truncate(string, last_nonspace + 1);
			}
		}
		/* Make sure that the attributes array is as long as the
		 * string. */
		if (attributes) {
			g_array_set_size(attributes, string->len);
		}
		/* If the last visible column on this line was selected and
		 * it contained whitespace, append a newline. */
		if (is_selected(terminal, terminal->column_count - 1,
				row, data)) {
			pcell = vte_terminal_find_charcell(terminal,
							   terminal->column_count - 1,
							   row);
			/* If it's whitespace, we snipped it off, so add a
			 * newline. */
			if ((pcell == NULL) ||
			    (pcell->c == '\0') ||
			    (pcell->c == ' ')) {
				string = g_string_append_c(string, '\n');
			}
			/* Move this newline to the end of the line. */
			attr.column = MAX(terminal->column_count,
					  attr.column + 1);
			/* If we broke out of the loop, there's at least one
			 * character with missing attributes. */
			if (attributes) {
				vte_g_array_fill(attributes, &attr,
						 string->len);
			}
		}
	}
	/* Sanity check. */
	if (attributes) {
		g_assert(string->len == attributes->len);
	}
	return g_string_free(string, FALSE);
}

/**
 * vte_terminal_get_text:
 * @terminal: a #VteTerminal
 * @is_selected: a callback
 * @data: user data to be passed to the callback
 * @attributes: location for storing text attributes
 *
 * Extracts a view of the visible part of the string.  If @is_selected is not
 * NULL, characters will only be read if @is_selected returns TRUE after being
 * passed the column and row, respectively.  A #vte_char_attributes structure
 * is added to @attributes for each byte added to the returned string detailing
 * the character's position, colors, and other characteristics.
 *
 * Returns: a text string which must be freed by the caller.
 */
char *
vte_terminal_get_text(VteTerminal *terminal,
		      gboolean(*is_selected)(VteTerminal *, glong, glong,
					     gpointer),
		      gpointer data,
		      GArray *attributes)
{
	long start_row, start_col, end_row, end_col;
	start_row = terminal->pvt->screen->scroll_delta;
	start_col = 0;
	end_row = start_row + terminal->row_count - 1;
	end_col = terminal->column_count - 1;
	return vte_terminal_get_text_range(terminal,
					   start_row, start_col,
					   end_row, end_col,
					   is_selected ?
					   is_selected : always_selected,
					   data,
					   attributes);
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

/* Place the selected text onto the clipboard.  Do this asynchronously so that
 * we get notified when the selection we placed on the clipboard is replaced. */
static void
vte_terminal_copy(VteTerminal *terminal, GdkAtom board)
{
	GtkClipboard *clipboard;
	GtkTargetEntry targets[] = {
		{"UTF8_STRING", 0, 0},
		{"COMPOUND_TEXT", 0, 0},
		{"TEXT", 0, 0},
		{"STRING", 0, 0},
	};

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	clipboard = gtk_clipboard_get(board);

	/* Chuck old selected text and retrieve the newly-selected text. */
	if (terminal->pvt->selection != NULL) {
		g_free(terminal->pvt->selection);
	}
	terminal->pvt->selection =
		vte_terminal_get_text_range(terminal,
					    terminal->pvt->selection_start.y,
					    0,
					    terminal->pvt->selection_end.y,
					    terminal->column_count,
					    vte_cell_is_selected,
					    NULL,
					    NULL);

	/* Place the text on the clipboard. */
	if (terminal->pvt->selection != NULL) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
			fprintf(stderr, "Assuming ownership of selection.\n");
		}
#endif
		gtk_clipboard_set_with_owner(clipboard,
					     targets,
					     G_N_ELEMENTS(targets),
					     vte_terminal_copy_cb,
					     vte_terminal_clear_cb,
					     G_OBJECT(terminal));
	}
}

/* Paste from the given clipboard. */
static void
vte_terminal_paste(VteTerminal *terminal, GdkAtom board)
{
	GtkClipboard *clipboard;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	clipboard = gtk_clipboard_get(board);
	if (clipboard != NULL) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
			fprintf(stderr, "Requesting clipboard contents.\n");
		}
#endif
		gtk_clipboard_request_text(clipboard,
					   vte_terminal_paste_cb,
					   terminal);
	}
}

/* Start selection at the location of the event. */
static void
vte_terminal_start_selection(GtkWidget *widget, GdkEventButton *event,
			     enum vte_selection_type selection_type)
{
	VteTerminal *terminal;
	long cellx, celly, delta;

	terminal = VTE_TERMINAL(widget);

	/* Convert the event coordinates to cell coordinates. */
	delta = terminal->pvt->screen->scroll_delta;
	cellx = (event->x - VTE_PAD_WIDTH) / terminal->char_width;
	celly = (event->y - VTE_PAD_WIDTH) / terminal->char_height + delta;

	/* Record that we have the selection, and where it started. */
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

		terminal->pvt->selection_restart_origin =
			terminal->pvt->selection_last;
		break;
	case selection_type_word:
	case selection_type_line:
		/* Mark the newly-selected areas now. */
		terminal->pvt->selecting_restart = FALSE;
		terminal->pvt->has_selection = TRUE;
		terminal->pvt->selecting_had_delta = FALSE;

		terminal->pvt->selection_start.x = cellx;
		terminal->pvt->selection_start.y = celly;
		terminal->pvt->selection_end = terminal->pvt->selection_start;
		terminal->pvt->selection_origin =
			terminal->pvt->selection_last;
		break;
	}

	/* Record the selection type. */
	terminal->pvt->selection_type = selection_type;
	terminal->pvt->selecting = TRUE;

	/* Draw the row the selection started on. */
	vte_invalidate_cells(terminal,
			     0, terminal->column_count,
			     terminal->pvt->selection_start.y, 1);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
		fprintf(stderr, "Selection started at (%ld,%ld).\n",
			terminal->pvt->selection_start.x,
			terminal->pvt->selection_start.y);
	}
#endif
	vte_terminal_emit_selection_changed(terminal);
}

/* Extend selection to include the given event coordinates. */
static void
vte_terminal_extend_selection(GtkWidget *widget, double x, double y,
			      gboolean always_grow)
{
	VteTerminal *terminal;
	VteScreen *screen;
	GArray *rowdata;
	long delta, height, width, last_nonspace, i, j;
	struct vte_charcell *cell;
	struct selection_event_coords *origin, *last, *start, *end;
	struct selection_cell_coords old_start, old_end, *sc, *ec, tc;

	terminal = VTE_TERMINAL(widget);
	screen = terminal->pvt->screen;
	old_start = terminal->pvt->selection_start;
	old_end = terminal->pvt->selection_end;
	height = terminal->char_height;
	width = terminal->char_width;

	/* Convert the event coordinates to cell coordinates. */
	delta = screen->scroll_delta;

	/* If we're restarting on a drag, then mark this as the start of
	 * the selected block. */
	if (terminal->pvt->selecting_restart) {
		vte_terminal_deselect_all(terminal);
		/* Record the origin of the selection. */
		terminal->pvt->selection_origin =
			terminal->pvt->selection_restart_origin;
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
			fprintf(stderr, "Selection delayed start at (%lf,%lf).\n",
				terminal->pvt->selection_origin.x / width,
				terminal->pvt->selection_origin.y / height);
		}
#endif
	}

	/* Recognize that we've got a selected block. */
	terminal->pvt->has_selection = TRUE;
	terminal->pvt->selecting_had_delta = TRUE;
	terminal->pvt->selecting_restart = FALSE;

	/* If we're not in always-grow mode, update the last location of
	 * the selection. */
	last = &terminal->pvt->selection_last;
	if (!always_grow) {
		last->x = x;
		last->y = y + height * delta;
	}

	/* Map the origin and last selected points to a start and end. */
	origin = &terminal->pvt->selection_origin;
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

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
		fprintf(stderr, "Selection is (%lf,%lf) to (%lf,%lf).\n",
			start->x, start->y, end->x, end->y);
	}
#endif

	/* Recalculate the selection area in terms of cell positions. */
	terminal->pvt->selection_start.x = MAX(0, start->x / width);
	terminal->pvt->selection_start.y = MAX(0, start->y / height);
	terminal->pvt->selection_end.x = MAX(0, end->x / width);
	terminal->pvt->selection_end.y = MAX(0, end->y / height);

	/* Re-sort using cell coordinates to catch round-offs that make two
	 * coordinates "the same". */
	sc = &terminal->pvt->selection_start;
	ec = &terminal->pvt->selection_end;
	if ((sc->y > ec->y) || ((sc->y == ec->y) && (sc->x > ec->x))) {
		tc = *sc;
		*sc = *ec;
		*ec = tc;
	}

	/* Extend the selection to handle end-of-line cases, word, and line
	 * selection.  We do this here because calculating it once is cheaper
	 * than recalculating for each cell as we render it. */

	/* Handle end-of-line at the start-cell. */
	if (_vte_ring_contains(screen->row_data, sc->y)) {
		rowdata = _vte_ring_index(screen->row_data, GArray*, sc->y);
	} else {
		rowdata = NULL;
	}
	if (rowdata != NULL) {
		/* Find the last non-space character on the first line. */
		last_nonspace = -1;
		for (i = 0; i < rowdata->len; i++) {
			cell = &g_array_index(rowdata,
					      struct vte_charcell, i);
			if (!g_unichar_isspace(cell->c)) {
				last_nonspace = i;
			}
		}
		/* Now find the first space after it. */
		i = last_nonspace + 1;
		/* If the start point is to its right, then move the
		 * startpoint up to the beginning of the next line
		 * unless that would move the startpoint after the end
		 * point. */
		if (sc->x > i) {
			if (sc->y < ec->y) {
				sc->x = 0;
				sc->y++;
			} else {
				sc->x = i;
			}
		}
	} else {
		/* Snap to the leftmost column. */
		sc->x = 0;
	}

	/* Handle end-of-line at the end-cell. */
	if (_vte_ring_contains(screen->row_data, ec->y)) {
		rowdata = _vte_ring_index(screen->row_data, GArray*, ec->y);
	} else {
		rowdata = NULL;
	}
	if (rowdata != NULL) {
		/* Find the last non-space character on the last line. */
		last_nonspace = -1;
		for (i = 0; i < rowdata->len; i++) {
			cell = &g_array_index(rowdata, struct vte_charcell, i);
			if (!g_unichar_isspace(cell->c)) {
				last_nonspace = i;
			}
		}
		/* Now find the first space after it. */
		i = last_nonspace + 1;
		/* If the end point is to its right, then extend the
		 * endpoint as far right as we can expect. */
		if (ec->x >= i) {
			ec->x = MAX(ec->x,
				    MAX(terminal->column_count - 1,
					rowdata->len));
		}
	} else {
		/* Snap to the rightmost column. */
		ec->x = MAX(ec->x, terminal->column_count - 1);
	}

	/* Now extend again based on selection type. */
	switch (terminal->pvt->selection_type) {
	case selection_type_char:
		/* Nothing more to do. */
		break;
	case selection_type_word:
		/* Keep selecting to the left as long as the next character we
		 * look at is of the same class as the current start point. */
		i = sc->x;
		j = sc->y;
		while (_vte_ring_contains(screen->row_data, j)) {
			/* Get the data for the row we're looking at. */
			rowdata = _vte_ring_index(screen->row_data, GArray*, j);
			if (rowdata == NULL) {
				break;
			}
			/* Back up. */
			for (i = (j == sc->y) ?
				 sc->x :
				 terminal->column_count - 1;
			     i > 0;
			     i--) {
				if (vte_uniform_class(terminal,
						      j,
						      i - 1,
						      i)) {
					sc->x = i - 1;
					sc->y = j;
				} else {
					break;
				}
			}
			if (i > 0) {
				/* We hit a stopping point, so stop. */
				break;
			} else {
				if (vte_line_is_wrappable(terminal, j - 1)) {
					/* Move on to the previous line. */
					j--;
					sc->x = terminal->column_count - 1;
					sc->y = j;
				} else {
					break;
				}
			}
		}
		/* Keep selecting to the right as long as the next character we
		 * look at is of the same class as the current end point. */
		i = ec->x;
		j = ec->y;
		while (_vte_ring_contains(screen->row_data, j)) {
			/* Get the data for the row we're looking at. */
			rowdata = _vte_ring_index(screen->row_data, GArray*, j);
			if (rowdata == NULL) {
				break;
			}
			/* Move forward. */
			for (i = (j == ec->y) ?
				 ec->x :
				 0;
			     i < terminal->column_count - 1;
			     i++) {
				if (vte_uniform_class(terminal,
						      j,
						      i,
						      i + 1)) {
					ec->x = i + 1;
					ec->y = j;
				} else {
					break;
				}
			}
			if (i < terminal->column_count - 1) {
				/* We hit a stopping point, so stop. */
				break;
			} else {
				if (vte_line_is_wrappable(terminal, j)) {
					/* Move on to the next line. */
					j++;
					ec->x = 0;
					ec->y = j;
				} else {
					break;
				}
			}
		}
		break;
	case selection_type_line:
		/* Extend the selection to the beginning of the start line. */
		sc->x = 0;
		/* Now back up as far as we can go. */
		j = sc->y;
		while (_vte_ring_contains(screen->row_data, j - 1) && 
		       vte_line_is_wrappable(terminal, j - 1)) {
			j--;
			sc->y = j;
		}
		/* And move forward as far as we can go. */
		j = ec->y;
		while (_vte_ring_contains(screen->row_data, j) && 
		       vte_line_is_wrappable(terminal, j)) {
			j++;
			ec->y = j;
		}
		/* Make sure we include all of the last line. */
		ec->x = terminal->column_count - 1;
		if (_vte_ring_contains(screen->row_data, ec->y)) {
			rowdata = _vte_ring_index(screen->row_data,
						  GArray*,
						  ec->y);
			if (rowdata != NULL) {
				ec->x = MAX(ec->x, rowdata->len);
			}
		}
		break;
	}

	/* Redraw the rows which contain cells which have changed their
	 * is-selected status. */
	if ((old_start.x != terminal->pvt->selection_start.x) ||
	    (old_start.y != terminal->pvt->selection_start.y)) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
			fprintf(stderr, "Refreshing lines %ld to %ld.\n",
				MIN(old_start.y,
				    terminal->pvt->selection_start.y),
				MAX(old_start.y,
				    terminal->pvt->selection_start.y));
		}
#endif
		vte_invalidate_cells(terminal,
				     0,
				     terminal->column_count,
				     MIN(old_start.y,
					 terminal->pvt->selection_start.y),
				     ABS(old_start.y -
					 terminal->pvt->selection_start.y) + 1);
	}
	if ((old_end.x != terminal->pvt->selection_end.x) ||
	    (old_end.y != terminal->pvt->selection_end.y)) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
			fprintf(stderr, "Refreshing lines %ld to %ld.\n",
				MIN(old_end.y, terminal->pvt->selection_end.y),
				MAX(old_end.y, terminal->pvt->selection_end.y));
		}
#endif
		vte_invalidate_cells(terminal,
				     0,
				     terminal->column_count,
				     MIN(old_end.y,
					 terminal->pvt->selection_end.y),
				     ABS(old_end.y -
					 terminal->pvt->selection_end.y) + 1);
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SELECTION)) {
		fprintf(stderr, "Selection changed to "
			"(%ld,%ld) to (%ld,%ld).\n",
			terminal->pvt->selection_start.x,
			terminal->pvt->selection_start.y,
			terminal->pvt->selection_end.x,
			terminal->pvt->selection_end.y);
	}
#endif
	vte_terminal_emit_selection_changed(terminal);
}

/* Autoscroll a bit. */
static int
vte_terminal_autoscroll(gpointer data)
{
	VteTerminal *terminal;
	GtkWidget *widget;
	gboolean extend = FALSE;
	gdouble x, y, xmax, ymax;
	double adj;

	terminal = VTE_TERMINAL(data);
	widget = GTK_WIDGET(terminal);

	/* Provide an immediate effect for mouse wigglers. */
	if (terminal->pvt->mouse_last_y < 0) {
		if (terminal->adjustment) {
			/* Try to scroll up by one line. */
			adj = CLAMP(terminal->adjustment->value - 1,
				    terminal->adjustment->lower,
				    terminal->adjustment->upper -
				    terminal->row_count);
			gtk_adjustment_set_value(terminal->adjustment, adj);
			extend = TRUE;
		}
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Autoscrolling down.\n");
		}
#endif
	}
	if (terminal->pvt->mouse_last_y >
	    terminal->row_count * terminal->char_height) {
		if (terminal->adjustment) {
			/* Try to scroll up by one line. */
			adj = CLAMP(terminal->adjustment->value + 1,
				    terminal->adjustment->lower,
				    terminal->adjustment->upper -
				    terminal->row_count);
			gtk_adjustment_set_value(terminal->adjustment, adj);
			extend = TRUE;
		}
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Autoscrolling up.\n");
		}
#endif
	}
	if (extend) {
		/* Don't select off-screen areas.  That just confuses people. */
		xmax = terminal->column_count * terminal->char_width;
		ymax = terminal->row_count * terminal->char_height;

		x = CLAMP(terminal->pvt->mouse_last_x, 0, xmax);
		y = CLAMP(terminal->pvt->mouse_last_y, 0, ymax);
		/* If we clamped the Y, mess with the X to get the entire
		 * lines. */
		if (terminal->pvt->mouse_last_y < 0) {
			x = 0;
		}
		if (terminal->pvt->mouse_last_y > ymax) {
			x = terminal->column_count * terminal->char_width;
		}
		/* Extend selection to cover the newly-scrolled area. */
		vte_terminal_extend_selection(widget, x, y, FALSE);
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
					   1000 / terminal->row_count,
					   vte_terminal_autoscroll,
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
static gint
vte_terminal_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
	VteTerminal *terminal;
	GdkModifierType modifiers;

	g_return_val_if_fail(VTE_IS_TERMINAL(widget), FALSE);
	terminal = VTE_TERMINAL(widget);

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		fprintf(stderr, "Motion notify (%lf,%lf).\n",
			event->x, event->y);
	}
#endif

	/* Show the cursor. */
	vte_terminal_set_pointer_visible(terminal, TRUE);

	/* Grab input focus. */
	if (!GTK_WIDGET_HAS_FOCUS(widget)) {
		gtk_widget_grab_focus(widget);
	}

	/* Read the modifiers. */
	if (gdk_event_get_state((GdkEvent*)event, &modifiers) == FALSE) {
		modifiers = 0;
	}

	switch (event->type) {
	case GDK_MOTION_NOTIFY:
		switch (terminal->pvt->mouse_last_button) {
		case 1:
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
				fprintf(stderr, "Mousing drag 1.\n");
			}
#endif
			if (((modifiers & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) ||
			    (!terminal->pvt->mouse_send_xy_on_click &&
			     !terminal->pvt->mouse_send_xy_on_button &&
			     !terminal->pvt->mouse_hilite_tracking &&
			     !terminal->pvt->mouse_cell_motion_tracking &&
			     !terminal->pvt->mouse_all_motion_tracking)) {
				vte_terminal_extend_selection(widget,
							      event->x - VTE_PAD_WIDTH,
							      event->y - VTE_PAD_WIDTH,
							      FALSE);
			} else {
				vte_terminal_maybe_send_mouse_drag(terminal,
								   event);
			}
			break;
		case 2:
		case 3:
		default:
			break;
		}
		break;
	default:
		break;
	}

	/* Start scrolling if we need to. */
	if ((event->y < VTE_PAD_WIDTH) ||
	    (event->y > (widget->allocation.height - VTE_PAD_WIDTH))) {
		switch (terminal->pvt->mouse_last_button) {
		case 1:
			/* Give mouse wigglers something. */
			vte_terminal_autoscroll(terminal);
			/* Start a timed autoscroll if we're not doing it
			 * already. */
			vte_terminal_start_autoscroll(terminal);
			break;
		case 2:
		case 3:
		default:
			break;
		}
	}

	/* Hilite any matches. */
	vte_terminal_match_hilite(terminal,
				  event->x - VTE_PAD_WIDTH,
				  event->y - VTE_PAD_WIDTH);

	/* Save the pointer coordinates for later use. */
	terminal->pvt->mouse_last_x = event->x - VTE_PAD_WIDTH;
	terminal->pvt->mouse_last_y = event->y - VTE_PAD_WIDTH;

	return FALSE;
}

/* Read and handle a pointing device buttonpress event. */
static gint
vte_terminal_button_press(GtkWidget *widget, GdkEventButton *event)
{
	VteTerminal *terminal;
	long height, width, delta;
	GdkModifierType modifiers;
	gboolean handled = FALSE;
	gboolean start_selecting = FALSE, extend_selecting = FALSE;
	long cellx, celly;

	g_return_val_if_fail(VTE_IS_TERMINAL(widget), FALSE);
	terminal = VTE_TERMINAL(widget);
	height = terminal->char_height;
	width = terminal->char_width;
	delta = terminal->pvt->screen->scroll_delta;
	vte_terminal_set_pointer_visible(terminal, TRUE);

	/* Grab input focus. */
	if (!GTK_WIDGET_HAS_FOCUS(widget)) {
		gtk_widget_grab_focus(widget);
	}

	/* Read the modifiers. */
	if (gdk_event_get_state((GdkEvent*)event, &modifiers) == FALSE) {
		modifiers = 0;
	}

	/* Convert the event coordinates to cell coordinates. */
	cellx = (event->x - VTE_PAD_WIDTH) / width;
	celly = (event->y - VTE_PAD_WIDTH) / height + delta;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Button %d single-click at (%lf,%lf)\n",
				event->button,
				event->x - VTE_PAD_WIDTH,
				event->y - VTE_PAD_WIDTH +
				(terminal->char_height * delta));
		}
#endif
		/* Handle this event ourselves. */
		switch (event->button) {
		case 1:
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
				fprintf(stderr, "Handling click ourselves.\n");
			}
#endif

			/* If the user hit shift, override event mode. */
			if ((modifiers & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) {
				if (terminal->pvt->mouse_send_xy_on_button ||
				    terminal->pvt->mouse_send_xy_on_click) {
					/* If shift is pressed in event mode,
					 * start selecting. */
					start_selecting = TRUE;
				} else {
					/* If shift is pressed in non-event
					 * mode, extend selection if the cell
					 * isn't already selected, otherwise
					 * start selection. */
					if (terminal->pvt->has_selection &&
					    !vte_cell_is_selected(terminal,
								  cellx,
								  celly,
								  NULL)) {
						extend_selecting = TRUE;
					} else {
						start_selecting = TRUE;
					}
				}
			} else {
				/* If shift isn't set, start selecting. */
				start_selecting = TRUE;
			}
			if (start_selecting) {
				vte_terminal_deselect_all(terminal);
				vte_terminal_start_selection(widget,
							     event,
							     selection_type_char);
				handled = TRUE;
			}
			if (extend_selecting) {
				vte_terminal_extend_selection(widget,
							      event->x - VTE_PAD_WIDTH,
							      event->y - VTE_PAD_WIDTH,
							      TRUE);
				handled = TRUE;
			}
			break;
		/* Paste if the user pressed shift or we're not sending events
		 * to the app. */
		case 2:
			if (((modifiers & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) ||
			    (!terminal->pvt->mouse_send_xy_on_button &&
			     !terminal->pvt->mouse_send_xy_on_click)) {
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
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Button %d double-click at (%lf,%lf)\n",
				event->button,
				event->x - VTE_PAD_WIDTH,
				event->y - VTE_PAD_WIDTH +
				(terminal->char_height * delta));
		}
#endif
		switch (event->button) {
		case 1:
			vte_terminal_start_selection(widget,
						     event,
						     selection_type_word);
			vte_terminal_extend_selection(widget,
						      event->x - VTE_PAD_WIDTH,
						      event->y - VTE_PAD_WIDTH,
						      FALSE);
			handled = TRUE;
			break;
		case 2:
		case 3:
		default:
			break;
		}
		break;
	case GDK_3BUTTON_PRESS:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Button %d triple-click at (%lf,%lf).\n",
				event->button,
				event->x - VTE_PAD_WIDTH,
				event->y - VTE_PAD_WIDTH +
				(terminal->char_height * delta));
		}
#endif
		switch (event->button) {
		case 1:
			vte_terminal_start_selection(widget,
						     event,
						     selection_type_line);
			vte_terminal_extend_selection(widget,
						      event->x - VTE_PAD_WIDTH,
						      event->y - VTE_PAD_WIDTH,
						      FALSE);
			handled = TRUE;
			break;
		case 2:
		case 3:
		default:
			break;
		}
	default:
		break;
	}

	/* Hilite any matches. */
	vte_terminal_match_hilite(terminal,
				  event->x - VTE_PAD_WIDTH,
				  event->y - VTE_PAD_WIDTH);

	/* Save the pointer state for later use. */
	terminal->pvt->mouse_last_button = event->button;
	terminal->pvt->mouse_last_x = event->x - VTE_PAD_WIDTH;
	terminal->pvt->mouse_last_y = event->y - VTE_PAD_WIDTH;

	return FALSE;
}

/* Read and handle a pointing device buttonrelease event. */
static gint
vte_terminal_button_release(GtkWidget *widget, GdkEventButton *event)
{
	VteTerminal *terminal;
	GdkModifierType modifiers;
	gboolean handled = FALSE;

	g_return_val_if_fail(VTE_IS_TERMINAL(widget), FALSE);
	terminal = VTE_TERMINAL(widget);
	vte_terminal_set_pointer_visible(terminal, TRUE);

	/* Grab input focus. */
	if (!GTK_WIDGET_HAS_FOCUS(widget)) {
		gtk_widget_grab_focus(widget);
	}

	/* Disconnect from autoscroll requests. */
	vte_terminal_stop_autoscroll(terminal);

	/* Read the modifiers. */
	if (gdk_event_get_state((GdkEvent*)event, &modifiers) == FALSE) {
		modifiers = 0;
	}

	switch (event->type) {
	case GDK_BUTTON_RELEASE:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Button %d released at (%lf,%lf).\n",
				event->button,
				event->x - VTE_PAD_WIDTH,
				event->y - VTE_PAD_WIDTH);
		}
#endif
		switch (event->button) {
		case 1:
			/* If Shift is held down, or we're not in events mode,
			 * copy the selected text. */
			if (((modifiers & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) ||
			    (!terminal->pvt->mouse_send_xy_on_button)) {
				/* Copy only if something was selected. */
				if (terminal->pvt->has_selection &&
				    !terminal->pvt->selecting_restart &&
				    terminal->pvt->selecting_had_delta) {
					vte_terminal_copy(terminal,
							  GDK_SELECTION_PRIMARY);
				}
				terminal->pvt->selecting = FALSE;
				handled = TRUE;
			}
			break;
		case 2:
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

	/* Hilite any matches. */
	vte_terminal_match_hilite(terminal,
				  event->x - VTE_PAD_WIDTH,
				  event->y - VTE_PAD_WIDTH);

	/* Save the pointer state for later use. */
	terminal->pvt->mouse_last_button = 0;
	terminal->pvt->mouse_last_x = event->x - VTE_PAD_WIDTH;
	terminal->pvt->mouse_last_y = event->y - VTE_PAD_WIDTH;

	return FALSE;
}

/* Handle receiving or losing focus. */
static gint
vte_terminal_focus_in(GtkWidget *widget, GdkEventFocus *event)
{
	g_return_val_if_fail(GTK_IS_WIDGET(widget), 0);
	GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);
	gtk_im_context_focus_in((VTE_TERMINAL(widget))->pvt->im_context);
	/* Force the cursor to be the foreground color twice, in case we're
	 * in blinking mode and the next scheduled redraw occurs just after
	 * the one we're about to perform. */
	(VTE_TERMINAL(widget))->pvt->cursor_force_fg = 2;
	vte_invalidate_cursor_once(VTE_TERMINAL(widget));
	return FALSE;
}

static gint
vte_terminal_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
	g_return_val_if_fail(GTK_WIDGET(widget), 0);
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
	gtk_im_context_focus_out((VTE_TERMINAL(widget))->pvt->im_context);
	vte_invalidate_cursor_once(VTE_TERMINAL(widget));
	return FALSE;
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
		g_warning(_("Using fontset \"%s\", which is missing "
			    "these character sets: %s."), font, charsets);
		g_free(charsets);
	}
}

#ifdef HAVE_XFT
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

/* Create an Xft pattern from a Pango font description. */
static XftPattern *
xft_pattern_from_pango_font_desc(const PangoFontDescription *font_desc)
{
	XftPattern *pattern;
	const char *family = "mono";
	int pango_mask = 0;
	int weight, style;
	double size = 14.0;

	if (font_desc != NULL) {
		pango_mask = pango_font_description_get_set_fields(font_desc);
	}

	pattern = XftPatternCreate ();

	/* Set the family for the pattern, or use a sensible default. */
	if (pango_mask & PANGO_FONT_MASK_FAMILY) {
		family = pango_font_description_get_family(font_desc);
	}
	XftPatternAddString(pattern, XFT_FAMILY, family);

	/* Set the font size for the pattern, or use a sensible default. */
	if (pango_mask & PANGO_FONT_MASK_SIZE) {
		size = (double) pango_font_description_get_size(font_desc);
		size /= (double) PANGO_SCALE;
	}
	XftPatternAddDouble(pattern, XFT_SIZE, size);

	/* There aren'ty any fallbacks for these, so just omit them from the
	 * pattern if they're not set in the pango font. */
	if (pango_mask & PANGO_FONT_MASK_WEIGHT) {
		weight = pango_font_description_get_weight(font_desc);
		XftPatternAddInteger(pattern, XFT_WEIGHT,
				     xft_weight_from_pango_weight (weight));
	}
	if (pango_mask & PANGO_FONT_MASK_STYLE) {
		style = pango_font_description_get_style(font_desc);
		XftPatternAddInteger(pattern, XFT_SLANT,
				     xft_slant_from_pango_style (style));
	}

	return pattern;
}
#endif

static char *
xlfd_from_pango_font_description(GtkWidget *widget,
				 const PangoFontDescription *fontdesc)
{
	char *spec;
	PangoContext *context;
	PangoFont *font;
	PangoXSubfont *subfont_ids;
	PangoFontMap *fontmap;
	int *subfont_charsets, i, count;
	char *xlfd = NULL, *tmp, *subfont, *ret;
	char **exploded;
	char *encodings[] = {
		"iso10646-0",
		"iso10646-1",
		"ascii-0",
		"big5-0",
		"dos-437",
		"dos-737",
		"gb18030.2000-0",
		"gb18030.2000-1",
		"gb2312.1980-0",
		"iso8859-1",
		"iso8859-10",
		"iso8859-15",
		"iso8859-2",
		"iso8859-3",
		"iso8859-4",
		"iso8859-5",
		"iso8859-7",
		"iso8859-8",
		"iso8859-9",
		"jisx0201.1976-0",
		"jisx0208.1983-0",
		"jisx0208.1990-0",
		"jisx0208.1997-0",
		"jisx0212.1990-0",
		"jisx0213.2000-1",
		"jisx0213.2000-2",
		"koi8-r",
		"koi8-u",
		"koi8-ub",
		"misc-fontspecific",
	};

	g_return_val_if_fail(fontdesc != NULL, NULL);
	g_return_val_if_fail(GTK_IS_WIDGET(widget), NULL);

	context = pango_x_get_context(GDK_DISPLAY());
	if (context == NULL) {
		return g_strdup(VTE_X_FIXED);
	}

	fontmap = pango_x_font_map_for_display(GDK_DISPLAY());
	if (fontmap == NULL) {
		return g_strdup(VTE_X_FIXED);
	}

	font = pango_font_map_load_font(fontmap, context, fontdesc);
	if (font == NULL) {
		return g_strdup(VTE_X_FIXED);
	}

	count = pango_x_list_subfonts(font,
				      encodings, G_N_ELEMENTS(encodings),
				      &subfont_ids, &subfont_charsets);
	for (i = 0; i < count; i++) {
		subfont = pango_x_font_subfont_xlfd(font,
						    subfont_ids[i]);
		exploded = g_strsplit(subfont, "-", 0);
		if (exploded != NULL) {
			g_free(exploded[6]);
			exploded[6] = g_strdup("*");
			g_free(exploded[8]);
			exploded[8] = g_strdup("*");
			g_free(exploded[9]);
			exploded[9] = g_strdup("*");
			g_free(subfont);
			subfont = g_strjoinv("-", exploded);
			g_strfreev(exploded);
		}
		if (xlfd) {
			tmp = g_strconcat(xlfd, ",", subfont, NULL);
			g_free(xlfd);
			g_free(subfont);
			xlfd = tmp;
		} else {
			xlfd = subfont;
		}
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		spec = pango_font_description_to_string(fontdesc);
		fprintf(stderr, "Converted PangoFontSpecification `%s' to "
			"xlfd `%s'.\n", spec, xlfd ? xlfd : "(null)");
		g_free(spec);
	}
#endif

	if (subfont_ids != NULL) {
		g_free(subfont_ids);
	}
	if (subfont_charsets != NULL) {
		g_free(subfont_charsets);
	}

	ret = strdup(xlfd);
	g_free(xlfd);
	return ret;
}

#ifdef HAVE_XFT
/* Convert an Xft pattern to a font name. */
static char *
vte_unparse_xft_pattern(XftPattern *pattern)
{
#ifdef HAVE_XFT2
	return FcNameUnparse(pattern);
#else /* !HAVE_XFT2 */
	char buf[256];
	if (!XftNameUnparse(pattern, buf, sizeof(buf)-1)) {
		buf[0] = '\0';
	}
	buf[sizeof(buf) - 1] = '\0';
	return strdup(buf);
#endif /* HAVE_XFT2 */
}
#endif /* HAVE_XFT */

/* A comparison function which helps sort quarks. */
static gint
vte_compare_direct(gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

/* Apply the changed metrics, and queue a resize if need be. */
static void
vte_terminal_apply_metrics(VteTerminal *terminal,
			   gint width, gint height, gint ascent, gint descent)
{
	gboolean resize = FALSE, cresize = FALSE;

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
	/* Queue a resize if anything's changed. */
	if (resize) {
		if (GTK_WIDGET_REALIZED(GTK_WIDGET(terminal))) {
			gtk_widget_queue_resize(GTK_WIDGET(terminal));
		}
	}
	/* Emit a signal that the font changed. */
	if (cresize) {
		vte_terminal_emit_char_size_changed(terminal,
						    terminal->char_width,
						    terminal->char_height);
	}
	/* Repaint. */
	vte_invalidate_all(terminal);
}

#ifdef HAVE_XFT2
/* Handle notification that Xft-related GTK settings have changed by resetting
 * the font using the new settings. */
static void
vte_xft_changed_cb(GtkSettings *settings, GParamSpec *spec,
		   VteTerminal *terminal)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Xft settings changed.\n");
	}
#endif
	vte_terminal_set_font(terminal, terminal->pvt->fontdesc);
}

/* Add default specifiers to the pattern which incorporate the current Xft
 * settings. */
static void
vte_default_substitute(VteTerminal *terminal, FcPattern *pattern)
{
	GtkSettings *settings;
	GObjectClass *klass;
	XftResult result;
	gboolean found;
	int i = -1;
	double d = -1;
	int antialias = -1, hinting = -1, dpi = -1;
	char *rgba = NULL, *hintstyle = NULL;

	settings = gtk_widget_get_settings(GTK_WIDGET(terminal));
	if (settings == NULL) {
		return;
	}

	/* Check that the properties we're looking at are defined. */
	klass = G_OBJECT_CLASS(GTK_SETTINGS_GET_CLASS(settings));
	if (g_object_class_find_property(klass, "gtk-xft-antialias") == NULL) {
		return;
	}

	/* If this is our first time in here, start listening for changes
	 * to the Xft settings. */
	if (terminal->pvt->connected_settings == NULL) {
		terminal->pvt->connected_settings = settings;
		g_signal_connect(G_OBJECT(settings),
				 "notify::gtk-xft-antialias",
				 G_CALLBACK(vte_xft_changed_cb), terminal);
		g_signal_connect(G_OBJECT(settings),
				 "notify::gtk-xft-hinting",
				 G_CALLBACK(vte_xft_changed_cb), terminal);
		g_signal_connect(G_OBJECT(settings),
				 "notify::gtk-xft-hintstyle",
				 G_CALLBACK(vte_xft_changed_cb), terminal);
		g_signal_connect(G_OBJECT(settings),
				 "notify::gtk-xft-rgba",
				 G_CALLBACK(vte_xft_changed_cb), terminal);
		g_signal_connect(G_OBJECT(settings),
				 "notify::gtk-xft-dpi",
				 G_CALLBACK(vte_xft_changed_cb), terminal);
	}

	/* Read the settings. */
	g_object_get(G_OBJECT(settings),
		     "gtk-xft-antialias", &antialias,
		     "gtk-xft-hinting", &hinting,
		     "gtk-xft-hintstyle", &hintstyle,
		     "gtk-xft-rgba", &rgba,
		     "gtk-xft-dpi", &dpi,
		     NULL);

	/* First get and set the settings Xft1 "knows" about. */
	if (antialias >= 0) {
		result = FcPatternGetBool(pattern, FC_ANTIALIAS, 0, &i);
		if (result == FcResultNoMatch) {
			FcPatternAddBool(pattern, FC_ANTIALIAS, antialias > 0);
		}
	}

	if (rgba != NULL) {
		result = FcPatternGetInteger(pattern, FC_RGBA, 0, &i);
		if (result == FcResultNoMatch) {
			i = FC_RGBA_NONE;
			found = TRUE;
			if (strcmp(rgba, "none") == 0) {
				i = FC_RGBA_NONE;
			} else
			if (strcmp(rgba, "rgb") == 0) {
				i = FC_RGBA_RGB;
			} else
			if (strcmp(rgba, "bgr") == 0) {
				i = FC_RGBA_BGR;
			} else
			if (strcmp(rgba, "vrgb") == 0) {
				i = FC_RGBA_VRGB;
			} else
			if (strcmp(rgba, "vbgr") == 0) {
				i = FC_RGBA_VBGR;
			} else {
				found = FALSE;
			}
			if (found) {
				FcPatternAddInteger(pattern, FC_RGBA, i);
			}
		}
	}

	if (dpi >= 0) {
		result = FcPatternGetDouble(pattern, FC_DPI, 0, &d);
		if (result == FcResultNoMatch) {
			FcPatternAddDouble(pattern, FC_DPI, dpi / 1024.0);
		}
	}

	if (hinting >= 0) {
		result = FcPatternGetBool(pattern, FC_HINTING, 0, &i);
		if (result == FcResultNoMatch) {
			FcPatternAddBool(pattern, FC_HINTING, hinting > 0);
		}
	}

#ifdef FC_HINT_STYLE
	if (hintstyle != NULL) {
		result = FcPatternGetInteger(pattern, FC_HINT_STYLE, 0, &i);
		if (result == FcResultNoMatch) {
			i = FC_HINT_NONE;
			found = TRUE;
			if (strcmp(hintstyle, "hintnone") == 0) {
				i = FC_HINT_NONE;
			} else
			if (strcmp(hintstyle, "hintslight") == 0) {
				i = FC_HINT_SLIGHT;
			} else
			if (strcmp(hintstyle, "hintmedium") == 0) {
				i = FC_HINT_MEDIUM;
			} else
			if (strcmp(hintstyle, "hintfull") == 0) {
				i = FC_HINT_FULL;
			} else {
				found = FALSE;
			}
			if (found) {
				FcPatternAddInteger(pattern, FC_HINT_STYLE, i);
			}
		}
	}
#endif
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		FcPatternPrint(pattern);
	}
#endif
	if (rgba != NULL) {
		g_free(rgba);
	}
	if (hintstyle != NULL) {
		g_free(hintstyle);
	}
}

/* Find a font which matches the request, figuring in FontConfig settings. */
static FcPattern *
vte_font_match(VteTerminal *terminal, FcPattern *pattern, FcResult *result)
{
	FcPattern *spec, *match;
	Display *display;
	int screen;

	spec = FcPatternDuplicate(pattern);
	if (spec == NULL) {
		return NULL;
	}

	display = GDK_DISPLAY();
	screen = gdk_x11_get_default_screen();

	FcConfigSubstitute(NULL, spec, FcMatchPattern);
	vte_default_substitute(terminal, spec);
	XftDefaultSubstitute(display, screen, spec);

	match = FcFontMatch(NULL, spec, result);

	FcPatternDestroy(spec);
	return match;
}
#endif

#ifdef HAVE_XFT
/* Ensure that an Xft font is loaded and metrics are known. */
static void
vte_terminal_open_font_xft(VteTerminal *terminal)
{
	XftFont *new_font;
	XftPattern *pattern;
	XftPattern *matched_pattern;
	XftResult result;
	XGlyphInfo glyph_info;
	gint width, height, ascent, descent;
	gboolean need_destroy = FALSE;
	char *name;

	/* Simple case -- we already loaded the font. */
	if (terminal->pvt->ftfont != NULL) {
		return;
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Opening Xft font.\n");
	}
#endif
	/* Map the font description to an Xft pattern. */
	pattern = xft_pattern_from_pango_font_desc(terminal->pvt->fontdesc);
	g_assert(pattern != NULL);

	/* Xft is on a lot of crack here - it fills in "result" when it
	 * feels like it, and leaves it uninitialized the rest of the
	 * time.  Whether it's filled in is impossible to determine
	 * afaict.  We don't care about its value anyhow.  */
	result = 0xffff; /* some bogus value to help in debugging */
#ifdef HAVE_XFT2
	matched_pattern = vte_font_match(terminal, pattern, &result);
#else
	matched_pattern = XftFontMatch(GDK_DISPLAY(),
				       gdk_x11_get_default_screen(),
				       pattern, &result);
#endif
	/* Keep track of whether or not we need to destroy this pattern. */
	if (matched_pattern != NULL) {
		need_destroy = TRUE;
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		if (matched_pattern != NULL) {
			name = vte_unparse_xft_pattern(matched_pattern);
			fprintf(stderr, "Matched pattern \"%s\".\n", name);
			free(name);
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
	}
#endif

	if (matched_pattern != NULL) {
		/* More Xft crackrock - it appears to "adopt" matched_pattern
		 * as new_font->pattern; whether it does this reliably or not,
		 * or does another unpredictable bogosity like the "result"
		 * field above, I don't know.  */
		new_font = XftFontOpenPattern(GDK_DISPLAY(), matched_pattern);
		need_destroy = FALSE;
	} else {
		new_font = NULL;
	}

	if (new_font == NULL) {
		name = vte_unparse_xft_pattern(matched_pattern);
		g_warning(_("Failed to load Xft font pattern \"%s\", "
			    "falling back to default font."), name);
		free(name);
		/* Try to use the default font. */
		new_font = XftFontOpen(GDK_DISPLAY(),
				       gdk_x11_get_default_screen(),
				       XFT_FAMILY, XftTypeString,
				       "monospace",
				       XFT_SIZE, XftTypeDouble, 12.0,
				       0);
	}
	if (new_font == NULL) {
		g_warning(_("Failed to load default Xft font."));
	}

	g_assert(pattern != new_font->pattern);
	XftPatternDestroy(pattern);
	if (need_destroy) {
		XftPatternDestroy(matched_pattern);
	}

	if (new_font != NULL) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			name = vte_unparse_xft_pattern(new_font->pattern);
			fprintf(stderr, "Opened new font `%s'.\n", name);
			free(name);
		}
#endif
		terminal->pvt->ftfont = new_font;
	}

	/* Read the metrics for the new font, if one was loaded. */
	if (terminal->pvt->ftfont != NULL) {
		ascent = terminal->pvt->ftfont->ascent;
		descent = terminal->pvt->ftfont->descent;
		memset(&glyph_info, 0, sizeof(glyph_info));
		XftTextExtents8(GDK_DISPLAY(), terminal->pvt->ftfont,
				VTE_REPRESENTATIVE_CHARACTERS,
				strlen(VTE_REPRESENTATIVE_CHARACTERS),
				&glyph_info);
		width = howmany(glyph_info.width,
				strlen(VTE_REPRESENTATIVE_CHARACTERS));
		height = MAX(terminal->pvt->ftfont->height, (ascent + descent));
		if (height == 0) {
			height = glyph_info.height;
		}
		vte_terminal_apply_metrics(terminal,
					   width, height,
					   ascent, descent);
	}
}
static void
vte_terminal_close_font_xft(VteTerminal *terminal)
{
	if (terminal->pvt->ftfont != NULL) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, "Closing Xft font.\n");
		}
#endif
		XftFontClose(GDK_DISPLAY(), terminal->pvt->ftfont);
		terminal->pvt->ftfont = NULL;
	}
}
#endif

/* Ensure that an Xlib font is loaded. */
static void
vte_terminal_open_font_xlib(VteTerminal *terminal)
{
	char *xlfds;
	long width, height, ascent, descent;
	XFontStruct **font_struct_list, font_struct;
	XRectangle ink, logical;
	char **missing_charset_list = NULL, *def_string = NULL;
	int missing_charset_count = 0;
	char **font_name_list = NULL;

	/* Simple case -- we already loaded the font. */
	if (terminal->pvt->fontset != NULL) {
		return;
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Opening Xlib font.\n");
	}
#endif
	xlfds = xlfd_from_pango_font_description(GTK_WIDGET(terminal),
						 terminal->pvt->fontdesc);
	if (xlfds == NULL) {
		xlfds = strdup(VTE_X_FIXED);
	}
	/* Open the font set. */
	terminal->pvt->fontset = XCreateFontSet(GDK_DISPLAY(),
						xlfds,
						&missing_charset_list,
						&missing_charset_count,
						&def_string);
	if (terminal->pvt->fontset != NULL) {
		vte_terminal_font_complain(xlfds,
					   missing_charset_list,
					   missing_charset_count);
	} else {
		g_warning(_("Failed to load font set \"%s\", "
			    "falling back to default font."), xlfds);
		if (missing_charset_list != NULL) {
			XFreeStringList(missing_charset_list);
			missing_charset_list = NULL;
		}
		terminal->pvt->fontset = XCreateFontSet(GDK_DISPLAY(),
							VTE_X_FIXED,
							&missing_charset_list,
							&missing_charset_count,
							&def_string);
		if (terminal->pvt->fontset == NULL) {
			g_warning(_("Failed to load default font, "
				    "crashing or behaving abnormally."));
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
	free(xlfds);
	xlfds = NULL;
	g_return_if_fail(terminal->pvt->fontset != NULL);

	/* Read the font metrics. */
	XmbTextExtents(terminal->pvt->fontset,
		       VTE_REPRESENTATIVE_CHARACTERS,
		       strlen(VTE_REPRESENTATIVE_CHARACTERS),
		       &ink, &logical);
	width = logical.width / strlen(VTE_REPRESENTATIVE_CHARACTERS);
	height = logical.height;
	ascent = height;
	descent = 0;
	if (XFontsOfFontSet(terminal->pvt->fontset,
			    &font_struct_list,
			    &font_name_list)) {
		if (font_struct_list) {
			if (font_struct_list[0]) {
				font_struct = font_struct_list[0][0];
				ascent = font_struct.max_bounds.ascent;
				descent = font_struct.max_bounds.descent;
				height = ascent + descent;
			}
		}
		font_struct_list = NULL;
		font_name_list = NULL;
	}
	xlfds = NULL;

	/* Save the new font metrics. */
	vte_terminal_apply_metrics(terminal, width, height, ascent, descent);
}

/* Free the Xlib font. */
static void
vte_terminal_close_font_xlib(VteTerminal *terminal)
{
	if (terminal->pvt->fontset != NULL) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, "Closing Xlib font.\n");
		}
#endif
		XFreeFontSet(GDK_DISPLAY(), terminal->pvt->fontset);
		terminal->pvt->fontset = NULL;
	}
}

/* Ensure that a Pango font's metrics are known. */
static void
vte_terminal_open_font_pango(VteTerminal *terminal)
{
	PangoFontDescription *desc = NULL;
	PangoContext *context = NULL;
	PangoFontMetrics *metrics = NULL;
	PangoLanguage *lang = NULL;
	PangoLayout *layout = NULL;
	PangoRectangle ink, logical;
	gint height, width, ascent, descent;

	if (terminal->pvt->pcontext != NULL) {
		return;
	}
	terminal->pvt->pcontext = vte_terminal_get_pango_context(terminal);
	context = terminal->pvt->pcontext;
	desc = terminal->pvt->fontdesc;

	/* Load a font using this description and read its metrics to find
	 * the ascent and descent. */
	if ((context != NULL) && (desc != NULL)) {
		lang = pango_context_get_language(context);
		metrics = pango_context_get_metrics(context, desc, lang);
		ascent = pango_font_metrics_get_ascent(metrics);
		descent = pango_font_metrics_get_descent(metrics);
		pango_font_metrics_unref(metrics);
		metrics = NULL;

		/* Create a layout object to get a width estimate. */
		layout = pango_layout_new(context);
		pango_layout_set_font_description(layout, desc);
		pango_layout_set_text(layout,
				      VTE_REPRESENTATIVE_CHARACTERS,
				      strlen(VTE_REPRESENTATIVE_CHARACTERS));
		pango_layout_get_extents(layout, &ink, &logical);
		width = howmany(logical.width, PANGO_SCALE);
		width = howmany(width, strlen(VTE_REPRESENTATIVE_CHARACTERS));
		height = howmany(logical.height, PANGO_SCALE);
		g_object_unref(G_OBJECT(layout));
		layout = NULL;

		/* Change the metrics. */
		vte_terminal_apply_metrics(terminal,
					   width, height,
					   ascent, descent);
	}
}
static void
vte_terminal_close_font_pango(VteTerminal *terminal)
{
	if (terminal->pvt->pcontext != NULL) {
		vte_terminal_maybe_unref_pango_context(terminal,
						       terminal->pvt->pcontext);
		terminal->pvt->pcontext = NULL;
	}
}

/* Ensure that the font's metrics are known. */
static void
vte_terminal_open_font(VteTerminal *terminal)
{
	switch (terminal->pvt->render_method) {
#ifdef HAVE_XFT
	case VteRenderXft2:
	case VteRenderXft1:
		vte_terminal_open_font_xft(terminal);
		break;
#endif
	case VteRenderPango:
	case VteRenderPangoX:
		vte_terminal_open_font_pango(terminal);
		break;
	case VteRenderXlib:
		vte_terminal_open_font_xlib(terminal);
		break;
	}
}
static void
vte_terminal_close_font(VteTerminal *terminal)
{
	vte_terminal_close_font_xlib(terminal);
	vte_terminal_close_font_pango(terminal);
#ifdef HAVE_XFT
	vte_terminal_close_font_xft(terminal);
#endif
}

/**
 * vte_terminal_set_font:
 * @terminal: a #VteTerminal
 * @font_desc: The #PangoFontDescription of the desired font.
 *
 * Sets the font used for rendering all text displayed by the terminal,
 * overriding any fonts set using gtk_widget_modify_font().  The terminal
 * will immediately attempt to load the desired font, retrieve its
 * metrics, and attempts to resize itself to keep the same number of rows
 * and columns.
 *
 */
void
vte_terminal_set_font(VteTerminal *terminal,
		      const PangoFontDescription *font_desc)
{
	GtkWidget *widget;
	PangoFontDescription *desc;

	g_return_if_fail(terminal != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);

	/* Destroy the font padding records. */
	if (terminal->pvt->fontpaddingl != NULL) {
		g_tree_destroy(terminal->pvt->fontpaddingl);
	}
	terminal->pvt->fontpaddingl = g_tree_new(vte_compare_direct);
	if (terminal->pvt->fontpaddingr != NULL) {
		g_tree_destroy(terminal->pvt->fontpaddingr);
	}
	terminal->pvt->fontpaddingr = g_tree_new(vte_compare_direct);

	/* Create an owned font description. */
	if (font_desc != NULL) {
		desc = pango_font_description_copy(font_desc);
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			if (desc) {
				char *tmp;
				tmp = pango_font_description_to_string(desc);
				fprintf(stderr, "Using pango font \"%s\".\n", tmp);
				g_free (tmp);
			}
		}
#endif
	} else {
		gtk_widget_ensure_style(widget);
		desc = pango_font_description_copy(widget->style->font_desc);
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, "Using default pango font.\n");
		}
#endif
	}

	/* Save the new font description. */
	if (terminal->pvt->fontdesc != NULL) {
		pango_font_description_free(terminal->pvt->fontdesc);
	}
	terminal->pvt->fontdesc = desc;

	/* Free the older fonts and load the new ones. */
	vte_terminal_close_font(terminal);
	vte_terminal_open_font(terminal);
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
	PangoFontDescription *font_desc;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(name != NULL);
	g_return_if_fail(strlen(name) > 0);

	font_desc = pango_font_description_from_string(name);
	vte_terminal_set_font(terminal, font_desc);
	pango_font_description_free(font_desc);
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
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	if (terminal->pvt->pty_master != -1) {
		/* Use an ioctl to read the size of the terminal. */
		if (_vte_pty_get_size(terminal->pvt->pty_master, &columns, &rows) != 0) {
			g_warning(_("Error reading PTY size, using defaults: "
				    "%s."), strerror(errno));
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
	struct winsize size;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Setting PTY size to %ldx%ld.\n",
			columns, rows);
	}
#endif
	if (terminal->pvt->pty_master != -1) {
		memset(&size, 0, sizeof(size));
		size.ws_row = rows;
		size.ws_col = columns;
		/* Try to set the terminal size. */
		if (_vte_pty_set_size(terminal->pvt->pty_master, columns, rows) != 0) {
			g_warning(_("Error setting PTY size: %s."),
				    strerror(errno));
		}
	} else {
		terminal->row_count = rows;
		terminal->column_count = columns;
	}
	/* Read the terminal size, in case something went awry. */
	vte_terminal_refresh_size(terminal);
}

/* Redraw the widget. */
static void
vte_terminal_handle_scroll(VteTerminal *terminal)
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
		vte_terminal_match_contents_clear(terminal);
		vte_terminal_scroll_region(terminal, screen->scroll_delta,
					   terminal->row_count, dy);
		vte_terminal_emit_contents_changed(terminal);
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
							     (gpointer)vte_terminal_handle_scroll,
							     terminal);
			g_object_unref(terminal->adjustment);
		}
		/* Set the new adjustment object. */
		terminal->adjustment = adjustment;
		/* We care about the offset, not the top or bottom. */
		g_signal_connect_swapped(terminal->adjustment,
					 "value_changed",
					 G_CALLBACK(vte_terminal_handle_scroll),
					 terminal);
	}
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
	const char *code, *value;
	gboolean found_cr = FALSE, found_lf = FALSE;
	char *stripped;
	gssize stripped_length;
	int columns, rows;
	GQuark quark;
	char *tmp;
	int i;

	/* Set the emulation type, for reference. */
	if (emulation == NULL) {
		emulation = VTE_DEFAULT_EMULATION;
	}
	quark = g_quark_from_string(emulation);
	terminal->pvt->emulation = g_quark_to_string(quark);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Setting emulation to `%s'...", emulation);
	}
#endif
	/* Find and read the right termcap file. */
	vte_terminal_set_termcap(terminal, NULL, FALSE);

	/* Create a table to hold the control sequences. */
	if (terminal->pvt->table != NULL) {
		_vte_table_free(terminal->pvt->table);
	}
	terminal->pvt->table = _vte_table_new();

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
				      (gpointer)vte_sequence_handlers[i].handler);
		}
	}

	/* Load the known capability strings from the termcap structure into
	 * the table for recognition. */
	for (i = 0;
	     _vte_terminal_capability_strings[i].capability != NULL;
	     i++) {
		if (_vte_terminal_capability_strings[i].key) {
			continue;
		}
		code = _vte_terminal_capability_strings[i].capability;
		tmp = _vte_termcap_find_string(terminal->pvt->termcap,
					       terminal->pvt->emulation,
					       code);
		if ((tmp != NULL) && (tmp[0] != '\0')) {
			_vte_termcap_strip(tmp, &stripped, &stripped_length);
			_vte_table_add(terminal->pvt->table,
				       stripped, stripped_length,
				       code,
				       0);
			if (stripped[0] == '\r') {
				found_cr = TRUE;
			} else
			if (stripped[0] == '\n') {
				if ((strcmp(code, "sf") == 0) ||
				    (strcmp(code, "do") == 0)) {
					found_lf = TRUE;
				}
			}
			g_free(stripped);
		}
		g_free(tmp);
	}

	/* Add emulator-specific sequences. */
	if (strstr(emulation, "xterm") || strstr(emulation, "dtterm")) {
		/* Add all of the xterm-specific stuff. */
		for (i = 0;
		     _vte_xterm_capability_strings[i].value != NULL;
		     i++) {
			code = _vte_xterm_capability_strings[i].code;
			value = _vte_xterm_capability_strings[i].value;
			_vte_termcap_strip(code, &stripped, &stripped_length);
			_vte_table_add(terminal->pvt->table,
				       stripped, stripped_length,
				       value, 0);
			g_free(stripped);
		}
	}

	/* Always define cr and lf. */
	if (!found_cr) {
		_vte_table_add(terminal->pvt->table, "\r", 1, "cr", 0);
	}
	if (!found_lf) {
		_vte_table_add(terminal->pvt->table, "\n", 1, "sf", 0);
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Trie contents:\n");
		_vte_table_print(terminal->pvt->table);
		fprintf(stderr, "\n");
	}
#endif

	/* Read emulation flags. */
	terminal->pvt->flags.am = _vte_termcap_find_boolean(terminal->pvt->termcap,
							    terminal->pvt->emulation,
							    "am");
	terminal->pvt->flags.bw = _vte_termcap_find_boolean(terminal->pvt->termcap,
							    terminal->pvt->emulation,
							    "bw");
	terminal->pvt->flags.ul = _vte_termcap_find_boolean(terminal->pvt->termcap,
							    terminal->pvt->emulation,
							    "ul");

	/* Resize to the given default. */
	columns = _vte_termcap_find_numeric(terminal->pvt->termcap,
					    terminal->pvt->emulation,
					    "co");
	rows = _vte_termcap_find_numeric(terminal->pvt->termcap,
					 terminal->pvt->emulation,
					 "li");
	terminal->pvt->default_column_count = columns;
	terminal->pvt->default_row_count = rows;

	/* Notify observers that we changed our emulation. */
	vte_terminal_emit_emulation_changed(terminal);
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

/* Set the path to the termcap file we read, and read it in. */
static void
vte_terminal_set_termcap(VteTerminal *terminal, const char *path,
			 gboolean reset)
{
	struct stat st;
	char path_default[PATH_MAX];

	if (path == NULL) {
		snprintf(path_default, sizeof(path_default),
			 DATADIR "/" PACKAGE "/termcap/%s",
			 terminal->pvt->emulation ?
			 terminal->pvt->emulation : VTE_DEFAULT_EMULATION);
		if (stat(path_default, &st) == 0) {
			path = path_default;
		} else {
			path = "/etc/termcap";
		}
	}
	terminal->pvt->termcap_path = g_quark_to_string(g_quark_from_string(path));
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Loading termcap `%s'...",
			terminal->pvt->termcap_path);
	}
#endif
	if (terminal->pvt->termcap) {
		_vte_termcap_free(terminal->pvt->termcap);
	}
	terminal->pvt->termcap = _vte_termcap_new(path);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "\n");
	}
#endif
	if (reset) {
		vte_terminal_set_emulation(terminal, terminal->pvt->emulation);
	}
}

static void
vte_terminal_reset_rowdata(VteRing **ring, glong lines)
{
	VteRing *new_ring;
	GArray *row;
	long i, next;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Sizing scrollback buffer to %ld lines.\n",
			lines);
	}
#endif
	if (*ring && (_vte_ring_max(*ring) == lines)) {
		return;
	}
	new_ring = _vte_ring_new_with_delta(lines,
					    *ring ? _vte_ring_delta(*ring) : 0,
					    vte_free_row_data,
					    NULL);
	if (*ring) {
		next = _vte_ring_next(*ring);
		for (i = _vte_ring_delta(*ring); i < next; i++) {
			row = _vte_ring_index(*ring, GArray*, i);
			_vte_ring_append(new_ring, row);
		}
		_vte_ring_free(*ring, FALSE);
	}
	*ring = new_ring;
}

/* Initialize the terminal widget after the base widget stuff is initialized.
 * We need to create a new psuedo-terminal pair, read in the termcap file, and
 * set ourselves up to do the interpretation of sequences. */
static void
vte_terminal_init(VteTerminal *terminal, gpointer *klass)
{
	VteTerminalPrivate *pvt;
	GtkWidget *widget;
	struct passwd *pwd;
	GtkAdjustment *adjustment;
	struct timezone tz;
	struct timeval tv;
	enum VteRenderMethod render_max;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	widget = GTK_WIDGET(terminal);
	GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

	/* Set up public-facing members. */

	/* Set an adjustment for the application to use to control scrolling. */
	adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0));
	vte_terminal_set_scroll_adjustment(terminal, adjustment);

	/* Initialize the default titles. */
	terminal->window_title = NULL;
	terminal->icon_title = NULL;

	/* Initialize private data. */
	pvt = terminal->pvt = g_malloc0(sizeof(*terminal->pvt));

	/* Load the termcap data and set up the emulation. */
	pvt->termcap = NULL;
	pvt->table = NULL;
	pvt->termcap_path = NULL;
	memset(&pvt->flags, 0, sizeof(pvt->flags));
	pvt->flags.am = FALSE;
	pvt->flags.bw = FALSE;
	pvt->flags.ul = FALSE;
	pvt->keypad_mode = VTE_KEYMODE_NORMAL;
	pvt->cursor_mode = VTE_KEYMODE_NORMAL;
	pvt->sun_fkey_mode = FALSE;
	pvt->hp_fkey_mode = FALSE;
	pvt->legacy_fkey_mode = FALSE;
	pvt->vt220_fkey_mode = FALSE;
	pvt->dec_saved = g_hash_table_new(g_direct_hash, g_direct_equal);
	pvt->default_column_count = 80;
	pvt->default_row_count = 24;

	/* Setting the terminal type and size requires the PTY master to
	 * be set up properly first. */
	pvt->pty_master = -1;
	vte_terminal_set_termcap(terminal, NULL, FALSE);
	vte_terminal_set_emulation(terminal, NULL);
	vte_terminal_set_size(terminal,
			      pvt->default_column_count,
			      pvt->default_row_count);

	/* Determine what the user's shell is. */
	if (pvt->shell == NULL) {
		pwd = getpwuid(getuid());
		if (pwd != NULL) {
			pvt->shell = pwd->pw_shell;
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_MISC)) {
				fprintf(stderr, "Using user's shell (%s).\n",
					pvt->shell);
			}
#endif
		}
	}
	if (pvt->shell == NULL) {
		pvt->shell = "/bin/sh";
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, "Using default shell (%s).\n",
				pvt->shell);
		}
#endif
	}
	pvt->shell = g_quark_to_string(g_quark_from_string(pvt->shell));
	pvt->pty_master = -1;
	pvt->pty_input = NULL;
	pvt->pty_input_source = -1;
	pvt->pty_output = NULL;
	pvt->pty_output_source = -1;
	pvt->pty_pid = -1;

	/* Set up I/O encodings. */
	pvt->encoding = NULL;
	pvt->substitutions = _vte_iso2022_new();
	pvt->incoming = _vte_buffer_new();
	pvt->processing = FALSE;
	pvt->processing_tag = 0;
	pvt->outgoing = _vte_buffer_new();
	pvt->incoming_conv = (GIConv) -1;
	pvt->outgoing_conv_wide = (GIConv) -1;
	pvt->outgoing_conv_utf8 = (GIConv) -1;
	pvt->conv_buffer = _vte_buffer_new();
	vte_terminal_set_encoding(terminal, NULL);
	g_assert(terminal->pvt->encoding != NULL);

	/* Initialize the screens and histories. */
	pvt->alternate_screen.row_data = NULL;
	vte_terminal_reset_rowdata(&pvt->alternate_screen.row_data,
				   pvt->scrollback_lines);
	pvt->alternate_screen.cursor_current.row = 0;
	pvt->alternate_screen.cursor_current.col = 0;
	pvt->alternate_screen.cursor_saved.row = 0;
	pvt->alternate_screen.cursor_saved.col = 0;
	pvt->alternate_screen.insert_delta = 0;
	pvt->alternate_screen.scroll_delta = 0;
	pvt->alternate_screen.insert_mode = FALSE;
	pvt->alternate_screen.origin_mode = FALSE;
	pvt->alternate_screen.reverse_mode = FALSE;
	pvt->alternate_screen.status_line = FALSE;
	pvt->alternate_screen.status_line_contents = g_string_new("");
	pvt->screen = &terminal->pvt->alternate_screen;
	vte_terminal_set_default_attributes(terminal);

	pvt->normal_screen.row_data = NULL;
	vte_terminal_reset_rowdata(&pvt->normal_screen.row_data,
				   pvt->scrollback_lines);
	pvt->normal_screen.cursor_current.row = 0;
	pvt->normal_screen.cursor_current.col = 0;
	pvt->normal_screen.cursor_saved.row = 0;
	pvt->normal_screen.cursor_saved.col = 0;
	pvt->normal_screen.insert_delta = 0;
	pvt->normal_screen.scroll_delta = 0;
	pvt->normal_screen.insert_mode = FALSE;
	pvt->normal_screen.origin_mode = FALSE;
	pvt->normal_screen.reverse_mode = FALSE;
	pvt->normal_screen.status_line = FALSE;
	pvt->normal_screen.status_line_contents = g_string_new("");
	pvt->screen = &terminal->pvt->normal_screen;
	vte_terminal_set_default_attributes(terminal);

	/* Selection info. */
	pvt->word_chars = NULL;
	pvt->has_selection = FALSE;
	pvt->selecting = FALSE;
	pvt->selecting_restart = FALSE;
	pvt->selecting_had_delta = FALSE;
	pvt->selection = NULL;
	pvt->selection_start.x = 0;
	pvt->selection_start.y = 0;
	pvt->selection_end.x = 0;
	pvt->selection_end.y = 0;
	vte_terminal_set_word_chars(terminal, NULL);

	/* Miscellaneous options. */
	vte_terminal_set_backspace_binding(terminal, VTE_ERASE_AUTO);
	vte_terminal_set_delete_binding(terminal, VTE_ERASE_AUTO);
	pvt->meta_sends_escape = TRUE;
	pvt->audible_bell = TRUE;
	pvt->visible_bell = FALSE;
	pvt->margin_bell = FALSE;
	pvt->bell_margin = 10;
	pvt->allow_bold = TRUE;
	pvt->nrc_mode = TRUE;
	pvt->smooth_scroll = FALSE;
	pvt->tabstops = NULL;
	vte_terminal_set_default_tabstops(terminal);

	/* Scrolling options. */
	pvt->scroll_on_output = FALSE;
	pvt->scroll_on_keystroke = TRUE;
	pvt->scrollback_lines = VTE_SCROLLBACK_MIN;
	vte_terminal_set_scrollback_lines(terminal,
					  terminal->pvt->scrollback_lines);

	/* Cursor blinking. */
	pvt->cursor_force_fg = 0;
	pvt->cursor_blinks = FALSE;
	pvt->cursor_blink_tag = 0;
	pvt->cursor_visible = TRUE;
	pvt->cursor_blink_timeout = 1000;

	/* Input options. */
	if (gettimeofday(&tv, &tz) == 0) {
		pvt->last_keypress_time = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	} else {
		pvt->last_keypress_time = 0;
	}
	pvt->mouse_send_xy_on_click = FALSE;
	pvt->mouse_send_xy_on_button = FALSE;
	pvt->mouse_hilite_tracking = FALSE;
	pvt->mouse_cell_motion_tracking = FALSE;
	pvt->mouse_all_motion_tracking = FALSE;
	pvt->mouse_last_button = 0;
	pvt->mouse_last_x = 0;
	pvt->mouse_last_y = 0;
	pvt->mouse_autohide = FALSE;
	pvt->mouse_autoscroll_tag = 0;

	/* Matching data. */
	pvt->match_contents = NULL;
	pvt->match_attributes = NULL;
	pvt->match_regexes = g_array_new(FALSE, TRUE,
					 sizeof(struct vte_match_regex));
	vte_terminal_match_hilite_clear(terminal);

	/* Server-side rendering data.  Try everything. */
	pvt->palette_initialized = FALSE;
	memset(&pvt->palette, 0, sizeof(pvt->palette));
	pvt->fontset = NULL;
	pvt->fontpaddingl = NULL;
	pvt->fontpaddingr = NULL;
	render_max = VteRenderPango;
#ifdef HAVE_XFT
	pvt->ftfont = NULL;
	render_max = VteRenderXft1;
#ifdef HAVE_XFT2
	/* GTK+ with Xft1-only = 2.0.  If we have fontconfig, it's either a
	 * devel version with fontconfig support or 2.2 or later. */
	render_max = VteRenderXft2;
#endif
	/* Let debugging users have some influence on how we render text. */
	if ((render_max == VteRenderXft2) &&
	    (getenv("VTE_USE_XFT2") != NULL)) {
		if (atol(getenv("VTE_USE_XFT2")) == 0) {
			render_max = VteRenderXft1;
		}
	}
	if ((render_max >= VteRenderXft1) &&
	    (getenv("VTE_USE_XFT") != NULL)) {
		if (atol(getenv("VTE_USE_XFT")) == 0) {
			render_max = VteRenderPango;
		}
	}
	if ((render_max >= VteRenderXft1) &&
	    (getenv("GDK_USE_XFT") != NULL)) {
		if (atol(getenv("GDK_USE_XFT")) == 0) {
			render_max = VteRenderPango;
		}
	}
#endif
	if ((render_max == VteRenderPango) &&
	    (getenv("VTE_USE_PANGO") != NULL)) {
		if (atol(getenv("VTE_USE_PANGO")) == 0) {
			render_max = VteRenderPangoX;
		}
	}
	if ((render_max == VteRenderPangoX) &&
	    (getenv("VTE_USE_PANGOX") != NULL)) {
		if (atol(getenv("VTE_USE_PANGOX")) == 0) {
			render_max = VteRenderXlib;
		}
	}
	pvt->render_method = render_max;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		switch (pvt->render_method) {
#ifdef HAVE_XFT2
		case VteRenderXft2:
			fprintf(stderr, "Using Xft2.\n");
			break;
#endif
#ifdef HAVE_XFT
		case VteRenderXft1:
			fprintf(stderr, "Using Xft1.\n");
			break;
#endif
		case VteRenderPango:
			fprintf(stderr, "Using Pango.\n");
			break;
		case VteRenderPangoX:
			fprintf(stderr, "Using PangoX.\n");
			break;
		case VteRenderXlib:
			fprintf(stderr, "Using Xlib fonts.\n");
			break;
		}
	}
#endif

	/* The font description. */
	pvt->fontdesc = NULL;
	gtk_widget_ensure_style(widget);
	vte_terminal_set_font(terminal, NULL);

	/* Input method support. */
	pvt->im_context = NULL;
	pvt->im_preedit = NULL;
	pvt->im_preedit_cursor = 0;

	/* Our accessible peer. */
	pvt->accessible = NULL;

	/* Settings we're monitoring. */
	pvt->connected_settings = NULL;

	/* Bookkeeping data for adjustment-changed signals. */
	pvt->adjustment_changed_tag = 0;

	/* Set up background information. */
	pvt->bg_transparent = FALSE;
	pvt->bg_transparent_update_pending = FALSE;
	pvt->bg_transparent_update_tag = 0;
	pvt->bg_transparent_atom = 0;
	pvt->bg_transparent_window = NULL;
	pvt->bg_transparent_image = NULL;
	pvt->bg_saturation = 0.4 * VTE_SATURATION_MAX;
	pvt->bg_image = NULL;

	/* Listen for hierarchy change notifications. */
	g_signal_connect(G_OBJECT(terminal), "hierarchy-changed",
			 G_CALLBACK(vte_terminal_hierarchy_changed),
			 NULL);

	/* Listen for style changes. */
	g_signal_connect(G_OBJECT(terminal), "style-set",
			 G_CALLBACK(vte_terminal_style_changed),
			 NULL);

	/* Mapping trees. */
	pvt->unichar_wc_map = g_tree_new(vte_compare_direct);
}

/* Tell GTK+ how much space we need. */
static void
vte_terminal_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	VteTerminal *terminal;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	terminal = VTE_TERMINAL(widget);

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

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Size request is %dx%d.\n",
			requisition->width, requisition->height);
	}
#endif
}

/* Accept a given size from GTK+. */
static void
vte_terminal_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	VteTerminal *terminal;
	glong width, height;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));

	terminal = VTE_TERMINAL(widget);

	width = (allocation->width - (2 * VTE_PAD_WIDTH)) /
		terminal->char_width;
	height = (allocation->height - (2 * VTE_PAD_WIDTH)) /
		 terminal->char_height;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Sizing window to %dx%d (%ldx%ld).\n",
			allocation->width, allocation->height,
			width, height);
	}
#endif

	/* Set our allocation to match the structure. */
	widget->allocation = *allocation;

	/* Set the size of the pseudo-terminal. */
	vte_terminal_set_size(terminal, width, height);

	/* Adjust scrolling area in case our boundaries have just been
	 * redefined to be invalid. */
	if (terminal->pvt->screen->scrolling_restricted) {
		terminal->pvt->screen->scrolling_region.start =
			CLAMP(terminal->pvt->screen->scrolling_region.start,
			      terminal->pvt->screen->insert_delta,
			      terminal->pvt->screen->insert_delta +
			      terminal->row_count - 1);
		terminal->pvt->screen->scrolling_region.end =
			CLAMP(terminal->pvt->screen->scrolling_region.end,
			      terminal->pvt->screen->insert_delta,
			      terminal->pvt->screen->insert_delta +
			      terminal->row_count - 1);
	}

	/* Adjust scrollback buffers to ensure that they're big enough. */
	vte_terminal_set_scrollback_lines(terminal,
					  MAX(terminal->pvt->scrollback_lines,
					      height));

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
	vte_terminal_adjust_adjustments(terminal, TRUE);
	vte_invalidate_all(terminal);
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

	/* Shut down accessibility. */
	if (terminal->pvt->accessible != NULL) {
		g_object_unref(G_OBJECT(terminal->pvt->accessible));
		terminal->pvt->accessible = NULL;
	}

	/* Deallocate the cursors. */
	gdk_cursor_unref(terminal->pvt->mouse_default_cursor);
	terminal->pvt->mouse_default_cursor = NULL;
	gdk_cursor_unref(terminal->pvt->mouse_mousing_cursor);
	terminal->pvt->mouse_mousing_cursor = NULL;
	gdk_cursor_unref(terminal->pvt->mouse_inviso_cursor);
	terminal->pvt->mouse_inviso_cursor = NULL;

	/* Shut down input methods. */
	g_object_unref(G_OBJECT(terminal->pvt->im_context));
	terminal->pvt->im_context = NULL;
	if (terminal->pvt->im_preedit != NULL) {
		g_free(terminal->pvt->im_preedit);
		terminal->pvt->im_preedit = NULL;
	}
	terminal->pvt->im_preedit_cursor = 0;

#ifdef HAVE_XFT
	if ((terminal->pvt->render_method == VteRenderXft1) ||
	    (terminal->pvt->render_method == VteRenderXft2)) {
		/* Clean up after Xft. */
		display = gdk_x11_drawable_get_xdisplay(widget->window);
		gvisual = gtk_widget_get_visual(widget);
		visual = gdk_x11_visual_get_xvisual(gvisual);
		gcolormap = gtk_widget_get_colormap(widget);
		colormap = gdk_x11_colormap_get_xcolormap(gcolormap);
		for (i = 0; i < G_N_ELEMENTS(terminal->pvt->palette); i++) {
			if (terminal->pvt->palette[i].ftcolor_allocated) {
				XftColorFree(display,
					     visual,
					     colormap,
					     &terminal->pvt->palette[i].ftcolor);
				terminal->pvt->palette[i].ftcolor_allocated = FALSE;
			}
		}
	}
#endif

	/* Clean up after Pango. */
	if (terminal->pvt->fontpaddingl != NULL) {
		g_tree_destroy(terminal->pvt->fontpaddingl);
		terminal->pvt->fontpaddingl = NULL;
	}
	if (terminal->pvt->fontpaddingr != NULL) {
		g_tree_destroy(terminal->pvt->fontpaddingr);
		terminal->pvt->fontpaddingr = NULL;
	}

	/* Unload fonts. */
	vte_terminal_close_font(terminal);

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

	/* Remove the GDK window. */
	if (widget->window != NULL) {
		gdk_window_destroy(widget->window);
		widget->window = NULL;
	}

	/* Remove the blink timeout function. */
	if (terminal->pvt->cursor_blink_tag) {
		g_source_remove(terminal->pvt->cursor_blink_tag);
		terminal->pvt->cursor_force_fg = 0;
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
	GtkClipboard *clipboard;
	struct vte_match_regex *regex;
	int i;

	g_return_if_fail(VTE_IS_TERMINAL(object));
	terminal = VTE_TERMINAL(object);
	object_class = G_OBJECT_GET_CLASS(G_OBJECT(object));
	widget_class = g_type_class_peek(GTK_TYPE_WIDGET);

	/* The unichar->wchar_t map. */
	if (terminal->pvt->unichar_wc_map != NULL) {
		g_tree_destroy(terminal->pvt->unichar_wc_map);
		terminal->pvt->unichar_wc_map = NULL;
	}

	/* The NLS maps. */
	if (terminal->pvt->substitutions != NULL) {
		_vte_iso2022_free(terminal->pvt->substitutions);
		terminal->pvt->substitutions = NULL;
	}

	/* Free background info. */
	if (terminal->pvt->bg_image != NULL) {
		g_object_unref(G_OBJECT(terminal->pvt->bg_image));
		terminal->pvt->bg_image = NULL;
	}
	if (terminal->pvt->bg_transparent_image != NULL) {
		g_object_unref(G_OBJECT(terminal->pvt->bg_transparent_image));
		terminal->pvt->bg_transparent_image = NULL;
	}
	if (terminal->pvt->bg_transparent_image != NULL) {
		g_object_unref(G_OBJECT(terminal->pvt->bg_transparent_image));
		terminal->pvt->bg_transparent_image = NULL;
	}
	if (terminal->pvt->bg_transparent_update_tag) {
		g_source_remove(terminal->pvt->bg_transparent_update_tag);
		terminal->pvt->bg_transparent_update_tag = 0;
	}

#ifdef HAVE_XFT2
	/* Disconnect from settings changes. */
	if (terminal->pvt->connected_settings) {
		g_signal_handlers_disconnect_by_func(G_OBJECT(terminal->pvt->connected_settings),
						     (gpointer)vte_xft_changed_cb,
						     terminal);
		terminal->pvt->connected_settings = NULL;
	}
#endif

	/* Free the fonts if we still have some loaded. */
	vte_terminal_close_font(terminal);

	/* Free the font description. */
	if (terminal->pvt->fontdesc != NULL) {
		pango_font_description_free(terminal->pvt->fontdesc);
		terminal->pvt->fontdesc = NULL;
	}

	/* Free matching data. */
	if (terminal->pvt->match_attributes != NULL) {
		g_array_free(terminal->pvt->match_attributes, TRUE);
		terminal->pvt->match_attributes = NULL;
	}
	if (terminal->pvt->match_contents != NULL) {
		g_free(terminal->pvt->match_contents);
		terminal->pvt->match_contents = NULL;
	}
	if (terminal->pvt->match_regexes != NULL) {
		for (i = 0; i < terminal->pvt->match_regexes->len; i++) {
			regex = &g_array_index(terminal->pvt->match_regexes,
					       struct vte_match_regex,
					       i);
			/* Skip holes. */
			if (regex->tag < 0) {
				continue;
			}
			regfree(&regex->reg);
			regex->tag = 0;
		}
		g_array_free(terminal->pvt->match_regexes, TRUE);
		terminal->pvt->match_regexes = NULL;
	}

	/* Disconnect from toplevel window configure events. */
	toplevel = gtk_widget_get_toplevel(GTK_WIDGET(object));
	if ((toplevel != NULL) && (G_OBJECT(toplevel) != G_OBJECT(object))) {
		g_signal_handlers_disconnect_by_func(toplevel,
						     (gpointer)vte_terminal_configure_toplevel,
						     terminal);
	}

	/* Disconnect from autoscroll requests. */
	vte_terminal_stop_autoscroll(terminal);

	/* Cancel pending adjustment change notifications. */
	if (terminal->pvt->adjustment_changed_tag) {
		g_source_remove(terminal->pvt->adjustment_changed_tag);
		terminal->pvt->adjustment_changed_tag = 0;
	}

	/* Tabstop information. */
	if (terminal->pvt->tabstops != NULL) {
		g_hash_table_destroy(terminal->pvt->tabstops);
		terminal->pvt->tabstops = NULL;
	}

	/* Free any selected text, but if we currently own the selection,
	 * throw the text onto the clipboard without an owner so that it
	 * doesn't just disappear. */
	if (terminal->pvt->selection != NULL) {
		clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
		if (gtk_clipboard_get_owner(clipboard) == G_OBJECT(terminal)) {
			gtk_clipboard_set_text(clipboard,
					       terminal->pvt->selection,
					       -1);
		}
		g_free(terminal->pvt->selection);
		terminal->pvt->selection = NULL;
	}
	if (terminal->pvt->word_chars != NULL) {
		g_array_free(terminal->pvt->word_chars, TRUE);
		terminal->pvt->word_chars = NULL;
	}

	/* Clear the output histories. */
	_vte_ring_free(terminal->pvt->normal_screen.row_data, TRUE);
	terminal->pvt->normal_screen.row_data = NULL;
	_vte_ring_free(terminal->pvt->alternate_screen.row_data, TRUE);
	terminal->pvt->alternate_screen.row_data = NULL;

	/* Clear the status lines. */
	terminal->pvt->normal_screen.status_line = FALSE;
	g_string_free(terminal->pvt->normal_screen.status_line_contents,
		      TRUE);
	terminal->pvt->alternate_screen.status_line = FALSE;
	g_string_free(terminal->pvt->alternate_screen.status_line_contents,
		      TRUE);

	/* Free conversion descriptors. */
	if (terminal->pvt->incoming_conv != ((GIConv) -1)) {
		g_iconv_close(terminal->pvt->incoming_conv);
	}
	terminal->pvt->incoming_conv = ((GIConv) -1);
	if (terminal->pvt->outgoing_conv_wide != ((GIConv) -1)) {
		g_iconv_close(terminal->pvt->outgoing_conv_wide);
	}
	terminal->pvt->outgoing_conv_wide = ((GIConv) -1);
	if (terminal->pvt->outgoing_conv_utf8 != ((GIConv) -1)) {
		g_iconv_close(terminal->pvt->outgoing_conv_utf8);
	}
	terminal->pvt->outgoing_conv_utf8 = ((GIConv) -1);

	/* Stop listening for child-exited signals. */
	g_signal_handlers_disconnect_by_func(vte_reaper_get(),
					     (gpointer)vte_terminal_catch_child_exited,
					     terminal);

	/* Stop processing input. */
	if (terminal->pvt->processing_tag != 0) {
		g_source_remove(terminal->pvt->processing_tag);
		terminal->pvt->processing_tag = 0;
	}

	/* Discard any pending data. */
	if (terminal->pvt->incoming != NULL) {
		_vte_buffer_free(terminal->pvt->incoming);
	}
	terminal->pvt->incoming = NULL;
	if (terminal->pvt->outgoing != NULL) {
		_vte_buffer_free(terminal->pvt->outgoing);
	}
	terminal->pvt->outgoing = NULL;
	if (terminal->pvt->conv_buffer != NULL) {
		_vte_buffer_free(terminal->pvt->conv_buffer);
	}
	terminal->pvt->conv_buffer = NULL;

	/* Stop the child and stop watching for input from the child. */
	if (terminal->pvt->pty_pid > 0) {
		kill(-terminal->pvt->pty_pid, SIGHUP);
	}
	terminal->pvt->pty_pid = 0;
	if (terminal->pvt->pty_input != NULL) {
		g_io_channel_unref(terminal->pvt->pty_input);
		terminal->pvt->pty_input = NULL;
		g_source_remove(terminal->pvt->pty_input_source);
		terminal->pvt->pty_input_source = -1;
	}
	if (terminal->pvt->pty_output != NULL) {
		g_io_channel_unref(terminal->pvt->pty_output);
		terminal->pvt->pty_output = NULL;
		g_source_remove(terminal->pvt->pty_output_source);
		terminal->pvt->pty_output_source = -1;
	}
	if (terminal->pvt->pty_master != -1) {
		_vte_pty_close(terminal->pvt->pty_master);
		terminal->pvt->pty_master = -1;
	}

	/* Clear some of our strings. */
	terminal->pvt->shell = NULL;

	/* Remove hash tables. */
	if (terminal->pvt->dec_saved != NULL) {
		g_hash_table_destroy(terminal->pvt->dec_saved);
		terminal->pvt->dec_saved = NULL;
	}

	/* Clean up emulation structures. */
	memset(&terminal->pvt->flags, 0, sizeof(terminal->pvt->flags));
	g_tree_destroy(terminal->pvt->sequences);
	terminal->pvt->sequences= NULL;
	terminal->pvt->emulation = NULL;
	terminal->pvt->termcap_path = NULL;
	if (terminal->pvt->table != NULL) {
		_vte_table_free(terminal->pvt->table);
		terminal->pvt->table = NULL;
	}
	_vte_termcap_free(terminal->pvt->termcap);
	terminal->pvt->termcap = NULL;

	/* Done with our private data. */
	g_free(terminal->pvt);
	terminal->pvt = NULL;

	/* Free public-facing data. */
	if (terminal->window_title != NULL) {
		g_free(terminal->window_title);
		terminal->window_title = NULL;
	}
	if (terminal->icon_title != NULL) {
		g_free(terminal->icon_title);
		terminal->icon_title = NULL;
	}
	if (terminal->adjustment != NULL) {
		g_object_unref(terminal->adjustment);
		terminal->adjustment = NULL;
	}

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
	GdkPixmap *pixmap, *mask;
	GdkColor black = {0,0,0}, color;
	int attributes_mask = 0, i;

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

	/* Set up the desired palette. */
	if (!terminal->pvt->palette_initialized) {
		vte_terminal_set_default_colors(terminal);
	}

	/* Set the realized flag. */
	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

	/* Add a filter to watch for property changes. */
	gdk_window_add_filter(widget->window,
			      vte_terminal_filter_property_changes,
			      terminal);

	/* Actually load the font. */
	vte_terminal_set_font(terminal, terminal->pvt->fontdesc);

	/* Allocate colors. */
	for (i = 0; i < G_N_ELEMENTS(terminal->pvt->palette); i++) {
		color.red = terminal->pvt->palette[i].red;
		color.green = terminal->pvt->palette[i].green;
		color.blue = terminal->pvt->palette[i].blue;
		color.pixel = 0;
		vte_terminal_set_color_internal(terminal, i, &color);
	}

	/* Set up the background. */
	vte_terminal_setup_background(terminal, TRUE);

	/* Setup cursor blink */
	terminal->pvt->cursor_blink_tag = g_timeout_add_full(G_PRIORITY_LOW,
							     0,
							     vte_invalidate_cursor_periodic,
							     terminal,
							     NULL);

	/* Set up input method support.  FIXME: do we need to handle the
	 * "retrieve-surrounding" and "delete-surrounding" events? */
	if (terminal->pvt->im_context != NULL) {
		g_object_unref(G_OBJECT(terminal->pvt->im_context));
		terminal->pvt->im_context = NULL;
	}
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

	/* Create the stock cursors. */
	terminal->pvt->mouse_default_cursor =
		gdk_cursor_new(VTE_DEFAULT_CURSOR);
	terminal->pvt->mouse_mousing_cursor =
		gdk_cursor_new(VTE_MOUSING_CURSOR);

	/* Create our invisible cursor. */
	pixmap = gdk_pixmap_new(widget->window, 1, 1, 1);
	mask = gdk_pixmap_new(widget->window, 1, 1, 1);
	terminal->pvt->mouse_inviso_cursor =
		gdk_cursor_new_from_pixmap(pixmap, mask,
					   &black, &black, 0, 0);
	g_object_unref(G_OBJECT(pixmap));
	g_object_unref(G_OBJECT(mask));

	/* Grab input focus. */
	if (!GTK_WIDGET_HAS_FOCUS(widget)) {
		gtk_widget_grab_focus(widget);
	}
}

static void
vte_terminal_determine_colors(VteTerminal *terminal,
			      const struct vte_charcell *cell,
			      gboolean reverse, int *fore, int *back)
{
	g_assert(fore != NULL);
	g_assert(back != NULL);

	/* Determine what the foreground and background colors for rendering
	 * text should be. */
	if (reverse ^ ((cell != NULL) && (cell->reverse))) {
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
		if (*fore == VTE_DEF_FG) {
			*fore = VTE_BOLD_FG;
		} else
		if ((*fore != VTE_DEF_BG) && (*fore < VTE_COLOR_SET_SIZE)) {
			*fore += VTE_COLOR_BRIGHT_OFFSET;
		}
	}
	if (cell && cell->half) {
		if (*fore == VTE_DEF_FG) {
			*fore = VTE_DIM_FG;
		} else
		if ((*fore < VTE_COLOR_SET_SIZE)) {
			*fore += VTE_COLOR_DIM_OFFSET;
		}
	}
	if (cell && cell->standout) {
		if (*back < VTE_COLOR_SET_SIZE) {
			*back += VTE_COLOR_BRIGHT_OFFSET;
		}
	}
}

#if HAVE_XFT
/* Try to map some common characters which are frequently missing from fonts
 * to others which look the same and may be there. */
static XftChar32
vte_terminal_xft_remap_char(Display *display, XftFont *font, XftChar32 orig)
{
	XftChar32 new;

	if (XftGlyphExists(display, font, orig)) {
		return orig;
	}

	switch (orig) {
	case 0:			/* NUL */
	case 0x00A0:		/* NO-BREAK SPACE */
		new = 0x0020;	/* SPACE */
		break;
	case 0x2010:		/* HYPHEN */
	case 0x2011:		/* NON-BREAKING HYPHEN */
	case 0x2012:		/* FIGURE DASH */
	case 0x2013:		/* EN DASH */
	case 0x2014:		/* EM DASH */
	case 0x2212:		/* MINUS SIGN */
		new = 0x002D;	/* HYPHEN-MINUS */
		break;
	default:
		return orig;
	}

	if (XftGlyphExists(display, font, new)) {
		return new;
	} else {
		return orig;
	}
}
#endif

/* Check if a unicode character is actually a graphic character we draw
 * ourselves to handle cases where fonts don't have glyphs for them. */
static gboolean
vte_unichar_isgraphic(gunichar c)
{
	switch (c) {
	case 0x2500: /* horizontal line */
	case 0x2502: /* vertical line */
	case 0x250c: /* upleft corner */
	case 0x2510: /* upright corner */
	case 0x2514: /* downleft corner */
	case 0x2518: /* downright corner */
	case 0x2524: /* right t */
	case 0x251c: /* left t */
	case 0x2534: /* up tee */
	case 0x252c: /* down tee */
	case 0x253c: /* cross */
	case 0x2592: /* checkerboard */
	case 0x25c6: /* diamond */
	case 0x00b0: /* degree */
	case 0x00b1: /* plus/minus */
	case 0x00b7: /* bullet */
	case 0x2190: /* left arrow */
	case 0x2192: /* right arrow */
	case 0x2193: /* down arrow */
	case 0x2191: /* up arrow */
	case 0x25ae: /* block */
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
	case 0x2264: /* <= */
	case 0x2265: /* >= */
	case 0x2260: /* != */
		return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

/* Draw the graphic representation of an alternate font graphics character. */
static void
vte_terminal_draw_graphic(VteTerminal *terminal, gunichar c,
			  gint fore, gint back, gboolean draw_default_bg,
			  gint x, gint y, gint column_width, gint row_height,
			  Display *display,
			  GdkDrawable *gdrawable, Drawable drawable,
			  GdkGC *ggc, GC gc)
{
	XPoint diamond[4];
	gint xcenter, xright, ycenter, ybottom, i, j, draw;

	x += VTE_PAD_WIDTH;
	y += VTE_PAD_WIDTH;

	/* If this is a unicode special graphics character, map it to one of
	 * the characters we know how to draw. */
	switch (c) {
	case 0x2500: /* horizontal line */
		c = 'q';
		break;
	case 0x2502: /* vertical line */
		c = 'x';
		break;
	case 0x250c: /* upleft corner */
		c = 'l';
		break;
	case 0x2510: /* upright corner */
		c = 'k';
		break;
	case 0x2514: /* downleft corner */
		c = 'm';
		break;
	case 0x2518: /* downright corner */
		c = 'j';
		break;
	case 0x2524: /* right t (points left) */
		c = 'u';
		break;
	case 0x251c: /* left t (points right) */
		c = 't';
		break;
	case 0x2534: /* bottom tee (points up) */
		c = 'v';
		break;
	case 0x252c: /* top tee (points down) */
		c = 'w';
		break;
	case 0x253c: /* cross */
		c = 'n';
		break;
	case 0x2592: /* checkerboard */
		c = 'a';
		break;
	case 0x25c6: /* diamond */
		c = 96;
		break;
	case 0x00b0: /* degree */
		c = 'f';
		break;
	case 0x00b1: /* plus/minus */
		c = 'g';
		break;
	case 0x00b7: /* bullet */
		c = 126;
		break;
	case 0x23ba: /* scanline 1/9 */
		c = 'o';
		break;
	case 0x23bb: /* scanline 3/9 */
		c = 'p';
		break;
	case 0x23bc: /* scanline 7/9 */
		c = 'r';
		break;
	case 0x23bd: /* scanline 9/9 */
		c = 's';
		break;
	case 0x2409: /* ht */
		c = 'b';
		break;
	case 0x240c: /*  ff */
		c = 'c';
		break;
	case 0x240d: /* cr */
		c = 'd';
		break;
	case 0x240a: /* lf */
		c = 'e';
		break;
	case 0x2424: /* nl */
		c = 'h';
		break;
	case 0x240b: /* vt */
		c = 'i';
		break;
	case 0x2264: /* <= */
		c = 'y';
		break;
	case 0x2265: /* >= */
		c = 'z';
		break;
	case 0x2260: /* != */
		c = '|';
		break;

	case 0x2190: /* left arrow */
	case 0x2192: /* right arrow */
	case 0x2193: /* down arrow */
	case 0x2191: /* up arrow */
	case 0x25ae: /* block */
	default:
		break;
	}

	xright = x + column_width;
	ybottom = y + row_height;
	xcenter = (x + xright) / 2;
	ycenter = (y + ybottom) / 2;

	if ((back != VTE_DEF_BG) || draw_default_bg) {
		XSetForeground(display, gc, terminal->pvt->palette[back].pixel);
		XFillRectangle(display, drawable, gc,
			       x, y,
			       column_width, row_height);
	}
	XSetForeground(display, gc, terminal->pvt->palette[fore].pixel);
	switch (c) {
	case 0x25ae: /* solid rectangle */
		XFillRectangle(display, drawable, gc, x, y,
			       xright - x, ybottom - y);
		break;
	case 95:
		/* drawing a blank */
		break;
	case 96:
		/* diamond */
		diamond[0].x = xcenter;
		diamond[0].y = y + 1;
		diamond[1].x = xright - 1;
		diamond[1].y = ycenter;
		diamond[2].x = xcenter;
		diamond[2].y = ybottom - 1;
		diamond[3].x = x + 1;
		diamond[3].y = ycenter;
		XFillPolygon(display, drawable, gc,
			     diamond, 4,
			     Convex, CoordModeOrigin);
		break;
	case 97:  /* a */
		for (i = x; i <= xright; i++) {
			draw = ((i - x) & 1) == 0;
			for (j = y; j < ybottom; j++) {
				if (draw) {
					XDrawPoint(display, drawable, gc, i, j);
				}
				draw = !draw;
			}
		}
		break;
	case 98:  /* b */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* H */
		XDrawLine(display, drawable, gc,
			  x, y,
			  x, ycenter);
		XDrawLine(display, drawable, gc,
			  xcenter, y,
			  xcenter, ycenter);
		XDrawLine(display, drawable, gc,
			  x, (y + ycenter) / 2,
			  xcenter, (y + ycenter) / 2);
		/* T */
		XDrawLine(display, drawable, gc,
			  xcenter, ycenter,
			  xright - 1, ycenter);
		XDrawLine(display, drawable, gc,
			  (xcenter + xright) / 2, ycenter,
			  (xcenter + xright) / 2, ybottom - 1);
		break;
	case 99:  /* c */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* F */
		XDrawLine(display, drawable, gc,
			  x, y,
			  x, ycenter);
		XDrawLine(display, drawable, gc,
			  x, y,
			  xcenter, y);
		XDrawLine(display, drawable, gc,
			  x, (y + ycenter) / 2,
			  xcenter, (y + ycenter) / 2);
		/* F */
		XDrawLine(display, drawable, gc,
			  xcenter, ycenter,
			  xcenter, ybottom - 1);
		XDrawLine(display, drawable, gc,
			  xcenter, ycenter,
			  xright - 1, ycenter);
		XDrawLine(display, drawable, gc,
			  xcenter, (ycenter + ybottom) / 2,
			  xright - 1, (ycenter + ybottom) / 2);
		break;
	case 100: /* d */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* C */
		XDrawLine(display, drawable, gc,
			  x, y,
			  x, ycenter);
		XDrawLine(display, drawable, gc,
			  x, y,
			  xcenter, y);
		XDrawLine(display, drawable, gc,
			  x, ycenter,
			  xcenter, ycenter);
		/* R */
		XDrawLine(display, drawable, gc,
			  xcenter, ycenter,
			  xcenter, ybottom - 1);
		XDrawLine(display, drawable, gc,
			  xcenter, ycenter,
			  xright - 1, ycenter);
		XDrawLine(display, drawable, gc,
			  xright - 1, ycenter,
			  xright - 1, (ycenter + ybottom) / 2);
		XDrawLine(display, drawable, gc,
			  xright - 1, (ycenter + ybottom) / 2,
			  xcenter, (ycenter + ybottom) / 2);
		XDrawLine(display, drawable, gc,
			  xcenter, (ycenter + ybottom) / 2,
			  xright - 1, ybottom - 1);
		break;
	case 101: /* e */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* L */
		XDrawLine(display, drawable, gc,
			  x, y,
			  x, ycenter);
		XDrawLine(display, drawable, gc,
			  x, ycenter,
			  xcenter, ycenter);
		/* F */
		XDrawLine(display, drawable, gc,
			  xcenter, ycenter,
			  xcenter, ybottom - 1);
		XDrawLine(display, drawable, gc,
			  xcenter, ycenter,
			  xright - 1, ycenter);
		XDrawLine(display, drawable, gc,
			  xcenter, (ycenter + ybottom) / 2,
			  xright - 1, (ycenter + ybottom) / 2);
		break;
	case 102: /* f */
		/* litle circle */
		diamond[0].x = xcenter - 1;
		diamond[0].y = ycenter;
		diamond[1].x = xcenter;
		diamond[1].y = ycenter - 1;
		diamond[2].x = xcenter + 1;
		diamond[2].y = ycenter;
		diamond[3].x = xcenter;
		diamond[3].y = ycenter + 1;
		XFillPolygon(display, drawable, gc,
			     diamond, 4,
			     Convex, CoordModeOrigin);
		break;
	case 103: /* g */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* +/- */
		XDrawLine(display, drawable, gc,
			  xcenter, (y + ycenter) / 2,
			  xcenter, (ycenter + ybottom) / 2);
		XDrawLine(display, drawable, gc,
			  (x + xcenter) / 2, ycenter,
			  (xcenter + xright) / 2, ycenter);
		XDrawLine(display, drawable, gc,
			  (x + xcenter) / 2,
			  (ycenter + ybottom) / 2,
			  (xcenter + xright) / 2,
			  (ycenter + ybottom) / 2);
		break;
	case 104: /* h */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* N */
		XDrawLine(display, drawable, gc,
			  x, y,
			  x, ycenter);
		XDrawLine(display, drawable, gc,
			  x, y,
			  xcenter, ycenter);
		XDrawLine(display, drawable, gc,
			  xcenter, y,
			  xcenter, ycenter);
		/* L */
		XDrawLine(display, drawable, gc,
			  xcenter, ycenter,
			  xcenter, ybottom - 1);
		XDrawLine(display, drawable, gc,
			  xcenter, ybottom - 1,
			  xright - 1, ybottom - 1);
		break;
	case 105: /* i */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* V */
		XDrawLine(display, drawable, gc,
			  x, y,
			  (x + xcenter) / 2, ycenter);
		XDrawLine(display, drawable, gc,
			  (x + xcenter) / 2, ycenter,
			  xcenter, y);
		/* T */
		XDrawLine(display, drawable, gc,
			  xcenter, ycenter,
			  xright - 1, ycenter);
		XDrawLine(display, drawable, gc,
			  (xcenter + xright) / 2, ycenter,
			  (xcenter + xright) / 2, ybottom - 1);
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
		break;
	case 110: /* n */
		XFillRectangle(display,
			       drawable,
			       gc,
			       xcenter,
			       y,
			       VTE_LINE_WIDTH,
			       row_height);
		XFillRectangle(display,
			       drawable,
			       gc,
			       x,
			       ycenter,
			       column_width,
			       VTE_LINE_WIDTH);
		break;
	case 111: /* o */
		XFillRectangle(display,
			       drawable,
			       gc,
			       x,
			       y,
			       column_width,
			       VTE_LINE_WIDTH);
		break;
	case 112: /* p */
		XFillRectangle(display,
			       drawable,
			       gc,
			       x,
			       (y + ycenter) / 2,
			       column_width,
			       VTE_LINE_WIDTH);
		break;
	case 113: /* q */
		XFillRectangle(display,
			       drawable,
			       gc,
			       x,
			       ycenter,
			       column_width,
			       VTE_LINE_WIDTH);
		break;
	case 114: /* r */
		XFillRectangle(display,
			       drawable,
			       gc,
			       x,
			       (ycenter + ybottom) / 2,
			       column_width,
			       VTE_LINE_WIDTH);
		break;
	case 115: /* s */
		XFillRectangle(display,
			       drawable,
			       gc,
			       x,
			       ybottom-1,
			       column_width,
			       VTE_LINE_WIDTH);
		break;
	case 116: /* t */
		XFillRectangle(display,
			       drawable,
			       gc,
			       xcenter,
			       y,
			       VTE_LINE_WIDTH,
			       row_height);
		XFillRectangle(display,
			       drawable,
			       gc,
			       xcenter,
			       ycenter,
			       xright - xcenter,
			       VTE_LINE_WIDTH);
		break;
	case 117: /* u */
		XFillRectangle(display,
			       drawable,
			       gc,
			       xcenter,
			       y,
			       VTE_LINE_WIDTH,
			       row_height);
		XFillRectangle(display,
			       drawable,
			       gc,
			       x,
			       ycenter,
			       xcenter - x + VTE_LINE_WIDTH,
			       VTE_LINE_WIDTH);
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
			       column_width,
			       VTE_LINE_WIDTH);
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
			       column_width,
			       VTE_LINE_WIDTH);
		break;
	case 120: /* x */
		XFillRectangle(display,
			       drawable,
			       gc,
			       xcenter,
			       y,
			       VTE_LINE_WIDTH,
			       row_height);
		break;
	case 121: /* y */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* <= */
		XDrawLine(display, drawable, gc,
			  xright - 1, y,
			  x, (y + ycenter) / 2);
		XDrawLine(display, drawable, gc,
			  x, (y + ycenter) / 2,
			  xright - 1, ycenter);
		XDrawLine(display, drawable, gc,
			  x, ycenter,
			  xright - 1, (ycenter + ybottom) / 2);
		break;
	case 122: /* z */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* >= */
		XDrawLine(display, drawable, gc,
			  x, y,
			  xright - 1, (y + ycenter) / 2);
		XDrawLine(display, drawable, gc,
			  xright - 1, (y + ycenter) / 2,
			  x, ycenter);
		XDrawLine(display, drawable, gc,
			  xright - 1, ycenter,
			  x, (ycenter + ybottom) / 2);
		break;
	case 123: /* pi */
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		XDrawLine(display, drawable, gc,
			  (x + xcenter) / 2 - 1,
			  (y + ycenter) / 2,
			  (xright + xcenter) / 2 + 1,
			  (y + ycenter) / 2);
		XDrawLine(display, drawable, gc,
			  (x + xcenter) / 2,
			  (y + ycenter) / 2,
			  (x + xcenter) / 2,
			  (ybottom + ycenter) / 2);
		XDrawLine(display, drawable, gc,
			  (xright + xcenter) / 2,
			  (y + ycenter) / 2,
			  (xright + xcenter) / 2,
			  (ybottom + ycenter) / 2);
		break;
	case 124:
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* != */
		XDrawLine(display, drawable, gc,
			  (x + xcenter) / 2 - 1, ycenter,
			  (xright + xcenter) / 2 + 1, ycenter);
		XDrawLine(display, drawable, gc,
			  (x + xcenter) / 2 - 1,
			  (ybottom + ycenter) / 2,
			  (xright + xcenter) / 2 + 1,
			  (ybottom + ycenter) / 2);
		XDrawLine(display, drawable, gc,
			  xright - 1, y + 1,
			  x + 1, ybottom - 1);
		break;
	case 125:
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* British pound.  An "L" with a hyphen. */
		XDrawLine(display, drawable, gc,
			  (x + xcenter) / 2,
			  (y + ycenter) / 2,
			  (x + xcenter) / 2,
			  (ycenter + ybottom) / 2);
		XDrawLine(display, drawable, gc,
			  (x + xcenter) / 2,
			  (ycenter + ybottom) / 2,
			  (xcenter + xright) / 2,
			  (ycenter + ybottom) / 2);
		XDrawLine(display, drawable, gc,
			  x, ycenter,
			  xcenter + 1, ycenter);
		break;
	case 126:
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* short hyphen? */
		XDrawLine(display, drawable, gc,
			  xcenter - 1, ycenter,
			  xcenter + 1, ycenter);
		break;
	case 127:
		xcenter--;
		ycenter--;
		xright--;
		ybottom--;
		/* A "delete" symbol I saw somewhere. */
		XDrawLine(display, drawable, gc,
			  x, ycenter,
			  xcenter, y);
		XDrawLine(display, drawable, gc,
			  xcenter, y,
			  xright - 1, ycenter);
		XDrawLine(display, drawable, gc,
			  xright - 1, ycenter,
			  xright - 1, ybottom - 1);
		XDrawLine(display, drawable, gc,
			  xright - 1, ybottom - 1,
			  x, ybottom - 1);
		XDrawLine(display, drawable, gc,
			  x, ybottom - 1,
			  x, ycenter);
		break;
	default:
		break;
	}
}

/* Calculate how much padding needs to be placed to either side of a character
 * to make sure it renders into the center of its cell. */
static void
vte_terminal_compute_padding(VteTerminal *terminal, Display *display,
			     gunichar c)
{
	int columns, pad = 0, rpad = 0, width;
	char utf8_buf[VTE_UTF8_BPC];
	GtkWidget *widget;
	PangoLayout *layout;
	PangoRectangle pink, plogical;
	XRectangle xink, xlogical;
	wchar_t wc;
#ifdef HAVE_XFT
	XGlyphInfo extents;
#endif
	/* Check how many columns this character uses up. */
	columns = g_unichar_iswide(c) ? 2 : 1;
	switch (terminal->pvt->render_method) {
#ifdef HAVE_XFT
	/* Ask Xft. */
	case VteRenderXft2:
	case VteRenderXft1:
		XftTextExtents32(display, terminal->pvt->ftfont,
				 &c, 1, &extents);
		pad = ((columns * terminal->char_width) - extents.xOff) / 2;
		rpad = ((columns * terminal->char_width) - extents.xOff) - pad;
		break;
#endif
	/* Ask Pango. */
	case VteRenderPango:
	case VteRenderPangoX:
		widget = GTK_WIDGET(terminal);
		layout = pango_layout_new(terminal->pvt->pcontext);
		pango_layout_set_font_description(layout,
						  terminal->pvt->fontdesc);
		pango_layout_set_text(layout, utf8_buf,
				      g_unichar_to_utf8(c, utf8_buf));
		pango_layout_get_extents(layout, &pink, &plogical);
		width = howmany(plogical.width, PANGO_SCALE);
		g_object_unref(G_OBJECT(layout));
		pad = ((columns * terminal->char_width) - width) / 2;
		rpad = ((columns * terminal->char_width) - width) - pad;
		break;
	case VteRenderXlib:
		/* Ask Xlib. */
		wc = vte_wc_from_unichar(terminal, c);
		XwcTextExtents(terminal->pvt->fontset, &wc, 1,
			       &xink, &xlogical);
		pad = ((columns * terminal->char_width) - xlogical.width) / 2;
		rpad = ((columns * terminal->char_width) - xlogical.width) - pad;
	}
	/* Sanitize possibly-negative padding values and save them. */
	pad = MAX(0, pad);
	if (pad == 0) {
		pad = -1;
	}
	g_tree_insert(terminal->pvt->fontpaddingl,
		      GINT_TO_POINTER(c), GINT_TO_POINTER(pad));
	rpad = MAX(0, rpad);
	if (rpad == 0) {
		rpad = -1;
	}
	g_tree_insert(terminal->pvt->fontpaddingr,
		      GINT_TO_POINTER(c), GINT_TO_POINTER(rpad));
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC) && 0) {
		fprintf(stderr, "Padding for %ld is %d/%d/%ld.\n",
			(long) c, pad, rpad, terminal->char_width);
	}
#endif
}
static int
vte_terminal_get_char_padding(VteTerminal *terminal, Display *display,
			      gunichar c)
{
	int pad;
	/* NUL gets no padding. */
	if (c == 0) {
		return 0;
	}
	/* Look up the cached value if there is one. */
	pad = GPOINTER_TO_INT(g_tree_lookup(terminal->pvt->fontpaddingl,
					    GINT_TO_POINTER(c)));
	/* If it's non-zero, clamp it. */
	if (pad != 0) {
		return CLAMP(pad, 0, terminal->char_width);
	}
	/* Compute the padding and try again. */
	vte_terminal_compute_padding(terminal, display, c);
	pad = GPOINTER_TO_INT(g_tree_lookup(terminal->pvt->fontpaddingl,
					    GINT_TO_POINTER(c)));
	return CLAMP(pad, 0, terminal->char_width);
}
static int
vte_terminal_get_char_padding_right(VteTerminal *terminal, Display *display,
				    gunichar c)
{
	int pad;
	/* NUL gets no padding. */
	if (c == 0) {
		return 0;
	}
	/* Look up the cached value if there is one. */
	pad = GPOINTER_TO_INT(g_tree_lookup(terminal->pvt->fontpaddingr,
					    GINT_TO_POINTER(c)));
	/* If it's non-zero, clamp it. */
	if (pad != 0) {
		return CLAMP(pad, 0, terminal->char_width);
	}
	/* Compute the padding and try again. */
	vte_terminal_compute_padding(terminal, display, c);
	pad = GPOINTER_TO_INT(g_tree_lookup(terminal->pvt->fontpaddingr,
					    GINT_TO_POINTER(c)));
	return CLAMP(pad, 0, terminal->char_width);
}

/* Draw a string of characters with similar attributes. */
static void
vte_terminal_draw_cells(VteTerminal *terminal,
			struct vte_draw_item *items, gssize n,
			gint fore, gint back, gboolean draw_default_bg,
			gboolean bold, gboolean underline, gboolean strikethrough,
			gboolean hilite, gboolean boxed,
			gint x, gint y, gint x_offs, gint y_offs,
			gint ascent, gboolean monospaced,
			gint column_width, gint row_height,
			Display *display,
			GdkDrawable *gdrawable,
			Drawable drawable,
			Colormap colormap,
			Visual *visual,
			GdkGC *ggc,
			GC gc,
#ifdef HAVE_XFT
			XftDraw *ftdraw,
#endif
			PangoLayout *layout)
{
	int i;
	gint columns = 0;
	struct vte_palette_entry *fg, *bg;
	GdkColor color;

#ifdef HAVE_XFT2
	XftCharSpec *ftcharspecs;
#endif
#ifdef HAVE_XFT
	XftChar32 ftchar;
#endif
	gunichar c;
	char utf8_buf[VTE_UTF8_BPC * VTE_DRAW_MAX_LENGTH];

	wchar_t wc;
	XwcTextItem textitem;

	bold = bold && terminal->pvt->allow_bold;
	fg = &terminal->pvt->palette[fore];
	bg = &terminal->pvt->palette[back];
	x += VTE_PAD_WIDTH;
	y += VTE_PAD_WIDTH;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC) && 0) {
		fprintf(stderr, "Rendering");
		for (i = 0; i < n; i++) {
			fprintf(stderr, " (%ld,%ld)",
				(long) items[i].c,
				(long) items[i].columns);
			g_assert(items[i].columns > 0);
		}
		fprintf(stderr, ".\n");
	}
#endif

	switch (terminal->pvt->render_method) {
#ifdef HAVE_XFT
	case VteRenderXft2:
#ifdef HAVE_XFT2
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC) && 0) {
			fprintf(stderr, "Rendering with Xft2.\n");
		}
#endif
		/* Set up the draw request. */
		ftcharspecs = terminal->pvt->xft_textitem;
		g_assert(n <= G_N_ELEMENTS(terminal->pvt->xft_textitem));
		for (i = columns = 0; i < n; i++) {
			c = items[i].c ? items[i].c : ' ';
			ftcharspecs[i].ucs4 = vte_terminal_xft_remap_char(display,
									  terminal->pvt->ftfont,
									  c);
			ftcharspecs[i].x = x + (columns * column_width) +
					   items[i].xpad;
			ftcharspecs[i].y = y + ascent;
			columns += items[i].columns;
		}
		if (ftdraw != NULL) {
			/* Draw the background rectangle. */
			if ((back != VTE_DEF_BG) || draw_default_bg) {
				XftDrawRect(ftdraw, &bg->ftcolor, x, y,
					    columns * column_width, row_height);
			}
			/* Draw the text. */
			XftDrawCharSpec(ftdraw, &fg->ftcolor,
					terminal->pvt->ftfont,
					ftcharspecs, n);
			/* Bold overdraw. */
			if (bold) {
				for (i = 0; i < n; i++) {
					ftcharspecs[i].x++;
				}
				XftDrawCharSpec(ftdraw, &fg->ftcolor,
						terminal->pvt->ftfont,
						ftcharspecs, n);
			}
		}
		break;
#endif
	case VteRenderXft1:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC) && 0) {
			fprintf(stderr, "Rendering with Xft1.\n");
		}
#endif
		/* Set up the draw request -- calculate the total width. */
		for (i = columns = 0; i < n; i++) {
			c = items[i].c ? items[i].c : ' ';
			columns += items[i].columns;
		}
		/* Draw the background rectangle. */
		if ((back != VTE_DEF_BG) || draw_default_bg) {
			XftDrawRect(ftdraw, &bg->ftcolor, x, y,
				    columns * column_width, row_height);
		}
		/* Draw the text. */
		for (i = columns = 0; i < n; i++) {
			c = items[i].c ? items[i].c : ' ';
			ftchar = vte_terminal_xft_remap_char(display,
							     terminal->pvt->ftfont,
							     c);
			XftDrawString32(ftdraw, &fg->ftcolor,
					terminal->pvt->ftfont,
					x + (columns * column_width) +
					items[i].xpad,
					y + ascent,
					&ftchar, 1);
			if (bold) {
				XftDrawString32(ftdraw, &fg->ftcolor,
						terminal->pvt->ftfont,
						x + (columns * column_width) +
						items[i].xpad + 1,
						y + ascent,
						&ftchar, 1);
			}
			columns += items[i].columns;
		}
		break;
#endif
	case VteRenderPango:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC) && 0) {
			fprintf(stderr, "Rendering with Pango.\n");
		}
#endif
		/* Draw the background. */
		if ((back != VTE_DEF_BG) || draw_default_bg) {
			for (i = columns = 0; i < n; i++) {
				c = items[i].c ? items[i].c : ' ';
				columns += items[i].columns;
			}
			color.red = terminal->pvt->palette[back].red;
			color.blue = terminal->pvt->palette[back].blue;
			color.green = terminal->pvt->palette[back].green;
			color.pixel = terminal->pvt->palette[back].pixel;
			gdk_gc_set_foreground(ggc, &color);
			gdk_draw_rectangle(gdrawable,
					   ggc,
					   TRUE,
					   x + x_offs, y + y_offs,
					   columns * column_width, row_height);
		}
		/* Draw the text in a monospaced manner. */
		color.red = terminal->pvt->palette[fore].red;
		color.blue = terminal->pvt->palette[fore].blue;
		color.green = terminal->pvt->palette[fore].green;
		color.pixel = terminal->pvt->palette[fore].pixel;
		gdk_gc_set_foreground(ggc, &color);
		for (i = columns = 0; (layout != NULL) && (i < n); i++) {
			c = items[i].c ? items[i].c : ' ';
			pango_layout_set_text(layout, utf8_buf,
					      g_unichar_to_utf8(c, utf8_buf));
			gdk_draw_layout(gdrawable, ggc,
					x + (columns * column_width) +
					items[i].xpad + x_offs,
					y + y_offs,
					layout);
			if (bold) {
				gdk_draw_layout(gdrawable, ggc,
						x + (columns * column_width) +
						items[i].xpad + x_offs + 1,
						y + y_offs,
						layout);
			}
			columns += items[i].columns;
		}
		break;
	case VteRenderPangoX:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC) && 0) {
			fprintf(stderr, "Rendering with PangoX.\n");
		}
#endif
		/* Draw the background. */
		if ((back != VTE_DEF_BG) || draw_default_bg) {
			for (i = columns = 0; i < n; i++) {
				c = items[i].c ? items[i].c : ' ';
				columns += items[i].columns;
			}
			XSetForeground(display, gc, bg->pixel);
			XFillRectangle(display, drawable, gc,
				       x, y,
				       columns * column_width, row_height);
		}
		/* Draw the text in a monospaced manner. */
		color.red = terminal->pvt->palette[fore].red;
		color.blue = terminal->pvt->palette[fore].blue;
		color.green = terminal->pvt->palette[fore].green;
		color.pixel = terminal->pvt->palette[fore].pixel;
		gdk_gc_set_foreground(ggc, &color);
		for (i = columns = 0; (layout != NULL) && (i < n); i++) {
			c = items[i].c ? items[i].c : ' ';
			pango_layout_set_text(layout, utf8_buf,
					      g_unichar_to_utf8(c, utf8_buf));
			XSetForeground(display, gc, fg->pixel);
			pango_x_render_layout(display,
					      drawable,
					      gc,
					      layout,
					      x + (columns * column_width) +
					      items[i].xpad,
					      y);
			if (bold) {
				pango_x_render_layout(display,
						      drawable,
						      gc,
						      layout,
						      x +
						      (columns * column_width) +
						      items[i].xpad + 1,
						      y);
			}
			columns += items[i].columns;
		}
		break;
	case VteRenderXlib:
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC) && 0) {
			fprintf(stderr, "Rendering with Xlib.\n");
		}
#endif
		/* Draw the background. */
		if ((back != VTE_DEF_BG) || draw_default_bg) {
			for (i = columns = 0; i < n; i++) {
				c = items[i].c ? items[i].c : ' ';
				columns += items[i].columns;
			}
			XSetForeground(display, gc, bg->pixel);
			XFillRectangle(display, drawable, gc,
				       x, y,
				       columns * column_width, row_height);
		}
		/* Create a separate draw item for each character. */
		XSetForeground(display, gc, fg->pixel);
		textitem.nchars = 1;
		textitem.font_set = terminal->pvt->fontset;
		for (i = columns = 0; i < n; i++) {
			c = items[i].c ? items[i].c : ' ';
			wc = vte_wc_from_unichar(terminal, c);
			textitem.chars = &wc;
			textitem.delta = items[i].xpad;
			XwcDrawText(display, drawable, gc,
				    x + (columns * column_width) +
				    items[i].xpad,
				    y + ascent, &textitem, 1);
			if (bold) {
				XwcDrawText(display, drawable, gc,
					    x + (columns * column_width) +
					    items[i].xpad + 1,
					    y + ascent, &textitem, 1);
			}
			columns += items[i].columns;
		}
	}

	/* Draw whatever SFX are required. */
	if (underline) {
		XSetForeground(display, gc, fg->pixel);
		XDrawLine(display, drawable, gc,
			  x, y + ascent+2,
			  x + (columns * column_width) - 1, y + ascent+2);
	}
	if (strikethrough) {
		XSetForeground(display, gc, fg->pixel);
		XDrawLine(display, drawable, gc,
			  x, y + (ascent+row_height)/4,
			  x + (columns * column_width) - 1, y + (ascent+row_height)/4);
	}
	if (hilite) {
		XSetForeground(display, gc, fg->pixel);
		XDrawLine(display, drawable, gc,
			  x, y + row_height - 1,
			  x + (columns * column_width) - 1, y + row_height - 1);
	}
	if (boxed) {
		XSetForeground(display, gc, fg->pixel);
		XDrawRectangle(display, drawable, gc,
			       x, y,
			       MAX(0, (columns * column_width) - 1),
			       MAX(0, row_height - 1));
	}
}

static gboolean
vte_terminal_get_blink_state(VteTerminal *terminal)
{
	struct timezone tz;
	struct timeval tv;
	gint blink_cycle = 1000;
	GtkSettings *settings;
	time_t daytime;
	gboolean blink;
	GtkWidget *widget;

	/* Determine if blinking text should be shown. */
	if (terminal->pvt->cursor_blinks) {
		if (gettimeofday(&tv, &tz) == 0) {
			widget = GTK_WIDGET(terminal);
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
	if (terminal->pvt->cursor_force_fg > 0) {
		terminal->pvt->cursor_force_fg--;
		blink = TRUE;
	}
	return blink;
}

/* Paint the contents of a given row at the given location.  Take advantage
 * of multiple-draw APIs by finding runs of characters with identical
 * attributes and bundling them together. */
static void
vte_terminal_draw_row(VteTerminal *terminal,
		      VteScreen *screen,
		      gint row,
		      gint column, gint column_count,
		      gint x, gint y,
		      gint x_offs, gint y_offs,
		      gint ascent, gboolean monospaced,
		      gint column_width, gint row_height,
		      Display *display,
		      GdkDrawable *gdrawable,
		      Drawable drawable,
		      Colormap colormap,
		      Visual *visual,
		      GdkGC *ggc,
		      GC gc,
#ifdef HAVE_XFT
		      XftDraw *ftdraw,
#endif
		      PangoLayout *layout)
{
	GArray *items;
	int i, j, fore, nfore, back, nback;
	gboolean underline, nunderline, bold, nbold, hilite, nhilite, reverse,
		 strikethrough, nstrikethrough;
	struct vte_draw_item item;
	struct vte_charcell *cell;

	/* Allocate an array to hold draw requests. */
	items = g_array_new(FALSE, FALSE, sizeof(struct vte_draw_item));

	/* Back up in case this is a multicolumn character, making the drawing
	 * area a little wider. */
	i = column;
	cell = vte_terminal_find_charcell(terminal, i, row);
	if ((cell != NULL) && (cell->fragment) && (i > 0)) {
		cell = vte_terminal_find_charcell(terminal, i - 1, row);
		column--;
		column_count++;
	}

	/* Walk the line. */
	while (i < column + column_count) {
		/* Get the character cell's contents. */
		cell = vte_terminal_find_charcell(terminal, i, row);
		/* Find the colors for this cell. */
		reverse = vte_cell_is_selected(terminal, i, row, NULL) ^
			  terminal->pvt->screen->reverse_mode;
		vte_terminal_determine_colors(terminal, cell, reverse,
					      &fore, &back);
		underline = (cell != NULL) ? (cell->underline != 0) : FALSE;
		strikethrough = (cell != NULL) ? (cell->strikethrough != 0) : FALSE;
		bold = (cell != NULL) ? (cell->bold != 0) : FALSE;
		if ((cell != NULL) && (terminal->pvt->match_contents != NULL)) {
			hilite = vte_cell_is_between(i, row,
						     terminal->pvt->match_start.column,
						     terminal->pvt->match_start.row,
						     terminal->pvt->match_end.column,
						     terminal->pvt->match_end.row,
						     TRUE);
		} else {
			hilite = FALSE;
		}

		/* If this is a graphics character, draw it locally. */
		if ((cell != NULL) &&
		    (cell->alternate || vte_unichar_isgraphic(cell->c))) {
			item.c = cell ? cell->c : ' ';
			item.columns = cell ? cell->columns : 1;
			vte_terminal_draw_graphic(terminal, cell->c, fore, back,
						  FALSE,
						  x +
						  ((i - column) * column_width),
						  y,
						  item.columns * column_width,
						  row_height,
						  display,
						  gdrawable,
						  drawable,
						  ggc,
						  gc);
			i += item.columns;
			continue;
		}

		/* Add this cell to the draw list. */
		item.c = cell ? cell->c : ' ';
		item.columns = cell ? cell->columns : 1;
		/* Special case certain whitespace characters which the font
		 * probably can't render correctly, if at all. */
		switch (item.c) {
		case '\0':
		case '\t':
			item.c = ' ';
			break;
		default:
			break;
		}
		/* Compute the left padding necessary to draw this character
		 * in the approximate middle of its columns. */
		if (monospaced) {
			item.xpad = 0;
		} else {
			item.xpad = vte_terminal_get_char_padding(terminal,
								  display,
								  item.c);
		}
		g_array_append_val(items, item);

		/* Now find out how many cells have the same attributes. */
		for (j = i + 1;
		     (j < column + column_count) &&
		     (j - i < VTE_DRAW_MAX_LENGTH);
		     j++) {
			/* Retrieve the cell. */
			cell = vte_terminal_find_charcell(terminal, j, row);
			/* Resolve attributes to colors where possible and
			 * compare visual attributes to the first character
			 * in this chunk. */
			reverse = vte_cell_is_selected(terminal, j, row, NULL) ^
				  terminal->pvt->screen->reverse_mode;
			vte_terminal_determine_colors(terminal, cell, reverse,
						      &nfore, &nback);
			if ((nfore != fore) || (nback != back)) {
				break;
			}
			nbold = (cell != NULL) ?
				(cell->bold != 0) :
			        FALSE;
			if (nbold != bold) {
				break;
			}
			/* Graphic characters must be drawn individually. */
			if ((cell != NULL) && (cell->alternate)) {
				break;
			}
			if ((cell != NULL) && vte_unichar_isgraphic(cell->c)) {
				break;
			}
			/* Don't render fragments of multicolumn characters
			 * which have the same attributes as the initial
			 * portions. */
			if ((cell != NULL) && (cell->fragment)) {
				continue;
			}
			/* Break up underlined/not-underlined text. */
			nunderline = (cell != NULL) ?
				     (cell->underline != 0) :
				     FALSE;
			if (nunderline != underline) {
				break;
			}
			nstrikethrough = (cell != NULL) ?
			      		 (cell->strikethrough != 0) :
					 FALSE;
			if (nstrikethrough != strikethrough) {
				break;
			}
			/* Break up matched/not-matched text. */
			if ((cell != NULL) &&
			    (terminal->pvt->match_contents != NULL)) {
				nhilite = vte_cell_is_between(j, row,
							      terminal->pvt->match_start.column,
							      terminal->pvt->match_start.row,
							      terminal->pvt->match_end.column,
							      terminal->pvt->match_end.row,
							      TRUE);
			} else {
				nhilite = FALSE;
			}
			if (nhilite != hilite) {
				break;
			}
			/* Special case certain non-printing characters. */
			if ((cell != NULL) && (cell->c == '\t')) {
				break;
			}
			/* Add this cell to the draw list. */
			item.c = cell ? (cell->c ? cell->c : ' ') : ' ';
			item.columns = cell ? cell->columns : 1;
			if (monospaced) {
				item.xpad = 0;
			} else {
				item.xpad = vte_terminal_get_char_padding(terminal,
									  display,
									  item.c);
			}
			g_array_append_val(items, item);
		}
		/* Draw the cells. */
		vte_terminal_draw_cells(terminal,
					(struct vte_draw_item*) items->data,
					items->len,
					fore, back, FALSE,
					bold, underline, strikethrough, hilite, FALSE,
					x + ((i - column) * column_width),
					y,
					x_offs,
					y_offs,
					ascent, monospaced,
					column_width, row_height,
					display, gdrawable, drawable,
					colormap, visual, ggc, gc,
#ifdef HAVE_XFT
					ftdraw,
#endif
					layout);
		g_array_set_size(items, 0);
		/* We'll need to continue at the first cell which didn't
		 * match the first one in this set. */
		i = j;
	}

	/* Clean up. */
	g_array_free(items, TRUE);
}

/* Draw the widget. */
static void
vte_terminal_paint(GtkWidget *widget, GdkRectangle *area)
{
	VteTerminal *terminal = NULL;
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
	struct vte_charcell *cell;
	struct vte_draw_item item, *items;
	int row, drow, col, row_stop, col_stop, x_offs = 0, y_offs = 0, columns;
	char *preedit;
	long width, height, ascent, descent, delta;
	int i, len, fore, back;
	gboolean blink, bold, underline, hilite, monospaced, strikethrough;
#ifdef HAVE_XFT
	XftDraw *ftdraw = NULL;
#endif

	/* Make a few sanity checks. */
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VTE_IS_TERMINAL(widget));
	g_return_if_fail(area != NULL);
	terminal = VTE_TERMINAL(widget);

	/* Get going. */
	screen = terminal->pvt->screen;

	/* Get the X11 structures we need for the drawing area. */
	gdk_window_get_internal_paint_info(widget->window, &gdrawable,
					   &x_offs, &y_offs);
	display = gdk_x11_drawable_get_xdisplay(gdrawable);
	drawable = gdk_x11_drawable_get_xid(gdrawable);
	gcolormap = gdk_drawable_get_colormap(widget->window);
	colormap = gdk_x11_colormap_get_xcolormap(gcolormap);
	gvisual = gtk_widget_get_visual(widget);
	visual = gdk_x11_visual_get_xvisual(gvisual);

	/* Reset the GDK drawable to the buffered version. */
	gdrawable = widget->window;

	switch (terminal->pvt->render_method) {
#ifdef HAVE_XFT
	case VteRenderXft2:
	case VteRenderXft1:
		ftdraw = XftDrawCreate(display, drawable, visual, colormap);
		if (ftdraw == NULL) {
			g_warning(_("Error allocating draw, disabling Xft."));
			return;
		}
		break;
#endif
	case VteRenderPango:
	case VteRenderPangoX:
		if (terminal->pvt->pcontext == NULL) {
			g_warning(_("Error allocating context, "
				    "disabling Pango."));
			return;
		}
		layout = pango_layout_new(terminal->pvt->pcontext);
		if (layout == NULL) {
			g_warning(_("Error allocating layout, "
				    "disabling Pango."));
			return;
		}
		pango_layout_set_font_description(layout,
						  terminal->pvt->fontdesc);
		break;
	case VteRenderXlib:
		/* do nothing */
		break;
	}

	gc = XCreateGC(display, drawable, 0, NULL);
	ggc = gdk_gc_new(gdrawable);

	/* Keep local copies of rendering information. */
	width = terminal->char_width;
	height = terminal->char_height;
	ascent = terminal->char_ascent;
	descent = terminal->char_descent;
	delta = screen->scroll_delta;

	monospaced =
		(vte_terminal_get_char_padding(terminal, display,
					       VTE_REPRESENTATIVE_WIDER_CHARACTER) == 0) &&
		(vte_terminal_get_char_padding(terminal, display,
					       VTE_REPRESENTATIVE_NARROWER_CHARACTER) == 0);

	/* Now we're ready to draw the text.  Iterate over the rows we
	 * need to draw. */
	row = (area->y - VTE_PAD_WIDTH) / height;
	row_stop = ((area->y - VTE_PAD_WIDTH) + area->height + height - 1) / height;
	while (row < row_stop) {
		col = (area->x - VTE_PAD_WIDTH) / width;
		col_stop = MIN(howmany((area->x - VTE_PAD_WIDTH) + area->width,
				       width),
			       terminal->column_count);
		vte_terminal_draw_row(terminal,
				      screen,
				      row + delta,
				      col,
				      col_stop - col,
				      col * width - x_offs,
				      row * height - y_offs,
				      x_offs, y_offs,
				      ascent, monospaced,
				      terminal->char_width,
				      terminal->char_height,
				      display, gdrawable, drawable,
				      colormap, visual, ggc, gc,
#ifdef HAVE_XFT
				      ftdraw,
#endif
				      layout);
		row++;
	}

	if (terminal->pvt->cursor_visible &&
	    (CLAMP(screen->cursor_current.col, 0, terminal->column_count - 1) ==
	     screen->cursor_current.col)) {
		/* Get the location of the cursor. */
		col = screen->cursor_current.col;
		if (terminal->pvt->im_preedit != NULL) {
			preedit = terminal->pvt->im_preedit;
			for (i = 0; i < terminal->pvt->im_preedit_cursor; i++) {
				col += vte_unichar_width(terminal, g_utf8_get_char(preedit));
				preedit = g_utf8_next_char(preedit);
			}
		}
		drow = screen->cursor_current.row;
		row = drow - delta;

		/* Find the character "under" the cursor. */
		cell = vte_terminal_find_charcell(terminal, col, drow);
		while ((cell != NULL) && (cell->fragment) && (col >= 0)) {
			col--;
			cell = vte_terminal_find_charcell(terminal, col, drow);
		}

		/* Draw the cursor. */
		if (GTK_WIDGET_HAS_FOCUS(GTK_WIDGET(terminal))) {
			blink = vte_terminal_get_blink_state(terminal) ^
				terminal->pvt->screen->reverse_mode;
			vte_terminal_determine_colors(terminal, cell, blink,
						      &fore, &back);
			if ((cell != NULL) &&
			    (cell->alternate || vte_unichar_isgraphic(cell->c))) {
				vte_terminal_draw_graphic(terminal,
							  cell->c, fore, back,
							  TRUE,
							  col * width - x_offs,
							  row * height - y_offs,
							  terminal->char_width,
							  terminal->char_height,
							  display,
							  gdrawable,
							  drawable,
							  ggc,
							  gc);
			} else {
				item.c = cell ? cell->c : ' ';
				item.columns = cell ? cell->columns : 1;
				item.xpad = vte_terminal_get_char_padding(terminal,
									  display,
									  item.c);
				underline = cell ? (cell->underline != 0) : FALSE;
				strikethrough= cell ? (cell->strikethrough!= 0) : FALSE;
				bold = cell ? (cell->bold != 0) : FALSE;
				hilite = FALSE;
				vte_terminal_draw_cells(terminal,
							&item, 1,
							fore, back, TRUE, bold,
							underline, strikethrough, hilite, FALSE,
							col * width - x_offs,
							row * height - y_offs,
							x_offs,
							y_offs,
							ascent, monospaced,
							terminal->char_width,
							terminal->char_height,
							display, gdrawable, drawable,
							colormap, visual, ggc, gc,
#ifdef HAVE_XFT
							ftdraw,
#endif
							layout);
			}
		} else {
			/* Determine how wide the cursor is. */
			columns = 1;
			if ((cell != NULL) && (cell->columns > 1)) {
				columns = cell->columns;
			}
			/* Draw it as a hollow rectangle. */
			fore = cell ? cell->fore : VTE_DEF_FG;
			XSetForeground(display, gc,
				       terminal->pvt->palette[fore].pixel);
			XDrawRectangle(display, drawable, gc,
				       col * width - x_offs + VTE_PAD_WIDTH,
				       row * height - y_offs + VTE_PAD_WIDTH,
				       columns * width - 1,
				       height - 1);
		}
	}

	/* Draw the pre-edit string (if one exists) over the cursor. */
	if (terminal->pvt->im_preedit) {
		drow = screen->cursor_current.row;
		row = screen->cursor_current.row - delta;

		/* If the pre-edit string won't fit on the screen, skip initial
		 * characters until it does. */
		preedit = terminal->pvt->im_preedit;
		len = g_utf8_strlen(preedit, -1);
		col = screen->cursor_current.col;
		while (((col + len) > terminal->column_count) && (len > 0)) {
			preedit = g_utf8_next_char(preedit);
			len = g_utf8_strlen(preedit, -1);
		}
		col = screen->cursor_current.col;

		/* Draw the preedit string, boxed. */
		if (len > 0) {
			items = g_malloc(sizeof(struct vte_draw_item) * len);
			for (i = columns = 0; i < len; i++) {
				items[i].c = g_utf8_get_char(preedit);
				items[i].xpad = vte_terminal_get_char_padding(terminal,
									      display,
									      item.c);
				items[i].columns = g_unichar_iswide(items[i].c) ? 2 : 1;
				columns += items[i].columns;
				preedit = g_utf8_next_char(preedit);
			}
			fore = screen->defaults.fore;
			back = screen->defaults.back;
			XSetForeground(display, gc,
				       terminal->pvt->palette[back].pixel);
			XFillRectangle(display, drawable, gc,
				       col * width - x_offs + VTE_PAD_WIDTH,
				       row * height - y_offs + VTE_PAD_WIDTH,
				       columns * terminal->char_width,
				       terminal->char_height);
			vte_terminal_draw_cells(terminal,
						items, len,
						screen->defaults.fore,
						screen->defaults.back,
						FALSE, FALSE, FALSE,
						FALSE, FALSE, TRUE,
						col * width - x_offs,
						row * height - y_offs,
						x_offs,
						y_offs,
						ascent, monospaced,
						terminal->char_width,
						terminal->char_height,
						display, gdrawable, drawable,
						colormap, visual, ggc, gc,
#ifdef HAVE_XFT
						ftdraw,
#endif
						layout);
			g_free(items);
		}
	}

	/* Done with various structures. */
#ifdef HAVE_XFT
	if (ftdraw != NULL) {
		XftDrawDestroy(ftdraw);
	}
#endif
	if (layout != NULL) {
		g_object_unref(G_OBJECT(layout));
	}
	g_object_unref(G_OBJECT(ggc));
	XFreeGC(display, gc);
}

/* Handle an expose event by painting the exposed area. */
static gint
vte_terminal_expose(GtkWidget *widget, GdkEventExpose *event)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(widget), 0);
	if (event->window == widget->window) {
		if (GTK_WIDGET_REALIZED(widget) &&
		    GTK_WIDGET_VISIBLE(widget) &&
		    GTK_WIDGET_MAPPED(widget)) {
			vte_terminal_paint(widget, &event->area);
		}
	} else {
		g_assert_not_reached();
	}
	return FALSE;
}

/* Handle a scroll event. */
static gboolean
vte_terminal_scroll(GtkWidget *widget, GdkEventScroll *event)
{
	GtkAdjustment *adj;
	VteTerminal *terminal;
	gdouble new_value;
	GdkModifierType modifiers;
	int button;

	g_return_val_if_fail(VTE_IS_TERMINAL(widget), FALSE);
	terminal = VTE_TERMINAL(widget);

	/* Read the modifiers. */
	if (gdk_event_get_state((GdkEvent*)event, &modifiers) == FALSE) {
		modifiers = 0;
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
		switch (event->direction) {
		case GDK_SCROLL_UP:
			fprintf(stderr, "Scroll up.\n");
			break;
		case GDK_SCROLL_DOWN:
			fprintf(stderr, "Scroll down.\n");
			break;
		default:
			break;
		}
	}
#endif

	/* If we're running a mouse-aware application, map the scroll event
	 * to a button press on buttons four and five. */
	if (terminal->pvt->mouse_send_xy_on_click ||
	    terminal->pvt->mouse_send_xy_on_button ||
	    terminal->pvt->mouse_hilite_tracking ||
	    terminal->pvt->mouse_cell_motion_tracking ||
	    terminal->pvt->mouse_all_motion_tracking) {
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
								event->x - VTE_PAD_WIDTH,
								event->y - VTE_PAD_WIDTH,
								modifiers);
			return TRUE;
		}
	}

	/* Perform a history scroll. */
	adj = (VTE_TERMINAL(widget))->adjustment;

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

	new_value = CLAMP(new_value, adj->lower, adj->upper - adj->page_size);
	gtk_adjustment_set_value(adj, new_value);

	return TRUE;
}

/* Create a new accessible object associated with ourselves, and return
 * it to the caller. */
static AtkObject *
vte_terminal_get_accessible(GtkWidget *widget)
{
	AtkObject *access;
	VteTerminal *terminal;
	g_return_val_if_fail(VTE_IS_TERMINAL(widget), NULL);
	terminal = VTE_TERMINAL(widget);
	if (terminal->pvt->accessible != NULL) {
		access = terminal->pvt->accessible;
	} else {
		access = vte_terminal_accessible_new(terminal);
		terminal->pvt->accessible = access;
	}
	return access;
}

/* Initialize methods. */
static void
vte_terminal_class_init(VteTerminalClass *klass, gconstpointer data)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	bindtextdomain(PACKAGE, LOCALEDIR);

	gobject_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	/* Override some of the default handlers. */
	gobject_class->finalize = vte_terminal_finalize;
	widget_class->realize = vte_terminal_realize;
	widget_class->scroll_event = vte_terminal_scroll;
	widget_class->expose_event = vte_terminal_expose;
	widget_class->key_press_event = vte_terminal_key_press;
	widget_class->key_release_event = vte_terminal_key_release;
	widget_class->button_press_event = vte_terminal_button_press;
	widget_class->button_release_event = vte_terminal_button_release;
	widget_class->motion_notify_event = vte_terminal_motion_notify;
	widget_class->focus_in_event = vte_terminal_focus_in;
	widget_class->focus_out_event = vte_terminal_focus_out;
	widget_class->unrealize = vte_terminal_unrealize;
	widget_class->size_request = vte_terminal_size_request;
	widget_class->size_allocate = vte_terminal_size_allocate;
	widget_class->get_accessible = vte_terminal_get_accessible;

	/* Register some signals of our own. */
	klass->eof_signal =
		g_signal_new("eof",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->child_exited_signal =
		g_signal_new("child-exited",
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
	klass->encoding_changed_signal =
		g_signal_new("encoding-changed",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->commit_signal =
		g_signal_new("commit",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__STRING_UINT,
			     G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_UINT);
	klass->emulation_changed_signal =
		g_signal_new("emulation-changed",
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
			     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
	klass->move_window_signal =
		g_signal_new("move-window",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__UINT_UINT,
			     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
	klass->status_line_changed_signal =
		g_signal_new("status-line-changed",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->increase_font_size_signal =
		g_signal_new("increase-font-size",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
	klass->decrease_font_size_signal =
		g_signal_new("decrease-font-size",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     0,
			     NULL,
			     NULL,
			     _vte_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

	/* Try to determine some acceptable encoding names. */
	if (_vte_table_narrow_encoding() == NULL) {
		g_error("Don't know how to read ISO-8859-1 data!");
	}
	if (_vte_table_wide_encoding() == NULL) {
		g_error("Don't know how to read native-endian unicode data!");
	}

#ifdef VTE_DEBUG
	/* Turn on debugging if we were asked to. */
	if (getenv("VTE_DEBUG_FLAGS") != NULL) {
		_vte_debug_parse_string(getenv("VTE_DEBUG_FLAGS"));
	}
#endif
}

GtkType
vte_terminal_erase_binding_get_type(void)
{
	static GtkType terminal_erase_binding_type = 0;
	static GEnumValue values[] = {
		{VTE_ERASE_AUTO, "VTE_ERASE_AUTO", "auto"},
		{VTE_ERASE_ASCII_BACKSPACE, "VTE_ERASE_ASCII_BACKSPACE",
		 "ascii-backspace"},
		{VTE_ERASE_ASCII_DELETE, "VTE_ERASE_ASCII_DELETE",
		 "ascii-delete"},
		{VTE_ERASE_DELETE_SEQUENCE, "VTE_ERASE_DELETE_SEQUENCE",
		 "delete-sequence"},
	};
	if (terminal_erase_binding_type == 0) {
		terminal_erase_binding_type =
			g_enum_register_static("VteTerminalEraseBinding",
					       values);
	}
	return terminal_erase_binding_type;
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

/**
 * vte_terminal_set_audible_bell:
 * @terminal: a #VteTerminal
 * @is_audible: TRUE if the terminal should beep
 *
 * Controls whether or not the terminal will beep when the child outputs the
 * "bl" sequence.
 *
 */
void
vte_terminal_set_audible_bell(VteTerminal *terminal, gboolean is_audible)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->audible_bell = is_audible;
}

/**
 * vte_terminal_get_audible_bell:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not the terminal will beep when the child outputs the
 * "bl" sequence.
 *
 * Returns: TRUE if audible bell is enabled, FALSE if not
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
 * @is_visible: TRUE if the terminal should flash
 *
 * Controls whether or not the terminal will present a visible bell to the
 * user when the child outputs the "bl" sequence.  The terminal
 * will clear itself to the default foreground color and then repaint itself.
 *
 */
void
vte_terminal_set_visible_bell(VteTerminal *terminal, gboolean is_visible)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->visible_bell = is_visible;
}

/**
 * vte_terminal_get_visible_bell:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not the terminal will present a visible bell to the
 * user when the child outputs the "bl" sequence.  The terminal
 * will clear itself to the default foreground color and then repaint itself.
 *
 * Returns: TRUE if visible bell is enabled, FALSE if not
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
 * @allow_bold: TRUE if the terminal should attempt to draw bold text
 *
 * Controls whether or not the terminal will attempt to draw bold text by
 * repainting text with a different offset.
 *
 */
void
vte_terminal_set_allow_bold(VteTerminal *terminal, gboolean allow_bold)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->allow_bold = allow_bold;
}

/**
 * vte_terminal_get_allow_bold:
 * @terminal: a #VteTerminal
 *
 * Checks whether or not the terminal will attempt to draw bold text by
 * repainting text with a one-pixel offset.
 *
 * Returns: TRUE if bolding is enabled, FALSE if not
 */
gboolean
vte_terminal_get_allow_bold(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->allow_bold;
}

/**
 * vte_terminal_set_scroll_on_output:
 * @terminal: a #VteTerminal
 * @scroll: TRUE if the terminal should scroll on output
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
 * @scroll: TRUE if the terminal should scroll on keystrokes
 *
 * Controls whether or not the terminal will forcibly scroll to the bottom of
 * the viewable history when the user presses a key.  Modifier keys do not
 * trigger this behavior.
 *
 */
void
vte_terminal_set_scroll_on_keystroke(VteTerminal *terminal, gboolean scroll)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->scroll_on_keystroke = scroll;
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
	vte_terminal_copy(terminal, GDK_SELECTION_CLIPBOARD);
}

/**
 * vte_terminal_paste_clipboard:
 * @terminal: a #VteTerminal
 *
 * Sends the contents of the #GDK_SELECTION_CLIPBOARD selection to the
 * terminal's child.  If necessary, the data is converted from UTF-8 to the
 * terminal's current encoding.
 *
 */
void
vte_terminal_paste_clipboard(VteTerminal *terminal)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	vte_terminal_paste(terminal, GDK_SELECTION_CLIPBOARD);
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
	g_return_if_fail(GTK_WIDGET_REALIZED(GTK_WIDGET((terminal))));
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
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, "Can not set background image without "
				"window.\n");
		}
#endif
		return;
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Setting up background image.\n");
	}
#endif

	/* Set the default background color. */
	bgcolor.red = terminal->pvt->palette[VTE_DEF_BG].red;
	bgcolor.green = terminal->pvt->palette[VTE_DEF_BG].green;
	bgcolor.blue = terminal->pvt->palette[VTE_DEF_BG].blue;
	bgcolor.pixel = terminal->pvt->palette[VTE_DEF_BG].pixel;
	gdk_window_set_background(widget->window, &bgcolor);

	if (terminal->pvt->bg_transparent) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, "Setting up background transparent.\n");
		}
#endif
		/* If we don't have a root pixmap, try to fetch one regardless
		 * of what the caller told us to do. */
		if (terminal->pvt->bg_transparent_image == NULL) {
			refresh_transparent = TRUE;
		}
		/* If we need a new copy of the desktop, get it. */
		if (refresh_transparent) {
			gint width, height, pwidth, pheight;

#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_MISC)) {
				fprintf(stderr, "Fetching new background "
					"pixmap.\n");
			}
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
			if (prop_data != NULL) {
				g_free(prop_data);
			}
			gdk_flush();
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
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, "Applying new background pixbuf.\n");
		}
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
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, "Desaturating background.\n");
		}
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
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, "Setting final background.\n");
		}
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
	vte_invalidate_all(terminal);
}

/**
 * vte_terminal_set_background_saturation:
 * @terminal: a #VteTerminal
 * @saturation: TRUE if the terminal should fake transparency
 *
 * If a background image has been set using
 * vte_terminal_set_background_image(),
 * vte_terminal_set_background_image_file(), or
 * vte_terminal_set_background_transparent(), the terminal will adjust the
 * brightness of the image before drawing the image.  To do so, the terminal
 * will create a copy of the background image (or snapshot of the root
 * window) and modify its pixel values.
 *
 * If your application intends to create multiple terminal widgets with the
 * same settings, performing this step yourself and just using
 * vte_terminal_set_background_image() will save memory.
 *
 */
void
vte_terminal_set_background_saturation(VteTerminal *terminal, double saturation)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->bg_saturation = saturation * VTE_SATURATION_MAX;
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Setting background saturation to %ld/%ld.\n",
			terminal->pvt->bg_saturation,
			(long) VTE_SATURATION_MAX);
	}
#endif
	vte_terminal_queue_background_update(terminal);
}

/* Set up the background, grabbing a new copy of the transparency background,
 * if possible. */
static gboolean
vte_terminal_update_transparent(gpointer data)
{
	VteTerminal *terminal;
	terminal = VTE_TERMINAL(data);
	if (terminal->pvt->bg_transparent_update_pending) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Starting background update.\n");
		}
#endif
		vte_terminal_setup_background(terminal, TRUE);
		terminal->pvt->bg_transparent_update_pending = FALSE;
		terminal->pvt->bg_transparent_update_tag = 0;
	}
	return FALSE;
}

/* Queue an update of the background image, to be done as soon as we can
 * get to it.  Just bail if there's already an update pending, so that if
 * opaque move tables to screw us, we don't end up with an insane backlog
 * of updates after the user finishes moving us. */
static void
vte_terminal_queue_background_update(VteTerminal *terminal)
{
	if (!terminal->pvt->bg_transparent_update_pending) {
		terminal->pvt->bg_transparent_update_pending = TRUE;
		terminal->pvt->bg_transparent_update_tag =
				g_idle_add_full(VTE_FX_PRIORITY,
						vte_terminal_update_transparent,
						terminal,
						NULL);
	} else {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_EVENTS)) {
			fprintf(stderr, "Skipping background update.\n");
		}
#endif
	}
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
		if (_vte_debug_on(VTE_DEBUG_MISC)) {
			fprintf(stderr, "Property changed.\n");
		}
#endif
		atom = terminal->pvt->bg_transparent_atom;
		if (xev->xproperty.atom == gdk_x11_atom_to_xatom(atom)) {
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_MISC)) {
				fprintf(stderr, "Property atom matches.\n");
			}
#endif
			window = terminal->pvt->bg_transparent_window;
			if (xev->xproperty.window == GDK_DRAWABLE_XID(window)) {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_MISC)) {
					fprintf(stderr, "Property window "
						"matches.\n");
				}
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

/**
 * vte_terminal_set_background_transparent:
 * @terminal: a #VteTerminal
 * @transparent: TRUE if the terminal should fake transparency
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
	GdkWindow *window;
	GdkAtom atom;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Turning background transparency %s.\n",
			transparent ? "on" : "off");
	}
#endif
	terminal->pvt->bg_transparent = transparent;

	/* To be "transparent", we treat the _XROOTPMAP_ID attribute of the
	 * root window as a picture of what's beneath us, and use that as
	 * the background.  It's a little tricky because we need to "scroll"
	 * the image to match our window position. */
	window = gdk_get_default_root_window();
	if (transparent) {
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

/**
 * vte_terminal_set_background_image:
 * @terminal: a #VteTerminal
 * @image: a #GdkPixbuf to use, or #NULL to cancel
 *
 * Sets a background image for the widget.  Text which would otherwise be
 * drawn using the default background color will instead be drawn over the
 * specified image.  If necessary, the image will be tiled to cover the
 * widget's entire visible area.
 *
 */
void
vte_terminal_set_background_image(VteTerminal *terminal, GdkPixbuf *image)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "%s background image.\n",
			GDK_IS_PIXBUF(image) ? "Setting" : "Clearing");
	}
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

/**
 * vte_terminal_set_background_image_file:
 * @terminal: a #VteTerminal
 * @path: path to an image file
 *
 * Sets a background image for the widget.  If specified by
 * vte_terminal_set_background_saturation, the terminal will make its
 * in-memory copy of the image darker for its own use.
 *
 * This is a convenience wrapper for vte_terminal_set_background_image().
 * If your application intends to create multiple terminal widgets using the
 * same background, performing this step yourself and just using
 * vte_terminal_set_background_image() will reduce memory consumption.
 *
 */
void
vte_terminal_set_background_image_file(VteTerminal *terminal, const char *path)
{
	GdkPixbuf *image;
	GError *error = NULL;
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_return_if_fail(path != NULL);
	g_return_if_fail(strlen(path) > 0);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Loading background image from `%s'.\n", path);
	}
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

/**
 * vte_terminal_get_has_selection:
 * @terminal: a #VteTerminal
 *
 * Checks if the terminal currently contains selected text.  Note that this
 * is different from determining if the terminal is the owner of any
 * #GtkClipboard items.
 *
 * Returns: TRUE if part of the text in the terminal is selected.
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
 * A #VteTerminal can use Xft, Pango, or Xlib to draw text.  This function
 * allows an application to determine which mode the widget is in.  This
 * setting cannot be changed by the caller, but in practice usually matches
 * the behavior of GTK+ itself.
 *
 * Returns: TRUE if the terminal is using Xft to render, FALSE if the terminal
 * is using Pango or Xlib.
 */
gboolean
vte_terminal_get_using_xft(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
#ifdef HAVE_XFT
	return (terminal->pvt->render_method == VteRenderXft2) ||
	       (terminal->pvt->render_method == VteRenderXft1);
#else
	return FALSE;
#endif
}

/**
 * vte_terminal_set_cursor_blinks:
 * @terminal: a #VteTerminal
 * @blink: TRUE if the cursor should blink
 *
 * Sets whether or not the cursor will blink.  The length of the blinking cycle
 * is controlled by the "gtk-cursor-blink-time" GTK+ setting.
 *
 */
void
vte_terminal_set_cursor_blinks(VteTerminal *terminal, gboolean blink)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->cursor_blinks = blink;
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
 * allowed.
 *
 */
void
vte_terminal_set_scrollback_lines(VteTerminal *terminal, glong lines)
{
	long highd, high, low, delta, max, next;
	VteScreen *screens[2];
	int i;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));

	/* We require a minimum buffer size. */
	lines = MAX(lines, VTE_SCROLLBACK_MIN);
	lines = MAX(lines, terminal->row_count);

	/* We need to resize both scrollback buffers, and this beats copying
	 * and pasting the same code twice. */
	screens[0] = &terminal->pvt->normal_screen;
	screens[1] = &terminal->pvt->alternate_screen;

	/* We want to do the same thing to both screens, so we use a loop
	 * to avoid cut/paste madness. */
	for (i = 0; i < G_N_ELEMENTS(screens); i++) {
		/* The main screen gets the full scrollback buffer, but the
		 * alternate screen isn't allowed to scroll at all. */
		delta = _vte_ring_delta(screens[i]->row_data);
		max = _vte_ring_max(screens[i]->row_data);
		next = _vte_ring_next(screens[i]->row_data);
		if (screens[i] == &terminal->pvt->alternate_screen) {
			vte_terminal_reset_rowdata(&screens[i]->row_data,
						   terminal->row_count);
		} else {
			vte_terminal_reset_rowdata(&screens[i]->row_data,
						   lines);
		}
		/* Force the offsets to point to valid rows. */
		low = _vte_ring_delta(screens[i]->row_data);
		high = low + MAX(_vte_ring_max(screens[i]->row_data), 1);
		highd = high - terminal->row_count + 1;
		screens[i]->insert_delta = CLAMP(screens[i]->insert_delta,
						 low, highd);
		screens[i]->scroll_delta = CLAMP(screens[i]->scroll_delta,
						 low, highd);
		screens[i]->cursor_current.row = CLAMP(screens[i]->cursor_current.row,
						       low, high);
		/* Clear the matching view. */
		vte_terminal_match_contents_clear(terminal);
		/* Notify viewers that the contents have changed. */
		vte_terminal_emit_contents_changed(terminal);
	}
	terminal->pvt->scrollback_lines = lines;

	/* Adjust the scrollbars to the new locations. */
	vte_terminal_adjust_adjustments(terminal, TRUE);
	vte_invalidate_all(terminal);
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
 * As a special case, if @spec is NULL or the empty string, the terminal will
 * treat all graphic non-punctuation characters as word characters.
 */
void
vte_terminal_set_word_chars(VteTerminal *terminal, const char *spec)
{
	GIConv conv;
	gunichar *wbuf;
	char *ibuf, *ibufptr, *obuf, *obufptr;
	gsize ilen, olen;
	VteWordCharRange range;
	int i;

	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* Allocate a new range array. */
	if (terminal->pvt->word_chars != NULL) {
		g_array_free(terminal->pvt->word_chars, TRUE);
	}
	terminal->pvt->word_chars = g_array_new(FALSE, TRUE,
						sizeof(VteWordCharRange));
	/* Special case: if spec is NULL, try to do the right thing. */
	if ((spec == NULL) || (strlen(spec) == 0)) {
		return;
	}
	/* Convert the spec from UTF-8 to a string of gunichars . */
	conv = g_iconv_open(_vte_table_wide_encoding(), "UTF-8");
	if (conv == ((GIConv) -1)) {
		/* Aaargh.  We're screwed. */
		g_warning(_("g_iconv_open() failed setting word characters"));
		return;
	}
	ilen = strlen(spec);
	ibuf = ibufptr = g_strdup(spec);
	olen = (ilen + 1) * sizeof(gunichar);
	_vte_buffer_set_minimum_size(terminal->pvt->conv_buffer, olen);
	obuf = obufptr = terminal->pvt->conv_buffer->bytes;
	wbuf = (gunichar*) obuf;
	wbuf[ilen] = '\0';
	g_iconv(conv, &ibuf, &ilen, &obuf, &olen);
	g_iconv_close(conv);
	for (i = 0; i < ((obuf - obufptr) / sizeof(gunichar)); i++) {
		/* The hyphen character. */
		if (wbuf[i] == '-') {
			range.start = wbuf[i];
			range.end = wbuf[i];
			g_array_append_val(terminal->pvt->word_chars, range);
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_MISC)) {
				fprintf(stderr, "Word charset includes hyphen.\n");
			}
#endif
			continue;
		}
		/* A single character, not the start of a range. */
		if ((wbuf[i] != '-') && (wbuf[i + 1] != '-')) {
			range.start = wbuf[i];
			range.end = wbuf[i];
			g_array_append_val(terminal->pvt->word_chars, range);
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_MISC)) {
				fprintf(stderr, "Word charset includes `%lc'.\n",
					(wint_t) wbuf[i]);
			}
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
			if (_vte_debug_on(VTE_DEBUG_MISC)) {
				fprintf(stderr, "Word charset includes range from "
					"`%lc' to `%lc'.\n", (wint_t) wbuf[i],
					(wint_t) wbuf[i + 2]);
			}
#endif
			i += 2;
			continue;
		}
	}
	g_free(ibufptr);
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
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* FIXME: should we set the pty mode to match? */
	terminal->pvt->backspace_binding = binding;
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
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->delete_binding = binding;
}

/**
 * vte_terminal_set_mouse_autohide:
 * @terminal: a #VteTerminal
 * @setting: TRUE if the autohide should be enabled
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
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	terminal->pvt->mouse_autohide = setting;
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
 * Returns: TRUE if autohiding is enabled, FALSE if not.
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
 * @full: TRUE to reset tabstops
 * @clear_history: TRUE to empty the terminal's scrollback buffer
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
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	/* Stop processing any of the data we've got backed up. */
	if (terminal->pvt->processing) {
		g_source_remove(terminal->pvt->processing_tag);
		terminal->pvt->processing_tag = 0;
		terminal->pvt->processing = FALSE;
	}
	/* Clear the input and output buffers. */
	if (terminal->pvt->incoming != NULL) {
		_vte_buffer_clear(terminal->pvt->incoming);
	}
	if (terminal->pvt->outgoing != NULL) {
		_vte_buffer_clear(terminal->pvt->outgoing);
	}
	/* Reset charset substitution state. */
	if (terminal->pvt->substitutions != NULL) {
		_vte_iso2022_free(terminal->pvt->substitutions);
	}
	terminal->pvt->substitutions = _vte_iso2022_new();
	/* Reset keypad/cursor/function key modes. */
	terminal->pvt->keypad_mode = VTE_KEYMODE_NORMAL;
	terminal->pvt->cursor_mode = VTE_KEYMODE_NORMAL;
	terminal->pvt->sun_fkey_mode = FALSE;
	terminal->pvt->hp_fkey_mode = FALSE;
	terminal->pvt->legacy_fkey_mode = FALSE;
	terminal->pvt->vt220_fkey_mode = FALSE;
	/* Enable meta-sends-escape. */
	terminal->pvt->meta_sends_escape = TRUE;
	/* Disable smooth scroll. */
	terminal->pvt->smooth_scroll = FALSE;
	/* Disable margin bell. */
	terminal->pvt->margin_bell = FALSE;
	/* Enable iso2022/NRC processing. */
	terminal->pvt->nrc_mode = TRUE;
	/* Reset saved settings. */
	if (terminal->pvt->dec_saved != NULL) {
		g_hash_table_destroy(terminal->pvt->dec_saved);
		terminal->pvt->dec_saved = g_hash_table_new(g_direct_hash,
							    g_direct_equal);
	}
	/* Reset the color palette. */
	/* vte_terminal_set_default_colors(terminal); */
	/* Reset the default attributes.  Reset the alternate attribute because
	 * it's not a real attribute, but we need to treat it as one here. */
	terminal->pvt->screen = &terminal->pvt->alternate_screen;
	vte_terminal_set_default_attributes(terminal);
	terminal->pvt->screen->defaults.alternate = FALSE;
	terminal->pvt->screen = &terminal->pvt->normal_screen;
	vte_terminal_set_default_attributes(terminal);
	terminal->pvt->screen->defaults.alternate = FALSE;
	/* Clear the scrollback buffers and reset the cursors. */
	if (clear_history) {
		_vte_ring_free(terminal->pvt->normal_screen.row_data, TRUE);
		terminal->pvt->normal_screen.row_data =
			_vte_ring_new(terminal->pvt->scrollback_lines,
				      vte_free_row_data, NULL);
		_vte_ring_free(terminal->pvt->alternate_screen.row_data, TRUE);
		terminal->pvt->alternate_screen.row_data =
			_vte_ring_new(terminal->pvt->scrollback_lines,
				      vte_free_row_data, NULL);
		terminal->pvt->normal_screen.cursor_saved.row = 0;
		terminal->pvt->normal_screen.cursor_saved.col = 0;
		terminal->pvt->normal_screen.cursor_current.row = 0;
		terminal->pvt->normal_screen.cursor_current.col = 0;
		terminal->pvt->normal_screen.scroll_delta = 0;
		terminal->pvt->normal_screen.insert_delta = 0;
		terminal->pvt->alternate_screen.cursor_saved.row = 0;
		terminal->pvt->alternate_screen.cursor_saved.col = 0;
		terminal->pvt->alternate_screen.cursor_current.row = 0;
		terminal->pvt->alternate_screen.cursor_current.col = 0;
		terminal->pvt->alternate_screen.scroll_delta = 0;
		terminal->pvt->alternate_screen.insert_delta = 0;
		vte_terminal_adjust_adjustments(terminal, TRUE);
	}
	/* Clear the status lines. */
	terminal->pvt->normal_screen.status_line = FALSE;
	if (terminal->pvt->normal_screen.status_line_contents != NULL) {
		g_string_free(terminal->pvt->normal_screen.status_line_contents,
			      TRUE);
	}
	terminal->pvt->normal_screen.status_line_contents = g_string_new("");
	terminal->pvt->alternate_screen.status_line = FALSE;
	if (terminal->pvt->alternate_screen.status_line_contents != NULL) {
		g_string_free(terminal->pvt->alternate_screen.status_line_contents,
			      TRUE);
	}
	terminal->pvt->alternate_screen.status_line_contents = g_string_new("");
	/* Do more stuff we refer to as a "full" reset. */
	if (full) {
		vte_terminal_set_default_tabstops(terminal);
	}
	/* Reset restricted scrolling regions, leave insert mode, make
	 * the cursor visible again. */
	terminal->pvt->normal_screen.scrolling_restricted = FALSE;
	terminal->pvt->normal_screen.insert_mode = FALSE;
	terminal->pvt->normal_screen.origin_mode = FALSE;
	terminal->pvt->normal_screen.reverse_mode = FALSE;
	terminal->pvt->alternate_screen.scrolling_restricted = FALSE;
	terminal->pvt->alternate_screen.insert_mode = FALSE;
	terminal->pvt->alternate_screen.origin_mode = FALSE;
	terminal->pvt->alternate_screen.reverse_mode = FALSE;
	terminal->pvt->cursor_visible = TRUE;
	/* Reset the encoding. */
	vte_terminal_set_encoding(terminal, NULL);
	g_assert(terminal->pvt->encoding != NULL);
	/* Reset selection. */
	vte_terminal_deselect_all(terminal);
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
	terminal->pvt->mouse_send_xy_on_click = FALSE;
	terminal->pvt->mouse_send_xy_on_button = FALSE;
	terminal->pvt->mouse_hilite_tracking = FALSE;
	terminal->pvt->mouse_cell_motion_tracking = FALSE;
	terminal->pvt->mouse_all_motion_tracking = FALSE;
	terminal->pvt->mouse_last_button = 0;
	terminal->pvt->mouse_last_x = 0;
	terminal->pvt->mouse_last_y = 0;
	/* Cause everything to be redrawn (or cleared). */
	vte_terminal_maybe_scroll_to_bottom(terminal);
	vte_invalidate_all(terminal);
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
 * edges of its visible area.  This is necessary for cases where characters
 * in the selected font don't themselves include a padding area and the
 * text itself would be contiguous with the window border.  Applications
 * which use the widget's #row_count, #column_count, #char_height, and
 * #char_width fields to set geometry hints using
 * gtk_window_set_geometry_hints() will need to add this value to the base
 * size.  The values returned in @xpad and @ypad are the total padding used
 * in each direction, and do not need to be doubled.
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
	return terminal->char_height;
}

/**
 * vte_terminal_get_char_descent:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's char_descent field
 */
glong
vte_terminal_get_char_descent(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
	return terminal->char_descent;
}

/**
 * vte_terminal_get_char_ascent:
 * @terminal: a #VteTerminal
 *
 * An accessor function provided for the benefit of language bindings.
 *
 * Returns: the contents of @terminal's char_ascent field
 */
glong
vte_terminal_get_char_ascent(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), -1);
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
