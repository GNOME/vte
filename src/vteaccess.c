/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
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

#ident "$Id$"

#include "../config.h"

#include <atk/atk.h>
#include <gtk/gtk.h>
#include <string.h>
#include "debug.h"
#include "vte.h"
#include "vteaccess.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#else
#define bindtextdomain(package,dir)
#endif

#define VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA "VteTerminalAccessiblePrivateData"
typedef struct _VteTerminalAccessiblePrivate {
	gboolean snapshot_contents_invalid;	/* This data is stale. */
	gboolean snapshot_caret_invalid;	/* This data is stale. */
	GString *snapshot_text;		/* Pointer to UTF-8 text. */
	GArray *snapshot_characters;	/* Offsets to character begin points. */
	GArray *snapshot_attributes;	/* Attributes, per byte. */
	GArray *snapshot_linebreaks;	/* Offsets to line breaks. */
	gint snapshot_caret;		/* Location of the cursor. */
} VteTerminalAccessiblePrivate;

enum direction {
	direction_previous = -1,
	direction_current = 0,
	direction_next = 1
};

static gunichar vte_terminal_accessible_get_character_at_offset(AtkText *text,
								gint offset);

static VteTerminalAccessiblePrivate *
vte_terminal_accessible_new_private_data(void)
{
	VteTerminalAccessiblePrivate *priv;
	priv = g_malloc0(sizeof(*priv));
	priv->snapshot_text = NULL;
	priv->snapshot_characters = NULL;
	priv->snapshot_attributes = NULL;
	priv->snapshot_linebreaks = NULL;
	priv->snapshot_caret = -1;
	priv->snapshot_contents_invalid = TRUE;
	priv->snapshot_caret_invalid = TRUE;
	return priv;
}

/* "Oh yeah, that's selected.  Sure."  */
static gboolean
all_selected(VteTerminal *terminal, glong column, glong row, gpointer data)
{
	return TRUE;
}

static void
emit_text_caret_moved(GObject *object, glong caret)
{
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Accessibility peer emitting "
			"`text-caret-moved'.\n");
	}
#endif
	g_signal_emit_by_name(object, "text-caret-moved", caret);
}

static void
emit_text_changed_insert(GObject *object,
			 const char *text, glong offset, glong len)
{
	const char *p;
	glong start, count;
	if (len == 0) {
		return;
	}
	/* Convert the byte offsets to character offsets. */
	start = 0;
	p = text;
	while (p < text + offset) {
		start++;
		p = g_utf8_next_char(p);
	}
	count = 0;
	p = text + offset;
	while (p < text + offset + len) {
		count++;
		p = g_utf8_next_char(p);
	}
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Accessibility peer emitting "
			"`text-changed::insert' (%ld, %ld) (%ld, %ld).\n",
			offset, len, start, count);
		fprintf(stderr, "Inserted text was `%.*s'.\n",
			(int) len, text + offset);
	}
#endif
	g_signal_emit_by_name(object, "text-changed::insert", start, count);
}

static void
emit_text_changed_delete(GObject *object,
			 const char *text, glong offset, glong len)
{
	const char *p;
	glong start, count;
	if (len == 0) {
		return;
	}
	/* Convert the byte offsets to characters. */
	start = 0;
	p = text;
	while (p < text + offset) {
		start++;
		p = g_utf8_next_char(p);
	}
	count = 0;
	p = text + offset;
	while (p < text + offset + len) {
		count++;
		p = g_utf8_next_char(p);
	}
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_SIGNALS)) {
		fprintf(stderr, "Accessibility peer emitting "
			"`text-changed::delete' (%ld, %ld) (%ld, %ld).\n",
			offset, len, start, count);
		fprintf(stderr, "Deleted text was `%.*s'.\n",
			(int) len, text + offset);
	}
#endif
	g_signal_emit_by_name(object, "text-changed::delete", start, count);
}

