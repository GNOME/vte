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

#ifndef vte_table_h_included
#define vte_table_h_included

#ident "$Id$"

#include <glib-object.h>

struct vte_table;

/* Create an empty, one-level table. */
struct vte_table *vte_table_new(void);
void vte_table_free(struct vte_table *table);

/* Add a string to the matching tree. */
void vte_table_add(struct vte_table *table,
		   const unsigned char *pattern, size_t length,
		   const char *result, GQuark quark);

/* Check if a string matches something in the tree. */
const char *vte_table_match(struct vte_table *table,
			    const gunichar *pattern, size_t length,
			    const char **res, const gunichar **consumed,
			    GQuark *quark, GValueArray **array);
/* Dump out the contents of a tree. */
void vte_table_print(struct vte_table *table);

/* A gunichar-compatible giconv target, if one can be found. */
const char *vte_table_wide_encoding(void);

/* A single-byte iso-8859-1 giconv target, if one can be found. */
const char *vte_table_narrow_encoding(void);

#endif
