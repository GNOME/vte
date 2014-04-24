/*
 * Copyright © 2001,2002 Red Hat, Inc.
 * Copyright © 2014 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

namespace Test
{

class Window : Gtk.ApplicationWindow
{
	private Vte.Terminal terminal;
	private Gtk.Scrollbar scrollbar;
	private Gtk.Clipboard clipboard;
	private GLib.Pid child_pid;

	private string[] builtin_dingus = {
		"(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?",
		"(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]"
	};

    private const GLib.ActionEntry[] action_entries = {
        { "copy",        action_copy_cb            },
        { "copy-match",  action_copy_match_cb, "s" },
        { "paste",       action_paste_cb           },
        { "reset",       action_reset_cb           }
    };

	public Window(App app)
	{
		Object(application: app);

		add_action_entries (action_entries, this);

        /* set_resize_mode(Gtk.ResizeMode.IMMEDIATE); */

		clipboard = get_clipboard(Gdk.SELECTION_CLIPBOARD);
		clipboard.owner_change.connect(clipboard_owner_change_cb);

		title = "Terminal";

		/* Set ARGB visual */
		if (!App.Options.no_argb_visual) {
			var screen = get_screen();
			Gdk.Visual? visual = screen.get_rgba_visual();
			if (visual != null)
				set_visual(visual);
		}

		var ui = new Gtk.Builder.from_resource("/org/gnome/vte/test/app/ui/window.ui");
		add(ui.get_object("main-box") as Gtk.Widget);

		var box = ui.get_object("terminal-box") as Gtk.Box;

		if (App.Options.no_toolbar) {
			var toolbar = ui.get_object("toolbar") as Gtk.Widget;
			toolbar.hide();
		}
			
		terminal = new Vte.Terminal();

		/* Connect scrollbar */
		scrollbar = ui.get_object("scrollbar") as Gtk.Scrollbar;
		scrollbar.set_adjustment(terminal.get_vadjustment());

		/* Signals */
		terminal.button_press_event.connect(button_press_event_cb);
		terminal.char_size_changed.connect(char_size_changed_cb);
		terminal.child_exited.connect(child_exited_cb);
		terminal.decrease_font_size.connect(decrease_font_size_cb);
		terminal.deiconify_window.connect(deiconify_window_cb);
		terminal.icon_title_changed.connect(icon_title_changed_cb);
		terminal.iconify_window.connect(iconify_window_cb);
		terminal.increase_font_size.connect(increase_font_size_cb);
		terminal.lower_window.connect(lower_window_cb);
		terminal.maximize_window.connect(maximize_window_cb);
		terminal.move_window.connect(move_window_cb);
		terminal.raise_window.connect(raise_window_cb);
		terminal.realize.connect(realize_cb);
		terminal.refresh_window.connect(refresh_window_cb);
		terminal.resize_window.connect(resize_window_cb);
		terminal.restore_window.connect(restore_window_cb);
		terminal.selection_changed.connect(selection_changed_cb);
		terminal.status_line_changed.connect(status_line_changed_cb);
		terminal.window_title_changed.connect(window_title_changed_cb);
		if (App.Options.object_notifications)
			terminal.notify.connect(notify_cb);

		/* Settings */
		if (App.Options.no_double_buffer)
			terminal.set_double_buffered(true);

		if (App.Options.term != null)
			terminal.set_emulation(App.Options.term);
		if (App.Options.encoding != null)
			terminal.set_encoding(App.Options.encoding);

		terminal.set_audible_bell(App.Options.audible);
		terminal.set_cjk_ambiguous_width(App.Options.get_cjk_ambiguous_width());
		terminal.set_cursor_blink_mode(App.Options.get_cursor_blink_mode());
		terminal.set_cursor_shape(App.Options.get_cursor_shape());
		terminal.set_mouse_autohide(true);
		terminal.set_rewrap_on_resize(!App.Options.no_rewrap);
		terminal.set_scroll_on_output(false);
		terminal.set_scroll_on_keystroke(true);
		terminal.set_scrollback_lines(App.Options.scrollback_lines);
		terminal.set_visible_bell(!App.Options.audible);

		/* Style */
		if (App.Options.font_string != null) {
			var desc = Pango.FontDescription.from_string(App.Options.font_string);
			terminal.set_font(desc);
		}

		terminal.set_colors_rgba(App.Options.get_color_fg(),
								 App.Options.get_color_bg(),
								 null);
		terminal.set_color_cursor_rgba(App.Options.get_color_cursor());
		terminal.set_color_highlight_rgba(App.Options.get_color_hl_bg());
		terminal.set_color_highlight_foreground_rgba(App.Options.get_color_hl_fg());

		/* Dingus */
		if (!App.Options.no_builtin_dingus)
			add_dingus(builtin_dingus);
		if (App.Options.dingus != null)
			add_dingus(App.Options.dingus);

		/* Property actions */
		add_action(new GLib.PropertyAction ("input-enabled", terminal, "input-enabled"));

		/* Done! */
		box.pack_start(terminal);
		terminal.show();

		update_paste_sensitivity();
		update_copy_sensitivity();

		terminal.grab_focus();

		assert(!get_realized());
	}

	private void add_dingus(string[] dingus)
	{
		const Gdk.CursorType cursors[] = { Gdk.CursorType.GUMBY, Gdk.CursorType.HAND1 };

		for (int i = 0; i < dingus.length; ++i) {
			try {
				GLib.Regex regex;
				int tag;

				regex = new GLib.Regex(dingus[i], GLib.RegexCompileFlags.OPTIMIZE, 0);
				tag = terminal.match_add_gregex(regex, 0);
				terminal.match_set_cursor_type(tag, cursors[i % cursors.length]);
			} catch (Error e) {
				printerr("Failed to compile regex \"%s\": %s\n", dingus[i], e.message);
			}
		}
	}

	private void adjust_font_size(double factor)
	{
		var columns = terminal.get_column_count();
		var rows = terminal.get_row_count();

		terminal.set_font_scale(terminal.get_font_scale() * factor);

		update_geometry();
		resize_to_geometry((int)columns, (int)rows);
	}

	public void apply_geometry()
	{
		/* The terminal needs to be realized first, so that when parsing the
		 * geometry, the right geometry hints are already in place.
		 */
		terminal.realize();

		if (App.Options.geometry != null) {
			if (parse_geometry(App.Options.geometry)) {
				/* After parse_geometry(), we can get the default size in
				 * width/height increments, i.e. in grid size.
				 */
				int columns, rows;
				get_default_size(out columns, out rows);
				terminal.set_size(columns, rows);
				resize_to_geometry(columns, rows);
			} else
				printerr("Failed to parse geometry spec \"%s\"\n", App.Options.geometry);
		} else {
			/* In GTK+ 3.0, the default size of a window comes from its minimum
			 * size not its natural size, so we need to set the right default size
			 * explicitly */
			set_default_geometry((int)terminal.get_column_count(),
								 (int)terminal.get_row_count());
		}
	}

	private void launch_command(string command) throws Error
	{
		string[] argv;

		Shell.parse_argv(command, out argv);
		terminal.spawn_sync(App.Options.get_pty_flags(),
							App.Options.working_directory,
							argv,
							App.Options.environment,
							GLib.SpawnFlags.SEARCH_PATH,
							null, /* child setup */
							out child_pid,
							null /* cancellable */);
		print("Fork succeeded, PID %d\n", child_pid);
	}

	private void launch_shell() throws Error
	{
		string? shell;

		shell = Vte.get_user_shell();
		if (shell == null || shell[0] == '\0')
			shell = Environment.get_variable("SHELL");
		if (shell == null || shell[0] == '\0')
			shell = "/bin/sh";

		launch_command(shell);
	}

	private void fork() throws Error
	{
		Vte.Pty pty;
		Posix.pid_t pid;

		pty = new Vte.Pty.sync(App.Options.get_pty_flags(), null);

		pid = Posix.fork();

		switch (pid) {
		case -1: /* error */
			printerr("Error forking: %m");
			break;
		case 0: /* child */ {
			pty.child_setup();

			for (int i = 0; ; i++) {
				switch (i % 3) {
				case 0:
				case 1:
					print("%d\n", i);
					break;
				case 2:
					printerr("%d\n", i);
					break;
				}
				Posix.sleep(1);
			}
		}
		default: /* parent */
			terminal.set_pty(pty);
			terminal.watch_child(pid);
			print("Child PID is %d (mine is %d).\n", (int)pid, (int)Posix.getpid());
			break;
		}
	}

	public void launch()
	{
		try {
			if (App.Options.command != null)
				launch_command(App.Options.command);
			else if (!App.Options.no_shell)
				launch_shell();
			else
				fork();
		} catch (Error e) {
			printerr("Error: %s\n", e.message);
		}
	}

	private void update_copy_sensitivity()
	{
		var action = lookup_action("copy") as GLib.SimpleAction;
		action.set_enabled(terminal.get_has_selection());
	}

	private void update_paste_sensitivity()
	{
		Gdk.Atom[] targets;
		bool can_paste;

		if (clipboard.wait_for_targets(out targets))
			can_paste = Gtk.targets_include_text(targets);
		else
			can_paste = false;

		var action = lookup_action("paste") as GLib.SimpleAction;
		action.set_enabled(can_paste);
	}

	private void update_geometry()
	{
		if (App.Options.no_geometry_hints)
			return;
		if (!terminal.get_realized())
			return;

		terminal.set_geometry_hints_for_window(this);
	}

	/* Callbacks */

	private void action_copy_cb()
	{
		terminal.copy_clipboard();
	}

	private void action_copy_match_cb(GLib.SimpleAction action, GLib.Variant? parameter)
	{
		size_t len;
		unowned string str = parameter.get_string(out len);
		clipboard.set_text(str, (int)len);
	}

	private void action_paste_cb()
	{
		terminal.paste_clipboard();
	}

	private void action_reset_cb()
	{
		bool clear;
		Gdk.ModifierType modifiers;

		if (Gtk.get_current_event_state(out modifiers))
			clear = (modifiers & Gdk.ModifierType.CONTROL_MASK) != 0;
		else
			clear = false;

		terminal.reset(true, clear);
	}

	private bool button_press_event_cb(Gtk.Widget widget, Gdk.EventButton event)
	{
		if (event.button != 3)
			return false;
		if (App.Options.no_context_menu)
			return false;

		var match = terminal.match_check_event(event, null);

		var menu = new GLib.Menu();
		menu.append("_Copy", "win.copy");
		if (match != null)
			menu.append("Copy _Match", "win.copy-match::" + match);

		menu.append("_Paste", "win.paste");

		var popup = new Gtk.Menu.from_model(menu);
		popup.attach_to_widget(this, null);
		popup.popup(null, null, null, event.button, event.time);

		return false;
	}

	private void char_size_changed_cb(Vte.Terminal terminal, uint width, uint height)
	{
		update_geometry();
	}

	private void child_exited_cb(Vte.Terminal terminal, int status)
	{
		printerr("Child exited with status %x\n", status);

		if (App.Options.output_filename != null) {
			try {
				var file = GLib.File.new_for_commandline_arg(App.Options.output_filename);
				var stream = file.replace(null, false, GLib.FileCreateFlags.NONE, null);
				terminal.write_contents(stream, Vte.WriteFlags.DEFAULT, null);
			} catch (Error e) {
				printerr("Failed to write output to \"%s\": %s\n",
						 App.Options.output_filename, e.message);
			}
		}

		if (App.Options.keep)
			return;

		destroy();
	}

	private void clipboard_owner_change_cb(Gtk.Clipboard clipboard, Gdk.Event event)
	{
		update_paste_sensitivity();
	}

	private void decrease_font_size_cb(Vte.Terminal terminal)
	{
		adjust_font_size(1.0 / 1.2);
	}

	public void deiconify_window_cb(Vte.Terminal terminal)
	{
		deiconify();
	}

	private void icon_title_changed_cb(Vte.Terminal terminal)
	{
		get_window().set_icon_name(terminal.get_icon_title());
	}

	private void iconify_window_cb(Vte.Terminal terminal)
	{
		iconify();
	}

	private void increase_font_size_cb(Vte.Terminal terminal)
	{
		adjust_font_size(1.2);
	}

	private void lower_window_cb(Vte.Terminal terminal)
	{
		if (!get_realized())
			return;

		get_window().lower();
	}

	private void maximize_window_cb(Vte.Terminal terminal)
	{
		maximize();
	}

	private void move_window_cb(Vte.Terminal terminal, uint x, uint y)
	{
		move((int)x, (int)y);
	}

	private void notify_cb(Object object, ParamSpec pspec)
	{
		if (pspec.owner_type != typeof(Vte.Terminal))
			return;

		var value = GLib.Value(pspec.value_type);
		object.get_property(pspec.name, ref value);
		var str = value.strdup_contents();
		print("NOTIFY property \"%s\" value %s\n", pspec.name, str);
	}

	private void raise_window_cb(Vte.Terminal terminal)
	{
		if (!get_realized())
			return;

		get_window().raise();
	}

	private void realize_cb(Gtk.Widget widget)
	{
		update_geometry();
	}

	private void refresh_window_cb(Vte.Terminal terminal)
	{
		queue_draw();
	}

	private void resize_window_cb(Vte.Terminal terminal, uint columns, uint rows)
	{
		if (columns < 2 || rows < 2)
			return;

		terminal.set_size((int)columns, (int)rows);
		resize_to_geometry((int)columns, (int)rows);
	}

	private void restore_window_cb(Vte.Terminal terminal)
	{
		unmaximize();
	}

	private void selection_changed_cb(Vte.Terminal terminal)
	{
		update_copy_sensitivity();
	}

	private void status_line_changed_cb(Vte.Terminal terminal)
	{
		print("Status: `%s'\n", terminal.get_status_line());
	}

	private void window_title_changed_cb(Vte.Terminal terminal)
	{
		set_title(terminal.get_window_title());
	}

} /* class Window */

