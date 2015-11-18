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

#include "vtetypes.hh"
#include "ring.h"
#include "vteconv.h"
#include "buffer.h"

#ifdef WITH_PCRE2
#include "vtepcre2.h"
#include "vteregexinternal.hh"
#endif

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
        VteTerminalPrivate(VteTerminal *t);
        ~VteTerminalPrivate();

public:
        VteTerminal *m_terminal;
        GtkWidget *m_widget;

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

public:

        void invalidate(vte::grid::span s, bool block = false);
        void invalidate_cell(vte::grid::column_t column, vte::grid::row_t row);
        void invalidate_cells(vte::grid::column_t sc, int cc,
                              vte::grid::row_t sr, int rc);
        void invalidate_region(vte::grid::column_t sc, vte::grid::column_t ec,
                               vte::grid::row_t sr, vte::grid::row_t er,
                               bool block = false);
        void invalidate_all();

        void invalidate_cursor_once(bool periodic = false);
        void invalidate_cursor_periodic();
        void check_cursor_blink();
        void add_cursor_timeout();
        void remove_cursor_timeout();

        void widget_paste(GdkAtom board);
        void widget_copy(VteSelection sel);

        void widget_set_hadjustment(GtkAdjustment *adjustment);
        void widget_set_vadjustment(GtkAdjustment *adjustment);

        void widget_realize();
        void widget_unrealize();
        void widget_style_updated();
        void widget_focus_in(GdkEventFocus *event);
        void widget_focus_out(GdkEventFocus *event);
        bool widget_key_press(GdkEventKey *event);
        bool widget_key_release(GdkEventKey *event);
        bool widget_button_press(GdkEventButton *event);
        bool widget_button_release(GdkEventButton *event);
        void widget_enter(GdkEventCrossing *event);
        void widget_leave(GdkEventCrossing *event);
        void widget_visibility_notify(GdkEventVisibility *event);
        void widget_scroll(GdkEventScroll *event);
        bool widget_motion_notify(GdkEventMotion *event);
        void widget_draw(cairo_t *cr);
        void widget_screen_changed (GdkScreen *previous_screen);
        void widget_get_preferred_width(int *minimum_width,
                                        int *natural_width);
        void widget_get_preferred_height(int *minimum_height,
                                         int *natural_height);
        void widget_size_allocate(GtkAllocation *allocation);

        void select_all();
        void deselect_all();

        void ensure_font();
        void update_font();

        void read_modifiers(GdkEvent *event);
        guint translate_ctrlkey(GdkEventKey *event);

        void set_pointer_visible(bool visible);

        void beep();

        void match_contents_clear();
        void match_contents_refresh();
        void set_cursor_from_regex_match(struct vte_match_regex *regex);
        void match_hilite_clear();
        bool cursor_inside_match(long x,
                                 long y);
        void match_hilite_show(long x,
                               long y);
        void match_hilite_hide();
        void match_hilite_update(long x,
                                 long y);
        void invalidate_match();
        void match_hilite(long x,
                          long y);

        bool regex_match_check_extra(GdkEvent *event,
                                     VteRegex **regexes,
                                     gsize n_regexes,
                                     guint32 match_flags,
                                     char **matches);
        bool regex_match_check_extra(GdkEvent *event,
                                     GRegex **regexes,
                                     gsize n_regexes,
                                     GRegexMatchFlags match_flags,
                                     char **matches);

        int regex_match_add(struct vte_match_regex *new_regex_match);
        struct vte_match_regex *regex_match_get(int tag);
        char *regex_match_check(vte::grid::column_t column,
                                vte::grid::row_t row,
                                int *tag);
        char *regex_match_check(GdkEvent *event,
                                int *tag);
        void regex_match_remove(int tag);
        void regex_match_remove_all();
        void regex_match_set_cursor(int tag,
                                    GdkCursor *gdk_cursor);
        void regex_match_set_cursor(int tag,
                                    GdkCursorType cursor_type);
        void regex_match_set_cursor(int tag,
                                    char const* cursor_name);
        bool match_rowcol_to_offset(vte::grid::column_t column,
                                    vte::grid::row_t row,
                                    gsize *offset_ptr,
                                    gsize *sattr_ptr,
                                    gsize *eattr_ptr);
#ifdef WITH_PCRE2
        pcre2_match_context_8 *create_match_context();
        bool match_check_pcre(pcre2_match_data_8 *match_data,
                              pcre2_match_context_8 *match_context,
                              VteRegex *regex,
                              guint32 match_flags,
                              gsize sattr,
                              gsize eattr,
                              gsize offset,
                              char **result,
                              gsize *start,
                              gsize *end,
                              gsize *sblank_ptr,
                              gsize *eblank_ptr);
        char *match_check_internal_pcre(vte::grid::column_t column,
                                        vte::grid::row_t row,
                                        int *tag,
                                        gsize *start,
                                        gsize *end);
