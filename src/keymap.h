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

#ifndef vte_keymap_h_included
#define vte_keymap_h_included

#include <glib.h>
#include <gdk/gdk.h>
#include "vtetc.h"

G_BEGIN_DECLS

#define VTE_META_MASK		GDK_META_MASK
#define VTE_NUMLOCK_MASK	GDK_MOD2_MASK

/* Map the specified keyval/modifier setup, dependent on the mode, to either
 * a literal string or a capability name. */
void _vte_keymap_map(guint keyval,
		     GdkModifierType modifiers,
		     gboolean sun_mode,
		     gboolean hp_mode,
		     gboolean legacy_mode,
		     gboolean vt220_mode,
		     gboolean app_cursor_keys,
		     gboolean app_keypad_keys,
		     struct _vte_termcap *termcap,
		     const char *term,
		     char **normal,
		     gssize *normal_length,
		     const char **special);

/* Return TRUE if a keyval is just a modifier key. */
gboolean _vte_keymap_key_is_modifier(guint keyval);

/* Add modifiers to the sequence if they're needed. */
void _vte_keymap_key_add_key_modifiers(guint keyval,
				       GdkModifierType modifiers,
				       gboolean sun_mode,
				       gboolean hp_mode,
				       gboolean legacy_mode,
				       gboolean vt220_mode,
				       gboolean app_cursor_keys,
				       char **normal,
				       gssize *normal_length);

G_END_DECLS

#endif
