/*
 * Copyright (C) 2003 Red Hat, Inc.
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

#ifndef vte_vteint_h_included
#define vte_vteint_h_included


#include "vte.h"

G_BEGIN_DECLS

void _vte_terminal_accessible_ref(VteTerminal *terminal);
char* _vte_terminal_get_selection(VteTerminal *terminal);
void _vte_terminal_get_start_selection(VteTerminal *terminal, long *x, long *y);
void _vte_terminal_get_end_selection(VteTerminal *terminal, long *x, long *y);
void _vte_terminal_select_text(VteTerminal *terminal, long start_x, long start_y, long end_x, long end_y, int start_offset, int end_offset);
void _vte_terminal_remove_selection(VteTerminal *terminal);

G_END_DECLS

#endif
