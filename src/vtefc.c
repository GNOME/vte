/*
 * Copyright (C) 2003,2004 Red Hat, Inc.
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
#include <pango/pango.h>
#include <fontconfig/fontconfig.h>
#include <glib.h>
#include "vtefc.h"
#include "vterdb.h"

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
_vte_fc_slant_from_pango_style(PangoStyle style)
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

static int
_vte_fc_width_from_pango_stretch(PangoStretch stretch)
{
	switch (stretch) {
	case PANGO_STRETCH_ULTRA_CONDENSED:
		return 60;
		break;
	case PANGO_STRETCH_EXTRA_CONDENSED:
		return 70;
		break;
	case PANGO_STRETCH_CONDENSED:
		return 80;
		break;
	case PANGO_STRETCH_SEMI_CONDENSED:
		return 90;
		break;
	case PANGO_STRETCH_NORMAL:
		return 100;
		break;
	case PANGO_STRETCH_SEMI_EXPANDED:
		return 105;
		break;
	case PANGO_STRETCH_EXPANDED:
		return 120;
		break;
	case PANGO_STRETCH_EXTRA_EXPANDED:
		return 150;
		break;
	case PANGO_STRETCH_ULTRA_EXPANDED:
		return 200;
		break;
	}
	return 100;
}

static void
_vte_fc_transcribe_from_pango_font_description(GtkWidget *widget,
					       FcPattern *pattern,
				       const PangoFontDescription *font_desc)
{
	GdkScreen *screen;
	const char *family = "monospace";
	PangoLanguage *language;
	double size = 10.0;
	int pango_mask;
	PangoContext *context;
	PangoWeight weight;
	PangoStyle style;
	PangoStretch stretch;

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
	if (gtk_widget_has_screen(widget)) {
		screen = gtk_widget_get_screen(widget);
	} else {
		screen = gdk_display_get_default_screen(gtk_widget_get_display(widget));
	}
	context = gdk_pango_context_get_for_screen(screen);
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

	if (pango_mask & PANGO_FONT_MASK_STRETCH) {
		stretch = pango_font_description_get_stretch(font_desc);
		FcPatternAddInteger(pattern, FC_WIDTH,
				    _vte_fc_width_from_pango_stretch(stretch));
	}

	if (pango_mask & PANGO_FONT_MASK_STYLE) {
		style = pango_font_description_get_style(font_desc);
		FcPatternAddInteger(pattern, FC_SLANT,
				    _vte_fc_slant_from_pango_style(style));
	}

	g_object_unref(context);
}

static void
_vte_fc_set_antialias(FcPattern *pattern, VteTerminalAntiAlias antialias)
{
	FcBool aa;
	if (antialias != VTE_ANTI_ALIAS_USE_DEFAULT) {
		if (FcPatternGetBool(pattern, FC_ANTIALIAS, 0,
				     &aa) != FcResultNoMatch) {
			FcPatternDel(pattern, FC_ANTIALIAS);
		}
		aa = (antialias == VTE_ANTI_ALIAS_FORCE_ENABLE) ?
		     FcTrue : FcFalse;
		FcPatternAddBool(pattern, FC_ANTIALIAS, aa);
	}
}

static void
_vte_fc_defaults_from_gtk(GtkWidget *widget, FcPattern *pattern,
			  VteTerminalAntiAlias explicit_antialias)
{
	GtkSettings *settings;
	GdkScreen *screen;
	GObjectClass *klass;
	int i, antialias = -1, hinting = -1, dpi = -1;
	char *rgba = NULL, *hintstyle = NULL;

	/* Add any defaults configured for GTK+. */
	if (gtk_widget_has_screen(widget)) {
		screen = gtk_widget_get_screen(widget);
	} else {
		screen = gdk_display_get_default_screen(gtk_widget_get_display(widget));
	}
	settings = gtk_settings_get_for_screen(screen);
	if (settings == NULL) {
		return;
	}

	/* Check that the properties we're looking at are defined. */
	klass = G_OBJECT_GET_CLASS(settings);
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
	_vte_fc_set_antialias(pattern, explicit_antialias);

	/* Pick up the configured DPI setting. */
	if (dpi >= 0) {
		FcPatternDel(pattern, FC_DPI);
		FcPatternAddDouble(pattern, FC_DPI, dpi / 1024.0);
	}

	/* Pick up the configured subpixel rendering setting. */
	if (rgba != NULL) {
		gboolean found;

		i = FC_RGBA_NONE;

		if (g_ascii_strcasecmp(rgba, "none") == 0) {
			i = FC_RGBA_NONE;
			found = TRUE;
		} else
		if (g_ascii_strcasecmp(rgba, "rgb") == 0) {
			i = FC_RGBA_RGB;
			found = TRUE;
		} else
		if (g_ascii_strcasecmp(rgba, "bgr") == 0) {
			i = FC_RGBA_BGR;
			found = TRUE;
		} else
		if (g_ascii_strcasecmp(rgba, "vrgb") == 0) {
			i = FC_RGBA_VRGB;
			found = TRUE;
		} else
		if (g_ascii_strcasecmp(rgba, "vbgr") == 0) {
			i = FC_RGBA_VBGR;
			found = TRUE;
		} else {
			found = FALSE;
		}
		if (found) {
			FcPatternDel(pattern, FC_RGBA);
			FcPatternAddInteger(pattern, FC_RGBA, i);
		}
		g_free(rgba);
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

		if (g_ascii_strcasecmp(hintstyle, "hintnone") == 0) {
			i = FC_HINT_NONE;
			found = TRUE;
		} else
		if (g_ascii_strcasecmp(hintstyle, "hintslight") == 0) {
			i = FC_HINT_SLIGHT;
			found = TRUE;
		} else
		if (g_ascii_strcasecmp(hintstyle, "hintmedium") == 0) {
			i = FC_HINT_MEDIUM;
			found = TRUE;
		} else
		if (g_ascii_strcasecmp(hintstyle, "hintfull") == 0) {
			i = FC_HINT_FULL;
			found = TRUE;
		} else {
			found = FALSE;
		}
		if (found) {
			FcPatternDel(pattern, FC_HINT_STYLE);
			FcPatternAddInteger(pattern, FC_HINT_STYLE, i);
		}
		g_free(hintstyle);
	}
