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
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>
#include <glib.h>
#include "caps.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

static enum {
	tracking_x10 = 9,
	tracking_mouse = 1000,
	tracking_hilite = 1001,
	tracking_cell_motion = 1002,
	tracking_all_motion = 1003
} tracking_mode = 0;

static void
decset(int mode, gboolean value)
{
	fprintf(stdout, _VTE_CAP_CSI "?%d%c", mode, value ? 'h' : 'l');
}

static void
reset(void)
{
	decset(tracking_x10, FALSE);
	decset(tracking_mouse, FALSE);
	decset(tracking_hilite, FALSE);
	decset(tracking_cell_motion, FALSE);
	decset(tracking_all_motion, FALSE);
	fflush(stdout);
}

static void
clear(void)
{
	fprintf(stdout, "%s",
		_VTE_CAP_ESC "7"
		_VTE_CAP_CSI "8;1H"
		_VTE_CAP_CSI "1J"
		_VTE_CAP_CSI "2K"
		_VTE_CAP_CSI "1;1H");
	reset();
	switch (tracking_mode) {
	case tracking_x10:
		fprintf(stdout, "X10 tracking enabled.\r\n");
		decset(tracking_x10, TRUE);
		break;
	case tracking_mouse:
		fprintf(stdout, "Mouse tracking enabled.\r\n");
		decset(tracking_mouse, TRUE);
		break;
	case tracking_hilite:
		fprintf(stdout, "Hilite tracking enabled.\r\n");
		decset(tracking_hilite, TRUE);
		break;
	case tracking_cell_motion:
		fprintf(stdout, "Cell motion tracking enabled.\r\n");
		decset(tracking_cell_motion, TRUE);
		break;
	case tracking_all_motion:
		fprintf(stdout, "All motion tracking enabled.\r\n");
		decset(tracking_all_motion, TRUE);
		break;
	default:
		fprintf(stdout, "Tracking disabled.\r\n");
		break;
	}
	fprintf(stdout, "A - X10.\r\n");
	fprintf(stdout, "B - Mouse tracking.\r\n");
	fprintf(stdout, "C - Hilite tracking [FIXME: NOT IMPLEMENTED].\r\n");
	fprintf(stdout, "D - Cell motion tracking.\r\n");
	fprintf(stdout, "E - All motion tracking.\r\n");
	fprintf(stdout, "%s", _VTE_CAP_ESC "8");
	fflush(stdout);
}

static gboolean
parse(void)
{
	GByteArray *bytes;
	gchar buffer[64];
	int i, length;
	guchar b;
	gboolean ret = FALSE;

	bytes = g_byte_array_new();
	if ((length = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
		g_byte_array_append(bytes, buffer, length);
	}

	i = 0;
	while (i < bytes->len) {
		switch (bytes->data[i]) {
		case 'A':
		case 'a':
			tracking_mode = (tracking_mode == tracking_x10) ?
					0 : tracking_x10;
			i++;
			break;
		case 'B':
		case 'b':
			tracking_mode = (tracking_mode == tracking_mouse) ?
					0 : tracking_mouse;
			i++;
			break;
		case 'C':
		case 'c':
			tracking_mode = (tracking_mode == tracking_hilite) ?
					0 : tracking_hilite;
			i++;
			break;
		case 'D':
		case 'd':
			tracking_mode = (tracking_mode == tracking_cell_motion) ?
					0 : tracking_cell_motion;
			i++;
			break;
		case 'E':
		case 'e':
			tracking_mode = (tracking_mode == tracking_all_motion) ?
					0 : tracking_all_motion;
			i++;
			break;
		case 'Q':
		case 'q':
			ret = TRUE;
			i++;
			break;
		case '\033':
			if (bytes->len - i >= 6) {
				int button = 0;
				const char *shift = "", *control = "",
					   *meta = "";
				gboolean motion = FALSE;
				int x, y;
				if ((bytes->data[i + 0] == '\033') &&
				    (bytes->data[i + 1] == '[')) {
					if (bytes->data[i + 2] == 'M') {
						b = bytes->data[i + 3] - 32;
						switch (b & 3) {
						case 0:
							button = 1;
							if (b & 64) {
								button += 3;
							}
							break;
						case 1:
							button = 2;
							if (b & 64) {
								button += 3;
							}
							break;
						case 2:
							button = 3;
							if (b & 64) {
								button += 3;
							}
							break;
						case 3:
							button = 0;
							break;
						}
						shift = b & 4 ?
							"[shift]" :
							"";
						meta = b & 8 ?
						       "[meta]" :
						       "";
						control = b & 16 ?
							  "[control]" :
							  "";
						motion = (b & 32) != 0;
						x = bytes->data[i + 4] - 32;
						y = bytes->data[i + 5] - 32;
						fprintf(stdout, "%d %s%s%s(%s%s%s) at %d,%d\r\n",
							button,
							motion ? "motion " : "",
							(!motion && button) ? "press" : "",
							(!motion && !button) ? "release" : "",
							meta, control, shift,
							x, y);
					}
				}
				i += 6;
				break;
			}
		default:
			while (i < length) {
				if (bytes->data[i] < 32) {
					fprintf(stdout, "'^%c' ",
						bytes->data[i] | 64);
				} else {
					fprintf(stdout, "'%c' ",
						bytes->data[i]);
				}
				i++;
			}
			fprintf(stdout, "\r\n");
			break;
		}
	}
	fflush(stdout);
	g_byte_array_free(bytes, TRUE);

	return ret;
}

static struct termios tcattr, original;

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
	int flags;
	gboolean stop;
	fd_set in_fds;

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

	flags = fcntl(STDIN_FILENO, F_GETFL);
	fcntl(STDIN_FILENO, F_SETFL, flags & ~(O_NONBLOCK));
	fprintf(stdout, "%s",
		_VTE_CAP_CSI "9;1H"
		_VTE_CAP_CSI "2K"
		_VTE_CAP_CSI "2J");
	do {
		clear();
		FD_ZERO(&in_fds);
		FD_SET(STDIN_FILENO, &in_fds);
		stop = TRUE;
		switch (select(STDIN_FILENO + 1, &in_fds, NULL, NULL, NULL)) {
		case 1:
			stop = parse();
			break;
		default:
			stop = TRUE;
			break;
		}
	} while (!stop);
	reset();
	fcntl(STDIN_FILENO, F_SETFL, flags);

	if (tcsetattr(STDIN_FILENO, TCSANOW, &original) != 0) {
		perror("tcsetattr");
		return 1;
	}

	return 0;
}
