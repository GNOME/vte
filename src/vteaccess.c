/*
 * Copyright (C) 2002 Red Hat, Inc.
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

/* VTE accessibility object.  Based heavily on inspection of libzvt's
 * accessibility code. */

#include "../config.h"
#include <iconv.h>
#include <atk/atk.h>
#include <gtk/gtk.h>
#include "vte.h"
#include "vteaccess.h"

#define VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA "VteTerminalAccessiblePrivateData"
typedef struct _VteTerminalAccessiblePrivate {
	gboolean snapshot_invalid;
	VteTerminalSnapshot *snapshot;
	GArray *snapshot_cells;
	GArray *snapshot_linebreaks;
	gint snapshot_caret;
} VteTerminalAccessiblePrivate;

static VteTerminalAccessiblePrivate *
vte_terminal_accessible_new_private_data(void)
{
	VteTerminalAccessiblePrivate *priv;
	priv = g_malloc0(sizeof(*priv));
	priv->snapshot = NULL;
	priv->snapshot_cells = NULL;
	priv->snapshot_linebreaks = NULL;
	priv->snapshot_caret = 0;
	priv->snapshot_invalid = TRUE;
	return priv;
}

static void
vte_terminal_accessible_update_private_data_if_needed(AtkObject *text)
{
	VteTerminal *terminal;
	VteTerminalAccessiblePrivate *priv;
	struct VteTerminalSnapshotCell cell;
	int row, col, i, caret;

	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text));

	/* Retrieve the private data structure. */
	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
	g_return_if_fail(priv != NULL);

	/* If nothing's changed, just return immediately. */
	if (priv->snapshot_invalid == FALSE) {
		return;
	}

	/* Free the possibly-outdated snapshot. */
	if (priv->snapshot != NULL) {
		vte_terminal_free_snapshot(priv->snapshot);
		priv->snapshot = NULL;
	}
	if (priv->snapshot_cells != NULL) {
		g_array_free(priv->snapshot_cells, FALSE);
		priv->snapshot_cells = NULL;
	}
	if (priv->snapshot_linebreaks != NULL) {
		g_array_free(priv->snapshot_linebreaks, FALSE);
		priv->snapshot_linebreaks = NULL;
	}
	priv->snapshot_caret = 0;

	/* Get a new snapshot, and munge it into something that might be
	 * mistaken for a continuous-text display widget. */
	terminal = VTE_TERMINAL((GTK_ACCESSIBLE(text))->widget);
	priv->snapshot = vte_terminal_get_snapshot(terminal);
	if (priv->snapshot == NULL) {
		/* Aaargh!  We're screwed. */
		return;
	}

	/* Get the addresses of each of the cells, and add them to a linear
	 * array of characters, tracking where line breaks occur, and setting
	 * the caret to point at the location where the cursor is. */
	priv->snapshot_cells = g_array_new(FALSE, TRUE, sizeof(cell));
	priv->snapshot_linebreaks = g_array_new(FALSE, TRUE, sizeof(int));
	caret = -1;
	for (row = 0; priv->snapshot->contents[row] != NULL; row++) {
		for (col = 0;
		     priv->snapshot->contents[row][col].c != 0;
		     col++) {
			if ((row == priv->snapshot->cursor.y) &&
			    (col == priv->snapshot->cursor.x)) {
				caret = priv->snapshot_cells->len;
			}
			cell = priv->snapshot->contents[row][col];
			g_array_append_val(priv->snapshot_cells, cell);

		}
		if ((row == priv->snapshot->cursor.y) && (caret == -1)) {
			caret = priv->snapshot_cells->len;
		}
		i = row;
		g_array_append_val(priv->snapshot_linebreaks, i);
	}
	if (caret == -1) {
		caret = priv->snapshot_cells->len;
	}
	priv->snapshot_caret = caret;
}

static void
vte_terminal_accessible_free_private_data(VteTerminalAccessiblePrivate *priv)
{
	g_return_if_fail(priv != NULL);
	if (priv->snapshot != NULL) {
		vte_terminal_free_snapshot(priv->snapshot);
		priv->snapshot = NULL;
	}
	if (priv->snapshot_cells != NULL) {
		g_array_free(priv->snapshot_cells, FALSE);
		priv->snapshot_cells = NULL;
	}
	if (priv->snapshot_linebreaks != NULL) {
		g_array_free(priv->snapshot_linebreaks, FALSE);
		priv->snapshot_linebreaks = NULL;
	}
	g_free(priv);
}

