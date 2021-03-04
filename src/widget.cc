/*
 * Copyright © 2008, 2009, 2010, 2018 Christian Persch
 * Copyright © 2001-2004,2009,2010 Red Hat, Inc.
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

#include "config.h"

#include "widget.hh"

#include <sys/wait.h> // for W_EXITCODE

#include <exception>
#include <new>
#include <stdexcept>
#include <string>

#include "vtegtk.hh"
#include "vteptyinternal.hh"
#include "debug.h"
#include "gobject-glue.hh"

#if VTE_GTK == 4
#include "graphene-glue.hh"
#endif

#if VTE_GTK == 3
#define VTE_STYLE_CLASS_MONOSPACE GTK_STYLE_CLASS_MONOSPACE
#elif VTE_GTK == 4
#define VTE_STYLE_CLASS_MONOSPACE "monospace"
#endif

using namespace std::literals;

namespace vte {

namespace platform {

static void vadjustment_value_changed_cb(vte::platform::Widget* that) noexcept;

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
        // Create a default adjustment
        set_vadjustment({});

#if VTE_GTK == 3
        gtk_widget_set_can_focus(gtk(), true);
#endif

#if VTE_GTK == 4
        gtk_widget_set_focusable(gtk(), true);
#endif

#if VTE_GTK == 3
        /* We do our own redrawing. */
        // FIXMEchpe is this still necessary?
        gtk_widget_set_redraw_on_allocate(gtk(), false);
#endif

        /* Until Terminal init is completely fixed, use zero'd memory */
        auto place = g_malloc0(sizeof(vte::terminal::Terminal));
        m_terminal = new (place) vte::terminal::Terminal(this, t);
}

Widget::~Widget() noexcept
try
{
        g_signal_handlers_disconnect_matched(m_settings.get(),
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL,
                                             this);

        if (m_vadjustment) {
                g_signal_handlers_disconnect_by_func(m_vadjustment.get(),
                                                     (void*)vadjustment_value_changed_cb,
                                                     this);
        }

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
                gtk_widget_error_bell(gtk());
}

#if VTE_GTK == 4

bool
Widget::contains(double x,
                 double y)
{
        return false;
}

#endif /* VTE_GTK == 4 */

vte::glib::RefPtr<GdkCursor>
Widget::create_cursor(std::string const& name) const noexcept
{
#if VTE_GTK == 3
	return vte::glib::take_ref(gdk_cursor_new_from_name(gtk_widget_get_display(m_widget), name.c_str()));
#elif VTE_GTK == 4
        return vte::glib::take_ref(gdk_cursor_new_from_name(name.c_str(), nullptr /* fallback */));
#endif
}

void
Widget::set_cursor(GdkCursor* cursor) noexcept
{
#if VTE_GTK == 3
        gdk_window_set_cursor(m_event_window, cursor);
#elif VTE_GTK == 4
        gtk_widget_set_cursor(gtk(), cursor);
#endif
}

void
Widget::set_cursor(Cursor const& cursor) noexcept
{
        if (!realized())
                return;

        GdkCursor* gdk_cursor{nullptr};
        switch (cursor.index()) {
        case 0:
#if VTE_GTK == 3
                gdk_cursor = gdk_cursor_new_from_name(gtk_widget_get_display(gtk()),
                                                      std::get<0>(cursor).c_str());
#elif VTE_GTK == 4
                gdk_cursor = gdk_cursor_new_from_name(std::get<0>(cursor).c_str(),
                                                      nullptr /* fallback */);
#endif /* VTE_GTK */
                break;

        case 1:
                gdk_cursor = std::get<1>(cursor).get();
                if (gdk_cursor != nullptr
#if VTE_GTK == 3
                    && gdk_cursor_get_display(gdk_cursor) == gtk_widget_get_display(gtk())
#endif
                ) {
                        g_object_ref(gdk_cursor);
                } else {
                        gdk_cursor = nullptr;
                }
                break;

#if VTE_GTK == 3
        case 2:
                gdk_cursor = gdk_cursor_new_for_display(gtk_widget_get_display(gtk()), std::get<2>(cursor));
                break;
#endif
        }

        set_cursor(gdk_cursor);
        if (gdk_cursor)
                g_object_unref(gdk_cursor);
}

