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
	gchar *snapshot_text;		/* Pointer to UTF-8 text. */
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
	/* Convert the byte offsets to characters. */
	for (p = text, start = 0;
	     p < text + offset;
	     p = g_utf8_next_char(p)) {
		start++;
	}
	for (p = text + offset, count = 0;
	     p < text + offset + len;
	     p = g_utf8_next_char(p)) {
		count++;
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
	for (p = text, start = 0;
	     p < text + offset;
	     p = g_utf8_next_char(p)) {
		start++;
	}
	for (p = text + offset, count = 0;
	     p < text + offset + len;
	     p = g_utf8_next_char(p)) {
		count++;
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
						      char **old)
{
	VteTerminal *terminal;
	VteTerminalAccessiblePrivate *priv;
	struct vte_char_attributes attrs;
	char *next;
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
				*old = g_strdup(priv->snapshot_text);
			} else {
				*old = g_strdup("");
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
				*old = priv->snapshot_text;
			} else {
				*old = g_strdup("");
			}
		} else {
			g_free(priv->snapshot_text);
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
							sizeof(struct vte_char_attributes));

		/* Free the linebreak offsets and allocate a new array to hold
		 * them. */
		if (priv->snapshot_linebreaks != NULL) {
			g_array_free(priv->snapshot_linebreaks, TRUE);
			priv->snapshot_linebreaks = NULL;
		}
		priv->snapshot_linebreaks = g_array_new(FALSE, TRUE, sizeof(int));

		/* Get a new view of the uber-label. */
		priv->snapshot_text = vte_terminal_get_text(terminal,
							    all_selected,
							    NULL,
							    priv->snapshot_attributes);
		if (priv->snapshot_text == NULL) {
			/* Aaargh!  We're screwed. */
			return;
		}

		/* Get the offsets to the beginnings of each character. */
		i = 0;
		next = priv->snapshot_text;
		while (i < priv->snapshot_attributes->len) {
			g_array_append_val(priv->snapshot_characters, i);
			next = g_utf8_find_next_char(next, NULL);
			if (next == NULL) {
				break;
			} else {
				i = next - priv->snapshot_text;
			}
		}
		/* Find offsets for the beginning of lines. */
		for (i = 0, row = 0; i < priv->snapshot_characters->len; i++) {
			/* Get the attributes for the current cell. */
			offset = g_array_index(priv->snapshot_characters,
					       int, i);
			attrs = g_array_index(priv->snapshot_attributes,
					      struct vte_char_attributes,
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
					fprintf(stderr, "Cursor at (%ld, "
						"%ld).\n", ccol, crow);
				}
#endif
				g_array_append_val(priv->snapshot_linebreaks,
						   i);
			}
			row = attrs.row;
		}
		/* Add the final line break. */
		g_array_append_val(priv->snapshot_linebreaks, i);
		/* We're finished updating this. */
		priv->snapshot_contents_invalid = FALSE;
	}
	if (priv->snapshot_caret_invalid) {
		vte_terminal_get_cursor_position(terminal, &ccol, &crow);

		/* Get the offsets to the beginnings of each line. */
		caret = -1;
		for (i = 0; i < priv->snapshot_characters->len; i++) {
			/* Get the attributes for the current cell. */
			offset = g_array_index(priv->snapshot_characters,
					       int, i);
			attrs = g_array_index(priv->snapshot_attributes,
					      struct vte_char_attributes,
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
		/* The caret may have moved. */
		if (caret != priv->snapshot_caret) {
			priv->snapshot_caret = caret;
			emit_text_caret_moved(G_OBJECT(text), caret);
		}
		/* Done updating the caret position, too. */
		priv->snapshot_caret_invalid = FALSE;
	}
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
		g_free(priv->snapshot_text);
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
	char *old;
	glong offset, olen, nlen;

	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));

	priv = g_object_get_data(G_OBJECT(data),
				 VTE_TERMINAL_ACCESSIBLE_PRIVATE_DATA);
	g_return_if_fail(priv != NULL);

	priv->snapshot_contents_invalid = TRUE;
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(data),
							      &old);
	g_return_if_fail(old != NULL);

	olen = strlen(old);
	nlen = strlen(priv->snapshot_text);

	/* Find the offset where they don't match. */
	offset = 0;
	while ((offset < olen) && (offset < nlen)) {
		if (old[offset] != priv->snapshot_text[offset]) {
			break;
		}
		offset++;
	}

	/* At least one of them had better have more data, right? */
	if ((offset < olen) || (offset < nlen)) {
		/* Back up both end points until we find the *last* point
		 * where they differed. */
		while ((olen > offset) && (nlen > offset)) {
			if (old[olen - 1] !=
			    priv->snapshot_text[nlen - 1]) {
				break;
			}
			olen--;
			nlen--;
		}
		/* At least one of them has to have text the other
		 * doesn't. */
		g_assert((nlen > offset) || (olen > offset));
		/* Now emit a deleted signal for text that was in the old
		 * string but isn't in the new one... */
		emit_text_changed_delete(G_OBJECT(data),
					 old,
					 offset, olen - offset);
		/* .. and an inserted signal for text that wasn't in the old
		 * string but is in the new one. */
		emit_text_changed_insert(G_OBJECT(data),
					 priv->snapshot_text,
					 offset, nlen - offset);
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
	struct vte_char_attributes attr;
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
			if (priv->snapshot_text) {
				emit_text_changed_delete(G_OBJECT(data),
							 priv->snapshot_text,
							 0,
							 strlen(priv->snapshot_text));
			}
		}
		priv->snapshot_contents_invalid = TRUE;
		vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(data),
								      NULL);
		/* All of the present text was added. */
		if (priv->snapshot_text != NULL) {
			if (priv->snapshot_text) {
				emit_text_changed_insert(G_OBJECT(data),
							 priv->snapshot_text,
							 0,
							 strlen(priv->snapshot_text));
			}
		}
		return;
	}
	/* Find the start point. */
	delta = 0;
	if (priv->snapshot_attributes != NULL) {
		if (priv->snapshot_attributes->len > 0) {
			attr = g_array_index(priv->snapshot_attributes,
					     struct vte_char_attributes,
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
					     struct vte_char_attributes,
					     i);
			if (attr.row >= delta + terminal->row_count - howmuch) {
				break;
			}
		}
		if (i < priv->snapshot_attributes->len) {
			/* The rest of the string was deleted -- make a note. */
			emit_text_changed_delete(G_OBJECT(data),
						 priv->snapshot_text,
						 i,
						 priv->snapshot_attributes->len - i);
		}
		/* Refresh.  Note that i is now the length of the data which
		 * we expect to have left over. */
		priv->snapshot_contents_invalid = TRUE;
		vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(data),
								      NULL);
		/* If we now have more text than before, the initial portion
		 * was added. */
		if (priv->snapshot_text != NULL) {
			len = strlen(priv->snapshot_text);
			if (len > i) {
				emit_text_changed_insert(G_OBJECT(data),
							 priv->snapshot_text,
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
					     struct vte_char_attributes,
					     i);
			if (attr.row >= delta + howmuch) {
				break;
			}
		}
		/* That many bytes disappeared -- make a note. */
		emit_text_changed_delete(G_OBJECT(data),
					 priv->snapshot_text,
					 0,
					 i);
		/* Figure out how much text was left, and refresh. */
		i = strlen(priv->snapshot_text + i);
		priv->snapshot_contents_invalid = TRUE;
		vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(data),
								      NULL);
		/* Any newly-added string data is new, so note that it was
		 * inserted. */
		if (priv->snapshot_text != NULL) {
			len = strlen(priv->snapshot_text);
			if (len > i) {
				emit_text_changed_insert(G_OBJECT(data),
							 priv->snapshot_text,
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
							      NULL);
}

/* Handle title changes by resetting the parent object. */
static void
vte_terminal_accessible_title_changed(VteTerminal *terminal, gpointer data)
{
	g_return_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(data));
	g_return_if_fail(VTE_IS_TERMINAL(terminal));
	atk_object_set_description(ATK_OBJECT(data), terminal->window_title);
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

	if (GTK_IS_WIDGET((GTK_WIDGET(terminal))->parent)) {
		parent = gtk_widget_get_accessible((GTK_WIDGET(terminal))->parent);
		atk_object_set_parent(ATK_OBJECT(access), parent);
	}

	atk_object_set_name(ATK_OBJECT(access), "Terminal");
	atk_object_set_description(ATK_OBJECT(access),
				   terminal->window_title ?
				   terminal->window_title :
				   "");
	
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

	g_return_val_if_fail((start_offset >= 0) && (end_offset >= -1),
			     g_strdup(""));

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL);

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
		end = strlen(priv->snapshot_text);
	} else {
		/* Map the stopping point. */
		end = g_array_index(priv->snapshot_characters, int, end_offset);
	}
	return g_strndup(priv->snapshot_text + start, end - start);
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
							      NULL);

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
							      NULL);
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
							      NULL);
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
							      NULL);
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
							      NULL);

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
							      NULL);

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
							      NULL);
	/* FIXME */
	return NULL;
}

