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

#ifndef vte_pty_h_included
#define vte_pty_h_included

#ident "$Id$"

#include <sys/types.h>

G_BEGIN_DECLS

/* Start up the given binary (exact path, not interpreted at all) in a
 * pseudo-terminal of its own, returning the descriptor for the master
 * side of the PTY pair, storing the child's PID in the given argument. */
int vte_pty_open(pid_t *child, char **env_add,
		 const char *command, char **argv,
		 int columns, int rows);

/* As above, but with session logging. */
int vte_pty_open_with_logging(pid_t *child, char **env_add,
			      const char *command, char **argv,
			      int columns, int rows,
			      gboolean lastlog, gboolean utmp, gboolean wtmp);

/* Set or read the size of a terminal.  Returns 0 on success, -1 on failure,
 * with errno set to defined return codes from ioctl(). */
int vte_pty_get_size(int master, int *columns, int *rows);
int vte_pty_set_size(int master, int columns, int rows);

/* Close a pty. */
void vte_pty_close(int pty);

G_END_DECLS

#endif
