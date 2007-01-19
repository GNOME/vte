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

#ifndef vte_debug_h_included
#define vte_debug_h_included


#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	VTE_DEBUG_MISC		= 1 << 0,
	VTE_DEBUG_PARSE		= 1 << 1,
	VTE_DEBUG_IO		= 1 << 2,
	VTE_DEBUG_UPDATES	= 1 << 3,
	VTE_DEBUG_EVENTS	= 1 << 4,
	VTE_DEBUG_SIGNALS	= 1 << 5,
	VTE_DEBUG_SELECTION	= 1 << 6,
	VTE_DEBUG_SUBSTITUTION	= 1 << 7,
	VTE_DEBUG_RING		= 1 << 8,
	VTE_DEBUG_PTY		= 1 << 9,
	VTE_DEBUG_CURSOR	= 1 << 10,
	VTE_DEBUG_KEYBOARD	= 1 << 11,
	VTE_DEBUG_LIFECYCLE	= 1 << 12,
	VTE_DEBUG_TRIE		= 1 << 13,
	VTE_DEBUG_WORK	= 1 << 14
} VteDebugFlags;

void _vte_debug_parse_string(const char *string);
gboolean _vte_debug_on(VteDebugFlags flags) G_GNUC_CONST;

G_END_DECLS

#endif
