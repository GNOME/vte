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

#ifndef VTE_DISABLE_DEPRECATED

#ifndef vte_reaper_h_included
#define vte_reaper_h_included

#include <sys/wait.h>
#include <signal.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

struct _VteReaper {
	GObject object;
	GIOChannel *_channel; /* unused */
	int _iopipe[2]; /* unused */
};
typedef struct _VteReaper VteReaper;

struct _VteReaperClass {
	GObjectClass parent_class;
	guint child_exited_signal;
};
typedef struct _VteReaperClass VteReaperClass;

GType vte_reaper_get_type(void);

#define VTE_TYPE_REAPER			(vte_reaper_get_type())
#define VTE_REAPER(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), \
							VTE_TYPE_REAPER, \
							VteReaper))
#define VTE_REAPER_CLASS(klass)		G_TYPE_CHECK_CLASS_CAST((klass), \
							     VTE_TYPE_REAPER, \
							     VteReaperClass)
#define VTE_IS_REAPER(obj)		G_TYPE_CHECK_INSTANCE_TYPE((obj), VTE_TYPE_REAPER)
#define VTE_IS_REAPER_CLASS(klass)	G_TYPE_CHECK_CLASS_TYPE((klass), \
							     VTE_TYPE_REAPER)
#define VTE_REAPER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
								   VTE_TYPE_REAPER, \
								   VteReaperClass))

VteReaper *vte_reaper_get(void);
int vte_reaper_add_child(GPid pid);

G_END_DECLS

#endif

#endif /* !VTE_DISABLE_DEPRECATED */
