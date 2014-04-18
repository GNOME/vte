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
#include "ring.h"
#include "caps.h"

G_BEGIN_DECLS

#define VTE_TAB_WIDTH			8
#define VTE_LINE_WIDTH			1
#define VTE_ROWS			24
#define VTE_COLUMNS			80

/*
 * Colors are encoded in 25 bits as follows:
 *
 * 0 .. 255:
 *   Colors set by SGR 256-color extension (38/48;5;index).
 *   These are direct indices into the color palette.
 *
 * 256 .. VTE_PALETTE_SIZE - 1 (262):
 *   Special values, such as default colors.
 *   These are direct indices into the color palette.
 *
 * VTE_LEGACY_COLORS_OFFSET (512) .. VTE_LEGACY_COLORS_OFFSET + VTE_LEGACY_FULL_COLOR_SET_SIZE - 1 (527):
 *   Colors set by legacy escapes (30..37/40..47, 90..97/100..107).
 *   These are translated to 0 .. 15 before looking up in the palette, taking bold into account.
 *
 * VTE_RGB_COLOR (2^24) .. VTE_RGB_COLOR + 16Mi - 1 (2^25 - 1):
 *   Colors set by SGR truecolor extension (38/48;2;red;green;blue)
 *   These are direct RGB values.
 */
#define VTE_LEGACY_COLORS_OFFSET	512
#define VTE_LEGACY_COLOR_SET_SIZE	8
#define VTE_LEGACY_FULL_COLOR_SET_SIZE	16
#define VTE_COLOR_PLAIN_OFFSET		0
#define VTE_COLOR_BRIGHT_OFFSET		8
#define VTE_COLOR_DIM_OFFSET		16
#define VTE_RGB_COLOR			(1 << 24)
/* More color defines in vterowdata.h */

#define VTE_SCROLLBACK_INIT		512
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

#define VTE_UTF8_BPC                    (6) /* Maximum number of bytes used per UTF-8 character */

/* Keep in decreasing order of precedence. */
#define VTE_COLOR_SOURCE_ESCAPE 0
#define VTE_COLOR_SOURCE_API 1

#define VTE_FONT_SCALE_MIN (.25)
#define VTE_FONT_SCALE_MAX (4.)

#define I_(string) (g_intern_static_string(string))

typedef enum {
  VTE_REGEX_CURSOR_GDKCURSOR,
  VTE_REGEX_CURSOR_GDKCURSORTYPE,
  VTE_REGEX_CURSOR_NAME
} VteRegexCursorMode;

/* The order is important */
typedef enum {
	MOUSE_TRACKING_NONE,
	MOUSE_TRACKING_SEND_XY_ON_CLICK,
	MOUSE_TRACKING_SEND_XY_ON_BUTTON,
	MOUSE_TRACKING_HILITE_TRACKING,
	MOUSE_TRACKING_CELL_MOTION_TRACKING,
	MOUSE_TRACKING_ALL_MOTION_TRACKING
} MouseTrackingMode;

/* A match regex, with a tag. */
struct vte_match_regex {
	gint tag;
        GRegex *regex;
        GRegexMatchFlags match_flags;
        VteRegexCursorMode cursor_mode;
        union {
	       GdkCursor *cursor;
               char *cursor_name;
               GdkCursorType cursor_type;
        } cursor;
};

/* The terminal's keypad/cursor state.  A terminal can either be using the
 * normal keypad, or the "application" keypad. */
typedef enum _VteKeymode {
	VTE_KEYMODE_NORMAL,
	VTE_KEYMODE_APPLICATION
} VteKeymode;

typedef struct _VteScreen VteScreen;

typedef struct _VtePaletteColor {
	struct {
		PangoColor color;
		gboolean is_set;
	} sources[2];
} VtePaletteColor;

/* Terminal private data. */
struct _VteTerminalPrivate {
        /* Metric and sizing data: dimensions of the window */
        glong row_count;
        glong column_count;

	/* Emulation setup data. */
	struct _vte_terminfo *terminfo;	/* terminfo */
	struct _vte_matcher *matcher;	/* control sequence matcher */
	const char *emulation;		/* terminal type to emulate */
	struct vte_terminal_flags {	/* boolean terminfo flags */
		gboolean am;
		gboolean bw;
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
	VtePty *pty;
	GIOChannel *pty_channel;	/* master channel */
	guint pty_input_source;
	guint pty_output_source;
	gboolean pty_input_active;
	GPid pty_pid;			/* pid of child using pty slave */
	guint child_watch_source;

