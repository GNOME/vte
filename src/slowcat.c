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
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

static void
catfile(const char *pathname, long delay)
{
	FILE *fp;
	struct timeval tv;
	int c;

	if (!((pathname == NULL) || (strcmp(pathname, "-") == 0))) {
		fp = fopen(pathname, "r");
		if (fp == NULL) {
			g_warning("Error opening file `%s': %s.\n",
				  pathname, strerror(errno));
			return;
		}
	} else {
		fp = stdin;
	}

	while (!feof(fp)) {
		tv.tv_sec = delay / 1000000;
		tv.tv_usec = delay % 1000000;
		select(0, NULL, NULL, NULL, &tv);
		c = fgetc(fp);
		if (c != EOF) {
			fputc(c, stdout);
		}
		fflush(stdout);
	}

	if (fp != stdin) {
		fclose(fp);
	}
}

int
main(int argc, char **argv)
{
	int i;
	long delay = 200000, tmp;
	char *p;
	GList *files = NULL, *file;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == '-') {
				break;
			}
			tmp = strtol(argv[i] + 1, &p, 0);
			g_assert(p != NULL);
			if (*p == '\0') {
				delay = tmp;
			} else {
				files = g_list_append(files, argv[i]);
			}
		} else {
			files = g_list_append(files, argv[i]);
		}
	}

	if (files) {
		for (file = files; file != NULL; file = g_list_next(file)) {
			catfile((const char*)file->data, delay);
		}
	} else {
		catfile(NULL, delay);
	}
	return 0;
}
