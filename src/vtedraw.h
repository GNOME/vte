/*
 * Copyright (C) 2003 Red Hat, Inc.
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

/* The interfaces in this file are subject to change at any time. */

#ifndef vte_vtedraw_h_included
#define vte_vtedraw_h_included

#ident "$Id$"

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VTE_DRAW_SINGLE_WIDE_CHARACTERS	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
					"abcdefgjijklmnopqrstuvwxyz" \
					"0123456789./+@"
#define VTE_DRAW_DOUBLE_WIDE_CHARACTERS	0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x4e94
#define VTE_DRAW_OPAQUE 0xff
#define VTE_DRAW_MAX_LENGTH 88

struct _vte_draw;

struct _vte_draw_text_request {
	gunichar c;
	gint x, y, columns;
};

struct _vte_draw_impl {
	const char *name, *environment;
	gboolean (*check)(struct _vte_draw *draw, GtkWidget *widget);
	void (*create)(struct _vte_draw *draw, GtkWidget *widget);
	void (*destroy)(struct _vte_draw *draw);
	void (*start)(struct _vte_draw *draw);
	void (*end)(struct _vte_draw *draw);
	void (*set_background_color)(struct _vte_draw *, GdkColor *);
	void (*set_background_pixbuf)(struct _vte_draw *, GdkPixbuf *);
	void (*clear)(struct _vte_draw *, gint, gint, gint, gint);
	void (*set_text_font)(struct _vte_draw *, const PangoFontDescription *);
	int (*get_text_width)(struct _vte_draw *);
	int (*get_text_height)(struct _vte_draw *);
	int (*get_text_base)(struct _vte_draw *);
	void (*draw_text)(struct _vte_draw *,
			  struct _vte_draw_text_request *, gsize,
			  GdkColor *, guchar);
	void (*draw_rectangle)(struct _vte_draw *,
			       gint, gint, gint, gint,
			       GdkColor *, guchar);
	void (*fill_rectangle)(struct _vte_draw *,
			       gint, gint, gint, gint,
			       GdkColor *, guchar);
	gboolean (*scroll)(struct _vte_draw *, gint, gint);
	void (*set_scroll)(struct _vte_draw *, gint, gint);
};

struct _vte_draw {
	GtkWidget *widget;
	gboolean started;
	gint width, height, base;
	struct _vte_draw_impl *impl;
	gpointer impl_data;
};

struct _vte_draw *_vte_draw_new(GtkWidget *widget);
void _vte_draw_free(struct _vte_draw *draw);
void _vte_draw_start(struct _vte_draw *draw);
void _vte_draw_end(struct _vte_draw *draw);

void _vte_draw_set_background_color(struct _vte_draw *draw, GdkColor *color);
void _vte_draw_set_background_pixbuf(struct _vte_draw *draw, GdkPixbuf *pixbuf);
void _vte_draw_clear(struct _vte_draw *draw,
		     gint x, gint y, gint width, gint height);

void _vte_draw_set_text_font(struct _vte_draw *draw,
			     const PangoFontDescription *fontdesc);
int _vte_draw_get_text_width(struct _vte_draw *draw);
int _vte_draw_get_text_height(struct _vte_draw *draw);
int _vte_draw_get_text_base(struct _vte_draw *draw);
void _vte_draw_text(struct _vte_draw *draw,
		    struct _vte_draw_text_request *requests, gsize n_requests,
		    GdkColor *color, guchar alpha);

void _vte_draw_fill_rectangle(struct _vte_draw *draw,
			      gint x, gint y, gint width, gint height,
			      GdkColor *color, guchar alpha);
void _vte_draw_draw_rectangle(struct _vte_draw *draw,
			      gint x, gint y, gint width, gint height,
			      GdkColor *color, guchar alpha);
gboolean _vte_draw_scroll(struct _vte_draw *draw, gint dx, gint dy);
void _vte_draw_set_scroll(struct _vte_draw *draw, gint x, gint y);

G_END_DECLS

#endif