class App : Gtk.Application
{
	private Window window;

	public App()
	{
		Object(application_id: "org.gnome.Vte.Test.App",
			   flags: ApplicationFlags.NON_UNIQUE);
	}

	protected override void startup()
	{
		base.startup();

		window = new Window(this);
		window.launch();
	}

	protected override void activate()
	{
		window.apply_geometry();
		window.present();
	}

	public struct Options
	{
		public static bool audible = false;
		public static string? command = null;
		private static string? cjk_ambiguous_width_string = null;
		private static string? cursor_blink_mode_string = null;
		private static string? cursor_color_string = null;
		private static string? cursor_shape_string = null;
		public static string[]? dingus = null;
		public static bool debug = false;
		public static string? encoding = null;
		public static string[]? environment = null;
		public static string? font_string = null;
		public static string? geometry = null;
		private static string? hl_bg_color_string = null;
		private static string? hl_fg_color_string = null;
		public static string? icon_title = null;
		public static bool keep = false;
		public static bool no_argb_visual = false;
		public static bool no_builtin_dingus = false;
		public static bool no_context_menu = false;
		public static bool no_double_buffer = false;
		public static bool no_geometry_hints = false;
		public static bool no_rewrap = false;
		public static bool no_shell = false;
		public static bool no_toolbar = false;
		public static bool object_notifications = false;
		public static string? output_filename = null;
		private static string? pty_flags_string = null;
		public static bool reverse = false;
		public static int scrollback_lines = 512;
		public static string? term = null;
		public static int transparency_percent = 0;
		public static bool version = false;
		public static string? working_directory = null;

