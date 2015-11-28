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
#include "vtedraw.h"
#include "ring.h"
#include "caps.h"

#include "vtedefines.hh"
#include "vteinternal.hh"

G_BEGIN_DECLS

void _vte_terminal_accessible_ref(VteTerminal *terminal);
char* _vte_terminal_get_selection(VteTerminal *terminal);
void _vte_terminal_get_start_selection(VteTerminal *terminal, long *x, long *y);
void _vte_terminal_get_end_selection(VteTerminal *terminal, long *x, long *y);
void _vte_terminal_select_text(VteTerminal *terminal, long start_x, long start_y, long end_x, long end_y, int start_offset, int end_offset);
void _vte_terminal_remove_selection(VteTerminal *terminal);

VteRowData *_vte_terminal_ensure_row(VteTerminal *terminal);
VteRowData * _vte_new_row_data(VteTerminal *terminal);
void _vte_terminal_queue_contents_changed(VteTerminal *terminal);
void _vte_terminal_emit_text_deleted(VteTerminal *terminal);
void _vte_terminal_emit_text_inserted(VteTerminal *terminal);
void _vte_terminal_cursor_down (VteTerminal *terminal);
void _vte_terminal_drop_scrollback (VteTerminal *terminal);
void _vte_terminal_restore_cursor (VteTerminal *terminal, VteScreen *screen);
void _vte_terminal_save_cursor (VteTerminal *terminal, VteScreen *screen);
gboolean _vte_terminal_insert_char(VteTerminal *terminal, gunichar c,
			       gboolean force_insert_mode,
			       gboolean invalidate_cells);
void _vte_terminal_scroll_region(VteTerminal *terminal,
				 long row, glong count, glong delta);
void _vte_terminal_set_default_attributes(VteTerminal *terminal);
void _vte_terminal_update_insert_delta(VteTerminal *terminal);

void _vte_terminal_feed_focus_event(VteTerminal *terminal, gboolean in);

VteRowData *_vte_terminal_ring_insert (VteTerminal *terminal, glong position, gboolean fill);
VteRowData *_vte_terminal_ring_append (VteTerminal *terminal, gboolean fill);
void _vte_terminal_ring_remove (VteTerminal *terminal, glong position);

void _vte_terminal_set_cursor_style(VteTerminal *terminal, VteCursorStyle style);

char *_vte_terminal_attributes_to_html(VteTerminal *terminal,
                                       const gchar *text,
                                       GArray *attributes);
char *_vte_terminal_get_text_range_full(VteTerminal *terminal,
                                              glong start_row, glong start_col,
                                              glong end_row, glong end_col,
                                              VteSelectionFunc is_selected,
                                              gpointer user_data,
                                              GArray *attributes,
                                              gsize *ret_len);
char *_vte_terminal_get_text_range_maybe_wrapped(VteTerminal *terminal,
						       glong start_row,
						       glong start_col,
						       glong end_row,
						       glong end_col,
						       gboolean wrap,
						       VteSelectionFunc is_selected,
						       gpointer data,
						       GArray *attributes,
						       gboolean include_trailing_spaces,
                                                       gsize *ret_len);
char *_vte_terminal_get_text_maybe_wrapped(VteTerminal *terminal,
						 gboolean wrap,
						 VteSelectionFunc is_selected,
						 gpointer data,
						 GArray *attributes,
						 gboolean include_trailing_spaces,
                                                 gsize *ret_len);
GdkCursor *_vte_terminal_cursor_new(VteTerminal *terminal, GdkCursorType cursor_type);

/* vteseq.c: */
void _vte_terminal_handle_sequence(VteTerminal *terminal,
				   const char *match,
				   GValueArray *params);

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
