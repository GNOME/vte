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
#include "vtefc.h"
#include "vtegl.h"
#include "vteglyph.h"

struct _vte_gl_data
{
	XVisualInfo *visual_info;
	GLXContext context;
	GdkColor color;
	GdkPixmap *bgpixmap, *pixmap;
	GLXPixmap glpixmap;
	gint scrollx, scrolly;
	struct _vte_glyph_cache *cache;
};

#define _vte_gl_pixmap_attributes GLX_RGBA, None,

static gboolean
_vte_gl_check(struct _vte_draw *draw, GtkWidget *widget)
{
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
#ifdef VTE_DEBUG
		g_warning("Unable to use GLX.\n");
#endif
		return FALSE;
	}

	/* See if a suitable visual exists. */
	visual_info = glXChooseVisual(display, screen,
				      pixmap_attributes);
	if (visual_info == NULL) {
#ifdef VTE_DEBUG
		g_warning("Unable to find a suitable GLX visual.\n");
#endif
		return FALSE;
	}

	/* Create a GLX context. */
	context = glXCreateContext(display, visual_info, NULL, False);
	if (context == NULL) {
#ifdef VTE_DEBUG
		g_warning("Unable to create a GLX context.\n");
#endif
		return FALSE;
	}
	glXDestroyContext(display, context);

	return TRUE;
}

static void
_vte_gl_create(struct _vte_draw *draw, GtkWidget *widget)
{
	struct _vte_gl_data *data;
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
	if (data->visual_info == NULL) {
		g_error("Unable to select GLX visual.\n");
	}
	data->context = glXCreateContext(display, data->visual_info,
					 NULL, False);
	if (data->context == NULL) {
		g_error("Unable to create GLX context.\n");
	}

	data->color.red = 0;
	data->color.green = 0;
	data->color.blue = 0;
	data->bgpixmap = NULL;
	data->pixmap = NULL;
	data->glpixmap = -1;
	data->scrollx = data->scrolly = 0;
	data->cache = _vte_glyph_cache_new();
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

	_vte_glyph_cache_free(data->cache);
	data->cache = NULL;

	if (data->glpixmap != -1) {
		glXDestroyGLXPixmap(display, data->glpixmap);
	}
	data->glpixmap = -1;

	if (GDK_IS_PIXBUF(data->pixmap)) {
		g_object_unref(G_OBJECT(data->pixmap));
	}
	data->pixmap = NULL;

	if (GDK_IS_PIXBUF(data->bgpixmap)) {
		g_object_unref(G_OBJECT(data->bgpixmap));
	}
	data->bgpixmap = NULL;

	glXDestroyContext(display, data->context);
	data->context = NULL;

	data->scrollx = data->scrolly = 0;

	memset(&data->color, 0, sizeof(data->color));

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
	gint width, height, depth;

	data = (struct _vte_gl_data*) draw->impl_data;
	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);

	if (data->glpixmap != -1) {
		glXDestroyGLXPixmap(display, data->glpixmap);
		data->glpixmap = -1;
	}
	if (GDK_IS_PIXMAP(data->pixmap)) {
		g_object_unref(G_OBJECT(data->pixmap));
	}

	width = height = depth = 0;
	gdk_drawable_get_size(draw->widget->window, &width, &height);
	depth = gdk_drawable_get_depth(draw->widget->window);
	data->pixmap = gdk_pixmap_new(draw->widget->window,
				      width, height, depth);
	data->glpixmap = glXCreateGLXPixmap(display, data->visual_info,
					    GDK_WINDOW_XID(data->pixmap));

	glXMakeCurrent(display, data->glpixmap, data->context);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, width - 1, 0, height - 1);
	glViewport(0, 0, width, height);
	glEnable(GL_BLEND);
	glEnable(GL_POINT_SMOOTH);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void
