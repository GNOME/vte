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

#ifndef vte_vteaccess_h_included
#define vte_vteaccess_h_included

#ident "$Id$"

#include <glib.h>
#include <gtk/gtk.h>
#include "vte.h"

G_BEGIN_DECLS

/* The terminal accessibility object itself. */
typedef struct _VteTerminalAccessible {
	GtkAccessible parent;
} VteTerminalAccessible;

/* The object's class structure. */
typedef struct _VteTerminalAccessibleClass {
	/*< public > */
	/* Inherited parent class. */
	GtkAccessibleClass parent_class;
} VteTerminalAccessibleClass;

/* The object's type. */
GtkType vte_terminal_accessible_get_type(void);

#define VTE_TYPE_TERMINAL_ACCESSIBLE	(vte_terminal_accessible_get_type())
#define VTE_TERMINAL_ACCESSIBLE(obj)	(GTK_CHECK_CAST((obj),\
							VTE_TYPE_TERMINAL_ACCESSIBLE,\
							VteTerminalAccessible))
#define VTE_TERMINAL_ACCESSIBLE_CLASS(klass)	GTK_CHECK_CLASS_CAST((klass),\
								     VTE_TYPE_TERMINAL_ACCESSIBLE,\
								     VteTerminalAccessibleClass)
#define VTE_IS_TERMINAL_ACCESSIBLE(obj)		GTK_CHECK_TYPE((obj),\
							       VTE_TYPE_TERMINAL_ACCESSIBLE)
#define VTE_IS_TERMINAL_ACCESSIBLE_CLASS(klass)	GTK_CHECK_CLASS_TYPE((klass),\
								     VTE_TYPE_TERMINAL_ACCESSIBLE)
#define VTE_TERMINAL_ACCESSIBLE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_TERMINAL_ACCESSIBLE, VteTerminalAccessibleClass))


AtkObject *vte_terminal_accessible_new(VteTerminal *terminal);

G_END_DECLS

#endif
