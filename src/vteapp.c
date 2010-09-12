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


#include <config.h>

#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <glib-object.h>
#include "debug.h"

#undef VTE_DISABLE_DEPRECATED
#include "vte.h"

#include <glib/gi18n.h>

#define DINGUS1 "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?"
#define DINGUS2 "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]"

static void
window_title_changed(GtkWidget *widget, gpointer win)
{
	GtkWindow *window;

	g_assert(VTE_TERMINAL(widget));
	g_assert(GTK_IS_WINDOW(win));
	g_assert(VTE_TERMINAL(widget)->window_title != NULL);
	window = GTK_WINDOW(win);

	gtk_window_set_title(window, VTE_TERMINAL(widget)->window_title);
}

static void
icon_title_changed(GtkWidget *widget, gpointer win)
{
	GtkWindow *window;

	g_assert(VTE_TERMINAL(widget));
	g_assert(GTK_IS_WINDOW(win));
	g_assert(VTE_TERMINAL(widget)->icon_title != NULL);
	window = GTK_WINDOW(win);

	g_message("Icon title changed to \"%s\".\n",
		  VTE_TERMINAL(widget)->icon_title);
}

static void
char_size_changed(GtkWidget *widget, guint width, guint height, gpointer data)
{
	VteTerminal *terminal;
	GtkWindow *window;
	GdkGeometry geometry;
        GtkBorder *inner_border;

	g_assert(GTK_IS_WINDOW(data));
	g_assert(VTE_IS_TERMINAL(widget));

	terminal = VTE_TERMINAL(widget);
	window = GTK_WINDOW(data);
	if (!gtk_widget_get_realized (GTK_WIDGET (window)))
		return;

        gtk_widget_style_get (widget, "inner-border", &inner_border, NULL);
	geometry.width_inc = width;
	geometry.height_inc = height;
	geometry.base_width = inner_border ? (inner_border->left + inner_border->right) : 0;
	geometry.base_height = inner_border ? (inner_border->top + inner_border->bottom) : 0;
	geometry.min_width = geometry.base_width + width * 2;
	geometry.min_height = geometry.base_height + height * 2;
        gtk_border_free (inner_border);

	gtk_window_set_geometry_hints(window, widget, &geometry,
				      GDK_HINT_RESIZE_INC |
				      GDK_HINT_BASE_SIZE |
				      GDK_HINT_MIN_SIZE);
}

static void
char_size_realized(GtkWidget *widget, gpointer data)
{
	VteTerminal *terminal;
	GtkWindow *window;
	GdkGeometry geometry;
	guint width, height;
        GtkBorder *inner_border;

	g_assert(GTK_IS_WINDOW(data));
	g_assert(VTE_IS_TERMINAL(widget));

	terminal = VTE_TERMINAL(widget);
	window = GTK_WINDOW(data);
	if (!gtk_widget_get_realized (GTK_WIDGET(window)))
		return;

        gtk_widget_style_get (widget, "inner-border", &inner_border, NULL);
	width = vte_terminal_get_char_width (terminal);
	height = vte_terminal_get_char_height (terminal);
	geometry.width_inc = width;
	geometry.height_inc = height;
	geometry.base_width = inner_border ? (inner_border->left + inner_border->right) : 0;
	geometry.base_height = inner_border ? (inner_border->top + inner_border->bottom) : 0;
	geometry.min_width = geometry.base_width + width * 2;
	geometry.min_height = geometry.base_height + height * 2;
        gtk_border_free (inner_border);

	gtk_window_set_geometry_hints(window, widget, &geometry,
				      GDK_HINT_RESIZE_INC |
				      GDK_HINT_BASE_SIZE |
				      GDK_HINT_MIN_SIZE);
}


