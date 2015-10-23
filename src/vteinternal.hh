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

#pragma once

#include <glib.h>

typedef enum {
        VTE_REGEX_UNDECIDED,
        VTE_REGEX_PCRE2,
        VTE_REGEX_GREGEX
} VteRegexMode;

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

struct vte_regex_and_flags {
        VteRegexMode mode;
        union { /* switched on @mode */
                struct {
                        VteRegex *regex;
                        guint32 match_flags;
                } pcre;
                struct {
                        GRegex *regex;
                        GRegexMatchFlags match_flags;
                } gregex;
        };
};

/* A match regex, with a tag. */
struct vte_match_regex {
	gint tag;
        struct vte_regex_and_flags regex;
        VteRegexCursorMode cursor_mode;
        union {
	       GdkCursor *cursor;
               char *cursor_name;
               GdkCursorType cursor_type;
        } cursor;
};

typedef enum _VteCharacterReplacement {
        VTE_CHARACTER_REPLACEMENT_NONE,
        VTE_CHARACTER_REPLACEMENT_LINE_DRAWING,
        VTE_CHARACTER_REPLACEMENT_BRITISH
} VteCharacterReplacement;

/* The terminal's keypad/cursor state.  A terminal can either be using the
 * normal keypad, or the "application" keypad. */
typedef enum _VteKeymode {
	VTE_KEYMODE_NORMAL,
	VTE_KEYMODE_APPLICATION
} VteKeymode;

typedef struct _VtePaletteColor {
	struct {
		PangoColor color;
		gboolean is_set;
	} sources[2];
} VtePaletteColor;

/* These correspond to the parameters for DECSCUSR (Set cursor style). */
typedef enum _VteCursorStyle {
        /* We treat 0 and 1 differently, assuming that the VT510 does so too.
         *
         * See, according to the "VT510 Video Terminal Programmer Information",
         * from vt100.net, paragraph "2.5.7 Cursor Display", there was a menu
         * item in the "Terminal Set-Up" to set the cursor's style. It looks
         * like that defaulted to blinking block. So it makes sense for 0 to
         * mean "set cursor style to default (set by Set-Up)" and 1 to mean
         * "set cursor style to blinking block", since that default need not be
         * blinking block. Access to a VT510 is needed to test this theory,
         * but it seems plausible. And, anyhow, we can even decide we know
         * better than the VT510 designers! */
        VTE_CURSOR_STYLE_TERMINAL_DEFAULT = 0,
        VTE_CURSOR_STYLE_BLINK_BLOCK      = 1,
        VTE_CURSOR_STYLE_STEADY_BLOCK     = 2,
        VTE_CURSOR_STYLE_BLINK_UNDERLINE  = 3,
        VTE_CURSOR_STYLE_STEADY_UNDERLINE = 4,
        /* *_IBEAM are xterm extensions */
        VTE_CURSOR_STYLE_BLINK_IBEAM      = 5,
        VTE_CURSOR_STYLE_STEADY_IBEAM     = 6
} VteCursorStyle;

typedef struct _vte_incoming_chunk _vte_incoming_chunk_t;
struct _vte_incoming_chunk{
        _vte_incoming_chunk_t *next;
        guint len;
        guchar data[VTE_INPUT_CHUNK_SIZE - 2 * sizeof(void *)];
};

typedef struct _VteScreen VteScreen;
struct _VteScreen {
        VteRing row_data[1];	/* buffer contents */
        double scroll_delta;	/* scroll offset */
        long insert_delta;	/* insertion offset */

        /* Stuff saved along with the cursor */
        struct {
                VteVisualPosition cursor;
                gboolean reverse_mode;
                gboolean origin_mode;
                gboolean sendrecv_mode;
                gboolean insert_mode;
                gboolean linefeed_mode;
                VteCell defaults;
                VteCell color_defaults;
                VteCell fill_defaults;
                VteCharacterReplacement character_replacements[2];
                VteCharacterReplacement *character_replacement;
        } saved;
};

enum vte_selection_type {
        selection_type_char,
        selection_type_word,
        selection_type_line
};

/* Until the selection can be generated on demand, let's not enable this on stable */
#include "vte/vteversion.h"
#if (VTE_MINOR_VERSION % 2) == 0
#undef HTML_SELECTION
#else
#define HTML_SELECTION
#endif

/* For unified handling of PRIMARY and CLIPBOARD selection */
typedef enum {
	VTE_SELECTION_PRIMARY,
	VTE_SELECTION_CLIPBOARD,
	LAST_VTE_SELECTION
} VteSelection;

/* Used in the GtkClipboard API, to distinguish requests for HTML and TEXT
 * contents of a clipboard */
typedef enum {
        VTE_TARGET_TEXT,
        VTE_TARGET_HTML,
        LAST_VTE_TARGET
} VteSelectionTarget;

struct selection_event_coords {
        long x, y;
};

struct vte_scrolling_region {
        int start, end;
};

