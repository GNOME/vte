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


#include <glib.h>
#include <gtk/gtk.h>
#include "vte.h"

G_BEGIN_DECLS

#define VTE_TYPE_TERMINAL_ACCESSIBLE            (vte_terminal_accessible_get_type ())
#define VTE_TERMINAL_ACCESSIBLE(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), VTE_TYPE_TERMINAL_ACCESSIBLE, VteTerminalAccessible))
#define VTE_TERMINAL_ACCESSIBLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VTE_TYPE_TERMINAL_ACCESSIBLE, VteTerminalAccessibleClass))
#define VTE_IS_TERMINAL_ACCESSIBLE(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), VTE_TYPE_TERMINAL_ACCESSIBLE))
#define VTE_IS_TERMINAL_ACCESSIBLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VTE_TYPE_TERMINAL_ACCESSIBLE))
#define VTE_TERMINAL_ACCESSIBLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_TERMINAL_ACCESSIBLE, VteTerminalAccessibleClass))

typedef struct _VteTerminalAccessible VteTerminalAccessible;
typedef struct _VteTerminalAccessibleClass VteTerminalAccessibleClass;

/**
 * VteTerminalAccessible:
 *
 * The accessible peer for #VteTerminal.
 */
struct _VteTerminalAccessible {
	GtkAccessible parent;
	/*< private > */
	/* Unknown GailWidget implementation stuffs, exact size of which is
	 * worked out at run-time. */
};

struct _VteTerminalAccessibleClass {
	GtkAccessibleClass parent_class;
	/*< private > */
	/* Unknown GailWidgetClass implementation stuffs, exact size of which
	 * is worked out at run-time. */
};

GType vte_terminal_accessible_get_type(void);

AtkObject *vte_terminal_accessible_new(VteTerminal *terminal);

#define VTE_TYPE_TERMINAL_ACCESSIBLE_FACTORY            (vte_terminal_accessible_factory_get_type ())
#define VTE_TERMINAL_ACCESSIBLE_FACTORY(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), VTE_TYPE_TERMINAL_ACCESSIBLE_FACTORY, VteTerminalAccessibleFactory))
#define VTE_TERMINAL_ACCESSIBLE_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VTE_TYPE_TERMINAL_ACCESSIBLE_FACTORY, VteTerminalAccessibleFactoryClass))
#define VTE_IS_TERMINAL_ACCESSIBLE_FACTORY(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), VTE_TYPE_TERMINAL_ACCESSIBLE_FACTORY))
#define VTE_IS_TERMINAL_ACCESSIBLE_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VTE_TYPE_TERMINAL_ACCESSIBLE_FACTORY))
#define VTE_TERMINAL_ACCESSIBLE_FACTORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_TERMINAL_ACCESSIBLE_FACTORY, VteTerminalAccessibleFactoryClass))

typedef struct _VteTerminalAccessibleFactory VteTerminalAccessibleFactory;
typedef struct _VteTerminalAccessibleFactoryClass VteTerminalAccessibleFactoryClass;

struct _VteTerminalAccessibleFactory {
	AtkObjectFactory parent;
};

struct _VteTerminalAccessibleFactoryClass {
	AtkObjectFactoryClass parent;
};

GType vte_terminal_accessible_factory_get_type(void);

AtkObjectFactory *vte_terminal_accessible_factory_new(void);

G_END_DECLS

#endif
