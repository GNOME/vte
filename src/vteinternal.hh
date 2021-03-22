/*
 * Copyright (C) 2001-2004 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

/* BEGIN sanity checks */

#ifndef __EXCEPTIONS
#error You MUST NOT use -fno-exceptions to build vte! Fix your build; and DO NOT file a bug upstream!
#endif

#ifndef __GXX_RTTI
#error You MUST NOT use -fno-rtti to build vte! Fix your build system; and DO NOT file a bug upstream!
#endif

/* END sanity checks */

#include <glib.h>
#include "glib-glue.hh"
#include "pango-glue.hh"

#include "debug.h"
#include "clipboard-gtk.hh"
#include "drawing-cairo.hh"
#include "vtedefines.hh"
#include "vtetypes.hh"
#include "reaper.hh"
#include "ring.hh"
#include "ringview.hh"
#include "buffer.h"
#include "parser.hh"
#include "parser-glue.hh"
#include "modes.hh"
#include "tabstops.hh"
#include "refptr.hh"
#include "fwd.hh"

#include "pcre2-glue.hh"
#include "vteregexinternal.hh"

#include "chunk.hh"
#include "pty.hh"
#include "utf8.hh"

#include <list>
#include <queue>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#ifdef WITH_ICU
#include "icu-converter.hh"
#endif

enum {
        VTE_BIDI_FLAG_IMPLICIT   = 1 << 0,
        VTE_BIDI_FLAG_RTL        = 1 << 1,
        VTE_BIDI_FLAG_AUTO       = 1 << 2,
        VTE_BIDI_FLAG_BOX_MIRROR = 1 << 3,
        VTE_BIDI_FLAG_ALL        = (1 << 4) - 1,
};

namespace vte {

namespace platform {

/*
 * Cursor:
 *
 * Holds a platform cursor. This is either a named cursor (string),
 * a reference to a GdkCursor*, or a cursor type.
 */
using Cursor = std::variant<std::string,
                            vte::glib::RefPtr<GdkCursor>,
                            GdkCursorType>;

} // namespace platform
} // namespace vte

typedef enum _VteCharacterReplacement {
        VTE_CHARACTER_REPLACEMENT_NONE,
        VTE_CHARACTER_REPLACEMENT_LINE_DRAWING
} VteCharacterReplacement;

typedef struct _VtePaletteColor {
	struct {
		vte::color::rgb color;
		gboolean is_set;
	} sources[2];
} VtePaletteColor;

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
                VteCharacterReplacement character_replacements[2];
                VteCharacterReplacement *character_replacement;
        } saved;
};

struct vte_scrolling_region {
        int start, end;
};

namespace vte {

namespace platform {
class Widget;
enum class ClipboardType;
}

namespace terminal {

class Terminal {
        friend class vte::platform::Widget;

private:
        class ProcessingContext;

        /* These correspond to the parameters for DECSCUSR (Set cursor style). */
        enum class CursorStyle {
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
                 * better than the VT510 designers!
                 */
                eTERMINAL_DEFAULT = 0,
                eBLINK_BLOCK      = 1,
                eSTEADY_BLOCK     = 2,
                eBLINK_UNDERLINE  = 3,
                eSTEADY_UNDERLINE = 4,
                /* *_IBEAM are xterm extensions */
                eBLINK_IBEAM      = 5,
                eSTEADY_IBEAM     = 6,
        };

        /* The order is important */
        enum class MouseTrackingMode {
	        eNONE,
                eSEND_XY_ON_CLICK,
                eSEND_XY_ON_BUTTON,
                eHILITE_TRACKING,
                eCELL_MOTION_TRACKING,
                eALL_MOTION_TRACKING,
        };

        enum class SelectionType {
               eCHAR,
               eWORD,
               eLINE,
        };

protected:

        /* NOTE: This needs to be kept in sync with the public VteCursorBlinkMode enum */
        enum class CursorBlinkMode {
                eSYSTEM,
                eON,
                eOFF
        };

        /* NOTE: This needs to be kept in sync with the public VteCursorShape enum */
        enum class CursorShape {
                eBLOCK,
                eIBEAM,
                eUNDERLINE
        };

        /* NOTE: This needs to be kept in sync with the public VteEraseMode enum */
        enum EraseMode {
                eAUTO,
                eASCII_BACKSPACE,
                eASCII_DELETE,
                eDELETE_SEQUENCE,
                eTTY,
        };

        /* NOTE: This needs to be kept in sync with the public VteTextBlinkMode enum */
        enum class TextBlinkMode {
                eNEVER     = 0,
                eFOCUSED   = 1,
                eUNFOCUSED = 2,
                eALWAYS    = 3
        };

public:
        Terminal(vte::platform::Widget* w,
                 VteTerminal *t);
        ~Terminal();

public:
        vte::platform::Widget* m_real_widget{nullptr};
        inline constexpr auto widget() const noexcept { return m_real_widget; }

