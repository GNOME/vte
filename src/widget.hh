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

#include "vteterminal.h"
#include "vteinternal.hh"

#include "refptr.hh"

namespace vte {

namespace terminal {
class Terminal;
}

namespace platform {

class Widget {
public:
        friend class vte::terminal::Terminal;

        Widget(VteTerminal* t) noexcept;
        ~Widget() noexcept;

        Widget(Widget const&) = delete;
        Widget(Widget&&) = delete;
        Widget& operator= (Widget const&) = delete;
        Widget& operator= (Widget&&) = delete;

        GObject* object() const noexcept { return reinterpret_cast<GObject*>(m_widget); }
        GtkWidget* gtk() const noexcept { return m_widget; }
        VteTerminal* vte() const noexcept { return reinterpret_cast<VteTerminal*>(m_widget); }

        vte::terminal::Terminal* terminal() const noexcept { return m_terminal; }

        void constructed() noexcept { m_terminal->widget_constructed(); }
        void dispose() noexcept;
        void realize() noexcept;
        void unrealize() noexcept;
        void map() noexcept;
        void unmap() noexcept;
        void style_updated() noexcept { m_terminal->widget_style_updated(); }
        void draw(cairo_t *cr) noexcept { m_terminal->widget_draw(cr); }
        void get_preferred_width(int *minimum_width,
                                 int *natural_width) const noexcept { m_terminal->widget_get_preferred_width(minimum_width, natural_width); }
        void get_preferred_height(int *minimum_height,
                                  int *natural_height) const noexcept { m_terminal->widget_get_preferred_height(minimum_height, natural_height); }
        void size_allocate(GtkAllocation *allocation) noexcept;

        void focus_in(GdkEventFocus *event) noexcept { m_terminal->widget_focus_in(event); }
        void focus_out(GdkEventFocus *event) noexcept { m_terminal->widget_focus_out(event); }
        bool key_press(GdkEventKey *event) noexcept { return m_terminal->widget_key_press(event); }
        bool key_release(GdkEventKey *event) noexcept { return m_terminal->widget_key_release(event); }
        bool button_press(GdkEventButton *event) noexcept { return m_terminal->widget_button_press(event); }
        bool button_release(GdkEventButton *event) noexcept { return m_terminal->widget_button_release(event); }
        void enter(GdkEventCrossing *event) noexcept { m_terminal->widget_enter(event); }
        void leave(GdkEventCrossing *event) noexcept { m_terminal->widget_leave(event); }
        void scroll(GdkEventScroll *event) noexcept { m_terminal->widget_scroll(event); }
        bool motion_notify(GdkEventMotion *event) noexcept { return m_terminal->widget_motion_notify(event); }

        void paste(GdkAtom board) noexcept { m_terminal->widget_paste(board); }
        void copy(VteSelection sel,
                  VteFormat format) noexcept { m_terminal->widget_copy(sel, format); }
        void paste_received(char const* text) noexcept { m_terminal->widget_paste_received(text); }
        void clipboard_cleared(GtkClipboard *clipboard) noexcept { m_terminal->widget_clipboard_cleared(clipboard); }
        void clipboard_requested(GtkClipboard *target_clipboard,
                                 GtkSelectionData *data,
                                 guint info) noexcept { m_terminal->widget_clipboard_requested(target_clipboard, data, info); }

        void screen_changed (GdkScreen *previous_screen) noexcept;
        void settings_changed() noexcept;

        void beep() noexcept;

        void set_hadjustment(GtkAdjustment *adjustment) noexcept { m_terminal->widget_set_hadjustment(adjustment); }
        GtkAdjustment* get_hadjustment() const noexcept { return m_terminal->m_hadjustment; }
        void set_vadjustment(GtkAdjustment *adjustment) noexcept { m_terminal->widget_set_vadjustment(adjustment); }
        GtkAdjustment* get_vadjustment() const noexcept { return m_terminal->m_vadjustment; }

        int hscroll_policy() const noexcept { return m_terminal->m_hscroll_policy; }
        int vscroll_policy() const noexcept { return m_terminal->m_vscroll_policy; }

        char const* encoding() const noexcept
        {
                return m_terminal->m_encoding ? m_terminal->m_encoding : "UTF-8";
        }

        void emit_child_exited(int status) noexcept;

protected:

        enum class Cursor {
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

        GdkCursor *create_cursor(GdkCursorType cursor_type) const noexcept;

        void set_cursor(Cursor type) noexcept;

        void set_cursor(GdkCursor* cursor) noexcept
        {
                gdk_window_set_cursor(m_event_window, cursor);
        }

        bool im_filter_keypress(GdkEventKey* event) noexcept;

        void im_focus_in() noexcept;
        void im_focus_out() noexcept;

        void im_reset() noexcept
        {
                if (m_im_context)
                        gtk_im_context_reset(m_im_context.get());
        }

        void im_set_cursor_location(cairo_rectangle_int_t const* rect) noexcept;

public: // FIXMEchpe
        void im_preedit_changed() noexcept;

private:
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
};

} // namespace platform

} // namespace vte
