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

G_BEGIN_DECLS

#define VTE_DRAW_SINGLE_WIDE_CHARACTERS	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
					"abcdefgjijklmnopqrstuvwxyz" \
					"0123456789./+@&"
#define VTE_DRAW_DOUBLE_WIDE_CHARACTERS 0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x4e94,\
					0xac00, 0xac01, 0xac04, 0xac08, 0xac10
/* For Pango, we have to use CJK Ideographs alone. Otherwise, 'width'
   returned by pango_layout would be screwed up for Chinese and Japanese
   fonts without Hangul */
#define VTE_DRAW_DOUBLE_WIDE_IDEOGRAPHS 0x4e00, 0x4e8c, 0x4e09, 0x56db, 0x4e94
#define VTE_DRAW_OPAQUE 0xff
#define VTE_DRAW_MAX_LENGTH 1024

/* The _vte_draw structure. */
struct _vte_draw;

/* A request to draw a particular character spanning a given number of columns
   at the given location.  Unlike most APIs, (x,y) specifies the top-left
   corner of the cell into which the character will be drawn instead of the
   left end of the baseline. */
struct _vte_draw_text_request {
	gunichar c;
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
	void (*set_background_color)(struct _vte_draw *, GdkColor *, guint16);
	void (*set_background_image)(struct _vte_draw *,
				     enum VteBgSourceType type,
				     GdkPixbuf *pixbuf,
				     const char *file,
				     const GdkColor *color,
				     double saturation);
	gboolean requires_repaint;
	void (*clip)(struct _vte_draw *, GdkRegion *);
	void (*clear)(struct _vte_draw *, gint, gint, gint, gint);
	void (*set_text_font)(struct _vte_draw *,
			      const PangoFontDescription *,
			      VteTerminalAntiAlias);
	int (*get_text_width)(struct _vte_draw *);
	int (*get_text_height)(struct _vte_draw *);
	int (*get_text_ascent)(struct _vte_draw *);
	int (*get_char_width)(struct _vte_draw *, gunichar c, int columns);
	gboolean (*get_using_fontconfig)(struct _vte_draw *);
	void (*draw_text)(struct _vte_draw *,
			  struct _vte_draw_text_request *, gsize,
			  GdkColor *, guchar);
	gboolean (*draw_char)(struct _vte_draw *,
			      struct _vte_draw_text_request *,
			      GdkColor *, guchar);
	gboolean (*has_char)(struct _vte_draw *, gunichar);
	void (*draw_rectangle)(struct _vte_draw *,
			       gint, gint, gint, gint,
			       GdkColor *, guchar);
	void (*fill_rectangle)(struct _vte_draw *,
			       gint, gint, gint, gint,
			       GdkColor *, guchar);
	void (*set_scroll)(struct _vte_draw *, gint, gint);
};

struct _vte_draw {
	GtkWidget *widget;
	gboolean started;
	gint width, height, ascent;
	gboolean has_background_image;
	const struct _vte_draw_impl *impl;
	gpointer impl_data;
};

/* Create and destroy a draw structure. */
struct _vte_draw *_vte_draw_new(GtkWidget *widget);
void _vte_draw_free(struct _vte_draw *draw);

/* Get the visual and colormap the draw structure desires.  Certain draw
   implementations may require that this visual/colormap pair be used when
   creating a window, and may fail otherwise. */
GdkVisual *_vte_draw_get_visual(struct _vte_draw *draw);
GdkColormap *_vte_draw_get_colormap(struct _vte_draw *draw,
				    gboolean maybe_use_default);

/* Begin and end a drawing operation.  If anything is buffered locally, it is
   flushed to the window system when _end() is called. */
void _vte_draw_start(struct _vte_draw *draw);
void _vte_draw_end(struct _vte_draw *draw);

/* Set the background color, a background pixbuf (if you want transparency,
   you'll have to do that yourself), and clear an area to the default. */
void _vte_draw_set_background_color(struct _vte_draw *draw,
				    GdkColor *color,
				    guint16 opacity);
void _vte_draw_set_background_image(struct _vte_draw *draw,
				    enum VteBgSourceType type,
				    GdkPixbuf *pixbuf,
				    const char *file,
				    const GdkColor *color,
				    double saturation);
gboolean _vte_draw_has_background_image (struct _vte_draw *draw);
gboolean _vte_draw_requires_repaint(struct _vte_draw *draw);
gboolean _vte_draw_clip(struct _vte_draw *draw, GdkRegion *region);
void _vte_draw_clear(struct _vte_draw *draw,
		     gint x, gint y, gint width, gint height);

/* Set the font which will be used to draw text. */
void _vte_draw_set_text_font(struct _vte_draw *draw,
			     const PangoFontDescription *fontdesc,
			     VteTerminalAntiAlias anti_alias);
/* Read font metrics. */
int _vte_draw_get_text_width(struct _vte_draw *draw);
int _vte_draw_get_text_height(struct _vte_draw *draw);
int _vte_draw_get_text_ascent(struct _vte_draw *draw);
int _vte_draw_get_char_width(struct _vte_draw *draw, gunichar c, int columns);
gboolean _vte_draw_get_using_fontconfig(struct _vte_draw *draw);

/* Draw text or rectangles. */
void _vte_draw_text(struct _vte_draw *draw,
		    struct _vte_draw_text_request *requests, gsize n_requests,
		    GdkColor *color, guchar alpha);
gboolean _vte_draw_char(struct _vte_draw *draw,
			struct _vte_draw_text_request *request,
			GdkColor *color, guchar alpha);
gboolean _vte_draw_has_char(struct _vte_draw *draw, gunichar c);
void _vte_draw_fill_rectangle(struct _vte_draw *draw,
			      gint x, gint y, gint width, gint height,
			      GdkColor *color, guchar alpha);
void _vte_draw_draw_rectangle(struct _vte_draw *draw,
			      gint x, gint y, gint width, gint height,
			      GdkColor *color, guchar alpha);

/* Set the scrolling offset for painting in a pixbuf background. */
void _vte_draw_set_scroll(struct _vte_draw *draw, gint x, gint y);

G_END_DECLS

#endif
