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
#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <langinfo.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <glib-object.h>
#include "caps.h"
#include "debug.h"
#include "termcap.h"
#include "table.h"

int
main(int argc, char **argv)
{
	char *terminal = NULL;
	struct vte_table *table = NULL;
	struct vte_termcap *termcap = NULL;
	GByteArray *array = NULL;
	int i;
	char c;
	GValue *value;
	FILE *infile = NULL;

	vte_debug_parse_string(getenv("VTE_DEBUG_FLAGS"));

	if (argc < 2) {
		printf("usage: %s terminal [file]\n", argv[0]);
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
	table = vte_table_new();
	termcap = vte_termcap_new("/etc/termcap");
	array = g_byte_array_new();

	for (i = 0;
	     vte_terminal_capability_strings[i].capability != NULL;
	     i++) {
		const char *capability;
		char *tmp;
		capability = vte_terminal_capability_strings[i].capability;
		if (vte_terminal_capability_strings[i].key) {
			continue;
		}
		tmp = vte_termcap_find_string(termcap, terminal, capability);
		if ((tmp != NULL) && (strlen(tmp) > 0)) {
			vte_table_add(table, tmp, strlen(tmp), capability,
				      g_quark_from_static_string(capability));
		}
		g_free(tmp);
	}
	for (i = 0; vte_xterm_capability_strings[i].value != NULL; i++) {
		const char *code, *value;
		code = vte_xterm_capability_strings[i].code;
		value = vte_xterm_capability_strings[i].value;
		vte_table_add(table, code, strlen(code), value,
			      g_quark_from_static_string(code));
	}

	while (fread(&c, 1, 1, infile) == 1) {
		g_byte_array_append(array, &c, 1);
		for (i = 1; i <= array->len; i++) {
			const char *tmp;
			GQuark quark;
			GValueArray *values;
			GError *error = NULL;
			gunichar *ubuf;
			gsize ubuflen;
			ubuf = (gunichar*) g_convert(array->data, i,
						     vte_table_wide_encoding(),
						     "UTF-8",
						     NULL, &ubuflen, &error);
			if (error != NULL) {
				g_print("%s\n",
					error->message ? error->message : "?");
				g_clear_error(&error);
			}
			vte_table_match(table, ubuf, ubuflen / sizeof(gunichar),
				        &tmp, NULL, &quark, &values);
			if (tmp != NULL) {
				if (strlen(tmp) > 0) {
					int j;
					printf("%s(", g_quark_to_string(quark));
					for (j = 0; (values != NULL) && (j < values->n_values); j++) {
						if (j > 0) {
							printf(", ");
						}
						value = g_value_array_get_nth(values, j);
						if (G_VALUE_HOLDS_LONG(value)) {
							printf("%ld",
								g_value_get_long(value));
						}
						if (G_VALUE_HOLDS_STRING(value)) {
							printf("`%s'",
								g_value_get_string(value));
						}
						if (G_VALUE_HOLDS_POINTER(value)) {
							printf("`%ls'",
							       g_value_get_pointer(value));
						}
					}
					if (values != NULL) {
						g_value_array_free(values);
					}
					for (j = 0; j < i; j++) {
						g_byte_array_remove_index(array, 0);
					}
					printf(")\n");
					break;
				}
			} else {
				while (array->len > 0) {
					printf("`%c'\n", array->data[0]);
					g_byte_array_remove_index(array, 0);
				}
			}
		}
	}

	if (infile != stdin) {
		fclose(infile);
	}

	g_byte_array_free(array, TRUE);
	vte_termcap_free(termcap);
	vte_table_free(table);
	return 0;
}