	/* Input data queues. */
	const char *encoding;		/* the pty's encoding */
        int iso2022_utf8_ambiguous_width;
	struct _vte_iso2022_state *iso2022;
	struct _vte_incoming_chunk{
		struct _vte_incoming_chunk *next;
		guint len;
		guchar data[VTE_INPUT_CHUNK_SIZE
			- 2 * sizeof(void *)];
	} *incoming;			/* pending bytestream */
	GArray *pending;		/* pending characters */
	GSList *update_regions;
	gboolean invalidated_all;	/* pending refresh of entire terminal */
	GList *active;                  /* is the terminal processing data */
	glong input_bytes;
	glong max_input_bytes;

	/* Output data queue. */
	VteByteArray *outgoing;	/* pending input characters */
	VteConv outgoing_conv;

	/* IConv buffer. */
	VteByteArray *conv_buffer;

	/* Screen data.  We support the normal screen, and an alternate
	 * screen, which seems to be a DEC-specific feature. */
	struct _VteScreen {
		VteRing row_data[1];	/* buffer contents */
		VteVisualPosition cursor_current, cursor_saved;
					/* the current and saved positions of
					   the [insertion] cursor -- current is
					   absolute, saved is relative to the
					   insertion delta */
		gboolean reverse_mode;	/* reverse mode */
		gboolean origin_mode;	/* origin mode */
		gboolean sendrecv_mode;	/* sendrecv mode */
		gboolean insert_mode;	/* insert mode */
		gboolean linefeed_mode;	/* linefeed mode */
		gboolean bracketed_paste_mode;
		struct vte_scrolling_region {
			int start, end;
		} scrolling_region;	/* the region we scroll in */
		gboolean scrolling_restricted;
		long scroll_delta;	/* scroll offset */
		long insert_delta;	/* insertion offset */
		VteCell defaults;	/* default characteristics
					   for insertion of any new
					   characters */
		VteCell color_defaults;	/* original defaults
					   plus the current
					   fore/back */
		VteCell fill_defaults;	/* original defaults
					   plus the current
					   fore/back with no
					   character data */
		gboolean alternate_charset;
		gboolean status_line;
		GString *status_line_contents;
		gboolean status_line_changed;
	} normal_screen, alternate_screen, *screen;

	/* Selection information. */
	gboolean has_selection;
	gboolean selecting;
	gboolean selecting_after_threshold;
	gboolean selecting_restart;
	gboolean selecting_had_delta;
	gboolean selection_block_mode;
	char *selection;
	enum vte_selection_type {
		selection_type_char,
		selection_type_word,
		selection_type_line
	} selection_type;
	struct selection_event_coords {
		long x, y;
	} selection_origin, selection_last;
	VteVisualPosition selection_start, selection_end;

	/* Miscellaneous options. */
	VteEraseBinding backspace_binding, delete_binding;
	gboolean meta_sends_escape;
	gboolean audible_bell;
	gboolean visible_bell;
	gboolean margin_bell;
	guint bell_margin;
	gboolean allow_bold;
	gboolean nrc_mode;
        gboolean deccolm_mode; /* DECCOLM allowed */
	GHashTable *tabstops;
	gboolean text_modified_flag;
	gboolean text_inserted_flag;
	gboolean text_deleted_flag;
	gboolean rewrap_on_resize;

	/* Scrolling options. */
	gboolean scroll_background;
	gboolean scroll_on_output;
	gboolean scroll_on_keystroke;
	gboolean alternate_screen_scroll;
	long scrollback_lines;

	/* Cursor shape */
	VteCursorShape cursor_shape;
        float cursor_aspect_ratio;

	/* Cursor blinking. */
        VteCursorBlinkMode cursor_blink_mode;
	gboolean cursor_blink_state;
	guint cursor_blink_tag;           /* cursor blinking timeout ID */
        gint cursor_blink_cycle;          /* gtk-cursor-blink-time / 2 */
	gint cursor_blink_timeout;        /* gtk-cursor-blink-timeout */
        gboolean cursor_blinks;           /* whether the cursor is actually blinking */
	gint64 cursor_blink_time;         /* how long the cursor has been blinking yet */
	gboolean cursor_visible;
	gboolean has_focus;               /* is the terminal window focused */

	/* Input device options. */
        gboolean input_enabled;
	time_t last_keypress_time;

	int mouse_tracking_mode; /* this is of type MouseTrackingMode,
				    but we need to guarantee its type. */
	guint mouse_last_button;
	long mouse_last_x, mouse_last_y;
	gboolean mouse_autohide;
	guint mouse_autoscroll_tag;
	gboolean mouse_xterm_extension;
	gboolean mouse_urxvt_extension;
	double mouse_smooth_scroll_delta;

