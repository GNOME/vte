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

#ifndef trie_h
#define trie_h

#ident "$Id$"

#include <wchar.h>
#include <glib.h>

G_BEGIN_DECLS

struct vte_trie;

/* Create a new trie structure. */
struct vte_trie *vte_trie_new(void);

/* Free a trie structure. */
void vte_trie_free(struct vte_trie *trie);

/* Add a string to the trie, along with its associated result and an optional
 * Quark to store with it. */
void vte_trie_add(struct vte_trie *trie,
		  const char *pattern, size_t length,
		  const char *result, GQuark quark);

/* See if a given pattern of a given length is in the trie.  The result is
 * returned both as the result of the function, and in the pointer res (if
 * res is not NULL).  The associated quark is also stored in "quark".  If
 * the string could be the initial portion of some sequence in the trie, the
 * empty string is returned for the answer.  If no match is found, and the
 * passed-in string can not be an initial substring of one of the strings in
 * the trie, then NULL is returned. */
const char *vte_trie_match(struct vte_trie *trie,
			   const wchar_t *pattern, size_t length,
			   const char **res,
			   const wchar_t **consumed,
			   GQuark *quark,
			   GValueArray **array);

/* Print the contents of the trie (mainly for diagnostic purposes). */
void vte_trie_print(struct vte_trie *trie);

/* Precompute internal information to hopefully make traversal faster. */
void vte_trie_precompute(struct vte_trie *trie);

G_END_DECLS

#endif
