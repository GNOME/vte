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
