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

#ifndef caps_h
#define caps_h

#ident "$Id$"

#include <glib.h>

G_BEGIN_DECLS

/* A NULL-terminated list of capability strings which have string values,
 * which means they're either key sequences or commands. */
struct vte_capability_quark {
	const char *capability;
	GQuark quark;
};
struct vte_capability_string {
	const char *code, *value;
	GQuark quark;
};

/* The known capability strings in termcap entries. */
extern struct vte_capability_quark vte_terminal_capability_strings[];

/* The xterm-specific terminal control strings. */
extern struct vte_capability_string vte_xterm_capability_strings[];

/* Initialize the Quarks in the various tables. */
void vte_capability_init(void);

G_END_DECLS

#endif
