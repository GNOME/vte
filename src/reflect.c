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

#include "../config.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <atk/atk.h>
#ifdef USE_VTE
#include "vte.h"
#endif
#ifdef USE_ZVT
#ifdef HAVE_ZVT
#include <libzvt/libzvt.h>
#endif
#endif

static GArray *contents = NULL;

#ifdef USE_TEXT_VIEW
/*
 * Implementation for a TextView widget.
 */
static void
terminal_init_text_view(GtkWidget **widget)
{
	*widget = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(*widget), TRUE);
}
static void
terminal_shell_text_view(GtkWidget *widget)
{
	/* no-op */
}
static GtkAdjustment *
terminal_adjustment_text_view(GtkWidget *terminal)
{
	return (GTK_TEXT_VIEW(terminal))->vadjustment;
}
#endif
#ifdef USE_VTE
/*
 * Implementation for a VteTerminal widget.
 */
static void
terminal_init_vte(GtkWidget **terminal)
{
	*terminal = vte_terminal_new();
	g_signal_connect(G_OBJECT(*terminal), "eof",
			 G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(*terminal), "child-exited",
			 G_CALLBACK(gtk_main_quit), NULL);
}
static void
terminal_shell_vte(GtkWidget *terminal)
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
static GtkAdjustment *
terminal_adjustment_vte(GtkWidget *terminal)
{
	return (VTE_TERMINAL(terminal))->adjustment;
}
#endif
#ifdef USE_ZVT
#ifdef HAVE_ZVT
/*
 * Implementation for a ZvtTerm widget.
 */
static void
terminal_hint_zvt(GtkWidget *widget, gpointer data)
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
terminal_init_zvt(GtkWidget **terminal)
{
	*terminal = zvt_term_new();
	g_signal_connect_after(G_OBJECT(*terminal), "realize",
			       G_CALLBACK(terminal_hint_zvt), NULL);
}
static void
terminal_shell_zvt(GtkWidget *terminal)
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
static GtkAdjustment *
terminal_adjustment_zvt(GtkWidget *terminal)
{
	return (ZVT_TERM(terminal))->adjustment;
}
#else
/*
 * Implementation for broken setups.
 */
static void
terminal_init_broken(GtkWidget **terminal)
{
	g_error("libzvt not found at compile-time");
	_exit(1);
}
static void
terminal_shell_broken(GtkWidget *terminal)
{
	g_error("libzvt not found at compile-time");
	_exit(1);
}
static GtkAdjustment *
terminal_adjustment_broken(GtkWidget *terminal)
{
	g_error("libzvt not found at compile-time");
	_exit(1);
}
#endif
#endif

/*
 * Update the contents of the widget with the data from our contents array.
 */
