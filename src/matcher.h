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

/* The interfaces in this file are subject to change at any time. */

#ifndef vte_matcher_h_included
#define vte_matcher_h_included

#ident "$Id$"

#include <glib-object.h>

struct _vte_matcher;

/* Create an initial matcher. */
struct _vte_matcher *_vte_matcher_new(const char *emulation_hint);

/* Free a matcher. */
void _vte_matcher_free(struct _vte_matcher *matcher);

/* Add a string to the matcher. */
void _vte_matcher_add(struct _vte_matcher *matcher,
		      const char *pattern, gssize length,
		      const char *result, GQuark quark);

/* Check if a string matches a sequence the matcher knows about. */
const char *_vte_matcher_match(struct _vte_matcher *matcher,
			       const gunichar *pattern, gssize length,
			       const char **res, const gunichar **consumed,
			       GQuark *quark, GValueArray **array);

/* Dump out the contents of a matcher, mainly for debugging. */
void _vte_matcher_print(struct _vte_matcher *matcher);

/* A gunichar-compatible giconv target, if one can be found. */
const char *_vte_matcher_wide_encoding(void);

#endif