static void
destroy_and_quit(VteTerminal *terminal, GtkWidget *window)
{
	const char *output_file = g_object_get_data (G_OBJECT (terminal), "output_file");

	if (output_file) {
		GFile *file;
		GOutputStream *stream;
		GError *error = NULL;

		file = g_file_new_for_commandline_arg (output_file);
		stream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));

		if (stream) {
			vte_terminal_write_contents (terminal, stream,
						     VTE_TERMINAL_WRITE_DEFAULT,
						     NULL, &error);
			g_object_unref (stream);
		}

		if (error) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
		}

		g_object_unref (file);
	}

	gtk_widget_destroy (window);
	gtk_main_quit ();
}
static void
delete_event(GtkWidget *window, GdkEvent *event, gpointer terminal)
{
	destroy_and_quit(VTE_TERMINAL (terminal), window);
}
static void
child_exited(GtkWidget *terminal, gpointer window)
{
	_vte_debug_print(VTE_DEBUG_MISC, "Child exited with status %x\n",
			 vte_terminal_get_child_exit_status (VTE_TERMINAL (terminal)));
	destroy_and_quit(VTE_TERMINAL (terminal), GTK_WIDGET (window));
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
        GtkBorder *inner_border;
        int char_width, char_height;

	switch (event->button) {
	case 3:
		terminal = VTE_TERMINAL(widget);

                gtk_widget_style_get (widget, "inner-border", &inner_border, NULL);
                char_width = vte_terminal_get_char_width (terminal);
                char_height = vte_terminal_get_char_height (terminal);
		match = vte_terminal_match_check(terminal,
						 (event->x - (inner_border ? inner_border->left : 0)) / char_width,
						 (event->y - (inner_border ? inner_border->top : 0)) / char_height,
						 &tag);
                gtk_border_free (inner_border);
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
	gtk_window_iconify(data);
}

static void
deiconify_window(GtkWidget *widget, gpointer data)
{
	gtk_window_deiconify(data);
}

static void
raise_window(GtkWidget *widget, gpointer data)
{
	GdkWindow *window;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(GTK_WIDGET(data));
		if (window) {
			gdk_window_raise(window);
		}
	}
}

static void
lower_window(GtkWidget *widget, gpointer data)
{
	GdkWindow *window;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(GTK_WIDGET(data));
		if (window) {
			gdk_window_lower(window);
		}
	}
}

static void
maximize_window(GtkWidget *widget, gpointer data)
{
	GdkWindow *window;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(GTK_WIDGET(data));
		if (window) {
			gdk_window_maximize(window);
		}
	}
}

static void
restore_window(GtkWidget *widget, gpointer data)
{
	GdkWindow *window;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(GTK_WIDGET(data));
		if (window) {
			gdk_window_unmaximize(window);
		}
	}
}

static void
refresh_window(GtkWidget *widget, gpointer data)
{
	GdkWindow *window;
	GtkAllocation allocation;
	GdkRectangle rect;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(widget);
		if (window) {
			gtk_widget_get_allocation(widget, &allocation);
			rect.x = rect.y = 0;
			rect.width = allocation.width;
			rect.height = allocation.height;
			gdk_window_invalidate_rect(window, &rect, TRUE);
		}
	}
}

static void
resize_window(GtkWidget *widget, guint width, guint height, gpointer data)
{
	VteTerminal *terminal;

	if ((GTK_IS_WINDOW(data)) && (width >= 2) && (height >= 2)) {
                gint owidth, oheight, char_width, char_height, column_count, row_count;
                GtkBorder *inner_border;

		terminal = VTE_TERMINAL(widget);

		gtk_window_get_size(GTK_WINDOW(data), &owidth, &oheight);

		/* Take into account border overhead. */
                char_width = vte_terminal_get_char_width (terminal);
                char_height = vte_terminal_get_char_height (terminal);
                column_count = vte_terminal_get_column_count (terminal);
                row_count = vte_terminal_get_row_count (terminal);
                gtk_widget_style_get (widget, "inner-border", &inner_border, NULL);

                owidth -= char_width * column_count;
                oheight -= char_height * row_count;
                if (inner_border != NULL) {
                        owidth -= inner_border->left + inner_border->right;
                        oheight -= inner_border->top + inner_border->bottom;
                }
		gtk_window_resize(GTK_WINDOW(data),
				  width + owidth, height + oheight);
                gtk_border_free (inner_border);
	}
}