/* A signal handler to catch "contents_changed" and "cursor_moved" signals. */
static void
vte_terminal_accessible_invalidate(VteTerminal *terminal, gpointer data)
{
	VteTerminalAccessiblePrivate *priv;

	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));
	g_return_if_fail(G_IS_OBJECT(data));

	priv = g_object_get_data(G_OBJECT(data),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
	g_return_if_fail(priv != NULL);

	priv->snapshot_invalid = TRUE;
}

/* Handle hierarchy changes by resetting the parent object. */
static void
vte_terminal_accessible_hierarchy_changed(VteTerminal *terminal, gpointer data)
{
	AtkObject *atk, *parent;
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	atk = ATK_OBJECT(data);
	if (GTK_IS_WIDGET((GTK_WIDGET(terminal))->parent)) {
		parent = gtk_widget_get_accessible((GTK_WIDGET(terminal))->parent);
	} else {
		parent = NULL;
	}
	atk_object_set_parent(atk, parent);
}

/* Handle title changes by resetting the parent object. */
static void
vte_terminal_accessible_title_changed(VteTerminal *terminal, gpointer data)
{
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	atk_object_set_description(ATK_OBJECT(data), terminal->window_title);
}

AtkObject *
vte_terminal_accessible_new(VteTerminal *terminal)
{
	GtkAccessible *access;
	AtkObject *parent;
	GObject *object;

	g_return_val_if_fail(VTE_IS_TERMINAL(terminal), NULL);

	object = g_object_new(VTE_TYPE_TERMINAL_ACCESSIBLE, NULL);
	g_return_val_if_fail(GTK_IS_ACCESSIBLE(object), NULL);

	access = GTK_ACCESSIBLE(object);
	atk_object_initialize(ATK_OBJECT(access), G_OBJECT(terminal));

	access->widget = GTK_WIDGET(terminal);

	g_object_set_data(G_OBJECT(access),
			  VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA,
			  vte_terminal_accessible_new_private_data());
	g_signal_connect(G_OBJECT(terminal), "contents_changed",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_invalidate),
			 access);
	g_signal_connect(G_OBJECT(terminal), "cursor_moved",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_invalidate),
			 access);
        g_signal_connect(G_OBJECT(terminal), "hierarchy-changed",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_hierarchy_changed),
			 access);
        g_signal_connect(G_OBJECT(terminal), "window-title-changed",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_title_changed),
			 access);

	if (GTK_IS_WIDGET((GTK_WIDGET(terminal))->parent)) {
		parent = gtk_widget_get_accessible((GTK_WIDGET(terminal))->parent);
		atk_object_set_parent(ATK_OBJECT(access), parent);
	}

	atk_object_set_name(ATK_OBJECT(access), "Terminal");
	atk_object_set_description(ATK_OBJECT(access), terminal->window_title);

	return ATK_OBJECT(access);
}

static void
vte_terminal_accessible_finalize(GObject *object)
{
	GtkAccessible *accessible = NULL;
        GObjectClass *gobject_class; 

	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(object));
	accessible = GTK_ACCESSIBLE(object);
	gobject_class = g_type_class_peek_parent(VTE_TERMINAL_ACCESSIBLE_GET_CLASS(object));

	g_signal_handlers_disconnect_by_func(G_OBJECT(accessible->widget),
					     GTK_SIGNAL_FUNC(vte_terminal_accessible_invalidate),
					     accessible);
	g_signal_handlers_disconnect_by_func(G_OBJECT(accessible->widget),
					     GTK_SIGNAL_FUNC(vte_terminal_accessible_hierarchy_changed),
					     accessible);
	g_signal_handlers_disconnect_by_func(G_OBJECT(accessible->widget),
					     GTK_SIGNAL_FUNC(vte_terminal_accessible_title_changed),
					     accessible);
	if (gobject_class->finalize != NULL) {
		gobject_class->finalize(object);
	}
}