		private static int parse_enum(Type type, string str)
		{
			int value = 0;
			EnumClass enum_klass = (EnumClass)type.class_ref();
			unowned EnumValue? enum_value = enum_klass.get_value_by_nick(str);
			if (enum_value != null)
				value = enum_value.value;
			else
				printerr("Failed to parse enum value \"%s\" as type \"%s\"\n",
						 str, type.qname().to_string());
			return value;
		}

		private static uint parse_flags(Type type, string str)
		{
			uint value = 0;
			var flags_klass = (FlagsClass)type.class_ref();
			string[]? flags = str.split(",|", -1);
		  
			if (flags == null)
				return value;

			for (int i = 0; i < flags.length; i++) {
				unowned FlagsValue? flags_value = flags_klass.get_value_by_nick(flags[i]);
				if (flags_value != null)
					value |= flags_value.value;
				else
					printerr("Failed to parse flags value \"%s\" as type \"%s\"\n",
							 str, type.qname().to_string());
			}
			return value;
		}

		public static int get_cjk_ambiguous_width()
		{
			if (cjk_ambiguous_width_string == null)
				return 1;
			if (cjk_ambiguous_width_string == "narrow")
				return 1;
			if (cjk_ambiguous_width_string == "wide")
				return 2;
			printerr("Failed to parse \"%s\" argument to --cjk-width. Allowed values are \"narrow\" or \"wide\".\n", cjk_ambiguous_width_string);
			return 1;
		}

