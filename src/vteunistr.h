/*
 * Copyright (C) 2008 Red Hat, Inc.
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
 *
 * Author(s):
 * 	Behdad Esfahbod
 */

#ifndef vte_vteunistr_h_included
#define vte_vteunistr_h_included

#include <glib.h>

G_BEGIN_DECLS

typedef guint32 vteunistr;

#define vte_unistr_from_unichar(c) ((vteunistr) c)

vteunistr
_vte_unistr_append_unichar (vteunistr s, gunichar c);

int
_vte_unistr_strlen (vteunistr s);

gunichar
_vte_unistr_get_base (vteunistr s);

void
_vte_unistr_append_to_string (vteunistr s, GString *gs);

G_END_DECLS

#endif
