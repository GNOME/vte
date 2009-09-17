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


#include <glib.h>
#include <gtk/gtk.h>
#include "vtebg.h"
#include "vte.h"
#include "vteunistr.h"

G_BEGIN_DECLS

#define VTE_DRAW_SINGLE_WIDE_CHARACTERS	\
					" !\"#$%&'()*+,-./" \
					"0123456789" \
					":;<=>?@" \
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
					"[\\]^_`" \
					"abcdefghijklmnopqrstuvwxyz" \
					"{|}~" \
					""
#define VTE_DRAW_DOUBLE_WIDE_CHARACTERS 0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x4e94,\
					0xac00, 0xac01, 0xac04, 0xac08, 0xac10
/* For Pango, we have to use CJK Ideographs alone. Otherwise, 'width'
   returned by pango_layout would be screwed up for Chinese and Japanese
   fonts without Hangul */
#define VTE_DRAW_DOUBLE_WIDE_IDEOGRAPHS 0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x4e94
#define VTE_DRAW_OPAQUE 0xff
#define VTE_DRAW_MAX_LENGTH 1024

struct _vte_draw;

/* A request to draw a particular character spanning a given number of columns
   at the given location.  Unlike most APIs, (x,y) specifies the top-left
   corner of the cell into which the character will be drawn instead of the
   left end of the baseline. */
struct _vte_draw_text_request {
	vteunistr c;
	gshort x, y, columns;
};

struct _vte_draw_impl {
	const char *name;
	gboolean (*check)(struct _vte_draw *draw, GtkWidget *widget);
	void (*create)(struct _vte_draw *draw, GtkWidget *widget);
	void (*destroy)(struct _vte_draw *draw);
	GdkVisual* (*get_visual)(struct _vte_draw *draw);
	GdkColormap* (*get_colormap)(struct _vte_draw *draw);
	void (*start)(struct _vte_draw *draw);
	void (*end)(struct _vte_draw *draw);
	void (*set_background_solid)(struct _vte_draw *,
				     GdkColor *color,
				     guint16 opacity);
	void (*set_background_image)(struct _vte_draw *,
				     enum VteBgSourceType type,
				     GdkPixbuf *pixbuf,
				     const char *file,
				     const GdkColor *color,
				     double saturation);
	void (*set_background_scroll)(struct _vte_draw *,
				      gint, gint);
	void (*clip)(struct _vte_draw *, GdkRegion *);
	gboolean always_requires_clear;
	void (*clear)(struct _vte_draw *, gint, gint, gint, gint);
	void (*set_text_font)(struct _vte_draw *,
			      const PangoFontDescription *,
			      VteTerminalAntiAlias);
	void (*get_text_metrics)(struct _vte_draw *, gint *, gint *, gint *);
	int (*get_char_width)(struct _vte_draw *, vteunistr c, int columns,
			      gboolean);
	gboolean (*has_bold)(struct _vte_draw *);
	void (*draw_text)(struct _vte_draw *,
			  		  struct _vte_draw_text_request *, gsize,
					  GdkColor *, guchar, gboolean);
	gboolean (*has_char)(struct _vte_draw *, vteunistr, gboolean);
	void (*draw_rectangle)(struct _vte_draw *,
			       gint, gint, gint, gint,
			       GdkColor *, guchar);
	void (*fill_rectangle)(struct _vte_draw *,
			       gint, gint, gint, gint,
			       GdkColor *, guchar);
};

struct _vte_draw {
	GtkWidget *widget;

	gboolean started;

	gboolean requires_clear;

	const struct _vte_draw_impl *impl;

	/* for use by impl */
	gpointer impl_data;
};

/* Create and destroy a draw structure. */
VTE_STATIC  struct _vte_draw *_vte_draw_new(GtkWidget *widget);
VTE_STATIC  void _vte_draw_free(struct _vte_draw *draw);

/* Get the visual and colormap the draw structure desires.  Certain draw
   implementations may require that this visual/colormap pair be used when
   creating a window, and may fail otherwise. */
VTE_STATIC  GdkVisual *_vte_draw_get_visual(struct _vte_draw *draw);
VTE_STATIC  GdkColormap *_vte_draw_get_colormap(struct _vte_draw *draw,
						gboolean maybe_use_default);

/* Begin and end a drawing operation.  If anything is buffered locally, it is
   flushed to the window system when _end() is called. */
VTE_STATIC  void _vte_draw_start(struct _vte_draw *draw);
VTE_STATIC  void _vte_draw_end(struct _vte_draw *draw);

VTE_STATIC  void _vte_draw_set_background_solid(struct _vte_draw *draw,
						GdkColor *color,
						guint16 opacity);
VTE_STATIC  void _vte_draw_set_background_image(struct _vte_draw *draw,
						enum VteBgSourceType type,
						GdkPixbuf *pixbuf,
						const char *file,
						const GdkColor *color,
						double saturation);
VTE_STATIC  void _vte_draw_set_background_scroll(struct _vte_draw *draw,
						 gint x, gint y);

VTE_STATIC  gboolean _vte_draw_clip(struct _vte_draw *draw, GdkRegion *region);
VTE_STATIC  gboolean _vte_draw_requires_clear (struct _vte_draw *draw);
VTE_STATIC  void _vte_draw_clear(struct _vte_draw *draw,
				 gint x, gint y, gint width, gint height);

VTE_STATIC  void _vte_draw_set_text_font(struct _vte_draw *draw,
					 const PangoFontDescription *fontdesc,
					 VteTerminalAntiAlias anti_alias);
VTE_STATIC  void _vte_draw_get_text_metrics(struct _vte_draw *draw,
					    gint *width, gint *height, gint *ascent);
VTE_STATIC  int _vte_draw_get_char_width(struct _vte_draw *draw, vteunistr c, int columns,
					 gboolean bold);

VTE_STATIC  void _vte_draw_text(struct _vte_draw *draw,
				struct _vte_draw_text_request *requests, gsize n_requests,
				GdkColor *color, guchar alpha, gboolean);
VTE_STATIC  gboolean _vte_draw_char(struct _vte_draw *draw,
				    struct _vte_draw_text_request *request,
				    GdkColor *color, guchar alpha, gboolean bold);
VTE_STATIC  gboolean _vte_draw_has_char(struct _vte_draw *draw, vteunistr c, gboolean bold);


VTE_STATIC  void _vte_draw_fill_rectangle(struct _vte_draw *draw,
					  gint x, gint y, gint width, gint height,
					  GdkColor *color, guchar alpha);
VTE_STATIC  void _vte_draw_draw_rectangle(struct _vte_draw *draw,
					  gint x, gint y, gint width, gint height,
					  GdkColor *color, guchar alpha);

G_END_DECLS

#endif
