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

#ident "$Id$"
#include "../config.h"
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <glib-object.h>
#ifdef HAVE_XFT2
#include <fontconfig/fontconfig.h>
#endif
#include "debug.h"
#include "vte.h"

#define DINGUS1 "(((news|telnet|nttp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?"
#define DINGUS2 "(((news|telnet|nttp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]"

static void
window_title_changed(GtkWidget *widget, gpointer win)
{
	GtkWindow *window;

	g_return_if_fail(VTE_TERMINAL(widget));
	g_return_if_fail(GTK_IS_WINDOW(win));
	g_return_if_fail(VTE_TERMINAL (widget)->window_title != NULL);
	window = GTK_WINDOW(win);

	gtk_window_set_title(window, VTE_TERMINAL (widget)->window_title);
}

static void
char_size_changed(GtkWidget *widget, guint width, guint height, gpointer data)
{
	VteTerminal *terminal;
	GtkWindow *window;
	GdkGeometry geometry;
	int xpad, ypad;

	g_return_if_fail(GTK_IS_WINDOW(data));
	g_return_if_fail(VTE_IS_TERMINAL(widget));

	terminal = VTE_TERMINAL(widget);
	window = GTK_WINDOW(data);

	vte_terminal_get_padding(terminal, &xpad, &ypad);

	geometry.width_inc = terminal->char_width;
	geometry.height_inc = terminal->char_height;
	geometry.base_width = xpad;
	geometry.base_height = ypad;
	geometry.min_width = xpad + terminal->char_width * 2;
	geometry.min_height = ypad + terminal->char_height * 2;

	gtk_window_set_geometry_hints(window, widget, &geometry,
				      GDK_HINT_RESIZE_INC |
				      GDK_HINT_BASE_SIZE |
				      GDK_HINT_MIN_SIZE);
}

static void
destroy_and_quit(GtkWidget *widget, gpointer data)
{
	if (GTK_IS_CONTAINER(data)) {
		gtk_container_remove(GTK_CONTAINER(data), widget);
	} else {
		gtk_widget_destroy(widget);
	}
	gtk_main_quit();
}
static void
destroy_and_quit_eof(GtkWidget *widget, gpointer data)
{
	g_print("Detected EOF.\n");
	destroy_and_quit(widget, data);
}
static void
destroy_and_quit_exited(GtkWidget *widget, gpointer data)
{
	g_print("Detected child exit.\n");
	destroy_and_quit(widget, data);
}

static void
status_line_changed(GtkWidget *widget, gpointer data)
{
	g_print("Status = `%s'.\n",
		vte_terminal_get_status_line(VTE_TERMINAL(widget)));
}

static int
button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	VteTerminal *terminal;
	char *match;
	int tag;
	gint xpad, ypad;
	switch (event->button) {
	case 3:
		terminal = VTE_TERMINAL(widget);
		vte_terminal_get_padding(terminal, &xpad, &ypad);
		match = vte_terminal_match_check(terminal,
						 (event->x - ypad) /
						 terminal->char_width,
						 (event->y - ypad) /
						 terminal->char_height,
						 &tag);
		if (match != NULL) {
			g_print("Matched `%s' (%d).\n", match, tag);
			g_free(match);
			if (GPOINTER_TO_INT(data) != 0) {
				vte_terminal_match_remove(terminal, tag);
			}
		}
		break;
	case 1:
	case 2:
	default:
		break;
	}
	return FALSE;
}

static void
iconify_window(GtkWidget *widget, gpointer data)
{
	if (GTK_IS_WIDGET(data)) {
		if ((GTK_WIDGET(data))->window) {
			gdk_window_iconify((GTK_WIDGET(data))->window);
		}
	}
}

static void
deiconify_window(GtkWidget *widget, gpointer data)
{
	if (GTK_IS_WIDGET(data)) {
		if ((GTK_WIDGET(data))->window) {
			gdk_window_deiconify((GTK_WIDGET(data))->window);
		}
	}
}

static void
raise_window(GtkWidget *widget, gpointer data)
{
	if (GTK_IS_WIDGET(data)) {
		if ((GTK_WIDGET(data))->window) {
			gdk_window_raise((GTK_WIDGET(data))->window);
		}
	}
}