        VteTerminal *m_terminal{nullptr};
        inline constexpr auto vte_terminal() const noexcept { return m_terminal; }

        GtkWidget *m_widget{nullptr};
        inline constexpr auto gtk_widget() const noexcept { return m_widget; }

        void unset_widget() noexcept;

        /* Metric and sizing data: dimensions of the window */
        vte::grid::row_t m_row_count{VTE_ROWS};
        vte::grid::column_t m_column_count{VTE_COLUMNS};

        vte::terminal::Tabstops m_tabstops{};

        vte::parser::Parser m_parser; /* control sequence state machine */

        vte::terminal::modes::ECMA m_modes_ecma{};
        vte::terminal::modes::Private m_modes_private{};

	/* PTY handling data. */
        vte::base::RefPtr<vte::base::Pty> m_pty{};
        inline constexpr auto& pty() const noexcept { return m_pty; }

        void unset_pty(bool notify_widget = true);
        bool set_pty(vte::base::Pty* pty);

        guint m_pty_input_source{0};
        guint m_pty_output_source{0};
        bool m_pty_input_active{false};
        pid_t m_pty_pid{-1};           /* pid of child process */
        int m_child_exit_status{-1};   /* pid's exit status, or -1 */
        bool m_eos_pending{false};
        bool m_child_exited_after_eos_pending{false};
        bool child_exited_eos_wait_callback();
        vte::glib::Timer m_child_exited_eos_wait_timer{std::bind(&Terminal::child_exited_eos_wait_callback,
                                                                 this),
                                                       "child-exited-eos-wait-timer"};
        VteReaper *m_reaper;

	/* Queue of chunks of data read from the PTY.
         * Chunks are inserted at the back, and processed from the front.
         */
        std::queue<vte::base::Chunk::unique_type, std::list<vte::base::Chunk::unique_type>> m_incoming_queue;

        vte::base::UTF8Decoder m_utf8_decoder;

        enum class DataSyntax {
                /* The primary data syntax is always one of the following: */
                ECMA48_UTF8,
                #ifdef WITH_ICU
                ECMA48_PCTERM,
                #endif
                /* ECMA48_ECMA35, not supported */

                /* The following can never be primary data syntax: */
        };

        DataSyntax m_primary_data_syntax{DataSyntax::ECMA48_UTF8};
        DataSyntax m_current_data_syntax{DataSyntax::ECMA48_UTF8};

        auto primary_data_syntax() const noexcept { return m_primary_data_syntax; }
        auto current_data_syntax() const noexcept { return m_current_data_syntax; }

        void push_data_syntax(DataSyntax syntax) noexcept
        {
                _vte_debug_print(VTE_DEBUG_IO, "Pushing data syntax %d -> %d\n",
                                 int(m_current_data_syntax), int(syntax));
                m_current_data_syntax = syntax;
        }

        void pop_data_syntax() noexcept
        {
                _vte_debug_print(VTE_DEBUG_IO, "Popping data syntax %d -> %d\n",
                                 int(m_current_data_syntax), int(m_primary_data_syntax));
                m_current_data_syntax = m_primary_data_syntax;
        }

        void reset_data_syntax();

        int m_utf8_ambiguous_width{VTE_DEFAULT_UTF8_AMBIGUOUS_WIDTH};
        gunichar m_last_graphic_character{0}; /* for REP */
        /* Array of dirty rectangles in view coordinates; need to
         * add allocation origin and padding when passing to gtk.
         */
        GArray *m_update_rects;
        bool m_invalidated_all{false};       /* pending refresh of entire terminal */
        /* If non-nullptr, contains the GList element for @this in g_active_terminals
         * and means that this terminal is processing data.
         */
        GList *m_active_terminals_link;
        // FIXMEchpe should these two be g[s]size ?
        size_t m_input_bytes;
        long m_max_input_bytes{VTE_MAX_INPUT_READ};

	/* Output data queue. */
        VteByteArray *m_outgoing; /* pending input characters */

#ifdef WITH_ICU
        /* Legacy charset support */
        std::unique_ptr<vte::base::ICUConverter> m_converter;
#endif /* WITH_ICU */

        char const* encoding() const noexcept
        {
                switch (primary_data_syntax()) {
                case DataSyntax::ECMA48_UTF8:   return "UTF-8";
                #ifdef WITH_ICU
                case DataSyntax::ECMA48_PCTERM: return m_converter->charset().c_str();
                #endif
                default: g_assert_not_reached(); return nullptr;
                }
        }

