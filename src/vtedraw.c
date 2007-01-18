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

static const struct _vte_draw_impl
*_vte_draw_impls[] = {
#ifndef X_DISPLAY_MISSING
#ifdef HAVE_XFT2
	&_vte_draw_xft,
#endif /* HAVE_XFT2 */
#endif /* !X_DISPLAY_MISSING */
	&_vte_draw_ft2,
#ifndef X_DISPLAY_MISSING
#ifdef HAVE_GL
	&_vte_draw_gl,
#endif /* HAVE_GL */
#endif /* !X_DISPLAY_MISSING */
	&_vte_draw_pango,
#ifndef X_DISPLAY_MISSING
#ifdef HAVE_PANGOX
	&_vte_draw_pango_x,
#endif /* HAVE_PANGOX */
#endif /* !X_DISPLAY_MISSING */
};

static gboolean
_vte_draw_init_user (struct _vte_draw *draw)
{
	const gchar *env;
	gchar **strv, **s;
	guint i;
	gboolean success = TRUE;

	env = g_getenv("VTE_BACKEND");
	if (!env) {
		return FALSE;
	}

	strv = g_strsplit(env, ":;, \t", -1);
	for (s = strv; *s; s++) {
		if (g_ascii_strcasecmp(*s, _vte_draw_skel.name) == 0) {
			draw->impl = &_vte_draw_skel;
			goto out;
		}
		for (i = 0; i < G_N_ELEMENTS(_vte_draw_impls); i++) {
			if (g_ascii_strcasecmp(*s, _vte_draw_impls[i]->name) == 0) {
				if (_vte_draw_impls[i]->check(draw, draw->widget)) {
					draw->impl = _vte_draw_impls[i];
					goto out;
				}
			}
		}
	}

	success = FALSE;
out:
	g_strfreev(strv);
	return success;
}


static gboolean
_vte_draw_init_default (struct _vte_draw *draw)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS(_vte_draw_impls); i++) {
		if (_vte_draw_impls[i]->check(draw, draw->widget)) {
			draw->impl = _vte_draw_impls[i];
			return TRUE;
		}
	}

	return FALSE;
}


struct _vte_draw *
_vte_draw_new(GtkWidget *widget)
{
	struct _vte_draw *draw;

	/* Create the structure. */
	draw = g_slice_new0(struct _vte_draw);
	draw->widget = g_object_ref(widget);
	draw->started = FALSE;

	/* Allow the user to specify her preferred backends */
	if (!_vte_draw_init_user (draw) &&
			/* Otherwise use the first thing that works */
			!_vte_draw_init_default (draw)) {
		/* Something has to work. */
		g_assert_not_reached ();
		draw->impl = &_vte_draw_skel;
	}

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_MISC)) {
		g_printerr("Using %s.\n", draw->impl->name);
	}
#endif

	draw->impl->create(draw, draw->widget);

	return draw;
}

void
_vte_draw_free(struct _vte_draw *draw)
{
	draw->impl->destroy(draw);

	if (draw->widget != NULL) {
		g_object_unref(draw->widget);
	}

	g_slice_free(struct _vte_draw, draw);
}

GdkVisual *
_vte_draw_get_visual(struct _vte_draw *draw)
{
	g_return_val_if_fail(draw->impl != NULL, NULL);
	g_return_val_if_fail(draw->impl->get_visual != NULL, NULL);
	return draw->impl->get_visual(draw);
}

GdkColormap *
_vte_draw_get_colormap(struct _vte_draw *draw, gboolean maybe_use_default)
{
	GdkColormap *colormap;
	GdkScreen *screen;
	g_return_val_if_fail(draw->impl != NULL, NULL);
	g_return_val_if_fail(draw->impl->get_colormap != NULL, NULL);
	colormap = draw->impl->get_colormap(draw);
	if (colormap) {
		return colormap;
	}
	if (!maybe_use_default) {
		return NULL;
	}
	if (gtk_widget_has_screen(draw->widget)) {
		screen = gtk_widget_get_screen(draw->widget);
	} else {
		screen = gdk_display_get_default_screen(gtk_widget_get_display(draw->widget));
	}
	colormap = gdk_screen_get_default_colormap(screen);
	return colormap;
}

void
_vte_draw_start(struct _vte_draw *draw)
{
	g_return_if_fail(GTK_WIDGET_REALIZED(draw->widget));
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->start != NULL);
	g_object_ref(draw->widget->window);
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
	g_object_unref(draw->widget->window);
	draw->started = FALSE;
}

void
_vte_draw_set_background_color(struct _vte_draw *draw,
			       GdkColor *color,
			       guint16 opacity)
{
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->set_background_color != NULL);
	draw->impl->set_background_color(draw, color, opacity);
}

void
_vte_draw_set_background_image(struct _vte_draw *draw,
			       enum VteBgSourceType type,
			       GdkPixbuf *pixbuf,
			       const char *filename,
			       const GdkColor *color,
			       double saturation)
{
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->set_background_image != NULL);
	draw->impl->set_background_image(draw, type, pixbuf, filename,
					 color, saturation);
}

gboolean
_vte_draw_requires_repaint(struct _vte_draw *draw)
{
	g_return_val_if_fail(draw->impl != NULL, TRUE);
	return draw->impl->requires_repaint;
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
			const PangoFontDescription *fontdesc,
			VteTerminalAntiAlias anti_alias)
{
	g_return_if_fail(draw->impl != NULL);
	g_return_if_fail(draw->impl->set_text_font != NULL);
	draw->impl->set_text_font(draw, fontdesc, anti_alias);
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

int
_vte_draw_get_char_width(struct _vte_draw *draw, gunichar c, int columns)
{
	g_return_val_if_fail(draw->impl != NULL, 1);
	g_return_val_if_fail(draw->impl->get_char_width != NULL, 1);
	return draw->impl->get_char_width(draw, c, columns);
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

gboolean
_vte_draw_char(struct _vte_draw *draw,
	       struct _vte_draw_text_request *request,
	       GdkColor *color, guchar alpha)
{
	g_return_val_if_fail(draw->started == TRUE, FALSE);
	g_return_val_if_fail(draw->impl != NULL, FALSE);
	if (draw->impl->draw_char == NULL) {
		draw->impl->draw_text(draw, request, 1, color, alpha);
		return TRUE;
	}
	return draw->impl->draw_char(draw, request, color, alpha);
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
	draw->impl->set_scroll(draw, x, y);
}