static void
lower_window(GtkWidget *widget, gpointer data)
{
	if (GTK_IS_WIDGET(data)) {
		if ((GTK_WIDGET(data))->window) {
			gdk_window_lower((GTK_WIDGET(data))->window);
		}
	}
}

static void
maximize_window(GtkWidget *widget, gpointer data)
{
	if (GTK_IS_WIDGET(data)) {
		if ((GTK_WIDGET(data))->window) {
			gdk_window_maximize((GTK_WIDGET(data))->window);
		}
	}
}

static void
restore_window(GtkWidget *widget, gpointer data)
{
	if (GTK_IS_WIDGET(data)) {
		if ((GTK_WIDGET(data))->window) {
			gdk_window_unmaximize((GTK_WIDGET(data))->window);
		}
	}
}

static void
refresh_window(GtkWidget *widget, gpointer data)
{
	GdkRectangle rect;
	if (GTK_IS_WIDGET(data)) {
		if ((GTK_WIDGET(data))->window) {
			rect.x = rect.y = 0;
			rect.width = (GTK_WIDGET(data))->allocation.width;
			rect.height = (GTK_WIDGET(data))->allocation.height;
			gdk_window_invalidate_rect((GTK_WIDGET(data))->window,
						   &rect, TRUE);
		}
	}
}

static void
resize_window(GtkWidget *widget, guint width, guint height, gpointer data)
{
	VteTerminal *terminal;
	gint owidth, oheight;
	if ((GTK_IS_WINDOW(data)) && (width >= 2) && (height >= 2)) {
		terminal = VTE_TERMINAL(widget);
		/* Take into account padding and border overhead. */
		gtk_window_get_size(GTK_WINDOW(data), &owidth, &oheight);
		owidth -= terminal->char_width * terminal->column_count;
		oheight -= terminal->char_height * terminal->row_count;
		gtk_window_resize(GTK_WINDOW(data),
				  width + owidth, height + oheight);
	}
}

static void
move_window(GtkWidget *widget, guint x, guint y, gpointer data)
{
	if (GTK_IS_WIDGET(data)) {
		if ((GTK_WIDGET(data))->window) {
			gdk_window_move((GTK_WIDGET(data))->window, x, y);
		}
	}
}

static void
adjust_font_size(GtkWidget *widget, gpointer data, gint howmuch)
{
	VteTerminal *terminal;
	PangoFontDescription *desired;
	gint newsize;
	gint columns, rows, owidth, oheight;

	/* Read the screen dimensions in cells. */
	terminal = VTE_TERMINAL(widget);
	columns = terminal->column_count;
	rows = terminal->row_count;

	/* Take into account padding and border overhead. */
	gtk_window_get_size(GTK_WINDOW(data), &owidth, &oheight);
	owidth -= terminal->char_width * terminal->column_count;
	oheight -= terminal->char_height * terminal->row_count;

	/* Calculate the new font size. */
	desired = pango_font_description_copy(vte_terminal_get_font(terminal));
	newsize = pango_font_description_get_size(desired) / PANGO_SCALE;
	newsize += howmuch;
	pango_font_description_set_size(desired,
					CLAMP(newsize, 4, 144) * PANGO_SCALE);

	/* Change the font, then resize the window so that we have the same
	 * number of rows and columns. */
	vte_terminal_set_font(terminal, desired);
	gtk_window_resize(GTK_WINDOW(data),
			  columns * terminal->char_width + owidth,
			  rows * terminal->char_height + oheight);

	pango_font_description_free(desired);
}

static void
increase_font_size(GtkWidget *widget, gpointer data)
{
	adjust_font_size(widget, data, 1);
}

static void
decrease_font_size(GtkWidget *widget, gpointer data)
{
	adjust_font_size(widget, data, -1);
}

static void
mess_with_fontconfig(void)
{
#ifdef HAVE_XFT2
	/* Is this even a good idea?  Probably not, since this doesn't expose
	 * these fonts to the gnome-font-properties capplet. */
	FcInit();
	FcConfigAppFontAddDir(NULL, DATADIR "/" PACKAGE "/fonts");
#endif
}