	/* Screen data.  We support the normal screen, and an alternate
	 * screen, which seems to be a DEC-specific feature. */
        VteScreen m_normal_screen;
        VteScreen m_alternate_screen;
        VteScreen *m_screen; /* points to either m_normal_screen or m_alternate_screen */

        VteCell m_defaults;        /* Default characteristics for insertion of new characters:
                                      colors (fore, back, deco) and other attributes (bold, italic,
                                      explicit hyperlink etc.). */
        VteCell m_color_defaults;  /* Default characteristics for erasing characters:
                                      colors (fore, back, deco) but no other attributes,
                                      and the U+0000 character that denotes erased cells. */

        /* charsets in the G0 and G1 slots */
        VteCharacterReplacement m_character_replacements[2] = { VTE_CHARACTER_REPLACEMENT_NONE,
                                                                VTE_CHARACTER_REPLACEMENT_NONE };
        /* pointer to the active one */
        VteCharacterReplacement *m_character_replacement{&m_character_replacements[0]};

        /* Word chars */
        std::vector<char32_t> m_word_char_exceptions;

	/* Selection information. */
        gboolean m_selecting;
        gboolean m_will_select_after_threshold;
        gboolean m_selecting_had_delta;
        bool m_selection_block_mode{false};  // FIXMEegmont move it into a 4th value in SelectionType?
        SelectionType m_selection_type{SelectionType::eCHAR};
        vte::grid::halfcoords m_selection_origin, m_selection_last;  /* BiDi: logical in normal modes, visual in m_selection_block_mode */
        vte::grid::span m_selection_resolved;

	/* Clipboard data information. */
        bool m_selection_owned[2]{false, false};
        bool m_changing_selection{false};
        vte::platform::ClipboardFormat m_selection_format[2];
        GString *m_selection[2];  // FIXMEegmont rename this so that m_selection_resolved can become m_selection?

	/* Miscellaneous options. */
        EraseMode m_backspace_binding{EraseMode::eAUTO};
        EraseMode m_delete_binding{EraseMode::eAUTO};
        bool m_audible_bell{true};
        bool m_allow_bold{true};
        bool m_bold_is_bright{false};
        bool m_rewrap_on_resize{true};
        gboolean m_text_modified_flag;
        gboolean m_text_inserted_flag;
        gboolean m_text_deleted_flag;

	/* Scrolling options. */
        bool m_fallback_scrolling{true};
        bool m_scroll_on_output{false};
        bool m_scroll_on_keystroke{true};
        guint m_scroll_speed;
        vte::grid::row_t m_scrollback_lines{0};

        /* Restricted scrolling */
        struct vte_scrolling_region m_scrolling_region;     /* the region we scroll in */
        gboolean m_scrolling_restricted;

	/* Cursor shape, as set via API */
        CursorShape m_cursor_shape{CursorShape::eBLOCK};
        double m_cursor_aspect_ratio{0.04};

	/* Cursor blinking */
        bool cursor_blink_timer_callback();
        vte::glib::Timer m_cursor_blink_timer{std::bind(&Terminal::cursor_blink_timer_callback,
                                                        this),
                                              "cursor-blink-timer"};
        CursorBlinkMode m_cursor_blink_mode{CursorBlinkMode::eSYSTEM};
        bool m_cursor_blink_state{false};
        bool m_cursor_blinks{false};           /* whether the cursor is actually blinking */
        gint m_cursor_blink_cycle;          /* gtk-cursor-blink-time / 2 */
        int m_cursor_blink_timeout{500};        /* gtk-cursor-blink-timeout */
        gint64 m_cursor_blink_time;         /* how long the cursor has been blinking yet */
        bool m_has_focus{false};            /* is the widget focused */

        /* Contents blinking */
        bool text_blink_timer_callback();
        vte::glib::Timer m_text_blink_timer{std::bind(&Terminal::text_blink_timer_callback,
                                                      this),
                                            "text-blink-timer"};
        bool m_text_blink_state{false};  /* whether blinking text should be visible at this very moment */
        bool m_text_to_blink{false};     /* drawing signals here if it encounters any cell with blink attribute */
        TextBlinkMode m_text_blink_mode{TextBlinkMode::eALWAYS};
        gint m_text_blink_cycle;  /* gtk-cursor-blink-time / 2 */

        /* DECSCUSR cursor style (shape and blinking possibly overridden
         * via escape sequence) */

        CursorStyle m_cursor_style{CursorStyle::eTERMINAL_DEFAULT};

	/* Input device options. */
        bool m_input_enabled{true};
        time_t m_last_keypress_time;

