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


#include "../config.h"

#include <sys/param.h>
#include <math.h>
#include <gdk/gdk.h>
#include <glib.h>
#include "iso2022.h"
#include "vtedraw.h"
#include "vtefc.h"
#include "vteglyph.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext(PACKAGE, String)
#else
#define _(String) String
#define bindtextdomain(package,dir)
#endif

#define FONT_INDEX_FUDGE 10
#define CHAR_WIDTH_FUDGE 10
#define INVALID_GLYPH    -1

static FT_Face _vte_glyph_cache_face_for_char(struct _vte_glyph_cache *cache,
					      gunichar c);

static int
_vte_direct_compare(gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

struct _vte_glyph_cache *
_vte_glyph_cache_new(void)
{
	struct _vte_glyph_cache *ret;
	int error;

	ret = g_slice_new(struct _vte_glyph_cache);

	ret->patterns = g_array_new(TRUE, TRUE, sizeof(FcPattern*));
	ret->faces = NULL;
	ret->cache = g_tree_new(_vte_direct_compare);
	ret->ft_load_flags = 0;
	ret->ft_render_flags = 0;
	ret->width = 0;
	ret->height = 0;
	ret->ascent = 0;

	error = FT_Init_FreeType(&ret->ft_library);
	g_assert(error == 0);

	return ret;
}

void
_vte_glyph_free(struct _vte_glyph *glyph)
{
	g_free(glyph);
}

static gboolean
free_tree_value(gpointer key, gpointer value, gpointer data)
{
	if (GPOINTER_TO_INT(value) != INVALID_GLYPH) {
		_vte_glyph_free(value);
	}
	return FALSE;
}

void
_vte_glyph_cache_free(struct _vte_glyph_cache *cache)
{
	GList *iter;
	int i;

	g_return_if_fail(cache != NULL);

	/* Destroy the patterns. */
	if (cache->patterns != NULL) {
		for (i = 0; i < cache->patterns->len; i++) {
			FcPatternDestroy(g_array_index(cache->patterns,
						       FcPattern*,
						       i));
		}
		g_array_free(cache->patterns, TRUE);
		cache->patterns = NULL;
	}

	/* Close all faces. */
	for (iter = cache->faces; iter != NULL; iter = g_list_next(iter)) {
		FT_Done_Face((FT_Face) iter->data);
		iter->data = NULL;
	}
	cache->faces = NULL;

	/* Free the glyph tree. */
	g_tree_foreach(cache->cache, free_tree_value, NULL);
	cache->cache = NULL;

	/* Close the FT library. */
	if (cache->ft_library) {
		FT_Done_FreeType(cache->ft_library);
		cache->ft_library = NULL;
	}

	/* Free the cache. */
	cache->ft_load_flags = 0;
	cache->ft_render_flags = 0;
	cache->width = 0;
	cache->height = 0;
	cache->ascent = 0;
	g_slice_free(struct _vte_glyph_cache, cache);
}

void
_vte_glyph_cache_set_font_description(GtkWidget *widget,
				      FcConfig *config,
				      struct _vte_glyph_cache *cache,
				      const PangoFontDescription *fontdesc,
				      VteTerminalAntiAlias antialias,
				      _vte_fc_defaults_cb defaults_cb,
				      gpointer defaults_data)
{
	FcChar8 *facefile;
	int i, j, error, count, width, faceindex;
	double dpi, size;
	GList *iter;
	FcPattern *pattern;
	GArray *patterns;
	FT_Face face;
	gunichar double_wide_characters[] = {VTE_DRAW_DOUBLE_WIDE_CHARACTERS};

	g_return_if_fail(cache != NULL);
	g_return_if_fail(fontdesc != NULL);

	/* Convert the font description to a sorted set of patterns. */
	patterns = g_array_new(TRUE, TRUE, sizeof(FcPattern*));
	if (!_vte_fc_patterns_from_pango_font_desc(widget, fontdesc,
						   antialias,
						   patterns,
						   defaults_cb,
						   defaults_data)) {
		g_array_free(patterns, TRUE);
		g_assert_not_reached();
	}

	/* Set the pattern list. */
	if (cache->patterns != NULL) {
		g_array_free(cache->patterns, TRUE);
	}
	cache->patterns = patterns;

	/* Clear the face list. */
	for (iter = cache->faces; iter != NULL; iter = g_list_next(iter)) {
		FT_Done_Face((FT_Face) iter->data);
		iter->data = NULL;
	}
	g_list_free(cache->faces);
	cache->faces = NULL;

	/* Clear the glyph tree. */
	g_tree_foreach(cache->cache, free_tree_value, NULL);
	g_tree_destroy(cache->cache);
	cache->cache = g_tree_new(_vte_direct_compare);

	/* Clear the load and render flags. */
	cache->ft_load_flags = 0;
	cache->ft_render_flags = 0;

	/* Open the all of the faces to which the patterns resolve. */
	for (i = 0; i < cache->patterns->len; i++) {
		pattern = g_array_index(cache->patterns, FcPattern*, i);
		j = 0;
		while (FcPatternGetString(pattern, FC_FILE, j,
					  &facefile) == FcResultMatch) {
			face = NULL;
			if (FcPatternGetInteger(pattern, FC_INDEX, i,
						&faceindex) != FcResultMatch) {
				faceindex = 0;
			}
			error = FT_New_Face(cache->ft_library,
					    facefile, faceindex,
					    &face);
			if (error == 0) {
				/* Set the requested size.  FIXME: what do we
				   do if horizontal and vertical DPI aren't the
				   same? */
				dpi = 72;
				FcPatternGetDouble(pattern, FC_DPI, 0, &dpi);
				size = 12;
				FcPatternGetDouble(pattern, FC_SIZE, 0, &size);
				FT_Set_Char_Size(face, 0, floor(size * 64.0),
						 floor(dpi), floor(dpi));
				cache->faces = g_list_append(cache->faces,
							     face);
			} else {
				if (face != NULL) {
					FT_Done_Face(face);
				}
				face = NULL;
			}
			j++;
		}
	}

	/* Make sure that we were able to load at least one face. */
	g_assert(cache->faces != NULL);

	/* Pull out other settings. */
	cache->ft_load_flags = 0;
	cache->ft_render_flags = 0;
	i = 0;
	pattern = g_array_index(cache->patterns, FcPattern*, 0);
	/* Read and set the "use the autohinter", er, hint. */
#if defined(FC_AUTOHINT) && defined(FT_LOAD_FORCE_AUTOHINT)
	if (FcPatternGetBool(pattern, FC_AUTOHINT, 0, &i) == FcResultMatch) {
		if (i != 0) {
			cache->ft_load_flags |= FT_LOAD_FORCE_AUTOHINT;
		}
	}
#endif
	/* Read and set the "use antialiasing" hint. */
	if (FcPatternGetBool(pattern, FC_ANTIALIAS, 0, &i) == FcResultMatch) {
		if (i == 0) {
			cache->ft_load_flags |= FT_LOAD_MONOCHROME;
#if HAVE_DECL_FT_RENDER_MODE_MONO
			cache->ft_render_flags = FT_RENDER_MODE_MONO;
#endif
#if HAVE_DECL_ft_render_mode_mono
			cache->ft_render_flags = ft_render_mode_mono;
#endif
		}
	}
	/* Read and set the "hinting" hint. */
	if (FcPatternGetBool(pattern, FC_HINTING, 0, &i) == FcResultMatch) {
		if (i == 0) {
			cache->ft_load_flags |= FT_LOAD_NO_HINTING;
		} else {
#if defined(FC_AUTOHINT) && defined(FT_LOAD_FORCE_AUTOHINT)
			if (FcPatternGetBool(pattern, FC_AUTOHINT, 0,
					     &i) == FcResultMatch) {
				if (i != 0) {
					cache->ft_render_flags |=
						FT_LOAD_FORCE_AUTOHINT;
				}
			}
#endif
#ifdef FC_HINT_STYLE
			if (FcPatternGetInteger(pattern, FC_HINT_STYLE, 0,
						&i) == FcResultMatch) {
				switch (i) {
#if HAVE_DECL_FT_LOAD_NO_HINTING
				case FC_HINT_NONE:
					cache->ft_load_flags |=
						FT_LOAD_NO_HINTING;
					break;
#endif
#if 0
/* FT_RENDER_MODE_LIGHT doesn't appear to work reliably enough. */
#if HAVE_DECL_FT_RENDER_MODE_LIGHT
				case FC_HINT_SLIGHT:
					cache->ft_render_flags |=
						FT_RENDER_MODE_LIGHT;
					break;
#endif
#if HAVE_DECL_FT_RENDER_MODE_LIGHT
				case FC_HINT_MEDIUM:
					cache->ft_render_flags |=
						FT_RENDER_MODE_LIGHT;
					break;
#endif
#endif
#if HAVE_DECL_FT_RENDER_MODE_NORMAL
				case FC_HINT_FULL:
					cache->ft_render_flags |=
						FT_RENDER_MODE_NORMAL;
					break;
#endif
				default:
					break;
				}
			}
#endif
		}
	}

	/* Calculate average cell size using the first face. */
	cache->width = 0;
	cache->height = 0;
	cache->ascent = 0;
	count = 0;
	for (i = 0; VTE_DRAW_SINGLE_WIDE_CHARACTERS[i] != '\0'; i++) {
		face = _vte_glyph_cache_face_for_char(cache,
						      VTE_DRAW_SINGLE_WIDE_CHARACTERS[i]);
		if (face == NULL) {
			face = cache->faces->data;
		}
		error = FT_Load_Char((FT_Face) face,
				     VTE_DRAW_SINGLE_WIDE_CHARACTERS[i],
				     cache->ft_load_flags);
		if (error == 0) {
			error = FT_Render_Glyph(face->glyph,
						cache->ft_render_flags);
		}
		if (error == 0) {
			cache->width += face->glyph->metrics.horiAdvance;
			if (face->size->metrics.ascender != 0) {
				cache->height += face->size->metrics.ascender -
						 face->size->metrics.descender;
				cache->ascent += face->size->metrics.ascender;
			} else
			if (face->glyph->metrics.height != 0) {
				cache->height += face->glyph->metrics.height;
				cache->ascent += face->glyph->metrics.height;
			} else {
				cache->height += face->glyph->bitmap.rows * 64;
				cache->ascent += face->glyph->bitmap.rows * 64;
			}
			count++;
		}
	}
	if (count > 0) {
		cache->width = howmany(cache->width / 64, count);
		cache->height = howmany((cache->height / 64), count);
		cache->ascent = howmany((cache->ascent / 64), count);
	} else {
		cache->width = 1;
		cache->height = 1;
		cache->ascent = 1;
	}
	width = 0;
	for (i = 0; i < G_N_ELEMENTS(double_wide_characters); i++) {
		face = _vte_glyph_cache_face_for_char(cache,
						      double_wide_characters[i]);
		if (face == NULL) {
			continue;
		}
		error = FT_Load_Char((FT_Face) face,
				     double_wide_characters[i],
				     cache->ft_load_flags);
		if (error == 0) {
			error = FT_Render_Glyph(face->glyph,
						cache->ft_render_flags);
		}
		if (error == 0) {
			width += face->glyph->metrics.horiAdvance;
			count++;
		}
	}
	if (count > 0) {
		if (cache->width == width / 64 / count) {
			cache->width /= 2;
		}
	}
}

static FT_Face
_vte_glyph_cache_face_for_char(struct _vte_glyph_cache *cache, gunichar c)
{
	GList *iter;
	for (iter = cache->faces; iter != NULL; iter = g_list_next(iter)) {
		if (FT_Get_Char_Index((FT_Face) iter->data, c) != 0) {
			return (FT_Face) iter->data;
		}
	}
	return NULL;
}

gboolean
_vte_glyph_cache_has_char(struct _vte_glyph_cache *cache, gunichar c)
{
	GList *iter;
	gpointer p;

	if ((p = g_tree_lookup(cache->cache, GINT_TO_POINTER(c))) != NULL) {
		if (GPOINTER_TO_INT(p) == INVALID_GLYPH) {
			return FALSE;
		}
	}

	for (iter = cache->faces; iter != NULL; iter = g_list_next(iter)) {
		if (FT_Get_Char_Index((FT_Face) iter->data, c) != 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static gunichar
_vte_glyph_remap_char(struct _vte_glyph_cache *cache, gunichar origc)
{
	gunichar newc;

	if (_vte_glyph_cache_has_char(cache, origc)) {
		return origc;
	}

	switch (origc) {
	case 0:			/* NUL */
	case 0x00A0:		/* NO-BREAK SPACE */
		newc = 0x0020;	/* SPACE */
		break;
	case 0x2010:		/* HYPHEN */
	case 0x2011:		/* NON-BREAKING HYPHEN */
	case 0x2012:		/* FIGURE DASH */
	case 0x2013:		/* EN DASH */
	case 0x2014:		/* EM DASH */
	case 0x2212:		/* MINUS SIGN */
		newc = 0x002D;	/* HYPHEN-MINUS */
		break;
	default:
		newc = origc;
		break;
	}

	if (_vte_glyph_cache_has_char(cache, newc)) {
		return newc;
	} else {
		return origc;
	}
}

#define DEFAULT_BYTES_PER_PIXEL 3

struct _vte_glyph *
_vte_glyph_get_uncached(struct _vte_glyph_cache *cache, gunichar c)
{
	int error = 0;
	GList *iter;
	struct _vte_glyph *glyph = NULL;
	FT_Face face;
	gint x, y, ooffset, ioffset;
	guchar r, g, b, t;

	g_return_val_if_fail(cache != NULL, NULL);

	/* Search through all of the faces to find one which contains a glyph
	 * for this character. */
	iter = cache->faces;
	face = NULL;
	while (iter != NULL) {
		/* Check if the face contains a proper glyph.  We do this
		 * separately because we don't want the "unknown glyph"
		 * glyph. */
		if (FT_Get_Char_Index((FT_Face) iter->data, c) == 0) {
			/* Try the next face. */
			iter = g_list_next(iter);
			face = NULL;
			continue;
		}
		/* Try to load the character. */
		error = FT_Load_Char((FT_Face) iter->data,
				     c,
				     cache->ft_load_flags);
		if (error != 0) {
			/* Try the next face. */
			iter = g_list_next(iter);
			face = NULL;
			continue;
		}
		/* Try to render the character. */
		error = FT_Render_Glyph(((FT_Face) iter->data)->glyph,
					cache->ft_render_flags);
		if (error != 0) {
			/* Try the next face. */
			iter = g_list_next(iter);
			face = NULL;
			continue;
		}

		/* Keep track of which face loaded it. */
		face = iter->data;
		break;
	}

	/* Bail if we weren't able to load the glyph. */
	if (face == NULL) {
		g_tree_insert(cache->cache, GINT_TO_POINTER(c),
			      GINT_TO_POINTER(INVALID_GLYPH));
		return NULL;
	}

	/* Build a new glyph. */
	glyph = g_malloc0(sizeof(struct _vte_glyph) +
			  face->glyph->bitmap.width *
			  face->glyph->bitmap.rows *
			  DEFAULT_BYTES_PER_PIXEL);
	glyph->width = face->glyph->bitmap.width;
	glyph->height = face->glyph->bitmap.rows;
	glyph->skip = MAX((face->size->metrics.ascender >> 6) -
			  face->glyph->bitmap_top, 0);
	glyph->bytes_per_pixel = DEFAULT_BYTES_PER_PIXEL;

	memset(glyph->bytes, 0,
	       glyph->width * glyph->height * DEFAULT_BYTES_PER_PIXEL);

	for (y = 0; y < face->glyph->bitmap.rows; y++)
	for (x = 0; x < face->glyph->bitmap.width; x++) {
		ooffset = (y * glyph->width + x) * DEFAULT_BYTES_PER_PIXEL;
		if (face->glyph->bitmap.pitch > 0) {
			ioffset = y;
			ioffset *= face->glyph->bitmap.pitch;
		} else {
			ioffset = face->glyph->bitmap.rows - (y + 1);
			ioffset *= (-face->glyph->bitmap.pitch);
		}
		switch (face->glyph->bitmap.pixel_mode) {
#if HAVE_DECL_FT_PIXEL_MODE_MONO
		case FT_PIXEL_MODE_MONO:
#else
#if HAVE_DECL_ft_pixel_mode_mono
		case ft_pixel_mode_mono:
#else
#error "Neither ft_pixel_mode_mono nor FT_PIXEL_MODE_MONO is defined!"
#endif
#endif
			ioffset += (x / 8);
			t = (face->glyph->bitmap.buffer[ioffset] << (x % 8));
			r = g = b = (t >> 7) ? 0xff : 0;
			break;
#if HAVE_DECL_FT_PIXEL_MODE_GRAY2
		case FT_PIXEL_MODE_GRAY2:
			ioffset += (x / 4);
			t = (face->glyph->bitmap.buffer[ioffset] << ((x % 4) * 2));
			r = g = b = CLAMP((t >> 6) * 0x55, 0, 0xff);
			break;
#endif
#if HAVE_DECL_FT_PIXEL_MODE_GRAY4
		case FT_PIXEL_MODE_GRAY4:
			ioffset += (x / 2);
			t = (face->glyph->bitmap.buffer[ioffset] << ((x % 2) * 4)) & 7;
			r = g = b = CLAMP((t >> 4) * 0x25, 0, 0xff);
			break;
#endif
#if HAVE_DECL_FT_PIXEL_MODE_LCD
		case FT_PIXEL_MODE_LCD:
		case FT_PIXEL_MODE_LCD_V:
			ioffset += (x * 3);
			r = face->glyph->bitmap.buffer[ioffset + 0];
			g = face->glyph->bitmap.buffer[ioffset + 1];
			b = face->glyph->bitmap.buffer[ioffset + 2];
			break;
#endif
#if HAVE_DECL_FT_PIXEL_MODE_GRAY
		case FT_PIXEL_MODE_GRAY:
#else
#if HAVE_DECL_ft_pixel_mode_grays
		case ft_pixel_mode_grays:
#else
#error "Neither FT_PIXEL_MODE_GRAY nor ft_pixel_mode_grays is defined!"
#endif
#endif
			ioffset += x;
			r = g = b = face->glyph->bitmap.buffer[ioffset];
			break;
		default:
			g_error(_("Unknown pixel mode %d.\n"),
				face->glyph->bitmap.pixel_mode);
			r = g = b = 0;
			g_assert_not_reached();
			break;
		}
		if (face->glyph->bitmap.pitch > 0) {
			g_assert(ioffset < ((y + 1) * face->glyph->bitmap.pitch));
		} else {
			g_assert(ioffset < ((y + 1) * -face->glyph->bitmap.pitch));
		}
		glyph->bytes[ooffset + 0] = r;
		glyph->bytes[ooffset + 1] = g;
		glyph->bytes[ooffset + 2] = b;
#if DEFAULT_BYTES_PER_PIXEL > 3
		memset(glyph->bytes[ooffset + 3], 0xff,
		       DEFAULT_BYTES_PER_PIXEL - 3);
#endif
	}

	return glyph;
}

const struct _vte_glyph *
_vte_glyph_get(struct _vte_glyph_cache *cache, gunichar c)
{
	struct _vte_glyph *glyph = NULL;
	gpointer p;

	g_return_val_if_fail(cache != NULL, NULL);

	/* See if we already have a glyph for this character. */
	if ((p = g_tree_lookup(cache->cache, GINT_TO_POINTER(c))) != NULL) {
		if (GPOINTER_TO_INT(p) == INVALID_GLYPH) {
			return NULL;
		} else {
			return p;
		}
	}

	/* Generate the glyph. */
	glyph = _vte_glyph_get_uncached(cache, c);

	/* Bail if we weren't able to load the glyph. */
	if (glyph == NULL) {
		g_tree_insert(cache->cache, GINT_TO_POINTER(c),
			      GINT_TO_POINTER(INVALID_GLYPH));
		return NULL;
	}

	/* Cache it. */
	g_tree_insert(cache->cache, GINT_TO_POINTER(c), glyph);

	return glyph;
}

void
_vte_glyph_draw(struct _vte_glyph_cache *cache,
		gunichar c, GdkColor *color,
		gint x, gint y, gint columns,
		enum vte_glyph_flags flags,
		struct _vte_rgb_buffer *buffer)
{
	const struct _vte_glyph *glyph;
	gint col, row, ioffset, ooffset, icol, ocol, ecol;
	gint strikethrough, underline, underline2;
	gint32 r, g, b, ar, ag, ab;
	guchar *pixels;

	if (cache == NULL) {
		return;
	}
	glyph = _vte_glyph_get(cache, _vte_glyph_remap_char(cache, c));
	if (glyph == NULL) {
		return;
	}

	if (x > buffer->width) {
		return;
	}
	if (y > buffer->height) {
		return;
	}

	pixels = buffer->pixels;
	r = color->red >> 8;
	g = color->green >> 8;
	b = color->blue >> 8;

	if (cache->ascent > 0) {
		strikethrough = cache->ascent >> 1;
		underline = cache->ascent + 1;
		underline2 = cache->ascent + 2;
	} else {
		strikethrough = MAX(0, cache->height >> 1);
		underline = MAX(0, cache->height - 2);
		underline2 = MAX(0, cache->height - 1);
	}

	icol = MAX(0, (glyph->width - (columns * cache->width)) >> 1);
	ocol = MAX(0, ((columns * cache->width) - glyph->width) >> 1);

_vte_glyph_draw_loop:

	for (row = glyph->skip;
	     row < MIN(cache->height, glyph->skip + glyph->height);
	     row++) {
		if (row + y >= buffer->height) {
			break;
		}
		ooffset = (y + row) * buffer->stride +
			  ((x + ocol) * 3);
		ioffset = (((row - glyph->skip) * glyph->width) + icol) *
			  DEFAULT_BYTES_PER_PIXEL;
		ecol = MIN(cache->width * columns, glyph->width);
		for (col = 0; col < ecol; col++) {
			if (col + x >= buffer->width) {
				break;
			}
			ar = glyph->bytes[ioffset + 0];
			ag = glyph->bytes[ioffset + 1];
			ab = glyph->bytes[ioffset + 2];
			ioffset += DEFAULT_BYTES_PER_PIXEL;

			if (flags & vte_glyph_dim) {
				ar = ar >> 1;
				ag = ag >> 1;
				ab = ab >> 1;
			}

			switch (ar) {
			case 0:
				break;
			case 0xff:
				pixels[ooffset + 0] = r;
				break;
			default:
				pixels[ooffset + 0] +=
					(((r - pixels[ooffset + 0]) * ar) >> 8);
				break;
			}

			switch (ag) {
			case 0:
				break;
			case 0xff:
				pixels[ooffset + 1] = g;
				break;
			default:
				pixels[ooffset + 1] +=
					(((g - pixels[ooffset + 1]) * ag) >> 8);
				break;
			}

			switch (ab) {
			case 0:
				break;
			case 0xff:
				pixels[ooffset + 2] = b;
				break;
			default:
				pixels[ooffset + 2] +=
					(((b - pixels[ooffset + 2]) * ab) >> 8);
				break;
			}

			ooffset += 3;
		}
	}

	if (flags &
	    (vte_glyph_underline | vte_glyph_underline2 |
	     vte_glyph_strikethrough | vte_glyph_boxed)) {
		for (col = 0; col < cache->width; col++) {
			if ((flags & vte_glyph_strikethrough) &&
			    (strikethrough >= 0) &&
			    (strikethrough < cache->height)) {
				ooffset = (y + strikethrough) * buffer->stride +
					  (x + col) * 3;
				pixels[ooffset + 0] = r;
				pixels[ooffset + 1] = g;
				pixels[ooffset + 2] = b;
			}
			if ((flags & vte_glyph_underline) &&
			    (underline >= 0) &&
			    (underline < cache->height)) {
				ooffset = (y + underline) * buffer->stride +
					  (x + col) * 3;
				pixels[ooffset + 0] = r;
				pixels[ooffset + 1] = g;
				pixels[ooffset + 2] = b;
			}
			if ((flags & vte_glyph_underline2) &&
			    (underline2 >= 0) &&
			    (underline2 < cache->height)) {
				ooffset = (y + underline2) * buffer->stride +
					  (x + col) * 3;
				pixels[ooffset + 0] = r;
				pixels[ooffset + 1] = g;
				pixels[ooffset + 2] = b;
			}
			if (flags & vte_glyph_boxed) {
				ooffset = y * buffer->stride + (x + col) * 3;
				pixels[ooffset + 0] = r;
				pixels[ooffset + 1] = g;
				pixels[ooffset + 2] = b;
				ooffset = (y + cache->height - 1) * buffer->stride +
					  (x + col) * 3;
				pixels[ooffset + 0] = r;
				pixels[ooffset + 1] = g;
				pixels[ooffset + 2] = b;
			}
		}
	}

	if (flags & vte_glyph_bold) {
		flags &= ~vte_glyph_bold;
		pixels += 3;
		goto _vte_glyph_draw_loop;
	}
}

void
_vte_glyph_draw_string(struct _vte_glyph_cache *cache,
		       const char *s, GdkColor *color,
		       gint x, gint y,
		       enum vte_glyph_flags flags,
		       struct _vte_rgb_buffer *buffer)
{
	gunichar c;
	gint width;

	if (y + cache->height > buffer->height) {
		return;
	}

	while (*s != '\0') {
		c = g_utf8_get_char(s);
		width = _vte_iso2022_unichar_width(c);
		g_assert(width >= 0);
		if (x + width * cache->width > buffer->width) {
			break;
		}
		_vte_glyph_draw(cache, c, color, x, y, width, flags, buffer);
		x += (width * cache->width);
		s = g_utf8_next_char(s);
	}
}
