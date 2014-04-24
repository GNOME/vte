/*
 * Copyright (C) 2000-2002 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* The interfaces in this file are subject to change at any time. */

#ifndef vte_termcap_h_included
#define vte_termcap_h_included


#include <glib.h>

G_BEGIN_DECLS

struct _vte_termcap;

/* Create a new termcap structure. */
struct _vte_termcap *_vte_termcap_new(const char *name);

/* Free a termcap structure. */
void _vte_termcap_free(struct _vte_termcap *termcap);

/* Read a boolean capability for a given terminal. */
gboolean _vte_termcap_find_boolean(struct _vte_termcap *termcap,
				   const char *tname, const char *cap);

/* Read a numeric capability for a given terminal. */
long _vte_termcap_find_numeric(struct _vte_termcap *termcap, const char *tname,
			       const char *cap);

/* Read a string capability for a given terminal.  The returned string should
 * be freed with g_free(). */
char *_vte_termcap_find_string(struct _vte_termcap *termcap, const char *tname,
			       const char *cap);

/* Read a string capability for a given terminal, and return the length of
 * the result in addition to the result itself.  The returned string should
 * be freed with g_free(). */
char *_vte_termcap_find_string_length(struct _vte_termcap *termcap,
				      const char *tname,
				      const char *cap, gssize *length);

G_END_DECLS

#endif
