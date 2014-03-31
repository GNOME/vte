/*
 * Copyright (C) 2002 Red Hat, Inc.
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

#ifndef vte_iso2022_h_included
#define vte_iso2022_h_included


#include <glib.h>
#include <glib-object.h>
#include "buffer.h"
#include "matcher.h"

G_BEGIN_DECLS

struct _vte_iso2022_state;
typedef void (*_vte_iso2022_codeset_changed_cb_fn)(struct _vte_iso2022_state *,
						   gpointer);
struct _vte_iso2022_state *_vte_iso2022_state_new(const char *native_codeset,
                                                  int utf8_ambiguous_width,
						  _vte_iso2022_codeset_changed_cb_fn,
						  gpointer);
void _vte_iso2022_state_set_codeset(struct _vte_iso2022_state *state,
				    const char *codeset);
void _vte_iso2022_state_set_utf8_ambiguous_width(struct _vte_iso2022_state *state,
                                                 int utf8_ambiguous_width);
const char *_vte_iso2022_state_get_codeset(struct _vte_iso2022_state *state);
gsize _vte_iso2022_process(struct _vte_iso2022_state *state,
			  guchar *input, gsize length,
			  GArray *gunichars);
gunichar _vte_iso2022_process_single(struct _vte_iso2022_state *state,
				     gunichar c, gunichar map);
void _vte_iso2022_state_free(struct _vte_iso2022_state *);

#define VTE_ISO2022_DEFAULT_UTF8_AMBIGUOUS_WIDTH 1

#define VTE_ISO2022_ENCODED_WIDTH_BIT_OFFSET	28
#define VTE_ISO2022_ENCODED_WIDTH_MASK		(3 << VTE_ISO2022_ENCODED_WIDTH_BIT_OFFSET)
#define VTE_ISO2022_HAS_ENCODED_WIDTH(__c)	(((__c) & VTE_ISO2022_ENCODED_WIDTH_MASK) != 0)
int _vte_iso2022_get_encoded_width(gunichar c);
int _vte_iso2022_unichar_width(struct _vte_iso2022_state *state,
			       gunichar c);

G_END_DECLS

#endif
