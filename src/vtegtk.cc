/*
 * Copyright (C) 2001-2004,2009,2010 Red Hat, Inc.
 * Copyright © 2008, 2009, 2010, 2015 Christian Persch
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: vte-terminal
 * @short_description: A terminal widget implementation
 *
 * A VteTerminal is a terminal emulator implemented as a GTK3 widget.
 */

#include "config.h"

#include <new> /* placement new */

#include <pwd.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "vte/vteenums.h"
#include "vte/vtepty.h"
#include "vte/vteterminal.h"
#include "vte/vtetypebuiltins.h"

#include "debug.h"
#include "marshal.h"
#include "vtedefines.hh"
#include "vteinternal.hh"
#include "vteaccess.h"

#include "vtegtk.hh"

#if !GLIB_CHECK_VERSION(2, 42, 0)
#define G_PARAM_EXPLICIT_NOTIFY 0
#endif

#define I_(string) (g_intern_static_string(string))

guint signals[LAST_SIGNAL];
GParamSpec *pspecs[LAST_PROP];
GTimer *process_timer;

struct _VteTerminalClassPrivate {
        GtkStyleProvider *style_provider;
};

static void
vte_terminal_set_hadjustment(VteTerminal *terminal,
                             GtkAdjustment *adjustment)
{
        g_return_if_fail(adjustment == nullptr || GTK_IS_ADJUSTMENT(adjustment));
        terminal->pvt->widget_set_hadjustment(adjustment);
}

static void
vte_terminal_set_vadjustment(VteTerminal *terminal,
                             GtkAdjustment *adjustment)
{
        g_return_if_fail(adjustment == nullptr || GTK_IS_ADJUSTMENT(adjustment));
        terminal->pvt->widget_set_vadjustment(adjustment);
}

static void
vte_terminal_set_hscroll_policy(VteTerminal *terminal,
                                GtkScrollablePolicy policy)
{
        terminal->pvt->hscroll_policy = policy;
        gtk_widget_queue_resize_no_redraw (GTK_WIDGET (terminal));
}


static void
vte_terminal_set_vscroll_policy(VteTerminal *terminal,
                                GtkScrollablePolicy policy)
{
        terminal->pvt->vscroll_policy = policy;
        gtk_widget_queue_resize_no_redraw (GTK_WIDGET (terminal));
}

#ifdef VTE_DEBUG
G_DEFINE_TYPE_WITH_CODE(VteTerminal, vte_terminal, GTK_TYPE_WIDGET,
                        g_type_add_class_private (g_define_type_id, sizeof (VteTerminalClassPrivate));
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL)
                        if (_vte_debug_on(VTE_DEBUG_LIFECYCLE)) {
                                g_printerr("vte_terminal_get_type()\n");
                        })
#else
G_DEFINE_TYPE_WITH_CODE(VteTerminal, vte_terminal, GTK_TYPE_WIDGET,
                        g_type_add_class_private (g_define_type_id, sizeof (VteTerminalClassPrivate));
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))
#endif

static void
vte_terminal_real_copy_clipboard(VteTerminal *terminal)
{
	terminal->pvt->widget_copy(VTE_SELECTION_CLIPBOARD);
}

static void
vte_terminal_real_paste_clipboard(VteTerminal *terminal)
{
	terminal->pvt->widget_paste(GDK_SELECTION_CLIPBOARD);
}

static void
vte_terminal_style_updated (GtkWidget *widget)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);

        GTK_WIDGET_CLASS (vte_terminal_parent_class)->style_updated (widget);

        terminal->pvt->widget_style_updated();
}

static gboolean
vte_terminal_key_press(GtkWidget *widget, GdkEventKey *event)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);

        /* We do NOT want chain up to GtkWidget::key-press-event, since that would
         * cause GtkWidget's keybindings to be handled and consumed. However we'll
         * have to handle the one sane binding (Shift-F10 or MenuKey, to pop up the
         * context menu) ourself, so for now we simply skip the offending keybinding
         * in class_init.
         */

	/* First, check if GtkWidget's behavior already does something with
	 * this key. */
	if (GTK_WIDGET_CLASS(vte_terminal_parent_class)->key_press_event) {
		if ((GTK_WIDGET_CLASS(vte_terminal_parent_class))->key_press_event(widget,
                                                                                   event)) {
			return TRUE;
		}
	}

        return terminal->pvt->widget_key_press(event);
}

static gboolean
vte_terminal_key_release(GtkWidget *widget, GdkEventKey *event)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        return terminal->pvt->widget_key_release(event);
}

static gboolean
vte_terminal_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
        VteTerminal *terminal = VTE_TERMINAL(widget);
        return terminal->pvt->widget_motion_notify(event);
}

static gboolean
vte_terminal_button_press(GtkWidget *widget, GdkEventButton *event)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        return terminal->pvt->widget_button_press(event);
}

static gboolean
vte_terminal_button_release(GtkWidget *widget, GdkEventButton *event)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        return terminal->pvt->widget_button_release(event);
}

static gboolean
vte_terminal_scroll(GtkWidget *widget, GdkEventScroll *event)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        terminal->pvt->widget_scroll(event);
        return TRUE;
}

static gboolean
vte_terminal_focus_in(GtkWidget *widget, GdkEventFocus *event)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        terminal->pvt->widget_focus_in(event);
        return FALSE;
}

static gboolean
vte_terminal_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        terminal->pvt->widget_focus_out(event);
        return FALSE;
}

static gboolean
vte_terminal_enter(GtkWidget *widget, GdkEventCrossing *event)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        gboolean ret = FALSE;

	if (GTK_WIDGET_CLASS (vte_terminal_parent_class)->enter_notify_event) {
		ret = GTK_WIDGET_CLASS (vte_terminal_parent_class)->enter_notify_event (widget, event);
	}

        terminal->pvt->widget_enter(event);

        return ret;
}

static gboolean
vte_terminal_leave(GtkWidget *widget, GdkEventCrossing *event)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
	gboolean ret = FALSE;

	if (GTK_WIDGET_CLASS (vte_terminal_parent_class)->leave_notify_event) {
		ret = GTK_WIDGET_CLASS (vte_terminal_parent_class)->leave_notify_event (widget, event);
	}

        terminal->pvt->widget_leave(event);

        return ret;
}

