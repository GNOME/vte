/*
 * Copyright © 2011 Christian Persch
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

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#ifndef VTE_BUFFER_H
#define VTE_BUFFER_H

#include <gio/gio.h>

G_BEGIN_DECLS

/* VteBuffer object */

#define VTE_TYPE_BUFFER            (vte_buffer_get_type())
#define VTE_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VTE_TYPE_BUFFER, VteBuffer))
#define VTE_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  VTE_TYPE_BUFFER, VteBufferClass))
#define VTE_IS_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VTE_TYPE_BUFFER))
#define VTE_IS_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  VTE_TYPE_BUFFER))
#define VTE_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  VTE_TYPE_BUFFER, VteBufferClass))

typedef struct _VteBuffer             VteBuffer;
typedef struct _VteBufferPrivate      VteBufferPrivate;
typedef struct _VteBufferClass        VteBufferClass;
typedef struct _VteBufferClassPrivate VteBufferClassPrivate;

struct _VteBufferClass {
  GObjectClass object_class;

  void (*commit)               (VteBuffer *buffer,
                                const gchar *text,
                                guint size);
  void (*emulation_changed)    (VteBuffer *buffer);
  void (*encoding_changed)     (VteBuffer *buffer);
  void (*icon_title_changed)   (VteBuffer* buffer);
  void (*window_title_changed) (VteBuffer* buffer);
  void (*status_line_changed)  (VteBuffer* buffer);
  void (*eof)                  (VteBuffer* buffer);
  void (*child_exited)         (VteBuffer *buffer,
                                gint status);
  void (*deiconify_window)     (VteBuffer* buffer);
  void (*iconify_window)       (VteBuffer* buffer);
  void (*raise_window)         (VteBuffer* buffer);
  void (*lower_window)         (VteBuffer* buffer);
  void (*refresh_window)       (VteBuffer* buffer);
  void (*restore_window)       (VteBuffer* buffer);
  void (*maximize_window)      (VteBuffer* buffer);
  void (*resize_window)        (VteBuffer* buffer,
                                guint width,
                                guint height);
  void (*move_window)          (VteBuffer* buffer,
                                guint x,
                                guint y);
  void (*cursor_moved)         (VteBuffer* buffer);

  /*< private >*/
  VteBufferClassPrivate *priv;
};

struct _VteBuffer {
  GObject object;

  VteBufferPrivate *pvt;
};

GType vte_buffer_get_type (void);

VteBuffer *vte_buffer_new (void);

void vte_buffer_set_backspace_binding   (VteBuffer *buffer,
                                         VteEraseBinding binding);
void vte_buffer_set_delete_binding      (VteBuffer *buffer,
                                         VteEraseBinding binding);

void vte_buffer_set_emulation           (VteBuffer *buffer,
                                         const char *emulation);

const char *vte_buffer_get_emulation    (VteBuffer *buffer);

void vte_buffer_set_encoding            (VteBuffer *buffer,
                                         const char *codeset);

const char *vte_buffer_get_encoding     (VteBuffer *buffer);

void vte_buffer_set_pty                 (VteBuffer *buffer,
                                         VtePty *pty);

VtePty *vte_buffer_get_pty              (VteBuffer *buffer);

VtePty *vte_buffer_pty_new_sync         (VteBuffer *buffer,
                                         VtePtyFlags flags,
                                         GCancellable *cancellable,
                                         GError **error);

void vte_buffer_set_scrollback_lines    (VteBuffer *buffer,
                                         glong lines);

void vte_buffer_feed                    (VteBuffer *buffer,
                                         const char *data,
                                         gssize length);

void vte_buffer_feed_child              (VteBuffer *buffer,
                                         const char *text,
                                         gssize length);

void vte_buffer_feed_child_binary       (VteBuffer *buffer,
                                         const char *data,
                                         gsize length);

void vte_buffer_reset                   (VteBuffer *buffer,
                                         gboolean clear_tabstops,
                                         gboolean clear_history);

const char *vte_buffer_get_window_title (VteBuffer *buffer);

const char *vte_buffer_get_icon_title   (VteBuffer *buffer);

const char *vte_buffer_get_status_line  (VteBuffer *buffer);

void vte_buffer_set_size                (VteBuffer *buffer,
                                         glong columns,
                                         glong rows);

glong vte_buffer_get_row_count          (VteBuffer *buffer);

glong vte_buffer_get_column_count       (VteBuffer *buffer);

gboolean vte_buffer_write_contents_sync (VteBuffer *buffer,
                                         GOutputStream *stream,
                                         VteWriteFlags flags,
                                         GCancellable *cancellable,
                                         GError **error);

void vte_buffer_watch_child             (VteBuffer *buffer,
                                         GPid child_pid);

gboolean vte_buffer_spawn_sync          (VteBuffer *buffer,
                                         VtePtyFlags pty_flags,
                                         const char *working_directory,
                                         char **argv,
                                         char **envv,
                                         GSpawnFlags spawn_flags,
                                         GSpawnChildSetupFunc child_setup,
                                         gpointer child_setup_data,
                                         GPid *child_pid /* out */,
                                         GCancellable *cancellable,
                                         GError **error);

G_END_DECLS

#endif /* VTE_BUFFER_H */