/* Terminal private data. */
class VteTerminalPrivate {
public:
        VteTerminalPrivate() { }
        ~VteTerminalPrivate() { }
public:
        /* Metric and sizing data: dimensions of the window */
        glong row_count;
        glong column_count;

	/* Emulation setup data. */
	struct _vte_matcher *matcher;	/* control sequence matcher */
        gboolean autowrap;              /* auto wraparound at right margin */
	int keypad_mode, cursor_mode;	/* these would be VteKeymodes, but we
					   need to guarantee its type */
	GHashTable *dec_saved;

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
        int utf8_ambiguous_width;
	struct _vte_iso2022_state *iso2022;
	_vte_incoming_chunk_t *incoming;/* pending bytestream */
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
	struct _VteScreen normal_screen, alternate_screen, *screen;

        /* Values we save along with the cursor */
        VteVisualPosition cursor;	/* relative to the insertion delta */
        gboolean reverse_mode;	/* reverse mode */
        gboolean origin_mode;	/* origin mode */
        gboolean sendrecv_mode;	/* sendrecv mode */
        gboolean insert_mode;	/* insert mode */
        gboolean linefeed_mode;	/* linefeed mode */
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
        VteCharacterReplacement character_replacements[2];  /* charsets in the G0 and G1 slots */
        VteCharacterReplacement *character_replacement;     /* pointer to the active one */


        /* Word chars */
        char *word_char_exceptions_string;
        gunichar *word_char_exceptions;
        gsize word_char_exceptions_len;

	/* Selection information. */
	gboolean has_selection;
	gboolean selecting;
	gboolean selecting_after_threshold;
	gboolean selecting_restart;
	gboolean selecting_had_delta;
	gboolean selection_block_mode;
	enum vte_selection_type selection_type;
	struct selection_event_coords selection_origin, selection_last;
	VteVisualPosition selection_start, selection_end;

	/* Clipboard data information. */
	char *selection_text[LAST_VTE_SELECTION];
#ifdef HTML_SELECTION
	char *selection_html[LAST_VTE_SELECTION];
#endif
	GtkClipboard *clipboard[LAST_VTE_SELECTION];

	/* Miscellaneous options. */
	VteEraseBinding backspace_binding, delete_binding;
	gboolean meta_sends_escape;
	gboolean audible_bell;
	gboolean margin_bell;
	guint bell_margin;
	gboolean allow_bold;
        gboolean deccolm_mode; /* DECCOLM allowed */
	GHashTable *tabstops;
	gboolean text_modified_flag;
	gboolean text_inserted_flag;
	gboolean text_deleted_flag;
	gboolean rewrap_on_resize;
	gboolean bracketed_paste_mode;

	/* Scrolling options. */
	gboolean scroll_background;
	gboolean scroll_on_output;
	gboolean scroll_on_keystroke;
	gboolean alternate_screen_scroll;
	long scrollback_lines;

        /* Restricted scrolling */
        struct vte_scrolling_region scrolling_region;     /* the region we scroll in */
        gboolean scrolling_restricted;

	/* Cursor shape, as set via API */
	VteCursorShape cursor_shape;
        float cursor_aspect_ratio;

	/* Cursor blinking, as set in dconf. */
        VteCursorBlinkMode cursor_blink_mode;
	gboolean cursor_blink_state;
	guint cursor_blink_tag;           /* cursor blinking timeout ID */
        gint cursor_blink_cycle;          /* gtk-cursor-blink-time / 2 */
	gint cursor_blink_timeout;        /* gtk-cursor-blink-timeout */
        gboolean cursor_blinks;           /* whether the cursor is actually blinking */
	gint64 cursor_blink_time;         /* how long the cursor has been blinking yet */
	gboolean cursor_visible;
	gboolean has_focus;               /* is the terminal window focused */

        /* DECSCUSR cursor style (shape and blinking possibly overridden
         * via escape sequence) */
        VteCursorStyle cursor_style;

	/* Input device options. */
        gboolean input_enabled;
	time_t last_keypress_time;

	int mouse_tracking_mode; /* this is of type MouseTrackingMode,
				    but we need to guarantee its type. */
        guint mouse_pressed_buttons;      /* bits 0, 1, 2 resp. for buttons 1, 2, 3 */
        guint mouse_handled_buttons;      /* similar bitmap for buttons we handled ourselves */
	long mouse_last_x, mouse_last_y;
        long mouse_last_col, mouse_last_row;
	gboolean mouse_autohide;
	guint mouse_autoscroll_tag;
	gboolean mouse_xterm_extension;
	gboolean mouse_urxvt_extension;
	double mouse_smooth_scroll_delta;

        gboolean focus_tracking_mode;

	/* State variables for handling match checks. */
	char *match_contents;
	GArray *match_attributes;
        VteRegexMode match_regex_mode;
	GArray *match_regexes;
	char *match;
	int match_tag;
	VteVisualPosition match_start, match_end;
	gboolean show_match;

	/* Search data. */
        struct vte_regex_and_flags search_regex;
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
	guint modifiers;

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
