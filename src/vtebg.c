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

#include "../config.h"
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include "debug.h"
#include "marshal.h"
#include "vtebg.h"

#include <glib/gi18n-lib.h>

G_DEFINE_TYPE(VteBg, vte_bg, G_TYPE_OBJECT)

struct VteBgPrivate {
	GList *cache;
};

struct VteBgCacheItem {
	enum VteBgSourceType source_type;
	GdkPixbuf *source_pixbuf;
	char *source_file;

	GdkColor tint_color;
	double saturation;

	GdkPixmap *pixmap;
	GdkPixbuf *pixbuf;
};


static GdkPixbuf *_vte_bg_resize_pixbuf(GdkPixbuf *pixbuf,
					gint min_width, gint min_height);
static void vte_bg_cache_item_free(struct VteBgCacheItem *item);
static void vte_bg_cache_prune_int(VteBg *bg, gboolean root);

#if 0
static const char *
vte_bg_source_name(enum VteBgSourceType type)
{
	switch (type) {
	case VTE_BG_SOURCE_NONE:
		return "none";
		break;
	case VTE_BG_SOURCE_ROOT:
		return "root";
		break;
	case VTE_BG_SOURCE_PIXBUF:
		return "pixbuf";
		break;
	case VTE_BG_SOURCE_FILE:
		return "file";
		break;
	}
	return "unknown";
}
#endif

#ifndef X_DISPLAY_MISSING

#include <gdk/gdkx.h>

struct VteBgNative {
	GdkDisplay *display;
	GdkWindow *window;
	XID native_window;
	GdkAtom atom;
	Atom native_atom;
};

static struct VteBgNative *
vte_bg_native_new(GdkWindow *window)
{
	struct VteBgNative *pvt;
	pvt = g_slice_new(struct VteBgNative);
	pvt->window = window;
	pvt->native_window = gdk_x11_drawable_get_xid(window);
	pvt->display = gdk_drawable_get_display(GDK_DRAWABLE(window));
	pvt->native_atom = gdk_x11_get_xatom_by_name_for_display(pvt->display, "_XROOTPMAP_ID");
	pvt->atom = gdk_x11_xatom_to_atom_for_display(pvt->display, pvt->native_atom);
	return pvt;
}

static void
_vte_bg_display_sync(VteBg *bg)
{
	gdk_display_sync(bg->native->display);
}

static gboolean
_vte_property_get_pixmaps(GdkWindow *window, GdkAtom atom,
			  GdkAtom *type, int *size,
			  XID **pixmaps)
{
	return gdk_property_get(window, atom, GDK_TARGET_PIXMAP,
				0, INT_MAX - 3,
				FALSE,
				type, NULL, size,
				(guchar**) pixmaps);
}

static GdkPixmap *
vte_bg_root_pixmap(VteBg *bg)
{
	GdkPixmap *pixmap;
	GdkAtom prop_type;
	int prop_size;
	XID *pixmaps;

	pixmap = NULL;
	pixmaps = NULL;
	gdk_error_trap_push();
	if (_vte_property_get_pixmaps(bg->native->window, bg->native->atom,
				      &prop_type, &prop_size,
				      &pixmaps)) {
		if ((prop_type == GDK_TARGET_PIXMAP) &&
		    (prop_size >= (int)sizeof(XID) &&
		    (pixmaps != NULL))) {
			pixmap = gdk_pixmap_foreign_new_for_display(bg->native->display, pixmaps[0]);
			_VTE_DEBUG_IF(VTE_DEBUG_MISC|VTE_DEBUG_EVENTS) {
				gint pwidth, pheight;
				gdk_drawable_get_size(pixmap,
						&pwidth, &pheight);
				g_printerr("New background image %dx%d\n",
						pwidth, pheight);
			}
		}
		g_free(pixmaps);
	}
	_vte_bg_display_sync(bg);
	gdk_error_trap_pop();
	return pixmap;
}