        MouseTrackingMode m_mouse_tracking_mode{MouseTrackingMode::eNONE};
        guint m_mouse_pressed_buttons;      /* bits 0, 1, 2 resp. for buttons 1, 2, 3 */
        guint m_mouse_handled_buttons;      /* similar bitmap for buttons we handled ourselves */
        /* The last known position the mouse pointer from an event. We don't store
         * this in grid coordinates because we want also to check if they were outside
         * the viewable area, and also want to catch in-cell movements if they make the pointer visible.
         */
        vte::view::coords m_mouse_last_position{-1, -1};
        double m_mouse_smooth_scroll_delta{0.0};
        bool mouse_autoscroll_timer_callback();
        vte::glib::Timer m_mouse_autoscroll_timer{std::bind(&Terminal::mouse_autoscroll_timer_callback,
                                                            this),
                                                  "mouse-autoscroll-timer"};

	/* State variables for handling match checks. */
        int m_match_regex_next_tag{0};
        auto regex_match_next_tag() noexcept { return m_match_regex_next_tag++; }

        class MatchRegex {
        public:
                MatchRegex() = default;
                MatchRegex(MatchRegex&&) = default;
                MatchRegex& operator= (MatchRegex&&) = default;

                MatchRegex(MatchRegex const&) = delete;
                MatchRegex& operator= (MatchRegex const&) = delete;

                MatchRegex(vte::base::RefPtr<vte::base::Regex>&& regex,
                           uint32_t match_flags,
                           vte::platform::Cursor&& cursor,
                           int tag = -1)
                        : m_regex{std::move(regex)},
                          m_match_flags{match_flags},
                          m_cursor{std::move(cursor)},
                          m_tag{tag}
                {
                }

                auto regex() const noexcept { return m_regex.get(); }
                auto match_flags() const noexcept { return m_match_flags; }
                auto const& cursor() const noexcept { return m_cursor; }
                auto tag() const noexcept { return m_tag; }

                void set_cursor(vte::platform::Cursor&& cursor) { m_cursor = std::move(cursor); }

        private:
                vte::base::RefPtr<vte::base::Regex> m_regex{};
                uint32_t m_match_flags{0};
                vte::platform::Cursor m_cursor{VTE_DEFAULT_CURSOR};
                int m_tag{-1};
        };

        MatchRegex const* m_match_current{nullptr};
        bool regex_match_has_current() const noexcept { return m_match_current != nullptr; }
        auto const* regex_match_current() const noexcept { return m_match_current; }

        std::vector<MatchRegex> m_match_regexes{};

        // m_match_current points into m_match_regex, so every write access to
        // m_match_regex must go through this function that clears m_current_match
        auto& match_regexes_writable() noexcept
        {
                match_hilite_clear();
                return m_match_regexes;
        }

        auto regex_match_get_iter(int tag) noexcept
        {
                return std::find_if(std::begin(m_match_regexes), std::end(m_match_regexes),
                                    [tag](MatchRegex const& rem) { return rem.tag() == tag; });
        }

        MatchRegex* regex_match_get(int tag) noexcept
        {
                auto i = regex_match_get_iter(tag);
                if (i == std::end(m_match_regexes))
                        return nullptr;

                return std::addressof(*i);
        }

        template<class... Args>
        auto& regex_match_add(Args&&... args)
        {
                return match_regexes_writable().emplace_back(std::forward<Args>(args)...);
        }

        char* m_match_contents;
        GArray* m_match_attributes;
        char* m_match;
        /* If m_match non-null, then m_match_span contains the region of the match.
         * If m_match is null, and m_match_span is not .empty(), then it contains
         * the minimal region around the last checked coordinates that don't contain
         * a match for any of the dingu regexes.
         */
        vte::grid::span m_match_span;

	/* Search data. */
        vte::base::RefPtr<vte::base::Regex> m_search_regex{};
        uint32_t m_search_regex_match_flags{0};
        gboolean m_search_wrap_around;
        GArray* m_search_attrs; /* Cache attrs */

	/* Data used when rendering the text which does not require server
	 * resources and which can be kept after unrealizing. */
        vte::Freeable<PangoFontDescription> m_api_font_desc{};
        vte::Freeable<PangoFontDescription> m_unscaled_font_desc{};
        vte::Freeable<PangoFontDescription> m_fontdesc{};
        double m_font_scale{1.};

        auto unscaled_font_description() const noexcept { return m_unscaled_font_desc.get(); }

        /* First, the dimensions of ASCII characters are measured. The result
         * could probably be called char_{width,height} or font_{width,height}
         * but these aren't stored directly here, not to accidentally be confused
         * with m_cell_{width_height}. The values are stored in FontInfo.
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
        long m_char_ascent{0};
        long m_char_descent{0};
        double m_cell_width_scale{1.};
        double m_cell_height_scale{1.};
        GtkBorder m_char_padding{0, 0, 0, 0};
        long m_cell_width{0};
        long m_cell_height{0};
        long m_cell_width_unscaled{0};
        long m_cell_height_unscaled{0};

        /* We allow the cell's text to draw a bit outside the cell at the top
         * and bottom. The following two functions return how much is the
         * maximally allowed overdraw (in px).
         */
        inline constexpr auto cell_overflow_top() const noexcept
        {
                /* Allow overdrawing up into the underline of the cell on top */
                return int(m_cell_height - m_underline_position);
        }

