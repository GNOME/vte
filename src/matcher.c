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
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <glib-object.h>
#include "debug.h"
#include "matcher.h"
#include "table.h"
#include "trie.h"

struct _vte_matcher {
	enum {
		_vte_matcher_table,
		_vte_matcher_trie
	} type;
	struct _vte_table *table;
	struct _vte_trie *trie;
};

/* Create an initial matcher. */
struct _vte_matcher *
_vte_matcher_new(const char *emulation_hint)
{
	struct _vte_matcher *ret = NULL;
	ret = g_malloc(sizeof(struct _vte_matcher));
	ret->type = _vte_matcher_trie;
	ret->table = NULL;
	ret->trie = NULL;
	if (emulation_hint != NULL) {
		if (strcmp(emulation_hint, "xterm") == 0) {
			ret->type = _vte_matcher_table;
		} else
		if (strcmp(emulation_hint, "dtterm") == 0) {
			ret->type = _vte_matcher_table;
		}
	}
	switch (ret->type) {
	case _vte_matcher_table:
		ret->table = _vte_table_new();
		break;
	case _vte_matcher_trie:
		ret->trie = _vte_trie_new();
		break;
	}
	return ret;
}

/* Free a matcher. */
void
_vte_matcher_free(struct _vte_matcher *matcher)
{
	if (matcher->table != NULL) {
		_vte_table_free(matcher->table);
	}
	if (matcher->trie != NULL) {
		_vte_trie_free(matcher->trie);
	}
	g_free(matcher);
}

/* Add a string to the matcher. */
void
_vte_matcher_add(struct _vte_matcher *matcher,
		 const char *pattern, gssize length,
		 const char *result, GQuark quark)
{
	switch (matcher->type) {
	case _vte_matcher_table:
		_vte_table_add(matcher->table, pattern, length, result, quark);
		break;
	case _vte_matcher_trie:
		_vte_trie_add(matcher->trie, pattern, length, result, quark);
		break;
	}
}

/* Check if a string matches a sequence the matcher knows about. */
const char *
_vte_matcher_match(struct _vte_matcher *matcher,
		   const gunichar *pattern, gssize length,
		   const char **res, const gunichar **consumed,
		   GQuark *quark, GValueArray **array)
{
	switch (matcher->type) {
	case _vte_matcher_table:
		return _vte_table_match(matcher->table, pattern, length,
					res, consumed, quark, array);
		break;
	case _vte_matcher_trie:
		return _vte_trie_match(matcher->trie, pattern, length,
				       res, consumed, quark, array);
		break;
	}
	return NULL;
}

/* Dump out the contents of a matcher, mainly for debugging. */
void
_vte_matcher_print(struct _vte_matcher *matcher)
{
	switch (matcher->type) {
	case _vte_matcher_table:
		_vte_table_print(matcher->table);
		break;
	case _vte_matcher_trie:
		_vte_trie_print(matcher->trie);
		break;
	}
}

/* Determine a giconv target name which produces output which is bit-for-bit
 * identical to either ASCII (wide==FALSE) or gunichar (wide==TRUE). */
static char *
_vte_matcher_find_valid_encoding(const char **list, gssize length,
				 gboolean wide)
{
	const char SAMPLE[] = "ABCDEF #@{|}[\\]~";
	gunichar wbuffer[sizeof(SAMPLE)];
	unsigned char nbuffer[sizeof(SAMPLE)];
	void *buffer;
	char inbuf[BUFSIZ];
	char outbuf[BUFSIZ];
	char *ibuf, *obuf;
	gsize isize, osize;
	int i;
	gsize outbytes;
	GIConv conv;

	/* Decide what the iconv output buffer must resemble. */
	if (wide) {
		buffer = wbuffer;
	} else {
		buffer = nbuffer;
	}

	/* Initialize both the narrow and wide output buffers. */
	for (i = 0; SAMPLE[i] != '\0'; i++) {
		wbuffer[i] = nbuffer[i] = SAMPLE[i];
	}
	wbuffer[i] = nbuffer[i] = SAMPLE[i];

	/* Iterate over the list, attempting to convert from UTF-8 to the
	 * named encoding, and then comparing it to the desired buffer. */
	for (i = 0; i < length; i++) {
		conv = g_iconv_open(list[i], "UTF-8");
		if (conv == ((GIConv) -1)) {
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_MISC)) {
				fprintf(stderr, "Conversions to `%s' are not "
					"supported by giconv.\n", list[i]);
			}
#endif
			continue;
		}

		ibuf = (char*) &inbuf;
		strcpy(inbuf, SAMPLE);
		isize = 3;
		obuf = (char*) &outbuf;
		osize = sizeof(outbuf);

		g_iconv(conv, &ibuf, &isize, &obuf, &osize);
		g_iconv_close(conv);

		outbytes = sizeof(outbuf) - osize;
		if ((isize == 0) && (outbytes > 0)) {
			if (memcmp(outbuf, buffer, outbytes) == 0) {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_MISC)) {
					fprintf(stderr, "Found iconv target "
						"`%s'.\n", list[i]);
				}
#endif
				return g_strdup(list[i]);
			}
		}
	}

	return NULL;
}

const char *
_vte_matcher_wide_encoding()
{
	const char *wide[] = {
		"10646",
		"ISO_10646",
		"ISO-10646",
		"ISO10646",
		"ISO-10646-1",
		"ISO10646-1",
		"ISO-10646/UCS4",
		"UCS-4",
		"UCS4",
		"UCS-4-BE",
		"UCS-4BE",
		"UCS4-BE",
		"UCS-4-INTERNAL",
		"UCS-4-LE",
		"UCS-4LE",
		"UCS4-LE",
		"UNICODE",
		"UNICODE-BIG",
		"UNICODEBIG",
		"UNICODE-LITTLE",
		"UNICODELITTLE",
		"WCHAR_T",
	};
	static char *ret = NULL;
	if (ret == NULL) {
		ret = _vte_matcher_find_valid_encoding(wide,
						       G_N_ELEMENTS(wide),
						       TRUE);
	}
	return ret;
}
