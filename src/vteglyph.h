/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
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

#ifndef vte_vteglyph_h_included
#define vte_vteglyph_h_included


#include <gtk/gtk.h>
#include "vtefc.h"
#include "vtergb.h"

#include <fontconfig/fontconfig.h>

#include <ft2build.h>
#include FT_FREETYPE_H

enum vte_glyph_flags {
	vte_glyph_bold		= 1 << 0,
	vte_glyph_dim		= 1 << 1,
	vte_glyph_underline	= 1 << 2,
	vte_glyph_underline2	= 1 << 3,
	vte_glyph_strikethrough	= 1 << 4,
	vte_glyph_boxed		= 1 << 5
};

#define vte_glyph_double_underline \
	(vte_glyph_underline | vte_glyph_underline2)
#define vte_glyph_all \
	(vte_glyph_bold | vte_glyph_dim | \
	 vte_glyph_underline | vte_glyph_underline2 | \
	 vte_glyph_strikethrough | vte_glyph_boxed)

struct _vte_glyph {
	glong width;
	glong height;
	glong skip;
	guchar bytes_per_pixel;
	guchar bytes[1];
};

struct _vte_glyph_cache {
	GArray *patterns;
	GList *faces;
	GTree *cache;
	gint ft_load_flags;
	gint ft_render_flags;
	glong width, height, ascent;
	FT_Library ft_library;
};

struct _vte_glyph_cache *_vte_glyph_cache_new(void);
void _vte_glyph_cache_free(struct _vte_glyph_cache *cache);
const FcPattern *_vte_glyph_cache_get_pattern(struct _vte_glyph_cache *cache);
void _vte_glyph_cache_set_font_description(GtkWidget *widget, FcConfig *config,
					   struct _vte_glyph_cache *cache,
					   const PangoFontDescription *fontdesc,
					   VteTerminalAntiAlias anti_alias,
					   _vte_fc_defaults_cb defaults_cb,
					   gpointer defaults_data);
gboolean _vte_glyph_cache_has_char(struct _vte_glyph_cache *cache, gunichar c);
const struct _vte_glyph *_vte_glyph_get(struct _vte_glyph_cache *cache,
					gunichar c);
struct _vte_glyph *_vte_glyph_get_uncached(struct _vte_glyph_cache *cache,
					   gunichar c);
void _vte_glyph_free(struct _vte_glyph *glyph);
void _vte_glyph_draw(struct _vte_glyph_cache *cache,
		     gunichar c, GdkColor *color,
		     gint x, gint y, gint columns,
		     enum vte_glyph_flags flags,
		     struct _vte_rgb_buffer *buffer);
void _vte_glyph_draw_string(struct _vte_glyph_cache *cache,
			    const char *s, GdkColor *color,
			    gint x, gint y,
			    enum vte_glyph_flags flags,
			    struct _vte_rgb_buffer *buffer);

#endif
