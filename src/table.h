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

#ifndef vte_table_h_included
#define vte_table_h_included


#include <glib-object.h>

G_BEGIN_DECLS

struct _vte_table;

/* Create an empty, one-level table. */
struct _vte_table *_vte_table_new(void);

/* Free a table tree. */
void _vte_table_free(struct _vte_table *table);

/* Add a string to the matching tree. */
void _vte_table_add(struct _vte_table *table,
		    const char *pattern, gssize length,
		    const char *result, GQuark quark);

/* Check if a string matches something in the tree. */
const char *_vte_table_match(struct _vte_table *table,
			     const gunichar *pattern, gssize length,
			     const char **res, const gunichar **consumed,
			     GQuark *quark, GValueArray **array);
/* Dump out the contents of a tree. */
void _vte_table_print(struct _vte_table *table);

extern const struct _vte_matcher_class _vte_matcher_table;

G_END_DECLS

#endif
