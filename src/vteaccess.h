/*
 * Copyright (C) 2002 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef vte_vteaccess_h_included
#define vte_vteaccess_h_included


#include <glib.h>
#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>

G_BEGIN_DECLS

#define VTE_TYPE_TERMINAL_ACCESSIBLE            (_vte_terminal_accessible_get_type ())
#define VTE_TERMINAL_ACCESSIBLE(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), VTE_TYPE_TERMINAL_ACCESSIBLE, VteTerminalAccessible))
#define VTE_TERMINAL_ACCESSIBLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VTE_TYPE_TERMINAL_ACCESSIBLE, VteTerminalAccessibleClass))
#define VTE_IS_TERMINAL_ACCESSIBLE(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), VTE_TYPE_TERMINAL_ACCESSIBLE))
#define VTE_IS_TERMINAL_ACCESSIBLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VTE_TYPE_TERMINAL_ACCESSIBLE))
#define VTE_TERMINAL_ACCESSIBLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_TERMINAL_ACCESSIBLE, VteTerminalAccessibleClass))

typedef struct _VteTerminalAccessible      VteTerminalAccessible;
typedef struct _VteTerminalAccessibleClass VteTerminalAccessibleClass;
typedef struct _VteTerminalAccessiblePrivate VteTerminalAccessiblePrivate;

/**
 * VteTerminalAccessible:
 *
 * The accessible peer for #VteTerminal.
 */
struct _VteTerminalAccessible {
	GtkWidgetAccessible parent;
};

struct _VteTerminalAccessibleClass {
	GtkWidgetAccessibleClass parent_class;
};

GType _vte_terminal_accessible_get_type(void);

enum direction {
	direction_previous = -1,
	direction_current = 0,
	direction_next = 1
};

void
emit_text_caret_moved(GObject *object, glong caret);

void
emit_text_changed_insert(GObject *object,
			 const char *text, glong offset, glong len);

void
emit_text_changed_delete(GObject *object,
			 const char *text, glong offset, glong len);

void
vte_terminal_accessible_update_private_data_if_needed(VteTerminalAccessible *accessible,
                                                      GString **old_text,
                                                      GArray **old_characters);

void
vte_terminal_accessible_maybe_emit_text_caret_moved(VteTerminalAccessible *accessible);

/* A signal handler to catch "text-inserted/deleted/modified" signals. */
void
vte_terminal_accessible_text_modified(VteTerminal *terminal, gpointer data);

/* A signal handler to catch "text-scrolled" signals. */
void
vte_terminal_accessible_text_scrolled(VteTerminal *terminal,
				      gint howmuch,
				      gpointer data);

/* A signal handler to catch "cursor-moved" signals. */
void
vte_terminal_accessible_invalidate_cursor(VteTerminal *terminal, gpointer data);

/* Handle title changes by resetting the description. */
void
vte_terminal_accessible_title_changed(VteTerminal *terminal, gpointer data);

/* Reflect visibility-notify events. */
gboolean
vte_terminal_accessible_visibility_notify(VteTerminal *terminal,
					  GdkEventVisibility *event,
					  gpointer data);

void
vte_terminal_accessible_selection_changed (VteTerminal *terminal,
					   gpointer data);

void
vte_terminal_accessible_initialize (AtkObject *obj, gpointer data);

void
vte_terminal_accessible_init (VteTerminalAccessible *accessible);

void
vte_terminal_accessible_finalize(GObject *object);

gchar *
vte_terminal_accessible_get_text(AtkText *text,
				 gint start_offset, gint end_offset);

/* Map a subsection of the text with before/at/after char/word/line specs
 * into a run of Unicode characters.  (The interface is specifying characters,
 * not bytes, plus that saves us from having to deal with parts of multibyte
 * characters, which are icky.) */
gchar *
vte_terminal_accessible_get_text_somewhere(AtkText *text,
					   gint offset,
					   AtkTextBoundary boundary_type,
					   enum direction direction,
					   gint *start_offset,
					   gint *end_offset);

