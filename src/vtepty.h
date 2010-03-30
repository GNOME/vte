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

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#ifndef VTE_PTY_H
#define VTE_PTY_H

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * VtePtyFlags:
 * @VTE_PTY_NO_LASTLOG: don't record the session in lastlog
 * @VTE_PTY_NO_UTMP: don't record the session in utmp
 * @VTE_PTY_NO_WTMP: don't record the session in wtmp
 * @VTE_PTY_NO_HELPER: don't use the GNOME PTY helper to allocate the PTY
 * @VTE_PTY_NO_FALLBACK: when allocating the PTY with the PTY helper fails,
 *   don't fall back to try using PTY98
 * @VTE_PTY_DEFAULT: the default flags
 *
 * Since: 0.26
 */
typedef enum {
  VTE_PTY_NO_LASTLOG  = 1 << 0,
  VTE_PTY_NO_UTMP     = 1 << 1,
  VTE_PTY_NO_WTMP     = 1 << 2,
  VTE_PTY_NO_HELPER   = 1 << 3,
  VTE_PTY_NO_FALLBACK = 1 << 4,
  VTE_PTY_DEFAULT     = 0
} VtePtyFlags;

/**
 * VtePtyError:
 * @VTE_PTY_ERROR_PTY_HELPER_FAILED: failure when using the GNOME PTY helper to
 *   allocate the PTY
 * @VTE_PTY_ERROR_PTY98_FAILED: failure when using PTY98 to allocate the PTY
 *
 * Since: 0.26
 */
typedef enum {
  VTE_PTY_ERROR_PTY_HELPER_FAILED = 0,
  VTE_PTY_ERROR_PTY98_FAILED
} VtePtyError;

GQuark vte_pty_error_quark (void);

/**
 * VTE_PTY_ERROR:
 *
 * Error domain for VTE PTY errors. Errors in this domain will be from the #VtePtyError
 * enumeration. See #GError for more information on error domains.
 *
 * Since: 0.26
 */
#define VTE_PTY_ERROR (vte_pty_error_quark ())

/* VTE PTY object */

#define VTE_TYPE_PTY            (vte_pty_get_type())
#define VTE_PTY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VTE_TYPE_PTY, VtePty))
#define VTE_PTY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  VTE_TYPE_PTY, VtePtyClass))
#define VTE_IS_PTY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VTE_TYPE_PTY))
#define VTE_IS_PTY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  VTE_TYPE_PTY))
#define VTE_PTY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  VTE_TYPE_PTY, VtePtyClass))

typedef struct _VtePty        VtePty;
typedef struct _VtePtyClass   VtePtyClass;

GType vte_pty_get_type (void);

VtePty *vte_pty_new (VtePtyFlags flags,
                     GError **error);

VtePty *vte_pty_new_foreign (int fd,
                             GError **error);

int vte_pty_get_fd (VtePty *pty);

void vte_pty_close (VtePty *pty);

void vte_pty_child_setup (VtePty *pty);

gboolean vte_pty_get_size (VtePty *pty,
                           int *rows,
                           int *columns,
                           GError **error);

gboolean vte_pty_set_size (VtePty *pty,
                           int rows,
                           int columns,
                           GError **error);

gboolean vte_pty_set_utf8 (VtePty *pty,
                           gboolean utf8,
                           GError **error);

void vte_pty_set_term (VtePty *pty,
                       const char *emulation);

G_END_DECLS

#endif /* VTE_PTY_H */