static void
vte_terminal_accessible_update_private_data_if_needed(AtkObject *text,
						      char **old, glong *olen)
{
	VteTerminal *terminal;
	VteTerminalAccessiblePrivate *priv;
	struct _VteCharAttributes attrs;
	char *next, *tmp;
	long row, i, offset, caret;
	long ccol, crow;

	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text));

	/* Retrieve the private data structure. */
	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
	g_return_if_fail(priv != NULL);

	/* If nothing's changed, just return immediately. */
	if ((priv->snapshot_contents_invalid == FALSE) &&
	    (priv->snapshot_caret_invalid == FALSE)) {
		if (old) {
			if (priv->snapshot_text) {
				*old = g_malloc(priv->snapshot_text->len + 1);
				memcpy(*old,
				       priv->snapshot_text->str,
				       priv->snapshot_text->len);
				(*old)[priv->snapshot_text->len] = '\0';
				if (olen) {
					*olen = priv->snapshot_text->len;
				}
			} else {
				*old = g_strdup("");
				if (olen) {
					*olen = 0;
				}
			}
		} else {
			if (olen) {
				g_assert_not_reached();
			}
		}
		return;
	}

	terminal = VTE_TERMINAL((GTK_ACCESSIBLE(text))->widget);

	/* Re-read the contents of the widget if the contents have changed. */
	if (priv->snapshot_contents_invalid) {
		/* Free the outdated snapshot data, unless the caller
		 * wants it. */
		if (old) {
			if (priv->snapshot_text != NULL) {
				*old = priv->snapshot_text->str;
				if (olen) {
					*olen = priv->snapshot_text->len;
				}
				g_string_free(priv->snapshot_text, FALSE);
			} else {
				*old = g_strdup("");
				if (olen) {
					*olen = 0;
				}
			}
		} else {
			if (olen) {
				g_assert_not_reached();
			}
			if (priv->snapshot_text != NULL) {
				g_string_free(priv->snapshot_text, TRUE);
			}
		}
		priv->snapshot_text = NULL;

		/* Free the character offsets and allocate a new array to hold
		 * them. */
		if (priv->snapshot_characters != NULL) {
			g_array_free(priv->snapshot_characters, TRUE);
			priv->snapshot_characters = NULL;
		}
		priv->snapshot_characters = g_array_new(FALSE, TRUE, sizeof(int));

		/* Free the attribute lists and allocate a new array to hold
		 * them. */
		if (priv->snapshot_attributes != NULL) {
			g_array_free(priv->snapshot_attributes, TRUE);
			priv->snapshot_attributes = NULL;
		}
		priv->snapshot_attributes = g_array_new(FALSE, TRUE,
							sizeof(struct _VteCharAttributes));

		/* Free the linebreak offsets and allocate a new array to hold
		 * them. */
		if (priv->snapshot_linebreaks != NULL) {
			g_array_free(priv->snapshot_linebreaks, TRUE);
			priv->snapshot_linebreaks = NULL;
		}
		priv->snapshot_linebreaks = g_array_new(FALSE, TRUE, sizeof(int));

		/* Get a new view of the uber-label. */
		tmp = vte_terminal_get_text(terminal,
					    all_selected,
					    NULL,
					    priv->snapshot_attributes);
		if (tmp == NULL) {
			/* Aaargh!  We're screwed. */
			return;
		}
		priv->snapshot_text = g_string_new_len(tmp,
						       priv->snapshot_attributes->len);
		g_free(tmp);

		/* Get the offsets to the beginnings of each character. */
		i = 0;
		next = priv->snapshot_text->str;
		while (i < priv->snapshot_attributes->len) {
			g_array_append_val(priv->snapshot_characters, i);
			next = g_utf8_next_char(next);
			if (next == NULL) {
				break;
			} else {
				i = next - priv->snapshot_text->str;
			}
		}
		/* Find offsets for the beginning of lines. */
		for (i = 0, row = 0; i < priv->snapshot_characters->len; i++) {
			/* Get the attributes for the current cell. */
			offset = g_array_index(priv->snapshot_characters,
					       int, i);
			attrs = g_array_index(priv->snapshot_attributes,
					      struct _VteCharAttributes,
					      offset);
			/* If this character is on a row different from the row
			 * the character we looked at previously was on, then
			 * it's a new line and we need to keep track of where
			 * it is. */
			if ((i == 0) || (attrs.row != row)) {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_MISC)) {
					fprintf(stderr, "Row %d/%ld begins at "
						"%ld.\n",
						priv->snapshot_linebreaks->len,
						attrs.row, i);
				}
#endif
				g_array_append_val(priv->snapshot_linebreaks, i);
			}
			row = attrs.row;
		}
		/* Add the final line break. */
		g_array_append_val(priv->snapshot_linebreaks, i);
		/* We're finished updating this. */
		priv->snapshot_contents_invalid = FALSE;
	}

	/* Update the caret position. */
	vte_terminal_get_cursor_position(terminal, &ccol, &crow);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Cursor at (%ld, " "%ld).\n", ccol, crow);
	}
#endif

	/* Get the offsets to the beginnings of each line. */
	caret = -1;
	for (i = 0; i < priv->snapshot_characters->len; i++) {
		/* Get the attributes for the current cell. */
		offset = g_array_index(priv->snapshot_characters,
				       int, i);
		attrs = g_array_index(priv->snapshot_attributes,
				      struct _VteCharAttributes,
				      offset);
		/* If this cell is "before" the cursor, move the
		 * caret to be "here". */
		if ((attrs.row < crow) ||
		    ((attrs.row == crow) && (attrs.column < ccol))) {
			caret = i + 1;
		}
	}

	/* If no cells are before the caret, then the caret must be
	 * at the end of the buffer. */
	if (caret == -1) {
		caret = priv->snapshot_characters->len;
	}

	/* Notify observers if the caret moved. */
	if (caret != priv->snapshot_caret) {
		priv->snapshot_caret = caret;
		emit_text_caret_moved(G_OBJECT(text), caret);
	}

	/* Done updating the caret position, whether we needed to or not. */
	priv->snapshot_caret_invalid = FALSE;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Refreshed accessibility snapshot, "
			"%ld cells.\n", (long)priv->snapshot_attributes->len);
	}
#endif
}

