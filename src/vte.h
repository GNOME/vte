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
#include <gtk/gtk.h>
#include "termcap.h"
#include "trie.h"

G_BEGIN_DECLS

/* The terminal widget itself. */
typedef struct _VteTerminal {
	/*< public >*/

	/* Widget implementation stuffs. */
	GtkWidget widget;
	GtkAdjustment *adjustment;	/* Scrolling adjustment. */

	/* Metric and sizing data. */
	long char_width, char_height;	/* dimensions of character cells */
	long char_ascent, char_descent; /* important font metrics */
	long row_count, column_count;	/* dimensions of the window */

	/* Titles. */
	char *window_title, *icon_title;

	/*< private >*/
	struct _VteTerminalPrivate *pvt;
} VteTerminal;

/* The widget's class structure. */
typedef struct _VteTerminalClass {
	/*< public > */
	/* Inherited parent class. */
	GtkWidgetClass parent_class;

	/*< private > */
	/* Signals we might emit. */
	guint eof_signal;
	guint char_size_changed_signal;
	guint window_title_changed_signal;
	guint icon_title_changed_signal;
	guint selection_changed_signal;
	guint contents_changed_signal;
	guint cursor_moved_signal;

	guint deiconify_window_signal;
	guint iconify_window_signal;
	guint raise_window_signal;
	guint lower_window_signal;
	guint refresh_window_signal;
	guint restore_window_signal;
	guint maximize_window_signal;
	guint resize_window_signal;
	guint move_window_signal;
} VteTerminalClass;

/* A snapshot of the screen contents. */
typedef struct _VteTerminalSnapshot {
	struct {
		int x, y;			/* Location of the cursor. */
	} cursor;
	int rows, columns;			/* Size of the screen[shot]. */
	gboolean cursor_visible;
	struct VteTerminalSnapshotCell {
		gunichar c;			/* The character itself. */
		struct {
			/* Colors of this character. */
			GdkColor foreground, background;
			/* Is it underlined? */
			gboolean underline;
			/* Is it a graphic character? */
			gboolean alternate;
		} attributes;
	} **contents;
} VteTerminalSnapshot;

typedef enum {
	VTE_ERASE_AUTO,
	VTE_ERASE_ASCII_BACKSPACE,
	VTE_ERASE_ASCII_DELETE,
	VTE_ERASE_DELETE_SEQUENCE,
} VteTerminalEraseBinding;

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
pid_t vte_terminal_fork_command(VteTerminal *terminal,
			        const char *command,
			        const char **argv);
void vte_terminal_feed(VteTerminal *terminal,
		       const char *data,
		       size_t length);
void vte_terminal_feed_child(VteTerminal *terminal,
			     const char *data,
			     size_t length);

void vte_terminal_copy_clipboard(VteTerminal *terminal);
void vte_terminal_paste_clipboard(VteTerminal *terminal);

void vte_terminal_set_size(VteTerminal *terminal, long columns, long rows);
void vte_terminal_set_audible_bell(VteTerminal *terminal, gboolean audible);
void vte_terminal_set_scroll_on_output(VteTerminal *terminal, gboolean scroll);
void vte_terminal_set_scroll_on_keystroke(VteTerminal *terminal,
					  gboolean scroll);
void vte_terminal_set_colors(VteTerminal *terminal,
			     const GdkColor *foreground,
			     const GdkColor *background,
			     const GdkColor *palette,
			     size_t palette_size);
void vte_terminal_set_default_colors(VteTerminal *terminal);
void vte_terminal_set_background_image(VteTerminal *terminal, GdkPixbuf *image);
void vte_terminal_set_background_image_file(VteTerminal *terminal,
					    const char *path);
void vte_terminal_set_background_saturation(VteTerminal *terminal,
					    double saturation);
void vte_terminal_set_background_transparent(VteTerminal *terminal,
					     gboolean transparent);
void vte_terminal_set_cursor_blinks(VteTerminal *terminal, gboolean blink);
void vte_terminal_set_scrollback_lines(VteTerminal *terminal, long lines);
void vte_terminal_set_word_chars(VteTerminal *terminal, const char *spec);
void vte_terminal_im_append_menuitems(VteTerminal *terminal,
				      GtkMenuShell *menushell);
void vte_terminal_set_font(VteTerminal *terminal,
                           const PangoFontDescription *font_desc);
void vte_terminal_set_font_from_string(VteTerminal *terminal, const char *name);
const PangoFontDescription *vte_terminal_get_font(VteTerminal *terminal);

gboolean vte_terminal_get_has_selection(VteTerminal *terminal);
gboolean vte_terminal_get_using_xft(VteTerminal *terminal);
gboolean vte_terminal_is_word_char(VteTerminal *terminal, gunichar c);

void vte_terminal_set_backspace_binding(VteTerminal *terminal,
					VteTerminalEraseBinding binding);
void vte_terminal_set_delete_binding(VteTerminal *terminal,
				     VteTerminalEraseBinding binding);

VteTerminalSnapshot *vte_terminal_get_snapshot(VteTerminal *terminal);
void vte_terminal_free_snapshot(VteTerminalSnapshot *snapshot);

G_END_DECLS

#endif