#endif
        bool match_check_gregex(GRegex *regex,
                                GRegexMatchFlags match_flags,
                                gsize sattr,
                                gsize eattr,
                                gsize offset,
                                char **result,
                                gsize *start,
                                gsize *end,
                                gsize *sblank_ptr,
                                gsize *eblank_ptr);
        char *match_check_internal_gregex(vte::grid::column_t column,
                                          vte::grid::row_t row,
                                          int *tag,
                                          gsize *start,
                                          gsize *end);

        char *match_check_internal(vte::grid::column_t column,
                                   vte::grid::row_t row,
                                   int *tag,
                                   gsize *start,
                                   gsize *end);

        bool mouse_pixels_to_grid (long x,
                                   long y,
                                   vte::grid::column_t *col,
                                   vte::grid::row_t *row);

        void feed_mouse_event(int button,
                              bool is_drag,
                              bool is_release,
                              vte::grid::column_t col,
                              vte::grid::row_t row);
        void send_mouse_button_internal(int button,
                                        bool is_release,
                                        long x,
                                        long y);
        bool maybe_send_mouse_button(GdkEventButton *event);
        bool maybe_send_mouse_drag(GdkEventMotion *event);

        void feed_focus_event(bool in);
        void maybe_feed_focus_event(bool in);

#ifdef WITH_PCRE2
        bool search_set_regex (VteRegex *regex,
                               guint32 flags);
#endif
        bool search_set_gregex (GRegex *gregex,
                                GRegexMatchFlags gflags);

        bool search_rows(
#ifdef WITH_PCRE2
                         pcre2_match_context_8 *match_context,
                         pcre2_match_data_8 *match_data,
#endif
                         vte::grid::row_t start_row,
                         vte::grid::row_t end_row,
                         bool backward);
        bool search_rows_iter(
#ifdef WITH_PCRE2
                              pcre2_match_context_8 *match_context,
                              pcre2_match_data_8 *match_data,
#endif
                              vte::grid::row_t start_row,
                              vte::grid::row_t end_row,
                              bool backward);
        bool search_find(bool backward);
        bool search_set_wrap_around(bool wrap);

        void set_size(long columns,
                      long rows);

        bool process_word_char_exceptions(char const *str,
                                          gunichar **arrayp,
                                          gsize *lenp);

        long get_char_height() { ensure_font(); return char_height; }
        long get_char_width()  { ensure_font(); return char_width;  }

        bool set_audible_bell(bool setting);
        bool set_allow_bold(bool setting);
        bool set_backspace_binding(VteEraseBinding binding);
        bool set_cursor_blink_mode(VteCursorBlinkMode mode);
        bool set_cursor_shape(VteCursorShape shape);
        bool set_delete_binding(VteEraseBinding binding);
        bool set_encoding(char const* codeset);
        bool set_font_desc(PangoFontDescription const* desc);
        bool set_font_scale(double scale);
        bool set_input_enabled(bool enabled);
        bool set_mouse_autohide(bool autohide);
        bool set_pty(VtePty *pty);
        bool set_rewrap_on_resize(bool rewrap);
        bool set_scrollback_lines(long lines);
        bool set_scroll_on_keystroke(bool scroll);
        bool set_scroll_on_output(bool scroll);
        bool set_word_char_exceptions(char const* exceptions);

        bool write_contents_sync (GOutputStream *stream,
                                  VteWriteFlags flags,
                                  GCancellable *cancellable,
                                  GError **error);
};

