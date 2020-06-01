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

#include <exception>
#include <new>
#include <string>

#include "cxx-utils.hh"
#include "vtegtk.hh"
#include "vteptyinternal.hh"
#include "debug.h"

using namespace std::literals;

namespace vte {

namespace platform {

static void
im_commit_cb(GtkIMContext* im_context,
             char const* text,
             Widget* that) noexcept
try
{
        if (text == nullptr)
                return;

        that->terminal()->im_commit(text);
}
catch (...)
{
        vte::log_exception();
}

static void
im_preedit_start_cb(GtkIMContext* im_context,
                    Widget* that) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method pre-edit started.\n");
        that->terminal()->im_preedit_set_active(true);
}
catch (...)
{
        vte::log_exception();
}

static void
im_preedit_end_cb(GtkIMContext* im_context,
                  Widget* that) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method pre-edit ended.\n");
        that->terminal()->im_preedit_set_active(false);
}
catch (...)
{
        vte::log_exception();
}

static void
im_preedit_changed_cb(GtkIMContext* im_context,
                      Widget* that) noexcept
try
{
        that->im_preedit_changed();
}
catch (...)
{
        vte::log_exception();
}

static gboolean
im_retrieve_surrounding_cb(GtkIMContext* im_context,
                           Widget* that) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method retrieve-surrounding.\n");
        return that->terminal()->im_retrieve_surrounding();
}
catch (...)
{
        vte::log_exception();
        return false;
}

static gboolean
im_delete_surrounding_cb(GtkIMContext* im_context,
                         int offset,
                         int n_chars,
                         Widget* that) noexcept
try
{
        _vte_debug_print(VTE_DEBUG_EVENTS,
                         "Input method delete-surrounding offset %d n-chars %d.\n",
                         offset, n_chars);
        return that->terminal()->im_delete_surrounding(offset, n_chars);
}
catch (...)
{
        vte::log_exception();
        return false;
}

static void
settings_notify_cb(GtkSettings* settings,
                   GParamSpec* pspec,
                   vte::platform::Widget* that) noexcept
try
{
        that->settings_changed();
}
catch (...)
{
        vte::log_exception();
}

Widget::Widget(VteTerminal* t)
        : m_widget{&t->widget},
          m_hscroll_policy{GTK_SCROLL_NATURAL},
          m_vscroll_policy{GTK_SCROLL_NATURAL}
{
        gtk_widget_set_can_focus(gtk(), true);

        /* We do our own redrawing. */
        // FIXMEchpe is this still necessary?
        gtk_widget_set_redraw_on_allocate(gtk(), false);

        /* Until Terminal init is completely fixed, use zero'd memory */
        auto place = g_malloc0(sizeof(vte::terminal::Terminal));
        m_terminal = new (place) vte::terminal::Terminal(this, t);
}

Widget::~Widget() noexcept
try
{
        g_signal_handlers_disconnect_matched(gtk_widget_get_settings(m_widget),
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL,
                                             this);

        m_widget = nullptr;

        m_terminal->~Terminal();
        g_free(m_terminal);
}
catch (...)
{
        vte::log_exception();
}

void
Widget::beep() noexcept
{
        if (realized())
                gdk_window_beep(gtk_widget_get_window(m_widget));
}

vte::glib::RefPtr<GdkCursor>
Widget::create_cursor(GdkCursorType cursor_type) const noexcept
{
	return vte::glib::take_ref(gdk_cursor_new_for_display(gtk_widget_get_display(m_widget), cursor_type));
}

void
Widget::set_cursor(GdkCursor* cursor) noexcept
{
        gdk_window_set_cursor(m_event_window, cursor);
}

void
Widget::set_cursor(Cursor const& cursor) noexcept
{
        if (!realized())
                return;

        auto display = gtk_widget_get_display(m_widget);
        GdkCursor* gdk_cursor{nullptr};
        switch (cursor.index()) {
        case 0:
                gdk_cursor = gdk_cursor_new_from_name(display, std::get<0>(cursor).c_str());
                break;
        case 1:
                gdk_cursor = std::get<1>(cursor).get();
                if (gdk_cursor != nullptr &&
                    gdk_cursor_get_display(gdk_cursor) == display) {
                        g_object_ref(gdk_cursor);
                } else {
                        gdk_cursor = nullptr;
                }
                break;
        case 2:
                gdk_cursor = gdk_cursor_new_for_display(display, std::get<2>(cursor));
                break;
        }

        set_cursor(gdk_cursor);
        if (gdk_cursor)
                g_object_unref(gdk_cursor);
}