Clipboard&
Widget::clipboard_get(ClipboardType type) const
{
        switch (type) {
        case ClipboardType::CLIPBOARD: return *m_clipboard;
        case ClipboardType::PRIMARY: return *m_primary_clipboard;
        default: g_assert_not_reached(); throw std::runtime_error{""}; break;
        }
}

std::optional<std::string_view>
Widget::clipboard_data_get_cb(Clipboard const& clipboard,
                              ClipboardFormat format)
{
        return terminal()->widget_clipboard_data_get(clipboard, format);
}

void
Widget::clipboard_data_clear_cb(Clipboard const& clipboard)
{
        terminal()->widget_clipboard_data_clear(clipboard);
}

void
Widget::clipboard_request_received_cb(Clipboard const& clipboard,
                                      std::string_view const& text)
{
        terminal()->widget_clipboard_text_received(clipboard, text);
}

void
Widget::clipboard_request_failed_cb(Clipboard const& clipboard)
{
        gtk_widget_error_bell(gtk());
}

void
Widget::clipboard_offer_data(ClipboardType type,
                             ClipboardFormat format) noexcept
{
        try {
                clipboard_get(type).offer_data(format,
                                               &Widget::clipboard_data_get_cb,
                                               &Widget::clipboard_data_clear_cb);
        } catch (...) {
                /* Let the caller know the request failed */
                terminal()->widget_clipboard_data_clear(clipboard_get(type));
        }
}

void
Widget::clipboard_request_text(ClipboardType type) noexcept
{
        try {
                clipboard_get(type).request_text(&Widget::clipboard_request_received_cb,
                                                 &Widget::clipboard_request_failed_cb);
        } catch (...) {
                /* Let the caller know the request failed */
                clipboard_request_failed_cb(clipboard_get(type));
        }
}

void
Widget::clipboard_set_text(ClipboardType type,
                           std::string_view const& str) noexcept
{
        clipboard_get(type).set_text(str);
}

#if VTE_GTK == 4

std::pair<bool, bool>
Widget::compute_expand()
{
        return {true, true};
}

#endif /* VTE_GTK == 4 */

void
Widget::constructed() noexcept
{
#if VTE_GTK == 3
        auto context = gtk_widget_get_style_context(m_widget);
        gtk_style_context_add_class (context, VTE_STYLE_CLASS_MONOSPACE);
#elif VTE_GTK == 4
        gtk_widget_add_css_class(gtk(), VTE_STYLE_CLASS_MONOSPACE);
#endif /* VTE_GTK */

#if VTE_GTK == 4

        connect_settings();

#endif /* VTE_GTK == 4 */

#if VTE_GTK == 3
        /* Set the style as early as possible, before GTK+ starts
         * invoking various callbacks. This is needed in order to
         * compute the initial geometry correctly in presence of
         * non-default padding, see bug 787710.
         */
        style_updated();
#elif VTE_GTK == 4
        padding_changed();
#endif /* VTE_GTK  */
}

#if VTE_GTK == 4

void
Widget::css_changed(GtkCssStyleChange* change)
{
        /* This function is mostly useless, since there's no public API for GtkCssStyleChange */

        padding_changed();
}

#endif /* VTE_GTK == 4 */

void
Widget::direction_changed(GtkTextDirection old_direction) noexcept
{
        // FIXME: does this need to feed to BiDi somehow?
}

void
Widget::dispose() noexcept
{
#ifdef WITH_A11Y
        m_terminal->set_accessible(nullptr);
#endif

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
Widget::im_filter_keypress(KeyEvent const& event) noexcept
{
        // FIXMEchpe this can only be called when realized, so the m_im_context check is redundant
        return m_im_context &&
                gtk_im_context_filter_keypress(m_im_context.get(),
#if VTE_GTK == 3
                                               reinterpret_cast<GdkEventKey*>(event.platform_event())
#elif VTE_GTK == 4
                                               event.platform_event()
#endif
                                               );
}

#if VTE_GTK == 3

void
Widget::event_focus_in(GdkEventFocus *event)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus In");

        m_terminal->widget_focus_in();
}

