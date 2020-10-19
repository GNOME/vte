/*
 * Copyright © 2018 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "vteterminal.h"
#include "vtepty.h"

#include "vteinternal.hh"

#include "fwd.hh"

#include "clipboard-gtk.hh"
#include "regex.hh"
#include "refptr.hh"

namespace vte {

namespace terminal {

class Terminal;

} // namespace terminal

namespace platform {

class EventBase {
        friend class vte::platform::Widget;
        friend class Terminal;

public:
        enum class Type {
                eKEY_PRESS,
                eKEY_RELEASE,
                eMOUSE_DOUBLE_PRESS,
                eMOUSE_ENTER,
                eMOUSE_LEAVE,
                eMOUSE_MOTION,
                eMOUSE_PRESS,
                eMOUSE_RELEASE,
                eMOUSE_SCROLL,
                eMOUSE_TRIPLE_PRESS,
        };

protected:

        EventBase() noexcept = default;

        constexpr EventBase(GdkEvent* gdk_event,
                            Type type,
                            unsigned timestamp) noexcept
                : m_platform_event{gdk_event},
                  m_type{type},
                  m_timestamp{timestamp}
        {
        }

        constexpr auto platform_event() const noexcept { return m_platform_event; }

public:
        ~EventBase() noexcept = default;

        EventBase(EventBase const&) = default;
        EventBase(EventBase&&) = default;
        EventBase& operator=(EventBase const&) = delete;
        EventBase& operator=(EventBase&&) = delete;

        constexpr auto timestamp()   const noexcept { return m_timestamp;   }
        constexpr auto type()        const noexcept { return m_type;        }

private:
        GdkEvent* m_platform_event;
        Type m_type;
        unsigned m_timestamp;
}; // class EventBase

class KeyEvent : public EventBase {
        friend class vte::platform::Widget;
        friend class Terminal;

protected:

        KeyEvent() noexcept = default;

        constexpr KeyEvent(GdkEvent* gdk_event,
                           Type type,
                           unsigned timestamp,
                           unsigned modifiers,
                           unsigned keyval,
                           unsigned keycode,
                           uint8_t group,
                           bool is_modifier) noexcept
                : EventBase{gdk_event,
                            type,
                            timestamp},
                  m_modifiers{modifiers},
                  m_keyval{keyval},
                  m_keycode{keycode},
                  m_group{group},
                  m_is_modifier{is_modifier}
        {
        }

public:
        ~KeyEvent() noexcept = default;

        KeyEvent(KeyEvent const&) = delete;
        KeyEvent(KeyEvent&&) = delete;
        KeyEvent& operator=(KeyEvent const&) = delete;
        KeyEvent& operator=(KeyEvent&&) = delete;

        constexpr auto group()       const noexcept { return m_group;       }
        constexpr auto is_modifier() const noexcept { return m_is_modifier; }
        constexpr auto keycode()     const noexcept { return m_keycode;     }
        constexpr auto keyval()      const noexcept { return m_keyval;      }
        constexpr auto modifiers()   const noexcept { return m_modifiers;   }

        constexpr auto is_key_press()   const noexcept { return type() == Type::eKEY_PRESS;   }
        constexpr auto is_key_release() const noexcept { return type() == Type::eKEY_RELEASE; }

private:
        unsigned m_modifiers;
        unsigned m_keyval;
        unsigned m_keycode;
        uint8_t m_group;
        bool m_is_modifier;
}; // class KeyEvent

class MouseEvent : public EventBase {
        friend class vte::platform::Widget;
        friend class Terminal;

public:
        enum class Button {
                eNONE   = 0,
                eLEFT   = 1,
                eMIDDLE = 2,
                eRIGHT  = 3,
                eFOURTH = 4,
                eFIFTH  = 5,
        };

        enum class ScrollDirection {
                eUP,
                eDOWN,
                eLEFT,
                eRIGHT,
                eSMOOTH,
                eNONE,
        };

protected:

        MouseEvent() noexcept = default;

        constexpr MouseEvent(GdkEvent* gdk_event,
                             Type type,
                             unsigned timestamp,
                             unsigned modifiers,
                             Button button,
                             double x,
                             double y) noexcept
                : EventBase{gdk_event,
                            type,
                            timestamp},
                  m_modifiers{modifiers},
                  m_button{button},
                  m_x{x},
                  m_y{y}
        {
        }

public:
        ~MouseEvent() noexcept = default;

        MouseEvent(MouseEvent const&) = default;
        MouseEvent(MouseEvent&&) = default;
        MouseEvent& operator=(MouseEvent const&) = delete;
        MouseEvent& operator=(MouseEvent&&) = delete;

        constexpr auto button()       const noexcept { return m_button;           }
        constexpr auto button_value() const noexcept { return unsigned(m_button); }
        constexpr auto modifiers()    const noexcept { return m_modifiers;        }
        constexpr auto x()            const noexcept { return m_x;                }
        constexpr auto y()            const noexcept { return m_y;                }

        constexpr auto is_mouse_double_press() const noexcept { return type() == Type::eMOUSE_DOUBLE_PRESS; }
        constexpr auto is_mouse_enter()        const noexcept { return type() == Type::eMOUSE_ENTER;        }
        constexpr auto is_mouse_leave()        const noexcept { return type() == Type::eMOUSE_LEAVE;        }
        constexpr auto is_mouse_motion()       const noexcept { return type() == Type::eMOUSE_MOTION;       }
        constexpr auto is_mouse_press()        const noexcept { return type() == Type::eMOUSE_PRESS;      }
        constexpr auto is_mouse_release()      const noexcept { return type() == Type::eMOUSE_RELEASE;      }
        constexpr auto is_mouse_scroll()       const noexcept { return type() == Type::eMOUSE_SCROLL;       }
        constexpr auto is_mouse_single_press() const noexcept { return type() == Type::eMOUSE_PRESS;        }
        constexpr auto is_mouse_triple_press() const noexcept { return type() == Type::eMOUSE_TRIPLE_PRESS; }

        ScrollDirection scroll_direction() const noexcept
        {
                /* Note that we cannot use gdk_event_get_scroll_direction() here since it
                 * returns false for smooth scroll events.
                 */
                if (!is_mouse_scroll())
                        return ScrollDirection::eNONE;
                switch (reinterpret_cast<GdkEventScroll*>(platform_event())->direction) {
                case GDK_SCROLL_UP:     return ScrollDirection::eUP;
                case GDK_SCROLL_DOWN:   return ScrollDirection::eDOWN;
                case GDK_SCROLL_LEFT:   return ScrollDirection::eLEFT;
                case GDK_SCROLL_RIGHT:  return ScrollDirection::eRIGHT;
                case GDK_SCROLL_SMOOTH: return ScrollDirection::eSMOOTH;
                default: return ScrollDirection::eNONE;
                }
        }

        auto scroll_delta_x() const noexcept
        {
                auto delta = double{0.};
                gdk_event_get_scroll_deltas(platform_event(), &delta, nullptr);
                return delta;
        }

        auto scroll_delta_y() const noexcept
        {
                auto delta = double{0.};
                gdk_event_get_scroll_deltas(platform_event(), nullptr, &delta);
                return delta;
        }

