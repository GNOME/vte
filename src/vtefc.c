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

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <fontconfig/fontconfig.h>
#include <glib.h>
#include "vtefc.h"

static int
_vte_fc_weight_from_pango_weight(int weight)
{
	/* Cut-and-pasted from Pango. */
	if (weight < (PANGO_WEIGHT_NORMAL + PANGO_WEIGHT_LIGHT) / 2)
		return FC_WEIGHT_LIGHT;
	else if (weight < (PANGO_WEIGHT_NORMAL + 600) / 2)
		return FC_WEIGHT_MEDIUM;
	else if (weight < (600 + PANGO_WEIGHT_BOLD) / 2)
		return FC_WEIGHT_DEMIBOLD;
	else if (weight < (PANGO_WEIGHT_BOLD + PANGO_WEIGHT_ULTRABOLD) / 2)
		return FC_WEIGHT_BOLD;
	else
		return FC_WEIGHT_BLACK;
}

static int
_vte_fc_slant_from_pango_style(int style)
{
	switch (style) {
	case PANGO_STYLE_NORMAL:
		return FC_SLANT_ROMAN;
		break;
	case PANGO_STYLE_ITALIC:
		return FC_SLANT_ITALIC;
		break;
	case PANGO_STYLE_OBLIQUE:
		return FC_SLANT_OBLIQUE;
		break;
	}
	return FC_SLANT_ROMAN;
}

static void
_vte_fc_transcribe_from_pango_font_description(FcPattern *pattern,
				       const PangoFontDescription *font_desc)
{
	const char *family = "monospace";
	PangoLanguage *language;
	double size = 10.0;
	int pango_mask;
	PangoContext *context;
	int weight, style;

	if (font_desc == NULL) {
		return;
	}

	pango_mask = pango_font_description_get_set_fields(font_desc);

	/* Set the family for the pattern, or use a sensible default. */
	if (pango_mask & PANGO_FONT_MASK_FAMILY) {
		family = pango_font_description_get_family(font_desc);
	}
	FcPatternAddString(pattern, FC_FAMILY, family);

	/* Set the font size for the pattern, or use a sensible default. */
	if (pango_mask & PANGO_FONT_MASK_SIZE) {
		size = pango_font_description_get_size(font_desc);
		size /= PANGO_SCALE;
	}
	FcPatternAddDouble(pattern, FC_SIZE, size);

	/* Set the language for the pattern. */
#if GTK_CHECK_VERSION(2,2,0)
	context = gdk_pango_context_get_for_screen(gdk_screen_get_default());
#else
	context = gdk_pango_context_get();
#endif
	language = pango_context_get_language(context);
	if (pango_language_to_string(language) != NULL) {
		FcPatternAddString(pattern, FC_LANG,
				   pango_language_to_string(language));
	}

	/* There aren't any fallbacks for these, so just omit them from the
	 * pattern if they're not set in the pango font. */
	if (pango_mask & PANGO_FONT_MASK_WEIGHT) {
		weight = pango_font_description_get_weight(font_desc);
		FcPatternAddInteger(pattern, FC_WEIGHT,
				    _vte_fc_weight_from_pango_weight(weight));
	}

	if (pango_mask & PANGO_FONT_MASK_STYLE) {
		style = pango_font_description_get_style(font_desc);
		FcPatternAddInteger(pattern, FC_SLANT,
				    _vte_fc_slant_from_pango_style(style));
	}
}

static void
_vte_fc_defaults_from_gtk(FcPattern *pattern)
{
	GtkSettings *settings;
#if GTK_CHECK_VERSION(2,2,0)
	GdkScreen *screen;
#endif
	GObjectClass *klass;
	int i, antialias = -1, hinting = -1, dpi = -1;
	double d;
	char *rgba = NULL, *hintstyle = NULL;
	FcResult result;

	/* Add any defaults configured for GTK+. */
#if GTK_CHECK_VERSION(2,2,0)
	screen = gdk_screen_get_default();
	settings = gtk_settings_get_for_screen(screen);
#else
	settings = gtk_settings_get_default();
#endif
	if (settings == NULL) {
		return;
	}

	/* Check that the properties we're looking at are defined. */
	klass = G_OBJECT_CLASS(GTK_SETTINGS_GET_CLASS(settings));
	if (g_object_class_find_property(klass, "gtk-xft-antialias") == NULL) {
		return;
	}

	/* Read the settings. */
	g_object_get(G_OBJECT(settings),
		     "gtk-xft-antialias", &antialias,
		     "gtk-xft-dpi", &dpi,
		     "gtk-xft-rgba", &rgba,
		     "gtk-xft-hinting", &hinting,
		     "gtk-xft-hintstyle", &hintstyle,
		     NULL);

	/* Pick up the antialiasing setting. */
	if (antialias >= 0) {
		FcPatternDel(pattern, FC_ANTIALIAS);
		FcPatternAddBool(pattern, FC_ANTIALIAS, antialias > 0);
	}

	/* Pick up the configured DPI setting. */
	if (dpi >= 0) {
		FcPatternDel(pattern, FC_DPI);
		FcPatternAddDouble(pattern, FC_DPI, dpi / 1024.0);
	}

	/* Pick up the configured subpixel rendering setting. */
	if (rgba != NULL) {
		gboolean found;

		i = FC_RGBA_NONE;

		if (strcmp(rgba, "none") == 0) {
			i = FC_RGBA_NONE;
			found = TRUE;
		} else
		if (strcmp(rgba, "rgb") == 0) {
			i = FC_RGBA_RGB;
			found = TRUE;
		} else
		if (strcmp(rgba, "bgr") == 0) {
			i = FC_RGBA_BGR;
			found = TRUE;
		} else
		if (strcmp(rgba, "vrgb") == 0) {
			i = FC_RGBA_VRGB;
			found = TRUE;
		} else
		if (strcmp(rgba, "vbgr") == 0) {
			i = FC_RGBA_VBGR;
			found = TRUE;
		} else {
			found = FALSE;
		}
		if (found) {
			FcPatternDel(pattern, FC_RGBA);
			FcPatternAddInteger(pattern, FC_RGBA, i);
		}
	}

	/* Pick up the configured hinting setting. */
	if (hinting >= 0) {
		FcPatternDel(pattern, FC_HINTING);
		FcPatternAddBool(pattern, FC_HINTING, hinting > 0);
	}

#ifdef FC_HINT_STYLE
	/* Pick up the default hinting style. */
	if (hintstyle != NULL) {
		gboolean found;

		i = FC_HINT_NONE;

		if (strcmp(hintstyle, "hintnone") == 0) {
			i = FC_HINT_NONE;
			found = TRUE;
		} else
		if (strcmp(hintstyle, "hintslight") == 0) {
			i = FC_HINT_SLIGHT;
			found = TRUE;
		} else
		if (strcmp(hintstyle, "hintmedium") == 0) {
			i = FC_HINT_MEDIUM;
			found = TRUE;
		} else
		if (strcmp(hintstyle, "hintfull") == 0) {
			i = FC_HINT_FULL;
			found = TRUE;
		} else {
			found = FALSE;
		}
		if (found) {
			FcPatternDel(pattern, FC_HINT_STYLE);
			FcPatternAddInteger(pattern, FC_HINT_STYLE, i);
		}
	}
#endif
}

