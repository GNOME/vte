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

#include <limits.h>
#include <string.h>
#include <gtk/gtk.h>
#include "vterdb.h"

#define DEFAULT_ANTIALIAS	TRUE
#define DEFAULT_DPI		-1
#define DEFAULT_RGBA		"none"
#define DEFAULT_HINTING		TRUE
#define DEFAULT_HINTSTYLE	"hintfull"

static gchar **
_vte_rdb_get(void)
{
	GdkWindow *root;
	char *prop_data;
	gchar **ret;
	GdkAtom atom, prop_type;

	/* Retrieve the window and the property which we're going to read. */
	root = gdk_get_default_root_window();
	atom = gdk_atom_intern("RESOURCE_MANAGER", TRUE);
	if (atom == 0) {
		return NULL;
	}

	/* Read the string property off of the window. */
	prop_data = NULL;
	gdk_error_trap_push();
	gdk_property_get(root, atom, GDK_TARGET_STRING, 0, LONG_MAX, FALSE,
			 &prop_type, NULL, NULL,
			 (guchar**) &prop_data);
	gdk_error_trap_pop();

	/* Only parse the information if we got a string. */
	if ((prop_type == GDK_TARGET_STRING) && (prop_data != NULL)) {
		ret = g_strsplit(prop_data, "\n", -1);
		g_free(prop_data);
		return ret;
	}

	return NULL;
}

static gchar *
_vte_rdb_search(const char *setting)
{
	gchar *ret = NULL;
	int i, l;
	gchar **rdb;
	rdb = _vte_rdb_get();
	if (rdb != NULL) {
		l = strlen(setting);
		for (i = 0; rdb[i] != NULL; i++) {
			if ((strncmp(rdb[i], setting, l) == 0) &&
			    (rdb[i][l] == ':') &&
			    (rdb[i][l + 1] == '\t')) {
				ret = g_strdup(rdb[i] + l + 2);
				break;
			}
		}
		g_strfreev(rdb);
	}
	return ret;
}

static double
_vte_rdb_double(const char *setting, double default_value)
{
	char *start, *endptr = NULL;
	double dbl;
	start = _vte_rdb_search(setting);
	if (start == NULL) {
		return default_value;
	}
	dbl = g_ascii_strtod(start, &endptr);
	if ((endptr == NULL) || (*endptr != '\0')) {
		dbl = default_value;
	}
	g_free(start);
	return dbl;
}

static int
_vte_rdb_integer(const char *setting, int default_value)
{
	char *start, *endptr = NULL;
	int n;
	start = _vte_rdb_search(setting);
	if (start == NULL) {
		return default_value;
	}
	n = CLAMP(g_ascii_strtoull(start, &endptr, 10), 0, INT_MAX);
	if ((endptr == NULL) || (*endptr != '\0')) {
		n = default_value;
	}
	g_free(start);
	return n;
}

static gboolean
_vte_rdb_boolean(const char *setting, gboolean default_value)
{
	char *start, *endptr = NULL;
	int n;
	start = _vte_rdb_search(setting);
	if (start == NULL) {
		return default_value;
	}
	n = CLAMP(g_ascii_strtoull(start, &endptr, 10), 0, INT_MAX);
	if ((endptr != NULL) && (*endptr == '\0')) {
		n = (n != 0) ? TRUE : FALSE;
	} else
	if (g_ascii_strcasecmp(start, "true") == 0) {
		n = TRUE;
	} else
	if (g_ascii_strcasecmp(start, "false") == 0) {
		n = FALSE;
	} else {
		n = default_value;
	}
	g_free(start);
	return n;
}

static GQuark
_vte_rdb_quark(const char *setting, GQuark default_value)
{
	char *start;
	GQuark q = 0;
	start = _vte_rdb_search(setting);
	if (start == NULL) {
		return default_value;
	}
	q = g_quark_from_string(start);
	g_free(start);
	return q;
}

double
_vte_rdb_get_dpi(void)
{
	return _vte_rdb_double("Xft.dpi", DEFAULT_DPI);
}

gboolean
_vte_rdb_get_antialias(void)
{
	return _vte_rdb_boolean("Xft.antialias", DEFAULT_ANTIALIAS);
}

gboolean
_vte_rdb_get_hinting(void)
{
	return _vte_rdb_boolean("Xft.hinting", DEFAULT_HINTING);
}

const char *
_vte_rdb_get_rgba(void)
{
	GQuark q;
	q = g_quark_from_string(DEFAULT_RGBA);
	return g_quark_to_string(_vte_rdb_quark("Xft.rgba", q));
}

const char *
_vte_rdb_get_hintstyle(void)
{
	GQuark q;
	q = g_quark_from_string(DEFAULT_HINTSTYLE);
	return g_quark_to_string(_vte_rdb_quark("Xft.hintstyle", q));
}

#ifdef VTERDB_MAIN
int
main(int argc, char **argv)
{
	gtk_init(&argc, &argv);
	g_print("DPI: %lf\n", _vte_rdb_get_dpi());
	g_print("Antialias: %s\n", _vte_rdb_get_antialias() ? "TRUE" : "FALSE");
	g_print("Hinting: %s\n", _vte_rdb_get_hinting() ? "TRUE" : "FALSE");
	g_print("Hint style: %s\n", _vte_rdb_get_hintstyle());
	g_print("RGBA: %s\n", _vte_rdb_get_rgba());
	return 0;
}
#endif
