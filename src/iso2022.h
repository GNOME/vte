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

#ifndef vte_iso2022_h_included
#define vte_iso2022_h_included

#ident "$Id"

#include <glib.h>
#include <glib-object.h>
#include "table.h"

G_BEGIN_DECLS

struct vte_iso2022;
struct vte_iso2022 *vte_iso2022_new(void);
struct vte_iso2022 *vte_iso2022_copy(struct vte_iso2022 *original);
void vte_iso2022_free(struct vte_iso2022 *p);
gssize vte_iso2022_substitute(struct vte_iso2022 *state,
			      gunichar *instring, gssize length,
			      gunichar *outstring, struct vte_table *specials);

G_END_DECLS

#endif
