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

#ident "$Id$"
#include "../config.h"
#include <glib.h>
#include "debug.h"

static VteDebugFlags vte_debug_flags = 0;

void
vte_debug_parse_string(const char *string)
{
	char **flags = NULL;
	int i;
	vte_debug_flags = 0;
	flags = g_strsplit(string ? string : "", ",", 0);
	if (flags != NULL) {
		for (i = 0; flags[i] != NULL; i++) {
			if (g_ascii_strcasecmp(flags[i], "ALL") == 0) {
				vte_debug_flags |= 0xffffffff;
			} else
			if (g_ascii_strcasecmp(flags[i], "MISC") == 0) {
				vte_debug_flags |= VTE_DEBUG_MISC;
			} else
			if (g_ascii_strcasecmp(flags[i], "IO") == 0) {
				vte_debug_flags |= VTE_DEBUG_IO;
			} else
			if (g_ascii_strcasecmp(flags[i], "UPDATES") == 0) {
				vte_debug_flags |= VTE_DEBUG_UPDATES;
			} else
			if (g_ascii_strcasecmp(flags[i], "EVENTS") == 0) {
				vte_debug_flags |= VTE_DEBUG_EVENTS;
			} else
			if (g_ascii_strcasecmp(flags[i], "PARSE") == 0) {
				vte_debug_flags |= VTE_DEBUG_PARSE;
			} else
			if (g_ascii_strcasecmp(flags[i], "SIGNALS") == 0) {
				vte_debug_flags |= VTE_DEBUG_SIGNALS;
			} else
			if (g_ascii_strcasecmp(flags[i], "SELECTION") == 0) {
				vte_debug_flags |= VTE_DEBUG_SELECTION;
			} else
			if (g_ascii_strcasecmp(flags[i], "SUBSTITUTION") == 0) {
				vte_debug_flags |= VTE_DEBUG_SUBSTITUTION;
			} else
			if (g_ascii_strcasecmp(flags[i], "RING") == 0) {
				vte_debug_flags |= VTE_DEBUG_RING;
			}
		}
		g_strfreev(flags);
	}
}

gboolean
vte_debug_on(VteDebugFlags flags)
{
	return (vte_debug_flags & flags) == flags;
}
