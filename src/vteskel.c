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


#include <config.h>

#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include "debug.h"
#include "vtebg.h"
#include "vtedraw.h"

static void
_vte_skel_clear(struct _vte_draw *draw,
		gint x, gint y, gint width, gint height)
{
	g_message ("_vte_skel_clear: %d,%d+%d,%d",
		   x, y, width, height);
}

static void
_vte_skel_get_text_metrics(struct _vte_draw *draw,
			   gint *width, gint *height, gint *ascent)
{
	g_message ("_vte_skel_get_text_metrics");
}

static void
_vte_skel_draw_text(struct _vte_draw *draw,
		    struct _vte_draw_text_request *requests, gsize n_requests,
		    GdkColor *color, guchar alpha)
{
	g_message ("_vte_skel_draw_text: %d chars",
		   n_requests);
}

static void
_vte_skel_fill_rectangle(struct _vte_draw *draw,
			 gint x, gint y, gint width, gint height,
			 GdkColor *color, guchar alpha)
{
	g_message ("_vte_skel_fill_rectangle: %d,%d+%d,%d",
		   x, y, width, height);
}

const struct _vte_draw_impl _vte_draw_skel = {
	"null",
	NULL, /* check */
	NULL, /* create */
	NULL, /* destroy */
	NULL, /* get_visual */
	NULL, /* get_colormap */
	NULL, /* start */
	NULL, /* end */
	NULL, /* set_background_solid */
	NULL, /* set_background_image */
	NULL, /* set_background_scroll */
	NULL, /* clip */
	TRUE, /* always_requires_clear */
	_vte_skel_clear,
	NULL, /* set_text_font */
	_vte_skel_get_text_metrics,
	NULL, /* get_char_width */
	NULL, /* has_bold */
	_vte_skel_draw_text,
	NULL, /* draw_has_char */
	NULL, /* draw_rectangle */
	_vte_skel_fill_rectangle
};