		public static Gdk.RGBA get_color_bg()
		{
			var color = Gdk.RGBA();
			color.alpha = (double)(100 - transparency_percent.clamp(0, 100)) / 100.0;
			if (Options.reverse) {
				color.red = color.green = color.blue = 1.0;
			} else {
				color.red = color.green = color.blue = 0.0;
			}
			return color;
		}

		public static Gdk.RGBA get_color_fg()
		{
			var color = Gdk.RGBA();
			color.alpha = 1.0;
			if (Options.reverse) {
				color.red = color.green = color.blue = 0.0;
			} else {
				color.red = color.green = color.blue = 1.0;
			}
			return color;
		}

		private static Gdk.RGBA? get_color(string? str)
		{
			if (str == null)
				return null;
			var color = Gdk.RGBA();
			if (!color.parse(str)) {
				printerr("Failed to parse \"%s\" as color.\n", str);
				return null;
			}
			return color;
		}

		public static Gdk.RGBA? get_color_cursor()
		{
			return get_color(cursor_color_string);
		}

		public static Gdk.RGBA? get_color_hl_bg()
		{
			return get_color(hl_bg_color_string);
		}

		public static Gdk.RGBA? get_color_hl_fg()
		{
			return get_color(hl_fg_color_string);
		}

		public static Vte.CursorBlinkMode get_cursor_blink_mode()
		{
			Vte.CursorBlinkMode value;
			if (cursor_blink_mode_string != null)
				value = (Vte.CursorBlinkMode)parse_enum(typeof(Vte.CursorBlinkMode),
														cursor_blink_mode_string);
			else
				value = Vte.CursorBlinkMode.SYSTEM;
			return value;
		}

