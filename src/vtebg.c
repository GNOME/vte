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
#include <string.h>
#include <gtk/gtk.h>
#include "debug.h"
#include "marshal.h"
#include "vtebg.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext(PACKAGE, String)
#else
#define _(String) String
#define bindtextdomain(package,dir)
#endif

struct VteBgPrivate {
	GList *cache;
};

static VteBg *singleton_bg = NULL;
static void vte_bg_set_root_pixmap(VteBg *bg, GdkPixmap *pixmap);
static void vte_bg_init(VteBg *bg, gpointer *klass);

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
	Atom atom;
	pvt = g_malloc0(sizeof(struct VteBgNative));
	pvt->window = window;
	pvt->native_window = gdk_x11_drawable_get_xid(window);
	pvt->atom = gdk_atom_intern("_XROOTPMAP_ID", FALSE);
#if GTK_CHECK_VERSION(2,2,0)
	atom = gdk_x11_atom_to_xatom_for_display(gdk_drawable_get_display(window),
						 pvt->atom);
#else
	atom = gdk_x11_atom_to_xatom(pvt->atom);
#endif
	pvt->native_atom = atom;
	return pvt;
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
	if (gdk_property_get(bg->native->window,
			     bg->native->atom,
			     GDK_TARGET_PIXMAP,
			     0,
			     INT_MAX,
			     FALSE,
			     &prop_type,
			     NULL,
			     &prop_size,
			     (guchar**) &pixmaps)) {
		if ((prop_type == GDK_TARGET_PIXMAP) &&
		    (prop_size >= sizeof(XID) &&
		    (pixmaps != NULL))) {
#if GTK_CHECK_VERSION(2,2,0)
			pixmap = gdk_pixmap_foreign_new_for_display(gdk_drawable_get_display(bg->native->window), pixmaps[0]);
#else
			pixmap = gdk_pixmap_foreign_new(pixmaps[0]);
#endif
			g_print("Got new root pixmap.\n");
		}
		if (pixmaps != NULL) {
			g_free(pixmaps);
		}
	}
	gdk_error_trap_pop();
	return pixmap;
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

#endif


static void
vte_bg_class_init(VteBgClass *klass, gpointer data)
{
	bindtextdomain(PACKAGE, LOCALEDIR);
	klass->root_pixmap_changed = g_signal_new("root-pixmap-changed",
						  G_OBJECT_CLASS_TYPE(klass),
						  G_SIGNAL_RUN_LAST,
						  0,
						  NULL,
						  NULL,
						  _vte_marshal_VOID__VOID,
						  G_TYPE_NONE, 0);
}

GType
vte_bg_get_type(void)
{
	static GType bg_type = 0;
	static GTypeInfo bg_type_info = {
		sizeof(VteBgClass),

		(GBaseInitFunc)NULL,
		(GBaseFinalizeFunc)NULL,

		(GClassInitFunc)vte_bg_class_init,
		(GClassFinalizeFunc)NULL,
		NULL,

		sizeof(VteBg),
		0,
		(GInstanceInitFunc) vte_bg_init,

		(const GTypeValueTable *) NULL,
	};
	if (bg_type == 0) {
		bg_type = g_type_register_static(G_TYPE_OBJECT,
						     "VteBg",
						     &bg_type_info,
						     0);
	}
	return bg_type;
}

/**
 * vte_bg_get:
 *
 * Finds the address of the global #VteBg object, creating the object if
 * necessary.
 *
 * Returns: the global #VteBg object
 */
VteBg *
vte_bg_get(void)
{
	if (!VTE_IS_BG(singleton_bg)) {
		singleton_bg = g_object_new(VTE_TYPE_BG, NULL);
	}
	return singleton_bg;
}

struct VteBgCacheItem {
	enum VteBgSourceType source_type;
	GdkPixbuf *source_pixbuf;
	char *source_file;

	GdkColor tint_color;
	double saturation;

	GdkPixmap *pixmap;
	GdkPixbuf *pixbuf;
};