        inline constexpr auto cell_overflow_bottom() const noexcept
        {
                /* Allow overdrawing up into the overline of the cell on bottom */
                return int(m_overline_position + m_overline_thickness);
        }

	/* Data used when rendering */
        vte::view::DrawingContext m_draw{};
        bool m_clear_background{true};

        VtePaletteColor m_palette[VTE_PALETTE_SIZE];

	/* Mouse cursors. */
        gboolean m_mouse_cursor_over_widget; /* as per enter and leave events */
        gboolean m_mouse_autohide;           /* the API setting */
        gboolean m_mouse_cursor_autohidden;  /* whether the autohiding logic wants to hide it; even if autohiding is disabled via API */

	/* Input method support. */
        bool m_im_preedit_active;
        std::string m_im_preedit;
        vte::Freeable<PangoAttrList> m_im_preedit_attrs{};
        int m_im_preedit_cursor;

        #ifdef WITH_A11Y
        gboolean m_accessible_emit;
        #endif

        /* Adjustment updates pending. */
        gboolean m_adjustment_changed_pending;
        gboolean m_adjustment_value_changed_pending;
        gboolean m_cursor_moved_pending;
        gboolean m_contents_changed_pending;

        /* desktop notification */
        std::string m_notification_summary;
        std::string m_notification_body;

        std::string m_window_title{};
        std::string m_current_directory_uri{};
        std::string m_current_file_uri{};
        std::string m_window_title_pending{};
        std::string m_current_directory_uri_pending{};
        std::string m_current_file_uri_pending{};

        std::vector<std::string> m_window_title_stack{};

        enum class PendingChanges {
                TITLE = 1u << 0,
                CWD   = 1u << 1,
                CWF   = 1u << 2,
                NOTIFICATION = 1u << 3,
                SHELL_PREEXEC = 1u << 4,
                SHELL_PRECMD = 1u << 5,
        };
        unsigned m_pending_changes{0};

	/* Background */
        double m_background_alpha{1.};

        /* Bell */
        int64_t m_bell_timestamp;
        bool m_bell_pending{false};

	/* Key modifiers. */
        guint m_modifiers;

	/* Font stuff. */
        bool m_has_fonts{false};
        bool m_fontdirty{true};
        long m_line_thickness{VTE_LINE_WIDTH};
        long m_underline_position{0};
        long m_underline_thickness{VTE_LINE_WIDTH};
        long m_double_underline_position{0};
        long m_double_underline_thickness{VTE_LINE_WIDTH};
        long m_strikethrough_position{0};
        long m_strikethrough_thickness{VTE_LINE_WIDTH};
        long m_overline_position{0};
        long m_overline_thickness{VTE_LINE_WIDTH};
        long m_regex_underline_position{0};
        long m_regex_underline_thickness{VTE_LINE_WIDTH};
        double m_undercurl_position{0.};
        double m_undercurl_thickness{VTE_LINE_WIDTH};

        /* Style stuff */
        GtkBorder m_padding{1, 1, 1, 1};
        auto padding() const noexcept { return &m_padding; }

        vte::glib::RefPtr<GtkAdjustment> m_vadjustment{};
        auto vadjustment() noexcept { return m_vadjustment.get(); }

        /* Hyperlinks */
        bool m_allow_hyperlink{false};
        vte::base::Ring::hyperlink_idx_t m_hyperlink_hover_idx;
        const char *m_hyperlink_hover_uri; /* data is owned by the ring */
        long m_hyperlink_auto_id{0};

        /* RingView and friends */
        vte::base::RingView m_ringview;
        bool m_enable_bidi{true};
        bool m_enable_shaping{true};

        /* BiDi parameters outside of ECMA and DEC private modes */
        guint m_bidi_rtl : 1;

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
        inline bool cursor_is_onscreen() const noexcept;

        inline VteRowData *insert_rows (guint cnt);
        VteRowData *ensure_row();
        VteRowData *ensure_cursor();
        void update_insert_delta();

        void set_hard_wrapped(vte::grid::row_t row);
        void set_soft_wrapped(vte::grid::row_t row);

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
        void invalidate_row_and_context(vte::grid::row_t row);
        void invalidate_rows_and_context(vte::grid::row_t row_start,
                                         vte::grid::row_t row_end /* inclusive */);
        void invalidate(vte::grid::span const& s);
        void invalidate_symmetrical_difference(vte::grid::span const& a, vte::grid::span const& b, bool block);
        void invalidate_match_span();
        void invalidate_all();