static gchar *
vte_terminal_accessible_get_text(AtkText *text,
				 gint start_offset, gint end_offset)
{
	VteTerminalAccessiblePrivate *priv;
	gchar *buf, *p;
	int i;

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));

	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);

	/* If the requested area is after all of the text, just return an
	 * empty string. */
	if (start_offset >= priv->snapshot_cells->len) {
		return g_strdup("");
	}

	/* Allocate space to hold as many UTF-8 characters as we have
	 * unicode characters. */
	p = buf = g_malloc((end_offset - start_offset) * VTE_UTF8_BPC + 1);
	for (i = start_offset; i < end_offset; i++) {
		p += g_unichar_to_utf8(g_array_index(priv->snapshot_cells,
						     struct VteTerminalSnapshotCell,
						     i).c,
				       p);
	}
	*p = '\0';
	return buf;
}

static gchar *
vte_terminal_accessible_get_text_somewhere(AtkText *text,
					   gint offset,
					   AtkTextBoundary boundary_type,
					   gint direction,
					   gint *start_offset,
					   gint *end_offset)
{
	VteTerminalAccessiblePrivate *priv;
	gunichar c;
	gboolean word, in_word;
	int i, line;

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));

	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);

	switch (boundary_type) {
		case ATK_TEXT_BOUNDARY_CHAR:
			/* We're either looking at the character at this
			 * position, the one before it, or the one after it. */
			offset += direction;
			*start_offset = MAX(offset, 0);
			*end_offset = MIN(offset + 1,
					  priv->snapshot_cells->len);
			break;
		case ATK_TEXT_BOUNDARY_WORD_START:
		case ATK_TEXT_BOUNDARY_WORD_END:
			/* Find the wordstart before the requested point.
			 * FIXME: use pango_break or g_unichar_break_type to
			 * find word boundaries. For now, this should work
			 * only for some locales. */
			c = g_array_index(priv->snapshot_cells,
					  struct VteTerminalSnapshotCell,
					  offset).c;
			word = in_word = !g_unichar_isspace(c);
			*start_offset = offset;
			for (i = offset; i >= 0; i--) {
				c = g_array_index(priv->snapshot_cells,
						  struct VteTerminalSnapshotCell,
						  i).c;
				if (word && g_unichar_isspace(c)) {
					*start_offset = i + 1;
					break;
				}
				if (i == 0) {
					*start_offset = 0;
					break;
				}
				word = g_unichar_isspace(c);
			}
			/* If we started in a word and we're looking for the
			 * word before this one, keep searching. */
			if (in_word && (direction == -1)) {
				word = !g_unichar_isspace(c);
				for (i = *start_offset; i >= 0; i--) {
					c = g_array_index(priv->snapshot_cells,
							  struct VteTerminalSnapshotCell,
							  i).c;
					if (word && g_unichar_isspace(c)) {
						*start_offset = i + 1;
						break;
					}
					if (i == 0) {
						*start_offset = 0;
						break;
					}
				}
			}
			/* If we're looking for the word after this one,
			 * search forward. */
			if (direction == 1) {
				word = g_unichar_isspace(c);
				for (i = *start_offset; i < priv->snapshot_cells->len; i--) {
					c = g_array_index(priv->snapshot_cells,
							  struct VteTerminalSnapshotCell,
							  i).c;
					if (!word && !g_unichar_isspace(c)) {
						*start_offset = i;
						break;
					}
					if (i == priv->snapshot_cells->len - 1) {
						*start_offset = i + 1;
						break;
					}
				}
			}
			/* Now find the end of this word. */
			word = TRUE;
			*end_offset = *start_offset;
			for (i = *start_offset; i < priv->snapshot_cells->len; i--) {
				c = g_array_index(priv->snapshot_cells,
						  struct VteTerminalSnapshotCell,
						  i).c;
				if (!word && !g_unichar_isspace(c)) {
					*end_offset = i;
					break;
				}
				if (i == priv->snapshot_cells->len - 1) {
					*end_offset = i + 1;
					break;
				}
			}
			break;
		case ATK_TEXT_BOUNDARY_LINE_START:
		case ATK_TEXT_BOUNDARY_LINE_END:
			/* Figure out which line we're on.  If the end of the
			 * i'th line is after the offset, then i is the line
			 * we're looking at. */
			line = 0;
			for (i = 0; i < priv->snapshot_cells->len; i++) {
				if (g_array_index(priv->snapshot_linebreaks,
						  int, i) > offset) {
					line = i;
					break;
				}
			}
			/* Perturb the line number to handle before/at/after. */
			line += direction;
			line = MAX(0, line);
			line = MIN(line, priv->snapshot_linebreaks->len - 1);
			/* Read the offsets for this line. */
			if (line == 0) {
				*start_offset = 0;
			} else {
				*start_offset = g_array_index(priv->snapshot_linebreaks,
							      int,
							      line - 1);
			}
			*end_offset = g_array_index(priv->snapshot_linebreaks,
						    int,
						    line);
			break;
		case ATK_TEXT_BOUNDARY_SENTENCE_START:
		case ATK_TEXT_BOUNDARY_SENTENCE_END:
			/* This doesn't make sense.  Fall through. */
		default:
			*start_offset = *end_offset = 0;
			break;
	}
	return vte_terminal_accessible_get_text(text,
						*start_offset,
						*end_offset);
}