void
Widget::event_focus_out(GdkEventFocus *event)
{
	_vte_debug_print(VTE_DEBUG_EVENTS, "Focus Out");

        m_terminal->widget_focus_out();
}

bool
Widget::event_key_press(GdkEventKey *event)
{
        auto key_event = key_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Key press key=%x keycode=%x modifiers=%x\n",
                         key_event.keyval(), key_event.keycode(), key_event.modifiers());

        return m_terminal->widget_key_press(key_event);
}

bool
Widget::event_key_release(GdkEventKey *event)
{
        auto key_event = key_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Key release key=%x keycode=%x modifiers=%x\n",
                         key_event.keyval(), key_event.keycode(), key_event.modifiers());

        return m_terminal->widget_key_release(key_event);
}

bool
Widget::event_button_press(GdkEventButton *event)
{
        auto mouse_event = mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Click press button=%d press_count=%d x=%.3f y=%.3f\n",
                         mouse_event.button_value(), mouse_event.press_count(),
                         mouse_event.x(), mouse_event.y());

        return m_terminal->widget_mouse_press(mouse_event);
}

bool
Widget::event_button_release(GdkEventButton *event)
{
        auto mouse_event = mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Click release button=%d x=%.3f y=%.3f\n",
                         mouse_event.button_value(), mouse_event.x(), mouse_event.y());

        return m_terminal->widget_mouse_release(mouse_event);
}

void
Widget::event_enter(GdkEventCrossing *event)
{
        auto mouse_event = mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Motion enter x=%.3f y=%.3f\n",
                         mouse_event.x(), mouse_event.y());

        m_terminal->widget_mouse_enter(mouse_event);
}

void
Widget::event_leave(GdkEventCrossing *event)
{
        auto mouse_event = mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Motion leave x=%.3f y=%.3f\n",
                         mouse_event.x(), mouse_event.y());

        m_terminal->widget_mouse_leave(mouse_event);
}
bool
Widget::event_scroll(GdkEventScroll *event)
{
        auto scroll_event = scroll_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Scroll delta_x=%.3f delta_y=%.3f\n",
                         scroll_event.dx(), scroll_event.dy());

        return m_terminal->widget_mouse_scroll(scroll_event);
}

bool
Widget::event_motion_notify(GdkEventMotion *event)
{
        auto mouse_event = mouse_event_from_gdk(reinterpret_cast<GdkEvent*>(event));

	_vte_debug_print(VTE_DEBUG_EVENTS, "Motion x=%.3f y=%.3f\n",
                         mouse_event.x(), mouse_event.y());

        return m_terminal->widget_mouse_motion(mouse_event);
}

#endif /* VTE_GTK == 3 */

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
        auto str = vte::glib::StringPtr{};
        auto attrs = vte::Freeable<PangoAttrList>{};
        auto cursorpos = 0;
        gtk_im_context_get_preedit_string(m_im_context.get(),
                                          vte::glib::StringGetter{str},
                                          vte::get_freeable(attrs),
                                          &cursorpos);
        _vte_debug_print(VTE_DEBUG_EVENTS, "Input method pre-edit changed (%s,%d).\n",
                         str.get(), cursorpos);

        if (str)
                m_terminal->im_preedit_changed(str.get(), cursorpos, std::move(attrs));
}

void
Widget::im_set_cursor_location(cairo_rectangle_int_t const* rect) noexcept
{
        gtk_im_context_set_cursor_location(m_im_context.get(), rect);
}

#if VTE_GTK == 3