private:
        unsigned m_modifiers;
        Button m_button;
        double m_x;
        double m_y;
}; // class MouseEvent

class Widget : public std::enable_shared_from_this<Widget> {
public:
        friend class vte::terminal::Terminal;

        Widget(VteTerminal* t);
        ~Widget() noexcept;

        Widget(Widget const&) = delete;
        Widget(Widget&&) = delete;
        Widget& operator= (Widget const&) = delete;
        Widget& operator= (Widget&&) = delete;

        GObject* object() const noexcept { return reinterpret_cast<GObject*>(m_widget); }
        GtkWidget* gtk() const noexcept { return m_widget; }
        VteTerminal* vte() const noexcept { return reinterpret_cast<VteTerminal*>(m_widget); }

        inline constexpr vte::terminal::Terminal* terminal() const noexcept { return m_terminal; }

        void constructed() noexcept;
        void dispose() noexcept;
        void realize() noexcept;
        void unrealize() noexcept;
        void map() noexcept;
        void unmap() noexcept;
        void style_updated() noexcept;
        void draw(cairo_t *cr) noexcept { m_terminal->widget_draw(cr); }
        void get_preferred_width(int *minimum_width,
                                 int *natural_width) const noexcept { m_terminal->widget_get_preferred_width(minimum_width, natural_width); }
        void get_preferred_height(int *minimum_height,
                                  int *natural_height) const noexcept { m_terminal->widget_get_preferred_height(minimum_height, natural_height); }
        void size_allocate(GtkAllocation *allocation) noexcept;

