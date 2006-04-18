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

#include "../config.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "table.h"
#define ESC "\033"

int
main(int argc, char **argv)
{
	int i;
	long l;
	char *p;

	if (argc < 2) {
		printf("usage: %s index [...]\n", argv[0]);
		return 1;
	}

	for (i = 1; i < argc; i++) {
		l = strtol(argv[i], &p, 0);
		do {
			printf("%c", (unsigned char) (l & 0xff));
			l = l >> 8;
		} while (l > 0);
	}

	return 0;
}
