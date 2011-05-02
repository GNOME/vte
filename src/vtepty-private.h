/*
 * Copyright © 2009, 2010 Christian Persch
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

G_BEGIN_DECLS

gboolean __vte_pty_spawn (VtePty *pty,
                          const char *working_directory,
                          char **argv,
                          char **envv,
                          GSpawnFlags spawn_flags,
                          GSpawnChildSetupFunc child_setup,
                          gpointer child_setup_data,
                          GPid *child_pid /* out */,
                          GError **error);

G_END_DECLS
