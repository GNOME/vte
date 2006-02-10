/*
 * Copyright (C) 2001-2004 Red Hat, Inc.
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

#ifndef vte_vteseq_h_included
#define vte_vteseq_h_included

#include <glib.h>

/* A function which can handle a terminal control sequence.  Returns TRUE only
 * if something happened (usually a signal emission) to which the controlling
 * application must have an immediate opportunity to respond. */
typedef gboolean (*VteTerminalSequenceHandler)(VteTerminal *terminal,
					       const char *match,
					       GQuark match_quark,
					       GValueArray *params);


VteTerminalSequenceHandler _vte_sequence_get_handler (const char *code,
						      GQuark quark);


gboolean _vte_sequence_handler_bl(VteTerminal *terminal, const char *match, GQuark match_quark, GValueArray *params);
gboolean _vte_sequence_handler_sf(VteTerminal *terminal, const char *match, GQuark match_quark, GValueArray *params);

#endif