_vte_gl_end(struct _vte_draw *draw)
{
	struct _vte_gl_data *data;
	GdkDisplay *gdisplay;
	Display *display;
	int state, width, height;

	data = (struct _vte_gl_data*) draw->impl_data;
	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);

	glXMakeCurrent(display, data->glpixmap, data->context);

	glFlush();
	glXWaitX();
	glXWaitGL();

	if (GDK_IS_PIXMAP(data->pixmap)) {
		state = GTK_WIDGET_STATE(draw->widget);
		width = height = 0;
		gdk_drawable_get_size(data->pixmap, &width, &height);
		gdk_draw_drawable(draw->widget->window,
				  draw->widget->style->fg_gc[state],
				  data->pixmap,
				  0, 0, 0, 0,
				  width, height);
	}
	if (data->glpixmap != -1) {
		glXDestroyGLXPixmap(display, data->glpixmap);
		data->glpixmap = -1;
	}
	if (GDK_IS_PIXMAP(data->pixmap)) {
		g_object_unref(G_OBJECT(data->pixmap));
	}
	data->pixmap = NULL;
}

static void
_vte_gl_set_background_color(struct _vte_draw *draw, GdkColor *color)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;
	data->color = *color;
}

static void
_vte_gl_set_background_image(struct _vte_draw *draw,
			     enum VteBgSourceType type,
			     GdkPixbuf *pixbuf,
			     const char *file,
			     const GdkColor *tint,
			     double saturation)
{
	struct _vte_gl_data *data;
	GdkPixmap *bgpixmap;

	data = (struct _vte_gl_data*) draw->impl_data;
	bgpixmap = vte_bg_get_pixmap(vte_bg_get(), type, pixbuf, file,
				     tint, saturation,
				     gdk_drawable_get_colormap(draw->widget->window));
	if (GDK_IS_PIXMAP(data->bgpixmap)) {
		g_object_unref(G_OBJECT(data->bgpixmap));
	}
	data->bgpixmap = bgpixmap;
}

static void
_vte_gl_clear(struct _vte_draw *draw,
		 gint x, gint y, gint width, gint height)
{
	GdkDisplay *gdisplay;
	Display *display;
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;
	long xstop, ystop, i, j;
	int pixmapw, pixmaph, w, h;

	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);

	glXMakeCurrent(display, data->glpixmap, data->context);

	if (GDK_IS_PIXMAP(data->bgpixmap)) {
		gdk_drawable_get_size(data->bgpixmap, &pixmapw, &pixmaph);
	} else {
		pixmapw = pixmaph = 0;
	}
	if ((pixmapw == 0) || (pixmaph == 0)) {
		glXWaitX();
		gdk_drawable_get_size(data->pixmap, &w, &h);
		glBegin(GL_POLYGON);
		glColor4us(data->color.red, data->color.green, data->color.blue,
			   0xffff);
		glVertex2d(x, h - y);
		glVertex2d(x + width, h - y);
		glVertex2d(x + width, h - (y + height));
		glVertex2d(x, h - (y + height));
		glEnd();
		return;
	}

	/* Flood fill. */
	glXWaitGL();

	xstop = x + width;
	ystop = y + height;

	y = ystop - height;
	j = (data->scrolly + y) % pixmaph;
	while (y < ystop) {
		x = xstop - width;
		i = (data->scrollx + x) % pixmapw;
		h = MIN(pixmaph - (j % pixmaph), ystop - y);
		while (x < xstop) {
			w = MIN(pixmapw - (i % pixmapw), xstop - x);
			gdk_draw_drawable(data->pixmap,
					  draw->widget->style->fg_gc[GTK_WIDGET_STATE(draw->widget)],
					  data->bgpixmap,
					  i, j,
					  x, y,
					  w, h);
			x += w;
			i = 0;
		}
		y += h;
		j = 0;
	}
}

static void
_vte_gl_fcpattern_disable_rgba(FcPattern *pattern, gpointer data)
{
	int rgba;
	if (FcPatternGetInteger(pattern,
				FC_RGBA, 0, &rgba) != FcResultNoMatch) {
		FcPatternDel(pattern, FC_RGBA);
	}
	FcPatternAddInteger(pattern, FC_RGBA, FC_RGBA_NONE);
}

