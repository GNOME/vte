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

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#ifndef __VTE_VIEW_H__
#define __VTE_VIEW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VTE_TYPE_VIEW            (vte_view_get_type())
#define VTE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VTE_TYPE_VIEW, VteView))
#define VTE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  VTE_TYPE_VIEW, VteViewClass))
#define VTE_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VTE_TYPE_VIEW))
#define VTE_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  VTE_TYPE_VIEW))
#define VTE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  VTE_TYPE_VIEW, VteViewClass))

typedef struct _VteView             VteView;
typedef struct _VteViewPrivate      VteViewPrivate;
typedef struct _VteViewClass        VteViewClass;
typedef struct _VteViewClassPrivate VteViewClassPrivate;

/**
 * VteView:
 */
struct _VteView {
	GtkWidget widget;

        /*< private >*/
        VteViewPrivate *pvt;
};

/**
 * VteViewClass:
 *
 * All of these fields should be considered read-only, except for derived classes.
 */
struct _VteViewClass {
	/*< public > */
	/* Inherited parent class. */
	GtkWidgetClass parent_class;

        /*< private >*/
        VteViewClassPrivate *priv;

	/*< protected > */
        void (*buffer_changed)(VteView *terminal, VteBuffer *previous_buffer);
	void (*char_size_changed)(VteView* terminal, guint char_width, guint char_height);
	void (*selection_changed)(VteView* terminal);

        /* FIXMEchpe: should these return gboolean and have defaul thandlers
         * settings the "scale" property?
         */
	void (*increase_font_size)(VteView* terminal);
	void (*decrease_font_size)(VteView* terminal);

	void (*text_scrolled)(VteView* terminal, gint delta);

	void (*copy_clipboard)(VteView* terminal);
	void (*paste_clipboard)(VteView* terminal);
        void (*copy_primary)(VteView* terminal);
        void (*paste_primary)(VteView* terminal);

        /* Padding for future expansion. */
        gpointer padding[16];
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
GType vte_view_get_type(void);

GtkWidget *vte_view_new(void);

void vte_view_set_buffer(VteView *terminal, VteBuffer *buffer);
VteBuffer *vte_view_get_buffer(VteView *terminal);

/* Copy currently-selected text to the clipboard, or from the clipboard to
 * the terminal. */
void vte_view_copy_clipboard(VteView *terminal, GtkClipboard *clipboard);
void vte_view_paste_clipboard(VteView *terminal, GtkClipboard *clipboard);

void vte_view_select_all(VteView *terminal);
void vte_view_unselect_all(VteView *terminal);

void vte_view_set_font_scale(VteView *terminal,
                                 gdouble scale);
gdouble vte_view_get_font_scale(VteView *terminal);

/* Set various on-off settings. */
void vte_view_set_audible_bell(VteView *terminal, gboolean is_audible);
gboolean vte_view_get_audible_bell(VteView *terminal);
void vte_view_set_visible_bell(VteView *terminal, gboolean is_visible);
gboolean vte_view_get_visible_bell(VteView *terminal);
void vte_view_set_scroll_on_output(VteView *terminal, gboolean scroll);
void vte_view_set_scroll_on_keystroke(VteView *terminal,
					  gboolean scroll);

/* Append the input method menu items to a given shell. */
void vte_view_im_append_menuitems(VteView *terminal,
				      GtkMenuShell *menushell);

/* Check if the terminal is the current selection owner. */
gboolean vte_view_get_has_selection(VteView *terminal);
gboolean vte_view_get_selection_bounds(VteView *terminal,
                                           VteBufferIter *start,
                                           VteBufferIter *end);

/* Set the list of word chars, optionally using hyphens to specify ranges
 * (to get a hyphen, place it first), and check if a character is in the
 * range. */
void vte_view_set_word_chars(VteView *terminal, const char *spec);

/* Manipulate the autohide setting. */
void vte_view_set_mouse_autohide(VteView *terminal, gboolean setting);
gboolean vte_view_get_mouse_autohide(VteView *terminal);

/* Add a matching expression, returning the tag the widget assigns to that
 * expression. */
int vte_view_match_add_gregex(VteView *terminal, GRegex *regex, GRegexMatchFlags flags);
/* Set the cursor to be used when the pointer is over a given match. */
void vte_view_match_set_cursor(VteView *terminal, int tag,
				   GdkCursor *cursor);
void vte_view_match_set_cursor_type(VteView *terminal,
					int tag, GdkCursorType cursor_type);
void vte_view_match_set_cursor_name(VteView *terminal,
					int tag, const char *cursor_name);
void vte_view_match_remove(VteView *terminal, int tag);
void vte_view_match_remove_all(VteView *terminal);

/* Check if a given cell on the screen contains part of a matched string.  If
 * it does, return the string, and store the match tag in the optional tag
 * argument. */
char *vte_view_match_check(VteView *terminal,
			       glong column, glong row,
			       int *tag);
char *vte_view_match_check_event(VteView *view,
                                 GdkEvent *event,
                                 int *tag);
char *vte_view_match_check_iter(VteView *view,
                                VteBufferIter *iter,
                                int *tag);

void      vte_view_search_set_gregex      (VteView *terminal,
					       GRegex      *regex,
                                               GRegexMatchFlags flags);
GRegex   *vte_view_search_get_gregex      (VteView *terminal);
void      vte_view_search_set_wrap_around (VteView *terminal,
					       gboolean     wrap_around);
gboolean  vte_view_search_get_wrap_around (VteView *terminal);
gboolean  vte_view_search_find_previous   (VteView *terminal);
gboolean  vte_view_search_find_next       (VteView *terminal);

void vte_view_get_geometry_hints(VteView *view,
                                 GdkGeometry *hints,
                                 int min_rows,
                                 int min_columns);
void vte_view_set_window_geometry_hints(VteView *view,
                                        GtkWindow *window);

gboolean vte_view_iter_from_event(VteView *view,
                                  GdkEvent *event,
                                  VteBufferIter *iter);
gboolean vte_view_iter_is_visible(VteView *view,
                                  VteBufferIter *iter);

G_END_DECLS

#endif /* __VTE_VIEW_H__ */