        guint8 get_bidi_flags() const noexcept;
        void apply_bidi_attributes(vte::grid::row_t start, guint8 bidi_flags, guint8 bidi_flags_mask);
        void maybe_apply_bidi_attributes(guint8 bidi_flags_mask);

        void reset_update_rects();
        bool invalidate_dirty_rects_and_process_updates();
        void time_process_incoming();
        void process_incoming();
        void process_incoming_utf8(ProcessingContext& context,
                                   vte::base::Chunk& chunk);
        #ifdef WITH_ICU
        void process_incoming_pcterm(ProcessingContext& context,
                                     vte::base::Chunk& chunk);
        #endif
        bool process(bool emit_adj_changed);
        inline bool is_processing() const { return m_active_terminals_link != nullptr; }
        void start_processing();

        gssize get_preedit_width(bool left_only);
        gssize get_preedit_length(bool left_only);

        void invalidate_cursor_once(bool periodic = false);
        void check_cursor_blink();
        void add_cursor_timeout();
        void remove_cursor_timeout();
        void update_cursor_blinks();
        CursorBlinkMode decscusr_cursor_blink() const noexcept;
        CursorShape decscusr_cursor_shape() const noexcept;

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

        vte::view::coords view_coords_from_event(vte::platform::MouseEvent const& event) const;
        vte::grid::coords grid_coords_from_event(vte::platform::MouseEvent const& event) const;

        vte::view::coords view_coords_from_grid_coords(vte::grid::coords const& rowcol) const;
        vte::grid::coords grid_coords_from_view_coords(vte::view::coords const& pos) const;

        vte::grid::halfcoords selection_grid_halfcoords_from_view_coords(vte::view::coords const& pos) const;
        bool view_coords_visible(vte::view::coords const& pos) const;
        bool grid_coords_visible(vte::grid::coords const& rowcol) const;

        inline bool grid_coords_in_scrollback(vte::grid::coords const& rowcol) const { return rowcol.row() < m_screen->insert_delta; }

        vte::grid::row_t confine_grid_row(vte::grid::row_t const& row) const;
        vte::grid::coords confine_grid_coords(vte::grid::coords const& rowcol) const;
        vte::grid::coords confined_grid_coords_from_event(vte::platform::MouseEvent const&) const;
        vte::grid::coords confined_grid_coords_from_view_coords(vte::view::coords const& pos) const;

        void confine_coordinates(long *xp,
                                 long *yp);

        void set_border_padding(GtkBorder const* padding);
        void set_cursor_aspect(float aspect);

        void widget_copy(vte::platform::ClipboardType selection,
                         vte::platform::ClipboardFormat format);

        void widget_clipboard_text_received(vte::platform::Clipboard const& clipboard,
                                            std::string_view const& text);

        std::optional<std::string_view> widget_clipboard_data_get(vte::platform::Clipboard const& clipboard,
                                                                  vte::platform::ClipboardFormat format);
        void widget_clipboard_data_clear(vte::platform::Clipboard const& clipboard);

        void widget_set_vadjustment(vte::glib::RefPtr<GtkAdjustment>&& adjustment);

        void widget_realize();
        void widget_unrealize();
        void widget_unmap();
        void widget_style_updated();
        void widget_focus_in();
        void widget_focus_out();
        bool widget_key_press(vte::platform::KeyEvent const& event);
        bool widget_key_release(vte::platform::KeyEvent const& event);
        bool widget_mouse_motion(vte::platform::MouseEvent const& event);
        bool widget_mouse_press(vte::platform::MouseEvent const& event);
        bool widget_mouse_release(vte::platform::MouseEvent const& event);
        void widget_mouse_enter(vte::platform::MouseEvent const& event);
        void widget_mouse_leave(vte::platform::MouseEvent const& event);
        bool widget_mouse_scroll(vte::platform::ScrollEvent const& event);
        void widget_draw(cairo_t *cr);
        void widget_get_preferred_width(int *minimum_width,
                                        int *natural_width);
        void widget_get_preferred_height(int *minimum_height,
                                         int *natural_height);
        void widget_size_allocate(GtkAllocation *allocation);

        void set_blink_settings(bool blink,
                                int blink_time,
                                int blink_timeout) noexcept;