gchar *
vte_terminal_accessible_get_text_before_offset(AtkText *text, gint offset,
					       AtkTextBoundary boundary_type,
					       gint *start_offset,
					       gint *end_offset);

gchar *
vte_terminal_accessible_get_text_after_offset(AtkText *text, gint offset,
					      AtkTextBoundary boundary_type,
					      gint *start_offset,
					      gint *end_offset);

gchar *
vte_terminal_accessible_get_text_at_offset(AtkText *text, gint offset,
					   AtkTextBoundary boundary_type,
					   gint *start_offset,
					   gint *end_offset);

gunichar
vte_terminal_accessible_get_character_at_offset(AtkText *text, gint offset);

gint
vte_terminal_accessible_get_caret_offset(AtkText *text);

AtkAttributeSet *
get_attribute_set (struct _VteCharAttributes attr);

gboolean
pango_color_equal(const PangoColor *a,
                   const PangoColor *b);

AtkAttributeSet *
vte_terminal_accessible_get_run_attributes(AtkText *text, gint offset,
					   gint *start_offset, gint *end_offset);

AtkAttributeSet *
vte_terminal_accessible_get_default_attributes(AtkText *text);

void
vte_terminal_accessible_get_character_extents(AtkText *text, gint offset,
					      gint *x, gint *y,
					      gint *width, gint *height,
					      AtkCoordType coords);

gint
vte_terminal_accessible_get_character_count(AtkText *text);

gint
vte_terminal_accessible_get_offset_at_point(AtkText *text,
					    gint x, gint y,
					    AtkCoordType coords);

gint
vte_terminal_accessible_get_n_selections(AtkText *text);

gchar *
vte_terminal_accessible_get_selection(AtkText *text, gint selection_number,
				      gint *start_offset, gint *end_offset);

gboolean
vte_terminal_accessible_add_selection(AtkText *text,
				      gint start_offset, gint end_offset);

gboolean
vte_terminal_accessible_remove_selection(AtkText *text,
					 gint selection_number);

gboolean
vte_terminal_accessible_set_selection(AtkText *text, gint selection_number,
				      gint start_offset, gint end_offset);

gboolean
vte_terminal_accessible_set_caret_offset(AtkText *text, gint offset);

void
vte_terminal_accessible_text_iface_init(AtkTextIface *text);

gboolean
vte_terminal_accessible_set_extents(AtkComponent *component,
				    gint x, gint y,
				    gint width, gint height,
				    AtkCoordType coord_type);

gboolean
vte_terminal_accessible_set_position(AtkComponent *component,
				     gint x, gint y,
				     AtkCoordType coord_type);

gboolean
vte_terminal_accessible_set_size(AtkComponent *component,
				 gint width, gint height);

AtkObject *
vte_terminal_accessible_ref_accessible_at_point(AtkComponent *component,
						gint x, gint y,
						AtkCoordType coord_type);

void
vte_terminal_accessible_component_iface_init(AtkComponentIface *component);

/* AtkAction interface */

gboolean
vte_terminal_accessible_do_action (AtkAction *accessible, int i);

int
vte_terminal_accessible_get_n_actions (AtkAction *accessible);

const char *
vte_terminal_accessible_action_get_description (AtkAction *action, int i);

const char *
vte_terminal_accessible_action_get_name (AtkAction *accessible, int i);

const char *
vte_terminal_accessible_action_get_keybinding (AtkAction *accessible, int i);

gboolean
vte_terminal_accessible_action_set_description (AtkAction *action,
                                                int i,
                                                const char *description);

void
vte_terminal_accessible_action_iface_init(AtkActionIface *action);

void
vte_terminal_accessible_class_init(VteTerminalAccessibleClass *klass);

gboolean
vte_terminal_get_selection_block_mode(VteTerminal *terminal);

void
vte_terminal_set_selection_block_mode(VteTerminal *terminal, gboolean block_mode);

void
vte_terminal_select_text(VteTerminal *terminal,
			 long start_col, long start_row,
			 long end_col, long end_row);

void
vte_terminal_set_cursor_position(VteTerminal *terminal,
				 long column, long row);

G_END_DECLS

#endif