/* Free snapshot private data. */
static void
vte_terminal_accessible_free_private_data(VteTerminalAccessiblePrivate *priv)
{
	g_return_if_fail(priv != NULL);
	if (priv->snapshot_text != NULL) {
		g_string_free(priv->snapshot_text, TRUE);
		priv->snapshot_text = NULL;
	}
	if (priv->snapshot_characters != NULL) {
		g_array_free(priv->snapshot_characters, TRUE);
		priv->snapshot_characters = NULL;
	}
	if (priv->snapshot_attributes != NULL) {
		g_array_free(priv->snapshot_attributes, TRUE);
		priv->snapshot_attributes = NULL;
	}
	if (priv->snapshot_linebreaks != NULL) {
		g_array_free(priv->snapshot_linebreaks, TRUE);
		priv->snapshot_linebreaks = NULL;
	}
	g_free(priv);
}

/* A signal handler to catch "text-inserted/deleted/modified" signals. */
static void
vte_terminal_accessible_text_modified(VteTerminal *terminal, gpointer data)
{
	VteTerminalAccessiblePrivate *priv;
	char *old, *current;
	glong offset, olen, clen;

	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));

	priv = g_object_get_data(G_OBJECT(data),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
	g_return_if_fail(priv != NULL);

	priv->snapshot_contents_invalid = TRUE;
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(data),
							      &old, &olen);
	g_return_if_fail(old != NULL);

	current = priv->snapshot_text->str;
	clen = priv->snapshot_text->len;

	/* Find the offset where they don't match. */
	offset = 0;
	while ((offset < olen) && (offset < clen)) {
		if (old[offset] != current[offset]) {
			break;
		}
		offset++;
	}

	/* At least one of them had better have more data, right? */
	if ((offset < olen) || (offset < clen)) {
		/* Back up from both end points until we find the *last* point
		 * where they differed. */
		while ((olen > offset) && (clen > offset)) {
			if (old[olen - 1] != current[clen - 1]) {
				break;
			}
			olen--;
			clen--;
		}
		/* At least one of them has to have text the other
		 * doesn't. */
		g_assert((clen > offset) || (olen > offset));
		g_assert((clen >= 0) && (olen >= 0));
		/* Now emit a deleted signal for text that was in the old
		 * string but isn't in the new one... */
		if (olen > offset) {
			emit_text_changed_delete(G_OBJECT(data),
						 old,
						 offset,
						 olen - offset);
		}
		/* .. and an inserted signal for text that wasn't in the old
		 * string but is in the new one. */
		if (clen > offset) {
			emit_text_changed_insert(G_OBJECT(data),
						 current,
						 offset,
						 clen - offset);
		}
	}

	g_free(old);
}

/* A signal handler to catch "text-scrolled" signals. */
static void
vte_terminal_accessible_text_scrolled(VteTerminal *terminal,
				      gint howmuch,
				      gpointer data)
{
	VteTerminalAccessiblePrivate *priv;
	struct _VteCharAttributes attr;
	long i, len, delta;

	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));
	g_return_if_fail(howmuch != 0);

	priv = g_object_get_data(G_OBJECT(data),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
	g_return_if_fail(priv != NULL);

	if (((howmuch < 0) && (howmuch <= -terminal->row_count)) ||
	    ((howmuch > 0) && (howmuch >= terminal->row_count))) {
		/* All of the text was removed. */
		if (priv->snapshot_text != NULL) {
			if (priv->snapshot_text->str != NULL) {
				emit_text_changed_delete(G_OBJECT(data),
							 priv->snapshot_text->str,
							 0,
							 priv->snapshot_text->len);
			}
		}
		priv->snapshot_contents_invalid = TRUE;
		vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(data),
								      NULL,
								      NULL);
		/* All of the present text was added. */
		if (priv->snapshot_text != NULL) {
			if (priv->snapshot_text->str != NULL) {
				emit_text_changed_insert(G_OBJECT(data),
							 priv->snapshot_text->str,
							 0,
							 priv->snapshot_text->len);
			}
		}
		return;
	}
	/* Find the start point. */
	delta = 0;
	if (priv->snapshot_attributes != NULL) {
		if (priv->snapshot_attributes->len > 0) {
			attr = g_array_index(priv->snapshot_attributes,
					     struct _VteCharAttributes,
					     0);
			delta = attr.row;
		}
	}
	/* We scrolled up, so text was added at the top and removed
	 * from the bottom. */
	if ((howmuch < 0) && (howmuch > -terminal->row_count)) {
		howmuch = -howmuch;
		/* Find the first byte that scrolled off. */
		for (i = 0; i < priv->snapshot_attributes->len; i++) {
			attr = g_array_index(priv->snapshot_attributes,
					     struct _VteCharAttributes,
					     i);
			if (attr.row >= delta + terminal->row_count - howmuch) {
				break;
			}
		}
		if (i < priv->snapshot_attributes->len) {
			/* The rest of the string was deleted -- make a note. */
			emit_text_changed_delete(G_OBJECT(data),
						 priv->snapshot_text->str,
						 i,
						 priv->snapshot_attributes->len - i);
		}
		/* Refresh.  Note that i is now the length of the data which
		 * we expect to have left over. */
		priv->snapshot_contents_invalid = TRUE;
		vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(data),
								      NULL,
								      NULL);
		/* If we now have more text than before, the initial portion
		 * was added. */
		if (priv->snapshot_text != NULL) {
			len = priv->snapshot_text->len;
			if (len > i) {
				emit_text_changed_insert(G_OBJECT(data),
							 priv->snapshot_text->str,
							 0,
							 len - i);
			}
		}
		return;
	}
	/* We scrolled down, so text was added at the bottom and removed
	 * from the top. */
	if ((howmuch > 0) && (howmuch < terminal->row_count)) {
		/* Find the first byte that wasn't scrolled off the top. */
		for (i = 0; i < priv->snapshot_attributes->len; i++) {
			attr = g_array_index(priv->snapshot_attributes,
					     struct _VteCharAttributes,
					     i);
			if (attr.row >= delta + howmuch) {
				break;
			}
		}
		/* That many bytes disappeared -- make a note. */
		emit_text_changed_delete(G_OBJECT(data),
					 priv->snapshot_text->str,
					 0,
					 i);
		/* Figure out how much text was left, and refresh. */
		i = strlen(priv->snapshot_text->str + i);
		priv->snapshot_contents_invalid = TRUE;
		vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(data),
								      NULL,
								      NULL);
		/* Any newly-added string data is new, so note that it was
		 * inserted. */
		if (priv->snapshot_text != NULL) {
			len = priv->snapshot_text->len;
			if (len > i) {
				emit_text_changed_insert(G_OBJECT(data),
							 priv->snapshot_text->str,
							 i,
							 len - i);
			}
		}
		return;
	}
	g_assert_not_reached();
}

