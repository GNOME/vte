/*
 * Copyright © 2014 Christian Persch
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

#ifndef __VTE_TERMINFO_H__
#define __VTE_TERMINFO_H__

#include <glib.h>

G_BEGIN_DECLS

#define VTE_TERMINFO_VARTYPE_BOOLEAN (1U << 13)
#define VTE_TERMINFO_VARTYPE_NUMERIC (1U << 14)
#define VTE_TERMINFO_VARTYPE_STRING  (1U << 15)

struct _vte_terminfo;

struct _vte_terminfo *_vte_terminfo_new(const char *term);

struct _vte_terminfo *_vte_terminfo_ref(struct _vte_terminfo *terminfo);
void _vte_terminfo_unref(struct _vte_terminfo *terminfo);

gboolean _vte_terminfo_is_xterm_like(struct _vte_terminfo *terminfo);

typedef void (* _vte_terminfo_foreach_boolean_func)(struct _vte_terminfo *terminfo,
                                                    const char *cap,
                                                    const char *compat_cap,
                                                    gboolean value,
                                                    gpointer user_data);

gboolean _vte_terminfo_get_boolean(struct _vte_terminfo *terminfo,
                                   guint variable);
gboolean _vte_terminfo_get_boolean_by_cap(struct _vte_terminfo *terminfo,
                                          const char *cap,
                                          gboolean compat);
void _vte_terminfo_foreach_boolean(struct _vte_terminfo *terminfo,
                                   gboolean include_extensions,
                                   _vte_terminfo_foreach_boolean_func func,
                                   gpointer user_data);

typedef void (* _vte_terminfo_foreach_numeric_func)(struct _vte_terminfo *terminfo,
                                                    const char *cap,
                                                    const char *compat_cap,
                                                    int value,
                                                    gpointer user_data);

int _vte_terminfo_get_numeric(struct _vte_terminfo *terminfo,
                              guint variable);
int _vte_terminfo_get_numeric_by_cap(struct _vte_terminfo *terminfo,
                                     const char *cap,
                                     gboolean compat);
void _vte_terminfo_foreach_numeric(struct _vte_terminfo *terminfo,
                                   gboolean include_extensions,
                                   _vte_terminfo_foreach_numeric_func func,
                                   gpointer user_data);

typedef void (* _vte_terminfo_foreach_string_func) (struct _vte_terminfo *terminfo,
                                                    const char *cap,
                                                    const char *compat_cap,
                                                    const char *value,
                                                    gpointer user_data);

const char * _vte_terminfo_get_string(struct _vte_terminfo *terminfo,
                                      guint variable);
const char * _vte_terminfo_get_string_by_cap(struct _vte_terminfo *terminfo,
                                             const char *cap,
                                             gboolean compat);
void _vte_terminfo_foreach_string(struct _vte_terminfo *terminfo,
                                  gboolean include_extensions,
                                  _vte_terminfo_foreach_string_func func,
                                  gpointer user_data);

const char * _vte_terminfo_sequence_to_string(const char *str);

G_END_DECLS

/* Capability defines */
#include "vtetivars.h"

#endif /* __VTE_TERMINFO_H__ */