		public static Vte.CursorShape get_cursor_shape()
		{
			Vte.CursorShape value;
			if (cursor_shape_string != null)
				value = (Vte.CursorShape)parse_enum(typeof(Vte.CursorShape),
													cursor_shape_string);
			else
				value = Vte.CursorShape.BLOCK;
			return value;
		}

		public static Vte.PtyFlags get_pty_flags()
		{
			Vte.PtyFlags flags;
			if (pty_flags_string != null)
				flags = (Vte.PtyFlags)parse_flags(typeof(Vte.CursorShape),
												  pty_flags_string);
			else
				flags = Vte.PtyFlags.DEFAULT;
			return flags;
		}
		
		public static const OptionEntry[] entries = {
			{ "audible-bell", 'a', 0, OptionArg.NONE, ref audible,
			  "Use audible terminal bell", null },
			{ "command", 'c', 0, OptionArg.STRING, ref command,
			  "Execute a command in the terminal", null },
			{ "cjk-width", 0, 0, OptionArg.STRING, ref cjk_ambiguous_width_string,
			  "Specify the cjk ambiguous width to use for UTF-8 encoding", "NARROW|WIDE" },
			{ "cursor-blink", 0, 0, OptionArg.STRING, ref cursor_blink_mode_string,
			  "Cursor blink mode (system|on|off)", "MODE" },
			{ "cursor-color", 0, 0, OptionArg.STRING, ref cursor_color_string,
			  "Enable a colored cursor", null },
			{ "cursor-shape", 0, 0, OptionArg.STRING, ref cursor_shape_string,
			  "Set cursor shape (block|underline|ibeam)", null },
			{ "dingu", 'D', 0, OptionArg.STRING_ARRAY, ref dingus,
			  "Add regex highlight", null },
			{ "debug", 'd', 0,OptionArg.NONE, ref debug,
			  "Enable various debugging checks", null },
			{ "encoding", 0, 0, OptionArg.STRING, ref encoding,
			  "Specify the terminal encoding to use", null },
			{ "env", 0, 0, OptionArg.STRING_ARRAY, ref environment,
			  "Add environment variable to the child\'s environment", "VAR=VALUE" },
			{ "font", 'f', 0, OptionArg.STRING, ref font_string,
			  "Specify a font to use", null },
			{ "geometry", 'g', 0, OptionArg.STRING, ref geometry,
			  "Set the size (in characters) and position", "GEOMETRY" },
			{ "highlight-background-color", 0, 0, OptionArg.STRING, ref hl_bg_color_string,
			  "Enable distinct highlight background color for selection", null },
			{ "highlight-foreground-color", 0, 0, OptionArg.STRING, ref hl_fg_color_string,
			  "Enable distinct highlight foreground color for selection", null },
			{ "icon-title", 'i', 0, OptionArg.NONE, ref icon_title,
			  "Enable the setting of the icon title", null },
			{ "keep", 'k', 0, OptionArg.NONE, ref keep,
			  "Live on after the command exits", null },
			{ "no-argb-visual", 0, 0, OptionArg.NONE, ref no_argb_visual,
			  "Don't use an ARGB visual", null },
			{ "no-builtin-dingus", 0, 0, OptionArg.NONE, ref no_builtin_dingus,
			  "Highlight URLs inside the terminal", null },
			{ "no-context-menu", 0, 0, OptionArg.NONE, ref no_context_menu,
			  "Disable context menu", null },
			{ "no-double-buffer", '2', 0, OptionArg.NONE, ref no_double_buffer,
			  "Disable double-buffering", null },
			{ "no-geometry-hints", 'G', 0, OptionArg.NONE, ref no_geometry_hints,
			  "Allow the terminal to be resized to any dimension, not constrained to fit to an integer multiple of characters", null },
			{ "no-rewrap", 'R', 0, OptionArg.NONE, ref no_rewrap,
			  "Disable rewrapping on resize", null },
			{ "no-shell", 'S', 0, OptionArg.NONE, ref no_shell,
			  "Disable spawning a shell inside the terminal", null },
			{ "no-toolbar", 0, 0, OptionArg.NONE, ref no_toolbar,
			  "Disable toolbar", null },
			{ "object-notifications", 'N', 0, OptionArg.NONE, ref object_notifications,
			  "Print VteTerminal object notifications", null },
			{ "output-file", 0, 0, OptionArg.FILENAME, ref output_filename,
			  "Save terminal contents to file at exit", null },
			{ "pty-flags", 0, 0, OptionArg.STRING, ref pty_flags_string,
			  "PTY flags set from default|no-utmp|no-wtmp|no-lastlog|no-helper|no-fallback", null },
			{ "reverse", 0, 0, OptionArg.NONE, ref reverse,
			  "Reverse foreground/background colors", null },
			{ "scrollback-lines", 'n', 0, OptionArg.INT, ref scrollback_lines,
			  "Specify the number of scrollback-lines", null },
			{ "term", 't', 0, OptionArg.STRING, ref term,
			  "Specify the terminal emulation to use", null },
			{ "transparent", 'T', 0, OptionArg.INT, ref transparency_percent,
			  "Enable the use of a transparent background", "0..100" },
			{ "version", 0, 0, OptionArg.NONE, ref version,
			  "Show version", null },
			{ "working-directory", 'w', 0,OptionArg.FILENAME, ref working_directory,
			  "Specify the initial working directory of the terminal", null },
			{ null }
		};
	}

	public static int main(string[] argv)
	{
		if (Environment.get_variable("VTE_CJK_WIDTH") != null) {
			printerr("VTE_CJK_WIDTH is not supported anymore, use --cjk-width instead\n");
		}
		Environment.set_prgname("vte-app");
		Environment.set_application_name("Terminal");

		try {
			var context = new OptionContext("— simple VTE test application");
			context.set_help_enabled(true);
			context.add_main_entries(Options.entries, null);
			context.add_group(Gtk.get_option_group(true));
			context.parse(ref argv);
		} catch (OptionError e) {
			printerr("Error parsing arguments: %s\n", e.message);
			return 1;
		}

		if (Options.version) {
			print("Simple VTE Test Application %s\n", Config.VERSION);
			return 0;
		}

		if (Options.debug)
			Gdk.Window.set_debug_updates(Options.debug);

		var app = new App();
		return app.run(null);
	}
} /* class App */

} /* namespace */
