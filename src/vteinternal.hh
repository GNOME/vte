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

#include "vtedefines.hh"
#include "vtetypes.hh"
#include "vtedraw.hh"
#include "reaper.hh"
#include "ring.hh"
#include "buffer.h"
#include "parser.hh"
#include "parser-glue.hh"
#include "modes.hh"
#include "tabstops.hh"
#include "refptr.hh"

#include "vtepcre2.h"
#include "vteregexinternal.hh"

#include "chunk.hh"
#include "utf8.hh"

#include <list>
#include <queue>
#include <string>
#include <vector>

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

enum {
        VTE_XTERM_WM_RESTORE_WINDOW = 1,
        VTE_XTERM_WM_MINIMIZE_WINDOW = 2,
        VTE_XTERM_WM_SET_WINDOW_POSITION = 3,
        VTE_XTERM_WM_SET_WINDOW_SIZE_PIXELS = 4,
        VTE_XTERM_WM_RAISE_WINDOW = 5,
        VTE_XTERM_WM_LOWER_WINDOW = 6,
        VTE_XTERM_WM_REFRESH_WINDOW = 7,
        VTE_XTERM_WM_SET_WINDOW_SIZE_CELLS = 8,
        VTE_XTERM_WM_MAXIMIZE_WINDOW = 9,
        VTE_XTERM_WM_FULLSCREEN_WINDOW = 10,
        VTE_XTERM_WM_GET_WINDOW_STATE = 11,
        VTE_XTERM_WM_GET_WINDOW_POSITION = 13,
        VTE_XTERM_WM_GET_WINDOW_SIZE_PIXELS = 14,
        VTE_XTERM_WM_GET_WINDOW_SIZE_CELLS = 18,
        VTE_XTERM_WM_GET_SCREEN_SIZE_CELLS = 19,
        VTE_XTERM_WM_GET_ICON_TITLE = 20,
        VTE_XTERM_WM_GET_WINDOW_TITLE = 21,
        VTE_XTERM_WM_TITLE_STACK_PUSH = 22,
        VTE_XTERM_WM_TITLE_STACK_POP = 23,
};

enum {
        VTE_SGR_COLOR_SPEC_RGB    = 2,
        VTE_SGR_COLOR_SPEC_LEGACY = 5
};

