/*
 * Copyright (C) 2000-2002 Red Hat, Inc.
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

#ifndef termcap_h
#define termcap_h

#ident "$Id$"

G_BEGIN_DECLS

struct vte_termcap;

/* Create a new termcap structure. */
struct vte_termcap *vte_termcap_new(const char *filename);

/* Free a termcap structure. */
void vte_termcap_free(struct vte_termcap *termcap);

/* Read a boolean capability for a given terminal. */
int vte_termcap_find_boolean(struct vte_termcap *termcap, const char *tname,
			     const char *cap);

/* Read a numeric capability for a given terminal. */
long vte_termcap_find_numeric(struct vte_termcap *termcap, const char *tname,
			      const char *cap);

/* Read a string capability for a given terminal.  The returned string should
 * be freed with g_free(). */
char *vte_termcap_find_string(struct vte_termcap *termcap, const char *tname,
			      const char *cap);

/* Read a string capability for a given terminal, and return the length of
 * the result in addition to the result itself.  The returned string should
 * be freed with g_free(). */
char *vte_termcap_find_string_length(struct vte_termcap *termcap,
				     const char *tname,
				     const char *cap, size_t *length);

G_END_DECLS

#endif
