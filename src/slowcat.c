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

#include "../config.h"
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>

static void
catfile(const char *pathname, long delay, long chunksize)
{
	FILE *fp;
	struct timeval tv;
	char *buf;
	int c;
	long i;

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

	buf = g_malloc(chunksize);

	while (!feof(fp)) {
		tv.tv_sec = delay / 1000000;
		tv.tv_usec = delay % 1000000;
		select(0, NULL, NULL, NULL, &tv);
		for (i = 0; i < chunksize; i++) {
			c = fgetc(fp);
			if (c != EOF) {
				buf[i] = c;
			} else {
				break;
			}
		}
		if (i > 0) {
			write(STDOUT_FILENO, buf, i);
			fsync(STDOUT_FILENO);
		}
	}

	g_free(buf);

	if (fp != stdin) {
		fclose(fp);
	}
}

int
main(int argc, char **argv)
{
	int i, c;
	long delay = 200000, chunksize = 1, tmp;
	char *p;
	GList *files = NULL, *file;

	while ((c = getopt(argc, argv, "t:c:")) != -1) {
		switch (c) {
		case 't':
			tmp = strtol(optarg, &p, 0);
			if ((p != NULL) && (*p == '\0')) {
				delay = tmp;
			}
			break;
		case 'c':
			tmp = strtol(optarg, &p, 0);
			if ((p != NULL) && (*p == '\0')) {
				chunksize = tmp;
			}
			break;
		default:
			fprintf(stderr, "Usage: slowcat [-t delay] [-c chunksize] [file ...]\n");
			exit(1);
			break;
		}
	}
	for (i = optind; i < argc; i++) {
		files = g_list_append(files, argv[i]);
	}

	if (files) {
		for (file = files; file != NULL; file = g_list_next(file)) {
			catfile((const char*)file->data, delay, chunksize);
		}
	} else {
		catfile(NULL, delay, chunksize);
	}
	return 0;
}