static gboolean
vte_terminal_visibility_notify(GtkWidget *widget, GdkEventVisibility *event)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        terminal->pvt->widget_visibility_notify(event);
	return FALSE;
}

static void
vte_terminal_get_preferred_width(GtkWidget *widget,
				 int       *minimum_width,
				 int       *natural_width)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        terminal->pvt->widget_get_preferred_width(minimum_width, natural_width);
}

static void
vte_terminal_get_preferred_height(GtkWidget *widget,
				  int       *minimum_height,
				  int       *natural_height)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        terminal->pvt->widget_get_preferred_height(minimum_height, natural_height);
}

static void
vte_terminal_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	VteTerminal *terminal = VTE_TERMINAL(widget);
        terminal->pvt->widget_size_allocate(allocation);
}

static gboolean
vte_terminal_draw(GtkWidget *widget,
                  cairo_t *cr)
{
        VteTerminal *terminal = VTE_TERMINAL (widget);
        terminal->pvt->widget_draw(cr);
        return FALSE;
}

static void
vte_terminal_realize(GtkWidget *widget)
{
        VteTerminal *terminal= VTE_TERMINAL(widget);
        terminal->pvt->widget_realize();
}

static void
vte_terminal_unrealize(GtkWidget *widget)
{
        VteTerminal *terminal = VTE_TERMINAL (widget);
        terminal->pvt->widget_unrealize();
}


static void
vte_terminal_screen_changed (GtkWidget *widget,
                             GdkScreen *previous_screen)
{
        VteTerminal *terminal = VTE_TERMINAL (widget);

        if (GTK_WIDGET_CLASS (vte_terminal_parent_class)->screen_changed) {
                GTK_WIDGET_CLASS (vte_terminal_parent_class)->screen_changed (widget, previous_screen);
        }

        terminal->pvt->widget_screen_changed(previous_screen);
}

static void
vte_terminal_init(VteTerminal *terminal)
{
        void *place;
	GtkStyleContext *context;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "vte_terminal_init()\n");

        context = gtk_widget_get_style_context(&terminal->widget);
        gtk_style_context_add_provider (context,
                                        VTE_TERMINAL_GET_CLASS (terminal)->priv->style_provider,
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* Initialize private data. NOTE: place is zeroed */
	place = G_TYPE_INSTANCE_GET_PRIVATE (terminal, VTE_TYPE_TERMINAL, VteTerminalPrivate);
        terminal->pvt = new (place) VteTerminalPrivate(terminal);
}

static void
vte_terminal_finalize(GObject *object)
{
    	VteTerminal *terminal = VTE_TERMINAL (object);

        terminal->pvt->~VteTerminalPrivate();
        terminal->pvt = nullptr;

	/* Call the inherited finalize() method. */
	G_OBJECT_CLASS(vte_terminal_parent_class)->finalize(object);
}

static void
vte_terminal_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
        VteTerminal *terminal = VTE_TERMINAL (object);
        VteTerminalPrivate *pvt = terminal->pvt;

	switch (prop_id)
                {
                case PROP_HADJUSTMENT:
                        g_value_set_object (value, pvt->hadjustment);
                        break;
                case PROP_VADJUSTMENT:
                        g_value_set_object (value, terminal->pvt->vadjustment);
                        break;
                case PROP_HSCROLL_POLICY:
                        g_value_set_enum (value, pvt->hscroll_policy);
                        break;
                case PROP_VSCROLL_POLICY:
                        g_value_set_enum (value, pvt->vscroll_policy);
                        break;
                case PROP_ALLOW_BOLD:
                        g_value_set_boolean (value, vte_terminal_get_allow_bold (terminal));
                        break;
                case PROP_AUDIBLE_BELL:
                        g_value_set_boolean (value, vte_terminal_get_audible_bell (terminal));
                        break;
                case PROP_BACKSPACE_BINDING:
                        g_value_set_enum (value, pvt->backspace_binding);
                        break;
                case PROP_CJK_AMBIGUOUS_WIDTH:
                        g_value_set_int (value, vte_terminal_get_cjk_ambiguous_width (terminal));
                        break;
                case PROP_CURSOR_BLINK_MODE:
                        g_value_set_enum (value, vte_terminal_get_cursor_blink_mode (terminal));
                        break;
                case PROP_CURRENT_DIRECTORY_URI:
                        g_value_set_string (value, vte_terminal_get_current_directory_uri (terminal));
                        break;
                case PROP_CURRENT_FILE_URI:
                        g_value_set_string (value, vte_terminal_get_current_file_uri (terminal));
                        break;
                case PROP_CURSOR_SHAPE:
                        g_value_set_enum (value, vte_terminal_get_cursor_shape (terminal));
                        break;
                case PROP_DELETE_BINDING:
                        g_value_set_enum (value, pvt->delete_binding);
                        break;
                case PROP_ENCODING:
                        g_value_set_string (value, vte_terminal_get_encoding (terminal));
                        break;
                case PROP_FONT_DESC:
                        g_value_set_boxed (value, vte_terminal_get_font (terminal));
                        break;
                case PROP_FONT_SCALE:
                        g_value_set_double (value, vte_terminal_get_font_scale (terminal));
                        break;
                case PROP_ICON_TITLE:
                        g_value_set_string (value, vte_terminal_get_icon_title (terminal));
                        break;
                case PROP_INPUT_ENABLED:
                        g_value_set_boolean (value, vte_terminal_get_input_enabled (terminal));
                        break;
                case PROP_MOUSE_POINTER_AUTOHIDE:
                        g_value_set_boolean (value, vte_terminal_get_mouse_autohide (terminal));
                        break;
                case PROP_PTY:
                        g_value_set_object (value, vte_terminal_get_pty(terminal));
                        break;
                case PROP_REWRAP_ON_RESIZE:
                        g_value_set_boolean (value, vte_terminal_get_rewrap_on_resize (terminal));
                        break;
                case PROP_SCROLLBACK_LINES:
                        g_value_set_uint (value, pvt->scrollback_lines);
                        break;
                case PROP_SCROLL_ON_KEYSTROKE:
                        g_value_set_boolean (value, pvt->scroll_on_keystroke);
                        break;
                case PROP_SCROLL_ON_OUTPUT:
                        g_value_set_boolean (value, pvt->scroll_on_output);
                        break;
                case PROP_WINDOW_TITLE:
                        g_value_set_string (value, vte_terminal_get_window_title (terminal));
                        break;
                case PROP_WORD_CHAR_EXCEPTIONS:
                        g_value_set_string (value, vte_terminal_get_word_char_exceptions (terminal));
                        break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			return;
                }
}