unsigned
Widget::read_modifiers_from_gdk(GdkEvent* event) const noexcept
{
        /* Read the modifiers. See bug #663779 for more information on why we do this. */
        auto mods = GdkModifierType{};
        if (!gdk_event_get_state(event, &mods))
                return 0;

        /* HACK! Treat META as ALT; see bug #663779. */
        if (mods & GDK_META_MASK)
                mods = GdkModifierType(mods | GDK_MOD1_MASK);

        /* Map non-virtual modifiers to virtual modifiers (Super, Hyper, Meta) */
        auto display = gdk_window_get_display(gdk_event_get_window(event));
        auto keymap = gdk_keymap_get_for_display(display);
        gdk_keymap_add_virtual_modifiers(keymap, &mods);

        return unsigned(mods);
}

#endif /* VTE_GTK == 3 */

unsigned
Widget::key_event_translate_ctrlkey(KeyEvent const& event) const noexcept
{
#if VTE_GTK == 3
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
#elif VTE_GTK == 4
        // FIXMEgtk4: find a way to do this on gtk4
        return event.keyval();
#endif
}

KeyEvent
Widget::key_event_from_gdk(GdkEvent* event) const
{
        auto type = EventBase::Type{};
        switch (gdk_event_get_event_type(event)) {
        case GDK_KEY_PRESS: type = KeyEvent::Type::eKEY_PRESS;     break;
        case GDK_KEY_RELEASE: type = KeyEvent::Type::eKEY_RELEASE; break;
        default: g_assert_not_reached(); return {};
        }

#if VTE_GTK == 3
        auto keyval = unsigned{};
        gdk_event_get_keyval(event, &keyval);
        auto const scancode = unsigned(reinterpret_cast<GdkEventKey*>(event)->hardware_keycode);
        auto const group = reinterpret_cast<GdkEventKey*>(event)->group;
        auto const is_modifier = reinterpret_cast<GdkEventKey*>(event)->is_modifier != 0;
#elif VTE_GTK == 4
        auto keyval = gdk_key_event_get_keyval(event);
        auto scancode = gdk_key_event_get_keycode(event);
        auto const group = gdk_key_event_get_level(event);
        auto const is_modifier = gdk_key_event_is_modifier(event) != false;
#endif /* VTE_GTK */

        return {event,
                type,
#if VTE_GTK == 3
                read_modifiers_from_gdk(event),
#elif VTE_GTK == 4
                gdk_event_get_modifier_state(event),
#endif
                keyval,
                scancode,
                group,
                is_modifier};
}

#if VTE_GTK == 3

MouseEvent
Widget::mouse_event_from_gdk(GdkEvent* event) const /* throws */
{
        auto type = EventBase::Type{};
        auto press_count = 0;
        switch (gdk_event_get_event_type(event)) {
        case GDK_2BUTTON_PRESS:
                type = MouseEvent::Type::eMOUSE_PRESS;
                press_count = 2;
                break;
        case GDK_3BUTTON_PRESS:
                type = MouseEvent::Type::eMOUSE_PRESS;
                press_count = 3;
                break;
        case GDK_BUTTON_PRESS:
                type = MouseEvent::Type::eMOUSE_PRESS;
                press_count = 1;
                break;
        case GDK_BUTTON_RELEASE:
                type = MouseEvent::Type::eMOUSE_RELEASE;
                press_count = 1;
                break;
        case GDK_ENTER_NOTIFY:   type = MouseEvent::Type::eMOUSE_ENTER;        break;
        case GDK_LEAVE_NOTIFY:   type = MouseEvent::Type::eMOUSE_LEAVE;        break;
        case GDK_MOTION_NOTIFY:  type = MouseEvent::Type::eMOUSE_MOTION;       break;
        case GDK_SCROLL:
                type = MouseEvent::Type::eMOUSE_SCROLL;
                press_count = 1;
                break;
        default:
                throw std::runtime_error{"Unexpected event type"};
        }

        auto x = double{};
        auto y = double{};
        if (gdk_event_get_window(event) != m_event_window ||
            !gdk_event_get_coords(event, &x, &y))
                x = y = -1.; // FIXMEchpe or throw?

        auto button = 0u;
        (void)gdk_event_get_button(event, &button);

        return {type,
                press_count,
                read_modifiers_from_gdk(event),
                MouseEvent::Button(button),
                x,
                y};
}

