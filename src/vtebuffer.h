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

  /*< private >*/
  VteBufferClassPrivate *priv;
};

struct _VteBuffer {
  GObject object;

  VteBufferPrivate *pvt;
};

GType vte_buffer_get_type (void);

VteBuffer *vte_buffer_new (void);

G_END_DECLS

#endif /* VTE_BUFFER_H */