static void
vte_terminal_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
        VteTerminal *terminal = VTE_TERMINAL (object);

	switch (prop_id)
                {
                case PROP_HADJUSTMENT:
                        vte_terminal_set_hadjustment (terminal, (GtkAdjustment *)g_value_get_object (value));
                        break;
                case PROP_VADJUSTMENT:
                        vte_terminal_set_vadjustment (terminal, (GtkAdjustment *)g_value_get_object (value));
                        break;
                case PROP_HSCROLL_POLICY:
                        vte_terminal_set_hscroll_policy(terminal, (GtkScrollablePolicy)g_value_get_enum(value));
                        break;
                case PROP_VSCROLL_POLICY:
                        vte_terminal_set_vscroll_policy(terminal, (GtkScrollablePolicy)g_value_get_enum(value));
                        break;
                case PROP_ALLOW_BOLD:
                        vte_terminal_set_allow_bold (terminal, g_value_get_boolean (value));
                        break;
                case PROP_AUDIBLE_BELL:
                        vte_terminal_set_audible_bell (terminal, g_value_get_boolean (value));
                        break;
                case PROP_BACKSPACE_BINDING:
                        vte_terminal_set_backspace_binding (terminal, (VteEraseBinding)g_value_get_enum (value));
                        break;
                case PROP_CJK_AMBIGUOUS_WIDTH:
                        vte_terminal_set_cjk_ambiguous_width (terminal, g_value_get_int (value));
                        break;
                case PROP_CURSOR_BLINK_MODE:
                        vte_terminal_set_cursor_blink_mode (terminal, (VteCursorBlinkMode)g_value_get_enum (value));
                        break;
                case PROP_CURSOR_SHAPE:
                        vte_terminal_set_cursor_shape (terminal, (VteCursorShape)g_value_get_enum (value));
                        break;
                case PROP_DELETE_BINDING:
                        vte_terminal_set_delete_binding (terminal, (VteEraseBinding)g_value_get_enum (value));
                        break;
                case PROP_ENCODING:
                        vte_terminal_set_encoding (terminal, g_value_get_string (value), NULL);
                        break;
                case PROP_FONT_DESC:
                        vte_terminal_set_font (terminal, (PangoFontDescription *)g_value_get_boxed (value));
                        break;
                case PROP_FONT_SCALE:
                        vte_terminal_set_font_scale (terminal, g_value_get_double (value));
                        break;
                case PROP_INPUT_ENABLED:
                        vte_terminal_set_input_enabled (terminal, g_value_get_boolean (value));
                        break;
                case PROP_MOUSE_POINTER_AUTOHIDE:
                        vte_terminal_set_mouse_autohide (terminal, g_value_get_boolean (value));
                        break;
                case PROP_PTY:
                        vte_terminal_set_pty (terminal, (VtePty *)g_value_get_object (value));
                        break;
                case PROP_REWRAP_ON_RESIZE:
                        vte_terminal_set_rewrap_on_resize (terminal, g_value_get_boolean (value));
                        break;
                case PROP_SCROLLBACK_LINES:
                        vte_terminal_set_scrollback_lines (terminal, g_value_get_uint (value));
                        break;
                case PROP_SCROLL_ON_KEYSTROKE:
                        vte_terminal_set_scroll_on_keystroke(terminal, g_value_get_boolean (value));
                        break;
                case PROP_SCROLL_ON_OUTPUT:
                        vte_terminal_set_scroll_on_output (terminal, g_value_get_boolean (value));
                        break;
                case PROP_WORD_CHAR_EXCEPTIONS:
                        vte_terminal_set_word_char_exceptions (terminal, g_value_get_string (value));
                        break;

                        /* Not writable */
                case PROP_CURRENT_DIRECTORY_URI:
                case PROP_CURRENT_FILE_URI:
                case PROP_ICON_TITLE:
                case PROP_WINDOW_TITLE:
                        g_assert_not_reached ();
                        break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			return;
                }
}