static gchar *
vte_terminal_accessible_get_text_before_offset(AtkText *text, gint offset,
					       AtkTextBoundary boundary_type,
					       gint *start_offset,
					       gint *end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	return vte_terminal_accessible_get_text_somewhere(text,
							  offset,
							  boundary_type,
							  -1,
							  start_offset,
							  end_offset);
}

static gchar *
vte_terminal_accessible_get_text_after_offset(AtkText *text, gint offset,
					      AtkTextBoundary boundary_type,
					      gint *start_offset,
					      gint *end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	return vte_terminal_accessible_get_text_somewhere(text,
							  offset,
							  boundary_type,
							  1,
							  start_offset,
							  end_offset);
}

static gchar *
vte_terminal_accessible_get_text_at_offset(AtkText *text, gint offset,
					   AtkTextBoundary boundary_type,
					   gint *start_offset,
					   gint *end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	return vte_terminal_accessible_get_text_somewhere(text,
							  offset,
							  boundary_type,
							  0,
							  start_offset,
							  end_offset);
}

static gunichar
vte_terminal_accessible_get_character_at_offset(AtkText *text, gint offset)
{
	VteTerminalAccessiblePrivate *priv;

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));

	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);

	return g_array_index(priv->snapshot_cells,
			     struct VteTerminalSnapshotCell,
			     offset).c;
}

static gint
vte_terminal_accessible_get_caret_offset(AtkText *text)
{
	VteTerminalAccessiblePrivate *priv;

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));

	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);

	return priv->snapshot_caret;
}

static AtkAttributeSet *
vte_terminal_accessible_get_run_attributes(AtkText *text, gint offset,
					   gint *start_offset, gint *end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	/* FIXME */
	return NULL;
}

static AtkAttributeSet *
vte_terminal_accessible_get_default_attributes(AtkText *text)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	/* FIXME */
	return NULL;
}

static void
vte_terminal_accessible_get_character_extents(AtkText *text, gint offset,
					      gint *x, gint *y,
					      gint *width, gint *height,
					      AtkCoordType coords)
{
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text));
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	/* FIXME */
}

static gint
vte_terminal_accessible_get_character_count(AtkText *text)
{
	VteTerminalAccessiblePrivate *priv;

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));

	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);

	return priv->snapshot_cells->len;
}

static gint
vte_terminal_accessible_get_offset_at_point(AtkText *text,
					    gint x, gint y,
					    AtkCoordType coords)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), 0);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	/* FIXME */
	return 0;
}

static gint
vte_terminal_accessible_get_n_selections(AtkText *text)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), 0);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	/* FIXME? */
	return 0;
}

static gchar *
vte_terminal_accessible_get_selection(AtkText *text, gint selection_number,
				      gint *start_offset, gint *end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	/* FIXME? */
	return NULL;
}

static gboolean
vte_terminal_accessible_add_selection(AtkText *text,
				      gint start_offset, gint end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	/* FIXME? */
	return FALSE;
}

static gboolean
vte_terminal_accessible_remove_selection(AtkText *text,
					 gint selection_number)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	/* FIXME? */
	return FALSE;
}

static gboolean
vte_terminal_accessible_set_selection(AtkText *text, gint selection_number,
				      gint start_offset, gint end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	/* FIXME? */
	return FALSE;
}

static gboolean
vte_terminal_accessible_set_caret_offset(AtkText *text, gint offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
	/* Whoa, very not allowed. */
	return FALSE;
}

static void
vte_terminal_accessible_text_changed(AtkText *text, gint position, gint length)
{
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text));
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
}

static void
vte_terminal_accessible_text_caret_moved(AtkText *text, gint location)
{
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text));
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
}

static void
vte_terminal_accessible_text_selection_changed(AtkText *text)
{
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text));
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text));
}