#define m_invalidated_all invalidated_all
#define m_column_count column_count
#define m_row_count row_count
#define m_padding padding
#define m_char_width char_width
#define m_char_height char_height
#define m_active active
#define m_update_regions update_regions
#define m_draw draw
#define m_cursor_blinks cursor_blinks
#define m_cursor_visible cursor_visible
#define m_cursor cursor
#define m_cursor_blink_state cursor_blink_state
#define m_cursor_blink_time cursor_blink_time
#define m_cursor_blink_cycle cursor_blink_cycle
#define m_cursor_blink_timeout cursor_blink_timeout
#define m_cursor_blink_tag cursor_blink_tag
#define m_cursor_aspect_ratio cursor_aspect_ratio
#define m_unscaled_font_desc unscaled_font_desc
#define m_match_regexes match_regexes
#define m_match_attributes match_attributes
#define m_match_contents match_contents
#define m_match_regex_mode match_regex_mode
#define m_screen screen
#define m_mouse_tracking_mode mouse_tracking_mode
#define m_mouse_pressed_buttons mouse_pressed_buttons
#define m_mouse_last_column mouse_last_col
#define m_mouse_last_row mouse_last_row
#define m_mouse_xterm_extension mouse_xterm_extension
#define m_mouse_urxvt_extension mouse_urxvt_extension
#define m_modifiers modifiers
#define m_focus_tracking_mode focus_tracking_mode
#define m_match_start match_start
#define m_match_end match_end
#define m_match_tag match_tag
#define m_show_match show_match
#define m_match match
#define m_mouse_last_x mouse_last_x
#define m_mouse_last_y mouse_last_y
#define m_has_focus has_focus
#define m_im_context im_context
#define m_mouse_cursor_visible mouse_cursor_visible
#define m_mouse_handled_buttons mouse_handled_buttons
#define m_mouse_autohide mouse_autohide
#define m_mouse_mousing_cursor mouse_mousing_cursor
#define m_mouse_default_cursor mouse_default_cursor
#define m_mouse_inviso_cursor mouse_inviso_cursor
#define m_audible_bell audible_bell
#define m_margin_bell margin_bell
#define m_bell_margin bell_margin
#define m_im_preedit_active im_preedit_active
#define m_input_enabled input_enabled
#define m_backspace_binding backspace_binding
#define m_delete_binding delete_binding
#define m_pty pty
#define m_normal_screen normal_screen
#define m_alternate_screen alternate_screen
#define m_meta_sends_escape meta_sends_escape
#define m_scroll_on_keystroke scroll_on_keystroke
#define m_scroll_on_output scroll_on_output
#define m_cursor_mode cursor_mode
#define m_keypad_mode keypad_mode
#define m_has_selection has_selection
#define m_selecting_restart selecting_restart
#define m_selecting_after_threshold selecting_after_threshold
#define m_selection_block_mode selection_block_mode
#define m_selecting selecting
#define m_visibility_state visibility_state
#define m_mouse_smooth_scroll_delta mouse_smooth_scroll_delta
#define m_vadjustment vadjustment
#define m_alternate_screen_scroll alternate_screen_scroll
#define m_draw draw
#define m_im_preedit_string im_preedit_string
#define m_im_preedit_attrs im_preedit_attrs
#define m_im_preedit_cursor im_preedit_cursor
#define m_fontdirty fontdirty
#define m_contents_changed_pending contents_changed_pending
#define m_cursor_moved_pending cursor_moved_pending
#define m_text_modified_flag text_modified_flag
#define m_text_inserted_flag text_inserted_flag
#define m_text_deleted_flag text_deleted_flag
#define m_im_preedit_active im_preedit_active
#define m_im_preedit im_preedit
#define m_hadjustment hadjustment
#define m_hscroll_policy hscroll_policy
#define m_vscroll_policy vscroll_policy
#define m_char_ascent char_ascent
#define m_char_descent char_descent
#define m_line_thickness line_thickness
#define m_underline_position underline_position
#define m_strikethrough_position strikethrough_position
#define m_character_replacements character_replacements
#define m_palette palette
#define m_utf8_ambiguous_width utf8_ambiguous_width
#define m_iso2022 iso2022
#define m_incoming incoming
#define m_pending pending
#define m_max_input_bytes max_input_bytes
#define m_outgoing outgoing
#define m_outgoing_conv outgoing_conv
#define m_conv_buffer conv_buffer
#define m_autowrap autowrap
#define m_sendrecv_mode sendrecv_mode
#define m_dec_saved dec_saved
#define m_matcher matcher
#define m_pty_input_source pty_input_source
#define m_pty_output_source pty_output_source
#define m_pty_pid pty_pid
#define m_scrollback_lines scrollback_lines
#define m_clipboard clipboard
#define m_meta_sends_escape meta_sends_escape
#define m_bell_margin bell_margin
#define m_allow_bold allow_bold
#define m_deccolm_mode deccolm_mode
#define m_rewrap_on_resize rewrap_on_resize
#define m_cursor_shape cursor_shape
#define m_search_regex search_regex
#define m_background_alpha background_alpha
#define m_font_scale font_scale
#define m_has_fonts has_fonts
#define m_encoding encoding
#define m_cursor_blink_mode cursor_blink_mode
#define m_cursor_style cursor_style
#define m_character_replacement character_replacement
#define m_fontdesc fontdesc
#define m_search_attrs search_attrs
#define m_adjustment_changed_pending adjustment_changed_pending
#define m_tabstops tabstops
#define m_selection_text selection_text
#define m_clipboard clipboard
#define m_selection_html selection_html
#define m_child_watch_source child_watch_source
#define m_pty_channel pty_channel
#define m_window_title window_title
#define m_window_title_changed window_title_changed
#define m_icon_title_changed icon_title_changed
#define m_current_directory_uri_changed current_directory_uri_changed
#define m_current_directory_uri current_directory_uri
#define m_current_file_uri_changed current_file_uri_changed
#define m_current_file_uri current_file_uri
#define m_word_char_exceptions_string word_char_exceptions_string
#define m_word_char_exceptions word_char_exceptions
#define m_word_char_exceptions_len word_char_exceptions_len
#define m_icon_title icon_title
#define m_selection_start selection_start
#define m_selection_end selection_end
#define m_search_wrap_around search_wrap_around
#define m_input_bytes input_bytes
#define m_scrolling_restricted scrolling_restricted
#define m_selecting_had_delta selecting_had_delta

extern GTimer *process_timer;