static void
vte_terminal_class_init(VteTerminalClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;
	GtkBindingSet  *binding_set;

#ifdef VTE_DEBUG
	{
                _vte_debug_init();
		_vte_debug_print(VTE_DEBUG_LIFECYCLE,
                                 "vte_terminal_class_init()\n");
		/* print out the legend */
		_vte_debug_print(VTE_DEBUG_WORK,
                                 "Debugging work flow (top input to bottom output):\n"
                                 "  .  _vte_terminal_process_incoming\n"
                                 "  <  start process_timeout\n"
                                 "  {[ start update_timeout  [ => rate limited\n"
                                 "  T  start of terminal in update_timeout\n"
                                 "  (  start _vte_terminal_process_incoming\n"
                                 "  ?  _vte_invalidate_cells (call)\n"
                                 "  !  _vte_invalidate_cells (dirty)\n"
                                 "  *  _vte_invalidate_all\n"
                                 "  )  end _vte_terminal_process_incoming\n"
                                 "  -  gdk_window_process_updates\n"
                                 "  =  vte_terminal_paint\n"
                                 "  ]} end update_timeout\n"
                                 "  >  end process_timeout\n");
	}
#endif

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
#ifdef HAVE_DECL_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif

	g_type_class_add_private(klass, sizeof (VteTerminalPrivate));

	gobject_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	/* Override some of the default handlers. */
	gobject_class->finalize = vte_terminal_finalize;
        gobject_class->get_property = vte_terminal_get_property;
        gobject_class->set_property = vte_terminal_set_property;
	widget_class->realize = vte_terminal_realize;
	widget_class->scroll_event = vte_terminal_scroll;
        widget_class->draw = vte_terminal_draw;
	widget_class->key_press_event = vte_terminal_key_press;
	widget_class->key_release_event = vte_terminal_key_release;
	widget_class->button_press_event = vte_terminal_button_press;
	widget_class->button_release_event = vte_terminal_button_release;
	widget_class->motion_notify_event = vte_terminal_motion_notify;
	widget_class->enter_notify_event = vte_terminal_enter;
	widget_class->leave_notify_event = vte_terminal_leave;
	widget_class->focus_in_event = vte_terminal_focus_in;
	widget_class->focus_out_event = vte_terminal_focus_out;
	widget_class->visibility_notify_event = vte_terminal_visibility_notify;
	widget_class->unrealize = vte_terminal_unrealize;
	widget_class->style_updated = vte_terminal_style_updated;
	widget_class->get_preferred_width = vte_terminal_get_preferred_width;
	widget_class->get_preferred_height = vte_terminal_get_preferred_height;
	widget_class->size_allocate = vte_terminal_size_allocate;
        widget_class->screen_changed = vte_terminal_screen_changed;

	/* Initialize default handlers. */
	klass->eof = NULL;
	klass->child_exited = NULL;
	klass->encoding_changed = NULL;
	klass->char_size_changed = NULL;
	klass->window_title_changed = NULL;
	klass->icon_title_changed = NULL;
	klass->selection_changed = NULL;
	klass->contents_changed = NULL;
	klass->cursor_moved = NULL;
	klass->commit = NULL;

	klass->deiconify_window = NULL;
	klass->iconify_window = NULL;
	klass->raise_window = NULL;
	klass->lower_window = NULL;
	klass->refresh_window = NULL;
	klass->restore_window = NULL;
	klass->maximize_window = NULL;
	klass->resize_window = NULL;
	klass->move_window = NULL;

	klass->increase_font_size = NULL;
	klass->decrease_font_size = NULL;

	klass->text_modified = NULL;
	klass->text_inserted = NULL;
	klass->text_deleted = NULL;
	klass->text_scrolled = NULL;

	klass->copy_clipboard = vte_terminal_real_copy_clipboard;
	klass->paste_clipboard = vte_terminal_real_paste_clipboard;

        klass->bell = NULL;

        /* GtkScrollable interface properties */
        g_object_class_override_property (gobject_class, PROP_HADJUSTMENT, "hadjustment");
        g_object_class_override_property (gobject_class, PROP_VADJUSTMENT, "vadjustment");
        g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
        g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

	/* Register some signals of our own. */

        /**
         * VteTerminal::eof:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the terminal receives an end-of-file from a child which
         * is running in the terminal.  This signal is frequently (but not
         * always) emitted with a #VteTerminal::child-exited signal.
         */
	g_signal_new(I_("eof"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, eof),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::child-exited:
         * @vteterminal: the object which received the signal
         * @status: the child's exit status
         *
         * This signal is emitted when the terminal detects that a child
         * watched using vte_terminal_watch_child() has exited.
         */
	g_signal_new(I_("child-exited"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, child_exited),
		     NULL,
		     NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE,
                     1, G_TYPE_INT);

        /**
         * VteTerminal::window-title-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the terminal's %window_title field is modified.
         */
	g_signal_new(I_("window-title-changed"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, window_title_changed),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::icon-title-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the terminal's %icon_title field is modified.
         */
	g_signal_new(I_("icon-title-changed"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, icon_title_changed),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::current-directory-uri-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the current directory URI is modified.
         */
	g_signal_new(I_("current-directory-uri-changed"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     0,
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::current-file-uri-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the current file URI is modified.
         */
	g_signal_new(I_("current-file-uri-changed"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     0,
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::encoding-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the terminal's current encoding has changed, either
         * as a result of receiving a control sequence which toggled between the
         * local and UTF-8 encodings, or at the parent application's request.
         */
	g_signal_new(I_("encoding-changed"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, encoding_changed),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::commit:
         * @vteterminal: the object which received the signal
         * @text: a string of text
         * @size: the length of that string of text
         *
         * Emitted whenever the terminal receives input from the user and
         * prepares to send it to the child process.  The signal is emitted even
         * when there is no child process.
         */
	g_signal_new(I_("commit"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, commit),
		     NULL,
		     NULL,
		     _vte_marshal_VOID__STRING_UINT,
		     G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_UINT);

        /**
         * VteTerminal::char-size-changed:
         * @vteterminal: the object which received the signal
         * @width: the new character cell width
         * @height: the new character cell height
         *
         * Emitted whenever selection of a new font causes the values of the
         * %char_width or %char_height fields to change.
         */
	g_signal_new(I_("char-size-changed"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, char_size_changed),
		     NULL,
		     NULL,
		     _vte_marshal_VOID__UINT_UINT,
		     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

        /**
         * VteTerminal::selection-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the contents of terminal's selection changes.
         */
	g_signal_new (I_("selection-changed"),
		      G_OBJECT_CLASS_TYPE(klass),
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET(VteTerminalClass, selection_changed),
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE, 0);

        /**
         * VteTerminal::contents-changed:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the visible appearance of the terminal has changed.
         * Used primarily by #VteTerminalAccessible.
         */
	g_signal_new(I_("contents-changed"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, contents_changed),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::cursor-moved:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever the cursor moves to a new character cell.  Used
         * primarily by #VteTerminalAccessible.
         */
	g_signal_new(I_("cursor-moved"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, cursor_moved),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::deiconify-window:
         * @vteterminal: the object which received the signal
         *
         * Emitted at the child application's request.
         */
	g_signal_new(I_("deiconify-window"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, deiconify_window),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::iconify-window:
         * @vteterminal: the object which received the signal
         *
         * Emitted at the child application's request.
         */
	g_signal_new(I_("iconify-window"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, iconify_window),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::raise-window:
         * @vteterminal: the object which received the signal
         *
         * Emitted at the child application's request.
         */
	g_signal_new(I_("raise-window"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, raise_window),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::lower-window:
         * @vteterminal: the object which received the signal
         *
         * Emitted at the child application's request.
         */
	g_signal_new(I_("lower-window"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, lower_window),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::refresh-window:
         * @vteterminal: the object which received the signal
         *
         * Emitted at the child application's request.
         */
	g_signal_new(I_("refresh-window"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, refresh_window),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::restore-window:
         * @vteterminal: the object which received the signal
         *
         * Emitted at the child application's request.
         */
	g_signal_new(I_("restore-window"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, restore_window),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::maximize-window:
         * @vteterminal: the object which received the signal
         *
         * Emitted at the child application's request.
         */
	g_signal_new(I_("maximize-window"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, maximize_window),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::resize-window:
         * @vteterminal: the object which received the signal
         * @width: the desired number of columns
         * @height: the desired number of rows
         *
         * Emitted at the child application's request.
         */
	g_signal_new(I_("resize-window"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, resize_window),
		     NULL,
		     NULL,
		     _vte_marshal_VOID__UINT_UINT,
		     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

        /**
         * VteTerminal::move-window:
         * @vteterminal: the object which received the signal
         * @x: the terminal's desired location, X coordinate
         * @y: the terminal's desired location, Y coordinate
         *
         * Emitted at the child application's request.
         */
	g_signal_new(I_("move-window"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, move_window),
		     NULL,
		     NULL,
		     _vte_marshal_VOID__UINT_UINT,
		     G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

        /**
         * VteTerminal::increase-font-size:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the user hits the '+' key while holding the Control key.
         */
	g_signal_new(I_("increase-font-size"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, increase_font_size),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::decrease-font-size:
         * @vteterminal: the object which received the signal
         *
         * Emitted when the user hits the '-' key while holding the Control key.
         */
	g_signal_new(I_("decrease-font-size"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, decrease_font_size),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-modified:
         * @vteterminal: the object which received the signal
         *
         * An internal signal used for communication between the terminal and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
	g_signal_new(I_("text-modified"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, text_modified),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-inserted:
         * @vteterminal: the object which received the signal
         *
         * An internal signal used for communication between the terminal and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
	g_signal_new(I_("text-inserted"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, text_inserted),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-deleted:
         * @vteterminal: the object which received the signal
         *
         * An internal signal used for communication between the terminal and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
	g_signal_new(I_("text-deleted"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, text_deleted),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__VOID,
		     G_TYPE_NONE, 0);

        /**
         * VteTerminal::text-scrolled:
         * @vteterminal: the object which received the signal
         * @delta: the number of lines scrolled
         *
         * An internal signal used for communication between the terminal and
         * its accessibility peer. May not be emitted under certain
         * circumstances.
         */
	g_signal_new(I_("text-scrolled"),
		     G_OBJECT_CLASS_TYPE(klass),
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET(VteTerminalClass, text_scrolled),
		     NULL,
		     NULL,
		     g_cclosure_marshal_VOID__INT,
		     G_TYPE_NONE, 1, G_TYPE_INT);

        /**
         * VteTerminal::copy-clipboard:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever vte_terminal_copy_clipboard() is called.
         */
	signals[COPY_CLIPBOARD] =
                g_signal_new(I_("copy-clipboard"),
			     G_OBJECT_CLASS_TYPE(klass),
			     (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
			     G_STRUCT_OFFSET(VteTerminalClass, copy_clipboard),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::paste-clipboard:
         * @vteterminal: the object which received the signal
         *
         * Emitted whenever vte_terminal_paste_clipboard() is called.
         */
	signals[PASTE_CLIPBOARD] =
                g_signal_new(I_("paste-clipboard"),
			     G_OBJECT_CLASS_TYPE(klass),
			     (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
			     G_STRUCT_OFFSET(VteTerminalClass, paste_clipboard),
			     NULL,
			     NULL,
                             g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);

        /**
         * VteTerminal::bell:
         * @vteterminal: the object which received the signal
         *
         * This signal is emitted when the a child sends a bell request to the
         * terminal.
         */
        g_signal_new(I_("bell"),
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(VteTerminalClass, bell),
                     NULL,
                     NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

        /**
         * VteTerminal:allow-bold:
         *
         * Controls whether or not the terminal will attempt to draw bold text.
         * This may happen either by using a bold font variant, or by
         * repainting text with a different offset.
         */
        pspecs[PROP_ALLOW_BOLD] =
                g_param_spec_boolean ("allow-bold", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:audible-bell:
         *
         * Controls whether or not the terminal will beep when the child outputs the
         * "bl" sequence.
         */
        pspecs[PROP_AUDIBLE_BELL] =
                g_param_spec_boolean ("audible-bell", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:backspace-binding:
         *
         * *Controls what string or control sequence the terminal sends to its child
         * when the user presses the backspace key.
         */
        pspecs[PROP_BACKSPACE_BINDING] =
                g_param_spec_enum ("backspace-binding", NULL, NULL,
                                   VTE_TYPE_ERASE_BINDING,
                                   VTE_ERASE_AUTO,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:cjk-ambiguous-width:
         *
         * This setting controls whether ambiguous-width characters are narrow or wide
         * when using the UTF-8 encoding (vte_terminal_set_encoding()). In all other encodings,
         * the width of ambiguous-width characters is fixed.
         *
         * This setting only takes effect the next time the terminal is reset, either
         * via escape sequence or with vte_terminal_reset().
         */
        pspecs[PROP_CJK_AMBIGUOUS_WIDTH] =
                g_param_spec_int ("cjk-ambiguous-width", NULL, NULL,
                                  1, 2, VTE_DEFAULT_UTF8_AMBIGUOUS_WIDTH,
                                  (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:cursor-blink-mode:
         *
         * Sets whether or not the cursor will blink. Using %VTE_CURSOR_BLINK_SYSTEM
         * will use the #GtkSettings::gtk-cursor-blink setting.
         */
        pspecs[PROP_CURSOR_BLINK_MODE] =
                g_param_spec_enum ("cursor-blink-mode", NULL, NULL,
                                   VTE_TYPE_CURSOR_BLINK_MODE,
                                   VTE_CURSOR_BLINK_SYSTEM,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:cursor-shape:
         *
         * Controls the shape of the cursor.
         */
        pspecs[PROP_CURSOR_SHAPE] =
                g_param_spec_enum ("cursor-shape", NULL, NULL,
                                   VTE_TYPE_CURSOR_SHAPE,
                                   VTE_CURSOR_SHAPE_BLOCK,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:delete-binding:
         *
         * Controls what string or control sequence the terminal sends to its child
         * when the user presses the delete key.
         */
        pspecs[PROP_DELETE_BINDING] =
                g_param_spec_enum ("delete-binding", NULL, NULL,
                                   VTE_TYPE_ERASE_BINDING,
                                   VTE_ERASE_AUTO,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:font-scale:
         *
         * The terminal's font scale.
         */
        pspecs[PROP_FONT_SCALE] =
                g_param_spec_double ("font-scale", NULL, NULL,
                                     VTE_FONT_SCALE_MIN,
                                     VTE_FONT_SCALE_MAX,
                                     1.,
                                     (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:encoding:
         *
         * Controls the encoding the terminal will expect data from the child to
         * be encoded with.  For certain terminal types, applications executing in the
         * terminal can change the encoding.  The default is defined by the
         * application's locale settings.
         */
        pspecs[PROP_ENCODING] =
                g_param_spec_string ("encoding", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:font-desc:
         *
         * Specifies the font used for rendering all text displayed by the terminal,
         * overriding any fonts set using gtk_widget_modify_font().  The terminal
         * will immediately attempt to load the desired font, retrieve its
         * metrics, and attempt to resize itself to keep the same number of rows
         * and columns.
         */
        pspecs[PROP_FONT_DESC] =
                g_param_spec_boxed ("font-desc", NULL, NULL,
                                    PANGO_TYPE_FONT_DESCRIPTION,
                                    (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:icon-title:
         *
         * The terminal's so-called icon title, or %NULL if no icon title has been set.
         */
        pspecs[PROP_ICON_TITLE] =
                g_param_spec_string ("icon-title", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:input-enabled:
         *
         * Controls whether the terminal allows user input. When user input is disabled,
         * key press and mouse button press and motion events are not sent to the
         * terminal's child.
         */
        pspecs[PROP_INPUT_ENABLED] =
                g_param_spec_boolean ("input-enabled", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:pointer-autohide:
         *
         * Controls the value of the terminal's mouse autohide setting.  When autohiding
         * is enabled, the mouse cursor will be hidden when the user presses a key and
         * shown when the user moves the mouse.
         */
        pspecs[PROP_MOUSE_POINTER_AUTOHIDE] =
                g_param_spec_boolean ("pointer-autohide", NULL, NULL,
                                      FALSE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:pty:
         *
         * The PTY object for the terminal.
         */
        pspecs[PROP_PTY] =
                g_param_spec_object ("pty", NULL, NULL,
                                     VTE_TYPE_PTY,
                                     (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:rewrap-on-resize:
         *
         * Controls whether or not the terminal will rewrap its contents, including
         * the scrollback buffer, whenever the terminal's width changes.
         */
        pspecs[PROP_REWRAP_ON_RESIZE] =
                g_param_spec_boolean ("rewrap-on-resize", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:scrollback-lines:
         *
         * The length of the scrollback buffer used by the terminal.  The size of
         * the scrollback buffer will be set to the larger of this value and the number
         * of visible rows the widget can display, so 0 can safely be used to disable
         * scrollback.  Note that this setting only affects the normal screen buffer.
         * For terminal types which have an alternate screen buffer, no scrollback is
         * allowed on the alternate screen buffer.
         */
        pspecs[PROP_SCROLLBACK_LINES] =
                g_param_spec_uint ("scrollback-lines", NULL, NULL,
                                   0, G_MAXUINT,
                                   VTE_SCROLLBACK_INIT,
                                   (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:scroll-on-keystroke:
         *
         * Controls whether or not the terminal will forcibly scroll to the bottom of
         * the viewable history when the user presses a key.  Modifier keys do not
         * trigger this behavior.
         */
        pspecs[PROP_SCROLL_ON_KEYSTROKE] =
                g_param_spec_boolean ("scroll-on-keystroke", NULL, NULL,
                                      FALSE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:scroll-on-output:
         *
         * Controls whether or not the terminal will forcibly scroll to the bottom of
         * the viewable history when the new data is received from the child.
         */
        pspecs[PROP_SCROLL_ON_OUTPUT] =
                g_param_spec_boolean ("scroll-on-output", NULL, NULL,
                                      TRUE,
                                      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:window-title:
         *
         * The terminal's title.
         */
        pspecs[PROP_WINDOW_TITLE] =
                g_param_spec_string ("window-title", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:current-directory-uri:
         *
         * The current directory URI, or %NULL if unset.
         */
        pspecs[PROP_CURRENT_DIRECTORY_URI] =
                g_param_spec_string ("current-directory-uri", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:current-file-uri:
         *
         * The current file URI, or %NULL if unset.
         */
        pspecs[PROP_CURRENT_FILE_URI] =
                g_param_spec_string ("current-file-uri", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        /**
         * VteTerminal:word-char-exceptions:
         *
         * The set of characters which will be considered parts of a word
         * when doing word-wise selection, in addition to the default which only
         * considers alphanumeric characters part of a word.
         *
         * If %NULL, a built-in set is used.
         *
         * Since: 0.40
         */
        pspecs[PROP_WORD_CHAR_EXCEPTIONS] =
                g_param_spec_string ("word-char-exceptions", NULL, NULL,
                                     NULL,
                                     (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

        g_object_class_install_properties(gobject_class, LAST_PROP, pspecs);

	/* Disable GtkWidget's keybindings except for Shift-F10 and MenuKey
         * which pop up the context menu.
         */
	binding_set = gtk_binding_set_by_class(vte_terminal_parent_class);
	gtk_binding_entry_skip(binding_set, GDK_KEY_F1, GDK_CONTROL_MASK);
	gtk_binding_entry_skip(binding_set, GDK_KEY_F1, GDK_SHIFT_MASK);
	gtk_binding_entry_skip(binding_set, GDK_KEY_KP_F1, GDK_CONTROL_MASK);
	gtk_binding_entry_skip(binding_set, GDK_KEY_KP_F1, GDK_SHIFT_MASK);

        process_timer = g_timer_new();

        klass->priv = G_TYPE_CLASS_GET_PRIVATE (klass, VTE_TYPE_TERMINAL, VteTerminalClassPrivate);

        klass->priv->style_provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
        gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (klass->priv->style_provider),
                                         "VteTerminal {\n"
                                         "padding: 1px 1px 1px 1px;\n"
                                         "background-color: @theme_base_color;\n"
                                         "color: @theme_fg_color;\n"
                                         "}\n",
                                         -1, NULL);

        /* a11y */
        gtk_widget_class_set_accessible_type(widget_class, VTE_TYPE_TERMINAL_ACCESSIBLE);
}

/* public API */

/**
 * vte_get_features:
 *
 * Gets a list of features vte was compiled with.
 *
 * Returns: (transfer none): a string with features
 *
 * Since: 0.40
 */
const char *
vte_get_features (void)
{
        return
#ifdef WITH_GNUTLS
                "+GNUTLS"
#else
                "-GNUTLS"
#endif
                " "
#ifdef WITH_PCRE2
                "+PCRE2"
#else
                "-PCRE2"
#endif
                ;
}

/**
 * vte_get_major_version:
 *
 * Returns the major version of the VTE library at runtime.
 * Contrast this with %VTE_MAJOR_VERSION which represents
 * the version of the VTE library that the code was compiled
 * with.
 *
 * Returns: the major version
 *
 * Since: 0.40
 */
guint
vte_get_major_version (void)
{
        return VTE_MAJOR_VERSION;
}

/**
 * vte_get_minor_version:
 *
 * Returns the minor version of the VTE library at runtime.
 * Contrast this with %VTE_MINOR_VERSION which represents
 * the version of the VTE library that the code was compiled
 * with.
 *
 * Returns: the minor version
 *
 * Since: 0.40
 */
guint
vte_get_minor_version (void)
{
        return VTE_MINOR_VERSION;
}

/**
 * vte_get_micro_version:
 *
 * Returns the micro version of the VTE library at runtime.
 * Contrast this with %VTE_MICRO_VERSION which represents
 * the version of the VTE library that the code was compiled
 * with.
 *
 * Returns: the micro version
 *
 * Since: 0.40
 */
guint
vte_get_micro_version (void)
{
        return VTE_MICRO_VERSION;
}

/**
 * vte_get_user_shell:
 *
 * Gets the user's shell, or %NULL. In the latter case, the
 * system default (usually "/bin/sh") should be used.
 *
 * Returns: (transfer full) (type filename): a newly allocated string with the
 *   user's shell, or %NULL
 */
char *
vte_get_user_shell (void)
{
	struct passwd *pwd;

	pwd = getpwuid(getuid());
        if (pwd && pwd->pw_shell)
                return g_strdup (pwd->pw_shell);

        return NULL;
}

/* VteTerminal public API */

/**
 * vte_terminal_search_find_previous:
 * @terminal: a #VteTerminal
 *
 * Searches the previous string matching the search regex set with
 * vte_terminal_search_set_gregex().
 *
 * Returns: %TRUE if a match was found
 */
gboolean
vte_terminal_search_find_previous (VteTerminal *terminal)
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->search_find(true);
}

/**
 * vte_terminal_search_find_next:
 * @terminal: a #VteTerminal
 *
 * Searches the next string matching the search regex set with
 * vte_terminal_search_set_gregex().
 *
 * Returns: %TRUE if a match was found
 */
gboolean
vte_terminal_search_find_next (VteTerminal *terminal)
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
	return terminal->pvt->search_find(false);
}

/**
 * vte_terminal_search_set_regex:
 * @terminal: a #VteTerminal
 * @regex: (allow-none): a #VteRegex, or %NULL
 * @flags: PCRE2 match flags, or 0
 *
 * Sets the regex to search for. Unsets the search regex when passed %NULL.
 *
 * Since: 0.44
 */
void
vte_terminal_search_set_regex (VteTerminal *terminal,
                               VteRegex    *regex,
                               guint32      flags)
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        terminal->pvt->search_set_regex(regex, flags);
}

/**
 * vte_terminal_search_get_regex:
 * @terminal: a #VteTerminal
 *
 * Returns: (transfer none): the search #VteRegex regex set in @terminal, or %NULL
 *
 * Since: 0.44
 */
VteRegex *
vte_terminal_search_get_regex(VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);

        if (G_LIKELY(terminal->pvt->search_regex.mode == VTE_REGEX_PCRE2))
                return terminal->pvt->search_regex.pcre.regex;
        else
                return NULL;
}

/**
 * vte_terminal_search_set_gregex:
 * @terminal: a #VteTerminal
 * @gregex: (allow-none): a #GRegex, or %NULL
 * @gflags: flags from #GRegexMatchFlags
 *
 * Sets the #GRegex regex to search for. Unsets the search regex when passed %NULL.
 *
 * Deprecated: 0.44: use vte_terminal_search_set_regex() instead.
 */
void
vte_terminal_search_set_gregex (VteTerminal *terminal,
				GRegex      *gregex,
                                GRegexMatchFlags gflags)
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        terminal->pvt->search_set_gregex(gregex, gflags);
}

/**
 * vte_terminal_search_get_gregex:
 * @terminal: a #VteTerminal
 *
 * Returns: (transfer none): the search #GRegex regex set in @terminal, or %NULL
 *
 * Deprecated: 0.44: use vte_terminal_search_get_regex() instead.
 */
GRegex *
vte_terminal_search_get_gregex (VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);

        if (G_LIKELY(terminal->pvt->search_regex.mode == VTE_REGEX_GREGEX))
                return terminal->pvt->search_regex.gregex.regex;
        else
                return NULL;
}

/**
 * vte_terminal_search_set_wrap_around:
 * @terminal: a #VteTerminal
 * @wrap_around: whether search should wrap
 *
 * Sets whether search should wrap around to the beginning of the
 * terminal content when reaching its end.
 */
void
vte_terminal_search_set_wrap_around (VteTerminal *terminal,
				     gboolean     wrap_around)
{
	g_return_if_fail(VTE_IS_TERMINAL(terminal));

        terminal->pvt->search_set_wrap_around(wrap_around != FALSE);
}

/**
 * vte_terminal_search_get_wrap_around:
 * @terminal: a #VteTerminal
 *
 * Returns: whether searching will wrap around
 */
gboolean
vte_terminal_search_get_wrap_around (VteTerminal *terminal)
{
	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);

	return terminal->pvt->search_wrap_around;
}

/* Just some arbitrary minimum values */
#define MIN_COLUMNS (16)
#define MIN_ROWS    (2)

/**
 * vte_terminal_get_geometry_hints:
 * @terminal: a #VteTerminal
 * @hints: (out caller-allocates): a #GdkGeometry to fill in
 * @min_rows: the minimum number of rows to request
 * @min_columns: the minimum number of columns to request
 *
 * Fills in some @hints from @terminal's geometry. The hints
 * filled are those covered by the %GDK_HINT_RESIZE_INC,
 * %GDK_HINT_MIN_SIZE and %GDK_HINT_BASE_SIZE flags.
 *
 * See gtk_window_set_geometry_hints() for more information.
 *
 * @terminal must be realized (see gtk_widget_get_realized()).
 */
void
vte_terminal_get_geometry_hints(VteTerminal *terminal,
                                GdkGeometry *hints,
                                int min_rows,
                                int min_columns)
{
        VteTerminalPrivate *pvt;
        GtkWidget *widget;
        GtkBorder padding;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(hints != NULL);
        widget = &terminal->widget;
        g_return_if_fail(gtk_widget_get_realized(widget));

        pvt = terminal->pvt;

        gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
                                      gtk_widget_get_state_flags(widget),
                                      &padding);

        hints->base_width  = padding.left + padding.right;
        hints->base_height = padding.top  + padding.bottom;
        hints->width_inc   = pvt->char_width;
        hints->height_inc  = pvt->char_height;
        hints->min_width   = hints->base_width  + hints->width_inc  * min_columns;
        hints->min_height  = hints->base_height + hints->height_inc * min_rows;
}

/**
 * vte_terminal_set_geometry_hints_for_window:
 * @terminal: a #VteTerminal
 * @window: a #GtkWindow
 *
 * Sets @terminal as @window's geometry widget. See
 * gtk_window_set_geometry_hints() for more information.
 *
 * @terminal must be realized (see gtk_widget_get_realized()).
 */
void
vte_terminal_set_geometry_hints_for_window(VteTerminal *terminal,
                                           GtkWindow *window)
{
        GdkGeometry hints;

        g_return_if_fail(VTE_IS_TERMINAL(terminal));
        g_return_if_fail(gtk_widget_get_realized(&terminal->widget));

        vte_terminal_get_geometry_hints(terminal, &hints, MIN_ROWS, MIN_COLUMNS);
        gtk_window_set_geometry_hints(window,
                                      &terminal->widget,
                                      &hints,
                                      (GdkWindowHints)(GDK_HINT_RESIZE_INC |
                                                       GDK_HINT_MIN_SIZE |
                                                       GDK_HINT_BASE_SIZE));
}

/**
 * vte_terminal_get_input_enabled:
 * @terminal: a #VteTerminal
 *
 * Returns whether the terminal allow user input.
 */
gboolean
vte_terminal_get_input_enabled (VteTerminal *terminal)
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);

        return terminal->pvt->input_enabled;
}

/**
 * vte_terminal_set_input_enabled:
 * @terminal: a #VteTerminal
 * @enabled: whether to enable user input
 *
 * Enables or disables user input. When user input is disabled,
 * the terminal's child will not receive any key press, or mouse button
 * press or motion events sent to it.
 */
void
vte_terminal_set_input_enabled (VteTerminal *terminal,
                                gboolean enabled)
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (terminal->pvt->set_input_enabled(enabled != FALSE))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_INPUT_ENABLED]);
}

/**
 * vte_terminal_get_word_char_exceptions:
 * @terminal: a #VteTerminal
 *
 * Returns the set of characters which will be considered parts of a word
 * when doing word-wise selection, in addition to the default which only
 * considers alphanumeric characters part of a word.
 *
 * If %NULL, a built-in set is used.
 *
 * Returns: (transfer none): a string, or %NULL
 *
 * Since: 0.40
 */
const char *
vte_terminal_get_word_char_exceptions(VteTerminal *terminal)
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);

        return terminal->pvt->word_char_exceptions_string;
}

/**
 * vte_terminal_set_word_char_exceptions:
 * @terminal: a #VteTerminal
 * @exceptions: a string of ASCII punctuation characters, or %NULL
 *
 * With this function you can provide a set of characters which will
 * be considered parts of a word when doing word-wise selection, in
 * addition to the default which only considers alphanumeric characters
 * part of a word.
 *
 * The characters in @exceptions must be non-alphanumeric, each character
 * must occur only once, and if @exceptions contains the character
 * U+002D HYPHEN-MINUS, it must be at the start of the string.
 *
 * Use %NULL to reset the set of exception characters to the default.
 *
 * Since: 0.40
 */
void
vte_terminal_set_word_char_exceptions(VteTerminal *terminal,
                                      const char *exceptions)
{
        g_return_if_fail(VTE_IS_TERMINAL(terminal));

        if (terminal->pvt->set_word_char_exceptions(exceptions))
                g_object_notify_by_pspec(G_OBJECT(terminal), pspecs[PROP_WORD_CHAR_EXCEPTIONS]);
}

/**
 * vte_terminal_write_contents_sync:
 * @terminal: a #VteTerminal
 * @stream: a #GOutputStream to write to
 * @flags: a set of #VteWriteFlags
 * @cancellable: (allow-none): a #GCancellable object, or %NULL
 * @error: (allow-none): a #GError location to store the error occuring, or %NULL
 *
 * Write contents of the current contents of @terminal (including any
 * scrollback history) to @stream according to @flags.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by triggering
 * the cancellable object from another thread. If the operation was cancelled,
 * the error %G_IO_ERROR_CANCELLED will be returned in @error.
 *
 * This is a synchronous operation and will make the widget (and input
 * processing) during the write operation, which may take a long time
 * depending on scrollback history and @stream availability for writing.
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 */
gboolean
vte_terminal_write_contents_sync (VteTerminal *terminal,
                                  GOutputStream *stream,
                                  VteWriteFlags flags,
                                  GCancellable *cancellable,
                                  GError **error)
{
        g_return_val_if_fail(VTE_IS_TERMINAL(terminal), FALSE);
        g_return_val_if_fail(G_IS_OUTPUT_STREAM(stream), FALSE);

        return terminal->pvt->write_contents_sync(stream, flags, cancellable, error);
}