static void
update_contents(AtkObject *obj, GtkWidget *widget)
{
	int caret, i;
	GString *s;

	caret = atk_text_get_caret_offset(ATK_TEXT(obj));
	s = g_string_new(NULL);
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
	if (GTK_IS_LABEL(widget)) {
		gtk_label_set_text(GTK_LABEL(widget), s->str);
		gtk_label_set_selectable(GTK_LABEL(widget),
					 atk_text_get_n_selections(ATK_TEXT(obj)) > 0);
		if (gtk_label_get_selectable(GTK_LABEL(widget))) {
			int selection_start, selection_end;
			atk_text_get_selection(ATK_TEXT(obj), 0,
					       &selection_start,
					       &selection_end);
			gtk_label_select_region(GTK_LABEL(widget),
						selection_start, selection_end);
		}
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

	p = inserted;
	i = 0;
	while (i < length) {
		c = g_utf8_get_char(p);
		if (offset + i >= contents->len) {
			g_array_append_val(contents, c);
		} else {
			g_array_insert_val(contents, offset + i, c);
		}
		i++;
		p = g_utf8_next_char(p);
	}

#ifdef VTE_DEBUG
	if ((getenv("REFLECT_VERBOSE") != NULL) &&
	    (atol(getenv("REFLECT_VERBOSE")) != 0)) {
		fprintf(stderr, "Inserted %d chars ('%.*s') at %d,",
			length, (int)(p - inserted), inserted, offset);
		fprintf(stderr, " buffer contains %d characters.\n",
			contents->len);
	}
#endif

	g_free(inserted);

	update_contents(obj, GTK_WIDGET(data));
}

/* Handle deleted text by removing the text from our gunichar array. */
static void
text_changed_delete(AtkObject *obj, gint offset, gint length, gpointer data)
{
	int i;
	for (i = offset + length - 1; i >= offset; i--) {
		if (i > contents->len - 1) {
			g_warning("Invalid character %d was deleted.\n", i);
		}
		g_array_remove_index(contents, i);
	}
#ifdef VTE_DEBUG
	if ((getenv("REFLECT_VERBOSE") != NULL) &&
	    (atol(getenv("REFLECT_VERBOSE")) != 0)) {
		fprintf(stderr, "Deleted %d chars at %d.\n", length, offset);
	}
#endif
	update_contents(obj, GTK_WIDGET(data));
}

static void
text_caret_moved(AtkObject *obj, gint offset, gpointer data)
{
	update_contents(obj, GTK_WIDGET(data));
}

static void
text_selection_changed(AtkObject *obj, gpointer data)
{
	update_contents(obj, GTK_WIDGET(data));
}

/* Wrapper versions. */
static void
terminal_init(GtkWidget **terminal)
{
	*terminal = NULL;
#ifdef USE_ZVT
#ifdef HAVE_ZVT
	terminal_init_zvt(terminal);
	return;
#else
	terminal_init_broken(terminal);
	return;
#endif
#endif
#ifdef USE_TEXT_VIEW
	terminal_init_text_view(terminal);
	return;
#endif
#ifdef USE_VTE
	terminal_init_vte(terminal);
	return;
#endif
	g_assert_not_reached();
}
static void
terminal_shell(GtkWidget *terminal)
{
#ifdef USE_ZVT
#ifdef HAVE_ZVT
	terminal_shell_zvt(terminal);
	return;
#else
	terminal_shell_broken(terminal);
	return;
#endif
#endif
#ifdef USE_TEXT_VIEW
	terminal_shell_text_view(terminal);
	return;
#endif
#ifdef USE_VTE
	terminal_shell_vte(terminal);
	return;
#endif
	g_assert_not_reached();
}
static GtkAdjustment *
terminal_adjustment(GtkWidget *terminal)
{
#ifdef USE_ZVT
#ifdef HAVE_ZVT
	return terminal_adjustment_zvt(terminal);
#else
	return terminal_adjustment_broken(terminal);
#endif
#endif
#ifdef USE_TEXT_VIEW
	return terminal_adjustment_text_view(terminal);
#endif
#ifdef USE_VTE
	return terminal_adjustment_vte(terminal);
#endif
	g_assert_not_reached();
}

int
main(int argc, char **argv)
{
	GtkWidget *label, *terminal, *tophalf, *pane, *window, *scrollbar, *sw;
	AtkObject *obj;
	char *text, *p;
	gunichar c;
	gint count;

	gtk_init(&argc, &argv);

	contents = g_array_new(TRUE, FALSE, sizeof(gunichar));

	terminal_init(&terminal);

#ifdef USE_TEXT_VIEW
	tophalf = gtk_scrolled_window_new(NULL, terminal_adjustment(terminal));
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tophalf),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	scrollbar = NULL;
	gtk_container_add(GTK_CONTAINER(tophalf), terminal);
#else
	tophalf = gtk_hbox_new(FALSE, 0);

	gtk_box_pack_start(GTK_BOX(tophalf), terminal, TRUE, TRUE, 0);
	gtk_widget_show(terminal);

	scrollbar = gtk_vscrollbar_new(terminal_adjustment(terminal));
	gtk_box_pack_start(GTK_BOX(tophalf), scrollbar, FALSE, TRUE, 0);
	gtk_widget_show(scrollbar);
#endif
	gtk_widget_show(terminal);

	label = gtk_label_new("");
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), label);
	gtk_widget_show(label);

	pane = gtk_vpaned_new();
	gtk_paned_pack1(GTK_PANED(pane), tophalf, TRUE, FALSE);
	gtk_paned_pack2(GTK_PANED(pane), sw, TRUE, FALSE);
	gtk_widget_show(tophalf);
	gtk_widget_show(sw);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(gtk_main_quit), NULL);
	gtk_container_add(GTK_CONTAINER(window), pane);
	gtk_widget_show(pane);

	obj = gtk_widget_get_accessible(terminal);
	g_assert(obj != NULL);
	g_signal_connect(G_OBJECT(obj), "text-changed::insert",
			 G_CALLBACK(text_changed_insert), label);
	g_signal_connect(G_OBJECT(obj), "text-changed::delete",
			 G_CALLBACK(text_changed_delete), label);
	g_signal_connect(G_OBJECT(obj), "text-caret-moved",
			 G_CALLBACK(text_caret_moved), label);
	g_signal_connect(G_OBJECT(obj), "text-selection-changed",
			 G_CALLBACK(text_selection_changed), label);

	count = atk_text_get_character_count(ATK_TEXT(obj));
	if (count > 0) {
		text = atk_text_get_text(ATK_TEXT(obj), 0, count);
		if (text != NULL) {
			for (p = text;
			     contents->len < count;
			     p = g_utf8_next_char(p)) {
				c = g_utf8_get_char(p);
				g_array_append_val(contents, c);
			}
			g_free(text);
		}
	}
	terminal_shell(terminal);

	gtk_window_set_default_size(GTK_WINDOW(window), 600, 450);
	gtk_widget_show(window);

	update_contents(obj, terminal);

	gtk_main();

	g_array_free(contents, TRUE);
	contents = NULL;

	return 0;
}