void
Widget::constructed() noexcept
{
        /* Set the style as early as possible, before GTK+ starts
         * invoking various callbacks. This is needed in order to
         * compute the initial geometry correctly in presence of
         * non-default padding, see bug 787710.
         */
        style_updated();
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

void
Widget::emit_eof() noexcept
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `eof'.\n");
        g_signal_emit(object(), signals[SIGNAL_EOF], 0);
}

bool
Widget::im_filter_keypress(vte::terminal::KeyEvent const& event) noexcept
{
        // FIXMEchpe this can only be called when realized, so the m_im_context check is redundant
        return m_im_context &&
                gtk_im_context_filter_keypress(m_im_context.get(),
                                               reinterpret_cast<GdkEventKey*>(event.platform_event()));
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
        char* str = nullptr;
	PangoAttrList* attrs = nullptr;
	int cursorpos = 0;

        gtk_im_context_get_preedit_string(m_im_context.get(), &str, &attrs, &cursorpos);
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method pre-edit changed (%s,%d).\n",
                         str, cursorpos);

        if (str != nullptr)
                m_terminal->im_preedit_changed(str, cursorpos, {attrs, &pango_attr_list_unref});
        else
                pango_attr_list_unref(attrs);

        g_free(str);
}

void
Widget::im_set_cursor_location(cairo_rectangle_int_t const* rect) noexcept
{
        gtk_im_context_set_cursor_location(m_im_context.get(), rect);
}

unsigned
Widget::read_modifiers_from_gdk(GdkEvent* event) const noexcept
{
        /* Read the modifiers. See bug #663779 for more information on why we do this. */
        auto mods = GdkModifierType{};
        if (!gdk_event_get_state(event, &mods))
                return 0;

        #if 1
        /* HACK! Treat META as ALT; see bug #663779. */
        if (mods & GDK_META_MASK)
                mods = GdkModifierType(mods | GDK_MOD1_MASK);
        #endif

        /* Map non-virtual modifiers to virtual modifiers (Super, Hyper, Meta) */
        auto display = gdk_window_get_display(gdk_event_get_window(event));
        auto keymap = gdk_keymap_get_for_display(display);
        gdk_keymap_add_virtual_modifiers(keymap, &mods);

        return unsigned(mods);
}

unsigned
Widget::key_event_translate_ctrlkey(vte::terminal::KeyEvent const& event) const noexcept
{
	if (event.keyval() < 128)
		return event.keyval();

        auto display = gdk_window_get_display(gdk_event_get_window(event.platform_event()));
        auto keymap = gdk_keymap_get_for_display(display);
        auto keyval = unsigned{event.keyval()};

	/* Try groups in order to find one mapping the key to ASCII */
	for (auto i = unsigned{0}; i < 4; i++) {
		auto consumed_modifiers = GdkModifierType{};
		gdk_keymap_translate_keyboard_state (keymap,
                                                     event.keycode(),
                                                     GdkModifierType(event.modifiers()),
                                                     i,
                                                     &keyval, NULL, NULL, &consumed_modifiers);
		if (keyval < 128) {
			_vte_debug_print (VTE_DEBUG_EVENTS,
                                          "ctrl+Key, group=%d de-grouped into keyval=0x%x\n",
                                          event.group(), keyval);
                        break;
		}
	}

        return keyval;
}

vte::terminal::KeyEvent
Widget::key_event_from_gdk(GdkEventKey* event) const
{
        auto type = vte::terminal::EventBase::Type{};
        switch (gdk_event_get_event_type(reinterpret_cast<GdkEvent*>(event))) {
        case GDK_KEY_PRESS: type = vte::terminal::KeyEvent::Type::eKEY_PRESS;     break;
        case GDK_KEY_RELEASE: type = vte::terminal::KeyEvent::Type::eKEY_RELEASE; break;
        default: g_assert_not_reached(); return {};
        }

        auto base_event = reinterpret_cast<GdkEvent*>(event);
        return {base_event,
                type,
                event->time,
                read_modifiers_from_gdk(base_event),
                event->keyval,
                event->hardware_keycode, // gdk_event_get_scancode(event),
                event->group,
                event->is_modifier != 0};
}

std::optional<vte::terminal::MouseEvent>
Widget::mouse_event_from_gdk(GdkEvent* event) const
{
        auto type = vte::terminal::EventBase::Type{};
        switch (gdk_event_get_event_type(event)) {
        case GDK_2BUTTON_PRESS:  type = vte::terminal::MouseEvent::Type::eMOUSE_DOUBLE_PRESS; break;
        case GDK_3BUTTON_PRESS:  type = vte::terminal::MouseEvent::Type::eMOUSE_TRIPLE_PRESS; break;
        case GDK_BUTTON_PRESS:   type = vte::terminal::MouseEvent::Type::eMOUSE_PRESS;        break;
        case GDK_BUTTON_RELEASE: type = vte::terminal::MouseEvent::Type::eMOUSE_RELEASE;      break;
        case GDK_ENTER_NOTIFY:   type = vte::terminal::MouseEvent::Type::eMOUSE_ENTER;        break;
        case GDK_LEAVE_NOTIFY:   type = vte::terminal::MouseEvent::Type::eMOUSE_LEAVE;        break;
        case GDK_MOTION_NOTIFY:  type = vte::terminal::MouseEvent::Type::eMOUSE_MOTION;       break;
        case GDK_SCROLL:         type = vte::terminal::MouseEvent::Type::eMOUSE_SCROLL;       break;
        default:
                return std::nullopt;
        }

        auto x = double{};
        auto y = double{};
        if (gdk_event_get_window(event) != m_event_window ||
            !gdk_event_get_coords(event, &x, &y))
                x = y = -1.; // FIXMEchpe or return std::nullopt?

        auto button = unsigned{0};
        (void)gdk_event_get_button(event, &button);

        auto mouse_event = vte::terminal::MouseEvent{event,
                                                     type,
                                                     gdk_event_get_time(event),
                                                     read_modifiers_from_gdk(event),
                                                     vte::terminal::MouseEvent::Button(button),
                                                     x,
                                                     y};
        return mouse_event;
}

void
Widget::map() noexcept
{
        if (m_event_window)
                gdk_window_show_unraised(m_event_window);
}

bool
Widget::primary_paste_enabled() const noexcept
{
        auto primary_paste = gboolean{};
        g_object_get(gtk_widget_get_settings(gtk()),
                     "gtk-enable-primary-paste", &primary_paste,
                     nullptr);

        return primary_paste != false;
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
	m_im_context.reset(gtk_im_multicontext_new());
#if GTK_CHECK_VERSION (3, 24, 14)
        g_object_set(m_im_context.get(),
                     "input-purpose", GTK_INPUT_PURPOSE_TERMINAL,
                     nullptr);
#endif
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
        auto blink = gboolean{};
        auto blink_time = int{};
        auto blink_timeout = int{};
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
Widget::set_cursor(CursorType type) noexcept
{
        switch (type) {
        case CursorType::eDefault:   return set_cursor(m_default_cursor.get());
        case CursorType::eInvisible: return set_cursor(m_invisible_cursor.get());
        case CursorType::eMousing:   return set_cursor(m_mousing_cursor.get());
        case CursorType::eHyperlink: return set_cursor(m_hyperlink_cursor.get());
        }
}

bool
Widget::set_pty(VtePty* pty_obj) noexcept
{
        if (pty() == pty_obj)
                return false;

        m_pty = vte::glib::make_ref(pty_obj);
        terminal()->set_pty(_vte_pty_get_impl(pty()));

        return true;
}

bool
Widget::set_word_char_exceptions(std::optional<std::string_view> stropt)
{
        if (m_word_char_exceptions == stropt)
                return false;

        if (terminal()->set_word_char_exceptions(stropt)) {
                m_word_char_exceptions = stropt;
                return true;
        }

        return false;
}

void
Widget::unset_pty() noexcept
{
        if (!pty())
                return;

        /* This is only called from Terminal, so we need
         * to explicitly notify the VteTerminal:pty property,
         * but we do NOT need to call Terminal::set_pty(nullptr).
         */
        m_pty.reset();
        g_object_notify_by_pspec(object(), pspecs[PROP_PTY]);
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

bool
Widget::should_emit_signal(int id) noexcept
{
        return g_signal_has_handler_pending(object(),
                                            signals[id],
                                            0 /* detail */,
                                            false /* not interested in blocked handlers */) != FALSE;
}

void
Widget::style_updated() noexcept
{
        auto padding = GtkBorder{};
        auto context = gtk_widget_get_style_context(gtk());
        gtk_style_context_get_padding(context, gtk_style_context_get_state(context),
                                      &padding);
        m_terminal->set_border_padding(&padding);

        auto aspect = float{};
        gtk_widget_style_get(gtk(), "cursor-aspect-ratio", &aspect, nullptr);
        m_terminal->set_cursor_aspect(aspect);

        m_terminal->widget_style_updated();
}

void
Widget::unmap() noexcept
{
        m_terminal->widget_unmap();

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
