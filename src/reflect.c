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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <atk/atk.h>

static GArray *contents = NULL;

#ifdef USE_ZVT
#ifdef HAVE_ZVT
#include <libzvt/libzvt.h>
static void
terminal_hint(GtkWidget *widget, gpointer data)
{
	ZvtTerm *terminal;
	GtkStyle *style;
	GdkGeometry hints;
	GtkWidget *toplevel;

	terminal = ZVT_TERM(widget);

	toplevel = gtk_widget_get_toplevel(widget);
	g_assert(toplevel != NULL);

	gtk_widget_ensure_style(widget);
	style = widget->style;
	hints.base_width = style->xthickness * 2 + 2;
	hints.base_height = style->ythickness * 2;

	hints.width_inc = terminal->charwidth;
	hints.height_inc = terminal->charheight;
	hints.min_width = hints.base_width + hints.width_inc;
	hints.min_height = hints.base_height + hints.height_inc;

	gtk_window_set_geometry_hints(GTK_WINDOW(toplevel),
				      widget,
				      &hints,
				      GDK_HINT_RESIZE_INC |
				      GDK_HINT_MIN_SIZE |
				      GDK_HINT_BASE_SIZE);
	gtk_widget_queue_resize(widget);
}
static void
terminal_init(GtkWidget **terminal, GtkWidget **vscroll)
{
	*terminal = zvt_term_new();
	*vscroll = gtk_vscrollbar_new((ZVT_TERM(*terminal))->adjustment);
	g_signal_connect_after(G_OBJECT(*terminal), "realize",
			       G_CALLBACK(terminal_hint), NULL);
}
static void
terminal_shell(GtkWidget *terminal)
{
	const char *shell;
	shell = getenv("SHELL") ? getenv("SHELL") : "/bin/sh";
	g_signal_connect(G_OBJECT(terminal), "child-died",
			 G_CALLBACK(gtk_main_quit), NULL);
	if (zvt_term_forkpty(ZVT_TERM(terminal), 0) == 0) {
		execlp(shell, shell, NULL);
		g_assert_not_reached();
	}
}
#else
static void
terminal_init(GtkWidget **terminal, GtkWidget **vscroll)
{
	/* We built the ZVT version, but we don't have ZVT. Bail. */
	g_assert_not_reached();
}
static void
terminal_shell(GtkWidget *terminal)
{
	/* We built the ZVT version, but we don't have ZVT. Bail. */
	g_assert_not_reached();
}
#endif
#else
#include "vte.h"
static void
terminal_init(GtkWidget **terminal, GtkWidget **vscroll)
{
	*terminal = vte_terminal_new();
	*vscroll = gtk_vscrollbar_new((VTE_TERMINAL(*terminal))->adjustment);
	g_signal_connect(G_OBJECT(*terminal), "eof",
			 G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(*terminal), "child-exited",
			 G_CALLBACK(gtk_main_quit), NULL);
}
static void
terminal_shell(GtkWidget *terminal)
{
	vte_terminal_fork_command(VTE_TERMINAL(terminal),
				  getenv("SHELL") ? getenv("SHELL") : "/bin/sh",
				  NULL,
				  NULL,
				  g_get_home_dir() ? g_get_home_dir() : NULL,
				  FALSE,
				  FALSE,
				  FALSE);
}
#endif

/*
 * Update the contents of the widget with the data from our contents array.
 */
static void
update_contents(AtkObject *obj, GtkWidget *widget)
{
	int caret, i;
	GString *s;
	GtkTextBuffer *buffer;
	caret = atk_text_get_caret_offset(ATK_TEXT(obj));
	s = g_string_new("");
	for (i = 0; i < contents->len; i++) {
		if (i == caret) {
			s = g_string_append(s, "[CARET]");
		}
		s = g_string_append_unichar(s,
					    g_array_index(contents,
						   	  gunichar,
							  i));
	}
	if (i == caret) {
		s = g_string_append(s, "[CARET]");
	}
	if (GTK_IS_TEXT_VIEW(widget)) {
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
		gtk_text_buffer_set_text(buffer, s->str, s->len);
	}
	if (GTK_IS_LABEL(widget)) {
		gtk_label_set_text(GTK_LABEL(widget), s->str);
	}
	g_string_free(s, TRUE);
}

/* Handle inserted text by inserting the text into our gunichar array. */
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

/* Handle deleted text by removing the text from our gunichar array. */
static void
text_changed_delete(AtkObject *obj, gint offset, gint length, gpointer data)
{
	int i;
	for (i = offset + length - 1; i >= offset; i--) {
		contents = g_array_remove_index(contents, i);
	}
	update_contents(obj, GTK_WIDGET(data));
}

static void
text_caret_moved(AtkObject *obj, gint offset, gpointer data)
{
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

	terminal_init(&terminal, &termvscroll);

	termbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(termbox), terminal, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(termbox), termvscroll, FALSE, FALSE, 0);
	gtk_widget_show(terminal);
	gtk_widget_show(termvscroll);
	termbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(termbox2), termbox, FALSE, FALSE, 0);
	gtk_widget_show(termbox);

#ifdef USE_GTK_TEXT_VIEW
	text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	texthscroll = gtk_hscrollbar_new((GTK_TEXT_VIEW(text))->hadjustment);
	textvscroll = gtk_vscrollbar_new((GTK_TEXT_VIEW(text))->vadjustment);
#else
	text = gtk_label_new("");
	gtk_label_set_justify(GTK_LABEL(text), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(text), 0, 0);
	texthscroll = NULL;
	textvscroll = NULL;
#endif
	texttable = gtk_table_new((texthscroll != NULL) ? 2 : 1,
				  (textvscroll != NULL) ? 2 : 1,
				  FALSE);
	gtk_table_attach(GTK_TABLE(texttable), text, 0, 1, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show(text);
	if (textvscroll) {
		gtk_table_attach(GTK_TABLE(texttable), textvscroll, 1, 2, 0, 1,
				 GTK_SHRINK, GTK_EXPAND | GTK_FILL, 0, 0);
		gtk_widget_show(textvscroll);
	}
	if (texthscroll) {
		gtk_table_attach(GTK_TABLE(texttable), texthscroll, 0, 1, 1, 2,
				 GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
		gtk_widget_show(texthscroll);
	}

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

#ifdef USE_ZVT
	gtk_widget_set_usize(terminal, 80 * 7, 25 * 13);
#endif
	terminal_shell(terminal);

	obj = gtk_widget_get_accessible(terminal);
	g_signal_connect(G_OBJECT(obj), "text-changed::insert",
			 G_CALLBACK(text_changed_insert), text);
	g_signal_connect(G_OBJECT(obj), "text-changed::delete",
			 G_CALLBACK(text_changed_delete), text);
	g_signal_connect(G_OBJECT(obj), "text-caret-moved",
			 G_CALLBACK(text_caret_moved), text);

	gtk_window_present(GTK_WINDOW(window));

	gtk_main();

	g_array_free(contents, TRUE);

	return 0;
}
