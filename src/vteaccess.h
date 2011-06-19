/*
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef vte_vteaccess_h_included
#define vte_vteaccess_h_included


#include <glib.h>
#include <gtk/gtk.h>
#include "vte.h"

G_BEGIN_DECLS

#define VTE_TYPE_VIEW_ACCESSIBLE            (_vte_view_accessible_get_type ())
#define VTE_VIEW_ACCESSIBLE(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), VTE_TYPE_VIEW_ACCESSIBLE, VteViewAccessible))
#define VTE_VIEW_ACCESSIBLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VTE_TYPE_VIEW_ACCESSIBLE, VteViewAccessibleClass))
#define VTE_IS_VIEW_ACCESSIBLE(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), VTE_TYPE_VIEW_ACCESSIBLE))
#define VTE_IS_VIEW_ACCESSIBLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VTE_TYPE_VIEW_ACCESSIBLE))
#define VTE_VIEW_ACCESSIBLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_VIEW_ACCESSIBLE, VteViewAccessibleClass))

typedef struct _VteViewAccessible VteViewAccessible;
typedef struct _VteViewAccessibleClass VteViewAccessibleClass;

/**
 * VteViewAccessible:
 *
 * The accessible peer for #VteView.
 */
struct _VteViewAccessible {
	GtkAccessible parent;
	/*< private > */
	/* Unknown GailWidget implementation stuffs, exact size of which is
	 * worked out at run-time. */
};

struct _VteViewAccessibleClass {
	GtkAccessibleClass parent_class;
	/*< private > */
	/* Unknown GailWidgetClass implementation stuffs, exact size of which
	 * is worked out at run-time. */
};

GType _vte_view_accessible_get_type(void);

AtkObject *_vte_view_accessible_new(VteView *terminal);

#define VTE_TYPE_VIEW_ACCESSIBLE_FACTORY            (_vte_view_accessible_factory_get_type ())
#define VTE_VIEW_ACCESSIBLE_FACTORY(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), VTE_TYPE_VIEW_ACCESSIBLE_FACTORY, VteViewAccessibleFactory))
#define VTE_VIEW_ACCESSIBLE_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VTE_TYPE_VIEW_ACCESSIBLE_FACTORY, VteViewAccessibleFactoryClass))
#define VTE_IS_VIEW_ACCESSIBLE_FACTORY(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), VTE_TYPE_VIEW_ACCESSIBLE_FACTORY))
#define VTE_IS_VIEW_ACCESSIBLE_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VTE_TYPE_VIEW_ACCESSIBLE_FACTORY))
#define VTE_VIEW_ACCESSIBLE_FACTORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_VIEW_ACCESSIBLE_FACTORY, VteViewAccessibleFactoryClass))

typedef struct _VteViewAccessibleFactory VteViewAccessibleFactory;
typedef struct _VteViewAccessibleFactoryClass VteViewAccessibleFactoryClass;

struct _VteViewAccessibleFactory {
	AtkObjectFactory parent;
};

struct _VteViewAccessibleFactoryClass {
	AtkObjectFactoryClass parent;
};

GType _vte_view_accessible_factory_get_type(void);

AtkObjectFactory *_vte_view_accessible_factory_new(void);

G_END_DECLS

#endif
