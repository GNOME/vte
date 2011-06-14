/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef vte_vte_h_included
#define vte_vte_h_included

#include <glib.h>
#include <gio/gio.h>
#include <pango/pango.h>
#include <gtk/gtk.h>

#define __VTE_VTE_H_INSIDE__ 1

#include "vteenums.h"
#include "vtepty.h"
#include "vtebuffer.h"

#include "vtetypebuiltins.h"
#include "vteversion.h"

#undef __VTE_VTE_H_INSIDE__

G_BEGIN_DECLS

#define VTE_TYPE_TERMINAL            (vte_terminal_get_type())
#define VTE_TERMINAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VTE_TYPE_TERMINAL, VteTerminal))
#define VTE_TERMINAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  VTE_TYPE_TERMINAL, VteTerminalClass))
#define VTE_IS_TERMINAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VTE_TYPE_TERMINAL))
#define VTE_IS_TERMINAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  VTE_TYPE_TERMINAL))
#define VTE_TERMINAL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  VTE_TYPE_TERMINAL, VteTerminalClass))

typedef struct _VteTerminal             VteTerminal;
typedef struct _VteTerminalPrivate      VteTerminalPrivate;
typedef struct _VteTerminalClass        VteTerminalClass;
typedef struct _VteTerminalClassPrivate VteTerminalClassPrivate;

typedef struct _VteTerminalRealPrivate VteTerminalRealPrivate;

/**
 * VteTerminal:
 */
struct _VteTerminal {
	GtkWidget widget;
        /*< private >*/
	VteTerminalRealPrivate *term_pvt;

        /* temporary hack! FIXMEchpe */
        VteBufferPrivate *pvt;
};

/**
 * VteTerminalClass:
 *
 * All of these fields should be considered read-only, except for derived classes.
 */
struct _VteTerminalClass {
	/*< public > */
	/* Inherited parent class. */
	GtkWidgetClass parent_class;

	/*< protected > */
        void (*buffer_changed)(VteTerminal *terminal, VteBuffer *previous_buffer);
	void (*char_size_changed)(VteTerminal* terminal, guint char_width, guint char_height);
	void (*selection_changed)(VteTerminal* terminal);
	void (*contents_changed)(VteTerminal* terminal);

        /* FIXMEchpe: should these return gboolean and have defaul thandlers
         * settings the "scale" property?
         */
	void (*increase_font_size)(VteTerminal* terminal);
	void (*decrease_font_size)(VteTerminal* terminal);

	void (*text_scrolled)(VteTerminal* terminal, gint delta);

	void (*copy_clipboard)(VteTerminal* terminal);
	void (*paste_clipboard)(VteTerminal* terminal);
        void (*copy_primary)(VteTerminal* terminal);
        void (*paste_primary)(VteTerminal* terminal);

        /* Padding for future expansion. */
        gpointer padding[16];

        VteTerminalClassPrivate *priv;
};

/**
 * VTE_STYLE_CLASS_TERMINAL:
 *
 * A CSS class to match terminals.
 *
 * Since: 0.30
 */
#define VTE_STYLE_CLASS_TERMINAL "terminal"

/* The widget's type. */
GType vte_terminal_get_type(void);

GtkWidget *vte_terminal_new(void);

void vte_terminal_set_buffer(VteTerminal *terminal, VteBuffer *buffer);
VteBuffer *vte_terminal_get_buffer(VteTerminal *terminal);

/* Copy currently-selected text to the clipboard, or from the clipboard to
 * the terminal. */
void vte_terminal_copy_clipboard(VteTerminal *terminal, GtkClipboard *clipboard);
void vte_terminal_paste_clipboard(VteTerminal *terminal, GtkClipboard *clipboard);

void vte_terminal_select_all(VteTerminal *terminal);
void vte_terminal_unselect_all(VteTerminal *terminal);

void vte_terminal_set_font_scale(VteTerminal *terminal,
                                 gdouble scale);
gdouble vte_terminal_get_font_scale(VteTerminal *terminal);

/* Set various on-off settings. */
void vte_terminal_set_audible_bell(VteTerminal *terminal, gboolean is_audible);
gboolean vte_terminal_get_audible_bell(VteTerminal *terminal);
void vte_terminal_set_visible_bell(VteTerminal *terminal, gboolean is_visible);
gboolean vte_terminal_get_visible_bell(VteTerminal *terminal);
void vte_terminal_set_scroll_on_output(VteTerminal *terminal, gboolean scroll);
void vte_terminal_set_scroll_on_keystroke(VteTerminal *terminal,
					  gboolean scroll);

/* Append the input method menu items to a given shell. */
void vte_terminal_im_append_menuitems(VteTerminal *terminal,
				      GtkMenuShell *menushell);

/* Check if the terminal is the current selection owner. */
gboolean vte_terminal_get_has_selection(VteTerminal *terminal);
gboolean vte_terminal_get_selection_bounds(VteTerminal *terminal,
                                           VteBufferIter *start,
                                           VteBufferIter *end);

/* Set the list of word chars, optionally using hyphens to specify ranges
 * (to get a hyphen, place it first), and check if a character is in the
 * range. */
void vte_terminal_set_word_chars(VteTerminal *terminal, const char *spec);

/* Manipulate the autohide setting. */
void vte_terminal_set_mouse_autohide(VteTerminal *terminal, gboolean setting);
gboolean vte_terminal_get_mouse_autohide(VteTerminal *terminal);

/* Add a matching expression, returning the tag the widget assigns to that
 * expression. */
int vte_terminal_match_add_gregex(VteTerminal *terminal, GRegex *regex, GRegexMatchFlags flags);
/* Set the cursor to be used when the pointer is over a given match. */
void vte_terminal_match_set_cursor(VteTerminal *terminal, int tag,
				   GdkCursor *cursor);
void vte_terminal_match_set_cursor_type(VteTerminal *terminal,
					int tag, GdkCursorType cursor_type);
void vte_terminal_match_set_cursor_name(VteTerminal *terminal,
					int tag, const char *cursor_name);
void vte_terminal_match_remove(VteTerminal *terminal, int tag);
void vte_terminal_match_remove_all(VteTerminal *terminal);

/* Check if a given cell on the screen contains part of a matched string.  If
 * it does, return the string, and store the match tag in the optional tag
 * argument. */
char *vte_terminal_match_check(VteTerminal *terminal,
			       glong column, glong row,
			       int *tag);

void      vte_terminal_search_set_gregex      (VteTerminal *terminal,
					       GRegex      *regex);
GRegex   *vte_terminal_search_get_gregex      (VteTerminal *terminal);
void      vte_terminal_search_set_wrap_around (VteTerminal *terminal,
					       gboolean     wrap_around);
gboolean  vte_terminal_search_get_wrap_around (VteTerminal *terminal);
gboolean  vte_terminal_search_find_previous   (VteTerminal *terminal);
gboolean  vte_terminal_search_find_next       (VteTerminal *terminal);

const char *vte_get_default_emulation(void);

char *vte_get_user_shell (void);

glong vte_terminal_get_char_width(VteTerminal *terminal);
glong vte_terminal_get_char_height(VteTerminal *terminal);

G_END_DECLS

#endif
