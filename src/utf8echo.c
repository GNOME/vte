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
#include <iconv.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ESC ""

int
main(int argc, char **argv)
{
	int i;
	iconv_t conv;
	char buf[LINE_MAX];
	wchar_t w;
	char *inbuf, *outbuf;
	size_t insize, outsize;

	if (argc < 2) {
		printf("usage: %s index [...]\n", argv[0]);
		return 1;
	}

	conv = iconv_open("UTF-8", "WCHAR_T");
	if (conv == NULL) {
		return 1;
	}

	printf(ESC "%%G");
	for (i = 1; i < argc; i++) {
		w = (wint_t)atol(argv[i]);
		inbuf = (char*)&w;
		insize = sizeof(w);
		memset(buf, 0, sizeof(buf));
		outbuf = buf;
		outsize = sizeof(buf);
		if (iconv(conv, &inbuf, &insize, &outbuf, &outsize) != -1) {
			printf("%*s", outbuf - buf, buf);
		}
	}
	printf(ESC "%%@\n");

	iconv_close(conv);

	return 0;
}