/* A signal handler to catch "cursor-moved" signals. */
static void
vte_terminal_accessible_invalidate_cursor(VteTerminal *terminal, gpointer data)
{
	VteTerminalAccessiblePrivate *priv;

	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));

	priv = g_object_get_data(G_OBJECT(data),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
	g_return_if_fail(priv != NULL);

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Invalidating accessibility cursor.\n");
	}
#endif
	priv->snapshot_caret_invalid = TRUE;
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(data),
							      NULL, NULL);
}

/* Handle title changes by resetting the parent object. */
static void
vte_terminal_accessible_title_changed(VteTerminal *terminal, gpointer data)
{
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	atk_object_set_description(ATK_OBJECT(data), terminal->window_title);
}

/* Reflect focus-in events. */
static void
vte_terminal_accessible_focus_in(VteTerminal *terminal, GdkEventFocus *event,
				 gpointer data)
{
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_signal_emit_by_name(data, "focus-event", TRUE);
	atk_object_notify_state_change(ATK_OBJECT(data),
				       ATK_STATE_FOCUSED, TRUE);
}

/* Reflect focus-out events. */
static void
vte_terminal_accessible_focus_out(VteTerminal *terminal, GdkEventFocus *event,
				  gpointer data)
{
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	g_signal_emit_by_name(data, "focus-event", FALSE);
	atk_object_notify_state_change(ATK_OBJECT(data),
				       ATK_STATE_FOCUSED, TRUE);
}

/* Reflect visibility-notify events. */
static void
vte_terminal_accessible_visibility_notify(VteTerminal *terminal,
					  GdkEventVisibility *event,
					  gpointer data)
{
	GtkWidget *widget;
	gboolean visible;
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	visible = event->state != GDK_VISIBILITY_FULLY_OBSCURED;
	atk_object_notify_state_change(ATK_OBJECT(data),
				       ATK_STATE_VISIBLE,
				       visible);
	widget = GTK_WIDGET(terminal);
	while (visible) {
		if (gtk_widget_get_toplevel(widget) == widget) {
			break;
		}
		if (widget == NULL) {
			break;
		}
		visible = visible && (GTK_WIDGET_VISIBLE(widget));
		widget = gtk_widget_get_parent(widget);
	}
	atk_object_notify_state_change(ATK_OBJECT(data),
				       ATK_STATE_SHOWING,
				       visible);
}

/**
 * vte_terminal_accessible_new:
 * @terminal: a #VteTerminal
 *
 * Creates a new accessibility peer for the terminal widget.
 *
 * Returns: the new #AtkObject
 */
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
	g_object_add_weak_pointer(G_OBJECT(terminal),
				  (gpointer*)&access->widget);

	g_object_set_data(G_OBJECT(access),
			  VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA,
			  vte_terminal_accessible_new_private_data());

	g_signal_connect(G_OBJECT(terminal), "text-inserted",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_text_modified),
			 object);
	g_signal_connect(G_OBJECT(terminal), "text-deleted",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_text_modified),
			 object);
	g_signal_connect(G_OBJECT(terminal), "text-modified",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_text_modified),
			 object);
	g_signal_connect(G_OBJECT(terminal), "text-scrolled",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_text_scrolled),
			 object);
	g_signal_connect(G_OBJECT(terminal), "cursor-moved",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_invalidate_cursor),
			 object);
        g_signal_connect(G_OBJECT(terminal), "window-title-changed",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_title_changed),
			 access);
        g_signal_connect(G_OBJECT(terminal), "focus-in-event",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_focus_in),
			 access);
        g_signal_connect(G_OBJECT(terminal), "focus-out-event",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_focus_out),
			 access);
        g_signal_connect(G_OBJECT(terminal), "visibility-notify",
			 GTK_SIGNAL_FUNC(vte_terminal_accessible_visibility_notify),
			 access);

	if (GTK_IS_WIDGET((GTK_WIDGET(terminal))->parent)) {
		parent = gtk_widget_get_accessible((GTK_WIDGET(terminal))->parent);
		atk_object_set_parent(ATK_OBJECT(access), parent);
	}

	atk_object_set_name(ATK_OBJECT(access), "Terminal");
	atk_object_set_description(ATK_OBJECT(access),
				   terminal->window_title ?
				   terminal->window_title :
				   "");

	atk_object_notify_state_change(ATK_OBJECT(access),
				       ATK_STATE_FOCUSABLE, TRUE);

	return ATK_OBJECT(access);
}