struct vte_regex_and_flags {
        VteRegex *regex;
        guint32 match_flags;
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

typedef struct _VtePaletteColor {
	struct {
		vte::color::rgb color;
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

struct VteScreen {
public:
        VteScreen(gulong max_rows,
                  bool has_streams) :
                m_ring{max_rows, has_streams},
                row_data(&m_ring),
                cursor{0,0}
        {
        }

        vte::base::Ring m_ring; /* buffer contents */
        VteRing* row_data;
        VteVisualPosition cursor;  /* absolute value, from the beginning of the terminal history */
        double scroll_delta{0.0}; /* scroll offset */
        long insert_delta{0}; /* insertion offset */

        /* Stuff saved along with the cursor */
        struct {
                VteVisualPosition cursor;  /* onscreen coordinate, that is, relative to insert_delta */
                uint8_t modes_ecma;
                bool reverse_mode;
                bool origin_mode;
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

struct vte_scrolling_region {
        int start, end;
};

template <class T>
class ClipboardTextRequestGtk {
public:
        typedef void (T::* Callback)(char const*);

        ClipboardTextRequestGtk() : m_request(nullptr) { }
        ~ClipboardTextRequestGtk() { cancel(); }

        void request_text(GtkClipboard *clipboard,
                          Callback callback,
                          T* that)
        {
                cancel();
                new Request(clipboard, callback, that, &m_request);
        }

private:

        class Request {
        public:
                Request(GtkClipboard *clipboard,
                        Callback callback,
                        T* that,
                        Request** location) :
                        m_callback(callback),
                        m_that(that),
                        m_location(location)
                {
                        /* We need to store this here instead of doing it after the |new| above,
                         * since gtk_clipboard_request_text may dispatch the callback
                         * immediately or only later, with no way to know this beforehand.
                         */
                        *m_location = this;
                        gtk_clipboard_request_text(clipboard, text_received, this);
                }

                ~Request()
                {
                        invalidate();
                }

                void cancel()
                {
                        invalidate();
                        m_that = nullptr;
                        m_location = nullptr;
                }

        private:
                Callback m_callback;
                T *m_that;
                Request** m_location;

                void invalidate()
                {
                        if (m_that && m_location)
                                *m_location = nullptr;
                }

                void dispatch(char const *text)
                {
                        if (m_that) {
                                g_assert(m_location == nullptr || *m_location == this);

                                (m_that->*m_callback)(text);
                        }
                }

                static void text_received(GtkClipboard *clipboard, char const* text, gpointer data) {
                        Request* request = reinterpret_cast<Request*>(data);
                        request->dispatch(text);
                        delete request;
                }
        };

private:
        void cancel()
        {
                if (m_request)
                        m_request->cancel();
                g_assert(m_request == nullptr);
        }

        Request *m_request;
};

namespace vte {

namespace platform {
class Widget;
}

namespace terminal {

class Terminal {
public:
        Terminal(vte::platform::Widget* w,
                 VteTerminal *t);
        ~Terminal();

public:
        vte::platform::Widget* m_real_widget;
        VteTerminal *m_terminal;
        GtkWidget *m_widget;

        /* Metric and sizing data: dimensions of the window */
        vte::grid::row_t m_row_count{VTE_ROWS};
        vte::grid::column_t m_column_count{VTE_COLUMNS};

        vte::terminal::Tabstops m_tabstops{};

        vte::parser::Parser m_parser; /* control sequence state machine */

        vte::terminal::modes::ECMA m_modes_ecma{};
        vte::terminal::modes::Private m_modes_private{};

	/* PTY handling data. */
        VtePty *m_pty;
        GIOChannel *m_pty_channel;      /* master channel */
        guint m_pty_input_source;
        guint m_pty_output_source;
        gboolean m_pty_input_active;
        pid_t m_pty_pid{-1};           /* pid of child process */
        VteReaper *m_reaper;

	/* Queue of chunks of data read from the PTY.
         * Chunks are inserted at the back, and processed from the front.
         */
        std::queue<vte::base::Chunk::unique_type, std::list<vte::base::Chunk::unique_type>> m_incoming_queue;

        vte::base::UTF8Decoder m_utf8_decoder;
        bool m_using_utf8{true};
        const char *m_encoding;            /* the pty's encoding */
        int m_utf8_ambiguous_width;
        gunichar m_last_graphic_character; /* for REP */
        /* Array of dirty rectangles in view coordinates; need to
         * add allocation origin and padding when passing to gtk.
         */
        GArray *m_update_rects;
        gboolean m_invalidated_all;       /* pending refresh of entire terminal */
        /* If non-nullptr, contains the GList element for @this in g_active_terminals
         * and means that this terminal is processing data.
         */
        GList *m_active_terminals_link;
        // FIXMEchpe should these two be g[s]size ?
        size_t m_input_bytes;
        glong m_max_input_bytes;

	/* Output data queue. */
        VteByteArray *m_outgoing; /* pending input characters */

#ifdef WITH_ICONV
        /* Legacy charset support */
        GIConv m_incoming_conv{GIConv(-1)};
        VteByteArray* m_incoming_leftover;
        GIConv m_outgoing_conv{GIConv(-1)};
        VteByteArray *m_conv_buffer;

        void convert_incoming() noexcept;
#endif

	/* Screen data.  We support the normal screen, and an alternate
	 * screen, which seems to be a DEC-specific feature. */
        VteScreen m_normal_screen;
        VteScreen m_alternate_screen;
        VteScreen *m_screen; /* points to either m_normal_screen or m_alternate_screen */

        VteCell m_defaults;       /* default characteristics
                                     for insertion of any new
                                     characters */
        VteCell m_color_defaults; /* original defaults
                                     plus the current
                                     fore/back */
        VteCell m_fill_defaults;  /* original defaults
                                     plus the current
                                     fore/back with no
                                     character data */
        VteCharacterReplacement m_character_replacements[2];  /* charsets in the G0 and G1 slots */
        VteCharacterReplacement *m_character_replacement;     /* pointer to the active one */

        /* Word chars */
        std::string m_word_char_exceptions_string;
        std::u32string m_word_char_exceptions;

	/* Selection information. */
        gboolean m_selecting;
        gboolean m_will_select_after_threshold;
        gboolean m_selecting_had_delta;
        gboolean m_selection_block_mode;  // FIXMEegmont move it into a 4th value in vte_selection_type?
        enum vte_selection_type m_selection_type;
        vte::grid::halfcoords m_selection_origin, m_selection_last;
        vte::grid::span m_selection_resolved;

	/* Clipboard data information. */
        bool m_selection_owned[LAST_VTE_SELECTION];
        VteFormat m_selection_format[LAST_VTE_SELECTION];
        bool m_changing_selection;
        GString *m_selection[LAST_VTE_SELECTION];  // FIXMEegmont rename this so that m_selection_resolved can become m_selection?
        GtkClipboard *m_clipboard[LAST_VTE_SELECTION];

        ClipboardTextRequestGtk<Terminal> m_paste_request;

	/* Miscellaneous options. */
        VteEraseBinding m_backspace_binding;
        VteEraseBinding m_delete_binding;
        gboolean m_audible_bell;
        gboolean m_allow_bold;
        gboolean m_bold_is_bright;
        gboolean m_text_modified_flag;
        gboolean m_text_inserted_flag;
        gboolean m_text_deleted_flag;
        gboolean m_rewrap_on_resize;

	/* Scrolling options. */
        gboolean m_scroll_on_output;
        gboolean m_scroll_on_keystroke;
        vte::grid::row_t m_scrollback_lines;

        /* Restricted scrolling */
        struct vte_scrolling_region m_scrolling_region;     /* the region we scroll in */
        gboolean m_scrolling_restricted;

	/* Cursor shape, as set via API */
        VteCursorShape m_cursor_shape;
        double m_cursor_aspect_ratio;

	/* Cursor blinking, as set in dconf. */
        VteCursorBlinkMode m_cursor_blink_mode;
        gboolean m_cursor_blink_state;
        guint m_cursor_blink_tag;           /* cursor blinking timeout ID */
        gint m_cursor_blink_cycle;          /* gtk-cursor-blink-time / 2 */
        gint m_cursor_blink_timeout;        /* gtk-cursor-blink-timeout */
        gboolean m_cursor_blinks;           /* whether the cursor is actually blinking */
        gint64 m_cursor_blink_time;         /* how long the cursor has been blinking yet */
        gboolean m_has_focus;               /* is the terminal window focused */

        /* Contents blinking */
        VteTextBlinkMode m_text_blink_mode;
        gint m_text_blink_cycle;  /* gtk-cursor-blink-time / 2 */
        bool m_text_blink_state;  /* whether blinking text should be visible at this very moment */
        bool m_text_to_blink;     /* drawing signals here if it encounters any cell with blink attribute */
        guint m_text_blink_tag;   /* timeout ID for redrawing due to blinking */

        /* DECSCUSR cursor style (shape and blinking possibly overridden
         * via escape sequence) */
        VteCursorStyle m_cursor_style;

	/* Input device options. */
        gboolean m_input_enabled;
        time_t m_last_keypress_time;

        MouseTrackingMode m_mouse_tracking_mode{MOUSE_TRACKING_NONE};
        guint m_mouse_pressed_buttons;      /* bits 0, 1, 2 resp. for buttons 1, 2, 3 */
        guint m_mouse_handled_buttons;      /* similar bitmap for buttons we handled ourselves */
        /* The last known position the mouse pointer from an event. We don't store
         * this in grid coordinates because we want also to check if they were outside
         * the viewable area, and also want to catch in-cell movements if they make the pointer visible.
         */
        vte::view::coords m_mouse_last_position;
        guint m_mouse_autoscroll_tag;
        double m_mouse_smooth_scroll_delta{0.0};

	/* State variables for handling match checks. */
        char* m_match_contents;
        GArray* m_match_attributes;
        GArray* m_match_regexes;
        char* m_match;
        int m_match_tag;
        /* If m_match non-null, then m_match_span contains the region of the match.
         * If m_match is null, and m_match_span is not .empty(), then it contains
         * the minimal region around the last checked coordinates that don't contain
         * a match for any of the dingu regexes.
         */
        vte::grid::span m_match_span;

	/* Search data. */
        struct vte_regex_and_flags m_search_regex;
        gboolean m_search_wrap_around;
        GArray* m_search_attrs; /* Cache attrs */

	/* Data used when rendering the text which does not require server
	 * resources and which can be kept after unrealizing. */
        PangoFontDescription *m_unscaled_font_desc;
        PangoFontDescription *m_fontdesc;
        gdouble m_font_scale;
        gboolean m_fontdirty;

        /* First, the dimensions of ASCII characters are measured. The result
         * could probably be called char_{width,height} or font_{width,height}
         * but these aren't stored directly here, not to accidentally be confused
         * with m_cell_{width_height}. The values are stored in vtedraw's font_info.
         *
         * Then in case of nondefault m_cell_{width,height}_scale an additional
         * m_char_padding is added, resulting in m_cell_{width,height} which are
         * hence potentially larger than the characters. This is to implement
         * line spacing and letter spacing, primarly for accessibility (bug 781479).
         *
         * Char width/height, if really needed, can be computed by subtracting
         * the char padding from the cell dimensions. Char height can also be
         * reconstructed from m_char_{ascent,descent}, one of which is redundant,
         * stored for convenience only.
         */
        long m_char_ascent;
        long m_char_descent;
        double m_cell_width_scale;
        double m_cell_height_scale;
        GtkBorder m_char_padding;
        glong m_cell_width;
        glong m_cell_height;

	/* Data used when rendering the text which reflects server resources
	 * and data, which should be dropped when unrealizing and (re)created
	 * when realizing. */
        struct _vte_draw *m_draw;
        bool m_clear_background{true};

        VtePaletteColor m_palette[VTE_PALETTE_SIZE];

	/* Mouse cursors. */
        gboolean m_mouse_cursor_over_widget; /* as per enter and leave events */
        gboolean m_mouse_autohide;           /* the API setting */
        gboolean m_mouse_cursor_autohidden;  /* whether the autohiding logic wants to hide it; even if autohiding is disabled via API */

	/* Input method support. */
        bool m_im_preedit_active;
        std::string m_im_preedit;
        PangoAttrList *m_im_preedit_attrs;
        int m_im_preedit_cursor;

        gboolean m_accessible_emit;

        /* Adjustment updates pending. */
        gboolean m_adjustment_changed_pending;
        gboolean m_adjustment_value_changed_pending;
        gboolean m_cursor_moved_pending;
        gboolean m_contents_changed_pending;

        std::string m_window_title{};
        std::string m_current_directory_uri{};
        std::string m_current_file_uri{};
        std::string m_window_title_pending{};
        std::string m_current_directory_uri_pending{};
        std::string m_current_file_uri_pending{};
        bool m_window_title_changed{false};
        bool m_current_directory_uri_changed{false};
        bool m_current_file_uri_changed{false};

        std::vector<std::string> m_window_title_stack{};

	/* Background */
        double m_background_alpha;

        /* Bell */
        int64_t m_bell_timestamp;
        bool m_bell_pending{false};

	/* Key modifiers. */
        guint m_modifiers;

	/* Font stuff. */
        gboolean m_has_fonts;
        long m_line_thickness;
        long m_underline_position;
        long m_underline_thickness;
        long m_double_underline_position;
        long m_double_underline_thickness;
        double m_undercurl_position;
        double m_undercurl_thickness;
        long m_strikethrough_position;
        long m_strikethrough_thickness;
        long m_overline_position;
        long m_overline_thickness;
        long m_regex_underline_position;
        long m_regex_underline_thickness;

        /* Style stuff */
        GtkBorder m_padding;

        /* GtkScrollable impl */
        GtkAdjustment* m_hadjustment; /* unused */
        GtkAdjustment* m_vadjustment;
        guint m_hscroll_policy : 1; /* unused */
        guint m_vscroll_policy : 1;

        /* Hyperlinks */
        gboolean m_allow_hyperlink;
        vte::base::Ring::hyperlink_idx_t m_hyperlink_hover_idx;
        const char *m_hyperlink_hover_uri; /* data is owned by the ring */
        long m_hyperlink_auto_id;

public:

        // FIXMEchpe inline!
        /* inline */ VteRowData* ring_insert(vte::grid::row_t position,
                                       bool fill);
        /* inline */ VteRowData* ring_append(bool fill);
        /* inline */ void ring_remove(vte::grid::row_t position);
        inline VteRowData const* find_row_data(vte::grid::row_t row) const;
        inline VteRowData* find_row_data_writable(vte::grid::row_t row) const;
        inline VteCell const* find_charcell(vte::grid::column_t col,
                                            vte::grid::row_t row) const;
        inline vte::grid::column_t find_start_column(vte::grid::column_t col,
                                                     vte::grid::row_t row) const;
        inline vte::grid::column_t find_end_column(vte::grid::column_t col,
                                                   vte::grid::row_t row) const;

        inline vte::view::coord_t scroll_delta_pixel() const;
        inline vte::grid::row_t pixel_to_row(vte::view::coord_t y) const;
        inline vte::view::coord_t row_to_pixel(vte::grid::row_t row) const;
        inline vte::grid::row_t first_displayed_row() const;
        inline vte::grid::row_t last_displayed_row() const;

        inline VteRowData *insert_rows (guint cnt);
        VteRowData *ensure_row();
        VteRowData *ensure_cursor();
        void update_insert_delta();

        void cleanup_fragments(long start,
                               long end);

        void cursor_down(bool explicit_sequence);
        void drop_scrollback();

        void restore_cursor(VteScreen *screen__);
        void save_cursor(VteScreen *screen__);

        void insert_char(gunichar c,
                         bool insert,
                         bool invalidate_now);

        void invalidate_row(vte::grid::row_t row);
        void invalidate_rows(vte::grid::row_t row_start,
                             vte::grid::row_t row_end /* inclusive */);
        void invalidate(vte::grid::span const& s);
        void invalidate_symmetrical_difference(vte::grid::span const& a, vte::grid::span const& b, bool block);
        void invalidate_match_span();
        void invalidate_all();

        void reset_update_rects();
        bool invalidate_dirty_rects_and_process_updates();
        void time_process_incoming();
        void process_incoming();
        bool process(bool emit_adj_changed);
        inline bool is_processing() const { return m_active_terminals_link != nullptr; }
        void start_processing();

        gssize get_preedit_width(bool left_only);
        gssize get_preedit_length(bool left_only);

        void invalidate_cursor_once(bool periodic = false);
        void invalidate_cursor_periodic();
        void check_cursor_blink();
        void add_cursor_timeout();
        void remove_cursor_timeout();
        void update_cursor_blinks();
        VteCursorBlinkMode decscusr_cursor_blink();
        VteCursorShape decscusr_cursor_shape();

        void remove_text_blink_timeout();

        /* The allocation of the widget */
        cairo_rectangle_int_t m_allocated_rect;
        /* The usable view area. This is the allocation, minus the padding, but
         * including additional right/bottom area if the allocation is not grid aligned.
         */
        vte::view::extents m_view_usable_extents;

        void set_allocated_rect(cairo_rectangle_int_t const& r) { m_allocated_rect = r; update_view_extents(); }
        void update_view_extents() {
                m_view_usable_extents =
                        vte::view::extents(m_allocated_rect.width - m_padding.left - m_padding.right,
                                           m_allocated_rect.height - m_padding.top - m_padding.bottom);
        }

        bool widget_realized() const noexcept;
        inline cairo_rectangle_int_t const& get_allocated_rect() const { return m_allocated_rect; }
        inline vte::view::coord_t get_allocated_width() const { return m_allocated_rect.width; }
        inline vte::view::coord_t get_allocated_height() const { return m_allocated_rect.height; }

        vte::view::coords view_coords_from_event(GdkEvent const* event) const;
        vte::grid::coords grid_coords_from_event(GdkEvent const* event) const;

        vte::view::coords view_coords_from_grid_coords(vte::grid::coords const& rowcol) const;
        vte::grid::coords grid_coords_from_view_coords(vte::view::coords const& pos) const;

        vte::grid::halfcoords selection_grid_halfcoords_from_view_coords(vte::view::coords const& pos) const;
        bool view_coords_visible(vte::view::coords const& pos) const;
        bool grid_coords_visible(vte::grid::coords const& rowcol) const;

        inline bool grid_coords_in_scrollback(vte::grid::coords const& rowcol) const { return rowcol.row() < m_screen->insert_delta; }

        vte::grid::coords confine_grid_coords(vte::grid::coords const& rowcol) const;
        vte::grid::coords confined_grid_coords_from_event(GdkEvent const* event) const;
        vte::grid::coords confined_grid_coords_from_view_coords(vte::view::coords const& pos) const;

        void confine_coordinates(long *xp,
                                 long *yp);

        void widget_paste(GdkAtom board);
        void widget_copy(VteSelection sel,
                         VteFormat format);
        void widget_paste_received(char const* text);
        void widget_clipboard_cleared(GtkClipboard *clipboard);
        void widget_clipboard_requested(GtkClipboard *target_clipboard,
                                        GtkSelectionData *data,
                                        guint info);

        void widget_set_hadjustment(GtkAdjustment *adjustment);
        void widget_set_vadjustment(GtkAdjustment *adjustment);

        void widget_constructed();
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
        void widget_scroll(GdkEventScroll *event);
        bool widget_motion_notify(GdkEventMotion *event);
        void widget_draw(cairo_t *cr);
        void widget_get_preferred_width(int *minimum_width,
                                        int *natural_width);
        void widget_get_preferred_height(int *minimum_height,
                                         int *natural_height);
        void widget_size_allocate(GtkAllocation *allocation);

        void set_blink_settings(bool blink,
                                int blink_time,
                                int blink_timeout) noexcept;

        void expand_rectangle(cairo_rectangle_int_t& rect) const;
        void paint_area(GdkRectangle const* area);
        void paint_cursor();
        void paint_im_preedit_string();
        void draw_cells(struct _vte_draw_text_request *items,
                        gssize n,
                        uint32_t fore,
                        uint32_t back,
                        uint32_t deco,
                        bool clear,
                        bool draw_default_bg,
                        uint32_t attr,
                        bool hyperlink,
                        bool hilite,
                        int column_width,
                        int row_height);
        void fudge_pango_colors(GSList *attributes,
                                VteCell *cells,
                                gsize n);
        void apply_pango_attr(PangoAttribute *attr,
                              VteCell *cells,
                              gsize n_cells);
        void translate_pango_cells(PangoAttrList *attrs,
                                   VteCell *cells,
                                   gsize n_cells);
        void draw_cells_with_attributes(struct _vte_draw_text_request *items,
                                        gssize n,
                                        PangoAttrList *attrs,
                                        bool draw_default_bg,
                                        int column_width,
                                        int height);
        void draw_rows(VteScreen *screen,
                       vte::grid::row_t start_row,
                       long row_count,
                       gint start_y,
                       gint column_width,
                       gint row_height);

        bool autoscroll();
        void start_autoscroll();
        void stop_autoscroll();

        void connect_pty_read();
        void disconnect_pty_read();

        void connect_pty_write();
        void disconnect_pty_write();

        void pty_termios_changed();
        void pty_scroll_lock_changed(bool locked);

        void pty_channel_eof();
        bool pty_io_read(GIOChannel *channel,
                         GIOCondition condition);
        bool pty_io_write(GIOChannel *channel,
                          GIOCondition condition);

        void send_child(char const* data,
                        gssize length,
                        bool local_echo) noexcept;
        void feed_child_using_modes(char const* data,
                                    gssize length);

        void watch_child (pid_t child_pid);
        bool terminate_child () noexcept;
        void child_watch_done(pid_t pid,
                              int status);

        void im_commit(char const* text);
        void im_preedit_set_active(bool active) noexcept;
        void im_preedit_reset() noexcept;
        void im_preedit_changed(char const* str,
                                int cursorpos,
                                PangoAttrList* attrs) noexcept;
        bool im_retrieve_surrounding();
        bool im_delete_surrounding(int offset,
                                   int n_chars);
        void im_reset();
        void im_update_cursor();

        bool spawn_sync(VtePtyFlags pty_flags,
                        const char *working_directory,
                        char **argv,
                        char **envv,
                        GSpawnFlags spawn_flags_,
                        GSpawnChildSetupFunc child_setup,
                        gpointer child_setup_data,
                        GPid *child_pid /* out */,
                        GCancellable *cancellable,
                        GError **error);
#if 0
        void spawn_async(VtePtyFlags pty_flags,
                         const char *working_directory,
                         char **argv,
                         char **envv,
                         GSpawnFlags spawn_flags_,
                         GSpawnChildSetupFunc child_setup,
                         gpointer child_setup_data,
                         GDestroyNotify child_setup_data_destroy,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data);
        bool spawn_finish(GAsyncResult *result,
                          GPid *child_pid /* out */);
#endif

        void reset(bool clear_tabstops,
                   bool clear_history,
                   bool from_api = false);

        void feed(char const* data,
                  gssize length,
                  bool start_processsing_ = true);
        void feed_child(char const *text,
                        gssize length);
        void feed_child_binary(guint8 const* data,
                               gsize length);

        bool is_word_char(gunichar c) const;
        bool is_same_class(vte::grid::column_t acol,
                           vte::grid::row_t arow,
                           vte::grid::column_t bcol,
                           vte::grid::row_t brow) const;

        inline bool line_is_wrappable(vte::grid::row_t row) const;

        GString* get_text(vte::grid::row_t start_row,
                          vte::grid::column_t start_col,
                          vte::grid::row_t end_row,
                          vte::grid::column_t end_col,
                          bool block,
                          bool wrap,
                          GArray* attributes = nullptr);

        GString* get_text_displayed(bool wrap,
                                    GArray* attributes = nullptr);

        GString* get_text_displayed_a11y(bool wrap,
                                         GArray* attributes = nullptr);

        GString* get_selected_text(GArray* attributes = nullptr);

        template<unsigned int redbits, unsigned int greenbits, unsigned int bluebits>
        inline void rgb_from_index(guint index,
                                   vte::color::rgb& color) const;
        inline void determine_colors(VteCellAttr const* attr,
                                     bool selected,
                                     bool cursor,
                                     guint *pfore,
                                     guint *pback,
                                     guint *pdeco) const;
        inline void determine_colors(VteCell const* cell,
                                     bool selected,
                                     guint *pfore,
                                     guint *pback,
                                     guint *pdeco) const;
        inline void determine_cursor_colors(VteCell const* cell,
                                            bool selected,
                                            guint *pfore,
                                            guint *pback,
                                            guint *pdeco) const;

        char *cellattr_to_html(VteCellAttr const* attr,
                               char const* text) const;
        VteCellAttr const* char_to_cell_attr(VteCharAttributes const* attr) const;

        GString* attributes_to_html(GString* text_string,
                                    GArray* attrs);

        void start_selection(vte::view::coords const& pos,
                             enum vte_selection_type type);
        bool maybe_end_selection();

        void select_all();
        void deselect_all();

        vte::grid::coords resolve_selection_endpoint(vte::grid::halfcoords const& rowcolhalf, bool after) const;
        void resolve_selection();
        void selection_maybe_swap_endpoints(vte::view::coords const& pos);
        void modify_selection(vte::view::coords const& pos);
        bool cell_is_selected(vte::grid::column_t col,
                              vte::grid::row_t) const;

        void reset_default_attributes(bool reset_hyperlink);

        void ensure_font();
        void update_font();
        void apply_font_metrics(int cell_width,
                                int cell_height,
                                int char_ascent,
                                int char_descent,
                                GtkBorder char_spacing);

        void refresh_size();
        void screen_set_size(VteScreen *screen_,
                             long old_columns,
                             long old_rows,
                             bool do_rewrap);

        void vadjustment_value_changed();

        void read_modifiers(GdkEvent *event);
        guint translate_ctrlkey(GdkEventKey *event);

        void apply_mouse_cursor();
        void set_pointer_autohidden(bool autohidden);

        void beep();

        void emit_adjustment_changed();
        void emit_commit(char const* text,
                         gssize length);
        void emit_eof();
        void emit_selection_changed();
        void queue_adjustment_changed();
        void queue_adjustment_value_changed(double v);
        void queue_adjustment_value_changed_clamped(double v);
        void adjust_adjustments();
        void adjust_adjustments_full();

        void scroll_lines(long lines);
        void scroll_pages(long pages) { scroll_lines(pages * m_row_count); }
        void maybe_scroll_to_top();
        void maybe_scroll_to_bottom();

        void queue_cursor_moved();
        void queue_contents_changed();
        void queue_eof();

        void emit_text_deleted();
        void emit_text_inserted();
        void emit_text_modified();
        void emit_text_scrolled(long delta);
        void emit_pending_signals();
        void emit_char_size_changed(int width,
                                    int height);
        void emit_increase_font_size();
        void emit_decrease_font_size();
        void emit_bell();
        void emit_deiconify_window();
        void emit_iconify_window();
        void emit_raise_window();
        void emit_lower_window();
        void emit_maximize_window();
        void emit_refresh_window();
        void emit_restore_window();
        void emit_move_window(guint x,
                              guint y);
        void emit_resize_window(guint columns,
                                guint rows);
        void emit_copy_clipboard();
        void emit_paste_clipboard();
        void emit_hyperlink_hover_uri_changed(const GdkRectangle *bbox);

        void hyperlink_invalidate_and_get_bbox(vte::base::Ring::hyperlink_idx_t idx, GdkRectangle *bbox);
        void hyperlink_hilite_update();

        void match_contents_clear();
        void match_contents_refresh();
        void set_cursor_from_regex_match(struct vte_match_regex *regex);
        void match_hilite_clear();
        void match_hilite_update();

        bool rowcol_from_event(GdkEvent *event,
                               long *column,
                               long *row);

        char *hyperlink_check(GdkEvent *event);

        bool regex_match_check_extra(GdkEvent *event,
                                     VteRegex **regexes,
                                     gsize n_regexes,
                                     guint32 match_flags,
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

        char *match_check_internal(vte::grid::column_t column,
                                   vte::grid::row_t row,
                                   int *tag,
                                   gsize *start,
                                   gsize *end);

        bool feed_mouse_event(vte::grid::coords const& unconfined_rowcol,
                              int button,
                              bool is_drag,
                              bool is_release);
        bool maybe_send_mouse_button(vte::grid::coords const& rowcol,
                                     GdkEventType event_type,
                                     int event_button);
        bool maybe_send_mouse_drag(vte::grid::coords const& rowcol,
                                   GdkEventType event_type);

        void feed_focus_event(bool in);
        void feed_focus_event_initial();
        void maybe_feed_focus_event(bool in);

        bool search_set_regex (VteRegex *regex,
                               guint32 flags);

        bool search_rows(pcre2_match_context_8 *match_context,
                         pcre2_match_data_8 *match_data,
                         vte::grid::row_t start_row,
                         vte::grid::row_t end_row,
                         bool backward);
        bool search_rows_iter(pcre2_match_context_8 *match_context,
                              pcre2_match_data_8 *match_data,
                              vte::grid::row_t start_row,
                              vte::grid::row_t end_row,
                              bool backward);
        bool search_find(bool backward);
        bool search_set_wrap_around(bool wrap);

        void set_size(long columns,
                      long rows);

        bool process_word_char_exceptions(char const *str,
                                          std::u32string& array) const noexcept;

        long get_cell_height() { ensure_font(); return m_cell_height; }
        long get_cell_width()  { ensure_font(); return m_cell_width;  }

        vte::color::rgb const* get_color(int entry) const;
        void set_color(int entry,
                       int source,
                       vte::color::rgb const& proposed);
        void reset_color(int entry,
                         int source);

        bool set_audible_bell(bool setting);
        bool set_text_blink_mode(VteTextBlinkMode setting);
        bool set_allow_bold(bool setting);
        bool set_allow_hyperlink(bool setting);
        bool set_backspace_binding(VteEraseBinding binding);
        bool set_background_alpha(double alpha);
        bool set_bold_is_bright(bool setting);
        bool set_cell_height_scale(double scale);
        bool set_cell_width_scale(double scale);
        bool set_cjk_ambiguous_width(int width);
        void set_color_background(vte::color::rgb const &color);
        void set_color_bold(vte::color::rgb const& color);
        void reset_color_bold();
        void set_color_cursor_background(vte::color::rgb const& color);
        void reset_color_cursor_background();
        void set_color_cursor_foreground(vte::color::rgb const& color);
        void reset_color_cursor_foreground();
        void set_color_foreground(vte::color::rgb const& color);
        void set_color_highlight_background(vte::color::rgb const& color);
        void reset_color_highlight_background();
        void set_color_highlight_foreground(vte::color::rgb const& color);
        void reset_color_highlight_foreground();
        void set_colors(vte::color::rgb const *foreground,
                        vte::color::rgb const *background,
                        vte::color::rgb const *palette,
                        gsize palette_size);
        void set_colors_default();
        bool set_cursor_blink_mode(VteCursorBlinkMode mode);
        bool set_cursor_shape(VteCursorShape shape);
        bool set_cursor_style(VteCursorStyle style);
        bool set_delete_binding(VteEraseBinding binding);
        bool set_encoding(char const* codeset);
        bool set_font_desc(PangoFontDescription const* desc);
        bool set_font_scale(double scale);
        bool set_input_enabled(bool enabled);
        bool set_mouse_autohide(bool autohide);
        bool set_pty(VtePty *pty,
                     bool proces_remaining = true);
        bool set_rewrap_on_resize(bool rewrap);
        bool set_scrollback_lines(long lines);
        bool set_scroll_on_keystroke(bool scroll);
        bool set_scroll_on_output(bool scroll);
        bool set_word_char_exceptions(char const* exceptions);
        void set_clear_background(bool setting);

        bool write_contents_sync (GOutputStream *stream,
                                  VteWriteFlags flags,
                                  GCancellable *cancellable,
                                  GError **error);

        inline void ensure_cursor_is_onscreen();
        inline void home_cursor();
        inline void clear_screen();
        inline void clear_current_line();
        inline void clear_above_current();
        inline void scroll_text(vte::grid::row_t scroll_amount);
        inline void switch_screen(VteScreen *new_screen);
        inline void switch_normal_screen();
        inline void switch_alternate_screen();
        inline void save_cursor();
        inline void restore_cursor();

        inline void set_mode_ecma(vte::parser::Sequence const& seq,
                                  bool set) noexcept;
        inline void set_mode_private(vte::parser::Sequence const& seq,
                                     bool set) noexcept;
        inline void set_mode_private(int mode,
                                     bool set) noexcept;
        inline void save_mode_private(vte::parser::Sequence const& seq,
                                      bool save) noexcept;
        void update_mouse_protocol() noexcept;

        inline void set_character_replacements(unsigned slot,
                                               VteCharacterReplacement replacement);
        inline void set_character_replacement(unsigned slot);
        inline void clear_to_bol();
        inline void clear_below_current();
        inline void clear_to_eol();
        inline void delete_character();
        inline void set_cursor_column(vte::grid::column_t col);
        inline void set_cursor_column1(vte::grid::column_t col); /* 1-based */
        inline int get_cursor_column() const noexcept { return CLAMP(m_screen->cursor.col, 0, m_column_count - 1); }
        inline int get_cursor_column1() const noexcept { return get_cursor_column() + 1; }
        inline void set_cursor_row(vte::grid::row_t row /* relative to scrolling region */);
        inline void set_cursor_row1(vte::grid::row_t row /* relative to scrolling region */); /* 1-based */
        inline int get_cursor_row() const noexcept { return CLAMP(m_screen->cursor.row, 0, m_row_count - 1); }
        inline int get_cursor_row1() const noexcept { return get_cursor_row() + 1; }
        inline void set_cursor_coords(vte::grid::row_t row /* relative to scrolling region */,
                                      vte::grid::column_t column);
        inline void set_cursor_coords1(vte::grid::row_t row /* relative to scrolling region */,
                                       vte::grid::column_t column); /* 1-based */
        inline vte::grid::row_t get_cursor_row_unclamped() const;
        inline vte::grid::column_t get_cursor_column_unclamped() const;
        inline void move_cursor_up(vte::grid::row_t rows);
        inline void move_cursor_down(vte::grid::row_t rows);
        inline void erase_characters(long count);
        inline void insert_blank_character();

        template<unsigned int redbits, unsigned int greenbits, unsigned int bluebits>
        inline bool seq_parse_sgr_color(vte::parser::Sequence const& seq,
                                        unsigned int& idx,
                                        uint32_t& color) const noexcept;

        inline void move_cursor_backward(vte::grid::column_t columns);
        inline void move_cursor_forward(vte::grid::column_t columns);
        inline void move_cursor_tab_backward(int count = 1);
        inline void move_cursor_tab_forward(int count = 1);
        inline void line_feed();
        inline void erase_in_display(vte::parser::Sequence const& seq);
        inline void erase_in_line(vte::parser::Sequence const& seq);
        inline void insert_lines(vte::grid::row_t param);
        inline void delete_lines(vte::grid::row_t param);

        unsigned int checksum_area(vte::grid::row_t start_row,
                                   vte::grid::column_t start_col,
                                   vte::grid::row_t end_row,
                                   vte::grid::column_t end_col);

        void subscribe_accessible_events();
        void select_text(vte::grid::column_t start_col,
                         vte::grid::row_t start_row,
                         vte::grid::column_t end_col,
                         vte::grid::row_t end_row);
        void select_empty(vte::grid::column_t col,
                          vte::grid::row_t row);

        void send(vte::parser::u8SequenceBuilder const& builder,
                  bool c1 = true,
                  vte::parser::u8SequenceBuilder::Introducer introducer = vte::parser::u8SequenceBuilder::Introducer::DEFAULT,
                  vte::parser::u8SequenceBuilder::ST st = vte::parser::u8SequenceBuilder::ST::DEFAULT) noexcept;
        void send(vte::parser::Sequence const& seq,
                  vte::parser::u8SequenceBuilder const& builder) noexcept;
        void send(unsigned int type,
                  std::initializer_list<int> params) noexcept;
        void reply(vte::parser::Sequence const& seq,
                   unsigned int type,
                   std::initializer_list<int> params) noexcept;
        void reply(vte::parser::Sequence const& seq,
                   unsigned int type,
                   std::initializer_list<int> params,
                   vte::parser::ReplyBuilder const& builder) noexcept;
        #if 0
        void reply(vte::parser::Sequence const& seq,
                   unsigned int type,
                   std::initializer_list<int> params,
                   std::string const& str) noexcept;
        #endif
        void reply(vte::parser::Sequence const& seq,
                   unsigned int type,
                   std::initializer_list<int> params,
                   char const* format,
                   ...) noexcept G_GNUC_PRINTF(5, 6);

        /* OSC handler helpers */
        bool get_osc_color_index(int osc,
                                 int value,
                                 int& index) const noexcept;
        void set_color_index(vte::parser::Sequence const& seq,
                             vte::parser::StringTokeniser::const_iterator& token,
                             vte::parser::StringTokeniser::const_iterator const& endtoken,
                             int number,
                             int index,
                             int index_fallback,
                             int osc) noexcept;

        /* OSC handlers */
        void set_color(vte::parser::Sequence const& seq,
                       vte::parser::StringTokeniser::const_iterator& token,
                       vte::parser::StringTokeniser::const_iterator const& endtoken,
                       int osc) noexcept;
        void set_special_color(vte::parser::Sequence const& seq,
                               vte::parser::StringTokeniser::const_iterator& token,
                               vte::parser::StringTokeniser::const_iterator const& endtoken,
                               int index,
                               int index_fallback,
                               int osc) noexcept;
        void reset_color(vte::parser::Sequence const& seq,
                         vte::parser::StringTokeniser::const_iterator& token,
                         vte::parser::StringTokeniser::const_iterator const& endtoken,
                         int osc) noexcept;
        void set_current_directory_uri(vte::parser::Sequence const& seq,
                                       vte::parser::StringTokeniser::const_iterator& token,
                                       vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept;
        void set_current_file_uri(vte::parser::Sequence const& seq,
                                  vte::parser::StringTokeniser::const_iterator& token,
                                  vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept;
        void set_current_hyperlink(vte::parser::Sequence const& seq,
                                   vte::parser::StringTokeniser::const_iterator& token,
                                   vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept;

        /* Sequence handlers */
        bool m_line_wrapped; // signals line wrapped from character insertion
        // Note: inlining the handlers seems to worsen the performance, so we don't do that
#define _VTE_CMD(cmd) \
	/* inline */ void cmd (vte::parser::Sequence const& seq);
#define _VTE_NOP(cmd) G_GNUC_UNUSED _VTE_CMD(cmd)
#include "parser-cmd.hh"
#undef _VTE_CMD
#undef _VTE_NOP
};

} // namespace terminal
} // namespace vte

extern GTimer *process_timer;

vte::terminal::Terminal* _vte_terminal_get_impl(VteTerminal *terminal);

static inline bool
_vte_double_equal(double a,
                  double b)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        return a == b;
#pragma GCC diagnostic pop
}

#define VTE_TEST_FLAG_DECRQCRA (G_GUINT64_CONSTANT(1) << 0)

extern uint64_t g_test_flags;
