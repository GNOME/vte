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

#ident "$Id$"
#include "../config.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <glib.h>

#define ESC ""
#define MODE_APPLICATION_KEYPAD		ESC "="
#define MODE_NORMAL_KEYPAD		ESC ">"
#define MODE_APPLICATION_CURSOR_KEYS	1
#define MODE_SUN_FUNCTION_KEYS		1051
#define MODE_HP_FUNCTION_KEYS		1052
#define MODE_XTERM_FUNCTION_KEYS	1060
#define MODE_VT220_FUNCTION_KEYS	1061
#define MODE_ALTERNATE_SCREEN		1047

enum {
	normal = 0, application = 1
} keypad_mode = normal, cursor_mode = normal;
gboolean sun_fkeys = FALSE, hp_fkeys = FALSE,
	 xterm_fkeys = FALSE, vt220_fkeys = FALSE;
struct termios original;

static void
decset(int mode, gboolean value)
{
	g_print(ESC "[?%d%c", mode, value ? 'h' : 'l');
}

static void
home(void)
{
       	g_print(ESC "[1;1H");
}

static void
clear(void)
{
	g_print(ESC "[2J");
	home();
}

static void
print_help(void)
{
	home();
	g_print(ESC "[K" "A - KEYPAD ");
	if (keypad_mode == application) {
		g_print("APPLICATION\n");
	} else {
		g_print("NORMAL\n");
	}
	g_print(ESC "[K" "B - CURSOR ");
	if (cursor_mode == application) {
		g_print("APPLICATION\n");
	} else {
		g_print("NORMAL\n");
	}
	g_print(ESC "[K" "C - SUN    ");
	if (sun_fkeys) {
		g_print("TRUE\n");
	} else {
		g_print("FALSE\n");
	}
	g_print(ESC "[K" "D - HP     ");
	if (hp_fkeys) {
		g_print("TRUE\n");
	} else {
		g_print("FALSE\n");
	}
	g_print(ESC "[K" "E - XTERM  ");
	if (xterm_fkeys) {
		g_print("TRUE\n");
	} else {
		g_print("FALSE\n");
	}
	g_print(ESC "[K" "F - VT220  ");
	if (vt220_fkeys) {
		g_print("TRUE\n");
	} else {
		g_print("FALSE\n");
	}
	g_print(ESC "[K" "Q - QUIT\n");
}

static void
reset_scrolling_region(void)
{
	g_print(ESC "[r");
}

static void
set_scrolling_region(void)
{
	g_print(ESC "[9;24r");
	g_print(ESC "[9;1H");
}

static void
save_cursor(void)
{
	g_print(ESC "7");
}

static void
restore_cursor(void)
{
	g_print(ESC "8");
}

static void
reset(void)
{
	g_print(MODE_NORMAL_KEYPAD);
	decset(MODE_APPLICATION_CURSOR_KEYS, FALSE);
	decset(MODE_SUN_FUNCTION_KEYS, FALSE);
	decset(MODE_HP_FUNCTION_KEYS, FALSE);
	decset(MODE_XTERM_FUNCTION_KEYS, FALSE);
	decset(MODE_VT220_FUNCTION_KEYS, FALSE);
	reset_scrolling_region();
	restore_cursor();
}

static void
sigint_handler(int signum)
{
	if (tcsetattr(STDIN_FILENO, TCSANOW, &original) != 0) {
		perror("tcsetattr");
	}
	reset();
	_exit(1);
}

int
main(int argc, char **argv)
{
	char c;
	int flags, i;
	struct termios tcattr;
	GByteArray *bytes;
	gboolean done = FALSE, saved = FALSE;

	bytes = g_byte_array_new();
	save_cursor();
	if (tcgetattr(STDIN_FILENO, &tcattr) != 0) {
		perror("tcgetattr");
		return 1;
	}
	original = tcattr;
	signal(SIGINT, sigint_handler);
	tcattr.c_lflag = tcattr.c_lflag & ~(ICANON | ECHO);
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tcattr) != 0) {
		perror("tcsetattr");
		return 1;
	}

	decset(MODE_ALTERNATE_SCREEN, TRUE);
	clear();
	reset();

	while (!done) {
		print_help();
		set_scrolling_region();
		if (saved) {
			restore_cursor();
		}

		if (read(STDIN_FILENO, &c, 1) != 1) {
			done = TRUE;
		}
		switch (c) {
		case 'A':
		case 'a':
			keypad_mode = 1 - keypad_mode;
			if (keypad_mode == normal) {
				g_print(MODE_NORMAL_KEYPAD);
			} else {
				g_print(MODE_APPLICATION_KEYPAD);
			}
			break;
		case 'B':
		case 'b':
			cursor_mode = 1 - cursor_mode;
			decset(MODE_APPLICATION_CURSOR_KEYS,
			       cursor_mode == application);
			break;
		case 'C':
		case 'c':
			sun_fkeys = !sun_fkeys;
			decset(MODE_SUN_FUNCTION_KEYS, sun_fkeys);
			break;
		case 'D':
		case 'd':
			hp_fkeys = !hp_fkeys;
			decset(MODE_HP_FUNCTION_KEYS, hp_fkeys);
			break;
		case 'E':
		case 'e':
			xterm_fkeys = !xterm_fkeys;
			decset(MODE_XTERM_FUNCTION_KEYS, xterm_fkeys);
			break;
		case 'F':
		case 'f':
			vt220_fkeys = !vt220_fkeys;
			decset(MODE_VT220_FUNCTION_KEYS, vt220_fkeys);
			break;
		case 'Q':
		case 'q':
			done = TRUE;
			break;
		default:
			if (saved) {
				restore_cursor();
			}
			g_byte_array_append(bytes, &c, 1);
			flags = fcntl(STDIN_FILENO, F_GETFL);
			fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
			while (read(STDIN_FILENO, &c, 1) == 1) {
				g_byte_array_append(bytes, &c, 1);
			}
			fcntl(STDIN_FILENO, F_SETFL, flags);
			g_print(ESC "[K");
			for (i = 0; i < bytes->len; i++)
			switch (bytes->data[i]) {
			case 27:
				g_print("<ESC> ");
				break;
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
			case 16:
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22:
			case 23:
			case 24:
			case 25:
			case 26:
			case 28:
			case 29:
			case 30:
			case 31:
				g_print("<0x%02x> ", bytes->data[i]);
				break;
			default:
				g_print("`%c' ", bytes->data[i]);
				break;
			}
			g_print("\r\n");
			g_byte_array_set_size(bytes, 0);
			save_cursor();
			saved = TRUE;
			break;
		}
		reset_scrolling_region();
	}

	decset(MODE_ALTERNATE_SCREEN, FALSE);

	if (tcsetattr(STDIN_FILENO, TCSANOW, &original) != 0) {
		perror("tcsetattr");
		return 1;
	}

	g_byte_array_free(bytes, TRUE);

	reset();

	return 0;
}
