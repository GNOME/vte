/*
 * Copyright (C) 2001,2002,2003 Red Hat, Inc.
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
#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <glib-object.h>
#include "caps.h"
#include "debug.h"
#include "iso2022.h"
#include "matcher.h"
#include "vtetc.h"

int
main(int argc, char **argv)
{
	char *terminal = NULL;
	struct _vte_matcher *matcher = NULL;
	struct _vte_termcap *termcap = NULL;
	struct _vte_buffer *buffer = NULL;
	GArray *array;
	unsigned int i, j;
	int l;
	char b;
	GValue *value;
	FILE *infile = NULL;
	struct _vte_iso2022_state *subst;
	const char *tmp;
	GQuark quark;
	GValueArray *values;

	_vte_debug_init();

	if (argc < 2) {
		g_print("usage: %s terminal [file]\n", argv[0]);
		return 1;
	}

	if ((argc > 2) && (strcmp(argv[2], "-") != 0)) {
		infile = fopen(argv[2], "r");
		if (infile == NULL) {
			g_print("error opening %s: %s\n", argv[2],
				strerror(errno));
			exit(1);
		}
	} else {
		infile = stdin;
	}

	g_type_init();
	terminal = argv[1];
	termcap = _vte_termcap_new(g_strdup_printf(DATADIR "/" PACKAGE
						   "/termcap/%s", terminal));
	if (termcap == NULL) {
		termcap = _vte_termcap_new("/etc/termcap");
	}
	buffer = _vte_buffer_new();
	array = g_array_new(FALSE, FALSE, sizeof(gunichar));

	matcher = _vte_matcher_new(terminal, termcap);

	subst = _vte_iso2022_state_new(NULL, NULL, NULL);

	while (fread(&b, 1, 1, infile) == 1) {
		_vte_buffer_append(buffer, &b, 1);
	}
	_vte_iso2022_process(subst, buffer->bytes,
			_vte_buffer_length(buffer), array);

	i = 0;
	while (i <= array->len) {
		tmp = NULL;
		values = NULL;
		for (j = 1; j < (array->len - i); j++) {
			_vte_matcher_match(matcher,
					   &g_array_index(array, gunichar, i),
					   j,
					   &tmp,
					   NULL,
					   &quark,
					   &values);
			if ((tmp == NULL) || (strlen(tmp) > 0)) {
				break;
			}
		}
		if (i + j == array->len) {
			g_print("End of data.\n");
			break;
		}
		if (tmp == NULL) {
			gunichar c;
			c = g_array_index(array, gunichar, i);
			if (VTE_ISO2022_HAS_ENCODED_WIDTH(c)) {
				c &= ~VTE_ISO2022_ENCODED_WIDTH_MASK;
			}
			if (c < 32) {
				g_print("`^%c'\n", c + 64);
			} else
			if (c < 127) {
				g_print("`%c'\n", c);
			} else {
				g_print("`0x%x'\n", c);
			}
			i++;
			continue;
		}

		l = j;
		g_print("%s(", g_quark_to_string(quark));
		for (j = 0; (values != NULL) && (j < values->n_values); j++) {
			if (j > 0) {
				g_print(", ");
			}
			value = g_value_array_get_nth(values, j);
			if (G_VALUE_HOLDS_LONG(value)) {
				g_print("%ld", g_value_get_long(value));
			}
			if (G_VALUE_HOLDS_STRING(value)) {
				g_print("`%s'",
				        g_value_get_string(value));
			}
			if (G_VALUE_HOLDS_POINTER(value)) {
				g_print("`%ls'",
				        (wchar_t*)
				        g_value_get_pointer(value));
			}
		}
		if (values != NULL) {
			_vte_matcher_free_params_array(matcher, values);
		}
		g_print(")\n");
		i += l;
	}

	if (infile != stdin) {
		fclose(infile);
	}

	_vte_iso2022_state_free(subst);
	_vte_buffer_free(buffer);
	g_array_free(array, TRUE);
	_vte_termcap_free(termcap);
	_vte_matcher_free(matcher);
	return 0;
}
