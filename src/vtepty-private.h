/*
 * Copyright © 2009, 2010 Christian Persch
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

G_BEGIN_DECLS

VtePtyFlags __vte_pty_get_pty_flags(gboolean lastlog,
                                    gboolean utmp,
                                    gboolean wtmp);

char **__vte_pty_get_argv (const char *command,
                           char **argv,
                           GSpawnFlags *flags /* inout */);

gboolean __vte_pty_spawn (VtePty *pty,
                          const char *working_directory,
                          char **argv,
                          char **envv,
                          GSpawnFlags spawn_flags,
                          GSpawnChildSetupFunc child_setup,
                          gpointer child_setup_data,
                          GPid *child_pid /* out */,
                          GError **error);

gboolean __vte_pty_fork(VtePty *pty,
                        GPid *pid,
                        GError **error);

G_END_DECLS