static void
vte_bg_set_root_pixmap(VteBg *bg, GdkPixmap *pixmap)
{
	if (bg->root_pixmap != NULL) {
		g_object_unref(bg->root_pixmap);
	}
	bg->root_pixmap = pixmap;
	vte_bg_cache_prune_int(bg, TRUE);
	g_signal_emit_by_name(bg, "root-pixmap-changed");
}


static GdkFilterReturn
vte_bg_root_filter(GdkXEvent *native, GdkEvent *event, gpointer data)
{
	XEvent *xevent = (XEvent*) native;
	VteBg *bg;
	GdkPixmap *pixmap;

	switch (xevent->type) {
	case PropertyNotify:
		bg = VTE_BG(data);
		if ((xevent->xproperty.window == bg->native->native_window) &&
		    (xevent->xproperty.atom == bg->native->native_atom)) {
			pixmap = vte_bg_root_pixmap(bg);
			vte_bg_set_root_pixmap(bg, pixmap);
		}
		break;
	default:
		break;
	}
	return GDK_FILTER_CONTINUE;
}

#else

struct VteBgNative {
	guchar filler;
};
static struct VteBgNative *
vte_bg_native_new(GdkWindow *window)
{
	return NULL;
}
static GdkFilterReturn
vte_bg_root_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	return GDK_FILTER_CONTINUE;
}
static void
_vte_bg_display_sync(VteBg *bg)
{
}

static GdkPixmap *
vte_bg_root_pixmap(VteBg *bg)
{
	return NULL;
}
#endif


static void
vte_bg_finalize (GObject *obj)
{
	VteBg *bg;

	bg = VTE_BG (obj);

	if (bg->pvt->cache) {
		g_list_foreach (bg->pvt->cache, (GFunc)vte_bg_cache_item_free, NULL);
		g_list_free (bg->pvt->cache);
	}

	G_OBJECT_CLASS(vte_bg_parent_class)->finalize (obj);
}

static void
vte_bg_class_init(VteBgClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	bindtextdomain(PACKAGE, LOCALEDIR);

	gobject_class->finalize = vte_bg_finalize;

	klass->root_pixmap_changed = g_signal_new("root-pixmap-changed",
						  G_OBJECT_CLASS_TYPE(klass),
						  G_SIGNAL_RUN_LAST,
						  0,
						  NULL,
						  NULL,
						  _vte_marshal_VOID__VOID,
						  G_TYPE_NONE, 0);
	g_type_class_add_private(klass, sizeof (struct VteBgPrivate));
}

static void
vte_bg_init(VteBg *bg)
{
	bg->pvt = G_TYPE_INSTANCE_GET_PRIVATE (bg, VTE_TYPE_BG, struct VteBgPrivate);
}

/**
 * vte_bg_get:
 * @screen : A #GdkScreen.
 *
 * Finds the address of the global #VteBg object, creating the object if
 * necessary.
 *
 * Returns: the global #VteBg object
 */
VteBg *
vte_bg_get_for_screen(GdkScreen *screen)
{
	GdkEventMask events;
	GdkWindow   *window;
	VteBg       *bg;

	bg = g_object_get_data(G_OBJECT(screen), "vte-bg");
	if (G_UNLIKELY(bg == NULL)) {
		bg = g_object_new(VTE_TYPE_BG, NULL);
		g_object_set_data_full(G_OBJECT(screen),
				"vte-bg", bg, (GDestroyNotify)g_object_unref);

		/* connect bg to screen */
		bg->screen = screen;
		window = gdk_screen_get_root_window(screen);
		bg->native = vte_bg_native_new(window);
		bg->root_pixmap = vte_bg_root_pixmap(bg);
		events = gdk_window_get_events(window);
		events |= GDK_PROPERTY_CHANGE_MASK;
		gdk_window_set_events(window, events);
		gdk_window_add_filter(window, vte_bg_root_filter, bg);
	}

	return bg;
}

/* Generate lookup tables for desaturating an image toward a given color.  The
 * saturation value is a floating point number between 0 and 1. */
