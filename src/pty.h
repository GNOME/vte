/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
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

#ifndef pty_h_included
#define pty_h_included

#include <sys/types.h>

G_BEGIN_DECLS

/* Start up the given binary (exact path, not interpreted at all) in a
 * pseudo-terminal of its own, returning the descriptor for the master
 * side of the PTY pair, storing the child's PID in the given argument. */
int vte_pty_open(pid_t *child, const char **env_add,
		 const char *command, const char **argv);

G_END_DECLS

#endif