static void
vte_terminal_accessible_finalize(GObject *object)
{
	GtkAccessible *accessible = NULL;
        GObjectClass *gobject_class; 

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Finalizing accessible peer.\n");
	}
#endif

	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(object));
	accessible = GTK_ACCESSIBLE(object);
	gobject_class = g_type_class_peek_parent(VTE_TERMINAL_ACCESSIBLE_GET_CLASS(object));

	if (accessible->widget != NULL) {
		g_object_remove_weak_pointer(G_OBJECT(accessible->widget),
					     (gpointer*) &accessible->widget);
	}
	if (G_IS_OBJECT(accessible->widget)) {
		g_signal_handlers_disconnect_matched(G_OBJECT(accessible->widget),
						     G_SIGNAL_MATCH_FUNC |
						     G_SIGNAL_MATCH_DATA,
						     0, 0, NULL,
						     vte_terminal_accessible_text_modified,
						     object);
		g_signal_handlers_disconnect_matched(G_OBJECT(accessible->widget),
						     G_SIGNAL_MATCH_FUNC |
						     G_SIGNAL_MATCH_DATA,
						     0, 0, NULL,
						     vte_terminal_accessible_text_scrolled,
						     object);
		g_signal_handlers_disconnect_matched(G_OBJECT(accessible->widget),
						     G_SIGNAL_MATCH_FUNC |
						     G_SIGNAL_MATCH_DATA,
						     0, 0, NULL,
						     vte_terminal_accessible_invalidate_cursor,
						     object);
		g_signal_handlers_disconnect_matched(G_OBJECT(accessible->widget),
						     G_SIGNAL_MATCH_FUNC |
						     G_SIGNAL_MATCH_DATA,
						     0, 0, NULL,
						     (gpointer)vte_terminal_accessible_title_changed,
						     object);
		g_signal_handlers_disconnect_matched(G_OBJECT(accessible->widget),
						     G_SIGNAL_MATCH_FUNC |
						     G_SIGNAL_MATCH_DATA,
						     0, 0, NULL,
						     (gpointer)vte_terminal_accessible_focus_in,
						     object);
		g_signal_handlers_disconnect_matched(G_OBJECT(accessible->widget),
						     G_SIGNAL_MATCH_FUNC |
						     G_SIGNAL_MATCH_DATA,
						     0, 0, NULL,
						     (gpointer)vte_terminal_accessible_focus_out,
						     object);
		g_signal_handlers_disconnect_matched(G_OBJECT(accessible->widget),
						     G_SIGNAL_MATCH_FUNC |
						     G_SIGNAL_MATCH_DATA,
						     0, 0, NULL,
						     (gpointer)vte_terminal_accessible_visibility_notify,
						     object);
	}
	if (gobject_class->finalize != NULL) {
		gobject_class->finalize(object);
	}
}

static gchar *
vte_terminal_accessible_get_text(AtkText *text,
				 gint start_offset, gint end_offset)
{
	VteTerminalAccessiblePrivate *priv;
	int start, end;
	gchar *ret;

	g_return_val_if_fail((start_offset >= 0) && (end_offset >= -1),
			     g_strdup(""));

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);

	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Getting text from %d to %d of %d.\n",
			start_offset, end_offset,
			priv->snapshot_characters->len);
	}
#endif
	g_return_val_if_fail(ATK_IS_TEXT(text), g_strdup(""));

	/* If the requested area is after all of the text, just return an
	 * empty string. */
	if (start_offset >= priv->snapshot_characters->len) {
		return g_strdup("");
	}

	/* Map the offsets to, er, offsets. */
	start = g_array_index(priv->snapshot_characters, int, start_offset);
	if ((end_offset == -1) || (end_offset >= priv->snapshot_characters->len) ) {
		/* Get everything up to the end of the buffer. */
		end = priv->snapshot_text->len;
	} else {
		/* Map the stopping point. */
		end = g_array_index(priv->snapshot_characters, int, end_offset);
	}
	ret = g_malloc(end - start + 1);
	memcpy(ret, priv->snapshot_text->str + start, end - start);
	ret[end - start] = '\0';
	return ret;
}

/* Map a subsection of the text with before/at/after char/word/line specs
 * into a run of Unicode characters.  (The interface is specifying characters,
 * not bytes, plus that saves us from having to deal with parts of multibyte
 * characters, which are icky.) */
