/*
 * Copyright © 2008, 2009, 2010, 2018 Christian Persch
 * Copyright © 2001-2004,2009,2010 Red Hat, Inc.
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

#include "config.h"

#include "widget.hh"

#include <new>

#include "debug.h"

namespace vte {

namespace platform {

Widget::Widget(VteTerminal* t) noexcept :
        m_widget{&t->widget}
{
        /* Until Terminal init is completely fixed, use zero'd memory */
        auto place = g_malloc0(sizeof(vte::terminal::Terminal));
        m_terminal = new (place) vte::terminal::Terminal(this, t);
}

Widget::~Widget() noexcept
{
        m_widget = nullptr;

        m_terminal->~Terminal();
        g_free(m_terminal);
}

GdkCursor*
Widget::create_cursor(GdkCursorType cursor_type) const noexcept
{
	return gdk_cursor_new_for_display(gtk_widget_get_display(m_widget), cursor_type);
}

void
Widget::map()
{
        if (m_event_window)
                gdk_window_show_unraised(m_event_window);
}

void
Widget::realize() noexcept
{
        //        m_mouse_cursor_over_widget = false;  /* We'll receive an enter_notify_event if the window appears under the cursor. */

	/* Create stock cursors */
	m_default_cursor = create_cursor(VTE_DEFAULT_CURSOR);
	m_invisible_cursor = create_cursor(GDK_BLANK_CURSOR);
	m_mousing_cursor = create_cursor(VTE_MOUSING_CURSOR);
        if (_vte_debug_on(VTE_DEBUG_HYPERLINK))
                /* Differ from the standard regex match cursor in debug mode. */
                m_hyperlink_cursor = create_cursor(VTE_HYPERLINK_CURSOR_DEBUG);
        else
                m_hyperlink_cursor = create_cursor(VTE_HYPERLINK_CURSOR);

	/* Create an input window for the widget. */
        auto allocation = m_terminal->get_allocated_rect();
	GdkWindowAttr attributes;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.visual = gtk_widget_get_visual(m_widget);
	attributes.event_mask =
                gtk_widget_get_events(m_widget) |
                GDK_EXPOSURE_MASK |
                GDK_FOCUS_CHANGE_MASK |
                GDK_SMOOTH_SCROLL_MASK |
                GDK_SCROLL_MASK |
                GDK_BUTTON_PRESS_MASK |
                GDK_BUTTON_RELEASE_MASK |
                GDK_POINTER_MOTION_MASK |
                GDK_BUTTON1_MOTION_MASK |
                GDK_ENTER_NOTIFY_MASK |
                GDK_LEAVE_NOTIFY_MASK |
                GDK_KEY_PRESS_MASK |
                GDK_KEY_RELEASE_MASK;
	attributes.cursor = m_default_cursor.get();
	guint attributes_mask =
                GDK_WA_X |
                GDK_WA_Y |
                (attributes.visual ? GDK_WA_VISUAL : 0) |
                GDK_WA_CURSOR;

	m_event_window = gdk_window_new(gtk_widget_get_parent_window (m_widget),
                                        &attributes, attributes_mask);
        gtk_widget_register_window(m_widget, m_event_window);

        m_terminal->widget_realize();
}

void
Widget::set_cursor(Cursor type) noexcept
{
        switch (type) {
        case Cursor::eDefault:   return set_cursor(m_default_cursor.get());
        case Cursor::eInvisible: return set_cursor(m_invisible_cursor.get());
        case Cursor::eMousing:   return set_cursor(m_mousing_cursor.get());
        case Cursor::eHyperlink: return set_cursor(m_hyperlink_cursor.get());
        }
}

void
Widget::size_allocate(GtkAllocation* allocation) noexcept
{
        m_terminal->widget_size_allocate(allocation);

        if (realized())
		gdk_window_move_resize(m_event_window,
                                       allocation->x,
                                       allocation->y,
                                       allocation->width,
                                       allocation->height);
}

void
Widget::unmap()
{
        if (m_event_window)
                gdk_window_hide(m_event_window);
}

void
Widget::unrealize() noexcept
{
        m_terminal->widget_unrealize();

        m_default_cursor.reset();
        m_invisible_cursor.reset();
        m_mousing_cursor.reset();
        m_hyperlink_cursor.reset();

        gtk_widget_unregister_window(m_widget, m_event_window);
        gdk_window_destroy(m_event_window);
        m_event_window = nullptr;
}

} // namespace platform

} // namespace vte
