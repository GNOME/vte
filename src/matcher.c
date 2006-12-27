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
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <glib-object.h>
#include "debug.h"
#include "caps.h"
#include "matcher.h"
#include "table.h"
#include "trie.h"

struct _vte_matcher {
	gboolean initialized;
	enum {
		_vte_matcher_table,
		_vte_matcher_trie
	} type;
	struct _vte_table *table;
	struct _vte_trie *trie;
};

static GStaticMutex _vte_matcher_mutex = G_STATIC_MUTEX_INIT;
static GCache *_vte_matcher_cache = NULL;

/* Add a string to the matcher. */
static void
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

/* Loads all sequences into matcher */
static void
_vte_matcher_init(struct _vte_matcher *matcher, const char *emulation,
		  struct _vte_termcap *termcap)
{
	const char *code, *value;
	gboolean found_cr = FALSE, found_lf = FALSE;
	gssize stripped_length;
	char *stripped;
	int i;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_LIFECYCLE)) {
		fprintf(stderr, "_vte_matcher_init()\n");
	}
#endif

	/* Load the known capability strings from the termcap structure into
	 * the table for recognition. */
	for (i = 0;
	     _vte_terminal_capability_strings[i].capability[0];
	     i++) {
		if (_vte_terminal_capability_strings[i].key) {
			continue;
		}
		code = _vte_terminal_capability_strings[i].capability;
		stripped = _vte_termcap_find_string_length(termcap,
                                                           emulation,
                                                           code,
                                                           &stripped_length);
		if (stripped[0] != '\0') {
			_vte_matcher_add(matcher, stripped, stripped_length,
					 code, 0);
			if (stripped[0] == '\r') {
				found_cr = TRUE;
			} else
			if (stripped[0] == '\n') {
				if ((strcmp(code, "sf") == 0) ||
				    (strcmp(code, "do") == 0)) {
					found_lf = TRUE;
				}
			}
		}
		g_free(stripped);
	}

	/* Add emulator-specific sequences. */
	if (strstr(emulation, "xterm") || strstr(emulation, "dtterm")) {
		/* Add all of the xterm-specific stuff. */
		for (i = 0;
		     _vte_xterm_capability_strings[i].value != NULL;
		     i++) {
			code = _vte_xterm_capability_strings[i].code;
			value = _vte_xterm_capability_strings[i].value;
			_vte_matcher_add(matcher, code, strlen (code),
					 value, 0);
		}
	}

	/* Always define cr and lf. */
	if (!found_cr) {
		_vte_matcher_add(matcher, "\r", 1, "cr", 0);
	}
	if (!found_lf) {
		_vte_matcher_add(matcher, "\n", 1, "sf", 0);
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_TRIE)) {
		fprintf(stderr, "Trie contents:\n");
		_vte_matcher_print(matcher);
		fprintf(stderr, "\n");
	}
#endif
}

/* Allocates new matcher structure. */
static gpointer
_vte_matcher_create(gpointer key)
{
	char *emulation = key;
	struct _vte_matcher *ret = NULL;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_LIFECYCLE)) {
		fprintf(stderr, "_vte_matcher_create()\n");
	}
#endif
	ret = g_slice_new(struct _vte_matcher);
	ret->initialized = FALSE;
	ret->type = _vte_matcher_trie;
	ret->table = NULL;
	ret->trie = NULL;

	if (strcmp(emulation, "xterm") == 0) {
		ret->type = _vte_matcher_table;
	} else
	if (strcmp(emulation, "dtterm") == 0) {
		ret->type = _vte_matcher_table;
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

/* Noone uses this matcher, free it. */
static void
_vte_matcher_destroy(gpointer value)
{
	struct _vte_matcher *matcher = value;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_LIFECYCLE)) {
		fprintf(stderr, "_vte_matcher_destroy()\n");
	}
#endif
	if (matcher->table != NULL) {
		_vte_table_free(matcher->table);
	}
	if (matcher->trie != NULL) {
		_vte_trie_free(matcher->trie);
	}
	g_slice_free(struct _vte_matcher, matcher);
}

/* Create and init matcher. */
struct _vte_matcher *
_vte_matcher_new(const char *emulation, struct _vte_termcap *termcap)
{
	struct _vte_matcher *ret = NULL;
	g_static_mutex_lock(&_vte_matcher_mutex);

	if (emulation == NULL) {
		emulation = "";
	}

	if (_vte_matcher_cache == NULL) {
		_vte_matcher_cache = g_cache_new(_vte_matcher_create,
				_vte_matcher_destroy,
			       	(GCacheDupFunc) g_strdup, g_free,
				g_str_hash, g_direct_hash, g_str_equal);
	}

	ret = g_cache_insert(_vte_matcher_cache, (gpointer) emulation);

	if (!ret->initialized) {
		_vte_matcher_init(ret, emulation, termcap);
		ret->initialized = TRUE;
	}

	g_static_mutex_unlock(&_vte_matcher_mutex);
	return ret;
}

/* Free a matcher. */
void
_vte_matcher_free(struct _vte_matcher *matcher)
{
	g_assert(_vte_matcher_cache != NULL);
	g_static_mutex_lock(&_vte_matcher_mutex);
	g_cache_remove(_vte_matcher_cache, matcher);
	g_static_mutex_unlock(&_vte_matcher_mutex);
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

/* Free a parameter array.  Most of the GValue elements can clean up after
 * themselves, but we're using gpointers to hold unicode character strings, and
 * we need to free those ourselves. */
void
_vte_matcher_free_params_array(GValueArray *params)
{
	guint i;
	GValue *value;
	gpointer ptr;
	if (params != NULL) {
		for (i = 0; i < params->n_values; i++) {
			value = g_value_array_get_nth(params, i);
			if (G_VALUE_HOLDS_POINTER(value)) {
				ptr = g_value_get_pointer(value);
				if (ptr != NULL) {
					g_free(ptr);
				}
				g_value_set_pointer(value, NULL);
			}
		}
		g_value_array_free(params);
	}
}