/* Emit an adjustment changed signal on our adjustment object. */
void
Widget::notify_scroll_bounds_changed(long lower,
                                     long upper,
                                     long row_count)
{
        auto const freezer = vte::glib::FreezeObjectNotify{m_vadjustment.get()};
        auto changed = false;

        auto const dlower = double(lower);
        auto const dupper = double(upper);

        auto current = gtk_adjustment_get_lower(m_vadjustment.get());
        if (!_vte_double_equal(current, dlower)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Changing lower bound from %.0f to %f\n",
                                 current, dlower);
                gtk_adjustment_set_lower(m_vadjustment.get(), dlower);
                changed = true;
        }

        current = gtk_adjustment_get_upper(m_vadjustment.get());
        if (!_vte_double_equal(current, dupper)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Changing upper bound from %.0f to %f\n",
                                 current, dupper);
                gtk_adjustment_set_upper(m_vadjustment.get(), dupper);
                changed = true;
        }

        /* The step increment should always be one. */
        auto v = gtk_adjustment_get_step_increment(m_vadjustment.get());
        if (!_vte_double_equal(v, 1.)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Changing step increment from %.0lf to 1.0\n", v);
                gtk_adjustment_set_step_increment(m_vadjustment.get(), 1.);
                changed = true;
        }

        v = gtk_adjustment_get_page_size(m_vadjustment.get());
        if (!_vte_double_equal(v, row_count)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Changing page size from %.0f to %ld\n",
                                 v, row_count);
                gtk_adjustment_set_page_size(m_vadjustment.get(), row_count);
                changed = true;
        }

        /* Clicking in the empty area should scroll exactly one screen,
         * so set the page size to the number of visible rows.
         */
        v = gtk_adjustment_get_page_increment(m_vadjustment.get());
        if (!_vte_double_equal(v, row_count)) {
                _vte_debug_print(VTE_DEBUG_ADJ,
                                 "Changing page increment from "
                                 "%.0f to %ld\n",
                                 v, row_count);
                gtk_adjustment_set_page_increment(m_vadjustment.get(), row_count);
                changed = true;
        }

        if (changed)
                _vte_debug_print(VTE_DEBUG_SIGNALS,
                                 "Adjustment changed.\n");
}

void
Widget::notify_scroll_value_changed(double value)
{
        auto const v = gtk_adjustment_get_value(m_vadjustment.get());
        if (!_vte_double_equal(v, value)) {
                /* Note that this will generate a 'value-changed' signal */
                gtk_adjustment_set_value(m_vadjustment.get(), value);
        }
}

ScrollEvent
Widget::scroll_event_from_gdk(GdkEvent* event) const /* throws */
{
        auto dx = double{}, dy = double{};
        if (!gdk_event_get_scroll_deltas(event, &dx, &dy)) {
                auto dir = GdkScrollDirection{};
                if (!gdk_event_get_scroll_direction(event, &dir))
                        __builtin_unreachable();

                switch (dir) {
                case GDK_SCROLL_UP:     dx =  0.; dy = -1.; break;
                case GDK_SCROLL_DOWN:   dx =  0.; dy =  1.; break;
                case GDK_SCROLL_LEFT:   dx = -1.; dy =  0.; break;
                case GDK_SCROLL_RIGHT:  dx =  1.; dy =  0.; break;
                case GDK_SCROLL_SMOOTH: break;
                default: __builtin_unreachable();
                }
        }

        return {read_modifiers_from_gdk(event),
                dx, dy};
}

#endif /* VTE_GTK == 3 */

void
Widget::map() noexcept
{
#if VTE_GTK == 3
        if (m_event_window)
                gdk_window_show_unraised(m_event_window);
#endif
}

#if VTE_GTK == 4

void
Widget::measure(GtkOrientation orientation,
                int for_size,
                int* minimum,
                int* natural,
                int* minimum_baseline,
                int* natural_baseline)
{
        _vte_debug_print(VTE_DEBUG_WIDGET_SIZE, "Widget measure for_size=%d orientation=%s\n",
                         for_size,
                         orientation == GTK_ORIENTATION_HORIZONTAL ? "horizontal" : "vertical");

        switch (orientation) {
        case GTK_ORIENTATION_HORIZONTAL:
                terminal()->widget_measure_width(minimum, natural);
                break;
        case GTK_ORIENTATION_VERTICAL:
                *minimum_baseline = *natural_baseline = 0;
                terminal()->widget_measure_height(minimum, natural);
                break;
        }
}