static void
_vte_bg_generate_desat_tables(const GdkColor *color, double saturation,
			      guchar red[256],
			      guchar green[256],
			      guchar blue[256])
{
	int i;
	/* Zero saturation -> exactly match the tinting color. */
	if (saturation == 0) {
		for (i = 0; i < 256; i++) {
			red[i] = color->red >> 8;
			green[i] = color->green >> 8;
			blue[i] = color->blue >> 8;
		}
		return;
	}
	/* 100% saturation -> exactly match the original color. */
	if (saturation == 1) {
		for (i = 0; i < 256; i++) {
			red[i] = green[i] = blue[i] = 1;
		}
		return;
	}
	/* 0-100% saturation -> weighted average */
	for (i = 0; i < 256; i++) {
		red[i] = CLAMP(((1.0 - saturation) * (color->red >> 8)) +
			       (saturation * i),
			       0, 255);
		green[i] = CLAMP(((1.0 - saturation) * (color->green >> 8)) +
				 (saturation * i),
				 0, 255);
		blue[i] = CLAMP(((1.0 - saturation) * (color->blue >> 8)) +
				(saturation * i),
				0, 255);
	}
}

static gboolean
vte_bg_colors_equal(const GdkColor *a, const GdkColor *b)
{
	return  (a->red >> 8) == (b->red >> 8) &&
		(a->green >> 8) == (b->green >> 8) &&
		(a->blue >> 8) == (b->blue >> 8);
}

static void
vte_bg_cache_item_free(struct VteBgCacheItem *item)
{
	/* Clean up whatever is left in the structure. */
	if (item->source_pixbuf != NULL) {
		g_object_remove_weak_pointer(G_OBJECT(item->source_pixbuf),
				(gpointer*)&item->source_pixbuf);
	}
	g_free(item->source_file);
	if (item->pixmap != NULL) {
		g_object_remove_weak_pointer(G_OBJECT(item->pixmap),
				(gpointer*)&item->pixmap);
	}
	if (item->pixbuf != NULL) {
		g_object_remove_weak_pointer(G_OBJECT(item->pixbuf),
				(gpointer*)&item->pixbuf);
	}

	g_slice_free(struct VteBgCacheItem, item);
}

static void
vte_bg_cache_prune_int(VteBg *bg, gboolean root)
{
	GList *i, *next;
	for (i = bg->pvt->cache; i != NULL; i = next) {
		struct VteBgCacheItem *item = i->data;
		next = g_list_next (i);
		/* Prune the item if either
		 * it is a "root pixmap" item and we want to prune them, or
		 * its pixmap and pixbuf fields are both NULL because whichever
		 * object it created has been destroyoed. */
		if ((root && (item->source_type == VTE_BG_SOURCE_ROOT)) ||
		    ((item->pixmap == NULL) && (item->pixbuf == NULL))) {
			vte_bg_cache_item_free (item);
			bg->pvt->cache = g_list_delete_link(bg->pvt->cache, i);
		}
	}
}

static void
vte_bg_cache_prune(VteBg *bg)
{
	vte_bg_cache_prune_int(bg, FALSE);
}

/**
 * _vte_bg_resize_pixbuf:
 * @pixmap: a #GdkPixbuf, or %NULL
 * @min_width: the requested minimum_width
 * @min_height: the requested minimum_height
 *
 * The background pixbuf may be tiled, and if it is tiled, it may be very, very
 * small.  This function creates a pixbuf consisting of the passed-in pixbuf
 * tiled to a usable size.
 *
 * Returns: a new #GdkPixbuf, unrefs @pixbuf.
 */
