/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef vte_h_included
#define vte_h_included

#ident "$Id$"

#include <sys/types.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <X11/Xlib.h>
#include <glib.h>
#include <pango/pango.h>
#include "termcap.h"
#include "trie.h"

G_BEGIN_DECLS

typedef struct _VteTerminal VteTerminal;
typedef struct _VteTerminalClass VteTerminalClass;

/* The structure we use to hold characters we're supposed to display -- this
 * includes any supported visible attributes. */
struct vte_charcell {
	wchar_t c;		/* The wide character. */
	guint16 columns: 2;	/* Number of visible columns (as determined
				   by wcwidth(c)). */
	guint16 fore: 3;	/* Indices in the color palette for the */
	guint16 back: 3;	/* foreground and background of the cell. */
	guint16 reverse: 1;	/* Single-bit attributes. */
	guint16 invisible: 1;
	guint16 bold: 1;
	guint16 standout: 1;
	guint16 underline: 1;
	guint16 half: 1;
	guint16 blink: 1;
	guint16 alternate: 1;
};

/* The terminal's keypad state.  A terminal can either be using the normal
 * keypad, or the "application" keypad.  Arrow key sequences, for example,
 * are really only defined for "application" mode. */
typedef enum {
	VTE_KEYPAD_NORMAL,
	VTE_KEYPAD_APPLICATION,
} VteKeypad;

/* The terminal widget itself. */
struct _VteTerminalPrivate;
struct _VteTerminal {
	/*< public >*/

	/* Widget implementation stuffs. */
	GtkWidget widget;
	GtkAdjustment *adjustment;	/* Scrolling adjustment. */

	/* Metric and sizing data. */
	guint char_width, char_height;	/* dimensions of character cells */
	guint char_ascent, char_descent;/* important font metrics */
	guint row_count, column_count;	/* dimensions of the window */

	/*< private >*/
	struct _VteTerminalPrivate *pvt;
};

/* The widget's class structure. */
struct _VteTerminalClass {
	/*< public > */
	/* Inherited parent class. */
	GtkWidgetClass parent_class;

	/*< private > */
	/* Signals we might omit. */
	guint eof_signal;
	guint char_size_changed_signal;
	guint set_window_title_signal;
	guint set_icon_title_signal;
};

/* The widget's type. */
GtkType vte_terminal_get_type(void);

#define VTE_TYPE_TERMINAL		(vte_terminal_get_type())
#define VTE_TERMINAL(obj)		(GTK_CHECK_CAST((obj),\
							VTE_TYPE_TERMINAL,\
							VteTerminal))
#define VTE_TERMINAL_CLASS(klass)	GTK_CHECK_CLASS_CAST((klass),\
							     VTE_TYPE_TERMINAL,\
							     VteTerminalClass)
#define VTE_IS_TERMINAL(obj)		GTK_CHECK_TYPE((obj),\
						       VTE_TYPE_TERMINAL)
#define VTE_IS_TERMINAL_CLASS(klass)	GTK_CHECK_CLASS_TYPE((klass),\
							     VTE_TYPE_TERMINAL)
#define VTE_TERMINAL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_TERMINAL, VteTerminalClass))


GtkWidget *vte_terminal_new(void);

G_END_DECLS

#endif