#endif /* VTE_GTK == 4 */

void
Widget::padding_changed()
{
#if VTE_GTK == 3
        auto padding = GtkBorder{};
        auto context = gtk_widget_get_style_context(gtk());
        gtk_style_context_get_padding(context,
#if VTE_GTK == 3
                                      gtk_style_context_get_state(context),
#endif
                                      &padding);
        terminal()->set_border_padding(&padding);
#endif /* VTE_GTK FIXMEgtk4 how to handle margin/padding? */
}

bool
Widget::primary_paste_enabled() const noexcept
{
        auto primary_paste = gboolean{};
        g_object_get(m_settings.get(),
                     "gtk-enable-primary-paste", &primary_paste,
                     nullptr);

        return primary_paste != false;
}

bool
Widget::query_tooltip(int x,
                      int y,
                      bool keyboard,
                      GtkTooltip* tooltip) noexcept
{
        return false;
}

void
Widget::realize() noexcept
{
        //        m_mouse_cursor_over_widget = false;  /* We'll receive an enter_notify_event if the window appears under the cursor. */

	/* Create stock cursors */
	m_default_cursor = create_cursor(VTE_DEFAULT_CURSOR);
	m_invisible_cursor = create_cursor("none"s);
	m_mousing_cursor = create_cursor(VTE_MOUSING_CURSOR);
        if (_vte_debug_on(VTE_DEBUG_HYPERLINK))
                /* Differ from the standard regex match cursor in debug mode. */
                m_hyperlink_cursor = create_cursor(VTE_HYPERLINK_CURSOR_DEBUG);
        else
                m_hyperlink_cursor = create_cursor(VTE_HYPERLINK_CURSOR);

#if VTE_GTK == 3
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
#endif /* VTE_GTK == 3 */

        assert(!m_im_context);
        m_im_context = vte::glib::take_ref(gtk_im_multicontext_new());
#if (VTE_GTK == 3 && GTK_CHECK_VERSION (3, 24, 14)) || VTE_GTK == 4
        g_object_set(m_im_context.get(),
                     "input-purpose", GTK_INPUT_PURPOSE_TERMINAL,
                     nullptr);
#endif

#if VTE_GTK == 3
	gtk_im_context_set_client_window(m_im_context.get(), m_event_window);
#elif VTE_GTK == 4
        gtk_im_context_set_client_widget(m_im_context.get(), gtk());
#endif
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

        m_clipboard = std::make_shared<Clipboard>(*this, ClipboardType::CLIPBOARD);
        m_primary_clipboard = std::make_shared<Clipboard>(*this, ClipboardType::PRIMARY);

        m_terminal->widget_realize();
}

#if VTE_GTK == 4

void
Widget::root()
{
}

#endif /* VTE_GTK == 4 */

#if VTE_GTK == 3

void
Widget::screen_changed(GdkScreen *previous_screen) noexcept
{
        auto gdk_screen = gtk_widget_get_screen(m_widget);
        if (gdk_screen == previous_screen || gdk_screen == nullptr)
                return;

        connect_settings();
}

#elif VTE_GTK == 4

void
Widget::display_changed() noexcept
{
        /* There appears to be no way to retrieve the previous display */
        connect_settings();
}

#endif /* VTE_GTK */

