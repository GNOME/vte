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


#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "debug.h"
#include "vtedraw.h"
#include "vtepangocairo.h"
#include "vteskel.h"

GdkVisual *
_vte_draw_get_visual (struct _vte_draw *draw)
{
	GdkVisual *visual = NULL;

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_get_visual\n");

	if (draw->impl->get_visual)
		visual = draw->impl->get_visual (draw);

	return visual ? visual : gtk_widget_get_visual (draw->widget);
}

GdkColormap *
_vte_draw_get_colormap (struct _vte_draw *draw, gboolean maybe_use_default)
{
	GdkColormap *colormap;

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_get_colormap\n");

	if (draw->impl->get_colormap)
		colormap = draw->impl->get_colormap (draw);
	else
		colormap = gtk_widget_get_colormap (draw->widget);

	if (colormap == NULL && maybe_use_default) {
		colormap = gdk_screen_get_default_colormap (gtk_widget_get_screen (draw->widget));
	}

	return colormap;
}

void
_vte_draw_start (struct _vte_draw *draw)
{
	g_return_if_fail (GTK_WIDGET_REALIZED (draw->widget));

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_start\n");

	g_object_ref (draw->widget->window);

	if (draw->impl->start)
		draw->impl->start (draw);

	draw->started = TRUE;
}

void
_vte_draw_end (struct _vte_draw *draw)
{
	g_return_if_fail (draw->started == TRUE);

	if (draw->impl->end)
		draw->impl->end (draw);

	g_object_unref (draw->widget->window);

	draw->started = FALSE;

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_end\n");
}

void
_vte_draw_set_background_solid(struct _vte_draw *draw,
			       GdkColor *color,
			       guint16 opacity)
{
	draw->requires_clear = draw->impl->always_requires_clear || opacity != 0xFFFF;

	if (draw->impl->set_background_solid)
		draw->impl->set_background_solid (draw, color, opacity);
}

void
_vte_draw_set_background_image (struct _vte_draw *draw,
			        enum VteBgSourceType type,
			        GdkPixbuf *pixbuf,
			        const char *filename,
			        const GdkColor *color,
			        double saturation)
{
	if (type != VTE_BG_SOURCE_NONE)
		draw->requires_clear = TRUE;

	if (draw->impl->set_background_image)
		draw->impl->set_background_image (draw, type, pixbuf, filename,
						  color, saturation);
}

void
_vte_draw_set_background_scroll (struct _vte_draw *draw,
				 gint x, gint y)
{
	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_set_scroll (%d, %d)\n",
			x, y);

	if (draw->impl->set_background_scroll)
		draw->impl->set_background_scroll (draw, x, y);
}

gboolean
_vte_draw_requires_clear (struct _vte_draw *draw)
{
	return draw->requires_clear;
}

gboolean
_vte_draw_clip (struct _vte_draw *draw, GdkRegion *region)
{
	gboolean clip = FALSE;
	_vte_debug_print (VTE_DEBUG_DRAW, "draw_clip\n");

	if (draw->impl->clip) {
		draw->impl->clip (draw, region);
		clip = TRUE;
	}

	return clip;
}

void
_vte_draw_clear (struct _vte_draw *draw, gint x, gint y, gint width, gint height)
{
	g_return_if_fail (draw->impl->clear != NULL);

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_clear (%d, %d, %d, %d)\n",
			  x,y,width, height);

	draw->impl->clear (draw, x, y, width, height);
}

void
_vte_draw_set_text_font (struct _vte_draw *draw,
			const PangoFontDescription *fontdesc,
			VteTerminalAntiAlias anti_alias)
{
	_vte_debug_print (VTE_DEBUG_DRAW, "draw_set_text_font (aa=%d)\n",
			  anti_alias);

	if (draw->impl->set_text_font)
		draw->impl->set_text_font (draw, fontdesc, anti_alias);
}

void
_vte_draw_get_text_metrics(struct _vte_draw *draw,
			   gint *width, gint *height, gint *ascent)
{
	gint swidth = 0, sheight = 0, sascent = 0;

	g_return_if_fail (draw->impl->get_text_metrics != NULL);

	draw->impl->get_text_metrics (draw, &swidth, &sheight, &sascent);

	if (width)  *width  = swidth;
	if (height) *height = sheight;
	if (ascent) *ascent = sascent;
}

