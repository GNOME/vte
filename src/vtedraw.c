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

GdkVisual *
_vte_draw_get_visual (struct _vte_draw *draw)
{
	_vte_debug_print (VTE_DEBUG_DRAW, "draw_get_visual\n");

	return gtk_widget_get_visual (draw->widget);
}

GdkColormap *
_vte_draw_get_colormap (struct _vte_draw *draw, gboolean maybe_use_default)
{
	GdkColormap *colormap;

	_vte_debug_print (VTE_DEBUG_DRAW, "draw_get_colormap\n");

	colormap = gtk_widget_get_colormap (draw->widget);

	if (colormap == NULL && maybe_use_default) {
		colormap = gdk_screen_get_default_colormap (gtk_widget_get_screen (draw->widget));
	}

	return colormap;
}

gboolean
_vte_draw_requires_clear (struct _vte_draw *draw)
{
	return draw->requires_clear;
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