void
Widget::connect_settings()
{
        auto settings = vte::glib::make_ref(gtk_widget_get_settings(m_widget));
        if (settings == m_settings)
                return;

        if (m_settings)
                g_signal_handlers_disconnect_matched(m_settings.get(), G_SIGNAL_MATCH_DATA,
                                                     0, 0, nullptr, nullptr,
                                                     this);

        m_settings = std::move(settings);

        settings_changed();

        g_signal_connect(m_settings.get(), "notify::gtk-cursor-blink",
                         G_CALLBACK(settings_notify_cb), this);
        g_signal_connect(m_settings.get(), "notify::gtk-cursor-blink-time",
                         G_CALLBACK(settings_notify_cb), this);
        g_signal_connect(m_settings.get(), "notify::gtk-cursor-blink-timeout",
                         G_CALLBACK(settings_notify_cb), this);
#if VTE_GTK == 4
        g_signal_connect(m_settings.get(), "notify::gtk-cursor-aspect-ratio",
                         G_CALLBACK(settings_notify_cb), this);
#endif
}

void
Widget::settings_changed()
{
        auto blink = gboolean{};
        auto blink_time = int{};
        auto blink_timeout = int{};
#if VTE_GTK == 4
        auto aspect = double{};
#endif
        g_object_get(m_settings.get(),
                     "gtk-cursor-blink", &blink,
                     "gtk-cursor-blink-time", &blink_time,
                     "gtk-cursor-blink-timeout", &blink_timeout,
#if VTE_GTK == 4
                     "gtk-cursor-aspect-ratio", &aspect,
#endif
                     nullptr);

        _vte_debug_print(VTE_DEBUG_MISC,
                         "Cursor blinking settings: blink=%d time=%d timeout=%d\n",
                         blink, blink_time, blink_timeout);

        m_terminal->set_blink_settings(blink, blink_time, blink_timeout);

#if VTE_GTK == 4
        m_terminal->set_cursor_aspect(aspect);
#endif
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

void
Widget::set_hscroll_policy(GtkScrollablePolicy policy)
{
        m_hscroll_policy = policy;

#if VTE_GTK == 3
        gtk_widget_queue_resize_no_redraw(gtk());
#elif VTE_GTK == 4
        gtk_widget_queue_resize(gtk());
#endif
}

void
Widget::set_vscroll_policy(GtkScrollablePolicy policy)
{
        m_vscroll_policy = policy;

#if VTE_GTK == 3
        gtk_widget_queue_resize_no_redraw(gtk());
#elif VTE_GTK == 4
        gtk_widget_queue_resize(gtk());
#endif
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


static void
vadjustment_value_changed_cb(vte::platform::Widget* that) noexcept
try
{
        that->vadjustment_value_changed();
}
catch (...)
{
        vte::log_exception();
}

void
Widget::set_vadjustment(vte::glib::RefPtr<GtkAdjustment> adjustment)
{
        if (adjustment && adjustment == m_vadjustment)
                return;
        if (!adjustment && m_vadjustment)
                return;

        if (m_vadjustment) {
                g_signal_handlers_disconnect_by_func(m_vadjustment.get(),
                                                     (void*)vadjustment_value_changed_cb,
                                                     this);
        }

        if (adjustment)
                m_vadjustment = std::move(adjustment);
        else
                m_vadjustment = vte::glib::make_ref_sink(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 0, 0, 0)));

        /* We care about the offset only, not the top or bottom. */
        g_signal_connect_swapped(m_vadjustment.get(),
                                 "value-changed",
                                 G_CALLBACK(vadjustment_value_changed_cb),
                                 this);
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

#if VTE_GTK == 3

void
Widget::size_allocate(GtkAllocation* allocation)
{
        _vte_debug_print(VTE_DEBUG_WIDGET_SIZE, "Widget size allocate width=%d height=%d x=%d y=%d\n",
                         allocation->width, allocation->height, allocation->x, allocation->y);

        m_terminal->widget_size_allocate(allocation->x, allocation->y,
                                         allocation->width, allocation->height,
                                         -1);

        gtk_widget_set_allocation(gtk(), allocation);

        if (realized())
		gdk_window_move_resize(m_event_window,
                                       allocation->x,
                                       allocation->y,
                                       allocation->width,
                                       allocation->height);
}

#elif VTE_GTK == 4

void
Widget::size_allocate(int width,
                      int height,
                      int baseline)
{
        _vte_debug_print(VTE_DEBUG_WIDGET_SIZE, "Widget size allocate width=%d height=%d baseline=%d\n",
                         width, height, baseline);

        terminal()->widget_size_allocate(width, height, baseline);

        gtk_widget_allocate(gtk(), width, height, baseline, nullptr);
}

#endif /* VTE_GTK */

bool
Widget::should_emit_signal(int id) noexcept
{
        return g_signal_has_handler_pending(object(),
                                            signals[id],
                                            0 /* detail */,
                                            false /* not interested in blocked handlers */) != FALSE;
}

void
Widget::state_flags_changed(GtkStateFlags old_flags)
{
        _vte_debug_print(VTE_DEBUG_STYLE, "Widget state flags changed\n");
}

#if VTE_GTK == 4

void
Widget::snapshot(GtkSnapshot* snapshot_object)
{
        _vte_debug_print(VTE_DEBUG_DRAW, "Widget snapshot\n");

        auto rect = terminal()->allocated_rect();
        auto region = vte::take_freeable(cairo_region_create_rectangle(rect));
        auto grect = vte::graphene::make_rect(rect);
        auto cr = vte::take_freeable(gtk_snapshot_append_cairo(snapshot_object, &grect));
        terminal()->draw(cr.get(), region.get());
}

#endif /* VTE_GTK == 4 */

#if VTE_GTK == 3

void
Widget::style_updated() noexcept
{
        _vte_debug_print(VTE_DEBUG_STYLE, "Widget style changed\n");

        padding_changed();

        auto aspect = float{};
        gtk_widget_style_get(gtk(), "cursor-aspect-ratio", &aspect, nullptr);
        m_terminal->set_cursor_aspect(aspect);

        m_terminal->widget_style_updated();
}

#endif /* VTE_GTK == 3 */

#if VTE_GTK == 4

void
Widget::system_setting_changed(GtkSystemSetting setting)
{
        _vte_debug_print(VTE_DEBUG_STYLE, "Widget system settings %d changed\n", int(setting));

        switch (setting) {
        case GTK_SYSTEM_SETTING_DISPLAY:
                display_changed();
                break;

        case GTK_SYSTEM_SETTING_DPI:
                break;

        case GTK_SYSTEM_SETTING_FONT_CONFIG:
                break;

        case GTK_SYSTEM_SETTING_FONT_NAME:
                break;

        case GTK_SYSTEM_SETTING_ICON_THEME:
                break;

        default:
                break;
        }
}

#endif /* VTE_GTK == 4 */

void
Widget::unmap() noexcept
{
        m_terminal->widget_unmap();

#if VTE_GTK == 3
        if (m_event_window)
                gdk_window_hide(m_event_window);
#endif
}

void
Widget::unrealize() noexcept
{
        m_terminal->widget_unrealize();

        // FIXMEgtk4 only withdraw content from clipboard, not unselect?
        if (m_clipboard) {
                terminal()->widget_clipboard_data_clear(*m_clipboard);
                m_clipboard->disown();
                m_clipboard.reset();
        }
        if (m_primary_clipboard) {
                terminal()->widget_clipboard_data_clear(*m_primary_clipboard);
                m_primary_clipboard->disown();
                m_primary_clipboard.reset();
        }

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
#if VTE_GTK == 3
        gtk_im_context_set_client_window(m_im_context.get(), nullptr);
#elif VTE_GTK == 4
        gtk_im_context_set_client_widget(m_im_context.get(), nullptr);
#endif
        m_im_context.reset();

#if VTE_GTK == 3
        /* Destroy input window */
        gtk_widget_unregister_window(m_widget, m_event_window);
        gdk_window_destroy(m_event_window);
        m_event_window = nullptr;
#endif /* VTE_GTK == 3 */
}

#if VTE_GTK == 4

void
Widget::unroot()
{
}

#endif /* VTE_GTK == 4 */

void
Widget::vadjustment_value_changed()
{
        if (!m_terminal)
                return;

        auto const adj = gtk_adjustment_get_value(m_vadjustment.get());
        m_terminal->set_scroll_value(adj);
}

} // namespace platform

} // namespace vte
