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

namespace vte {

namespace platform {

class Widget {
public:
        Widget(VteTerminal* t) noexcept;
        ~Widget() noexcept;

        Widget(Widget const&) = delete;
        Widget(Widget&&) = delete;
        Widget& operator= (Widget const&) = delete;
        Widget& operator= (Widget&&) = delete;

        VteTerminal* vte() const noexcept { return m_widget; }

        vte::terminal::Terminal* terminal() const noexcept { return m_terminal; }


        void constructed() noexcept { m_terminal->widget_constructed(); }
        void realize() noexcept { m_terminal->widget_realize(); }
        void unrealize() noexcept { m_terminal->widget_unrealize(); }
        void map() noexcept { m_terminal->widget_map(); }
        void unmap() noexcept { m_terminal->widget_unmap(); }
        void style_updated() noexcept { m_terminal->widget_style_updated(); }
        void draw(cairo_t *cr) noexcept { m_terminal->widget_draw(cr); }
        void screen_changed (GdkScreen *previous_screen) noexcept { m_terminal->widget_screen_changed(previous_screen); }
        void get_preferred_width(int *minimum_width,
                                 int *natural_width) const noexcept { m_terminal->widget_get_preferred_width(minimum_width, natural_width); }
        void get_preferred_height(int *minimum_height,
                                  int *natural_height) const noexcept { m_terminal->widget_get_preferred_height(minimum_height, natural_height); }
        void size_allocate(GtkAllocation *allocation) noexcept { m_terminal->widget_size_allocate(allocation); }

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

        void set_hadjustment(GtkAdjustment *adjustment) noexcept { m_terminal->widget_set_hadjustment(adjustment); }
        GtkAdjustment* get_hadjustment() const noexcept { return m_terminal->m_hadjustment; }
        void set_vadjustment(GtkAdjustment *adjustment) noexcept { m_terminal->widget_set_vadjustment(adjustment); }
        GtkAdjustment* get_vadjustment() const noexcept { return m_terminal->m_vadjustment; }

        int get_hscroll_policy() const noexcept { return m_terminal->m_hscroll_policy; }
        int get_vscroll_policy() const noexcept { return m_terminal->m_vscroll_policy; }

private:
        VteTerminal* m_widget;

        vte::terminal::Terminal* m_terminal;
};

} // namespace platform

} // namespace vte