static gchar *
vte_terminal_accessible_get_text_somewhere(AtkText *text,
					   gint offset,
					   AtkTextBoundary boundary_type,
					   enum direction direction,
					   gint *start_offset,
					   gint *end_offset)
{
	VteTerminalAccessiblePrivate *priv;
	VteTerminal *terminal;
	gunichar current, prev, next;
	int line;

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);

	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
	terminal = VTE_TERMINAL((GTK_ACCESSIBLE(text))->widget);

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Getting %s %s at %d of %d.\n",
			(direction == direction_current) ? "this" :
			((direction == direction_next) ? "next" : "previous"),
			(boundary_type == ATK_TEXT_BOUNDARY_CHAR) ? "char" :
			((boundary_type == ATK_TEXT_BOUNDARY_LINE_START) ? "line (start)" :
			((boundary_type == ATK_TEXT_BOUNDARY_LINE_END) ? "line (end)" :
			((boundary_type == ATK_TEXT_BOUNDARY_WORD_START) ? "word (start)" :
			((boundary_type == ATK_TEXT_BOUNDARY_WORD_END) ? "word (end)" :
			((boundary_type == ATK_TEXT_BOUNDARY_SENTENCE_START) ? "sentence (start)" :
			((boundary_type == ATK_TEXT_BOUNDARY_SENTENCE_END) ? "sentence (end)" : "unknown")))))),
			offset, priv->snapshot_attributes->len);
	}
#endif
	g_return_val_if_fail(priv->snapshot_text != NULL, g_strdup(""));
	g_return_val_if_fail(priv->snapshot_characters != NULL, g_strdup(""));
	if (offset == priv->snapshot_characters->len) {
		return g_strdup("");
	}
	g_return_val_if_fail(offset < priv->snapshot_characters->len,
			     g_strdup(""));
	g_return_val_if_fail(offset >= 0, g_strdup(""));

	switch (boundary_type) {
		case ATK_TEXT_BOUNDARY_CHAR:
			/* We're either looking at the character at this
			 * position, the one before it, or the one after it. */
			offset += direction;
			*start_offset = MAX(offset, 0);
			*end_offset = MIN(offset + 1,
					  priv->snapshot_attributes->len);
			break;
		case ATK_TEXT_BOUNDARY_WORD_START:
		case ATK_TEXT_BOUNDARY_WORD_END:
			/* Back up to the previous non-word-word transition. */
			while (offset > 0) {
				prev = vte_terminal_accessible_get_character_at_offset(text, offset - 1);
				if (vte_terminal_is_word_char(terminal, prev)) {
					offset--;
				} else {
					break;
				}
			}
			*start_offset = offset;
			/* If we started in a word and we're looking for the
			 * word before this one, keep searching by backing up
			 * to the previous non-word character and then searching
			 * for the word-start before that. */
			if (direction == direction_previous) {
				while (offset > 0) {
					prev = vte_terminal_accessible_get_character_at_offset(text, offset - 1);
					if (!vte_terminal_is_word_char(terminal, prev)) {
						offset--;
					} else {
						break;
					}
				}
				while (offset > 0) {
					prev = vte_terminal_accessible_get_character_at_offset(text, offset - 1);
					if (vte_terminal_is_word_char(terminal, prev)) {
						offset--;
					} else {
						break;
					}
				}
				*start_offset = offset;
			}
			/* If we're looking for the word after this one,
			 * search forward by scanning forward for the next
			 * non-word character, then the next word character
			 * after that. */
			if (direction == direction_next) {
				while (offset < priv->snapshot_characters->len) {
					next = vte_terminal_accessible_get_character_at_offset(text, offset);
					if (vte_terminal_is_word_char(terminal, next)) {
						offset++;
					} else {
						break;
					}
				}
				while (offset < priv->snapshot_characters->len) {
					next = vte_terminal_accessible_get_character_at_offset(text, offset);
					if (!vte_terminal_is_word_char(terminal, next)) {
						offset++;
					} else {
						break;
					}
				}
				*start_offset = offset;
			}
			/* Now find the end of this word. */
			while (offset < priv->snapshot_characters->len) {
				current = vte_terminal_accessible_get_character_at_offset(text, offset);
				if (vte_terminal_is_word_char(terminal, current)) {
					offset++;
				} else {
					break;
				}

			}
			*end_offset = offset;
			break;
		case ATK_TEXT_BOUNDARY_LINE_START:
		case ATK_TEXT_BOUNDARY_LINE_END:
			/* Figure out which line we're on.  If the start of the
			 * i'th line is before the offset, then i could be the
			 * line we're looking for. */
			line = 0;
			for (line = 0;
			     line < priv->snapshot_linebreaks->len;
			     line++) {
				if (g_array_index(priv->snapshot_linebreaks,
						  int, line) > offset) {
					line--;
					break;
				}
			}
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_MISC)) {
				fprintf(stderr, "Character %d is on line %d.\n",
					offset, line);
			}
#endif
			/* Perturb the line number to handle before/at/after. */
			line += direction;
			line = CLAMP(line,
				     0, priv->snapshot_linebreaks->len - 1);
			/* Read the offsets for this line. */
			*start_offset = g_array_index(priv->snapshot_linebreaks,
						      int, line);
			line++;
			line = CLAMP(line,
				     0, priv->snapshot_linebreaks->len - 1);
			*end_offset = g_array_index(priv->snapshot_linebreaks,
						    int, line);
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_MISC)) {
				fprintf(stderr, "Line runs from %d to %d.\n",
					*start_offset, *end_offset);
			}
