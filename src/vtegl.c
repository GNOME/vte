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

#ident "$Id$"

#include "../config.h"

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(2,2,0)
#ifndef X_DISPLAY_MISSING
#ifdef HAVE_GL

#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <X11/Xutil.h>
#include "debug.h"
#include "vtedraw.h"
#include "vtegl.h"

struct _vte_gl_data
{
	XVisualInfo *visual_info;
	gboolean double_buffered;
	GLXContext context;
	GLXPixmap pixmap;
	GdkColor color;
	GdkPixbuf *pixbuf;
	gint scrollx, scrolly;
};

#define _vte_gl_pixmap_attributes \
	GLX_RGBA, \
	GLX_RED_SIZE, 8, \
	GLX_BLUE_SIZE, 8, \
	GLX_GREEN_SIZE, 8, \
	GLX_ALPHA_SIZE, 8, \
	None,
#define _vte_gl_window_attributes \
	GLX_RGBA, \
	GLX_RED_SIZE, 8, \
	GLX_BLUE_SIZE, 8, \
	GLX_GREEN_SIZE, 8, \
	GLX_ALPHA_SIZE, 8, \
	GLX_DOUBLEBUFFER, \
	None,

static gboolean
_vte_gl_check(struct _vte_draw *draw, GtkWidget *widget)
{
	int window_attributes[] = {_vte_gl_window_attributes};
	int pixmap_attributes[] = {_vte_gl_pixmap_attributes};
	XVisualInfo *visual_info;
	GLXContext context = NULL;
	GdkDisplay *gdisplay;
	Display *display;
	GdkScreen *gscreen;
	int screen;
	int error, event;

	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);
	gscreen = gdk_screen_get_default();
	screen = gdk_x11_screen_get_screen_number(gscreen);

	/* Check for GLX. */
	if (!glXQueryExtension(display, &error, &event)) {
		g_warning("Unable to use GLX.\n");
		return FALSE;
	}

	/* See if a suitable visual exists. */
	visual_info = glXChooseVisual(display, screen,
				      pixmap_attributes);
	if (visual_info == NULL) {
		visual_info = glXChooseVisual(display, screen,
					      window_attributes);
	}
	if (visual_info == NULL) {
		return FALSE;
	}

	/* Create a GLX context. */
	context = glXCreateContext(display, visual_info, NULL, False);
	if (context == NULL) {
		return FALSE;
	}

	return TRUE;
}

static void
_vte_gl_create(struct _vte_draw *draw, GtkWidget *widget)
{
	struct _vte_gl_data *data;
	int window_attributes[] = {_vte_gl_window_attributes};
	int pixmap_attributes[] = {_vte_gl_pixmap_attributes};
	GdkDisplay *gdisplay;
	Display *display;
	GdkScreen *gscreen;
	int screen;

	draw->impl_data = g_malloc(sizeof(struct _vte_gl_data));
	data = (struct _vte_gl_data*) draw->impl_data;

	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);
	gscreen = gdk_screen_get_default();
	screen = gdk_x11_screen_get_screen_number(gscreen);

	data->visual_info = glXChooseVisual(display, screen,
					    pixmap_attributes);
	if (data->visual_info != NULL) {
		data->double_buffered = FALSE;
	} else {
		data->visual_info = glXChooseVisual(display, screen,
						    window_attributes);
		data->double_buffered = TRUE;
	}
	gtk_widget_set_double_buffered(widget, !data->double_buffered);

	data->context = glXCreateContext(display, data->visual_info,
					 NULL, False);
	if (data->context == NULL) {
		g_error("Unable to create GLX context.\n");
	}

	data->color.red = 0;
	data->color.green = 0;
	data->color.blue = 0;
	data->pixbuf = NULL;
	data->pixmap = -1;
	data->scrollx = data->scrolly = 0;
}

static void
_vte_gl_destroy(struct _vte_draw *draw)
{
	struct _vte_gl_data *data;
	GdkDisplay *gdisplay;
	Display *display;
	GdkScreen *gscreen;
	int screen;

	data = (struct _vte_gl_data*) draw->impl_data;
	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);
	gscreen = gdk_screen_get_default();
	screen = gdk_x11_screen_get_screen_number(gscreen);

	if (data->pixmap != -1) {
		glXDestroyGLXPixmap(display, data->pixmap);
	}
	glXDestroyContext(display, data->context);

	data->scrollx = data->scrolly = 0;

	if (GDK_IS_PIXBUF(data->pixbuf)) {
		g_object_unref(G_OBJECT(data->pixbuf));
	}
	data->pixbuf = NULL;
	memset(&data->color, 0, sizeof(data->color));

	glXDestroyContext(display, data->context);

	g_free(draw->impl_data);
}

static GdkVisual *
_vte_gl_get_visual(struct _vte_draw *draw)
{
	GdkScreen *gscreen;
	struct _vte_gl_data *data;

	data = (struct _vte_gl_data*) draw->impl_data;
	gscreen = gdk_screen_get_default();
	return gdk_x11_screen_lookup_visual(gscreen,
					    data->visual_info->visualid);
}

static GdkColormap *
_vte_gl_get_colormap(struct _vte_draw *draw)
{
	return NULL;
}

