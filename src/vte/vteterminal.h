/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
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

#pragma once

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#include <glib.h>
#include <gio/gio.h>
#include <pango/pango.h>
#include <gtk/gtk.h>

#include "vteenums.h"
#include "vtemacros.h"
#include "vtepty.h"
#include "vteregex.h"

G_BEGIN_DECLS

#define VTE_TYPE_TERMINAL            (vte_terminal_get_type())
#define VTE_TERMINAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VTE_TYPE_TERMINAL, VteTerminal))
#define VTE_TERMINAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  VTE_TYPE_TERMINAL, VteTerminalClass))
#define VTE_IS_TERMINAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VTE_TYPE_TERMINAL))
#define VTE_IS_TERMINAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  VTE_TYPE_TERMINAL))
#define VTE_TERMINAL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  VTE_TYPE_TERMINAL, VteTerminalClass))

typedef struct _VteTerminal             VteTerminal;
typedef struct _VteTerminalClass        VteTerminalClass;
typedef struct _VteTerminalClassPrivate VteTerminalClassPrivate;
typedef struct _VteCharAttributes       VteCharAttributes;

/**
 * VteTerminal:
 */
struct _VteTerminal {
	GtkWidget widget;
        /*< private >*/
	gpointer *_unused_padding[1]; /* FIXMEchpe: remove this field on the next ABI break */
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
	/* Default signal handlers. */
	void (*eof)(VteTerminal* terminal);
	void (*child_exited)(VteTerminal* terminal, int status);
	void (*encoding_changed)(VteTerminal* terminal);
	void (*char_size_changed)(VteTerminal* terminal, guint char_width, guint char_height);
	void (*window_title_changed)(VteTerminal* terminal);
	void (*icon_title_changed)(VteTerminal* terminal);
	void (*selection_changed)(VteTerminal* terminal);
	void (*contents_changed)(VteTerminal* terminal);
	void (*cursor_moved)(VteTerminal* terminal);
	void (*commit)(VteTerminal* terminal, const gchar *text, guint size);

	void (*deiconify_window)(VteTerminal* terminal);
	void (*iconify_window)(VteTerminal* terminal);
	void (*raise_window)(VteTerminal* terminal);
	void (*lower_window)(VteTerminal* terminal);
	void (*refresh_window)(VteTerminal* terminal);
	void (*restore_window)(VteTerminal* terminal);
	void (*maximize_window)(VteTerminal* terminal);
	void (*resize_window)(VteTerminal* terminal, guint width, guint height);
	void (*move_window)(VteTerminal* terminal, guint x, guint y);

        /* FIXMEchpe: should these return gboolean and have defaul thandlers
         * settings the "scale" property?
         */
	void (*increase_font_size)(VteTerminal* terminal);
	void (*decrease_font_size)(VteTerminal* terminal);

	void (*text_modified)(VteTerminal* terminal);
	void (*text_inserted)(VteTerminal* terminal);
	void (*text_deleted)(VteTerminal* terminal);
	void (*text_scrolled)(VteTerminal* terminal, gint delta);
	void (*copy_clipboard)(VteTerminal* terminal);
	void (*paste_clipboard)(VteTerminal* terminal);

	void (*bell)(VteTerminal* terminal);

	void (*notification_received)(VteTerminal* terminal, const gchar *summary, const gchar *body);

        /* Padding for future expansion. */
        gpointer padding[15];

        VteTerminalClassPrivate *priv;
};

/* The structure we return as the supplemental attributes for strings. */
struct _VteCharAttributes {
        /*< private >*/
        long row, column;  /* logical column */
	PangoColor fore, back;
	guint underline:1, strikethrough:1, columns:4;
};

typedef gboolean (*VteSelectionFunc)(VteTerminal *terminal,
                                     glong column,
                                     glong row,
                                     gpointer data) _VTE_GNUC_NONNULL(1);

/* The widget's type. */
_VTE_PUBLIC
GType vte_terminal_get_type(void);

_VTE_PUBLIC
GtkWidget *vte_terminal_new(void) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
VtePty *vte_terminal_pty_new_sync (VteTerminal *terminal,
                                   VtePtyFlags flags,
                                   GCancellable *cancellable,
                                   GError **error) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_watch_child (VteTerminal *terminal,
                               GPid child_pid) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

