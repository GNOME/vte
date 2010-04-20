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

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include "debug.h"
#include "marshal.h"
#include "vtebg.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <cairo-xlib.h>
#endif

G_DEFINE_TYPE(VteBg, vte_bg, G_TYPE_OBJECT)

struct _VteBgPrivate {
	GList *cache;
	GdkScreen *screen;
#ifdef GDK_WINDOWING_X11
	cairo_surface_t *root_surface;
        struct {
                GdkDisplay *display;
                GdkWindow *window;
                XID native_window;
                GdkAtom atom;
                Atom native_atom;
        } native;
#endif
};

typedef struct {
	VteBgSourceType source_type;
	GdkPixbuf *source_pixbuf;
	char *source_file;

	PangoColor tint_color;
	double saturation;
	cairo_surface_t *surface;
} VteBgCacheItem;

static void vte_bg_cache_item_free(VteBgCacheItem *item);
static void vte_bg_cache_prune_int(VteBg *bg, gboolean root);
static const cairo_user_data_key_t item_surface_key;

#ifdef GDK_WINDOWING_X11

static void
_vte_bg_display_sync(VteBg *bg)
{
        VteBgPrivate *pvt = bg->pvt;

	gdk_display_sync(pvt->native.display);
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

static cairo_surface_t *
vte_bg_root_surface(VteBg *bg)
{
        VteBgPrivate *pvt = bg->pvt;
	GdkPixmap *pixmap;
	GdkAtom prop_type;
	int prop_size;
	Window root;
	XID *pixmaps;
	int x, y;
	unsigned int width, height, border_width, depth;
	cairo_surface_t *surface = NULL;
	Display *display;
	Screen *screen;

	pixmap = NULL;
	pixmaps = NULL;
	gdk_error_trap_push();
	if (!_vte_property_get_pixmaps(pvt->native.window, pvt->native.atom,
                                       &prop_type, &prop_size,
                                       &pixmaps))
		goto out;

	if ((prop_type != GDK_TARGET_PIXMAP) ||
	    (prop_size < (int)sizeof(XID) ||
	     (pixmaps == NULL)))
		goto out_pixmaps;
		
	if (!XGetGeometry (GDK_DISPLAY_XDISPLAY (pvt->native.display),
			   pixmaps[0], &root,
			   &x, &y, &width, &height, &border_width, &depth))
		goto out_pixmaps;

	display = gdk_x11_display_get_xdisplay (pvt->native.display);
	screen = gdk_x11_screen_get_xscreen (pvt->screen);
	surface = cairo_xlib_surface_create (display,
					     pixmaps[0],
					     DefaultVisualOfScreen(screen),
					     width, height);

        _vte_debug_print(VTE_DEBUG_BG|VTE_DEBUG_EVENTS,
                         "VteBg new background image %dx%d\n", width, height);

 out_pixmaps:
	g_free(pixmaps);
 out:
	_vte_bg_display_sync(bg);
	gdk_error_trap_pop();

	return surface;
}

static void
vte_bg_set_root_surface(VteBg *bg, cairo_surface_t *surface)
{
        VteBgPrivate *pvt = bg->pvt;

	if (pvt->root_surface != NULL) {
		cairo_surface_destroy (pvt->root_surface);
	}
	pvt->root_surface = surface;
	vte_bg_cache_prune_int (bg, TRUE);
	g_signal_emit_by_name(bg, "root-pixmap-changed");
}

static GdkFilterReturn
vte_bg_root_filter(GdkXEvent *native, GdkEvent *event, gpointer data)
{
	XEvent *xevent = (XEvent*) native;
	VteBg *bg;
        VteBgPrivate *pvt;
	cairo_surface_t *surface;

	switch (xevent->type) {
	case PropertyNotify:
		bg = VTE_BG(data);
                pvt = bg->pvt;
		if ((xevent->xproperty.window == pvt->native.native_window) &&
		    (xevent->xproperty.atom == pvt->native.native_atom)) {
			surface = vte_bg_root_surface(bg);
			vte_bg_set_root_surface(bg, surface);
		}
		break;
	default:
		break;
	}
	return GDK_FILTER_CONTINUE;
}

#endif /* GDK_WINDOWING_X11 */

static void
vte_bg_finalize (GObject *obj)
{
	VteBg *bg = VTE_BG (obj);
        VteBgPrivate *pvt = bg->pvt;

        g_list_foreach (pvt->cache, (GFunc)vte_bg_cache_item_free, NULL);
        g_list_free (pvt->cache);

	G_OBJECT_CLASS(vte_bg_parent_class)->finalize (obj);
}

static void
vte_bg_class_init(VteBgClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = vte_bg_finalize;

	g_signal_new("root-pixmap-changed",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL,
                     NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
	g_type_class_add_private(klass, sizeof (VteBgPrivate));
}

static void
vte_bg_init(VteBg *bg)
{
	bg->pvt = G_TYPE_INSTANCE_GET_PRIVATE (bg, VTE_TYPE_BG, VteBgPrivate);
}

/**
 * vte_bg_get:
 * @screen: a #GdkScreen
 *
 * Returns the global #VteBg object for @screen, creating it if necessary.
 *
 * Returns: (transfer none): a #VteBg
 */
VteBg *
vte_bg_get_for_screen(GdkScreen *screen)
{
	VteBg       *bg;

	bg = g_object_get_data(G_OBJECT(screen), "vte-bg");
	if (G_UNLIKELY(bg == NULL)) {
                VteBgPrivate *pvt;

		bg = g_object_new(VTE_TYPE_BG, NULL);
		g_object_set_data_full(G_OBJECT(screen),
				"vte-bg", bg, (GDestroyNotify)g_object_unref);

		/* connect bg to screen */
                pvt = bg->pvt;
		pvt->screen = screen;
#ifdef GDK_WINDOWING_X11
            {
                GdkEventMask events;
                GdkWindow   *window;

		window = gdk_screen_get_root_window(screen);
                pvt->native.window = window;
                pvt->native.native_window = gdk_x11_drawable_get_xid(window);
                pvt->native.display = gdk_drawable_get_display(GDK_DRAWABLE(window));
                pvt->native.native_atom = gdk_x11_get_xatom_by_name_for_display(pvt->native.display, "_XROOTPMAP_ID");
                pvt->native.atom = gdk_x11_xatom_to_atom_for_display(pvt->native.display, pvt->native.native_atom);
		pvt->root_surface = vte_bg_root_surface(bg);
		events = gdk_window_get_events(window);
		events |= GDK_PROPERTY_CHANGE_MASK;
		gdk_window_set_events(window, events);
		gdk_window_add_filter(window, vte_bg_root_filter, bg);
            }
#endif /* GDK_WINDOWING_X11 */
	}

	return bg;
}

static gboolean
vte_bg_colors_equal(const PangoColor *a, const PangoColor *b)
{
	return  (a->red >> 8) == (b->red >> 8) &&
		(a->green >> 8) == (b->green >> 8) &&
		(a->blue >> 8) == (b->blue >> 8);
}

static void
vte_bg_cache_item_free(VteBgCacheItem *item)
{
        _vte_debug_print(VTE_DEBUG_BG,
                         "VteBgCacheItem %p freed\n", item);

	/* Clean up whatever is left in the structure. */
	if (item->source_pixbuf != NULL) {
		g_object_remove_weak_pointer(G_OBJECT(item->source_pixbuf),
				(gpointer*)(void*)&item->source_pixbuf);
	}
	g_free(item->source_file);

	if (item->surface != NULL)
		cairo_surface_set_user_data (item->surface,
					     &item_surface_key, NULL, NULL);

	g_slice_free(VteBgCacheItem, item);
}

static void
vte_bg_cache_prune_int(VteBg *bg, gboolean root)
{
	GList *i, *next;
	for (i = bg->pvt->cache; i != NULL; i = next) {
		VteBgCacheItem *item = i->data;
		next = g_list_next (i);
		/* Prune the item if either it is a "root pixmap" item and
		 * we want to prune them, or its surface is NULL because
		 * whichever object it created has been destroyed. */
		if ((root && (item->source_type == VTE_BG_SOURCE_ROOT)) ||
		    item->surface == NULL) {
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

static void item_surface_destroy_func(void *data)
{
	VteBgCacheItem *item = data;

        _vte_debug_print(VTE_DEBUG_BG,
                         "VteBgCacheItem %p surface destroyed\n", item);

	item->surface = NULL;
}

/*
 * vte_bg_cache_add:
 * @bg: a #VteBg
 * @item: a #VteBgCacheItem
 *
 * Adds @item to @bg's cache, instructing all of the objects therein to
 * clear the field which holds a pointer to the object upon its destruction.
 */
static void
vte_bg_cache_add(VteBg *bg, VteBgCacheItem *item)
{
	vte_bg_cache_prune(bg);
	bg->pvt->cache = g_list_prepend(bg->pvt->cache, item);
	if (item->source_pixbuf != NULL) {
		g_object_add_weak_pointer(G_OBJECT(item->source_pixbuf),
					  (gpointer*)(void*)&item->source_pixbuf);
	}

        if (item->surface != NULL)
                cairo_surface_set_user_data (item->surface, &item_surface_key, item,
                                            item_surface_destroy_func);
}

/*
 * vte_bg_cache_search:
 * @bg: a #VteBg
 * @source_type: a #VteBgSourceType
 * @source_pixbuf: a #GdkPixbuf, or %NULL
 * @source_file: path of an image file, or %NULL
 * @tint: a #PangoColor to use as tint color
 * @saturation: the saturation as a value between 0.0 and 1.0
 *
 * Returns: a reference to a #cairo_surface_t, or %NULL on if
 *   there is no matching item in the cache
 */
static cairo_surface_t *
vte_bg_cache_search(VteBg *bg,
		    VteBgSourceType source_type,
		    const GdkPixbuf *source_pixbuf,
		    const char *source_file,
		    const PangoColor *tint,
		    double saturation)
{
	GList *i;

	vte_bg_cache_prune(bg);
	for (i = bg->pvt->cache; i != NULL; i = g_list_next(i)) {
		VteBgCacheItem *item = i->data;
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

			return cairo_surface_reference(item->surface);
		}
	}
	return NULL;
}

/*< private >
 * vte_bg_get_surface:
 * @bg: a #VteBg
 * @source_type: a #VteBgSourceType
 * @source_pixbuf: (allow-none): a #GdkPixbuf, or %NULL
 * @source_file: (allow-none): path of an image file, or %NULL
 * @tint: a #PangoColor to use as tint color
 * @saturation: the saturation as a value between 0.0 and 1.0
 * @other: a #cairo_surface_t
 *
 * Returns: a reference to a #cairo_surface_t, or %NULL on failure
 */
cairo_surface_t *
vte_bg_get_surface(VteBg *bg,
		   VteBgSourceType source_type,
		   GdkPixbuf *source_pixbuf,
		   const char *source_file,
		   const PangoColor *tint,
		   double saturation,
		   cairo_surface_t *other)
{
        VteBgPrivate *pvt;
	VteBgCacheItem *item;
	GdkPixbuf *pixbuf;
	cairo_surface_t *cached;
	cairo_t *cr;
	int width, height;

        g_return_val_if_fail(VTE_IS_BG(bg), NULL);
        pvt = bg->pvt;

	if (source_type == VTE_BG_SOURCE_NONE) {
		return NULL;
	}
#ifndef GDK_WINDOWING_X11
        if (source_type == VTE_BG_SOURCE_ROOT) {
                return NULL;
        }
#endif

	cached = vte_bg_cache_search(bg, source_type,
				     source_pixbuf, source_file,
				     tint, saturation);
	if (cached != NULL) {
		return cached;
	}

        /* FIXME: The above only returned a hit when the source *and*
         * tint and saturation matched. This means that for VTE_BG_SOURCE_FILE,
         * we will create below *another* #GdkPixbuf for the same source file,
         * wasting memory. We should instead look up the source pixbuf regardless
         * of tint and saturation, and just create a new #VteBgCacheItem
         * with a new surface for it.
         */

	item = g_slice_new(VteBgCacheItem);
	item->source_type = source_type;
	item->source_pixbuf = NULL;
	item->source_file = NULL;
	item->tint_color = *tint;
	item->saturation = saturation;
        item->surface = NULL;
	pixbuf = NULL;

	switch (source_type) {
	case VTE_BG_SOURCE_ROOT:
		break;
	case VTE_BG_SOURCE_PIXBUF:
		item->source_pixbuf = g_object_ref (source_pixbuf);
		pixbuf = g_object_ref (source_pixbuf);
		break;
	case VTE_BG_SOURCE_FILE:
		if (source_file != NULL && source_file[0] != '\0') {
			item->source_file = g_strdup(source_file);
			pixbuf = gdk_pixbuf_new_from_file(source_file, NULL);
		}
		break;
	default:
		g_assert_not_reached();
		break;
	}

	if (pixbuf) {
		width = gdk_pixbuf_get_width(pixbuf);
		height = gdk_pixbuf_get_height(pixbuf);
	}
#ifdef GDK_WINDOWING_X11
        else if (source_type == VTE_BG_SOURCE_ROOT &&
                 pvt->root_surface != NULL) {
		width = cairo_xlib_surface_get_width(pvt->root_surface);
		height = cairo_xlib_surface_get_height(pvt->root_surface);
	}
#endif
        else
                goto out;

	item->surface =
		cairo_surface_create_similar(other, CAIRO_CONTENT_COLOR_ALPHA,
					     width, height);

	cr = cairo_create (item->surface);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	if (pixbuf)
		gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
#ifdef GDK_WINDOWING_X11
	else if (source_type == VTE_BG_SOURCE_ROOT)
		cairo_set_source_surface (cr, pvt->root_surface, 0, 0);
#endif
	cairo_paint (cr);

	if (saturation < 1.0) {
		cairo_set_source_rgba (cr, 
				       tint->red / 65535.,
				       tint->green / 65535.,
				       tint->blue / 65535.,
				       1 - saturation);
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_paint (cr);
	}
	cairo_destroy (cr);

    out:
	vte_bg_cache_add(bg, item);

	if (pixbuf)
		g_object_unref (pixbuf);

	return item->surface;
}
