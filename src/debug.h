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
	VTE_DEBUG_WORK		= 1 << 14,
	VTE_DEBUG_CELLS		= 1 << 15,
	VTE_DEBUG_TIMEOUT	= 1 << 16,
	VTE_DEBUG_DRAW		= 1 << 17,
	VTE_DEBUG_ALLY		= 1 << 18,
	VTE_DEBUG_ADJ		= 1 << 19,
	VTE_DEBUG_PANGOCAIRO    = 1 << 20,
	VTE_DEBUG_WIDGET_SIZE   = 1 << 21
} VteDebugFlags;

void _vte_debug_init(void);

extern VteDebugFlags _vte_debug_flags;
static inline gboolean _vte_debug_on(VteDebugFlags flags) G_GNUC_CONST G_GNUC_UNUSED;

static inline gboolean
_vte_debug_on(VteDebugFlags flags)
{
	return (_vte_debug_flags & flags) == flags;
}

#ifdef VTE_DEBUG
#define _VTE_DEBUG_IF(flags) if (G_UNLIKELY (_vte_debug_on (flags)))
#else
#define _VTE_DEBUG_IF(flags) if (0)
#endif

#if defined(__GNUC__) && G_HAVE_GNUC_VARARGS
#define _vte_debug_print(flags, fmt, ...) \
	G_STMT_START { _VTE_DEBUG_IF(flags) g_printerr(fmt, ##__VA_ARGS__); } G_STMT_END
#else
#include <stdarg.h>
#include <glib/gstdio.h>
static void _vte_debug_print(guint flags, const char *fmt, ...)
{
	if (_vte_debug_on (flags)) {
		va_list  ap;
		va_start (ap, fmt);
		g_vfprintf (stderr, fmt, ap);
		va_end (ap);
	}
}
#endif

G_END_DECLS

#endif