static void
_vte_gl_start(struct _vte_draw *draw)
{
	struct _vte_gl_data *data;
	GdkDisplay *gdisplay;
	Display *display;
	GdkScreen *gscreen;
	int screen;
	guint width, height;
	GdkDrawable *drawable;
	gint x_offset, y_offset;
	GLXDrawable glx_drawable = -1;

	data = (struct _vte_gl_data*) draw->impl_data;
	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);
	gscreen = gdk_screen_get_default();
	screen = gdk_x11_screen_get_screen_number(gscreen);

	gdk_window_get_internal_paint_info(draw->widget->window,
					   &drawable,
					   &x_offset,
					   &y_offset);
	if (GDK_IS_PIXMAP(drawable)) {
		gdk_drawable_get_size(drawable, &width, &height);
		data->pixmap = glXCreateGLXPixmap(display, data->visual_info,
						  GDK_WINDOW_XID(drawable));
		glx_drawable = data->pixmap;
	} else
	if (GDK_IS_WINDOW(draw->widget->window)) {
		gdk_drawable_get_size(draw->widget->window, &width, &height);
		x_offset = y_offset = 0;
		glx_drawable = GDK_WINDOW_XID(drawable);
	} else {
		g_assert_not_reached();
	}

	glXMakeCurrent(display, glx_drawable, data->context);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
        gluOrtho2D(x_offset, width, y_offset, height);
        glViewport(x_offset, y_offset, width - x_offset, height - y_offset);

	glClearColor(data->color.red / 65535.0,
		     data->color.green / 65535.0,
		     data->color.blue / 65535.0,
		     1.0);
	glClear(GL_COLOR_BUFFER_BIT);
}

static void
_vte_gl_end(struct _vte_draw *draw)
{
	struct _vte_gl_data *data;
	GdkDisplay *gdisplay;
	Display *display;
	GdkScreen *gscreen;
	int screen;

	data = (struct _vte_gl_data*) draw->impl_data;
	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);
	gscreen = gdk_screen_get_default();
	screen = gdk_x11_screen_get_screen_number(gscreen);

	glFlush();
	if (data->double_buffered) {
		glXSwapBuffers(display, GDK_WINDOW_XID(draw->widget->window));
	} else {
		glXDestroyGLXPixmap(display, data->pixmap);
		data->pixmap = -1;
	}
}

static void
_vte_gl_set_background_color(struct _vte_draw *draw, GdkColor *color)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;
	data->color = *color;
}

static void
_vte_gl_set_background_pixbuf(struct _vte_draw *draw, GdkPixbuf *pixbuf)
{
	struct _vte_gl_data *data;

	data = (struct _vte_gl_data*) draw->impl_data;
	if (GDK_IS_PIXBUF(pixbuf)) {
		g_object_ref(G_OBJECT(pixbuf));
	}
	if (GDK_IS_PIXBUF(data->pixbuf)) {
		g_object_unref(G_OBJECT(data->pixbuf));
	}
	data->pixbuf = pixbuf;
}

static void
_vte_gl_clear(struct _vte_draw *draw,
		 gint x, gint y, gint width, gint height)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;

	glBegin(GL_POLYGON);
	glColor4us(data->color.red, data->color.green, data->color.blue,
		   0xffff);
	glVertex2d(x, y);
	glVertex2d(x + width, y);
	glVertex2d(x + width, y + height);
	glVertex2d(x, y + height);
	glEnd();
}

static void
_vte_gl_set_text_font(struct _vte_draw *draw,
			 const PangoFontDescription *fontdesc)
{
}

static int
_vte_gl_get_text_width(struct _vte_draw *draw)
{
	return 5;
}

static int
_vte_gl_get_text_height(struct _vte_draw *draw)
{
	return 10;
}

static int
_vte_gl_get_text_ascent(struct _vte_draw *draw)
{
	return 8;
}

static gboolean
_vte_gl_get_using_fontconfig(struct _vte_draw *draw)
{
	return TRUE;
}

static void
_vte_gl_draw_text(struct _vte_draw *draw,
		  struct _vte_draw_text_request *requests, gsize n_requests,
		  GdkColor *color, guchar alpha)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;
}

static void
_vte_gl_draw_rectangle(struct _vte_draw *draw,
		       gint x, gint y, gint width, gint height,
		       GdkColor *color, guchar alpha)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;
	glBegin(GL_LINE_LOOP);
	glColor4us(color->red, color->green, color->blue,
		   (alpha == VTE_DRAW_OPAQUE) ? 0xffff : (alpha << 8));
	glVertex2d(x, y);
	glVertex2d(x + width, y);
	glVertex2d(x + width, y + height);
	glVertex2d(x, y + height);
	glEnd();
}

static void
_vte_gl_fill_rectangle(struct _vte_draw *draw,
		       gint x, gint y, gint width, gint height,
		       GdkColor *color, guchar alpha)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;
	glBegin(GL_POLYGON);
	glColor4us(color->red, color->green, color->blue,
		   (alpha == VTE_DRAW_OPAQUE) ? 0xffff : (alpha << 8));
	glVertex2d(x, y);
	glVertex2d(x + width, y);
	glVertex2d(x + width, y + height);
	glVertex2d(x, y + height);
	glEnd();
}

static void
_vte_gl_set_scroll(struct _vte_draw *draw, gint x, gint y)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;
	data->scrollx = x;
	data->scrolly = y;
}

struct _vte_draw_impl _vte_draw_gl = {
	"VteGL", "VTE_USE_GL",
	_vte_gl_check,
	_vte_gl_create,
	_vte_gl_destroy,
	_vte_gl_get_visual,
	_vte_gl_get_colormap,
	_vte_gl_start,
	_vte_gl_end,
	_vte_gl_set_background_color,
	_vte_gl_set_background_pixbuf,
	_vte_gl_clear,
	_vte_gl_set_text_font,
	_vte_gl_get_text_width,
	_vte_gl_get_text_height,
	_vte_gl_get_text_ascent,
	_vte_gl_get_using_fontconfig,
	_vte_gl_draw_text,
	_vte_gl_draw_rectangle,
	_vte_gl_fill_rectangle,
	_vte_gl_set_scroll,
};

#endif
#endif
#endif
