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
#include <glib.h>
#include <limits.h>
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "matcher.h"
#include "vteconv.h"
#define ESC "\033"

int
main(int argc, char **argv)
{
	int i;
	VteConv conv;
	char buf[LINE_MAX];
	wchar_t w;
	char *inbuf, *outbuf, *p;
	size_t insize, outsize;

	if (argc < 2) {
		printf("usage: [-r] %s index [...]\n", argv[0]);
		printf("        -r  reset to default terminal encoding "
		       "when finished\n");
		return 1;
	}

	conv = _vte_conv_open("UTF-8", VTE_CONV_GUNICHAR_TYPE);
	if (conv == VTE_INVALID_CONV) {
		return 1;
	}

	for (i = 1; i < argc; i++) {
		w = (wchar_t)strtol(argv[i], &p, 0);
		inbuf = (char*)&w;
		insize = sizeof(w);
		memset(buf, 0, sizeof(buf));
		outbuf = buf;
		outsize = sizeof(buf);
		if (_vte_conv(conv, &inbuf, &insize, &outbuf, &outsize) != -1) {
			printf("%.*s", (int)(outbuf - buf), buf);
		}
	}

	_vte_conv_close(conv);

	return 0;
}