/* Create a sorted set of fontconfig patterns from a Pango font description
 * and append them to the array. */
gboolean
_vte_fc_patterns_from_pango_font_desc(const PangoFontDescription *font_desc,
				      GArray *pattern_array)
{
	FcPattern *pattern, *match, *tmp;
	FcFontSet *fontset;
	FcResult result;
	gboolean ret = FALSE;
	int i;

	g_return_val_if_fail(pattern_array != NULL, FALSE);

	/* Create a scratch pattern. */
	pattern = FcPatternCreate();

	/* Transcribe what we can get from the Pango font description. */
	_vte_fc_transcribe_from_pango_font_description(pattern, font_desc);

	/* Add any defaults specified in the configuration. */
	FcConfigSubstitute(NULL, pattern, FcMatchPattern);

	/* Add any defaults configured for GTK+. */
	_vte_fc_defaults_from_gtk(pattern);

	/* Add any defaults which are hard-coded in fontconfig. */
	FcDefaultSubstitute(pattern);

	/* Get a sorted list of patterns, duplicate them, and append them
	 * to the passed-in array. */
	fontset = FcFontSort(NULL, pattern, FcTrue, NULL, &result);
	if (fontset != NULL) {
		for (i = 0; i < fontset->nfont; i++) {
			tmp = FcFontRenderPrepare(NULL,
						  pattern,
						  fontset->fonts[i]);
			_vte_fc_defaults_from_gtk(tmp);
			g_array_append_val(pattern_array, tmp);
		}
		FcFontSetDestroy(fontset);
		ret = TRUE;
	}

	/* Last ditch effort. */
	if (pattern_array->len == 0) {
		match = FcFontMatch(NULL, pattern, &result);
		if (result == FcResultMatch) {
			tmp = FcPatternDuplicate(match);
			_vte_fc_defaults_from_gtk(tmp);
			g_array_append_val(pattern_array, tmp);
			ret = TRUE;
		} else {
			ret = FALSE;
		}
	}

	FcPatternDestroy(pattern);

	return ret;
}

void
_vte_fc_connect_settings_changes(GtkWidget *widget, GCallback *changed_cb)
{
	GtkSettings *settings;
	GObjectClass *klass;

	/* Get the settings object used by the widget. */
	settings = gtk_widget_get_settings(widget);
	if (settings == NULL) {
		return;
	}

	/* Check that the properties we're looking at are defined. */
	klass = G_OBJECT_CLASS(GTK_SETTINGS_GET_CLASS(settings));
	if (g_object_class_find_property(klass, "gtk-xft-antialias") == NULL) {
		return;
	}

	/* Start listening for changes to the fontconfig settings. */
	g_signal_connect(G_OBJECT(settings),
			 "notify::gtk-xft-antialias",
			 G_CALLBACK(changed_cb), widget);
	g_signal_connect(G_OBJECT(settings),
			 "notify::gtk-xft-hinting",
			 G_CALLBACK(changed_cb), widget);
	g_signal_connect(G_OBJECT(settings),
			 "notify::gtk-xft-hintstyle",
			 G_CALLBACK(changed_cb), widget);
	g_signal_connect(G_OBJECT(settings),
			 "notify::gtk-xft-rgba",
			 G_CALLBACK(changed_cb), widget);
	g_signal_connect(G_OBJECT(settings),
			 "notify::gtk-xft-dpi",
			 G_CALLBACK(changed_cb), widget);
}

void
_vte_fc_disconnect_settings_changes(GtkWidget *widget, GCallback *changed_cb)
{
	GtkSettings *settings;

	/* Get the settings object used by the widget. */
	settings = gtk_widget_get_settings(widget);
	if (settings == NULL) {
		return;
	}

	/* Stop listening for changes to the fontconfig settings. */
	g_signal_handlers_disconnect_matched(G_OBJECT(settings),
					     G_SIGNAL_MATCH_FUNC |
					     G_SIGNAL_MATCH_DATA,
					     0, 0, NULL,
					     changed_cb,
					     widget);
}