	/* State variables for handling match checks. */
	char *match_contents;
	GArray *match_attributes;
	GArray *match_regexes;
	char *match;
	int match_tag;
	VteVisualPosition match_start, match_end;
	gboolean show_match;

	/* Search data. */
	GRegex *search_regex;
        GRegexMatchFlags search_match_flags;
	gboolean search_wrap_around;
	GArray *search_attrs; /* Cache attrs */

	/* Data used when rendering the text which does not require server
	 * resources and which can be kept after unrealizing. */
        PangoFontDescription *unscaled_font_desc;
	PangoFontDescription *fontdesc;
        gdouble font_scale;
	gboolean fontdirty;
        glong char_ascent;
        glong char_descent;
        /* dimensions of character cells */
        glong char_width;
        glong char_height;

	/* Data used when rendering the text which reflects server resources
	 * and data, which should be dropped when unrealizing and (re)created
	 * when realizing. */
	struct _vte_draw *draw;

	VtePaletteColor palette[VTE_PALETTE_SIZE];

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
        gchar *window_title;
	gchar *window_title_changed;
        gchar *icon_title;
	gchar *icon_title_changed;
        gchar *current_directory_uri;
        gchar *current_directory_uri_changed;
        gchar *current_file_uri;
        gchar *current_file_uri_changed;

	/* Background */
        gdouble background_alpha;

	/* Key modifiers. */
	GdkModifierType modifiers;

	/* Obscured? state. */
	GdkVisibilityState visibility_state;

	/* Font stuff. */
	gboolean has_fonts;
	glong line_thickness;
	glong underline_position;
	glong strikethrough_position;

        /* Style stuff */
        GtkBorder padding;

        /* GtkScrollable impl */
        GtkAdjustment *hadjustment; /* unused */
        GtkAdjustment *vadjustment;
        guint hscroll_policy : 1; /* unused */

        guint vscroll_policy : 1;
};

struct _VteTerminalClassPrivate {
        GtkStyleProvider *style_provider;
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
void _vte_terminal_adjust_adjustments(VteTerminal *terminal);
void _vte_terminal_queue_contents_changed(VteTerminal *terminal);
void _vte_terminal_emit_text_deleted(VteTerminal *terminal);
void _vte_terminal_emit_text_inserted(VteTerminal *terminal);
void _vte_terminal_cursor_down (VteTerminal *terminal);
gboolean _vte_terminal_insert_char(VteTerminal *terminal, gunichar c,
			       gboolean force_insert_mode,
			       gboolean invalidate_cells);
void _vte_terminal_scroll_region(VteTerminal *terminal,
				 long row, glong count, glong delta);
void _vte_terminal_set_default_attributes(VteTerminal *terminal);
void _vte_terminal_clear_tabstop(VteTerminal *terminal, int column);
gboolean _vte_terminal_get_tabstop(VteTerminal *terminal, int column);
void _vte_terminal_set_tabstop(VteTerminal *terminal, int column);
void _vte_terminal_update_insert_delta(VteTerminal *terminal);
void _vte_terminal_cleanup_tab_fragments_at_cursor (VteTerminal *terminal);
void _vte_terminal_audible_beep(VteTerminal *terminal);
void _vte_terminal_visible_beep(VteTerminal *terminal);
void _vte_terminal_beep(VteTerminal *terminal);
PangoColor *_vte_terminal_get_color(const VteTerminal *terminal, int idx);
void _vte_terminal_set_color_internal(VteTerminal *terminal,
                                      int idx,
                                      int source,
                                      const PangoColor *color);

void _vte_terminal_inline_error_message(VteTerminal *terminal, const char *format, ...) G_GNUC_PRINTF(2,3);

VteRowData *_vte_terminal_ring_insert (VteTerminal *terminal, glong position, gboolean fill);
VteRowData *_vte_terminal_ring_append (VteTerminal *terminal, gboolean fill);
void _vte_terminal_ring_remove (VteTerminal *terminal, glong position);

/* vteseq.c: */
void _vte_terminal_handle_sequence(VteTerminal *terminal,
				   const char *match_s,
				   GQuark match,
				   GValueArray *params);
gboolean _vte_terminal_can_handle_sequence(const char *name);
gboolean _vte_terminal_xy_to_grid(VteTerminal *terminal,
                                  long x,
                                  long y,
                                  long *col,
                                  long *row);
gboolean _vte_terminal_size_to_grid_size(VteTerminal *terminal,
                                         long w,
                                         long h,
                                         long *cols,
                                         long *rows);

gboolean _vte_is_word_char(gunichar c) G_GNUC_CONST;

G_END_DECLS

#endif
