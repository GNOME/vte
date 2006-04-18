/*
 * Copyright (C) 2003 Red Hat, Inc.
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


#ifndef vte_rdbh_included
#define vte_rdbh_included

#include <gtk/gtk.h>

G_BEGIN_DECLS

double _vte_rdb_get_dpi(GtkWidget *widget);
gboolean _vte_rdb_get_antialias(GtkWidget *widget);
gboolean _vte_rdb_get_hinting(GtkWidget *widget);
const char *_vte_rdb_get_rgba(GtkWidget *widget);
const char *_vte_rdb_get_hintstyle(GtkWidget *widget);

G_END_DECLS

#endif
