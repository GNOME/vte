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

/* The interfaces in this file are subject to change at any time. */

#ifndef vte_caps_h_included
#define vte_caps_h_included


#include <glib.h>

G_BEGIN_DECLS

#define _VTE_CAP_ESC "\033"		/* Escape */
#define _VTE_CAP_CSI _VTE_CAP_ESC "["	/* Control Sequence Introducer */
#define _VTE_CAP_ST  _VTE_CAP_ESC "\\"	/* String Terminator */
#define _VTE_CAP_OSC _VTE_CAP_ESC "]"	/* Operating System Command */
#define _VTE_CAP_PM  _VTE_CAP_ESC "^"	/* Privacy Message */
#define _VTE_CAP_APC _VTE_CAP_ESC "_"	/* Application Program Command */
#define _VTE_CAP_SS2 _VTE_CAP_ESC "N"	/* Single-shift to G2 */
#define _VTE_CAP_SS3 _VTE_CAP_ESC "O"	/* Single-shift to G3 */

/* A NULL-terminated list of capability strings which have string values,
 * which means they're either key sequences or commands. */
struct _vte_capability_quark {
	const char capability[4];
	gboolean key;
	GQuark quark;
};
struct _vte_capability_string {
	const char *code;
	const char *value;
	GQuark quark;
};

/* The known capability strings in termcap entries, terminated by NULLs. */
extern struct _vte_capability_quark _vte_terminal_capability_strings[];

/* The xterm-specific terminal control strings, terminated by NULLs. */
extern struct _vte_capability_string _vte_xterm_capability_strings[];

/* Initialize the Quarks in the various tables. */
void _vte_capability_init(void);

G_END_DECLS

#endif
