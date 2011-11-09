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

#define VTE_LEGACY_COLOR_SET_SIZE	8
#define VTE_COLOR_PLAIN_OFFSET		0
#define VTE_COLOR_BRIGHT_OFFSET		8
#define VTE_COLOR_DIM_OFFSET		16
#define VTE_COLOR_COLORCUBE_OFFSET      16
#define VTE_COLOR_SHADES_OFFSET         232
/* more color defines in vterowdata.h */

#define VTE_PALETTE_HAS_OVERRIDE(array, idx)    (array[(idx) / 32] & (1U << ((idx) % 32)))
#define VTE_PALETTE_SET_OVERRIDE(array, idx)    (array[(idx) / 32] |= (guint32)(1U << ((idx) % 32)))
#define VTE_PALETTE_CLEAR_OVERRIDE(array,idx)   (array[(idx) / 32] &= ~(guint32)(1U << ((idx) % 32)))

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

#define VTE_UTF8_BPC                    (6) /* Maximum number of bytes used per UTF-8 character */

#define VTE_SCALE_MIN                   (.25)
#define VTE_SCALE_MAX                   (4.)

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

struct _vte_regex_match {
       int rm_so;
       int rm_eo;
};

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

typedef struct _VteWordCharRange {
	gunichar start, end;
} VteWordCharRange;

typedef struct _VteVisualPosition {
	long row, col;
} VteVisualPosition;

typedef struct _VteBufferIterReal VteBufferIterReal;
struct _VteBufferIterReal {
        VteVisualPosition position;
        VteBuffer *buffer;
        VteScreen *screen;
};
G_STATIC_ASSERT(sizeof(VteBufferIterReal) <= sizeof(VteBufferIter));

struct _VteBufferClassPrivate {
        gpointer dummy;
};

struct _VteBufferPrivate {
        /* The VteView that's displaying this buffer */
        VteView *terminal;

        /* Metric and sizing data: dimensions of the window */
        glong row_count;
        glong column_count;

	/* Emulation setup data. */
	struct _vte_termcap *termcap;	/* termcap storage */
	struct _vte_matcher *matcher;	/* control sequence matcher */
	const char *termcap_path;	/* path to termcap file */
	const char *emulation;		/* terminal type to emulate */
	struct vte_view_flags {	/* boolean termcap flags */
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
	VtePty *pty;
	GIOChannel *pty_channel;	/* master channel */
	guint pty_input_source;
	guint pty_output_source;
	gboolean pty_input_active;
	GPid pty_pid;			/* pid of child using pty slave */
	guint child_watch_source;

	/* Input data queues. */
	const char *encoding;		/* the pty's encoding */
	struct _vte_iso2022_state *iso2022;
	struct _vte_incoming_chunk{
		struct _vte_incoming_chunk *next;
		guint len;
		guchar data[VTE_INPUT_CHUNK_SIZE
			- 2 * sizeof(void *)];
	} *incoming;			/* pending bytestream */
	GArray *pending;		/* pending characters */
//	GList *active;                  /* is the terminal processing data */
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

        /* Scrolling options. */
        glong scrollback_lines;

	/* Miscellaneous options. */
	VteEraseBinding backspace_binding, delete_binding;
	gboolean meta_sends_escape;
	gboolean margin_bell;
	gboolean nrc_mode;
	gboolean smooth_scroll;
	GHashTable *tabstops;
	gboolean text_modified_flag;
	gboolean text_inserted_flag;
	gboolean text_deleted_flag;
        gboolean contents_changed_pending;

	GdkRGBA palette[VTE_PALETTE_SIZE];
        guint32 palette_set[(VTE_PALETTE_SIZE + 31) / 32];

	/* Adjustment updates pending. */
        gboolean accessible_emit;
        gboolean cursor_moved_pending;

	/* window name changes */
        gchar *window_title;
	gchar *window_title_changed;
        gchar *icon_title;
	gchar *icon_title_changed;

        /* Cursor */
        gboolean cursor_visible;

        /* Mouse tracking */
        gboolean mouse_urxvt_extension;

        /* FIXMEchpe: this is duplicated wiht VteBufferPrivate; keep just one
         * and update the other! (Not sure if this belongs in the view or the
         * buffer, although it _is_ set from vteseq.c.)
         */
        int mouse_tracking_mode; /* this is of type MouseTrackingMode,
                                    but we need to guarantee its type. */
        
};

struct _VteViewPrivate {
        VteBuffer *buffer;
        VteBufferPrivate *buffer_pvt;

        /* Expose event handling */
        GSList *update_regions;
        gboolean invalidated_all;       /* pending refresh of entire terminal */

        /* Cursor */
        VteCursorShape cursor_shape;
        VteCursorBlinkMode cursor_blink_mode;
        float cursor_aspect_ratio;
        gboolean cursor_blink_state;
        guint cursor_blink_tag;           /* cursor blinking timeout ID */
        gint cursor_blink_cycle;          /* gtk-cursor-blink-time / 2 */
        gint cursor_blink_timeout;        /* gtk-cursor-blink-timeout */
        gboolean cursor_blinks;           /* whether the cursor is actually blinking */
        gint64 cursor_blink_time;         /* how long the cursor has been blinking yet */
        gboolean has_focus;               /* is the terminal window focused */

        /* Input method support */
        GtkIMContext *im_context;
        gboolean im_preedit_active;
        char *im_preedit;
        PangoAttrList *im_preedit_attrs;
        int im_preedit_cursor;

        /* Selection information. */
        GArray *word_chars;
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
        gboolean audible_bell;
        gboolean visible_bell;
        guint bell_margin;
        gboolean allow_bold;