static void
move_window(GtkWidget *widget, guint x, guint y, gpointer data)
{
	GdkWindow *window;

	if (GTK_IS_WIDGET(data)) {
		window = gtk_widget_get_window(GTK_WIDGET(data));
		if (window) {
			gdk_window_move(window, x, y);
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

static gboolean
read_and_feed(GIOChannel *source, GIOCondition condition, gpointer data)
{
	char buf[2048];
	gsize size;
	GIOStatus status;
	g_assert(VTE_IS_TERMINAL(data));
	status = g_io_channel_read_chars(source, buf, sizeof(buf),
					 &size, NULL);
	if ((status == G_IO_STATUS_NORMAL) && (size > 0)) {
		vte_terminal_feed(VTE_TERMINAL(data), buf, size);
		return TRUE;
	}
	return FALSE;
}

static void
disconnect_watch(GtkWidget *widget, gpointer data)
{
	g_source_remove(GPOINTER_TO_INT(data));
}

static void
clipboard_get(GtkClipboard *clipboard, GtkSelectionData *selection_data,
	      guint info, gpointer owner)
{
	/* No-op. */
	return;
}

static void
take_xconsole_ownership(GtkWidget *widget, gpointer data)
{
	char *name, hostname[255];
	GdkAtom atom;
	GtkClipboard *clipboard;
	const GtkTargetEntry targets[] = {
		{"UTF8_STRING", 0, 0},
		{"COMPOUND_TEXT", 0, 0},
		{"TEXT", 0, 0},
		{"STRING", 0, 0},
	};

	memset(hostname, '\0', sizeof(hostname));
	gethostname(hostname, sizeof(hostname) - 1);

	name = g_strdup_printf("MIT_CONSOLE_%s", hostname);
	atom = gdk_atom_intern(name, FALSE);
	clipboard = gtk_clipboard_get_for_display(gtk_widget_get_display(widget),
						  atom);
	g_free(name);

	gtk_clipboard_set_with_owner(clipboard,
				     targets,
				     G_N_ELEMENTS(targets),
				     clipboard_get,
				     (GtkClipboardClearFunc)gtk_main_quit,
				     G_OBJECT(widget));
}

static void
add_weak_pointer(GObject *object, GtkWidget **target)
{
	g_object_add_weak_pointer(object, (gpointer*)target);
}

static void
terminal_notify_cb(GObject *object,
                   GParamSpec *pspec,
                   gpointer user_data)
{
  GValue value = { 0, };
  char *value_string;

  if (!pspec ||
      pspec->owner_type != VTE_TYPE_TERMINAL)
    return;


  g_value_init(&value, pspec->value_type);
  g_object_get_property(object, pspec->name, &value);
  value_string = g_strdup_value_contents(&value);
  g_print("NOTIFY property \"%s\" value '%s'\n", pspec->name, value_string);
  g_free(value_string);
  g_value_unset(&value);
}

static void
child_exit_cb(VteTerminal *terminal,
                 gpointer user_data)
{
}

static int
parse_enum(GType type,
           const char *string)
{
  GEnumClass *enum_klass;
  const GEnumValue *enum_value;
  int value = 0;

  enum_klass = (GEnumClass*)g_type_class_ref(type);
  enum_value = g_enum_get_value_by_nick(enum_klass, string);
  if (enum_value)
    value = enum_value->value;
  else
    g_warning("Unknown enum '%s'\n", string);
  g_type_class_unref(enum_klass);

  return value;
}

static guint
parse_flags(GType type,
            const char *string)
{
  GFlagsClass *flags_klass;
  guint value = 0;
  char **flags;
  guint i;

  flags = g_strsplit_set(string, ",|", -1);
  if (flags == NULL)
    return 0;

  flags_klass = (GFlagsClass*)g_type_class_ref(type);
  for (i = 0; flags[i] != NULL; ++i) {
          const GFlagsValue *flags_value;

          flags_value = g_flags_get_value_by_nick(flags_klass, flags[i]);
          if (flags_value)
                  value |= flags_value->value;
          else
                  g_warning("Unknown flag '%s'\n", flags[i]);
  }
  g_type_class_unref(flags_klass);

  return value;
}

int
main(int argc, char **argv)
{
	GdkScreen *screen;
	GdkColormap *colormap;
	GtkWidget *window, *widget,*hbox = NULL, *scrollbar, *scrolled_window = NULL;
	VteTerminal *terminal;
	char *env_add[] = {
#ifdef VTE_DEBUG
		(char *) "FOO=BAR", (char *) "BOO=BIZ",
#endif
		NULL};
	const char *background = NULL;
	gboolean transparent = FALSE, audible = TRUE,
		 debug = FALSE, dingus = FALSE, dbuffer = TRUE,
		 console = FALSE, scroll = FALSE, keep = FALSE,
		 icon_title = FALSE, shell = TRUE, highlight_set = FALSE,
		 cursor_set = FALSE, reverse = FALSE, use_geometry_hints = TRUE,
		 antialias = TRUE, use_scrolled_window = FALSE,
                 show_object_notifications = FALSE;
        char *geometry = NULL;
	gint lines = 100;
	const char *message = "Launching interactive shell...\r\n";
	const char *font = NULL;
	const char *termcap = NULL;
	const char *command = NULL;
	const char *working_directory = NULL;
	const char *output_file = NULL;
        char *pty_flags_string = NULL;
        char *cursor_blink_mode_string = NULL;
        char *cursor_shape_string = NULL;
        char *scrollbar_policy_string = NULL;
	GdkColor fore, back, tint, highlight, cursor;
	const GOptionEntry options[]={
		{
			"antialias", 'A', G_OPTION_FLAG_REVERSE,
			G_OPTION_ARG_NONE, &antialias,
			"Disable the use of anti-aliasing", NULL
		},
		{
			"background", 'B', 0,
			G_OPTION_ARG_FILENAME, &background,
			"Specify a background image", NULL
		},
		{
			"console", 'C', 0,
			G_OPTION_ARG_NONE, &console,
			"Watch /dev/console", NULL
		},
		{
			"dingus", 'D', 0,
			G_OPTION_ARG_NONE, &dingus,
			"Highlight URLs inside the terminal", NULL
		},
		{
			"shell", 'S', G_OPTION_FLAG_REVERSE,
			G_OPTION_ARG_NONE, &shell,
			"Disable spawning a shell inside the terminal", NULL
		},
		{
			"transparent", 'T', 0,
			G_OPTION_ARG_NONE, &transparent,
			"Enable the use of a transparent background", NULL
		},
		{
			"double-buffer", '2', G_OPTION_FLAG_REVERSE,
			G_OPTION_ARG_NONE, &dbuffer,
			"Disable double-buffering", NULL
		},
		{
			"audible", 'a', G_OPTION_FLAG_REVERSE,
			G_OPTION_ARG_NONE, &audible,
			"Use visible, instead of audible, terminal bell",
			NULL
		},
		{
			"command", 'c', 0,
			G_OPTION_ARG_STRING, &command,
			"Execute a command in the terminal", NULL
		},
		{
			"debug", 'd', 0,
			G_OPTION_ARG_NONE, &debug,
			"Enable various debugging checks", NULL
		},
		{
			"font", 'f', 0,
			G_OPTION_ARG_STRING, &font,
			"Specify a font to use", NULL
		},
		{
			"geometry", 'g', 0,
			G_OPTION_ARG_STRING, &geometry,
			"Set the size (in characters) and position", "GEOMETRY"
		},
		{
			"highlight", 'h', 0,
			G_OPTION_ARG_NONE, &highlight_set,
			"Enable distinct highlight color for selection", NULL
		},
		{
			"icon-title", 'i', 0,
			G_OPTION_ARG_NONE, &icon_title,
			"Enable the setting of the icon title", NULL
		},
		{
			"keep", 'k', 0,
			G_OPTION_ARG_NONE, &keep,
			"Live on after the window closes", NULL
		},
		{
			"scrollback-lines", 'n', 0,
			G_OPTION_ARG_INT, &lines,
			"Specify the number of scrollback-lines", NULL
		},
                {
                        "cursor-blink", 0, 0,
                        G_OPTION_ARG_STRING, &cursor_blink_mode_string,
                        "Cursor blink mode (system|on|off)", "MODE"
                },
		{
			"color-cursor", 'r', 0,
			G_OPTION_ARG_NONE, &cursor_set,
			"Enable a colored cursor", NULL
		},
		{
			"cursor-shape", 0, 0,
			G_OPTION_ARG_STRING, &cursor_shape_string,
			"Set cursor shape (block|underline|ibeam)", NULL
		},
		{
			"scroll-background", 's', 0,
			G_OPTION_ARG_NONE, &scroll,
			"Enable a scrolling background", NULL
		},
		{
			"termcap", 't', 0,
			G_OPTION_ARG_STRING, &termcap,
			"Specify the terminal emulation to use", NULL
		},
		{
			"working-directory", 'w', 0,
			G_OPTION_ARG_FILENAME, &working_directory,
			"Specify the initial working directory of the terminal",
			NULL
		},
		{
			"reverse", 0, 0,
			G_OPTION_ARG_NONE, &reverse,
			"Reverse foreground/background colors", NULL
		},
		{
			"no-geometry-hints", 'G', G_OPTION_FLAG_REVERSE,
			G_OPTION_ARG_NONE, &use_geometry_hints,
			"Allow the terminal to be resized to any dimension, not constrained to fit to an integer multiple of characters",
			NULL
		},
		{
			"scrolled-window", 'W', 0,
			G_OPTION_ARG_NONE, &use_scrolled_window,
			"Use a GtkScrolledWindow as terminal container",
			NULL
		},
		{
			"scrollbar-policy", 'P', 0,
			G_OPTION_ARG_STRING, &scrollbar_policy_string,
			"Set the policy for the vertical scroolbar in the scrolled window (always|auto|never; default:always)",
			NULL
		},
		{
			"object-notifications", 'N', 0,
			G_OPTION_ARG_NONE, &show_object_notifications,
			"Print VteTerminal object notifications",
			NULL
		},
		{
			"output-file", 0, 0,
			G_OPTION_ARG_STRING, &output_file,
			"Save terminal contents to file at exit", NULL
		},
		{
			"pty-flags", 0, 0,
			G_OPTION_ARG_STRING, &pty_flags_string,
			"PTY flags set from default|no-utmp|no-wtmp|no-lastlog|no-helper|no-fallback", NULL
		},
		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;
        VteTerminalCursorBlinkMode cursor_blink_mode = VTE_CURSOR_BLINK_SYSTEM;
        VteTerminalCursorShape cursor_shape = VTE_CURSOR_SHAPE_BLOCK;
        GtkPolicyType scrollbar_policy = GTK_POLICY_ALWAYS;
        VtePtyFlags pty_flags = VTE_PTY_DEFAULT;

	/* Have to do this early. */
	if (getenv("VTE_PROFILE_MEMORY")) {
		if (atol(getenv("VTE_PROFILE_MEMORY")) != 0) {
			g_mem_set_vtable(glib_mem_profiler_table);
		}
	}

	context = g_option_context_new (" - test VTE terminal emulation");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);
	if (error != NULL) {
		g_printerr ("Failed to parse command line arguments: %s\n",
				error->message);
		g_error_free (error);
		return 1;
	}

        if (cursor_blink_mode_string) {
                cursor_blink_mode = parse_enum(VTE_TYPE_TERMINAL_CURSOR_BLINK_MODE, cursor_blink_mode_string);
                g_free(cursor_blink_mode_string);
        }
        if (cursor_shape_string) {
                cursor_shape = parse_enum(VTE_TYPE_TERMINAL_CURSOR_SHAPE, cursor_shape_string);
                g_free(cursor_shape_string);
        }
        if (scrollbar_policy_string) {
                scrollbar_policy = parse_enum(GTK_TYPE_POLICY_TYPE, scrollbar_policy_string);
                g_free(scrollbar_policy_string);
        }
        if (pty_flags_string) {
                pty_flags |= parse_flags(VTE_TYPE_PTY_FLAGS, pty_flags_string);
                g_free(pty_flags_string);
        }

	if (!reverse) {
		back.red = back.green = back.blue = 0xffff;
		fore.red = fore.green = fore.blue = 0x0000;
	} else {
		back.red = back.green = back.blue = 0x0000;
		fore.red = fore.green = fore.blue = 0xffff;
	}

	highlight.red = highlight.green = highlight.blue = 0xc000;
	cursor.red = 0xffff;
	cursor.green = cursor.blue = 0x8000;
	tint.red = tint.green = tint.blue = 0;
	tint = back;

	gdk_window_set_debug_updates(debug);

	/* Create a window to hold the scrolling shell, and hook its
	 * delete event to the quit function.. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_resize_mode(GTK_CONTAINER(window),
				      GTK_RESIZE_IMMEDIATE);

	/* Set ARGB colormap */
	screen = gtk_widget_get_screen (window);
	colormap = gdk_screen_get_rgba_colormap (screen);
	if (colormap)
	    gtk_widget_set_colormap(window, colormap);

        if (use_scrolled_window) {
                scrolled_window = gtk_scrolled_window_new (NULL, NULL);
                gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                               GTK_POLICY_NEVER, scrollbar_policy);
                gtk_container_add(GTK_CONTAINER(window), scrolled_window);
        } else {
                /* Create a box to hold everything. */
                hbox = gtk_hbox_new(0, FALSE);
                gtk_container_add(GTK_CONTAINER(window), hbox);
        }

	/* Create the terminal widget and add it to the scrolling shell. */
	widget = vte_terminal_new();
	terminal = VTE_TERMINAL (widget);
	if (!dbuffer) {
		gtk_widget_set_double_buffered(widget, dbuffer);
	}
        g_signal_connect(terminal, "child-exited", G_CALLBACK(child_exit_cb), NULL);
        if (show_object_notifications)
                g_signal_connect(terminal, "notify", G_CALLBACK(terminal_notify_cb), NULL);
        if (use_scrolled_window) {
                gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(terminal));
        } else {
                gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
        }

	/* Connect to the "char_size_changed" signal to set geometry hints
	 * whenever the font used by the terminal is changed. */
	if (use_geometry_hints) {
		char_size_changed(widget, 0, 0, window);
		g_signal_connect(widget, "char-size-changed",
				 G_CALLBACK(char_size_changed), window);
		g_signal_connect(widget, "realize",
				 G_CALLBACK(char_size_realized), window);
	}

	/* Connect to the "window_title_changed" signal to set the main
	 * window's title. */
	g_signal_connect(widget, "window-title-changed",
			 G_CALLBACK(window_title_changed), window);
	if (icon_title) {
		g_signal_connect(widget, "icon-title-changed",
				 G_CALLBACK(icon_title_changed), window);
	}

	/* Connect to the "status-line-changed" signal. */
	g_signal_connect(widget, "status-line-changed",
			 G_CALLBACK(status_line_changed), widget);

	/* Connect to the "button-press" event. */
	g_signal_connect(widget, "button-press-event",
			 G_CALLBACK(button_pressed), widget);

	/* Connect to application request signals. */
	g_signal_connect(widget, "iconify-window",
			 G_CALLBACK(iconify_window), window);
	g_signal_connect(widget, "deiconify-window",
			 G_CALLBACK(deiconify_window), window);
	g_signal_connect(widget, "raise-window",
			 G_CALLBACK(raise_window), window);
	g_signal_connect(widget, "lower-window",
			 G_CALLBACK(lower_window), window);
	g_signal_connect(widget, "maximize-window",
			 G_CALLBACK(maximize_window), window);
	g_signal_connect(widget, "restore-window",
			 G_CALLBACK(restore_window), window);
	g_signal_connect(widget, "refresh-window",
			 G_CALLBACK(refresh_window), window);
	g_signal_connect(widget, "resize-window",
			 G_CALLBACK(resize_window), window);
	g_signal_connect(widget, "move-window",
			 G_CALLBACK(move_window), window);

	/* Connect to font tweakage. */
	g_signal_connect(widget, "increase-font-size",
			 G_CALLBACK(increase_font_size), window);
	g_signal_connect(widget, "decrease-font-size",
			 G_CALLBACK(decrease_font_size), window);

        if (!use_scrolled_window) {
                /* Create the scrollbar for the widget. */
                scrollbar = gtk_vscrollbar_new(terminal->adjustment);
                gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);
        }

	/* Set some defaults. */
	vte_terminal_set_audible_bell(terminal, audible);
	vte_terminal_set_visible_bell(terminal, !audible);
	vte_terminal_set_cursor_blink_mode(terminal, cursor_blink_mode);
	vte_terminal_set_scroll_background(terminal, scroll);
	vte_terminal_set_scroll_on_output(terminal, FALSE);
	vte_terminal_set_scroll_on_keystroke(terminal, TRUE);
	vte_terminal_set_scrollback_lines(terminal, lines);
	vte_terminal_set_mouse_autohide(terminal, TRUE);
	if (background != NULL) {
		vte_terminal_set_background_image_file(terminal,
						       background);
	}
	if (transparent) {
		vte_terminal_set_background_transparent(terminal,
							TRUE);
	}
	vte_terminal_set_background_tint_color(terminal, &tint);
	vte_terminal_set_colors(terminal, &fore, &back, NULL, 0);
	vte_terminal_set_opacity(terminal, 0xdddd);
	if (highlight_set) {
		vte_terminal_set_color_highlight(terminal,
						 &highlight);
	}
	if (cursor_set) {
		vte_terminal_set_color_cursor(terminal, &cursor);
	}
	if (termcap != NULL) {
		vte_terminal_set_emulation(terminal, termcap);
	}
        vte_terminal_set_cursor_shape(terminal, cursor_shape);

	/* Set the default font. */
	vte_terminal_set_font_from_string_full(terminal, font,
					       antialias ? VTE_ANTI_ALIAS_USE_DEFAULT : VTE_ANTI_ALIAS_FORCE_DISABLE);

	/* Match "abcdefg". */
	if (dingus) {
		int id;
		GRegex *regex;
		regex = g_regex_new (DINGUS1, 0, 0, NULL);
		id = vte_terminal_match_add_gregex(terminal, regex, 0);
		g_regex_unref (regex);
		vte_terminal_match_set_cursor_type(terminal,
						   id, GDK_GUMBY);
		regex = g_regex_new (DINGUS2, 0, 0, NULL);
		id = vte_terminal_match_add_gregex(terminal, regex, 0);
		g_regex_unref (regex);
		vte_terminal_match_set_cursor_type(terminal,
						   id, GDK_HAND1);
	}

	if (console) {
		/* Open a "console" connection. */
		int consolefd = -1, yes = 1, watch;
		GIOChannel *channel;
		consolefd = open("/dev/console", O_RDONLY | O_NOCTTY);
		if (consolefd != -1) {
			/* Assume failure. */
			console = FALSE;
#ifdef TIOCCONS
			if (ioctl(consolefd, TIOCCONS, &yes) != -1) {
				/* Set up a listener. */
				channel = g_io_channel_unix_new(consolefd);
				watch = g_io_add_watch(channel,
						       G_IO_IN,
						       read_and_feed,
						       widget);
				g_signal_connect(widget,
						 "eof",
						 G_CALLBACK(disconnect_watch),
						 GINT_TO_POINTER(watch));
				g_signal_connect(widget,
						 "child-exited",
						 G_CALLBACK(disconnect_watch),
						 GINT_TO_POINTER(watch));
				g_signal_connect(widget,
						 "realize",
						 G_CALLBACK(take_xconsole_ownership),
						 NULL);
#ifdef VTE_DEBUG
				vte_terminal_feed(terminal,
						  "Console log for ...\r\n",
						  -1);
#endif
				/* Record success. */
				console = TRUE;
			}
#endif
		} else {
			/* Bail back to normal mode. */
			g_warning(_("Could not open console.\n"));
			close(consolefd);
			console = FALSE;
		}
	}

	if (!console) {
		if (shell) {
                        GError *err = NULL;
                        char **command_argv = NULL;
                        int command_argc;
                        GPid pid = -1;

			_VTE_DEBUG_IF(VTE_DEBUG_MISC)
				vte_terminal_feed(terminal, message, -1);

                        if (command == NULL)
                                command = "/bin/sh"; // FIXMEchpe

                        if (command != NULL) {
                                if (!g_shell_parse_argv(command, &command_argc, &command_argv, &err) ||
                                    !vte_terminal_fork_command_full(terminal,
                                                                    pty_flags,
                                                                    NULL,
                                                                    command_argv,
                                                                    env_add,
                                                                    G_SPAWN_SEARCH_PATH,
                                                                    NULL, NULL,
                                                                    &pid,
                                                                    &err)) {
                                        g_warning("Failed to fork: %s\n", err->message);
                                        g_error_free(err);
                                } else {
                                        g_print("Fork succeeded, PID %d\n", pid);
                                }
                        }
                        g_strfreev(command_argv);
	#ifdef VTE_DEBUG
			if (command == NULL) {
				vte_terminal_feed_child(terminal,
							"pwd\n", -1);
			}
	#endif
		} else {
			long i;
			i = vte_terminal_forkpty(terminal,
						 env_add, working_directory,
						 TRUE, TRUE, TRUE);
			switch (i) {
			case -1:
				/* abnormal */
				g_warning("Error in vte_terminal_forkpty(): %s",
					  strerror(errno));
				break;
			case 0:
				/* child */
				for (i = 0; ; i++) {
					switch (i % 3) {
					case 0:
					case 1:
						g_print("%ld\n", i);
						break;
					case 2:
						g_printerr("%ld\n", i);
						break;
					}
					sleep(1);
				}
				_exit(0);
				break;
			default:
				g_print("Child PID is %ld (mine is %ld).\n",
					(long) i, (long) getpid());
				/* normal */
				break;
			}
		}
	}

	g_object_set_data (G_OBJECT (widget), "output_file", (gpointer) output_file);

	/* Go for it! */
	g_signal_connect(widget, "child-exited", G_CALLBACK(child_exited), window);
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), widget);

	add_weak_pointer(G_OBJECT(widget), &widget);
	add_weak_pointer(G_OBJECT(window), &window);

        gtk_widget_realize(widget);
        if (geometry) {
                if (!gtk_window_parse_geometry (GTK_WINDOW(window), geometry)) {
                        g_warning (_("Could not parse the geometry spec passed to --geometry"));
                }
        }

	gtk_widget_show_all(window);

	gtk_main();

	g_assert(widget == NULL);
	g_assert(window == NULL);

	if (keep) {
		while (TRUE) {
			sleep(60);
		}
	}

	return 0;
}
