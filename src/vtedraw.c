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

#ident "$Id$"

#include "../config.h"
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "debug.h"
#include "vtedraw.h"
#include "vteft2.h"
#include "vtegl.h"
#include "vtepango.h"
#include "vtepangox.h"
#include "vteskel.h"
#include "vtexft.h"

struct _vte_draw_impl
*_vte_draw_impls[] = {
	&_vte_draw_skel,
#ifndef X_DISPLAY_MISSING
#ifdef HAVE_GL
	/* &_vte_draw_gl, */
#endif
#endif
#ifndef X_DISPLAY_MISSING
#ifdef HAVE_XFT2
	&_vte_draw_xft,
#endif
#endif
	&_vte_draw_ft2,
	&_vte_draw_pango,
#ifndef X_DISPLAY_MISSING
#ifdef HAVE_PANGOX
	&_vte_draw_pango_x,
#endif
#endif
};

struct _vte_draw *
_vte_draw_new(GtkWidget *widget)
{
	int i;
	struct _vte_draw *draw;
	char *var;

	/* Create the structure. */
	draw = g_malloc0(sizeof(struct _vte_draw));
	g_object_ref(G_OBJECT(widget));
	draw->widget = widget;
	draw->started = FALSE;

	/* Let each implementation decide if it's right for the job. */
	for (i = 0; i < G_N_ELEMENTS(_vte_draw_impls); i++) {
		if (_vte_draw_impls[i]->environment != NULL) {
			var = getenv(_vte_draw_impls[i]->environment);
			if (var != NULL) {
				if (atol(var) == 0) {
					continue;
				}
			}
		}
		if (_vte_draw_impls[i]->check(draw, draw->widget)) {
			draw->impl = _vte_draw_impls[i];
			draw->impl->create(draw, draw->widget);
			break;
		}
	}

	/* Something has to have worked. */
	g_assert(i < G_N_ELEMENTS(_vte_draw_impls));

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		fprintf(stderr, "Using %s.\n", draw->impl->name);
	}
#endif

	return draw;
}

void
_vte_draw_free(struct _vte_draw *draw)
{
	draw->impl->destroy(draw);
	draw->impl = NULL;
	draw->impl_data = NULL;

	if (GTK_IS_WIDGET(draw->widget)) {
		g_object_unref(G_OBJECT(draw->widget));
		draw->widget = NULL;
	}
	draw->started = FALSE;
	
	g_free(draw);
}

GdkVisual *
_vte_draw_get_visual(struct _vte_draw *draw)
{
	g_return_val_if_fail(draw->impl != NULL, NULL);
	g_return_val_if_fail(draw->impl->get_visual != NULL, NULL);
	return draw->impl->get_visual(draw);
}

GdkColormap *
_vte_draw_get_colormap(struct _vte_draw *draw)
{
	g_return_val_if_fail(draw->impl != NULL, NULL);
	g_return_val_if_fail(draw->impl->get_colormap != NULL, NULL);
	return draw->impl->get_colormap(draw);
}

void
_vte_draw_start(struct _vte_draw *draw)
{
	g_return_if_fail(GTK_WIDGET_REALIZED(draw->widget));
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->start != NULL);
	g_object_ref(G_OBJECT(draw->widget->window));
	draw->impl->start(draw);
	draw->started = TRUE;
}

void
_vte_draw_end(struct _vte_draw *draw)
{
	g_return_if_fail(draw->started == TRUE);
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->end != NULL);
	draw->impl->end(draw);
	g_object_unref(G_OBJECT(draw->widget->window));
	draw->started = FALSE;
}

void
_vte_draw_set_background_color(struct _vte_draw *draw, GdkColor *color)
{
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->set_background_color != NULL);
	draw->impl->set_background_color(draw, color);
}

void
_vte_draw_set_background_pixbuf(struct _vte_draw *draw, GdkPixbuf *pixbuf)
{
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->set_background_pixbuf != NULL);
	draw->impl->set_background_pixbuf(draw, pixbuf);
}
void
_vte_draw_clear(struct _vte_draw *draw, gint x, gint y, gint width, gint height)
{
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->clear != NULL);
	draw->impl->clear(draw, x, y, width, height);
}

void
_vte_draw_set_text_font(struct _vte_draw *draw,
			const PangoFontDescription *fontdesc)
{
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->set_text_font != NULL);
	draw->impl->set_text_font(draw, fontdesc);
}

int
_vte_draw_get_text_width(struct _vte_draw *draw)
{
	g_return_val_if_fail(draw->impl != NULL, 1);
	g_return_val_if_fail(draw->impl->get_text_width != NULL, 1);
	return draw->impl->get_text_width(draw);
}

int
_vte_draw_get_text_height(struct _vte_draw *draw)
{
	g_return_val_if_fail(draw->impl != NULL, 1);
	g_return_val_if_fail(draw->impl->get_text_height != NULL, 1);
	return draw->impl->get_text_height(draw);
}

int
_vte_draw_get_text_ascent(struct _vte_draw *draw)
{
	g_return_val_if_fail(draw->impl != NULL, 1);
	g_return_val_if_fail(draw->impl->get_text_ascent != NULL, 1);
	return draw->impl->get_text_ascent(draw);
}

gboolean
_vte_draw_get_using_fontconfig(struct _vte_draw *draw)
{
	g_return_val_if_fail(draw->impl != NULL, 1);
	g_return_val_if_fail(draw->impl->get_using_fontconfig != NULL, FALSE);
	return draw->impl->get_using_fontconfig(draw);
}

void
_vte_draw_text(struct _vte_draw *draw,
	       struct _vte_draw_text_request *requests, gsize n_requests,
	       GdkColor *color, guchar alpha)
{
	g_return_if_fail(draw->started == TRUE);
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->draw_text != NULL);
	draw->impl->draw_text(draw, requests, n_requests, color, alpha);
}

void
_vte_draw_fill_rectangle(struct _vte_draw *draw,
			 gint x, gint y, gint width, gint height,
			 GdkColor *color, guchar alpha)
{
	g_return_if_fail(draw->started == TRUE);
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->fill_rectangle != NULL);
	draw->impl->fill_rectangle(draw, x, y, width, height, color, alpha);
}

void
_vte_draw_draw_rectangle(struct _vte_draw *draw,
			 gint x, gint y, gint width, gint height,
			 GdkColor *color, guchar alpha)
{
	g_return_if_fail(draw->started == TRUE);
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->draw_rectangle != NULL);
	draw->impl->draw_rectangle(draw, x, y, width, height, color, alpha);
}

void
_vte_draw_set_scroll(struct _vte_draw *draw, gint x, gint y)
{
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->set_scroll != NULL);
	return draw->impl->set_scroll(draw, x, y);
}