#endif
}

static void
_vte_fc_defaults_from_rdb(GtkWidget *widget, FcPattern *pattern,
			  VteTerminalAntiAlias explicit_antialias)
{
	FcBool fcb;
	double fcd;
	int antialias = -1, hinting = -1, fci;
	double dpi;
	const char *rgba = NULL, *hintstyle = NULL;

	/* Read the settings. */
	hintstyle = _vte_rdb_get_hintstyle(widget);
	rgba = _vte_rdb_get_rgba(widget);

	/* Pick up the antialiasing setting. */
	if (FcPatternGetBool(pattern, FC_ANTIALIAS, 0,
			     &fcb) == FcResultNoMatch) {
		antialias = _vte_rdb_get_antialias(widget);
		FcPatternAddBool(pattern, FC_ANTIALIAS, antialias);
	}
	if (explicit_antialias != VTE_ANTI_ALIAS_USE_DEFAULT) {
		FcBool aa;
		if (FcPatternGetBool(pattern, FC_ANTIALIAS, 0,
				     &aa) != FcResultNoMatch) {
			FcPatternDel(pattern, FC_ANTIALIAS);
		}
		aa = (explicit_antialias == VTE_ANTI_ALIAS_FORCE_ENABLE) ?
		     FcTrue : FcFalse;
		FcPatternAddBool(pattern, FC_ANTIALIAS, aa);
	}

	/* Pick up the hinting setting. */
	if (FcPatternGetBool(pattern, FC_HINTING, 0,
			     &fcb) == FcResultNoMatch) {
		hinting = _vte_rdb_get_hinting(widget);
		FcPatternAddBool(pattern, FC_HINTING, hinting);
	}

	/* Pick up the configured DPI setting. */
	if (FcPatternGetDouble(pattern, FC_DPI, 0,
			       &fcd) == FcResultNoMatch) {
		dpi = _vte_rdb_get_dpi(widget);
		if (dpi >= 0) {
			FcPatternAddDouble(pattern, FC_DPI, dpi);
		}
	}

	/* Pick up the configured subpixel rendering setting. */
	if (FcPatternGetInteger(pattern, FC_RGBA, 0,
				&fci) == FcResultNoMatch) {
		rgba = _vte_rdb_get_rgba(widget);
		if (g_ascii_strcasecmp(rgba, "none") == 0) {
			FcPatternAddInteger(pattern, FC_RGBA, FC_RGBA_NONE);
		} else
		if (g_ascii_strcasecmp(rgba, "rgb") == 0) {
			FcPatternAddInteger(pattern, FC_RGBA, FC_RGBA_RGB);
		} else
		if (g_ascii_strcasecmp(rgba, "bgr") == 0) {
			FcPatternAddInteger(pattern, FC_RGBA, FC_RGBA_BGR);
		} else
		if (g_ascii_strcasecmp(rgba, "vrgb") == 0) {
			FcPatternAddInteger(pattern, FC_RGBA, FC_RGBA_VRGB);
		} else
		if (g_ascii_strcasecmp(rgba, "vbgr") == 0) {
			FcPatternAddInteger(pattern, FC_RGBA, FC_RGBA_VBGR);
		}
	}

#ifdef FC_HINT_STYLE
	/* Pick up the default hinting style. */
	if (FcPatternGetInteger(pattern, FC_HINT_STYLE, 0,
				&fci) == FcResultNoMatch) {
		hintstyle = _vte_rdb_get_hintstyle(widget);
		if (g_ascii_strcasecmp(hintstyle, "hintnone") == 0) {
			FcPatternAddInteger(pattern, FC_HINT_STYLE,
					    FC_HINT_NONE);
		} else
		if (g_ascii_strcasecmp(hintstyle, "hintslight") == 0) {
			FcPatternAddInteger(pattern, FC_HINT_STYLE,
					    FC_HINT_SLIGHT);
		} else
		if (g_ascii_strcasecmp(hintstyle, "hintmedium") == 0) {
			FcPatternAddInteger(pattern, FC_HINT_STYLE,
					    FC_HINT_MEDIUM);
		} else
		if (g_ascii_strcasecmp(hintstyle, "hintfull") == 0) {
			FcPatternAddInteger(pattern, FC_HINT_STYLE,
					    FC_HINT_FULL);
		}
	}
#endif
}