#endif
			break;
		case ATK_TEXT_BOUNDARY_SENTENCE_START:
		case ATK_TEXT_BOUNDARY_SENTENCE_END:
			/* This doesn't make sense.  Fall through. */
		default:
			*start_offset = *end_offset = 0;
			break;
	}
	*start_offset = MIN(*start_offset, priv->snapshot_characters->len - 1);
	*end_offset = CLAMP(*end_offset, *start_offset,
			    priv->snapshot_characters->len);
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
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
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
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
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
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
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
	int mapped;
	char *unichar;
	gunichar ret;

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);

	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);

	g_return_val_if_fail(offset < priv->snapshot_characters->len, 0);

	mapped = g_array_index(priv->snapshot_characters, int, offset);

	unichar = vte_terminal_accessible_get_text(text, offset, offset + 1);
	ret = g_utf8_get_char(unichar);
	g_free(unichar);

	return ret;
}

static gint
vte_terminal_accessible_get_caret_offset(AtkText *text)
{
	VteTerminalAccessiblePrivate *priv;

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);

	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);

	return priv->snapshot_caret;
}

static AtkAttributeSet *
vte_terminal_accessible_get_run_attributes(AtkText *text, gint offset,
					   gint *start_offset, gint *end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
	/* FIXME */
	return NULL;
}

static AtkAttributeSet *
vte_terminal_accessible_get_default_attributes(AtkText *text)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
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
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
	/* FIXME */
}

static gint
vte_terminal_accessible_get_character_count(AtkText *text)
{
	VteTerminalAccessiblePrivate *priv;

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);

	priv = g_object_get_data(G_OBJECT(text),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);

	return priv->snapshot_attributes->len;
}

static gint
vte_terminal_accessible_get_offset_at_point(AtkText *text,
					    gint x, gint y,
					    AtkCoordType coords)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), 0);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
	/* FIXME */
	return 0;
}

static gint
vte_terminal_accessible_get_n_selections(AtkText *text)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), 0);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
	/* FIXME? */
	return 0;
}

static gchar *
vte_terminal_accessible_get_selection(AtkText *text, gint selection_number,
				      gint *start_offset, gint *end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
	/* FIXME? */
	return NULL;
}

static gboolean
vte_terminal_accessible_add_selection(AtkText *text,
				      gint start_offset, gint end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
	/* FIXME? */
	return FALSE;
}

static gboolean
vte_terminal_accessible_remove_selection(AtkText *text,
					 gint selection_number)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
	/* FIXME? */
	return FALSE;
}

static gboolean
vte_terminal_accessible_set_selection(AtkText *text, gint selection_number,
				      gint start_offset, gint end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
	/* FIXME? */
	return FALSE;
}

static gboolean
vte_terminal_accessible_set_caret_offset(AtkText *text, gint offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL, NULL);
	/* Whoa, very not allowed. */
	return FALSE;
}

static void
vte_terminal_accessible_text_init(gpointer iface, gpointer data)
{
	AtkTextIface *text;
	g_return_if_fail(G_TYPE_FROM_INTERFACE(iface) == ATK_TYPE_TEXT);
	text = iface;
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Initializing accessible peer's "
			"AtkText interface.\n");
	}
#endif
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
}

static void
vte_terminal_accessible_text_finalize(gpointer iface, gpointer data)
{
	GtkAccessibleClass *accessible_class;
	VteTerminalAccessiblePrivate *priv;
	accessible_class = g_type_class_peek(GTK_TYPE_ACCESSIBLE); 

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Finalizing accessible peer's "
			"AtkText interface.\n");
	}
#endif

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

static AtkLayer
vte_terminal_accessible_component_get_layer(AtkComponent *component)
{
	return ATK_LAYER_WIDGET;
}

static gint
vte_terminal_accessible_component_get_mdi_zorder(AtkComponent *component)
{
	return G_MININT;
}

static gboolean
vte_terminal_accessible_component_contains(AtkComponent *component,
					   gint x, gint y,
					   AtkCoordType coord_type)
{
	gint ex, ey, ewidth, eheight;
	atk_component_get_extents(component, &ex, &ey, &ewidth, &eheight,
				  coord_type);
	return ((x >= ex) &&
		(x < ex + ewidth) &&
		(y >= ey) &&
		(y < ey + eheight));
}

static void
vte_terminal_accessible_component_get_extents(AtkComponent *component,
					      gint *x, gint *y,
					      gint *width, gint *height,
					      AtkCoordType coord_type)
{
	atk_component_get_position(component, x, y, coord_type);
	atk_component_get_size(component, width, height);
}

static void
vte_terminal_accessible_component_get_position(AtkComponent *component,
					       gint *x, gint *y,
					       AtkCoordType coord_type)
{
	GtkWidget *widget;
	*x = 0;
	*y = 0;
	widget = (GTK_ACCESSIBLE(component))->widget;
	if (!GTK_WIDGET_REALIZED(widget)) {
		return;
	}
	switch (coord_type) {
	case ATK_XY_SCREEN:
		gdk_window_get_origin(widget->window, x, y);
		break;
	case ATK_XY_WINDOW:
		gdk_window_get_position(widget->window, x, y);
		break;
	default:
		g_assert_not_reached();
		break;
	}
}