        void paint_cursor();
        void paint_im_preedit_string();
        void draw_cells(vte::view::DrawingContext::TextRequest* items,
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
        void draw_cells_with_attributes(vte::view::DrawingContext::TextRequest* items,
                                        gssize n,
                                        PangoAttrList *attrs,
                                        bool draw_default_bg,
                                        int column_width,
                                        int height);
        void draw_rows(VteScreen *screen,
                       cairo_region_t const* region,
                       vte::grid::row_t start_row,
                       long row_count,
                       gint start_y,
                       gint column_width,
                       gint row_height);

        void start_autoscroll();
        void stop_autoscroll() noexcept { m_mouse_autoscroll_timer.abort(); }

        void connect_pty_read();
        void disconnect_pty_read();

        void connect_pty_write();
        void disconnect_pty_write();

        void pty_termios_changed();
        void pty_scroll_lock_changed(bool locked);

        void pty_channel_eof();
        bool pty_io_read(int const fd,
                         GIOCondition const condition);
        bool pty_io_write(int const fd,
                          GIOCondition const condition);

        void send_child(std::string_view const& data);

        void watch_child (pid_t child_pid);
        bool terminate_child () noexcept;
        void child_watch_done(pid_t pid,
                              int status);
        void emit_child_exited();

        void im_commit(std::string_view const& str);
        void im_preedit_set_active(bool active) noexcept;
        void im_preedit_reset() noexcept;
        void im_preedit_changed(std::string_view const& str,
                                int cursorpos,
                                vte::Freeable<PangoAttrList> attrs) noexcept;
        bool im_retrieve_surrounding();
        bool im_delete_surrounding(int offset,
                                   int n_chars);
        void im_reset();
        void im_update_cursor();

        void reset(bool clear_tabstops,
                   bool clear_history,
                   bool from_api = false);
        void reset_decoder();

        void feed(std::string_view const& data,
                  bool start_processsing_ = true);
        void feed_child(char const* data,
                        size_t length) { assert(data); feed_child({data, length}); }
        void feed_child(std::string_view const& str);
        void feed_child_binary(std::string_view const& data);

        bool is_word_char(gunichar c) const;
        bool is_same_class(vte::grid::column_t acol,
                           vte::grid::row_t arow,
                           vte::grid::column_t bcol,
                           vte::grid::row_t brow) const;

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

        void resolve_normal_colors(VteCell const* cell,
                                   unsigned* pfore,
                                   unsigned* pback,
                                   vte::color::rgb& fg,
                                   vte::color::rgb& bg);

        char *cellattr_to_html(VteCellAttr const* attr,
                               char const* text) const;
        VteCellAttr const* char_to_cell_attr(VteCharAttributes const* attr) const;

        GString* attributes_to_html(GString* text_string,
                                    GArray* attrs);

        void start_selection(vte::view::coords const& pos,
                             SelectionType type);
        bool maybe_end_selection();

        void select_all();
        void deselect_all();

        vte::grid::coords resolve_selection_endpoint(vte::grid::halfcoords const& rowcolhalf, bool after) const;
        void resolve_selection();
        void selection_maybe_swap_endpoints(vte::view::coords const& pos);
        void modify_selection(vte::view::coords const& pos);
        bool cell_is_selected_log(vte::grid::column_t lcol,
                                  vte::grid::row_t) const;
        bool cell_is_selected_vis(vte::grid::column_t vcol,
                                  vte::grid::row_t) const;

        void reset_default_attributes(bool reset_hyperlink);

        void ensure_font();
        void update_font();
        void apply_font_metrics(int cell_width_unscaled,
                                int cell_height_unscaled,
                                int cell_width,
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

        unsigned translate_ctrlkey(vte::platform::KeyEvent const& event) const noexcept;

        void apply_mouse_cursor();
        void set_pointer_autohidden(bool autohidden);

        void beep();

        void emit_adjustment_changed();
        void emit_commit(std::string_view const& str);
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
        void queue_child_exited();
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

        bool m_xterm_wm_iconified{false};

        void emit_resize_window(guint columns,
                                guint rows);
        void emit_copy_clipboard();
        void emit_paste_clipboard();
        void emit_hyperlink_hover_uri_changed(const GdkRectangle *bbox);

        void hyperlink_invalidate_and_get_bbox(vte::base::Ring::hyperlink_idx_t idx, GdkRectangle *bbox);
        void hyperlink_hilite_update();

        void match_contents_clear();
        void match_contents_refresh();
        void match_hilite_clear();
        void match_hilite_update();

        bool rowcol_from_event(vte::platform::MouseEvent const& event,
                               long *column,
                               long *row);

        char *hyperlink_check(vte::platform::MouseEvent const& event);

        bool regex_match_check_extra(vte::platform::MouseEvent const& event,
                                     vte::base::Regex const** regexes,
                                     size_t n_regexes,
                                     uint32_t match_flags,
                                     char** matches);

        char *regex_match_check(vte::grid::column_t column,
                                vte::grid::row_t row,
                                int *tag);
        char *regex_match_check(vte::platform::MouseEvent const& event,
                                int *tag);
        void regex_match_remove(int tag) noexcept;
        void regex_match_remove_all() noexcept;
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

        vte::Freeable<pcre2_match_context_8> create_match_context();
        bool match_check_pcre(pcre2_match_data_8 *match_data,
                              pcre2_match_context_8 *match_context,
                              vte::base::Regex const* regex,
                              uint32_t match_flags,
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
                                        MatchRegex const** match,
                                        gsize *start,
                                        gsize *end);

        char *match_check_internal(vte::grid::column_t column,
                                   vte::grid::row_t row,
                                   MatchRegex const** match,
                                   gsize *start,
                                   gsize *end);

        bool feed_mouse_event(vte::grid::coords const& unconfined_rowcol,
                              int button,
                              bool is_drag,
                              bool is_release);
        bool maybe_send_mouse_button(vte::grid::coords const& rowcol,
                                     vte::platform::MouseEvent const& event);
        bool maybe_send_mouse_drag(vte::grid::coords const& rowcol,
                                   vte::platform::MouseEvent const& event);

        void feed_focus_event(bool in);
        void feed_focus_event_initial();
        void maybe_feed_focus_event(bool in);

        bool search_set_regex(vte::base::RefPtr<vte::base::Regex>&& regex,
                              uint32_t flags);
        auto search_regex() const noexcept { return m_search_regex.get(); }

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

        std::optional<std::vector<char32_t>> process_word_char_exceptions(std::string_view str) const noexcept;

        long get_cell_height() { ensure_font(); return m_cell_height; }
        long get_cell_width()  { ensure_font(); return m_cell_width;  }

        vte::color::rgb const* get_color(int entry) const;
        void set_color(int entry,
                       int source,
                       vte::color::rgb const& proposed);
        void reset_color(int entry,
                         int source);

        bool set_audible_bell(bool setting);
        bool set_text_blink_mode(TextBlinkMode setting);
        auto text_blink_mode() const noexcept { return m_text_blink_mode; }
        bool set_allow_bold(bool setting);
        bool set_allow_hyperlink(bool setting);
        bool set_backspace_binding(EraseMode binding);
        auto backspace_binding() const noexcept { return m_backspace_binding; }
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
        bool set_cursor_blink_mode(CursorBlinkMode mode);
        auto cursor_blink_mode() const noexcept { return m_cursor_blink_mode; }
        bool set_cursor_shape(CursorShape shape);
        auto cursor_shape() const noexcept { return m_cursor_shape; }
        bool set_cursor_style(CursorStyle style);
        bool set_delete_binding(EraseMode binding);
        auto delete_binding() const noexcept { return m_delete_binding; }
        bool set_enable_bidi(bool setting);
        bool set_enable_shaping(bool setting);
        bool set_encoding(char const* codeset,
                          GError** error);
        bool set_font_desc(vte::Freeable<PangoFontDescription> desc);
        bool update_font_desc();
        bool set_font_scale(double scale);
        bool set_input_enabled(bool enabled);
        bool set_mouse_autohide(bool autohide);
        bool set_rewrap_on_resize(bool rewrap);
        bool set_scroll_speed(unsigned int scroll_speed);
        bool set_scrollback_lines(long lines);
        bool set_fallback_scrolling(bool set);
        auto fallback_scrolling() const noexcept { return m_fallback_scrolling; }
        bool set_scroll_on_keystroke(bool scroll);
        bool set_scroll_on_output(bool scroll);
        bool set_images_enabled(bool enabled);
        bool set_word_char_exceptions(std::optional<std::string_view> stropt);
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
        inline void erase_characters(long count,
                                     bool use_basic = false);
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
        void handle_urxvt_extension(vte::parser::Sequence const& seq,
                                    vte::parser::StringTokeniser::const_iterator& token,
                                    vte::parser::StringTokeniser::const_iterator const& endtoken) noexcept;
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

        void ringview_update();

        /* Sequence handlers */
        bool m_line_wrapped; // signals line wrapped from character insertion
        // Note: inlining the handlers seems to worsen the performance, so we don't do that
#define _VTE_CMD_HANDLER(cmd) \
	/* inline */ void cmd (vte::parser::Sequence const& seq);
#define _VTE_CMD_HANDLER_NOP(cmd) \
	/* inline */ void cmd (vte::parser::Sequence const& seq);
#define _VTE_CMD_HANDLER_R(cmd) \
	/* inline */ bool cmd (vte::parser::Sequence const& seq);
#include "parser-cmd-handlers.hh"
#undef _VTE_CMD_HANDLER
#undef _VTE_CMD_HANDLER_NOP
#undef _VTE_CMD_HANDLER_R
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