static void
_vte_gl_set_text_font(struct _vte_draw *draw,
		      const PangoFontDescription *fontdesc)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;

	if (data->cache != NULL) {
		_vte_glyph_cache_free(data->cache);
		data->cache = NULL;
	}
	data->cache = _vte_glyph_cache_new();
	_vte_glyph_cache_set_font_description(NULL, data->cache, fontdesc,
					      _vte_gl_fcpattern_disable_rgba,
					      NULL);
}

static int
_vte_gl_get_text_width(struct _vte_draw *draw)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;
	return data->cache->width;
}

static int
_vte_gl_get_text_height(struct _vte_draw *draw)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;
	return data->cache->height;
}

static int
_vte_gl_get_text_ascent(struct _vte_draw *draw)
{
	struct _vte_gl_data *data;
	data = (struct _vte_gl_data*) draw->impl_data;
	return data->cache->ascent;
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
	GdkDisplay *gdisplay;
	Display *display;
	struct _vte_gl_data *data;
	const struct _vte_glyph *glyph;
	guint16 a, r, g, b;
	int width, height;
	int i, j, x, y, w, pad;

	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);

	data = (struct _vte_gl_data*) draw->impl_data;

	glXMakeCurrent(display, data->glpixmap, data->context);
	glXWaitX();

	r = color->red;
	g = color->green;
	b = color->blue;

	gdk_drawable_get_size(data->pixmap, &width, &height);

	glBegin(GL_POINTS);
	for (i = 0; i < n_requests; i++) {
		glyph = _vte_glyph_get(data->cache, requests[i].c);
		if ((glyph == NULL) ||
		    (glyph->width == 0) ||
		    (glyph->height == 0)) {
			continue;
		}
		w = requests[i].columns * data->cache->width;
		pad = (w - glyph->width) / 2;
		for (y = 0; y < glyph->height; y++) {
			for (x = 0; x < glyph->width; x++) {
				j = (y * glyph->width + x) *
				    glyph->bytes_per_pixel;
				a = glyph->bytes[j] * alpha;
				glColor4us(r, g, b, a);
				glVertex2i(requests[i].x + pad + x,
					   height - (requests[i].y + glyph->skip + y));
			}
		}
	}
	glEnd();
}

static void
_vte_gl_draw_rectangle(struct _vte_draw *draw,
		       gint x, gint y, gint width, gint height,
		       GdkColor *color, guchar alpha)
{
	GdkDisplay *gdisplay;
	Display *display;
	struct _vte_gl_data *data;
	int w, h;

	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);

	data = (struct _vte_gl_data*) draw->impl_data;

	glXMakeCurrent(display, data->glpixmap, data->context);
	glXWaitX();

	gdk_drawable_get_size(data->pixmap, &w, &h);

	glBegin(GL_LINE_LOOP);
	glColor4us(color->red, color->green, color->blue,
		   (alpha == VTE_DRAW_OPAQUE) ? 0xffff : (alpha << 8));
	glVertex2d(x, h - y);
	glVertex2d(x + width, h - y);
	glVertex2d(x + width, h - (y + height));
	glVertex2d(x, h - (y + height));
	glEnd();
}

static void
_vte_gl_fill_rectangle(struct _vte_draw *draw,
		       gint x, gint y, gint width, gint height,
		       GdkColor *color, guchar alpha)
{
	GdkDisplay *gdisplay;
	Display *display;
	struct _vte_gl_data *data;
	int w, h;

	gdisplay = gdk_display_get_default();
	display = gdk_x11_display_get_xdisplay(gdisplay);

	data = (struct _vte_gl_data*) draw->impl_data;

	glXMakeCurrent(display, data->glpixmap, data->context);
	glXWaitX();

	gdk_drawable_get_size(data->pixmap, &w, &h);

	glBegin(GL_POLYGON);
	glColor4us(color->red, color->green, color->blue,
		   (alpha == VTE_DRAW_OPAQUE) ? 0xffff : (alpha << 8));
	glVertex2d(x, h - y);
	glVertex2d(x + width, h - y);
	glVertex2d(x + width, h - (y + height));
	glVertex2d(x, h - (y + height));
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
	_vte_gl_set_background_image,
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
