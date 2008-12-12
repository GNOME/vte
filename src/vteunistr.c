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

#include <config.h>

#include <string.h>

#include "vteunistr.h"

#define VTE_UNISTR_START 0x80000000


static vteunistr unistr_next = VTE_UNISTR_START + 1;

struct VteUnistrDecomp {
	vteunistr prefix;
	gunichar  suffix;
};

GArray     *unistr_decomp;
GHashTable *unistr_comp;

static guint
unistr_comp_hash (gconstpointer key)
{
	struct VteUnistrDecomp *decomp;
	decomp = &g_array_index (unistr_decomp,
				 struct VteUnistrDecomp,
				 GPOINTER_TO_UINT (key));
	return decomp->prefix ^ decomp->suffix;
}

static gboolean
unistr_comp_equal (gconstpointer a,
		      gconstpointer b)
{
	return 0 == memcmp (&g_array_index (unistr_decomp,
					    struct VteUnistrDecomp,
					    GPOINTER_TO_UINT (a)),
			    &g_array_index (unistr_decomp,
					    struct VteUnistrDecomp,
					    GPOINTER_TO_UINT (b)),
			    sizeof (struct VteUnistrDecomp));
}

vteunistr
_vte_unistr_append_unichar (vteunistr s, gunichar c)
{
	struct VteUnistrDecomp decomp;
	vteunistr ret = 0;

	decomp.prefix = s;
	decomp.suffix = c;

	if (G_UNLIKELY (!unistr_decomp)) {
		unistr_decomp = g_array_new (FALSE, TRUE,
						sizeof (struct VteUnistrDecomp));
		g_array_set_size (unistr_decomp, 1);
		unistr_comp = g_hash_table_new (unistr_comp_hash,
						unistr_comp_equal);
	} else {
		g_array_index (unistr_decomp,
			       struct VteUnistrDecomp,
			       0) = decomp;
		ret = GPOINTER_TO_UINT (g_hash_table_lookup (unistr_comp,
							     GUINT_TO_POINTER (0)));
	}

	if (G_UNLIKELY (!ret)) {
		ret = unistr_next++;
		g_array_append_val (unistr_decomp, decomp);
		g_hash_table_insert (unistr_comp,
				     GUINT_TO_POINTER (ret - VTE_UNISTR_START),
				     GUINT_TO_POINTER (ret));
	}

	return ret;
}

/* Unused
int
_vte_unistr_strlen (vteunistr s)
{
	int len = 1;
	g_return_val_if_fail (s < unistr_next, len);
	while (G_UNLIKELY (s >= VTE_UNISTR_START)) {
		s = g_array_index (unistr_decomp,
				   struct VteUnistrDecomp,
				   s - VTE_UNISTR_START).prefix;
		len++;
	}
	return len;
}
*/

gunichar
_vte_unistr_get_base (vteunistr s)
{
	g_return_val_if_fail (s < unistr_next, s);
	while (G_UNLIKELY (s >= VTE_UNISTR_START))
		s = g_array_index (unistr_decomp,
				   struct VteUnistrDecomp,
				   s - VTE_UNISTR_START).prefix;
	return (gunichar) s;
}

void
_vte_unistr_append_to_string (vteunistr s, GString *gs)
{
	g_return_if_fail (s < unistr_next);
	if (G_UNLIKELY (s >= VTE_UNISTR_START)) {
		struct VteUnistrDecomp *decomp;
		decomp = &g_array_index (unistr_decomp,
					 struct VteUnistrDecomp,
					 s - VTE_UNISTR_START);
		_vte_unistr_append_to_string (decomp->prefix, gs);
		s = decomp->suffix;
	}
	g_string_append_unichar (gs, (gunichar) s);
}
