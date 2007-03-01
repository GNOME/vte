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

#ifndef vte_vte_private_h_included
#define vte_vte_private_h_included

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>
#include <glib/gi18n-lib.h>

#include "vte.h"
#include "buffer.h"
#include "debug.h"
#include "vteconv.h"
#include "vtedraw.h"
#include "reaper.h"
#include "ring.h"
#include "caps.h"

G_BEGIN_DECLS

#define VTE_PAD_WIDTH			1
#define VTE_TAB_WIDTH			8
#define VTE_LINE_WIDTH			1
#define VTE_ROWS			24
#define VTE_COLUMNS			80
#define VTE_LEGACY_COLOR_SET_SIZE	8
#define VTE_COLOR_PLAIN_OFFSET		0
#define VTE_COLOR_BRIGHT_OFFSET		8
#define VTE_COLOR_DIM_OFFSET		16
#define VTE_DEF_FG			256
#define VTE_DEF_BG			257
#define VTE_BOLD_FG			258
#define VTE_DIM_FG			259
#define VTE_DEF_HL                      260
#define VTE_CUR_BG			261

#define VTE_SATURATION_MAX		10000
#define VTE_SCROLLBACK_INIT		100
#define VTE_DEFAULT_CURSOR		GDK_XTERM
#define VTE_MOUSING_CURSOR		GDK_LEFT_PTR
#define VTE_TAB_MAX			999
#define VTE_ADJUSTMENT_PRIORITY		G_PRIORITY_DEFAULT_IDLE
#define VTE_INPUT_RETRY_PRIORITY	G_PRIORITY_HIGH
#define VTE_INPUT_PRIORITY		G_PRIORITY_DEFAULT_IDLE
#define VTE_CHILD_INPUT_PRIORITY	G_PRIORITY_DEFAULT_IDLE
#define VTE_CHILD_OUTPUT_PRIORITY	G_PRIORITY_HIGH
#define VTE_FX_PRIORITY			G_PRIORITY_DEFAULT_IDLE
#define VTE_REGCOMP_FLAGS		REG_EXTENDED
#define VTE_REGEXEC_FLAGS		0
#define VTE_INPUT_CHUNK_SIZE		0x2000
#define VTE_MAX_INPUT_READ		0x1000
#define VTE_INVALID_BYTE		'?'
#define VTE_DISPLAY_TIMEOUT		10
#define VTE_UPDATE_TIMEOUT		15
#define VTE_UPDATE_REPEAT_TIMEOUT	30
#define VTE_MAX_PROCESS_TIME		100
#define VTE_CELL_BBOX_SLACK		1

/* The structure we use to hold characters we're supposed to display -- this
 * includes any supported visible attributes. */
struct vte_charcell {
	gunichar c;		/* The Unicode character. */

	guint32 columns: 2;	/* Number of visible columns (as determined
				   by g_unicode_iswide(c)). */
	guint32 fore: 9;	/* Indices in the color palette for the */
	guint32 back: 9;	/* foreground and background of the cell. */

	guint32 fragment: 1;	/* The nth fragment of a wide character. */
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
	guint32 empty : 1;

	/* no more bits left.  any addition will enlarge the struct */
};

/* A match regex, with a tag. */
struct vte_match_regex {
	struct _vte_regex *reg;
	gint tag;
	GdkCursor *cursor;
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

typedef struct _VteRowData {
	GArray *cells;
	guchar soft_wrapped: 1;
} VteRowData;

/* Terminal private data. */
struct _VteTerminalPrivate {
	/* Emulation setup data. */
	struct _vte_termcap *termcap;	/* termcap storage */
	struct _vte_matcher *matcher;	/* control sequence matcher */
	const char *termcap_path;	/* path to termcap file */
	const char *emulation;		/* terminal type to emulate */
	struct vte_terminal_flags {	/* boolean termcap flags */
		gboolean am;
		gboolean bw;
		gboolean LP;
		gboolean ul;
		gboolean xn;
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

	/* PTY handling data. */
	const char *shell;		/* shell we started */
	int pty_master;			/* pty master descriptor */
	GIOChannel *pty_input;		/* master input watch */
	guint pty_input_source;
	gboolean pty_input_active;
	GIOChannel *pty_output;		/* master output watch */
	guint pty_output_source;
	GPid pty_pid;			/* pid of child using pty slave */
	VteReaper *pty_reaper;

	/* Input data queues. */
	const char *encoding;		/* the pty's encoding */
	struct _vte_iso2022_state *iso2022;
	struct _vte_incoming_chunk{
		struct _vte_incoming_chunk *next;
		gint len;
		guchar data[VTE_INPUT_CHUNK_SIZE
			- sizeof(struct _vte_incoming_chunk *) - sizeof(gint)];
	} *incoming;			/* pending bytestream */
	GArray *pending;		/* pending characters */
	GSList *update_regions;
	gboolean invalidated_all;	/* pending refresh of entire terminal */
	GList *active;                  /* is the terminal processing data */
	glong input_bytes;
	glong max_input_bytes;

