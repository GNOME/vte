/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
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
#define DINGUS2 DINGUS1 "/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]"

static const char *builtin_dingus[] = {
  DINGUS1,
  DINGUS2,
  NULL
};

static void
window_title_changed(VteBuffer *buffer, GtkWindow *window)
{
	gtk_window_set_title(window, vte_buffer_get_window_title(buffer));
}

static void
icon_title_changed(VteBuffer *buffer, GtkWindow *window)
{
	g_message("Icon title changed to \"%s\".\n",
		  vte_buffer_get_icon_title(buffer));
}

static void
char_size_changed(GtkWidget *widget, guint width, guint height, gpointer data)
{
	VteTerminal *terminal;
	GtkWindow *window;
	GdkGeometry geometry;
	GtkBorder padding;

	g_assert(GTK_IS_WINDOW(data));
	g_assert(VTE_IS_TERMINAL(widget));

	terminal = VTE_TERMINAL(widget);
	window = GTK_WINDOW(data);
	if (!gtk_widget_get_realized (GTK_WIDGET (window)))
		return;

        gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
                                      gtk_widget_get_state_flags(widget),
                                      &padding);
        geometry.width_inc = width;
	geometry.height_inc = height;
	geometry.base_width = padding.left + padding.right;
	geometry.base_height = padding.top + padding.bottom;
	geometry.min_width = geometry.base_width + width * 2;
	geometry.min_height = geometry.base_height + height * 2;

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
	GtkBorder padding;

	g_assert(GTK_IS_WINDOW(data));
	g_assert(VTE_IS_TERMINAL(widget));

	terminal = VTE_TERMINAL(widget);
	window = GTK_WINDOW(data);
	if (!gtk_widget_get_realized (GTK_WIDGET(window)))
		return;

        gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
                                      gtk_widget_get_state_flags(widget),
                                      &padding);
        width = vte_terminal_get_char_width (terminal);
	height = vte_terminal_get_char_height (terminal);
	geometry.width_inc = width;
	geometry.height_inc = height;
	geometry.base_width = padding.left + padding.right;
	geometry.base_height = padding.top + padding.bottom;
	geometry.min_width = geometry.base_width + width * 2;
	geometry.min_height = geometry.base_height + height * 2;

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
child_exited(GtkWidget *terminal, int status, gpointer window)
{
	_vte_debug_print(VTE_DEBUG_MISC, "Child exited with status %x\n", status);
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
	GtkBorder padding;
	int char_width, char_height;

	switch (event->button) {
	case 3:
		terminal = VTE_TERMINAL(widget);

                gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
                                              gtk_widget_get_state_flags(widget),
                                              &padding);
                char_width = vte_terminal_get_char_width (terminal);
		char_height = vte_terminal_get_char_height (terminal);
		match = vte_terminal_match_check(terminal,
						 (event->x - padding.left) / char_width,
						 (event->y - padding.top) / char_height,
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
	cairo_rectangle_int_t rect;

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
		GtkBorder padding;

		terminal = VTE_TERMINAL(widget);

		gtk_window_get_size(GTK_WINDOW(data), &owidth, &oheight);

		/* Take into account border overhead. */
		char_width = vte_terminal_get_char_width (terminal);
		char_height = vte_terminal_get_char_height (terminal);
		column_count = vte_terminal_get_column_count (terminal);
		row_count = vte_terminal_get_row_count (terminal);
                gtk_style_context_get_padding(gtk_widget_get_style_context(widget),
                                              gtk_widget_get_state_flags(widget),
                                              &padding);

                owidth -= char_width * column_count + padding.left + padding.right;
                oheight -= char_height * row_count + padding.top + padding.bottom;
		gtk_window_resize(GTK_WINDOW(data),
				  width + owidth, height + oheight);
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
adjust_font_size(GtkWidget *widget, gpointer data, gdouble factor)
{
	VteTerminal *terminal;
        gdouble scale;
        glong char_width, char_height;
	gint columns, rows, owidth, oheight;

	/* Read the screen dimensions in cells. */
	terminal = VTE_TERMINAL(widget);
	columns = vte_terminal_get_column_count(terminal);
	rows = vte_terminal_get_row_count(terminal);

	/* Take into account padding and border overhead. */
	gtk_window_get_size(GTK_WINDOW(data), &owidth, &oheight);
        char_width = vte_terminal_get_char_width (terminal);
        char_height = vte_terminal_get_char_height (terminal);
	owidth -= char_width * columns;
	oheight -= char_height * rows;

	scale = vte_terminal_get_font_scale(terminal);
        vte_terminal_set_font_scale(terminal, scale * factor);

	/* Change the font, then resize the window so that we have the same
	 * number of rows and columns. */

        /* This above call will have changed the char size! */
        char_width = vte_terminal_get_char_width (terminal);
        char_height = vte_terminal_get_char_height (terminal);

	gtk_window_resize(GTK_WINDOW(data),
			  columns * char_width + owidth,
			  rows * char_height + oheight);
}

static void
increase_font_size(GtkWidget *widget, gpointer data)
{
	adjust_font_size(widget, data, 1.2);
}

static void
decrease_font_size(GtkWidget *widget, gpointer data)
{
	adjust_font_size(widget, data, 1. / 1.2);
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
		vte_buffer_feed(vte_terminal_get_buffer(VTE_TERMINAL(data)), buf, size);
		return TRUE;
	}
	return FALSE;
}

static void
disconnect_watch(gpointer data)
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
        GtkTargetList *target_list;
        GtkTargetEntry *targets;
        int n_targets;

        target_list = gtk_target_list_new(NULL, 0);
        gtk_target_list_add_text_targets(target_list, 0);
        targets = gtk_target_table_new_from_list (target_list, &n_targets);
        gtk_target_list_unref(target_list);

	memset(hostname, '\0', sizeof(hostname));
	gethostname(hostname, sizeof(hostname) - 1);

	name = g_strdup_printf("MIT_CONSOLE_%s", hostname);
	atom = gdk_atom_intern(name, FALSE);
	clipboard = gtk_clipboard_get_for_display(gtk_widget_get_display(widget),
						  atom);
	g_free(name);

	gtk_clipboard_set_with_owner(clipboard,
				     targets,
				     n_targets,
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

/* Derived terminal class */

typedef struct _VteappTerminal      VteappTerminal;
typedef struct _VteappTerminalClass VteappTerminalClass;

struct _VteappTerminalClass {
        VteTerminalClass parent_class;
};
struct _VteappTerminal {
        VteTerminal parent_instance;
};

static GType vteapp_terminal_get_type(void);

G_DEFINE_TYPE(VteappTerminal, vteapp_terminal, VTE_TYPE_TERMINAL)

static void
vteapp_terminal_class_init(VteappTerminalClass *klass)
{
}

static void
vteapp_terminal_init(VteappTerminal *terminal)
{
}

static GtkWidget *
vteapp_terminal_new(void)
{
        return g_object_new(vteapp_terminal_get_type(), NULL);
}

/* Command line options */

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

static void
add_dingus (VteTerminal *terminal,
            char **dingus)
{
        const GdkCursorType cursors[] = { GDK_GUMBY, GDK_HAND1 };
        GRegex *regex;
        GError *error;
        int id, i;

        for (i = 0; dingus[i]; ++i) {
                error = NULL;
                if (!(regex = g_regex_new(dingus[i], G_REGEX_OPTIMIZE, 0, &error))) {
                        g_warning("Failed to compile regex '%s': %s\n",
                                  dingus[i], error->message);
                        g_error_free(error);
                        continue;
                }

                id = vte_terminal_match_add_gregex(terminal, regex, 0);
                g_regex_unref (regex);
                vte_terminal_match_set_cursor_type(terminal, id,
                                                   cursors[i % G_N_ELEMENTS(cursors)]);
        }
}

int
main(int argc, char **argv)
{
	GdkScreen *screen;
	GdkVisual *visual;
	GtkWidget *window, *widget,*hbox = NULL, *scrollbar, *scrolled_window = NULL;
	VteTerminal *terminal;
        VteBuffer *buffer;
	char *env_add[] = {
#ifdef VTE_DEBUG
		(char *) "FOO=BAR", (char *) "BOO=BIZ",
#endif
		NULL};
	char *background = NULL;
	gboolean audible = TRUE,
		 debug = FALSE, use_builtin_dingus = FALSE, dbuffer = TRUE,
		 console = FALSE, scroll = FALSE, keep = FALSE,
		 icon_title = FALSE, shell = TRUE,
		 reverse = FALSE, use_geometry_hints = TRUE,
		 use_scrolled_window = FALSE,
		 show_object_notifications = FALSE;
	char *geometry = NULL;
	gint lines = 100;
	const char *message = "Launching interactive shell...\r\n";
	char *font = NULL;
	const char *termcap = NULL;
	const char *command = NULL;
	const char *working_directory = NULL;
	const char *output_file = NULL;
	char *pty_flags_string = NULL;
        char *cursor_color_string = NULL;
	char *cursor_blink_mode_string = NULL;
	char *cursor_shape_string = NULL;
	char *scrollbar_policy_string = NULL;
        char *border_width_string = NULL;
        char *css = NULL;
        char *css_file = NULL;
        char *selection_background_color_string = NULL;
        char **dingus = NULL;
	const GOptionEntry options[]={
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
                        "builtin-dingus", 'D', 0,
                        G_OPTION_ARG_NONE, &use_builtin_dingus,
                        "Highlight URLs inside the terminal", NULL
                },
		{
			"dingu", '\0', 0,
			G_OPTION_ARG_STRING_ARRAY, &dingus,
			"Add regex highlight", NULL
		},
		{
			"shell", 'S', G_OPTION_FLAG_REVERSE,
			G_OPTION_ARG_NONE, &shell,
			"Disable spawning a shell inside the terminal", NULL
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
			"selection-color", 'h', 0,
			G_OPTION_ARG_STRING, &selection_background_color_string,
			"Use distinct highlight color for selection", NULL
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
			"cursor-color", 'r', 0,
			G_OPTION_ARG_STRING, &cursor_color_string,
			"Enable a colored cursor", "COLOR"
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
                {
                        "border-width", 0, 0,
                        G_OPTION_ARG_STRING, &border_width_string,
                        "Border with", "WIDTH"
                },
                {
                        "css", 0, 0,
                        G_OPTION_ARG_STRING, &css,
                        "Inline CSS", "CSS"
                },
                {
                        "css-file", 0, 0,
                        G_OPTION_ARG_FILENAME, &css_file,
                        "CSS File", "FILE"
                },
		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;
	GtkPolicyType scrollbar_policy = GTK_POLICY_ALWAYS;
	VtePtyFlags pty_flags = VTE_PTY_DEFAULT;
        GString *css_string;

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

	if (scrollbar_policy_string) {
		scrollbar_policy = parse_enum(GTK_TYPE_POLICY_TYPE, scrollbar_policy_string);
		g_free(scrollbar_policy_string);
	}
	if (pty_flags_string) {
		pty_flags |= parse_flags(VTE_TYPE_PTY_FLAGS, pty_flags_string);
		g_free(pty_flags_string);
	}

        if (css_file) {
                GtkCssProvider *provider;
                GError *err = NULL;

                provider = gtk_css_provider_new();
                if (gtk_css_provider_load_from_path(provider, css_file, &err)) {
                        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                                                  GTK_STYLE_PROVIDER(provider),
                                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
                } else {
                        g_printerr("Failed to load CSS file: %s\n", err->message);
                        g_error_free(err);
                }
                g_object_unref(provider);
                provider = NULL;
                g_free(css_file);
        }

        css_string = g_string_new (NULL);
        if (css) {
                g_string_append (css_string, css);
                g_string_append_c (css_string, '\n');
        }

        g_string_append (css_string, "VteTerminal {\n");
        if (background) {
                g_string_append_printf (css_string, "background-image: url(\"%s\");\n",
                                        background);
                g_free(background);
        }
        if (cursor_color_string) {
                g_string_append_printf (css_string, "-VteTerminal-cursor-background-color: %s;\n"
                                                    "-VteTerminal-cursor-effect: color;\n",
                                        cursor_color_string);
                g_free(cursor_color_string);
        }
        if (selection_background_color_string) {
                g_string_append_printf (css_string, "-VteTerminal-selection-background-color: %s;\n"
                                                    "-VteTerminal-selection-effect: color;\n",
                                        selection_background_color_string);
                g_free(selection_background_color_string);
        }
        if (cursor_blink_mode_string) {
                g_string_append_printf (css_string, "-VteTerminal-cursor-blink-mode: %s;\n",
                                        cursor_blink_mode_string);
                g_free(cursor_blink_mode_string);
        }
        if (cursor_shape_string) {
                g_string_append_printf (css_string, "-VteTerminal-cursor-shape: %s;\n",
                                        cursor_shape_string);
                g_free(cursor_shape_string);
        }
        if (font) {
                g_string_append_printf (css_string, "-VteTerminal-font: %s;\n",
                                        font);
                g_free(font);
        }
        if (scroll) {
                g_string_append (css_string, "-VteTerminal-scroll-background: true;\n");
        }
        if (reverse) {
                g_string_append (css_string, "-VteTerminal-reverse: true;\n");
        }
        g_string_append (css_string, "}\n");

        if (css_string->len > 14) {
                GtkCssProvider *provider;
                GError *err = NULL;

                provider = gtk_css_provider_new();
                if (gtk_css_provider_load_from_data(provider, css_string->str, css_string->len, &err)) {
                        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                                                  GTK_STYLE_PROVIDER(provider),
                                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
                } else {
                        g_printerr("Failed to parse CSS: %s\n", err->message);
                        g_error_free(err);
                }
                g_object_unref(provider);
                g_free(css_file);
                g_string_free (css_string, TRUE);
        }


	gdk_window_set_debug_updates(debug);

	/* Create a window to hold the scrolling shell, and hook its
	 * delete event to the quit function.. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_resize_mode(GTK_CONTAINER(window),
				      GTK_RESIZE_IMMEDIATE);
        if (border_width_string) {
                guint w;

                w = g_ascii_strtoull (border_width_string, NULL, 10);
                gtk_container_set_border_width(GTK_CONTAINER(window), w);
                g_free (border_width_string);
        }

	/* Set ARGB visual */
	screen = gtk_widget_get_screen (window);
	visual = gdk_screen_get_rgba_visual(screen);
	if (visual)
		gtk_widget_set_visual(GTK_WIDGET(window), visual);

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
	widget = vteapp_terminal_new();
	terminal = VTE_TERMINAL (widget);
        buffer = vte_terminal_get_buffer(terminal);
        if (!dbuffer) {
		gtk_widget_set_double_buffered(widget, dbuffer);
	}
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
	g_signal_connect(buffer, "window-title-changed",
			 G_CALLBACK(window_title_changed), window);
	if (icon_title) {
		g_signal_connect(buffer, "icon-title-changed",
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
		scrollbar = gtk_vscrollbar_new(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(terminal)));
		gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);
	}

	/* Set some defaults. */
	vte_terminal_set_audible_bell(terminal, audible);
	vte_terminal_set_visible_bell(terminal, !audible);
	vte_terminal_set_scroll_on_output(terminal, FALSE);
	vte_terminal_set_scroll_on_keystroke(terminal, TRUE);
	vte_buffer_set_scrollback_lines(vte_terminal_get_buffer(terminal), lines);
	vte_terminal_set_mouse_autohide(terminal, TRUE);

	if (termcap != NULL) {
		vte_buffer_set_emulation(vte_terminal_get_buffer(terminal), termcap);
	}

	/* Match "abcdefg". */
	if (use_builtin_dingus) {
                add_dingus (terminal, (char **) builtin_dingus);
	}
	if (dingus) {
                add_dingus (terminal, dingus);
                g_strfreev (dingus);
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
				g_signal_connect_swapped(widget,
                                                         "eof",
                                                         G_CALLBACK(disconnect_watch),
                                                         GINT_TO_POINTER(watch));
				g_signal_connect_swapped(widget,
                                                         "child-exited",
                                                         G_CALLBACK(disconnect_watch),
                                                         GINT_TO_POINTER(watch));
				g_signal_connect(widget,
						 "realize",
						 G_CALLBACK(take_xconsole_ownership),
						 NULL);
#ifdef VTE_DEBUG
				vte_buffer_feed(vte_terminal_get_buffer(terminal),
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
				vte_buffer_feed(vte_terminal_get_buffer(terminal), message, -1);

                        if (command == NULL || *command == '\0')
                                command = vte_get_user_shell ();

			if (command == NULL || *command == '\0')
				command = g_getenv ("SHELL");

			if (command == NULL || *command == '\0')
				command = "/bin/sh";

			if (!g_shell_parse_argv(command, &command_argc, &command_argv, &err) ||
			    !vte_terminal_spawn_sync(terminal,
							    pty_flags,
							    NULL,
							    command_argv,
							    env_add,
							    G_SPAWN_SEARCH_PATH,
							    NULL, NULL,
							    &pid,
                                                            NULL /* cancellable */,
							    &err)) {
				g_warning("Failed to fork: %s\n", err->message);
				g_error_free(err);
			} else {
				g_print("Fork succeeded, PID %d\n", pid);
			}

			g_strfreev(command_argv);
		} else {
                        #ifdef HAVE_FORK
                        GError *err = NULL;
                        VtePty *pty;
			pid_t pid;
                        int i;

                        pty = vte_pty_new_sync(VTE_PTY_DEFAULT, NULL, &err);
                        if (pty == NULL) {
                                g_printerr ("Failed to create PTY: %s\n", err->message);
                                g_error_free(err);
                                return 1;
                        }

			pid = fork();
			switch (pid) {
			case -1:
				/* abnormal */
				g_warning("Error forking: %s",
					  g_strerror(errno));
                                g_object_unref(pty);
                                break;
			case 0:
				/* child */
                                vte_pty_child_setup(pty);

				for (i = 0; ; i++) {
					switch (i % 3) {
					case 0:
					case 1:
						g_print("%d\n", i);
						break;
					case 2:
						g_printerr("%d\n", i);
						break;
					}
					sleep(1);
				}
				_exit(0);
				break;
			default:
                                vte_terminal_set_pty(terminal, pty);
                                g_object_unref(pty);
                                vte_terminal_watch_child(terminal, pid);
				g_print("Child PID is %d (mine is %d).\n",
					(int) pid, (int) getpid());
				/* normal */
				break;
			}
                        #endif /* HAVE_FORK */
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
	else {
		/* As of GTK+ 2.91.0, the default size of a window comes from its minimum
		 * size not its natural size, so we need to set the right default size
		 * explicitly */
		gtk_window_set_default_geometry (GTK_WINDOW (window),
						 vte_terminal_get_column_count (terminal),
						 vte_terminal_get_row_count (terminal));
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