static GdkPixbuf *
_vte_bg_resize_pixbuf(GdkPixbuf *pixbuf, gint min_width, gint min_height)
{
	GdkPixbuf *tmp;
	gint src_width, src_height;
	gint dst_width, dst_height;
	gint x, y;

	src_width = gdk_pixbuf_get_width(pixbuf);
	src_height = gdk_pixbuf_get_height(pixbuf);
	dst_width = (((min_width - 1) / src_width) + 1) * src_width;
	dst_height = (((min_height - 1) / src_height) + 1) * src_height;
	if ((dst_width == src_width) && (dst_height == src_height)) {
		return pixbuf;
	}

	_vte_debug_print(VTE_DEBUG_MISC|VTE_DEBUG_EVENTS,
		"Resizing (root?) pixbuf from %dx%d to %dx%d\n",
			src_width, src_height, dst_width, dst_height);

	tmp = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(pixbuf),
			     gdk_pixbuf_get_has_alpha(pixbuf),
			     gdk_pixbuf_get_bits_per_sample(pixbuf),
			     dst_width, dst_height);
	for (y = 0; y < dst_height; y += src_height) {
		for (x = 0; x < dst_width; x += src_width) {
			gdk_pixbuf_copy_area(pixbuf,
					     0, 0, src_width, src_height,
					     tmp,
					     x, y);
		}
	}

	g_object_unref(pixbuf);
	return tmp;
}

/* Add an item to the cache, instructing all of the objects therein to clear
   the field which holds a pointer to the object upon its destruction. */
static void
vte_bg_cache_add(VteBg *bg, struct VteBgCacheItem *item)
{
	vte_bg_cache_prune(bg);
	bg->pvt->cache = g_list_prepend(bg->pvt->cache, item);
	if (item->source_pixbuf != NULL) {
		g_object_add_weak_pointer(G_OBJECT(item->source_pixbuf),
					  (gpointer*)&item->source_pixbuf);
	}
	if (item->pixbuf != NULL) {
		g_object_add_weak_pointer(G_OBJECT(item->pixbuf),
					  (gpointer*)&item->pixbuf);
	}
	if (item->pixmap != NULL) {
		g_object_add_weak_pointer(G_OBJECT(item->pixmap),
					  (gpointer*)&item->pixmap);
	}
}

/* Desaturate a pixbuf in the direction of a specified color. */
static void
vte_bg_desaturate_pixbuf(GdkPixbuf *pixbuf,
			 const GdkColor *tint, double saturation)
{
	guchar red[256], green[256], blue[256];
	long stride, width, height, channels, x, y;
	guchar *pixels;

	_vte_bg_generate_desat_tables(tint, saturation, red, green, blue);

	stride = gdk_pixbuf_get_rowstride(pixbuf);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	channels = gdk_pixbuf_get_n_channels(pixbuf);

	for (y = 0; y < height; y++) {
		pixels = gdk_pixbuf_get_pixels(pixbuf) +
			 y * stride;
		for (x = 0; x < width * channels; x++) {
			switch(x % channels) {
			case 0:
				pixels[x] = red[pixels[x]];
				break;
			case 1:
				pixels[x] = green[pixels[x]];
				break;
			case 2:
				pixels[x] = blue[pixels[x]];
				break;
			default:
				break;
			}
		}
	}
}

/* Search for a match in the cache, and if found, return an object with an
   additional ref. */
static gpointer
vte_bg_cache_search(VteBg *bg,
		    enum VteBgSourceType source_type,
		    const GdkPixbuf *source_pixbuf,
		    const char *source_file,
		    const GdkColor *tint,
		    double saturation,
		    GdkVisual *visual,
		    gboolean pixbuf,
		    gboolean pixmap)
{
	GList *i;

	g_assert((pixmap && !pixbuf) || (!pixmap && pixbuf));

	vte_bg_cache_prune(bg);
	for (i = bg->pvt->cache; i != NULL; i = g_list_next(i)) {
		struct VteBgCacheItem *item = i->data;
		if (vte_bg_colors_equal(&item->tint_color, tint) &&
		    (saturation == item->saturation) &&
		    (source_type == item->source_type)) {
			switch (source_type) {
			case VTE_BG_SOURCE_ROOT:
				break;
			case VTE_BG_SOURCE_PIXBUF:
				if (item->source_pixbuf != source_pixbuf) {
					continue;
				}
				break;
			case VTE_BG_SOURCE_FILE:
				if (strcmp(item->source_file, source_file)) {
					continue;
				}
				break;
			default:
				g_assert_not_reached();
				break;
			}
			if (pixbuf && item->pixbuf != NULL) {
				return g_object_ref(item->pixbuf);
			}
			if (pixmap && item->pixmap != NULL &&
					gdk_drawable_get_visual (item->pixmap) == visual) {
				return g_object_ref(item->pixmap);
			}
		}
	}
	return NULL;
}

