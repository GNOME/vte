/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
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

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#ifndef __VTE_DEPRECATED_H__
#define __VTE_DEPRECATED_H__

#ifndef VTE_DISABLE_DEPRECATION_WARNINGS
#define _VTE_DEPRECATED G_DEPRECATED
#else
#define _VTE_DEPRECATED
#endif

G_BEGIN_DECLS

_VTE_DEPRECATED
void vte_terminal_match_set_cursor(VteTerminal *terminal,
                                   int tag,
                                   GdkCursor *cursor) _VTE_GNUC_NONNULL(1);

G_END_DECLS

#undef _VTE_DEPRECATED

#endif /* !__VTE_DEPRECATED__H__ */