static void
vte_terminal_accessible_component_get_size(AtkComponent *component,
					   gint *width, gint *height)
{
	GtkWidget *widget;
	*width = 0;
	*height = 0;
	widget = (GTK_ACCESSIBLE(component))->widget;
	if (!GTK_WIDGET_REALIZED(widget)) {
		return;
	}
	gdk_drawable_get_size(widget->window, width, height);
}

static gboolean
vte_terminal_accessible_component_set_extents(AtkComponent *component,
					      gint x, gint y,
					      gint width, gint height,
					      AtkCoordType coord_type)
{
	/* FIXME (?) */
	return FALSE;
}

static gboolean
vte_terminal_accessible_component_set_position(AtkComponent *component,
					       gint x, gint y,
					       AtkCoordType coord_type)
{
	/* FIXME (?) */
	return FALSE;
}

static gboolean
vte_terminal_accessible_component_set_size(AtkComponent *component,
					   gint width, gint height)
{
	VteTerminal *terminal;
	gint columns, rows, xpad, ypad;
	terminal = VTE_TERMINAL((GTK_ACCESSIBLE(component))->widget);
	vte_terminal_get_padding(terminal, &xpad, &ypad);
	if (width % terminal->char_width == 0) {
		columns = width / terminal->char_width;
	} else {
		columns = (width + xpad) / terminal->char_width;
	}
	if (height % terminal->char_height == 0) {
		rows = height / terminal->char_height;
	} else {
		rows = (height + xpad) / terminal->char_height;
	}
	vte_terminal_set_size(terminal, columns, rows);
	return (terminal->row_count == rows) &&
	       (terminal->column_count == columns);
}

static gboolean
vte_terminal_accessible_component_grab_focus(AtkComponent *component)
{
	GtkWidget *widget;
	widget = (GTK_ACCESSIBLE(component))->widget;
	if (GTK_WIDGET_HAS_FOCUS(widget)) {
		return TRUE;
	}
	gtk_widget_grab_focus(widget);
	if (GTK_WIDGET_HAS_FOCUS(widget)) {
		return TRUE;
	}
	return FALSE;
}

static AtkObject *
vte_terminal_accessible_component_ref_accessible_at_point(AtkComponent *component,
							  gint x, gint y,
							  AtkCoordType coord_type)
{
	/* There are no children. */
	return NULL;
}

static guint
vte_terminal_accessible_component_add_focus_handler(AtkComponent *component,
						    AtkFocusHandler handler)
{
	guint signal_id;
	signal_id = g_signal_lookup("focus-event",
				    VTE_TYPE_TERMINAL_ACCESSIBLE);
	if (g_signal_handler_find(component,
				  G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_ID,
				  signal_id,
				  0,
				  NULL,
				  handler,
				  NULL) != 0) {
		return 0;
	}
	return g_signal_connect(component, "focus-event",
				G_CALLBACK(handler), NULL);
}

static void
vte_terminal_accessible_component_remove_focus_handler(AtkComponent *component,
						       guint handler_id)
{
	g_return_if_fail(g_signal_handler_is_connected(component, handler_id));
	g_signal_handler_disconnect(component, handler_id);
}

static void
vte_terminal_accessible_component_init(gpointer iface, gpointer data)
{
	AtkComponentIface *component;
	g_return_if_fail(G_TYPE_FROM_INTERFACE(iface) == ATK_TYPE_COMPONENT);
	component = iface;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Initializing accessible peer's "
			"AtkComponent interface.\n");
	}
#endif
	/* Set our virtual functions. */
	component->add_focus_handler = vte_terminal_accessible_component_add_focus_handler;
	component->contains = vte_terminal_accessible_component_contains;
	component->ref_accessible_at_point = vte_terminal_accessible_component_ref_accessible_at_point;
	component->get_extents = vte_terminal_accessible_component_get_extents;
	component->get_position = vte_terminal_accessible_component_get_position;
	component->get_size = vte_terminal_accessible_component_get_size;
	component->grab_focus = vte_terminal_accessible_component_grab_focus;
	component->remove_focus_handler = vte_terminal_accessible_component_remove_focus_handler;
	component->set_extents = vte_terminal_accessible_component_set_extents;
	component->set_position = vte_terminal_accessible_component_set_position;
	component->set_size = vte_terminal_accessible_component_set_size;
	component->get_layer = vte_terminal_accessible_component_get_layer;
	component->get_mdi_zorder = vte_terminal_accessible_component_get_mdi_zorder;

}

static void
vte_terminal_accessible_component_finalize(gpointer iface, gpointer data)
{
	GtkAccessibleClass *accessible_class;
	accessible_class = g_type_class_peek(GTK_TYPE_ACCESSIBLE); 

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Finalizing accessible peer's "
			"AtkComponent interface.\n");
	}
#endif
}

static void
vte_terminal_accessible_class_init(gpointer *klass)
{
        GObjectClass *gobject_class; 

	bindtextdomain(PACKAGE, LOCALEDIR);

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
	static GInterfaceInfo component = {
		vte_terminal_accessible_component_init,
		vte_terminal_accessible_component_finalize,
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
		/* Add a component interface to this object class. */
		g_type_add_interface_static(terminal_accessible_type,
					    ATK_TYPE_COMPONENT,
					    &component);
	}

	return terminal_accessible_type;
}