typedef void (* VteTerminalSpawnAsyncCallback) (VteTerminal *terminal,
                                                GPid pid,
                                                GError *error,
                                                gpointer user_data);

_VTE_PUBLIC
void vte_terminal_spawn_async(VteTerminal *terminal,
                              VtePtyFlags pty_flags,
                              const char *working_directory,
                              char **argv,
                              char **envv,
                              GSpawnFlags spawn_flags,
                              GSpawnChildSetupFunc child_setup,
                              gpointer child_setup_data,
                              GDestroyNotify child_setup_data_destroy,
                              int timeout,
                              GCancellable *cancellable,
                              VteTerminalSpawnAsyncCallback callback,
                              gpointer user_data) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(4);

_VTE_PUBLIC
void vte_terminal_spawn_with_fds_async(VteTerminal* terminal,
                                       VtePtyFlags pty_flags,
                                       char const* working_directory,
                                       char const* const* argv,
                                       char const* const* envv,
                                       int const* fds,
                                       int n_fds,
                                       int const* map_fds,
                                       int n_map_fds,
                                       GSpawnFlags spawn_flags,
                                       GSpawnChildSetupFunc child_setup,
                                       gpointer child_setup_data,
                                       GDestroyNotify child_setup_data_destroy,
                                       int timeout,
                                       GCancellable* cancellable,
                                       VteTerminalSpawnAsyncCallback callback,
                                       gpointer user_data) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(4);

/* Send data to the terminal to display, or to the terminal's forked command
 * to handle in some way.  If it's 'cat', they should be the same. */
_VTE_PUBLIC
void vte_terminal_feed(VteTerminal *terminal,
                       const char *data,
                       gssize length) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_feed_child(VteTerminal *terminal,
                             const char *text,
                             gssize length) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Copy currently-selected text to the clipboard, or from the clipboard to
 * the terminal. */
