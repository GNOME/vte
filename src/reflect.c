/*
 * Copyright (C) 2003 Red Hat, Inc.
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

#ident "$Id$"
#include "../config.h"
#include <sys/types.h>
#include <string.h>
#include <gtk/gtk.h>
#include <atk/atk.h>
#include "vte.h"

static GArray *contents = NULL;

static void
update_contents(AtkObject *obj, GtkWidget *widget)
{
	int caret, i;
	GString *s;
	GtkTextBuffer *buffer;
	s = g_string_new("");
	for (i = 0; i < contents->len; i++) {
		s = g_string_append_unichar(s,
					    g_array_index(contents,
						   	  gunichar,
							  i));
	}
	caret = atk_text_get_caret_offset(ATK_TEXT(obj));
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
	gtk_text_buffer_set_text(buffer, s->str, s->len);
	g_string_free(s, TRUE);
}

static void
text_changed_insert(AtkObject *obj, gint offset, gint length, gpointer data)
{
	char *inserted, *p;
	gunichar c;
	int i;

	inserted = atk_text_get_text(ATK_TEXT(obj), offset, offset + length);

	if (!g_utf8_validate(inserted, -1, NULL)) {
		g_free(inserted);
		g_error("UTF-8 validation error");
		return;
	}

	for (p = inserted, i = 0;
	     (*p != '\0') && (i < length);
	     p = g_utf8_next_char(p)) {
		c = g_utf8_get_char(p);
		g_array_insert_val(contents, offset + i, c);
		i++;
	}

	if (*p != '\0') {
		g_warning("%d unprocessed bytes\n", strlen(p));
	}
	if (i != length) {
		g_warning("%d unprocessed insertions\n", length - i);
	}
	g_assert((i == length) && (*p == '\0'));

	g_free(inserted);

	update_contents(obj, GTK_WIDGET(data));
}

static void
text_changed_delete(AtkObject *obj, gint offset, gint length, gpointer data)
{
	int i;
	for (i = offset + length - 1; i >= offset; i--) {
		contents = g_array_remove_index(contents, i);
	}
	update_contents(obj, GTK_WIDGET(data));
}

int
main(int argc, char **argv)
{
	GtkWidget *text, *texttable, *terminal, *termbox, *termbox2, *pane;
	GtkWidget *texthscroll, *textvscroll, *termvscroll, *window;
	AtkObject *obj;
	gtk_init(&argc, &argv);

	contents = g_array_new(TRUE, FALSE, sizeof(gunichar));

	terminal = vte_terminal_new();
	termvscroll = gtk_vscrollbar_new((VTE_TERMINAL(terminal))->adjustment);
	termbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(termbox), terminal, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(termbox), termvscroll, FALSE, FALSE, 0);
	gtk_widget_show(terminal);
	gtk_widget_show(termvscroll);
	termbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(termbox2), termbox, FALSE, FALSE, 0);
	gtk_widget_show(termbox);

	text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	texthscroll = gtk_hscrollbar_new((GTK_TEXT_VIEW(text))->hadjustment);
	textvscroll = gtk_vscrollbar_new((GTK_TEXT_VIEW(text))->vadjustment);
	texttable = gtk_table_new(2, 2, FALSE);
	gtk_table_attach(GTK_TABLE(texttable), text, 0, 1, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(texttable), textvscroll, 1, 2, 0, 1,
			 GTK_SHRINK, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(texttable), texthscroll, 0, 1, 1, 2,
			 GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_widget_show(text);
	gtk_widget_show(textvscroll);
	gtk_widget_show(texthscroll);

	pane = gtk_vpaned_new();
	gtk_paned_add1(GTK_PANED(pane), termbox2);
	gtk_paned_add2(GTK_PANED(pane), texttable);
	gtk_widget_show(termbox2);
	gtk_widget_show(texttable);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(gtk_main_quit), NULL);
	gtk_container_add(GTK_CONTAINER(window), pane);
	gtk_widget_show(pane);

	vte_terminal_fork_command(VTE_TERMINAL(terminal),
				  getenv("SHELL") ? getenv("SHELL") : "/bin/sh",
				  NULL,
				  NULL,
				  g_get_home_dir() ? g_get_home_dir() : NULL,
				  FALSE,
				  FALSE,
				  FALSE);

	g_signal_connect(G_OBJECT(terminal), "eof",
			 G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(terminal), "child-exited",
			 G_CALLBACK(gtk_main_quit), NULL);
	obj = gtk_widget_get_accessible(terminal);
	g_signal_connect(G_OBJECT(obj), "text_changed::insert",
			 G_CALLBACK(text_changed_insert), text);
	g_signal_connect(G_OBJECT(obj), "text_changed::delete",
			 G_CALLBACK(text_changed_delete), text);

	gtk_window_present(GTK_WINDOW(window));

	gtk_main();

	g_array_free(contents, TRUE);

	return 0;
}