int
_vte_draw_get_char_width (struct _vte_draw *draw, vteunistr c, int columns,
			  gboolean bold)
{
	int width = 0;

	if (draw->impl->get_char_width)
		width = draw->impl->get_char_width (draw, c, columns, bold);

	if (width == 0)
		_vte_draw_get_text_metrics (draw, &width, NULL, NULL);

	return width;
}

void
_vte_draw_text (struct _vte_draw *draw,
	       struct _vte_draw_text_request *requests, gsize n_requests,
	       GdkColor *color, guchar alpha, gboolean bold)
{
	g_return_if_fail (draw->started == TRUE);
	g_return_if_fail (draw->impl->draw_text != NULL);

	if (_vte_debug_on (VTE_DEBUG_DRAW)) {
		GString *string = g_string_new ("");
		gchar *str;
		gsize n;
		for (n = 0; n < n_requests; n++) {
			g_string_append_unichar (string, requests[n].c);
		}
		str = g_string_free (string, FALSE);
		g_printerr ("draw_text (\"%s\", len=%"G_GSIZE_FORMAT", color=(%d,%d,%d,%d), %s)\n",
				str, n_requests, color->red, color->green, color->blue,
				alpha, bold ? "bold" : "normal");
		g_free (str);
	}

	draw->impl->draw_text (draw, requests, n_requests, color, alpha, bold);

	/* handle fonts that lack a bold face by double-striking */
	if (bold && !(draw->impl->has_bold && draw->impl->has_bold (draw))) {
		gsize i;

		/* Take a step to the right. */
		for (i = 0; i < n_requests; i++) {
			requests[i].x++;
		}
		draw->impl->draw_text (draw, requests, n_requests, color, alpha, FALSE);
		/* Now take a step back. */
		for (i = 0; i < n_requests; i++) {
			requests[i].x--;
		}
	}
}

gboolean
_vte_draw_char (struct _vte_draw *draw,
	       struct _vte_draw_text_request *request,
	       GdkColor *color, guchar alpha, gboolean bold)
{
	gboolean has_char;

	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_char ('%c', color=(%d,%d,%d,%d), %s)\n",
			request->c,
			color->red, color->green, color->blue,
			alpha, bold ? "bold" : "normal");

	has_char =_vte_draw_has_char (draw, request->c, bold);
	if (has_char)
		_vte_draw_text (draw, request, 1, color, alpha, bold);

	return has_char;
}
gboolean
_vte_draw_has_char (struct _vte_draw *draw, vteunistr c, gboolean bold)
{
	gboolean has_char = TRUE;

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_has_char ('0x%04X', %s)\n", c,
			  bold ? "bold" : "normal");

	if (draw->impl->has_char)
		has_char = draw->impl->has_char (draw, c, bold);

	return has_char;
}

void
_vte_draw_fill_rectangle (struct _vte_draw *draw,
			 gint x, gint y, gint width, gint height,
			 GdkColor *color, guchar alpha)
{
	g_return_if_fail (draw->started == TRUE);
	g_return_if_fail (draw->impl->fill_rectangle != NULL);

	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_fill_rectangle (%d, %d, %d, %d, color=(%d,%d,%d,%d))\n",
			x,y,width,height,
			color->red, color->green, color->blue,
			alpha);

	draw->impl->fill_rectangle (draw, x, y, width, height, color, alpha);
}

void
_vte_draw_draw_rectangle (struct _vte_draw *draw,
			 gint x, gint y, gint width, gint height,
			 GdkColor *color, guchar alpha)
{
	g_return_if_fail (draw->started == TRUE);

	_vte_debug_print (VTE_DEBUG_DRAW,
			"draw_rectangle (%d, %d, %d, %d, color=(%d,%d,%d,%d))\n",
			x,y,width,height,
			color->red, color->green, color->blue,
			alpha);

	if (draw->impl->draw_rectangle)
		draw->impl->draw_rectangle (draw, x, y, width, height, color, alpha);
	else {
		if (width > 0) {
			_vte_draw_fill_rectangle (draw, x, y, width-1, 1, color, alpha);
			_vte_draw_fill_rectangle (draw, x+1, y+height-1, width-1, 1, color, alpha);
		}
		if (height > 0) {
			_vte_draw_fill_rectangle (draw, x, y+1, 1, height-1, color, alpha);
			_vte_draw_fill_rectangle (draw, x+width-1, y, 1, height-1, color, alpha);
		}
	}
}