        void focus_in(GdkEventFocus *event) noexcept { m_terminal->widget_focus_in(); }
        void focus_out(GdkEventFocus *event) noexcept { m_terminal->widget_focus_out(); }
        bool key_press(GdkEventKey *event) noexcept { return m_terminal->widget_key_press(key_event_from_gdk(event)); }
        bool key_release(GdkEventKey *event) noexcept { return m_terminal->widget_key_release(key_event_from_gdk(event)); }
        bool button_press(GdkEventButton *event) noexcept { return m_terminal->widget_mouse_press(mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event))); }
        bool button_release(GdkEventButton *event) noexcept { return m_terminal->widget_mouse_release(mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event))); }
        void enter(GdkEventCrossing *event) noexcept { m_terminal->widget_mouse_enter(mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event))); }
        void leave(GdkEventCrossing *event) noexcept { m_terminal->widget_mouse_leave(mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event))); }
        bool scroll(GdkEventScroll *event) noexcept { return m_terminal->widget_mouse_scroll(mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event))); }
        bool motion_notify(GdkEventMotion *event) noexcept { return m_terminal->widget_mouse_motion(mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event))); }

        void grab_focus() noexcept { gtk_widget_grab_focus(gtk()); }

        bool primary_paste_enabled() const noexcept;

        Clipboard& clipboard_get(ClipboardType type) const;
        void clipboard_offer_data(ClipboardType type,
                                  ClipboardFormat format) noexcept;
        void clipboard_request_text(ClipboardType type) noexcept;
        void clipboard_set_text(ClipboardType type,
                                std::string_view const& str) noexcept;

        void paste(vte::platform::ClipboardType type) { clipboard_request_text(type); }
        void copy(vte::platform::ClipboardType type,
                  vte::platform::ClipboardFormat format) noexcept { m_terminal->widget_copy(type, format); }

        void screen_changed (GdkScreen *previous_screen) noexcept;
        void settings_changed() noexcept;

        void beep() noexcept;

        void set_hadjustment(vte::glib::RefPtr<GtkAdjustment>&& adjustment) noexcept { m_hadjustment = std::move(adjustment); }
        void set_vadjustment(vte::glib::RefPtr<GtkAdjustment>&& adjustment) { terminal()->widget_set_vadjustment(std::move(adjustment)); }
        auto hadjustment() noexcept { return m_hadjustment.get(); }
        auto vadjustment() noexcept { return terminal()->vadjustment(); }
        void set_hscroll_policy(GtkScrollablePolicy policy) noexcept { m_hscroll_policy = policy; }
        void set_vscroll_policy(GtkScrollablePolicy policy) noexcept { m_vscroll_policy = policy; }
        auto hscroll_policy() const noexcept { return m_hscroll_policy; }
        auto vscroll_policy() const noexcept { return m_vscroll_policy; }
        auto padding() const noexcept { return terminal()->padding(); }

        bool set_cursor_blink_mode(VteCursorBlinkMode mode) { return terminal()->set_cursor_blink_mode(vte::terminal::Terminal::CursorBlinkMode(mode)); }
        auto cursor_blink_mode() const noexcept { return VteCursorBlinkMode(terminal()->cursor_blink_mode()); }

        bool set_cursor_shape(VteCursorShape shape) { return terminal()->set_cursor_shape(vte::terminal::Terminal::CursorShape(shape)); }
        auto cursor_shape() const noexcept { return VteCursorShape(terminal()->cursor_shape()); }

        bool set_backspace_binding(VteEraseBinding mode) { return terminal()->set_backspace_binding(vte::terminal::Terminal::EraseMode(mode)); }
        auto backspace_binding() const noexcept { return VteEraseBinding(terminal()->backspace_binding()); }

        bool set_delete_binding(VteEraseBinding mode) { return terminal()->set_delete_binding(vte::terminal::Terminal::EraseMode(mode)); }
        auto delete_binding() const noexcept { return VteEraseBinding(terminal()->delete_binding()); }

        bool set_text_blink_mode(VteTextBlinkMode mode) { return terminal()->set_text_blink_mode(vte::terminal::Terminal::TextBlinkMode(mode)); }
        auto text_blink_mode() const noexcept { return VteTextBlinkMode(terminal()->text_blink_mode()); }

        bool set_word_char_exceptions(std::optional<std::string_view> stropt);
        auto word_char_exceptions() const noexcept { return m_word_char_exceptions ? m_word_char_exceptions.value().c_str() : nullptr; }

        char const* encoding() const noexcept { return m_terminal->encoding(); }

        void emit_child_exited(int status) noexcept;
        void emit_eof() noexcept;

        bool set_pty(VtePty* pty) noexcept;
        inline auto pty() const noexcept { return m_pty.get(); }

        void feed(std::string_view const& str) { terminal()->feed(str); }
        void feed_child(std::string_view const& str) { terminal()->feed_child(str); }
        void feed_child_binary(std::string_view const& str) { terminal()->feed_child_binary(str); }

        char *regex_match_check(vte::grid::column_t column,
                                vte::grid::row_t row,
                                int* tag)
        {
                return terminal()->regex_match_check(column, row, tag);
        }

        char* regex_match_check(GdkEvent* event,
                                int* tag)
        {
                return terminal()->regex_match_check(mouse_event_from_gdk(event), tag);
        }

        bool regex_match_check_extra(GdkEvent* event,
                                     vte::base::Regex const** regexes,
                                     size_t n_regexes,
                                     uint32_t match_flags,
                                     char** matches)
        {
                return terminal()->regex_match_check_extra(mouse_event_from_gdk(event),
                                                           regexes, n_regexes, match_flags, matches);
        }

        char* hyperlink_check(GdkEvent* event)
        {
                return terminal()->hyperlink_check(mouse_event_from_gdk(event));
        }

        bool should_emit_signal(int id) noexcept;

        bool set_sixel_enabled(bool enabled) noexcept { return m_terminal->set_sixel_enabled(enabled); }
        bool sixel_enabled() const noexcept { return m_terminal->sixel_enabled(); }