static void
vte_bg_init(VteBg *bg, gpointer *klass)
{
	GdkWindow *window;
	GdkEventMask events;
	window = gdk_get_default_root_window();
	bg->native = vte_bg_native_new(window);
	bg->root_pixmap = vte_bg_root_pixmap(bg);
	bg->pvt = g_malloc0(sizeof(struct VteBgPrivate));
	bg->pvt->cache = NULL;
	events = gdk_window_get_events(window);
	events |= GDK_PROPERTY_CHANGE_MASK;
	gdk_window_set_events(window, events);
	gdk_window_add_filter(window, vte_bg_root_filter, bg);
}

/* Generate lookup tables for desaturating an image toward a given color.  The
 * saturation value is a floating point number between 0 and 1. */
static void
vte_bg_generate_desat_tables(const GdkColor *color, double saturation,
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
vte_bg_cache_prune_int(VteBg *bg, gboolean root)
{
	GList *i;
	struct VteBgCacheItem *item;
	GList *removals = NULL;
	i = 0;
	for (i = bg->pvt->cache; i != NULL; i = g_list_next(i)) {
		item = i->data;
		/* Prune the item if either
		 * it is a "root pixmap" item and we want to prune them, or
		 * its pixmap and pixbuf fields are both NULL because whichever
		 * object it created has been destroyoed. */
		if ((root && (item->source_type == VTE_BG_SOURCE_ROOT)) ||
		    ((item->pixmap == NULL) && (item->pixbuf == NULL))) {
			/* Clean up whatever is left in the structure. */
			if (G_IS_OBJECT(item->source_pixbuf)) {
				g_object_remove_weak_pointer(G_OBJECT(item->source_pixbuf),
							     (gpointer*)&item->source_pixbuf);
			}
			item->source_pixbuf = NULL;
			if (item->source_file) {
				g_free(item->source_file);
			}
			item->source_file = NULL;
			if (G_IS_OBJECT(item->pixmap)) {
				g_object_remove_weak_pointer(G_OBJECT(item->pixmap),
							     (gpointer*)&item->pixmap);
			}
			item->pixmap = NULL;
			if (G_IS_OBJECT(item->pixbuf)) {
				g_object_remove_weak_pointer(G_OBJECT(item->pixbuf),
							     (gpointer*)&item->pixbuf);
			}
			item->pixbuf = NULL;
			removals = g_list_prepend(removals, i->data);
		}
	}
	if (removals != NULL) {
		for (i = removals; i != NULL; i = g_list_next(i)) {
			bg->pvt->cache = g_list_remove(bg->pvt->cache, i->data);
		}
		g_list_free(removals);
	}
}

static void
vte_bg_cache_prune(VteBg *bg)
{
	vte_bg_cache_prune_int(bg, FALSE);
}

static void
vte_bg_set_root_pixmap(VteBg *bg, GdkPixmap *pixmap)
{
	if (GDK_IS_PIXMAP(bg->root_pixmap)) {
		g_object_unref(bg->root_pixmap);
	}
	bg->root_pixmap = pixmap;
	vte_bg_cache_prune_int(bg, TRUE);
	g_print("Root pixmap changed.\n");
	g_signal_emit_by_name(bg, "root-pixmap-changed");
}

/* Add an item to the cache, instructing all of the objects therein to clear
   the field which holds a pointer to the object upon its destruction. */
static void
vte_bg_cache_add(VteBg *bg, struct VteBgCacheItem *item)
{
	vte_bg_cache_prune(bg);
	bg->pvt->cache = g_list_prepend(bg->pvt->cache, item);
	if (G_IS_OBJECT(item->source_pixbuf)) {
		g_object_add_weak_pointer(G_OBJECT(item->source_pixbuf),
					  (gpointer*)&item->source_pixbuf);
	}
	if (G_IS_OBJECT(item->pixbuf)) {
		g_object_add_weak_pointer(G_OBJECT(item->pixbuf),
					  (gpointer*)&item->pixbuf);
	}
	if (G_IS_OBJECT(item->pixmap)) {
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

	vte_bg_generate_desat_tables(tint, saturation, red, green, blue);

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
static GObject *
vte_bg_cache_search(VteBg *bg,
		    enum VteBgSourceType source_type,
		    const GdkPixbuf *source_pixbuf,
		    const char *source_file,
		    const GdkColor *tint,
		    double saturation,
		    gboolean pixbuf,
		    gboolean pixmap)
{
	struct VteBgCacheItem *item;
	GList *i;

	g_assert((pixmap && !pixbuf) || (!pixmap && pixbuf));
	vte_bg_cache_prune(bg);

	for (i = bg->pvt->cache; i != NULL; i = g_list_next(i)) {
		item = i->data;
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
			if (pixbuf && GDK_IS_PIXBUF(item->pixbuf)) {
				g_object_ref(G_OBJECT(item->pixbuf));
				return G_OBJECT(item->pixbuf);
			}
			if (pixmap && GDK_IS_PIXMAP(item->pixmap)) {
				g_object_ref(G_OBJECT(item->pixmap));
				return G_OBJECT(item->pixmap);
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
	GObject *cached;
	GdkColormap *rcolormap;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkPixbuf *pixbuf;
	char *file;
	int width, height;

	if (bg == NULL) {
		bg = vte_bg_get();
	}

	if (source_type == VTE_BG_SOURCE_NONE) {
		return NULL;
	}

	cached = vte_bg_cache_search(bg, source_type,
				     source_pixbuf, source_file,
				     tint, saturation, FALSE, TRUE);
	if (G_IS_OBJECT(cached) && GDK_IS_PIXMAP(cached)) {
		return GDK_PIXMAP(cached);
	}

	item = g_malloc0(sizeof(struct VteBgCacheItem));
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
			gdk_drawable_get_size(bg->root_pixmap, &width, &height);
			rcolormap = gdk_drawable_get_colormap(gdk_get_default_root_window());
			pixbuf = gdk_pixbuf_get_from_drawable(NULL,
							      bg->root_pixmap,
							      rcolormap,
							      0, 0,
							      0, 0,
							      width, height);
		}
		break;
	case VTE_BG_SOURCE_PIXBUF:
		pixbuf = source_pixbuf;
		if (GDK_IS_PIXBUF(pixbuf)) {
			g_object_ref(G_OBJECT(pixbuf));
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
		g_object_ref(G_OBJECT(item->source_pixbuf));
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
		gdk_pixbuf_render_pixmap_and_mask_for_colormap(pixbuf,
							       colormap,
							       &pixmap, &mask,
							       0);
		if (G_IS_OBJECT(mask)) {
			g_object_unref(G_OBJECT(mask));
		}
		g_object_unref(G_OBJECT(pixbuf));
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
	GObject *cached;
	GdkPixbuf *pixbuf;
	char *file;

	if (bg == NULL) {
		bg = vte_bg_get();
	}

	if (source_type == VTE_BG_SOURCE_NONE) {
		return NULL;
	}

	cached = vte_bg_cache_search(bg, source_type,
				     source_pixbuf, source_file,
				     tint, saturation, TRUE, FALSE);
	if (G_IS_OBJECT(cached) && GDK_IS_PIXBUF(cached)) {
		return GDK_PIXBUF(cached);
	}

	item = g_malloc0(sizeof(struct VteBgCacheItem));
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
			GdkColormap *colormap;
			gint width, height;
			gdk_drawable_get_size(bg->root_pixmap, &width, &height);
			colormap = gdk_drawable_get_colormap(bg->root_pixmap);
			pixbuf = gdk_pixbuf_get_from_drawable(NULL,
							      bg->root_pixmap,
							      colormap,
							      0, 0,
							      0, 0,
							      width, height);
		}
		break;
	case VTE_BG_SOURCE_PIXBUF:
		pixbuf = source_pixbuf;
		if (G_IS_OBJECT(pixbuf)) {
			g_object_ref(G_OBJECT(pixbuf));
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
			g_object_ref(G_OBJECT(item->source_pixbuf));
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