GdkPixmap *
vte_bg_get_pixmap(VteBg *bg,
		  enum VteBgSourceType source_type,
		  GdkPixbuf *source_pixbuf,
		  const char *source_file,
		  const GdkColor *tint,
		  double saturation,
		  GdkColormap *colormap)
{
	struct VteBgCacheItem *item;
	gpointer cached;
	GdkColormap *rcolormap;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkPixbuf *pixbuf;
	char *file;

	if (source_type == VTE_BG_SOURCE_NONE) {
		return NULL;
	}

	cached = vte_bg_cache_search(bg, source_type,
				     source_pixbuf, source_file,
				     tint, saturation,
				     gdk_colormap_get_visual (colormap),
				     FALSE, TRUE);
	if (cached != NULL) {
		return cached;
	}

	item = g_slice_new(struct VteBgCacheItem);
	item->source_type = source_type;
	item->source_pixbuf = NULL;
	item->source_file = NULL;
	item->tint_color = *tint;
	item->saturation = saturation;
	item->pixmap = NULL;
	item->pixbuf = NULL;
	pixbuf = NULL;
	pixmap = NULL;
	file = NULL;

	switch (source_type) {
	case VTE_BG_SOURCE_ROOT:
		if (GDK_IS_PIXMAP(bg->root_pixmap)) {
			int width, height;
			/* Tell GTK+ that this foreign pixmap shares the
			 * root window's colormap. */
			rcolormap = gdk_drawable_get_colormap(gdk_screen_get_root_window(bg->screen));
			if (gdk_drawable_get_colormap(bg->root_pixmap) == NULL) {
				gdk_drawable_set_colormap(bg->root_pixmap,
							  rcolormap);
			}

			/* Retrieve the pixmap's size. */
			gdk_error_trap_push();
			width = height = -1;
			gdk_drawable_get_size(bg->root_pixmap, &width, &height);
			_vte_bg_display_sync(bg);
			gdk_error_trap_pop();

			/* If the pixmap gave us a valid size, retrieve its
			 * contents. */
			if ((width > 0) && (height > 0)) {
				gdk_error_trap_push();
				pixbuf = gdk_pixbuf_get_from_drawable(NULL,
								      bg->root_pixmap,
								      NULL,
								      0, 0,
								      0, 0,
								      width, height);
				_vte_bg_display_sync(bg);
				gdk_error_trap_pop();
			}
		}
		break;
	case VTE_BG_SOURCE_PIXBUF:
		pixbuf = source_pixbuf;
		if (GDK_IS_PIXBUF(pixbuf)) {
			g_object_ref(pixbuf);
		}
		break;
	case VTE_BG_SOURCE_FILE:
		if ((source_file != NULL) && (strlen(source_file) > 0)) {
			file = g_strdup(source_file);
			pixbuf = gdk_pixbuf_new_from_file(source_file, NULL);
		}
		break;
	default:
		g_assert_not_reached();
		break;
	}

	item->source_pixbuf = source_pixbuf;
	if (G_IS_OBJECT(item->source_pixbuf)) {
		g_object_ref(item->source_pixbuf);
	}
	item->source_file = file;

	if (GDK_IS_PIXBUF(pixbuf)) {
		if (saturation != 1.0) {
			vte_bg_desaturate_pixbuf(pixbuf, tint, saturation);
		}
	}

	pixmap = NULL;
	mask = NULL;
	if (GDK_IS_PIXBUF(pixbuf)) {
		/* If the image is smaller than 256x256 then tile it into a
		 * pixbuf that is at least this large.  This is done because
		 * tiling a 1x1 pixmap onto the screen using thousands of calls
		 * to XCopyArea is very slow. */
		pixbuf = _vte_bg_resize_pixbuf(pixbuf, 256, 256);
		gdk_pixbuf_render_pixmap_and_mask_for_colormap(pixbuf,
							       colormap,
							       &pixmap, &mask,
							       0);
		if (mask != NULL) {
			g_object_unref(mask);
		}
		g_object_unref(pixbuf);
	}

	item->pixmap = pixmap;

	vte_bg_cache_add(bg, item);

	return item->pixmap;
}

