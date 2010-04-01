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

#include <config.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>
#include <glib.h>

#define ESC "\033"
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

/* Output the DEC private mode set sequence. */
static void
decset(int mode, gboolean value)
{
	g_print(ESC "[?%d%c", mode, value ? 'h' : 'l');
}

/* Move the cursor to the upper left corner of the screen. */
static void
home(void)
{
	g_print(ESC "[1;1H");
}

/* Clear the screen. */
static void
clear(void)
{
	g_print(ESC "[2J");
	home();
}

/* Print the what-does-this-key-do help messages and current status. */
static void
print_help(void)
{
	g_print(ESC "[m");
	home();
	g_print(ESC "[K" "A - KEYPAD ");
	if (keypad_mode == application) {
		g_print("APPLICATION\r\n");
	} else {
		g_print("NORMAL\r\n");
	}
	g_print(ESC "[K" "B - CURSOR ");
	if (cursor_mode == application) {
		g_print("APPLICATION\r\n");
	} else {
		g_print("NORMAL\r\n");
	}
	g_print(ESC "[K" "C - SUN    ");
	if (sun_fkeys) {
		g_print("TRUE\r\n");
	} else {
		g_print("FALSE\r\n");
	}
	g_print(ESC "[K" "D - HP     ");
	if (hp_fkeys) {
		g_print("TRUE\r\n");
	} else {
		g_print("FALSE\r\n");
	}
	g_print(ESC "[K" "E - XTERM  ");
	if (xterm_fkeys) {
		g_print("TRUE\r\n");
	} else {
		g_print("FALSE\r\n");
	}
	g_print(ESC "[K" "F - VT220  ");
	if (vt220_fkeys) {
		g_print("TRUE\r\n");
	} else {
		g_print("FALSE\r\n");
	}
	g_print(ESC "[K" "R - RESET\r\n");
	g_print(ESC "[K" "Q - QUIT\r\n");
}

/* Reset the scrolling region, so that the entire screen becomes
 * addressable again. */
static void
reset_scrolling_region(void)
{
	g_print(ESC "[r");
}

/* Set the scrolling region, so that the help/status at the top of the
 * screen doesn't scroll off. */
static void
set_scrolling_region(void)
{
	g_print(ESC "[10;24r");
	g_print(ESC "[9;1H");
}

/* Save the current location of the cursor in the terminal's memory. */
static void
save_cursor(void)
{
	g_print(ESC "7");
}

/* Restore the cursor to the location stored in the terminal's memory. */
static void
restore_cursor(void)
{
	g_print(ESC "8");
}

/* Reset all of the keyboard modes. */
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

/* Cleanly exit. */
G_GNUC_NORETURN static void
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
	guint i;
	struct termios tcattr;
	GByteArray *bytes;
	gboolean done = FALSE, saved = FALSE;
	struct timeval tv;
	fd_set readset;

	/* Start up: save the cursor location and put the terminal in
	 * raw mode. */
	bytes = g_byte_array_new();
	save_cursor();
	if (tcgetattr(STDIN_FILENO, &tcattr) != 0) {
		perror("tcgetattr");
		return 1;
	}
	original = tcattr;
	signal(SIGINT, sigint_handler);
	/* Here we approximate what cfmakeraw() would do, for the benefit
	 * of systems which don't actually provide the function. */
	tcattr.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
			    INLCR | IGNCR | ICRNL | IXON);
	tcattr.c_oflag &= ~(OPOST);
	tcattr.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tcattr.c_cflag &= ~(CSIZE | PARENB);
	tcattr.c_cflag |= CS8;
#ifdef HAVE_CFMAKERAW
	cfmakeraw(&tcattr);
#endif
	if (tcsetattr(STDIN_FILENO, TCSANOW, &tcattr) != 0) {
		perror("tcsetattr");
		return 1;
	}

	/* Switch to the alternate screen, clear it, and reset the keyboard. */
	decset(MODE_ALTERNATE_SCREEN, TRUE);
	clear();
	reset();

	/* Main processing loop. */
	while (!done) {
		print_help();
		set_scrolling_region();
		if (saved) {
			restore_cursor();
		}

		/* Read a single byte. */
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
		case 'R':
		case 'r':
			keypad_mode = cursor_mode = normal;
			sun_fkeys = hp_fkeys = xterm_fkeys = vt220_fkeys = FALSE;
			reset();
			break;
		case 'Q':
		case 'q':
			done = TRUE;
			break;
		case 0x0c: /* ^L */
			clear();
			if (saved) {
				restore_cursor();
				saved = FALSE;
			}
			break;
		default:
			/* We get here if it's not one of the keys we care
			 * about, so it might be a sequence. */
			if (saved) {
				restore_cursor();
			}
			g_byte_array_append(bytes, (const guint8 *) &c, 1);
			/* Wait for up to just under 1/50 second. */
			tv.tv_sec = 0;
			tv.tv_usec = 1000000 / 50;
			FD_ZERO(&readset);
			FD_SET(STDIN_FILENO, &readset);
			while (select(STDIN_FILENO + 1,
				      &readset, NULL, NULL, &tv) == 1) {
				if (read(STDIN_FILENO, &c, 1) == 1) {
					g_byte_array_append(bytes, (const guint8 *) &c, 1);
				} else {
					break;
				}
				tv.tv_sec = 0;
				tv.tv_usec = 1000000 / 50;
				FD_ZERO(&readset);
				FD_SET(STDIN_FILENO, &readset);
			}
			/* Clear this line, and print the sequence. */
			g_print(ESC "[K");
			for (i = 0; i < bytes->len; i++) {
				if (bytes->data[i] == 27) {
					g_print("<ESC> ");
				} else
				if ((((guint8)bytes->data[i]) < 32) ||
				    (((guint8)bytes->data[i]) > 126)) {
					g_print("<0x%02x> ", bytes->data[i]);
				} else {
					g_print("`%c' ", bytes->data[i]);
				}
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