static AtkAttributeSet *
vte_terminal_accessible_get_default_attributes(AtkText *text)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL);
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
							      NULL);
	/* FIXME */
}

static gint
vte_terminal_accessible_get_character_count(AtkText *text)
{
	VteTerminalAccessiblePrivate *priv;

	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL);

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
							      NULL);
	/* FIXME */
	return 0;
}

static gint
vte_terminal_accessible_get_n_selections(AtkText *text)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), 0);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL);
	/* FIXME? */
	return 0;
}

static gchar *
vte_terminal_accessible_get_selection(AtkText *text, gint selection_number,
				      gint *start_offset, gint *end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), NULL);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL);
	/* FIXME? */
	return NULL;
}

static gboolean
vte_terminal_accessible_add_selection(AtkText *text,
				      gint start_offset, gint end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL);
	/* FIXME? */
	return FALSE;
}

static gboolean
vte_terminal_accessible_remove_selection(AtkText *text,
					 gint selection_number)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL);
	/* FIXME? */
	return FALSE;
}

static gboolean
vte_terminal_accessible_set_selection(AtkText *text, gint selection_number,
				      gint start_offset, gint end_offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL);
	/* FIXME? */
	return FALSE;
}

static gboolean
vte_terminal_accessible_set_caret_offset(AtkText *text, gint offset)
{
	g_return_val_if_fail(VTE_IS_TERMINAL_ACCESSIBLE(text), FALSE);
	vte_terminal_accessible_update_private_data_if_needed(ATK_OBJECT(text),
							      NULL);
	/* Whoa, very not allowed. */
	return FALSE;
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