int
main(int argc, char **argv)
{
	GtkWidget *window, *hbox, *scrollbar, *widget;
	char *env_add[] = {"FOO=BAR", "BOO=BIZ", NULL};
	const char *background = NULL;
	gboolean transparent = FALSE, audible = TRUE, blink = TRUE,
		 debug = FALSE, dingus = FALSE, geometry = TRUE, dbuffer = TRUE;
	long lines = 100;
	const char *message = "Launching interactive shell...\r\n";
	const char *font = NULL;
	const char *terminal = NULL;
	const char *command = NULL;
	const char *working_directory = NULL;
	char **argv2;
	int opt;
	int i, j;
	GList *args = NULL;
	GdkColor fore, back, tint;
	const char *usage = "Usage: %s "
			    "[ [-B image] | [-T] ] "
			    "[-D] "
			    "[-2] "
			    "[-a] "
			    "[-b] "
			    "[-c command] "
			    "[-d] "
			    "[-f font] "
			    "[-g] "
			    "[-h] "
			    "[-n] "
			    "[-t terminaltype]\n";
	back.red = back.green = back.blue = 0xffff;
	fore.red = fore.green = fore.blue = 0x0000;
	tint.red = tint.green = tint.blue = 0;

	/* Have to do this early. */
	if (getenv("VTE_PROFILE_MEMORY")) {
		if (atol(getenv("VTE_PROFILE_MEMORY")) != 0) {
			g_mem_set_vtable(glib_mem_profiler_table);
		}
	}

	/* Pull out long options for GTK+. */
	for (i = j = 1; i < argc; i++) {
		if (g_ascii_strncasecmp("--", argv[i], 2) == 0) {
			args = g_list_append(args, argv[i]);
			for (j = i; j < argc; j++) {
				argv[j] = argv[j + 1];
			}
			argc--;
			i--;
		}
	}
	argv2 = g_malloc0(sizeof(char*) * (g_list_length(args) + 2));
	argv2[0] = argv[0];
	for (i = 1; i <= g_list_length(args); i++) {
		argv2[i] = (char*) g_list_nth(args, i - 1);
	}
	argv2[i] = NULL;
	g_assert(i < (g_list_length(args) + 2));
	/* Parse some command-line options. */
	while ((opt = getopt(argc, argv, "B:DT2abc:df:ghn:t:w:")) != -1) {
		switch (opt) {
			case 'B':
				background = optarg;
				break;
			case 'D':
				dingus = TRUE;
				break;
			case 'T':
				transparent = TRUE;
				break;
			case '2':
				dbuffer = !dbuffer;
				break;
			case 'a':
				audible = !audible;
				break;
			case 'b':
				blink = !blink;
				break;
			case 'c':
				command = optarg;
				break;
			case 'd':
				debug = !debug;
				break;
			case 'f':
				font = optarg;
				break;
			case 'g':
				geometry = !geometry;
				break;
			case 'n':
				lines = atol(optarg);
				if (lines == 0) {
					lines = 100;
				}
				break;
			case 't':
				terminal = optarg;
				break;
			case 'w':
				working_directory = optarg;
				break;
			case 'h':
			default:
				g_print(usage, argv[0]);
				exit(1);
				break;
		}
	}

	gtk_init(&argc, &argv);
	gdk_window_set_debug_updates(debug);

	/* Create a window to hold the scrolling shell, and hook its
	 * delete event to the quit function.. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	/* Create a box to hold everything. */
	hbox = gtk_hbox_new(0, FALSE);
	gtk_container_add(GTK_CONTAINER(window), hbox);

	/* Create the terminal widget and add it to the scrolling shell. */
	widget = vte_terminal_new();
	gtk_widget_set_double_buffered(widget, dbuffer);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

	/* Connect to the "char_size_changed" signal to set geometry hints
	 * whenever the font used by the terminal is changed. */
	if (geometry) {
		char_size_changed(widget, 0, 0, window);
		g_signal_connect(G_OBJECT(widget), "char-size-changed",
				 G_CALLBACK(char_size_changed), window);
	}

	/* Connect to the "window_title_changed" signal to set the main
	 * window's title. */
	g_signal_connect(G_OBJECT(widget), "window-title-changed",
			 G_CALLBACK(window_title_changed), window);

	/* Connect to the "eof" signal to quit when the session ends. */
	g_signal_connect(G_OBJECT(widget), "eof",
			 G_CALLBACK(destroy_and_quit_eof), widget);
	g_signal_connect(G_OBJECT(widget), "child-exited",
			 G_CALLBACK(destroy_and_quit_exited), widget);

	/* Connect to the "status-line-changed" signal. */
	g_signal_connect(G_OBJECT(widget), "status-line-changed",
			 G_CALLBACK(status_line_changed), widget);

	/* Connect to the "button-press" event. */
	g_signal_connect(G_OBJECT(widget), "button-press-event",
			 G_CALLBACK(button_pressed), widget);

	/* Connect to application request signals. */
	g_signal_connect(G_OBJECT(widget), "iconify-window",
			 G_CALLBACK(iconify_window), window);
	g_signal_connect(G_OBJECT(widget), "deiconify-window",
			 G_CALLBACK(deiconify_window), window);
	g_signal_connect(G_OBJECT(widget), "raise-window",
			 G_CALLBACK(raise_window), window);
	g_signal_connect(G_OBJECT(widget), "lower-window",
			 G_CALLBACK(lower_window), window);
	g_signal_connect(G_OBJECT(widget), "maximize-window",
			 G_CALLBACK(maximize_window), window);
	g_signal_connect(G_OBJECT(widget), "restore-window",
			 G_CALLBACK(restore_window), window);
	g_signal_connect(G_OBJECT(widget), "refresh-window",
			 G_CALLBACK(refresh_window), window);
	g_signal_connect(G_OBJECT(widget), "resize-window",
			 G_CALLBACK(resize_window), window);
	g_signal_connect(G_OBJECT(widget), "move-window",
			 G_CALLBACK(move_window), window);

	/* Connect to font tweakage. */
	g_signal_connect(G_OBJECT(widget), "increase-font-size",
			 G_CALLBACK(increase_font_size), window);
	g_signal_connect(G_OBJECT(widget), "decrease-font-size",
			 G_CALLBACK(decrease_font_size), window);

	/* Create the scrollbar for the widget. */
	scrollbar = gtk_vscrollbar_new((VTE_TERMINAL(widget))->adjustment);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);

	/* Set some defaults. */
	vte_terminal_set_audible_bell(VTE_TERMINAL(widget), audible);
	vte_terminal_set_cursor_blinks(VTE_TERMINAL(widget), blink);
	vte_terminal_set_scroll_on_output(VTE_TERMINAL(widget), FALSE);
	vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(widget), TRUE);
	vte_terminal_set_scrollback_lines(VTE_TERMINAL(widget), lines);
	vte_terminal_set_mouse_autohide(VTE_TERMINAL(widget), TRUE);
	if (background != NULL) {
		vte_terminal_set_background_image_file(VTE_TERMINAL(widget),
						       background);
	}
	if (transparent) {
		vte_terminal_set_background_transparent(VTE_TERMINAL(widget),
							TRUE);
	}
	vte_terminal_set_background_tint_color(VTE_TERMINAL(widget), &tint);
	vte_terminal_set_colors(VTE_TERMINAL(widget), &fore, &back, NULL, 0);
	if (terminal != NULL) {
		vte_terminal_set_emulation(VTE_TERMINAL(widget), terminal);
	}

	/* Mess with our fontconfig setup. */
	mess_with_fontconfig();

	/* Set the default font. */
	if (font != NULL) {
		vte_terminal_set_font_from_string(VTE_TERMINAL(widget), font);
	}

	/* Match "abcdefg". */
	vte_terminal_match_add(VTE_TERMINAL(widget), "abcdefg");
	if (dingus) {
		vte_terminal_match_add(VTE_TERMINAL(widget), DINGUS1);
		vte_terminal_match_add(VTE_TERMINAL(widget), DINGUS2);
	}

	/* Launch a shell. */
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		vte_terminal_feed(VTE_TERMINAL(widget), message,
				  strlen(message));
	}
#endif
	vte_terminal_fork_command(VTE_TERMINAL(widget),
				  command, NULL, env_add, working_directory,
				  TRUE, TRUE, TRUE);
	if (command == NULL) {
		vte_terminal_feed_child(VTE_TERMINAL(widget), "pwd\n", -1);
	}

	/* Go for it! */
	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}
