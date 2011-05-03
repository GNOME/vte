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

G_DEFINE_TYPE(VteBg, vte_bg, G_TYPE_OBJECT)

struct _VteBgPrivate {
	GList *cache;
	GdkScreen *screen;
};

typedef struct {
	VteBgSourceType source_type;
	GdkPixbuf *source_pixbuf;
	char *source_file;

	GdkRGBA tint_color;
	double saturation;
	cairo_surface_t *surface;
} VteBgCacheItem;

static void vte_bg_cache_item_free(VteBgCacheItem *item);
static const cairo_user_data_key_t item_surface_key;

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
	}

	return bg;
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
vte_bg_cache_prune(VteBg *bg)
{
	GList *i, *next;
	for (i = bg->pvt->cache; i != NULL; i = next) {
		VteBgCacheItem *item = i->data;
		next = g_list_next (i);
		/* Prune the item if its surface is NULL because
		 * whichever object it created has been destroyed. */
		if (item->surface == NULL) {
			vte_bg_cache_item_free (item);
			bg->pvt->cache = g_list_delete_link(bg->pvt->cache, i);
		}
	}
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
 * @tint: a #GdkRGBA to use as tint color
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
		    const GdkRGBA *tint,
		    double saturation)
{
	GList *i;

	vte_bg_cache_prune(bg);
	for (i = bg->pvt->cache; i != NULL; i = g_list_next(i)) {
		VteBgCacheItem *item = i->data;
		if (gdk_rgba_equal (&item->tint_color, tint) &&
		    (saturation == item->saturation) &&
		    (source_type == item->source_type)) {
			switch (source_type) {
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
 * @tint: a #GdkRGBA to use as tint color
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
		   const GdkRGBA *tint,
		   double saturation,
		   cairo_surface_t *other)
{
        VteBgPrivate *pvt;
	VteBgCacheItem *item;
	GdkPixbuf *pixbuf;
	cairo_surface_t *cached;
	cairo_t *cr;
	int width, height;
        double alpha;

        g_return_val_if_fail(VTE_IS_BG(bg), NULL);
        pvt = bg->pvt;

	if (source_type == VTE_BG_SOURCE_NONE) {
		return NULL;
	}

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
        else
                goto out;

	item->surface =
		cairo_surface_create_similar(other, CAIRO_CONTENT_COLOR_ALPHA,
					     width, height);

	cr = cairo_create (item->surface);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	if (pixbuf)
		gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
	cairo_paint (cr);

        alpha = (1. - saturation) * tint->alpha;
	if (alpha > 0.) {
		cairo_set_source_rgba (cr, 
				       tint->red,
				       tint->green,
				       tint->blue,
				       alpha);
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