        /* Scrolling options. */
        gboolean scroll_on_output;
        gboolean scroll_on_keystroke;

        /* Input device options. */
        time_t last_keypress_time;

        int mouse_tracking_mode; /* this is of type MouseTrackingMode,
                                    but we need to guarantee its type. */
        guint mouse_last_button;
        long mouse_last_x, mouse_last_y;
        long mouse_last_cell_x, mouse_last_cell_y;
        gboolean mouse_autohide;
        guint mouse_autoscroll_tag;

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

        gboolean reverse;
        gboolean highlight_color_set;
        gboolean cursor_color_set;
        gboolean reverse_color_set;
        GdkRGBA palette[VTE_PALETTE_SIZE];
        guint32 palette_set[(VTE_PALETTE_SIZE + 31) / 32];

        /* Mouse cursors. */
        gboolean mouse_cursor_visible;
        GdkCursor *mouse_default_cursor,
                  *mouse_mousing_cursor,
                  *mouse_inviso_cursor;

        /* Adjustment updates pending. */
        gboolean adjustment_changed_pending;
        gboolean adjustment_value_changed_pending;

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

        /* FIXMEchpe move this to buffer! */
        GList *active;                  /* is the terminal processing data */

};

struct _VteViewClassPrivate {
        GtkStyleProvider *style_provider;
};

void _vte_view_set_pointer_visible(VteView *terminal, gboolean visible);
void _vte_invalidate_all(VteView *terminal);
void _vte_invalidate_cells(VteView *terminal,
			   glong column_start, gint column_count,
			   glong row_start, gint row_count);
void _vte_invalidate_cell(VteView *terminal, glong col, glong row);
void _vte_invalidate_cursor_once(VteView *terminal, gboolean periodic);
void _vte_view_adjust_adjustments(VteView *terminal);
void _vte_view_scroll_region(VteView *terminal,
				 long row, glong count, glong delta);

gboolean _vte_view_xy_to_grid(VteView *terminal,
                                  long x,
                                  long y,
                                  long *col,
                                  long *row);
gboolean _vte_view_size_to_grid_size(VteView *terminal,
                                         long w,
                                         long h,
                                         long *cols,
                                         long *rows);

void _vte_view_set_effect_color(VteView *terminal,
                                    int entry,
                                    const GdkRGBA *rgba,
                                    VteEffect effect,
                                    gboolean override);

gboolean _vte_view_is_word_char(VteView *terminal, gunichar c);
glong _vte_view_get_char_width(VteView *view);
glong _vte_view_get_char_height(VteView *view);

/* private VteBuffer methods */

VteRowData *_vte_buffer_ring_insert (VteBuffer *buffer, glong position, gboolean fill);
VteRowData *_vte_buffer_ring_append (VteBuffer *buffer, gboolean fill);
void _vte_buffer_ring_remove (VteBuffer *buffer, glong position);
void _vte_buffer_clear_tabstop(VteBuffer *buffer, int column);
void _vte_buffer_clear_tabstops(VteBuffer *buffer);
gboolean _vte_buffer_get_tabstop(VteBuffer *buffer, int column);
void _vte_buffer_set_tabstop(VteBuffer *buffer, int column);
VteRowData *_vte_buffer_ensure_row(VteBuffer *buffer);
void _vte_buffer_cleanup_tab_fragments_at_cursor (VteBuffer *buffer);
void _vte_buffer_cursor_down (VteBuffer *buffer);
gboolean _vte_buffer_insert_char(VteBuffer *buffer, gunichar c,
                                 gboolean force_insert_mode,
                                 gboolean invalidate_cells);
void _vte_buffer_emit_deiconify_window(VteBuffer *buffer);
void _vte_buffer_emit_iconify_window(VteBuffer *buffer);
void _vte_buffer_emit_raise_window(VteBuffer *buffer);
void _vte_buffer_emit_lower_window(VteBuffer *buffer);
void _vte_buffer_emit_refresh_window(VteBuffer *buffer);
void _vte_buffer_emit_restore_window(VteBuffer *buffer);
void _vte_buffer_emit_maximize_window(VteBuffer *buffer);
void _vte_buffer_emit_resize_window(VteBuffer *buffer, guint w, guint h);
void _vte_buffer_emit_move_window(VteBuffer *buffer, guint x, guint y);
void _vte_buffer_emit_text_deleted(VteBuffer *buffer);
void _vte_buffer_emit_text_inserted(VteBuffer *buffer);
void _vte_buffer_emit_bell(VteBuffer *buffer, VteBellType bell_type);
void _vte_buffer_queue_contents_changed(VteBuffer *buffer);
void _vte_buffer_handle_sequence(VteBuffer *buffer,
                                 const char *match_s,
                                 GQuark match,
                                 GValueArray *params);
void _vte_buffer_view_adjust_adjustments(VteBuffer *buffer);
void _vte_buffer_view_invalidate_all(VteBuffer *buffer);
void _vte_buffer_view_invalidate_cells(VteBuffer *buffer,
                                       glong column_start, gint column_count,
                                       glong row_start, gint row_count);
void _vte_buffer_view_scroll_region(VteBuffer *buffer,
                                    glong row, glong count, glong delta);

/* private VteBufferIter methods */
void _vte_buffer_iter_init(VteBufferIterReal *iter, VteBuffer *buffer);
void _vte_buffer_iter_get_position(VteBufferIter *iter, glong *row, glong *column);

/* private VteScreen methods */
void _vte_screen_set_default_attributes(VteScreen *screen);

G_END_DECLS

#endif
