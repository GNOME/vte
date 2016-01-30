/*
 * Copyright (C) 2001-2004 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef vte_vte_private_h_included
#define vte_vte_private_h_included

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>
#include <glib/gi18n-lib.h>

#include "buffer.h"
#include "debug.h"
#include "vteconv.h"
#include "vtedraw.hh"
#include "ring.h"
#include "caps.h"

#include "vtedefines.hh"
#include "vteinternal.hh"

G_BEGIN_DECLS

VteTerminalPrivate *_vte_terminal_get_impl(VteTerminal *terminal);

VteRowData *_vte_terminal_ensure_row(VteTerminal *terminal);
VteRowData * _vte_new_row_data(VteTerminal *terminal);

VteRowData *_vte_terminal_ring_insert (VteTerminal *terminal, glong position, gboolean fill);
VteRowData *_vte_terminal_ring_append (VteTerminal *terminal, gboolean fill);
void _vte_terminal_ring_remove (VteTerminal *terminal, glong position);

gboolean _vte_terminal_size_to_grid_size(VteTerminal *terminal,
                                         long w,
                                         long h,
                                         long *cols,
                                         long *rows);

static inline bool
_vte_double_equal(double a,
                  double b)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        return a == b;
#pragma GCC diagnostic pop
}

G_END_DECLS

#endif