GdkPixbuf *
vte_bg_get_pixbuf(VteBg *bg,
		  enum VteBgSourceType source_type,
		  GdkPixbuf *source_pixbuf,
		  const char *source_file,
		  const GdkColor *tint,
		  double saturation)
{
	struct VteBgCacheItem *item;
	gpointer cached;
	GdkPixbuf *pixbuf;
	GdkColormap *rcolormap;
	char *file;

	if (source_type == VTE_BG_SOURCE_NONE) {
		return NULL;
	}

	cached = vte_bg_cache_search(bg, source_type,
				     source_pixbuf, source_file,
				     tint, saturation, NULL, TRUE, FALSE);
	if (cached != NULL) {
		return cached;
	}

	item = g_slice_new(struct VteBgCacheItem);
	item->source_type = source_type;
	item->source_pixbuf = NULL;
	item->source_file = NULL;
	item->tint_color = *tint;
	item->saturation = saturation;
	item->pixmap = NULL;
	item->pixbuf = NULL;
	pixbuf = NULL;
	file = NULL;

	switch (source_type) {
	case VTE_BG_SOURCE_ROOT:
		if (GDK_IS_PIXMAP(bg->root_pixmap)) {
			gint width, height;

			/* If the pixmap doesn't have a colormap, tell GTK+ that
			 * it shares the root window's colormap. */
			rcolormap = gdk_drawable_get_colormap(gdk_screen_get_root_window(bg->screen));
			if (gdk_drawable_get_colormap(bg->root_pixmap) == NULL) {
				gdk_drawable_set_colormap(bg->root_pixmap, rcolormap);
			}

			/* Read the pixmap's size. */
			gdk_error_trap_push();
			width = height = -1;
			gdk_drawable_get_size(bg->root_pixmap, &width, &height);
			_vte_bg_display_sync(bg);
			gdk_error_trap_pop();

			/* If we got a valid size, read the pixmap's
			 * contents. */
			if ((width > 0) && (height > 0)) {
				gdk_error_trap_push();
				pixbuf = gdk_pixbuf_get_from_drawable(NULL,
								      bg->root_pixmap,
								      NULL,
								      0, 0,
								      0, 0,
								      width, height);
				_vte_bg_display_sync(bg);
				gdk_error_trap_pop();
			}
		}
		break;
	case VTE_BG_SOURCE_PIXBUF:
		pixbuf = source_pixbuf;
		if (G_IS_OBJECT(pixbuf)) {
			g_object_ref(pixbuf);
		}
		break;
	case VTE_BG_SOURCE_FILE:
		if ((source_file != NULL) && (strlen(source_file) > 0)) {
			file = g_strdup(source_file);
			pixbuf = gdk_pixbuf_new_from_file(source_file, NULL);
		}
		break;
	default:
		g_assert_not_reached();
		break;
	}

	item->source_pixbuf = pixbuf;
	item->source_file = file;

	if (GDK_IS_PIXBUF(item->source_pixbuf)) {
		if (saturation == 1.0) {
			g_object_ref(item->source_pixbuf);
			item->pixbuf = item->source_pixbuf;
		} else {
			item->pixbuf = gdk_pixbuf_copy(item->source_pixbuf);
			vte_bg_desaturate_pixbuf(item->pixbuf,
						 tint, saturation);
		}
	}

	vte_bg_cache_add(bg, item);

	return item->pixbuf;
}
