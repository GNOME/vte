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

#include <sys/wait.h> // for W_EXITCODE

#include <new>
#include <string>

#include "vtegtk.hh"
#include "debug.h"

using namespace std::literals;

namespace vte {

namespace platform {

static void
im_commit_cb(GtkIMContext* im_context,
             char const* text,
             Widget* that)
{
        that->terminal()->im_commit(text);
}

static void
im_preedit_start_cb(GtkIMContext* im_context,
                    Widget* that)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method pre-edit started.\n");
        that->terminal()->im_preedit_set_active(true);
}

static void
im_preedit_end_cb(GtkIMContext* im_context,
                  Widget* that)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method pre-edit ended.\n");
        that->terminal()->im_preedit_set_active(false);
}

static void
im_preedit_changed_cb(GtkIMContext* im_context,
                      Widget* that)
{
        that->im_preedit_changed();
}

static gboolean
im_retrieve_surrounding_cb(GtkIMContext* im_context,
                           Widget* that)
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method retrieve-surrounding.\n");
        return that->terminal()->im_retrieve_surrounding();
}

static gboolean
im_delete_surrounding_cb(GtkIMContext* im_context,
                         int offset,
                         int n_chars,
                         Widget* that)
{
        _vte_debug_print(VTE_DEBUG_EVENTS,
                         "Input method delete-surrounding offset %d n-chars %d.\n",
                         offset, n_chars);
        return that->terminal()->im_delete_surrounding(offset, n_chars);
}

static void
settings_notify_cb(GtkSettings* settings,
                   GParamSpec* pspec,
                   vte::platform::Widget* that)
{
        that->settings_changed();
}

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

void
Widget::beep() noexcept
{
        if (realized())
                gdk_window_beep(gtk_widget_get_window(m_widget));
}

GdkCursor*
Widget::create_cursor(GdkCursorType cursor_type) const noexcept
{
	return gdk_cursor_new_for_display(gtk_widget_get_display(m_widget), cursor_type);
}

void
Widget::dispose() noexcept
{
        if (m_terminal->terminate_child()) {
                int status = W_EXITCODE(0, SIGKILL);
                emit_child_exited(status);
        }
}

void
Widget::emit_child_exited(int status) noexcept
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `child-exited'.\n");
        g_signal_emit(object(), signals[SIGNAL_CHILD_EXITED], 0, status);
}

bool
Widget::im_filter_keypress(GdkEventKey* event) noexcept
{
        // FIXMEchpe this can only be called when realized, so the m_im_context check is redundant
        return m_im_context &&
                gtk_im_context_filter_keypress(m_im_context.get(), event);
}

void
Widget::im_focus_in() noexcept
{
        gtk_im_context_focus_in(m_im_context.get());
}

void
Widget::im_focus_out() noexcept
{
        gtk_im_context_focus_out(m_im_context.get());
}

void
Widget::im_preedit_changed() noexcept
{
        char* str;
	PangoAttrList* attrs;
	int cursorpos;

        gtk_im_context_get_preedit_string(m_im_context.get(), &str, &attrs, &cursorpos);
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method pre-edit changed (%s,%d).\n",
                         str, cursorpos);

        m_terminal->im_preedit_changed(str, cursorpos, attrs);
        g_free(str);
}

void
Widget::im_set_cursor_location(cairo_rectangle_int_t const* rect) noexcept
{
        gtk_im_context_set_cursor_location(m_im_context.get(), rect);
}

void
Widget::map() noexcept
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

        assert(!m_im_context);
	m_im_context = gtk_im_multicontext_new();
	gtk_im_context_set_client_window(m_im_context.get(), m_event_window);
	g_signal_connect(m_im_context.get(), "commit",
			 G_CALLBACK(im_commit_cb), this);
	g_signal_connect(m_im_context.get(), "preedit-start",
			 G_CALLBACK(im_preedit_start_cb), this);
	g_signal_connect(m_im_context.get(), "preedit-changed",
			 G_CALLBACK(im_preedit_changed_cb), this);
	g_signal_connect(m_im_context.get(), "preedit-end",
			 G_CALLBACK(im_preedit_end_cb), this);
	g_signal_connect(m_im_context.get(), "retrieve-surrounding",
			 G_CALLBACK(im_retrieve_surrounding_cb), this);
	g_signal_connect(m_im_context.get(), "delete-surrounding",
			 G_CALLBACK(im_delete_surrounding_cb), this);
	gtk_im_context_set_use_preedit(m_im_context.get(), true);

        m_terminal->widget_realize();
}

void
Widget::screen_changed(GdkScreen *previous_screen) noexcept
{
        auto gdk_screen = gtk_widget_get_screen(m_widget);
        if (previous_screen != nullptr &&
            (gdk_screen != previous_screen || gdk_screen == nullptr)) {
                auto settings = gtk_settings_get_for_screen(previous_screen);
                g_signal_handlers_disconnect_matched(settings, G_SIGNAL_MATCH_DATA,
                                                     0, 0, nullptr, nullptr,
                                                     this);
        }

        if (gdk_screen == previous_screen || gdk_screen == nullptr)
                return;

        settings_changed();

        auto settings = gtk_widget_get_settings(m_widget);
        g_signal_connect (settings, "notify::gtk-cursor-blink",
                          G_CALLBACK(settings_notify_cb), this);
        g_signal_connect (settings, "notify::gtk-cursor-blink-time",
                          G_CALLBACK(settings_notify_cb), this);
        g_signal_connect (settings, "notify::gtk-cursor-blink-timeout",
                          G_CALLBACK(settings_notify_cb), this);
}

void
Widget::settings_changed() noexcept
{
        gboolean blink;
        int blink_time;
        int blink_timeout;

        g_object_get(gtk_widget_get_settings(m_widget),
                     "gtk-cursor-blink", &blink,
                     "gtk-cursor-blink-time", &blink_time,
                     "gtk-cursor-blink-timeout", &blink_timeout,
                     nullptr);

        _vte_debug_print(VTE_DEBUG_MISC,
                         "Cursor blinking settings: blink=%d time=%d timeout=%d\n",
                         blink, blink_time, blink_timeout);

        m_terminal->set_blink_settings(blink, blink_time, blink_timeout);
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

	/* Shut down input methods. */
        assert(m_im_context);
        g_signal_handlers_disconnect_matched(m_im_context.get(),
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL,
                                             this);
        m_terminal->im_preedit_reset();
        gtk_im_context_set_client_window(m_im_context.get(), nullptr);
        m_im_context.reset();

        /* Destroy input window */
        gtk_widget_unregister_window(m_widget, m_event_window);
        gdk_window_destroy(m_event_window);
        m_event_window = nullptr;
}

} // namespace platform

} // namespace vte