/* Create a sorted set of fontconfig patterns from a Pango font description
 * and append them to the array. */
gboolean
_vte_fc_patterns_from_pango_font_desc(GtkWidget *widget,
				      const PangoFontDescription *font_desc,
				      VteTerminalAntiAlias antialias,
				      GPtrArray *pattern_array,
				      _vte_fc_defaults_cb defaults_cb,
				      gpointer defaults_data)

{
	FcPattern *pattern, *match, *tmp, *save;
	FcFontSet *fontset;
	FcResult result;
	gboolean ret = FALSE;
	int i;

	g_return_val_if_fail(pattern_array != NULL, FALSE);

	/* Create a scratch pattern. */
	pattern = FcPatternCreate();

	/* Transcribe what we can get from the Pango font description. */
	_vte_fc_transcribe_from_pango_font_description(widget, pattern,
						       font_desc);

	/* Add any defaults specified in the configuration. */
	FcConfigSubstitute(NULL, pattern, FcMatchPattern);

	/* Add any defaults configured for GTK+. */
	_vte_fc_defaults_from_gtk(widget, pattern, antialias);

	/* Add defaults configured via the resource database. */
	_vte_fc_defaults_from_rdb(widget, pattern, antialias);

	/* Add any hard-coded default for antialiasing. */
	_vte_fc_set_antialias(pattern, antialias);

	/* Add any defaults which are hard-coded in fontconfig. */
	FcDefaultSubstitute(pattern);

	/* Add any defaults via a callback. */
	if (defaults_cb != NULL) {
		defaults_cb(pattern, defaults_data);
	}

	/* Get a sorted list of patterns, duplicate them, and append them
	 * to the passed-in array. */
	fontset = FcFontSort(NULL, pattern, FcTrue, NULL, &result);
	if (fontset != NULL) {
		for (i = 0; i < fontset->nfont; i++) {
			tmp = FcFontRenderPrepare(NULL,
						  pattern,
						  fontset->fonts[i]);
			_vte_fc_defaults_from_gtk(widget, tmp, antialias);
			_vte_fc_set_antialias(tmp, antialias);
			save = FcPatternDuplicate(tmp);
			FcPatternDestroy(tmp);
			g_ptr_array_add(pattern_array, save);
		}
		FcFontSetDestroy(fontset);
		ret = TRUE;
	}

	/* Last ditch effort. */
	if (pattern_array->len == 0) {
		match = FcFontMatch(NULL, pattern, &result);
		if (result == FcResultMatch) {
			tmp = FcPatternDuplicate(match);
			_vte_fc_defaults_from_gtk(widget, tmp, antialias);
			_vte_fc_set_antialias(tmp, antialias);
			save = FcPatternDuplicate(tmp);
			FcPatternDestroy(tmp);
			g_ptr_array_add(pattern_array, save);
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
	klass = G_OBJECT_GET_CLASS(settings);
	if (g_object_class_find_property(klass, "gtk-xft-antialias") == NULL) {
		return;
	}

	/* Start listening for changes to the fontconfig settings. */
	g_signal_connect(settings,
			 "notify::gtk-xft-antialias",
			 G_CALLBACK(changed_cb), widget);
	g_signal_connect(settings,
			 "notify::gtk-xft-hinting",
			 G_CALLBACK(changed_cb), widget);
	g_signal_connect(settings,
			 "notify::gtk-xft-hintstyle",
			 G_CALLBACK(changed_cb), widget);
	g_signal_connect(settings,
			 "notify::gtk-xft-rgba",
			 G_CALLBACK(changed_cb), widget);
	g_signal_connect(settings,
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
	g_signal_handlers_disconnect_matched(settings,
					     G_SIGNAL_MATCH_FUNC |
					     G_SIGNAL_MATCH_DATA,
					     0, 0, NULL,
					     changed_cb,
					     widget);
}
