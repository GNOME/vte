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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <glib-object.h>
#include "caps.h"
#include "termcap.h"
#include "trie.h"

static void
convert_mbstowcs(const char *i, size_t ilen, wchar_t *o, size_t *olen)
{
	mbstate_t state;
	memset(&state, 0, sizeof(state));
	*olen = mbsrtowcs(o, &i, ilen, &state);
}

int
main(int argc, char **argv)
{
	char *terminal = NULL;
	struct vte_trie *trie = NULL;
	struct vte_termcap *termcap = NULL;
	GByteArray *array = NULL;
	int i;
	char c;
	GValue *value;
	FILE *infile = NULL;

	if (argc < 2) {
		g_print("usage: %s terminal [file]\n", argv[0]);
		return 1;
	}

	if (argc > 2) {
		infile = fopen(argv[2], "r");
	} else {
		infile = stdin;
	}

	g_type_init();
	terminal = argv[1];
	trie = vte_trie_new();
	termcap = vte_termcap_new("/etc/termcap");
	array = g_byte_array_new();

	for (i = 0;
	     vte_terminal_capability_strings[i].capability != NULL;
	     i++) {
		const char *capability;
		char *tmp;
		capability = vte_terminal_capability_strings[i].capability;
		tmp = vte_termcap_find_string(termcap, terminal, capability);
		if ((tmp != NULL) && (strlen(tmp) > 0)) {
			vte_trie_add(trie, tmp, strlen(tmp), capability,
				     g_quark_from_static_string(capability));
		}
		g_free(tmp);
	}
	for (i = 0; vte_xterm_capability_strings[i].value != NULL; i++) {
		const char *code, *value;
		code = vte_xterm_capability_strings[i].code;
		value = vte_xterm_capability_strings[i].value;
		vte_trie_add(trie, code, strlen(code), value,
			     g_quark_from_static_string(code));
	}

	while (fread(&c, 1, 1, infile) == 1) {
		g_byte_array_append(array, &c, 1);
		for (i = 1; i <= array->len; i++) {
			const char *tmp;
			GQuark quark;
			GValueArray *values;
			wchar_t wbuf[LINE_MAX];
			size_t wbuflen;
			convert_mbstowcs(array->data, i, wbuf, &wbuflen);
			vte_trie_match(trie, wbuf, wbuflen,
				       &tmp, &quark, &values);
			if (tmp != NULL) {
				if (strlen(tmp) > 0) {
					int j;
					g_print("%s(", g_quark_to_string(quark));
					for (j = 0; (values != NULL) && (j < values->n_values); j++) {
						if (j > 0) {
							g_print(", ");
						}
						value = g_value_array_get_nth(values, j);
						if (G_VALUE_HOLDS_LONG(value)) {
							g_print("%ld",
								g_value_get_long(value));
						}
						if (G_VALUE_HOLDS_STRING(value)) {
							g_print("`%s'",
								g_value_get_string(value));
						}
					}
					if (values != NULL) {
						g_value_array_free(values);
					}
					for (j = 0; j < i; j++) {
						g_byte_array_remove_index(array, 0);
					}
					g_print(")\n");
					break;
				}
			} else {
				while (array->len > 0) {
					g_print("`%c'\n", array->data[0]);
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
	vte_trie_free(trie);
	return 0;
}
