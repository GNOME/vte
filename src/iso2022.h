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

#ifndef vte_iso2022_h_included
#define vte_iso2022_h_included

#ident "$Id$"

#include <glib.h>
#include <glib-object.h>
#include "matcher.h"

G_BEGIN_DECLS

struct _vte_iso2022 *_vte_iso2022_new(void);
struct _vte_iso2022 *_vte_iso2022_copy(struct _vte_iso2022 *original);
void _vte_iso2022_free(struct _vte_iso2022 *p);
gunichar _vte_iso2022_substitute_single(gunichar mapname, gunichar c);
gssize _vte_iso2022_substitute(struct _vte_iso2022 *state,
			       gunichar *instring, gssize length,
			       gunichar *outstring,
			       struct _vte_matcher *specials);

#define VTE_ISO2022_WIDTH_BIT_OFFSET	28
#define VTE_ISO2022_WIDTH_MASK		(3 << VTE_ISO2022_WIDTH_BIT_OFFSET)
#define VTE_ISO2022_HAS_WIDTH(__c)	(((__c) & VTE_ISO2022_WIDTH_MASK) != 0)
gssize _vte_iso2022_get_width(gunichar c);

G_END_DECLS

#endif