_VTE_PUBLIC
void vte_terminal_copy_clipboard_format(VteTerminal *terminal,
                                        VteFormat format) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_paste_clipboard(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_copy_primary(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_paste_primary(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_select_all(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_unselect_all(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* By-word selection */
_VTE_PUBLIC
void vte_terminal_set_word_char_exceptions(VteTerminal *terminal,
                                           const char *exceptions) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
const char *vte_terminal_get_word_char_exceptions(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Set the terminal's size. */
_VTE_PUBLIC
void vte_terminal_set_size(VteTerminal *terminal,
			   glong columns, glong rows) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_set_font_scale(VteTerminal *terminal,
                                 gdouble scale) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gdouble vte_terminal_get_font_scale(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_set_cell_width_scale(VteTerminal *terminal,
                                       double scale) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
double vte_terminal_get_cell_width_scale(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_set_cell_height_scale(VteTerminal *terminal,
                                        double scale) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
double vte_terminal_get_cell_height_scale(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Set various on-off settings. */
_VTE_PUBLIC
void vte_terminal_set_text_blink_mode(VteTerminal *terminal,
                                      VteTextBlinkMode text_blink_mode) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
VteTextBlinkMode vte_terminal_get_text_blink_mode(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_set_audible_bell(VteTerminal *terminal,
                                   gboolean is_audible) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean vte_terminal_get_audible_bell(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_set_scroll_on_output(VteTerminal *terminal,
                                       gboolean scroll) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean vte_terminal_get_scroll_on_output(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_set_scroll_on_keystroke(VteTerminal *terminal,
					  gboolean scroll) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean vte_terminal_get_scroll_on_keystroke(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_set_enable_fallback_scrolling(VteTerminal *terminal,
                                                gboolean enable) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean vte_terminal_get_enable_fallback_scrolling(VteTerminal *terminal)  _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Set the color scheme. */
_VTE_PUBLIC
void vte_terminal_set_color_bold(VteTerminal *terminal,
                                 const GdkRGBA *bold) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_set_color_foreground(VteTerminal *terminal,
                                       const GdkRGBA *foreground) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(2);
_VTE_PUBLIC
void vte_terminal_set_color_background(VteTerminal *terminal,
                                       const GdkRGBA *background) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(2);
_VTE_PUBLIC
void vte_terminal_set_color_cursor(VteTerminal *terminal,
                                   const GdkRGBA *cursor_background) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_set_color_cursor_foreground(VteTerminal *terminal,
                                              const GdkRGBA *cursor_foreground) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_set_color_highlight(VteTerminal *terminal,
                                      const GdkRGBA *highlight_background) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_set_color_highlight_foreground(VteTerminal *terminal,
                                                 const GdkRGBA *highlight_foreground) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_set_colors(VteTerminal *terminal,
                             const GdkRGBA *foreground,
                             const GdkRGBA *background,
                             const GdkRGBA *palette,
                             gsize palette_size) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_set_default_colors(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Set whether or not the cursor blinks. */
_VTE_PUBLIC
void vte_terminal_set_cursor_blink_mode(VteTerminal *terminal,
					VteCursorBlinkMode mode) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
VteCursorBlinkMode vte_terminal_get_cursor_blink_mode(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Set cursor shape */
_VTE_PUBLIC
void vte_terminal_set_cursor_shape(VteTerminal *terminal,
				   VteCursorShape shape) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
VteCursorShape vte_terminal_get_cursor_shape(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Set the number of scrollback lines, above or at an internal minimum. */
_VTE_PUBLIC
void vte_terminal_set_scrollback_lines(VteTerminal *terminal,
                                       glong lines) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
glong vte_terminal_get_scrollback_lines(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Set or retrieve the current font. */
_VTE_PUBLIC
void vte_terminal_set_font(VteTerminal *terminal,
			   const PangoFontDescription *font_desc) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
const PangoFontDescription *vte_terminal_get_font(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_set_bold_is_bright(VteTerminal *terminal,
                                     gboolean bold_is_bright) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean vte_terminal_get_bold_is_bright(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_set_allow_hyperlink(VteTerminal *terminal,
                                      gboolean allow_hyperlink) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean vte_terminal_get_allow_hyperlink(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Check if the terminal is the current selection owner. */
_VTE_PUBLIC
gboolean vte_terminal_get_has_selection(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Set what happens when the user strikes backspace or delete. */
_VTE_PUBLIC
void vte_terminal_set_backspace_binding(VteTerminal *terminal,
					VteEraseBinding binding) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_set_delete_binding(VteTerminal *terminal,
				     VteEraseBinding binding) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* BiDi and shaping */
_VTE_PUBLIC
void vte_terminal_set_enable_bidi(VteTerminal *terminal,
                                  gboolean enable_bidi) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean vte_terminal_get_enable_bidi(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_set_enable_shaping(VteTerminal *terminal,
                                     gboolean enable_shaping) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean vte_terminal_get_enable_shaping(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Manipulate the autohide setting. */
_VTE_PUBLIC
void vte_terminal_set_mouse_autohide(VteTerminal *terminal,
                                     gboolean setting) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean vte_terminal_get_mouse_autohide(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Reset the terminal, optionally clearing the tab stops and line history. */
_VTE_PUBLIC
void vte_terminal_reset(VteTerminal *terminal,
                        gboolean clear_tabstops,
			gboolean clear_history) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Read the contents of the terminal, using a callback function to determine
 * if a particular location on the screen (0-based) is interesting enough to
 * include.  Each byte in the returned string will have a corresponding
 * VteCharAttributes structure in the passed GArray, if the array was not %NULL.
 * Note that it will have one entry per byte, not per character, so indexes
 * should match up exactly. */
_VTE_PUBLIC
char *vte_terminal_get_text(VteTerminal *terminal,
			    VteSelectionFunc is_selected,
			    gpointer user_data,
			    GArray *attributes) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) G_GNUC_MALLOC;
_VTE_PUBLIC
char *vte_terminal_get_text_range(VteTerminal *terminal,
				  glong start_row, glong start_col,
				  glong end_row, glong end_col,
				  VteSelectionFunc is_selected,
				  gpointer user_data,
				  GArray *attributes) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) G_GNUC_MALLOC;
_VTE_PUBLIC
void vte_terminal_get_cursor_position(VteTerminal *terminal,
				      glong *column,
                                      glong *row) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
char *vte_terminal_hyperlink_check_event(VteTerminal *terminal,
                                         GdkEvent *event) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(2) G_GNUC_MALLOC;

/* Add a matching expression, returning the tag the widget assigns to that
 * expression. */
_VTE_PUBLIC
int vte_terminal_match_add_regex(VteTerminal *terminal,
                                 VteRegex *regex,
                                 guint32 flags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(2);
/* Set the cursor to be used when the pointer is over a given match. */
_VTE_PUBLIC
void vte_terminal_match_set_cursor_name(VteTerminal *terminal,
					int tag,
                                        const char *cursor_name) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(3);
_VTE_PUBLIC
void vte_terminal_match_remove(VteTerminal *terminal,
                               int tag) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_match_remove_all(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Check if a given cell on the screen contains part of a matched string.  If
 * it does, return the string, and store the match tag in the optional tag
 * argument. */
_VTE_PUBLIC
char *vte_terminal_match_check_event(VteTerminal *terminal,
                                     GdkEvent *event,
                                     int *tag) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(2) G_GNUC_MALLOC;
_VTE_PUBLIC
char **vte_terminal_event_check_regex_array(VteTerminal *terminal,
                                            GdkEvent *event,
                                            VteRegex **regexes,
                                            gsize n_regexes,
                                            guint32 match_flags,
                                            gsize *n_matches) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(2) G_GNUC_MALLOC;
_VTE_PUBLIC
gboolean vte_terminal_event_check_regex_simple(VteTerminal *terminal,
                                               GdkEvent *event,
                                               VteRegex **regexes,
                                               gsize n_regexes,
                                               guint32 match_flags,
                                               char **matches) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(2);

_VTE_PUBLIC
void      vte_terminal_search_set_regex      (VteTerminal *terminal,
                                              VteRegex    *regex,
                                              guint32      flags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
VteRegex *vte_terminal_search_get_regex      (VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void      vte_terminal_search_set_wrap_around (VteTerminal *terminal,
					       gboolean     wrap_around) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean  vte_terminal_search_get_wrap_around (VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean  vte_terminal_search_find_previous   (VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean  vte_terminal_search_find_next       (VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);


/* CJK compatibility setting */
_VTE_PUBLIC
void vte_terminal_set_cjk_ambiguous_width(VteTerminal *terminal,
                                          int width) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
int vte_terminal_get_cjk_ambiguous_width(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
void vte_terminal_set_pty(VteTerminal *terminal,
                          VtePty *pty) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
VtePty *vte_terminal_get_pty(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* Accessors for bindings. */
_VTE_PUBLIC
glong vte_terminal_get_char_width(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
glong vte_terminal_get_char_height(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
glong vte_terminal_get_row_count(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
glong vte_terminal_get_column_count(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
const char *vte_terminal_get_window_title(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
const char *vte_terminal_get_current_directory_uri(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
const char *vte_terminal_get_current_file_uri(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* misc */
_VTE_PUBLIC
void vte_terminal_set_input_enabled (VteTerminal *terminal,
                                     gboolean enabled) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
gboolean vte_terminal_get_input_enabled (VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

/* rarely useful functions */
_VTE_PUBLIC
void vte_terminal_set_clear_background(VteTerminal* terminal,
                                       gboolean setting) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);
_VTE_PUBLIC
void vte_terminal_get_color_background_for_draw(VteTerminal* terminal,
                                                GdkRGBA* color) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(2);

/* Writing contents out */
_VTE_PUBLIC
gboolean vte_terminal_write_contents_sync (VteTerminal *terminal,
                                           GOutputStream *stream,
                                           VteWriteFlags flags,
                                           GCancellable *cancellable,
                                           GError **error) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1) _VTE_GNUC_NONNULL(2);

/* Images */

/* Set or get whether SIXEL image support is enabled */
_VTE_PUBLIC
void vte_terminal_set_enable_sixel(VteTerminal *terminal,
                                    gboolean enabled) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_terminal_get_enable_sixel(VteTerminal *terminal) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(VteTerminal, g_object_unref)

G_END_DECLS