static void
vte_terminal_accessible_text_init(gpointer iface, gpointer data)
{
	AtkTextIface *text;
	g_return_if_fail(G_TYPE_FROM_INTERFACE(iface) == ATK_TYPE_TEXT);
	text = iface;
	text->get_text = vte_terminal_accessible_get_text;
	text->get_text_after_offset = vte_terminal_accessible_get_text_after_offset;
	text->get_text_at_offset = vte_terminal_accessible_get_text_at_offset;
	text->get_character_at_offset = vte_terminal_accessible_get_character_at_offset;
	text->get_text_before_offset = vte_terminal_accessible_get_text_before_offset;
	text->get_caret_offset = vte_terminal_accessible_get_caret_offset;
	text->get_run_attributes = vte_terminal_accessible_get_run_attributes;
	text->get_default_attributes = vte_terminal_accessible_get_default_attributes;
	text->get_character_extents = vte_terminal_accessible_get_character_extents;
	text->get_character_count = vte_terminal_accessible_get_character_count;
	text->get_offset_at_point = vte_terminal_accessible_get_offset_at_point;
	text->get_n_selections = vte_terminal_accessible_get_n_selections;
	text->get_selection = vte_terminal_accessible_get_selection;
	text->add_selection = vte_terminal_accessible_add_selection;
	text->remove_selection = vte_terminal_accessible_remove_selection;
	text->set_selection = vte_terminal_accessible_set_selection;
	text->set_caret_offset = vte_terminal_accessible_set_caret_offset;
	text->text_changed = vte_terminal_accessible_text_changed;
	text->text_caret_moved = vte_terminal_accessible_text_caret_moved;
	text->text_selection_changed = vte_terminal_accessible_text_selection_changed;
}

static void
vte_terminal_accessible_text_finalize(gpointer iface, gpointer data)
{
	GtkAccessibleClass *accessible_class;
	VteTerminalAccessiblePrivate *priv;
	accessible_class = g_type_class_peek(GTK_TYPE_ACCESSIBLE); 

	/* Free the private data. */
	priv = g_object_get_data(G_OBJECT(iface),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
	if (priv) {
		vte_terminal_accessible_free_private_data(priv);
		g_object_set_data(G_OBJECT(iface),
				  VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA,
				  NULL);
	}

	if ((G_OBJECT_CLASS(accessible_class))->finalize) {
		(G_OBJECT_CLASS(accessible_class))->finalize(iface);
	}
}

static void
vte_terminal_accessible_class_init(gpointer *klass)
{
        GObjectClass *gobject_class; 

        gobject_class = G_OBJECT_CLASS(klass); 

	/* Override the finalize method. */
	gobject_class->finalize = vte_terminal_accessible_finalize;
}

static void
vte_terminal_accessible_init(gpointer *instance, gpointer *klass)
{
	/* Mark the role this object plays. The rest of the work is handled
	 * by the AtkText interface the object class exports. */
	g_return_if_fail(ATK_IS_OBJECT(instance));
	atk_object_set_role(ATK_OBJECT(instance), ATK_ROLE_TERMINAL);
}

GtkType
vte_terminal_accessible_get_type(void)
{
	static GtkType terminal_accessible_type = 0;
	static GInterfaceInfo text = {
		vte_terminal_accessible_text_init,
		vte_terminal_accessible_text_finalize,
		NULL,
	};
	static const GTypeInfo terminal_accessible_info = {
		sizeof(VteTerminalAccessibleClass),
		(GBaseInitFunc)NULL,
		(GBaseFinalizeFunc)NULL,

		(GClassInitFunc)vte_terminal_accessible_class_init,
		(GClassFinalizeFunc)NULL,
		(gconstpointer)NULL,

		sizeof(VteTerminalAccessible),
		0,
		(GInstanceInitFunc)vte_terminal_accessible_init,

		(GTypeValueTable*)NULL,
	};

	if (terminal_accessible_type == 0) {
		/* Register the class with the GObject type system. */
		terminal_accessible_type = g_type_register_static(GTK_TYPE_ACCESSIBLE,
								  "VteTerminalAccessible",
								  &terminal_accessible_info,
								  0);

		/* Add a text interface to this object class. */
		g_type_add_interface_static(terminal_accessible_type,
					    ATK_TYPE_TEXT,
					    &text);
	}

	return terminal_accessible_type;
}