protected:

        enum class CursorType {
                eDefault,
                eInvisible,
                eMousing,
                eHyperlink
        };

        GdkWindow* event_window() const noexcept { return m_event_window; }

        bool realized() const noexcept
        {
                return gtk_widget_get_realized(m_widget);
        }

        vte::glib::RefPtr<GdkCursor> create_cursor(std::string const& name) const noexcept;

        void set_cursor(CursorType type) noexcept;
        void set_cursor(GdkCursor* cursor) noexcept;
        void set_cursor(Cursor const& cursor) noexcept;

        bool im_filter_keypress(KeyEvent const& event) noexcept;

        void im_focus_in() noexcept;
        void im_focus_out() noexcept;

        void im_reset() noexcept
        {
                if (m_im_context)
                        gtk_im_context_reset(m_im_context.get());
        }

        void im_set_cursor_location(cairo_rectangle_int_t const* rect) noexcept;

        void unset_pty() noexcept;

        unsigned key_event_translate_ctrlkey(KeyEvent const& event) const noexcept;

public: // FIXMEchpe
        void im_preedit_changed() noexcept;

private:
        unsigned read_modifiers_from_gdk(GdkEvent* event) const noexcept;
        KeyEvent key_event_from_gdk(GdkEventKey* event) const;
        MouseEvent mouse_event_from_gdk(GdkEvent* event) const /* throws */;

        void clipboard_request_received_cb(Clipboard const& clipboard,
                                           std::string_view const& text);
        void clipboard_request_failed_cb(Clipboard const& clipboard);

        std::optional<std::string_view> clipboard_data_get_cb(Clipboard const& clipboard,
                                                              ClipboardFormat format);
        void clipboard_data_clear_cb(Clipboard const& clipboard);

        GtkWidget* m_widget;

        vte::terminal::Terminal* m_terminal;

        /* Event window */
        GdkWindow *m_event_window;

        /* Cursors */
        vte::glib::RefPtr<GdkCursor> m_default_cursor;
        vte::glib::RefPtr<GdkCursor> m_invisible_cursor;
        vte::glib::RefPtr<GdkCursor> m_mousing_cursor;
        vte::glib::RefPtr<GdkCursor> m_hyperlink_cursor;

        /* Input method */
        vte::glib::RefPtr<GtkIMContext> m_im_context;

        /* PTY */
        vte::glib::RefPtr<VtePty> m_pty;

        /* Clipboard */
        std::shared_ptr<Clipboard> m_clipboard;
        std::shared_ptr<Clipboard> m_primary_clipboard;

        /* Misc */
        std::optional<std::string> m_word_char_exceptions{};

        vte::glib::RefPtr<GtkAdjustment> m_hadjustment{};
        uint32_t m_hscroll_policy : 1;
        uint32_t m_vscroll_policy : 1;
};

} // namespace platform

} // namespace vte
