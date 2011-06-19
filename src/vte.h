/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
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

#ifndef __VTE_H__
#define __VTE_H__

#include <glib.h>
#include <gio/gio.h>
#include <pango/pango.h>
#include <gtk/gtk.h>

#define __VTE_VTE_H_INSIDE__ 1

#include "vteenums.h"
#include "vtepty.h"
#include "vtebuffer.h"
#include "vteview.h"
#include "vtetypebuiltins.h"
#include "vteversion.h"

#undef __VTE_VTE_H_INSIDE__

G_BEGIN_DECLS

const char *vte_get_default_emulation(void);

char *vte_get_user_shell (void);

G_END_DECLS

#endif /* __VTE_H__ */