	/* Output data queue. */
	struct _vte_buffer *outgoing;	/* pending input characters */
	VteConv outgoing_conv;

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
		gboolean sendrecv_mode;	/* sendrecv mode */
		gboolean insert_mode;	/* insert mode */
		gboolean linefeed_mode;	/* linefeed mode */
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
							   plus the current
							   fore/back with no
							   character data */
		struct vte_charcell basic_defaults;	/* original defaults */
		gboolean status_line;
		GString *status_line_contents;
		gboolean status_line_changed;
	} normal_screen, alternate_screen, *screen;
	VteRowData *free_row;

	/* Selection information. */
	GArray *word_chars;
	gboolean has_selection;
	gboolean selecting;
	gboolean selecting_restart;
	gboolean selecting_had_delta;
	gboolean block_mode;
	gboolean had_block_mode;
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
	gboolean text_modified_flag;
	glong text_inserted_count;
	glong text_deleted_count;

	/* Scrolling options. */
	gboolean scroll_background;
	gboolean scroll_on_output;
	gboolean scroll_on_keystroke;
	long scrollback_lines;

	/* Cursor blinking. */
	gboolean cursor_blink_state;
	gboolean cursor_blinks;
	guint cursor_blink_tag;
	gint cursor_blink_timeout;
	gint64 cursor_blink_time;
	gboolean cursor_visible;

	/* Input device options. */
	time_t last_keypress_time;
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
	char *match;
	int match_tag;
	struct {
		long row, column;
	} match_start, match_end;
	gboolean show_match;

	/* Data used when rendering the text which does not require server
	 * resources and which can be kept after unrealizing. */
	PangoFontDescription *fontdesc;
	VteTerminalAntiAlias fontantialias;
	gboolean fontdirty;
	GtkSettings *connected_settings;

	/* Data used when rendering the text which reflects server resources
	 * and data, which should be dropped when unrealizing and (re)created
	 * when realizing. */
	struct _vte_draw *draw;

	gboolean palette_initialized;
	gboolean highlight_color_set;
	gboolean cursor_color_set;
	struct vte_palette_entry {
		guint16 red, green, blue;
	} palette[VTE_CUR_BG + 1];

	/* Mouse cursors. */
	gboolean mouse_cursor_visible;
	GdkCursor *mouse_default_cursor,
		  *mouse_mousing_cursor,
		  *mouse_inviso_cursor;

	/* Input method support. */
	GtkIMContext *im_context;
	gboolean im_preedit_active;
	char *im_preedit;
	PangoAttrList *im_preedit_attrs;
	int im_preedit_cursor;

	gboolean accessible_emit;

	/* Adjustment updates pending. */
	gboolean adjustment_changed_pending;
	gboolean adjustment_value_changed_pending;

	gboolean cursor_moved_pending;
	gboolean contents_changed_pending;

	/* window name changes */
	gchar *window_title_changed;
	gchar *icon_title_changed;

	/* Background images/"transparency". */
	gboolean bg_update_pending;
	gboolean bg_transparent;
	GdkPixbuf *bg_pixbuf;
	char *bg_file;
	GdkColor bg_tint_color;
	long bg_saturation;	/* out of VTE_SATURATION_MAX */
	guint16 bg_opacity;

	/* Key modifiers. */
	GdkModifierType modifiers;

	/* Obscured? state. */
	GdkVisibilityState visibility_state;

	gboolean has_fonts;
};


VteRowData *_vte_terminal_ensure_row(VteTerminal *terminal);
void _vte_terminal_set_pointer_visible(VteTerminal *terminal, gboolean visible);
void _vte_invalidate_all(VteTerminal *terminal);
void _vte_invalidate_cells(VteTerminal *terminal,
			   glong column_start, gint column_count,
			   glong row_start, gint row_count);
void _vte_invalidate_cell(VteTerminal *terminal, glong col, glong row);
void _vte_invalidate_cursor_once(VteTerminal *terminal, gboolean periodic);
VteRowData * _vte_new_row_data(VteTerminal *terminal);
VteRowData * _vte_new_row_data_sized(VteTerminal *terminal, gboolean fill);
VteRowData * _vte_reset_row_data (VteTerminal *terminal, VteRowData *row, gboolean fill);
void _vte_terminal_adjust_adjustments(VteTerminal *terminal);
void _vte_terminal_queue_contents_changed(VteTerminal *terminal);
void _vte_terminal_emit_text_deleted(VteTerminal *terminal);
void _vte_terminal_emit_text_inserted(VteTerminal *terminal);
void _vte_terminal_insert_char(VteTerminal *terminal, gunichar c,
			       gboolean force_insert_mode,
			       gboolean invalidate_cells,
			       gboolean paint_cells,
			       gint forced_width);
void _vte_terminal_scroll_region(VteTerminal *terminal,
				 long row, glong count, glong delta);
void _vte_terminal_set_default_attributes(VteTerminal *terminal);
void _vte_terminal_clear_tabstop(VteTerminal *terminal, int column);
gboolean _vte_terminal_get_tabstop(VteTerminal *terminal, int column);
void _vte_terminal_set_tabstop(VteTerminal *terminal, int column);
void _vte_terminal_update_insert_delta(VteTerminal *terminal);

void _vte_terminal_inline_error_message(VteTerminal *terminal, const char *format, ...) G_GNUC_PRINTF(2,3);

G_END_DECLS

#endif
