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
#include "termcap.h"

int
main(int argc, char **argv)
{
	char *terminal = NULL;
	struct _vte_matcher *matcher = NULL;
	struct _vte_termcap *termcap = NULL;
	GByteArray *array = NULL;
	int i, j;
	char c;
	GValue *value;
	FILE *infile = NULL;
	struct _vte_iso2022 *substitutions, *tmpsubst;
	const char *tmp;
	GQuark quark;
	GValueArray *values;
	GError *error = NULL;
	gunichar *ubuf;
	gssize substlen;
	gsize ubuflen;

	_vte_debug_parse_string(getenv("VTE_DEBUG_FLAGS"));

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
	matcher = _vte_matcher_new(terminal);
	termcap = _vte_termcap_new(g_strdup_printf(DATADIR "/" PACKAGE
						   "/termcap/%s", terminal));
	if (termcap == NULL) {
		termcap = _vte_termcap_new("/etc/termcap");
	}
	array = g_byte_array_new();

	for (i = 0;
	     _vte_terminal_capability_strings[i].capability != NULL;
	     i++) {
		const char *capability;
		char *tmp;
		capability = _vte_terminal_capability_strings[i].capability;
		if (_vte_terminal_capability_strings[i].key) {
			continue;
		}
		tmp = _vte_termcap_find_string(termcap, terminal, capability);
		if ((tmp != NULL) && (strlen(tmp) > 0)) {
			_vte_matcher_add(matcher, tmp, strlen(tmp), capability,
				       g_quark_from_static_string(capability));
		}
		g_free(tmp);
	}
	for (i = 0; _vte_xterm_capability_strings[i].value != NULL; i++) {
		const char *code, *value;
		code = _vte_xterm_capability_strings[i].code;
		value = _vte_xterm_capability_strings[i].value;
		_vte_matcher_add(matcher, code, strlen(code), value,
				 g_quark_from_static_string(code));
	}

	substitutions = _vte_iso2022_new();

	while (fread(&c, 1, 1, infile) == 1) {
		g_byte_array_append(array, (guint8*) &c, 1);
		for (i = 1; i <= array->len; i++) {
			ubuf = (gunichar*) g_convert((const gchar*)array->data,
						     i,
						     _vte_matcher_wide_encoding(),
						     "UTF-8",
						     NULL, &ubuflen, &error);
			if (error != NULL) {
				if (error->code ==
				    G_CONVERT_ERROR_ILLEGAL_SEQUENCE) {
					g_print("Munging input byte %02x->?.\n",
						array->data[0]);
					array->data[0] = '?';
				} else
				if (error->code !=
				    G_CONVERT_ERROR_PARTIAL_INPUT) {
					g_print("%s\n",
						error->message ?
						error->message :
						"?");
					g_print("Data: ");
					for (j = 0; j < array->len; j++) {
						if (j > 0) {
							g_print(", ");
						}
						g_print("0x%x", array->data[j]);
					}
					g_print("\n");
				}
				g_clear_error(&error);
				continue;
			}
			tmpsubst = _vte_iso2022_copy(substitutions);
			substlen = _vte_iso2022_substitute(tmpsubst,
							   ubuf,
							   ubuflen / sizeof(gunichar),
							   ubuf,
							   matcher);
			if (substlen < 0) {
				/* Incomplete state-change. */
				_vte_iso2022_free(tmpsubst);
				g_free(ubuf);
				continue;
			}
			if (substlen == 0) {
				/* State change. (We gave it more than one
				 * character, so that one's and all of the
				 * others have been consumed.) */
				_vte_iso2022_free(substitutions);
				substitutions = tmpsubst;
				while (array->len > 0) {
					g_byte_array_remove_index(array, 0);
				}
				g_free(ubuf);
				break;
			}

			_vte_matcher_match(matcher, ubuf, substlen,
					   &tmp, NULL, &quark, &values);
			if (tmp != NULL) {
				if (strlen(tmp) > 0) {
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
							       (wchar_t*) g_value_get_pointer(value));
						}
					}
					if (values != NULL) {
						g_value_array_free(values);
					}
					for (j = 0; j < i; j++) {
						g_byte_array_remove_index(array, 0);
					}
					printf(")\n");
					g_free(ubuf);
					break;
				} else {
					g_free(ubuf);
					continue;
				}
			} else {
				while (array->len > 0) {
					g_byte_array_remove_index(array, 0);
				}
				for (j = 0; j < substlen; j++) {
					if (VTE_ISO2022_HAS_ENCODED_WIDTH(ubuf[j])) {
						ubuf[j] &= ~VTE_ISO2022_ENCODED_WIDTH_MASK;
					}
					if (ubuf[j] < 32) {
						printf("`^%c'\n", ubuf[j] + 64);
					} else
					if (ubuf[j] < 127) {
						printf("`%c'\n", ubuf[j]);
					} else {
						printf("`0x%x'\n", ubuf[j]);
					}
				}
			}
			g_free(ubuf);
		}
	}

	if (infile != stdin) {
		fclose(infile);
	}

	_vte_iso2022_free(substitutions);
	g_byte_array_free(array, TRUE);
	_vte_termcap_free(termcap);
	_vte_matcher_free(matcher);
	return 0;
}
