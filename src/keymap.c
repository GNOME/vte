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

#include <config.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "caps.h"
#include "debug.h"
#include "keymap.h"
#include "vtetc.h"

#if GTK_CHECK_VERSION (2, 90, 7)
#define GDK_KEY(symbol) GDK_KEY_##symbol
#else
#include <gdk/gdkkeysyms.h>
#define GDK_KEY(symbol) GDK_##symbol
#endif

#if defined(HAVE_NCURSES_H) && defined(HAVE_TERM_H)
#include <ncurses.h>
#include <term.h>
#define VTE_TERMCAP_NAME "ncurses"
#elif defined(HAVE_NCURSES_CURSES_H) && defined(HAVE_NCURSES_TERM_H)
#include <ncurses/curses.h>
#include <ncurses/term.h>
#define VTE_TERMCAP_NAME "ncurses"
#elif defined(HAVE_CURSES_H) && defined(HAVE_TERM_H)
#include <curses.h>
#include <term.h>
#define VTE_TERMCAP_NAME "curses"
#elif defined(HAVE_TERMCAP_H)
#include <termcap.h>
#define VTE_TERMCAP_NAME "termcap"
#else
#error No termcap??
#endif

#ifdef VTE_DEBUG
static const char *
_vte_keysym_name(guint keyval)
{
	switch (keyval) {
#include "keysyms.c"
		default:
			break;
	}
	return "(unknown)";
}
static void
_vte_keysym_print(guint keyval,
		GdkModifierType modifiers,
		gboolean sun_mode,
		gboolean hp_mode,
		gboolean legacy_mode,
		gboolean vt220_mode)
{
	g_printerr("Mapping ");
	if (modifiers & GDK_CONTROL_MASK) {
		g_printerr("Control+");
	}
	if (modifiers & VTE_META_MASK) {
		g_printerr("Meta+");
	}
	if (modifiers & VTE_NUMLOCK_MASK) {
		g_printerr("NumLock+");
	}
	if (modifiers & GDK_SHIFT_MASK) {
		g_printerr("Shift+");
	}
	g_printerr("%s" , _vte_keysym_name(keyval));
	if (sun_mode|hp_mode|legacy_mode|vt220_mode) {
		gboolean first = TRUE;
		g_printerr("(");
		if (sun_mode) {
			if (!first) {
				g_printerr(",");
			}
			first = FALSE;
			g_printerr("Sun");
		}
		if (hp_mode) {
			if (!first) {
				g_printerr(",");
			}
			first = FALSE;
			g_printerr("HP");
		}
		if (legacy_mode) {
			if (!first) {
				g_printerr(",");
			}
			first = FALSE;
			g_printerr("Legacy");
		}
		if (vt220_mode) {
			if (!first) {
				g_printerr(",");
			}
			first = FALSE;
			g_printerr("VT220");
		}
		g_printerr(")");
	}
}
#else
static void
_vte_keysym_print(guint keyval,
		GdkModifierType modifiers,
		gboolean sun_mode,
		gboolean hp_mode,
		gboolean legacy_mode,
		gboolean vt220_mode)
{
}
#endif

enum _vte_cursor_mode {
	cursor_default =	1 << 0,
	cursor_app =		1 << 1
};

enum _vte_keypad_mode {
	keypad_default =	1 << 0,
	keypad_app =		1 << 1
};

enum _vte_fkey_mode {
	fkey_default =	1 << 0,
	fkey_sun =	1 << 1,
	fkey_hp =	1 << 2,
	fkey_legacy =	1 << 3,
	fkey_vt220 =	1 << 4
};

#define cursor_all	(cursor_default | cursor_app)
#define keypad_all	(keypad_default | keypad_app)
#define fkey_all	(fkey_default | fkey_sun | fkey_hp | fkey_legacy | fkey_vt220)
#define fkey_notvt220	(fkey_default | fkey_sun | fkey_hp | fkey_legacy)
#define fkey_notsun	(fkey_default | fkey_hp | fkey_legacy | fkey_vt220)
#define fkey_nothp	(fkey_default | fkey_sun | fkey_legacy | fkey_vt220)
#define fkey_notsunvt	(fkey_default | fkey_hp | fkey_legacy)
#define fkey_notsunhp	(fkey_default | fkey_legacy | fkey_vt220)
#define fkey_nothpvt	(fkey_default | fkey_sun | fkey_legacy)

struct _vte_keymap_entry {
	enum _vte_cursor_mode cursor_mode;
	enum _vte_keypad_mode keypad_mode;
	enum _vte_fkey_mode fkey_mode;
	GdkModifierType mod_mask;
	const char normal[8];
	gssize normal_length;
	const char special[4];
};

#define X_NULL ""

/* Normal keys unaffected by modes. */
static const struct _vte_keymap_entry _vte_keymap_GDK_space[] = {
	/* Meta+space = ESC+" " */
	{cursor_all, keypad_all, fkey_all,
	 VTE_META_MASK, _VTE_CAP_ESC " ", 2, X_NULL},
	/* Control+space = NUL */
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\0", 1, X_NULL},
	/* Regular space. */
	{cursor_all, keypad_all, fkey_all, 0, " ", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Tab[] = {
	/* Shift+Tab = Back-Tab */
	{cursor_all, keypad_all, fkey_all,
	 GDK_SHIFT_MASK, X_NULL, 0, "kB"},
	{cursor_all, keypad_all, fkey_all,
	 GDK_SHIFT_MASK, _VTE_CAP_CSI "Z", -1, X_NULL},
	/* Alt+Tab = Esc+Tab */
	{cursor_all, keypad_all, fkey_all,
	 VTE_META_MASK, _VTE_CAP_ESC "\t", -1, X_NULL},
	/* Regular tab. */
	{cursor_all, keypad_all, fkey_all,
	 0, X_NULL, 0, "ta"},
	{cursor_all, keypad_all, fkey_all, 0, "\t", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Return[] = {
	{cursor_all, keypad_all, fkey_all,
	 VTE_META_MASK, _VTE_CAP_ESC "\n", 2, X_NULL},
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\n", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, "\r", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Escape[] = {
	{cursor_all, keypad_all, fkey_all,
	 VTE_META_MASK, _VTE_CAP_ESC _VTE_CAP_ESC, 2, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, _VTE_CAP_ESC, 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Insert[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "kI"},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "Q", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "2z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_notsunhp, 0, _VTE_CAP_CSI "2~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_ISO_Left_Tab[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "kB"},
	{cursor_all, keypad_all, fkey_all, 0, _VTE_CAP_CSI "Z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_slash[] = {
	{cursor_all, keypad_all, fkey_all,
	 VTE_META_MASK, _VTE_CAP_ESC "/", 2, X_NULL},
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\037", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, "/", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_question[] = {
	{cursor_all, keypad_all, fkey_all,
	 VTE_META_MASK, _VTE_CAP_ESC "?", 2, X_NULL},
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\177", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, "?", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

/* Various numeric keys enter control characters. */
static const struct _vte_keymap_entry _vte_keymap_GDK_2[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\0", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_3[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\033", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_4[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\034", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_5[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\035", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_6[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\036", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_7[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\037", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_8[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\177", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};
static const struct _vte_keymap_entry _vte_keymap_GDK_Minus[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\037", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

/* Home and End are strange cases because their sequences vary wildly from
 * system to system, or mine's just broken.  But anyway. */
static const struct _vte_keymap_entry _vte_keymap_GDK_Home[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "kh"},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_CSI "1~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "h", -1, X_NULL},
	{cursor_all, keypad_all, fkey_nothpvt, 0, X_NULL, 0, "kh"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_End[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "@7"},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_CSI "4~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "@7"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Page_Up[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "kP"},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "T", -1, X_NULL},
	{cursor_all, keypad_all, fkey_notsunhp, 0, _VTE_CAP_CSI "5~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "5z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Page_Down[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "kN"},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "S", -1, X_NULL},
	{cursor_all, keypad_all, fkey_notsunhp, 0, _VTE_CAP_CSI "6~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "6z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

/* Keys affected by the cursor key mode. */
static const struct _vte_keymap_entry _vte_keymap_GDK_Up[] = {
	{cursor_default, keypad_all, fkey_all, 0, X_NULL, 0, "ku"},
	{cursor_default, keypad_all, fkey_nothp, 0, _VTE_CAP_CSI "A", -1, X_NULL},
	{cursor_default, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "A", -1, X_NULL},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "A", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Down[] = {
	{cursor_default, keypad_all, fkey_all, 0, X_NULL, 0, "kd"},
	{cursor_default, keypad_all, fkey_nothp, 0, _VTE_CAP_CSI "B", -1, X_NULL},
	{cursor_default, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "B", -1, X_NULL},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "B", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Right[] = {
	{cursor_default, keypad_all, fkey_all, 0, X_NULL, 0, "kr"},
	{cursor_default, keypad_all, fkey_nothp, 0, _VTE_CAP_CSI "C", -1, X_NULL},
	{cursor_default, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "C", -1, X_NULL},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "C", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_Left[] = {
	{cursor_default, keypad_all, fkey_all, 0, X_NULL, 0, "kl"},
	{cursor_default, keypad_all, fkey_nothp, 0, _VTE_CAP_CSI "D", -1, X_NULL},
	{cursor_default, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "D", -1, X_NULL},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "D", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

/* Keys (potentially) affected by the keypad key mode. */
static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Space[] = {
	{cursor_all, keypad_default, fkey_all, 0, " ", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 " ", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Tab[] = {
	{cursor_all, keypad_default, fkey_all, 0, "\t", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "I", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Enter[] = {
	{cursor_all, keypad_default, fkey_all, 0, X_NULL, 0, "@8"},
	{cursor_all, keypad_app, fkey_all, VTE_NUMLOCK_MASK | GDK_CONTROL_MASK, "\n", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, VTE_NUMLOCK_MASK, "\r", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "M", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "\n", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, "\r", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_F1[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "k1"},
	{cursor_all, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "P", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_F2[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "k2"},
	{cursor_all, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "Q", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_F3[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "k3"},
	{cursor_all, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "R", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_F4[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "k4"},
	{cursor_all, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "S", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Multiply[] = {
	{cursor_all, keypad_default, fkey_all, 0, "*", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, VTE_NUMLOCK_MASK, "*", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "j", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Add[] = {
	{cursor_all, keypad_default, fkey_notvt220, 0, "+", 1, X_NULL},
	{cursor_all, keypad_default, fkey_vt220, 0, ",", 1, X_NULL},
	{cursor_all, keypad_app, fkey_notvt220, VTE_NUMLOCK_MASK, "+", 1, X_NULL},
	{cursor_all, keypad_app, fkey_vt220, VTE_NUMLOCK_MASK, ",", 1, X_NULL},
	{cursor_all, keypad_app, fkey_notvt220, 0, _VTE_CAP_SS3 "k", -1, X_NULL},
	{cursor_all, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "l", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Separator[] = {
	{cursor_all, keypad_default, fkey_all, 0, ",", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "l", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Subtract[] = {
	{cursor_all, keypad_default, fkey_all, 0, "-", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, VTE_NUMLOCK_MASK, "-", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "m", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Decimal_Delete[] = {
	{cursor_all, keypad_default, fkey_default, 0, ".", 1, X_NULL},
	{cursor_all, keypad_app, fkey_notsun, 0, _VTE_CAP_SS3 "3~", -1, X_NULL},
	{cursor_all, keypad_app, fkey_sun, 0, _VTE_CAP_SS3 "3~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Divide[] = {
	{cursor_all, keypad_default, fkey_all, 0, "/", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, VTE_NUMLOCK_MASK, "/", 1, X_NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "o", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

/* GDK already separates keypad "0" from keypad "Insert", so the only time
 * we'll see this key is when NumLock is on, and that means that we're in
 * "default" mode. */
static const struct _vte_keymap_entry _vte_keymap_GDK_KP_0[] = {
	{cursor_all, keypad_all, fkey_all, 0, "0", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_1[] = {
	{cursor_all, keypad_all, fkey_all, 0, "1", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_2[] = {
	{cursor_all, keypad_all, fkey_all, 0, "2", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_3[] = {
	{cursor_all, keypad_all, fkey_all, 0, "3", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_4[] = {
	{cursor_all, keypad_all, fkey_all, 0, "4", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_5[] = {
	{cursor_all, keypad_all, fkey_all, 0, "5", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_6[] = {
	{cursor_all, keypad_all, fkey_all, 0, "6", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_7[] = {
	{cursor_all, keypad_all, fkey_all, 0, "7", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_8[] = {
	{cursor_all, keypad_all, fkey_all, 0, "8", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_9[] = {
	{cursor_all, keypad_all, fkey_all, 0, "9", 1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

/* These are the same keys as above, but without numlock.  If there's a
 * capability associated with the key, then we send that, unless we're in
 * application mode. */
static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Insert[] = {
	{cursor_all, keypad_default, fkey_notsunvt,
	 0, _VTE_CAP_CSI "2~", -1, X_NULL},
	{cursor_all, keypad_default, fkey_sun, 0, _VTE_CAP_CSI "2z", -1, X_NULL},
	{cursor_all, keypad_default, fkey_vt220, 0, "0", 1, X_NULL},
	{cursor_all, keypad_app, fkey_notvt220, 0, _VTE_CAP_CSI "2~", -1, X_NULL},
	{cursor_all, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "p", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_End[] = {
	{cursor_all, keypad_default, fkey_all, 0, X_NULL, 0, "K4"},
	{cursor_all, keypad_default, fkey_notvt220,
	 0, _VTE_CAP_CSI "4~", -1, X_NULL},
	{cursor_all, keypad_default, fkey_vt220, 0, "1", 1, X_NULL},
	{cursor_all, keypad_app, fkey_notvt220, 0, _VTE_CAP_CSI "4~", -1, X_NULL},
	{cursor_all, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "q", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Down[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "kd"},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "B", -1, X_NULL},
	{cursor_default, keypad_all, fkey_notvt220,
	 0, _VTE_CAP_CSI "B", -1, X_NULL},
	{cursor_default, keypad_default, fkey_vt220, 0, "2", 1, X_NULL},
	{cursor_default, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "r", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Page_Down[] = {
	{cursor_all, keypad_default, fkey_all, 0, X_NULL, 0, "K5"},
	{cursor_all, keypad_default, fkey_notsunvt,
	 0, _VTE_CAP_CSI "6~", -1, X_NULL},
	{cursor_all, keypad_default, fkey_sun, 0, _VTE_CAP_CSI "6z", -1, X_NULL},
	{cursor_all, keypad_default, fkey_vt220, 0, "3", 1, X_NULL},
	{cursor_all, keypad_app, fkey_notvt220, 0, _VTE_CAP_CSI "6~", -1, X_NULL},
	{cursor_all, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "s", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Left[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "kl"},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "D", -1, X_NULL},
	{cursor_default, keypad_all, fkey_notvt220,
	 0, _VTE_CAP_CSI "D", -1, X_NULL},
	{cursor_default, keypad_default, fkey_vt220, 0, "4", 1, X_NULL},
	{cursor_default, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "t", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Begin[] = {
	{cursor_all, keypad_default, fkey_all, 0, X_NULL, 0, "K2"},
	{cursor_all, keypad_default, fkey_notvt220,
	 0, _VTE_CAP_CSI "E", -1, X_NULL},
	{cursor_all, keypad_default, fkey_vt220, 0, "5", 1, X_NULL},
	{cursor_all, keypad_app, fkey_notvt220, 0, _VTE_CAP_CSI "E", -1, X_NULL},
	{cursor_all, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "u", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Right[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "kr"},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "C", -1, X_NULL},
	{cursor_default, keypad_all, fkey_notvt220,
	 0, _VTE_CAP_CSI "C", -1, X_NULL},
	{cursor_default, keypad_default, fkey_vt220, 0, "6", 1, X_NULL},
	{cursor_default, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "v", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Home[] = {
	{cursor_all, keypad_default, fkey_all, 0, X_NULL, 0, "K1"},
	{cursor_all, keypad_default, fkey_notvt220,
	 0, _VTE_CAP_CSI "1~", -1, X_NULL},
	{cursor_all, keypad_default, fkey_vt220, 0, "7", 1, X_NULL},
	{cursor_all, keypad_app, fkey_notvt220, 0, _VTE_CAP_CSI "1~", -1, X_NULL},
	{cursor_all, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "w", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Up[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "ku"},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "A", -1, X_NULL},
	{cursor_default, keypad_all, fkey_notvt220,
	 0, _VTE_CAP_CSI "A", -1, X_NULL},
	{cursor_default, keypad_default, fkey_vt220, 0, "8", 1, X_NULL},
	{cursor_default, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "x", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_KP_Page_Up[] = {
	{cursor_all, keypad_default, fkey_all, 0, X_NULL, 0, "K3"},
	{cursor_all, keypad_default, fkey_notvt220,
	 0, _VTE_CAP_CSI "5~", -1, X_NULL},
	{cursor_all, keypad_default, fkey_vt220, 0, "9", 1, X_NULL},
	{cursor_all, keypad_app, fkey_notvt220, 0, _VTE_CAP_CSI "5~", -1, X_NULL},
	{cursor_all, keypad_app, fkey_vt220, 0, _VTE_CAP_SS3 "y", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};


/* Keys affected by the function key mode. */
static const struct _vte_keymap_entry _vte_keymap_GDK_F1[] = {
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "k1"},
	{cursor_all, keypad_all, fkey_vt220, GDK_CONTROL_MASK, X_NULL, 0, "F3"},
	{cursor_all, keypad_all, fkey_vt220, 0, X_NULL, 0, "k1"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_SS3 "P", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "224z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "p", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "11~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220,
	 GDK_CONTROL_MASK, _VTE_CAP_CSI "23~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_SS3 "P", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F2[] = {
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "k2"},
	{cursor_all, keypad_all, fkey_vt220, GDK_CONTROL_MASK, X_NULL, 0, "F4"},
	{cursor_all, keypad_all, fkey_vt220, 0, X_NULL, 0, "k2"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_SS3 "Q", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "225z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "q", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "12~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220,
	 GDK_CONTROL_MASK, _VTE_CAP_CSI "24~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_SS3 "Q", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F3[] = {
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "k3"},
	{cursor_all, keypad_all, fkey_vt220, GDK_CONTROL_MASK, X_NULL, 0, "F5"},
	{cursor_all, keypad_all, fkey_vt220, 0, X_NULL, 0, "k3"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_SS3 "R", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "226z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "r", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "13~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220,
	 GDK_CONTROL_MASK, _VTE_CAP_CSI "25~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_SS3 "R", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F4[] = {
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "k4"},
	{cursor_all, keypad_all, fkey_vt220, GDK_CONTROL_MASK, X_NULL, 0, "F6"},
	{cursor_all, keypad_all, fkey_vt220, 0, X_NULL, 0, "k4"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_SS3 "S", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "227z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "s", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "14~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220,
	 GDK_CONTROL_MASK, _VTE_CAP_CSI "26~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_SS3 "S", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F5[] = {
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "k5"},
	{cursor_all, keypad_all, fkey_vt220, GDK_CONTROL_MASK, X_NULL, 0, "F7"},
	{cursor_all, keypad_all, fkey_vt220, 0, X_NULL, 0, "k5"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_CSI "15~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "228z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "t", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "15~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220,
	 GDK_CONTROL_MASK, _VTE_CAP_CSI "28~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_CSI "15~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F6[] = {
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "k6"},
	{cursor_all, keypad_all, fkey_vt220, GDK_CONTROL_MASK, X_NULL, 0, "F8"},
	{cursor_all, keypad_all, fkey_vt220, 0, X_NULL, 0, "k6"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_CSI "17~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "229z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "u", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "17~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220,
	 GDK_CONTROL_MASK, _VTE_CAP_CSI "29~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_CSI "17~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F7[] = {
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "k7"},
	{cursor_all, keypad_all, fkey_vt220, GDK_CONTROL_MASK, X_NULL, 0, "F9"},
	{cursor_all, keypad_all, fkey_vt220, 0, X_NULL, 0, "k7"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_CSI "18~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "230z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "v", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "18~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220,
	 GDK_CONTROL_MASK, _VTE_CAP_CSI "31~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_CSI "18~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F8[] = {
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "k8"},
	{cursor_all, keypad_all, fkey_vt220, GDK_CONTROL_MASK, X_NULL, 0, "FA"},
	{cursor_all, keypad_all, fkey_vt220, 0, X_NULL, 0, "k8"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_CSI "19~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "231z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_ESC "w", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "19~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220,
	 GDK_CONTROL_MASK, _VTE_CAP_CSI "32~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_CSI "19~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F9[] = {
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "k9"},
	{cursor_all, keypad_all, fkey_vt220, GDK_CONTROL_MASK, X_NULL, 0, "FB"},
	{cursor_all, keypad_all, fkey_vt220, 0, X_NULL, 0, "k9"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_CSI "20~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "232z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_CSI "20~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "20~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220,
	 GDK_CONTROL_MASK, _VTE_CAP_CSI "33~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_CSI "20~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F10[] = {
	{cursor_all, keypad_all, fkey_notvt220, 0, X_NULL, 0, "k;"},
	{cursor_all, keypad_all, fkey_vt220, GDK_CONTROL_MASK, X_NULL, 0, "FC"},
	{cursor_all, keypad_all, fkey_vt220, 0, X_NULL, 0, "k;"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_CSI "21~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "233z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_CSI "21~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "21~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220,
	 GDK_CONTROL_MASK, _VTE_CAP_CSI "34~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_CSI "21~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F11[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "F1"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_CSI "23~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "192z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_CSI "23~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "23~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_CSI "23~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F12[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "F2"},
	{cursor_all, keypad_all, fkey_default, 0, _VTE_CAP_CSI "24~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "193z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_hp, 0, _VTE_CAP_CSI "24~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "24~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_vt220, 0, _VTE_CAP_CSI "24~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F13[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "F3"},
	{cursor_all, keypad_all, fkey_notsun, 0, _VTE_CAP_CSI "25~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "194z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F14[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "F4"},
	{cursor_all, keypad_all, fkey_notsun, 0, _VTE_CAP_CSI "26~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "195z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F15[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "F5"},
	{cursor_all, keypad_all, fkey_notsun, 0, _VTE_CAP_CSI "28~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "196z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F16[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "F6"},
	{cursor_all, keypad_all, fkey_notsun, 0, _VTE_CAP_CSI "29~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "197z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F17[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "F7"},
	{cursor_all, keypad_all, fkey_notsun, 0, _VTE_CAP_CSI "31~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "198z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F18[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "F8"},
	{cursor_all, keypad_all, fkey_notsun, 0, _VTE_CAP_CSI "32~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "199z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F19[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "F9"},
	{cursor_all, keypad_all, fkey_notsun, 0, _VTE_CAP_CSI "33~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "200z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F20[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FA"},
	{cursor_all, keypad_all, fkey_notsun, 0, _VTE_CAP_CSI "34~", -1, X_NULL},
	{cursor_all, keypad_all, fkey_sun, 0, _VTE_CAP_CSI "201z", -1, X_NULL},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F21[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FB"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F22[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FC"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F23[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FD"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F24[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FE"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F25[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FF"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F26[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FG"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F27[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FH"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F28[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FI"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F29[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FJ"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F30[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FK"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F31[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FL"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F32[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FM"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F33[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FN"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F34[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FO"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_entry _vte_keymap_GDK_F35[] = {
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, "FP"},
	{cursor_all, keypad_all, fkey_all, 0, X_NULL, 0, X_NULL},
};

static const struct _vte_keymap_group {
	guint keyval;
	const struct _vte_keymap_entry *entries;
} _vte_keymap[] = {
	{GDK_KEY (space),		_vte_keymap_GDK_space},
	{GDK_KEY (Return),		_vte_keymap_GDK_Return},
	{GDK_KEY (Escape),		_vte_keymap_GDK_Escape},
	{GDK_KEY (Tab),		        _vte_keymap_GDK_Tab},
	{GDK_KEY (ISO_Left_Tab),	_vte_keymap_GDK_ISO_Left_Tab},
	{GDK_KEY (Home),		_vte_keymap_GDK_Home},
	{GDK_KEY (End),		        _vte_keymap_GDK_End},
	{GDK_KEY (Insert),		_vte_keymap_GDK_Insert},
	{GDK_KEY (slash),		_vte_keymap_GDK_slash},
	{GDK_KEY (question),		_vte_keymap_GDK_question},
	/* GDK_KEY (Delete is all handled in code), due to funkiness. */
	{GDK_KEY (Page_Up),		_vte_keymap_GDK_Page_Up},
	{GDK_KEY (Page_Down),		_vte_keymap_GDK_Page_Down},

	{GDK_KEY (2),			_vte_keymap_GDK_2},
	{GDK_KEY (3),			_vte_keymap_GDK_3},
	{GDK_KEY (4),			_vte_keymap_GDK_4},
	{GDK_KEY (5),			_vte_keymap_GDK_5},
	{GDK_KEY (6),			_vte_keymap_GDK_6},
	{GDK_KEY (7),			_vte_keymap_GDK_7},
	{GDK_KEY (8),			_vte_keymap_GDK_8},
	{GDK_KEY (minus),		_vte_keymap_GDK_Minus},

	{GDK_KEY (Up),		_vte_keymap_GDK_Up},
	{GDK_KEY (Down),		_vte_keymap_GDK_Down},
	{GDK_KEY (Right),		_vte_keymap_GDK_Right},
	{GDK_KEY (Left),		_vte_keymap_GDK_Left},

	{GDK_KEY (KP_Space),		_vte_keymap_GDK_KP_Space},
	{GDK_KEY (KP_Tab),		_vte_keymap_GDK_KP_Tab},
	{GDK_KEY (KP_Enter),		_vte_keymap_GDK_KP_Enter},
	{GDK_KEY (KP_F1),		_vte_keymap_GDK_KP_F1},
	{GDK_KEY (KP_F2),		_vte_keymap_GDK_KP_F2},
	{GDK_KEY (KP_F3),		_vte_keymap_GDK_KP_F3},
	{GDK_KEY (KP_F4),		_vte_keymap_GDK_KP_F4},
	{GDK_KEY (KP_Multiply),	_vte_keymap_GDK_KP_Multiply},
	{GDK_KEY (KP_Add),		_vte_keymap_GDK_KP_Add},
	{GDK_KEY (KP_Separator),	_vte_keymap_GDK_KP_Separator},
	{GDK_KEY (KP_Subtract),	_vte_keymap_GDK_KP_Subtract},
	{GDK_KEY (KP_Decimal),	_vte_keymap_GDK_KP_Decimal_Delete},
	{GDK_KEY (KP_Delete),		_vte_keymap_GDK_KP_Decimal_Delete},
	{GDK_KEY (KP_Divide),		_vte_keymap_GDK_KP_Divide},
	{GDK_KEY (KP_0),		_vte_keymap_GDK_KP_0},
	{GDK_KEY (KP_Insert),		_vte_keymap_GDK_KP_Insert},
	{GDK_KEY (KP_1),		_vte_keymap_GDK_KP_1},
	{GDK_KEY (KP_End),		_vte_keymap_GDK_KP_End},
	{GDK_KEY (KP_2),		_vte_keymap_GDK_KP_2},
	{GDK_KEY (KP_Down),		_vte_keymap_GDK_KP_Down},
	{GDK_KEY (KP_3),		_vte_keymap_GDK_KP_3},
	{GDK_KEY (KP_Page_Down),	_vte_keymap_GDK_KP_Page_Down},
	{GDK_KEY (KP_4),		_vte_keymap_GDK_KP_4},
	{GDK_KEY (KP_Left),		_vte_keymap_GDK_KP_Left},
	{GDK_KEY (KP_5),		_vte_keymap_GDK_KP_5},
	{GDK_KEY (KP_Begin),		_vte_keymap_GDK_KP_Begin},
	{GDK_KEY (KP_6),		_vte_keymap_GDK_KP_6},
	{GDK_KEY (KP_Right),		_vte_keymap_GDK_KP_Right},
	{GDK_KEY (KP_7),		_vte_keymap_GDK_KP_7},
	{GDK_KEY (KP_Home),		_vte_keymap_GDK_KP_Home},
	{GDK_KEY (KP_8),		_vte_keymap_GDK_KP_8},
	{GDK_KEY (KP_Up),		_vte_keymap_GDK_KP_Up},
	{GDK_KEY (KP_9),		_vte_keymap_GDK_KP_9},
	{GDK_KEY (KP_Page_Up),	_vte_keymap_GDK_KP_Page_Up},

	{GDK_KEY (F1),		_vte_keymap_GDK_F1},
	{GDK_KEY (F2),		_vte_keymap_GDK_F2},
	{GDK_KEY (F3),		_vte_keymap_GDK_F3},
	{GDK_KEY (F4),		_vte_keymap_GDK_F4},
	{GDK_KEY (F5),		_vte_keymap_GDK_F5},
	{GDK_KEY (F6),		_vte_keymap_GDK_F6},
	{GDK_KEY (F7),		_vte_keymap_GDK_F7},
	{GDK_KEY (F8),		_vte_keymap_GDK_F8},
	{GDK_KEY (F9),		_vte_keymap_GDK_F9},
	{GDK_KEY (F10),		_vte_keymap_GDK_F10},
	{GDK_KEY (F11),		_vte_keymap_GDK_F11},
	{GDK_KEY (F12),		_vte_keymap_GDK_F12},
	{GDK_KEY (F13),		_vte_keymap_GDK_F13},
	{GDK_KEY (F14),		_vte_keymap_GDK_F14},
	{GDK_KEY (F15),		_vte_keymap_GDK_F15},
	{GDK_KEY (F16),		_vte_keymap_GDK_F16},
	{GDK_KEY (F17),		_vte_keymap_GDK_F17},
	{GDK_KEY (F18),		_vte_keymap_GDK_F18},
	{GDK_KEY (F19),		_vte_keymap_GDK_F19},
	{GDK_KEY (F20),		_vte_keymap_GDK_F20},
	{GDK_KEY (F21),		_vte_keymap_GDK_F21},
	{GDK_KEY (F22),		_vte_keymap_GDK_F22},
	{GDK_KEY (F23),		_vte_keymap_GDK_F23},
	{GDK_KEY (F24),		_vte_keymap_GDK_F24},
	{GDK_KEY (F25),		_vte_keymap_GDK_F25},
	{GDK_KEY (F26),		_vte_keymap_GDK_F26},
	{GDK_KEY (F27),		_vte_keymap_GDK_F27},
	{GDK_KEY (F28),		_vte_keymap_GDK_F28},
	{GDK_KEY (F29),		_vte_keymap_GDK_F29},
	{GDK_KEY (F30),		_vte_keymap_GDK_F30},
	{GDK_KEY (F31),		_vte_keymap_GDK_F31},
	{GDK_KEY (F32),		_vte_keymap_GDK_F32},
	{GDK_KEY (F33),		_vte_keymap_GDK_F33},
	{GDK_KEY (F34),		_vte_keymap_GDK_F34},
	{GDK_KEY (F35),		_vte_keymap_GDK_F35},
};

/* Map the specified keyval/modifier setup, dependent on the mode, to either
 * a literal string or a capability name. */
void
_vte_keymap_map(guint keyval,
		GdkModifierType modifiers,
		gboolean sun_mode,
		gboolean hp_mode,
		gboolean legacy_mode,
		gboolean vt220_mode,
		gboolean app_cursor_keys,
		gboolean app_keypad_keys,
		struct _vte_termcap *termcap,
		const char *terminal,
		char **normal,
		gssize *normal_length,
		const char **special)
{
	gsize i;
	const struct _vte_keymap_entry *entries;
	enum _vte_cursor_mode cursor_mode;
	enum _vte_keypad_mode keypad_mode;
	enum _vte_fkey_mode fkey_mode;
	char *cap, *tmp;
	const char *termcap_special = NULL;
	char ncurses_buffer[4096];
	char ncurses_area[512];

	g_return_if_fail(normal != NULL);
	g_return_if_fail(normal_length != NULL);
	g_return_if_fail(special != NULL);

	_VTE_DEBUG_IF(VTE_DEBUG_KEYBOARD) 
		_vte_keysym_print(keyval, modifiers,
				sun_mode,
				hp_mode,
				legacy_mode,
				vt220_mode);

	/* Start from scratch. */
	*normal = NULL;
	*special = NULL;
	*normal_length = 0;

	/* Search for the list for this key. */
	entries = NULL;
	for (i = 0; i < G_N_ELEMENTS(_vte_keymap); i++) {
#ifdef VTE_DEBUG
		int j;
		GdkModifierType mods;
		/* Check for NULL strings with non-zero length, and
		 * vice-versa. */
		entries = _vte_keymap[i].entries;
		for (j = 0; entries[j].normal_length || entries[j].special[0]; j++) {
			if (entries[j].normal_length) {
				g_assert(!entries[j].special[0]);
			} else {
				g_assert(!entries[j].normal[0]);
			}
		}
		/* Check for coverage. This check is not exhaustive. */
		fkey_mode = 0;
		mods = GDK_SHIFT_MASK | GDK_CONTROL_MASK | VTE_META_MASK | VTE_NUMLOCK_MASK;
		for (j = 0; entries[j].normal_length || entries[j].special[0]; j++) {
			if (entries[j].fkey_mode != fkey_all) {
				fkey_mode |= entries[j].fkey_mode;
			}
			mods &= entries[j].mod_mask;
		}
		switch (_vte_keymap[i].keyval) {
		case GDK_KEY (2):
		case GDK_KEY (3):
		case GDK_KEY (4):
		case GDK_KEY (5):
		case GDK_KEY (6):
		case GDK_KEY (7):
		case GDK_KEY (8):
			/* Known non-full-coverage cases. */
			break;
		default:
			/* Everything else we double-check. */
			g_assert((fkey_mode == 0) || (fkey_mode == fkey_all));
			break;
		}
		entries = NULL;
#endif
		if (_vte_keymap[i].keyval == keyval) {
			/* Found it! */
			entries = _vte_keymap[i].entries;
			break;
		}
	}
	if (entries == NULL) {
		_vte_debug_print(VTE_DEBUG_KEYBOARD,
				" (ignoring, no map for key).\n");
		return;
	}

	/* Build mode masks. */
	cursor_mode = app_cursor_keys ? cursor_app : cursor_default;
	keypad_mode = app_keypad_keys ? keypad_app : keypad_default;
	if (sun_mode) {
		fkey_mode = fkey_sun;
	} else
	if (hp_mode) {
		fkey_mode = fkey_hp;
	} else
	if (legacy_mode) {
		fkey_mode = fkey_legacy;
	} else
	if (vt220_mode) {
		fkey_mode = fkey_vt220;
	} else {
		fkey_mode = fkey_default;
	}
	modifiers &= (GDK_SHIFT_MASK | GDK_CONTROL_MASK | VTE_META_MASK | VTE_NUMLOCK_MASK);

	/* Search for the conditions. */
	for (i = 0; entries[i].normal_length || entries[i].special[0]; i++)
	if ((entries[i].cursor_mode & cursor_mode) &&
	    (entries[i].keypad_mode & keypad_mode) &&
	    (entries[i].fkey_mode & fkey_mode))
	if ((modifiers & entries[i].mod_mask) == entries[i].mod_mask) {
		if (entries[i].normal_length) {
			if (entries[i].normal_length != -1) {
				*normal_length = entries[i].normal_length;
				*normal = g_memdup(entries[i].normal,
						   entries[i].normal_length);
			} else {
				*normal_length = strlen(entries[i].normal);
				*normal = g_strdup(entries[i].normal);
			}
			_vte_keymap_key_add_key_modifiers(keyval,
							  modifiers,
							  sun_mode,
							  hp_mode,
							  legacy_mode,
							  vt220_mode,
							  cursor_mode & cursor_app,
							  normal,
							  normal_length);
			_VTE_DEBUG_IF(VTE_DEBUG_KEYBOARD) {
				int j;
				g_printerr(" to '");
				for (j = 0; j < *normal_length; j++) {
					if (((*normal)[j] < 32) ||
					    ((*normal)[j] >= 127)) {
						g_printerr("<0x%02x>",
							(*normal)[j]);
					} else {
						g_printerr("%c",
							(*normal)[j]);
					}
				}
				g_printerr("'.\n");
			}
			return;
		} else {
			termcap_special = entries[i].special;
			cap = _vte_termcap_find_string(termcap,
						       terminal,
						       entries[i].special);
			if (cap != NULL) {
				*special = NULL;
				if (strlen(cap) > 0) {
					/* Save the special string. */
					*special = entries[i].special;
					_vte_debug_print(VTE_DEBUG_KEYBOARD,
							" to \"%s\"", *special);
				}
				g_free(cap);
				if (*special != NULL) {
					/* Return the special string. */
					_vte_debug_print(VTE_DEBUG_KEYBOARD,
							", returning.\n");
					return;
				}
			}
		}
	}
	if (termcap_special != NULL) {
		tmp = g_strdup(terminal);
		cap = NULL;
		if (tgetent(ncurses_buffer, tmp) == 1) {
			cap = ncurses_area;
			tmp = g_strdup(termcap_special);
			cap = tgetstr(tmp, &cap);
		}
		if ((cap == NULL) && (strstr(terminal, "xterm") != NULL)) {
			/* try, try again */
			if (tgetent(ncurses_buffer, "xterm-xfree86") == 1) {
				cap = ncurses_area;
				tmp = g_strdup(termcap_special);
				cap = tgetstr(tmp, &cap);
			}
		}
		g_free(tmp);
		if ((cap != NULL) && (*cap != '\0')) {
			*normal_length = strlen(cap);
			*normal = g_strdup(cap);
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_KEYBOARD)) {
				int j;
				g_printerr(" via " VTE_TERMCAP_NAME " to '");
				for (j = 0; j < *normal_length; j++) {
					if (((*normal)[j] < 32) ||
					    ((*normal)[j] >= 127)) {
						g_printerr("<0x%02x>",
							(*normal)[j]);
					} else {
						g_printerr("%c",
							(*normal)[j]);
					}
				}
				g_printerr("', returning.\n");
			}
#endif
			return;
		}
	}

	_vte_debug_print(VTE_DEBUG_KEYBOARD,
			" (ignoring, no match for modifier state).\n");
}

gboolean
_vte_keymap_key_is_modifier(guint keyval)
{
	gboolean modifier = FALSE;
	/* Determine if this is just a modifier key. */
	switch (keyval) {
	case GDK_KEY (Alt_L):
	case GDK_KEY (Alt_R):
	case GDK_KEY (Caps_Lock):
	case GDK_KEY (Control_L):
	case GDK_KEY (Control_R):
	case GDK_KEY (Eisu_Shift):
	case GDK_KEY (Hyper_L):
	case GDK_KEY (Hyper_R):
	case GDK_KEY (ISO_First_Group_Lock):
	case GDK_KEY (ISO_Group_Lock):
	case GDK_KEY (ISO_Group_Shift):
	case GDK_KEY (ISO_Last_Group_Lock):
	case GDK_KEY (ISO_Level3_Lock):
	case GDK_KEY (ISO_Level3_Shift):
	case GDK_KEY (ISO_Lock):
	case GDK_KEY (ISO_Next_Group_Lock):
	case GDK_KEY (ISO_Prev_Group_Lock):
	case GDK_KEY (Kana_Lock):
	case GDK_KEY (Kana_Shift):
	case GDK_KEY (Meta_L):
	case GDK_KEY (Meta_R):
	case GDK_KEY (Num_Lock):
	case GDK_KEY (Scroll_Lock):
	case GDK_KEY (Shift_L):
	case GDK_KEY (Shift_Lock):
	case GDK_KEY (Shift_R):
	case GDK_KEY (Super_L):
	case GDK_KEY (Super_R):
		modifier = TRUE;
		break;
	default:
		modifier = FALSE;
		break;
	}
	return modifier;
}

static gboolean
_vte_keymap_key_gets_modifiers(guint keyval)
{
	gboolean fkey = FALSE;
	/* Determine if this key gets modifiers. */
	switch (keyval) {
	case GDK_KEY (Up):
	case GDK_KEY (Down):
	case GDK_KEY (Left):
	case GDK_KEY (Right):
	case GDK_KEY (Insert):
	case GDK_KEY (Delete):
	case GDK_KEY (Page_Up):
	case GDK_KEY (Page_Down):
	case GDK_KEY (KP_Up):
	case GDK_KEY (KP_Down):
	case GDK_KEY (KP_Left):
	case GDK_KEY (KP_Right):
	case GDK_KEY (KP_Insert):
	case GDK_KEY (KP_Delete):
	case GDK_KEY (KP_Page_Up):
	case GDK_KEY (KP_Page_Down):
	case GDK_KEY (F1):
	case GDK_KEY (F2):
	case GDK_KEY (F3):
	case GDK_KEY (F4):
	case GDK_KEY (F5):
	case GDK_KEY (F6):
	case GDK_KEY (F7):
	case GDK_KEY (F8):
	case GDK_KEY (F9):
	case GDK_KEY (F10):
	case GDK_KEY (F11):
	case GDK_KEY (F12):
	case GDK_KEY (F13):
	case GDK_KEY (F14):
	case GDK_KEY (F15):
	case GDK_KEY (F16):
	case GDK_KEY (F17):
	case GDK_KEY (F18):
	case GDK_KEY (F19):
	case GDK_KEY (F20):
	case GDK_KEY (F21):
	case GDK_KEY (F22):
	case GDK_KEY (F23):
	case GDK_KEY (F24):
	case GDK_KEY (F25):
	case GDK_KEY (F26):
	case GDK_KEY (F27):
	case GDK_KEY (F28):
	case GDK_KEY (F29):
	case GDK_KEY (F30):
	case GDK_KEY (F31):
	case GDK_KEY (F32):
	case GDK_KEY (F33):
	case GDK_KEY (F34):
	case GDK_KEY (F35):
		fkey = TRUE;
		break;
	default:
		fkey = FALSE;
		break;
	}
	return fkey;
}

/* Prior and Next are ommitted for the SS3 to CSI switch below */
static gboolean
is_cursor_key(guint keyval)
{
	switch (keyval) {
	case GDK_KEY (Home):
	case GDK_KEY (Left):
	case GDK_KEY (Up):
	case GDK_KEY (Right):
	case GDK_KEY (Down):
	case GDK_KEY (End):

	case GDK_KEY (KP_Home):
	case GDK_KEY (KP_Left):
	case GDK_KEY (KP_Up):
	case GDK_KEY (KP_Right):
	case GDK_KEY (KP_Down):
	case GDK_KEY (KP_End):
		return TRUE;
	default:
		return FALSE;
	}
}


void
_vte_keymap_key_add_key_modifiers(guint keyval,
				  GdkModifierType modifiers,
				  gboolean sun_mode,
				  gboolean hp_mode,
				  gboolean legacy_mode,
				  gboolean vt220_mode,
				  gboolean cursor_app_mode,
				  char **normal,
				  gssize *normal_length)
{
	int modifier, offset;
	char *nnormal;
	GdkModifierType significant_modifiers;

	significant_modifiers = GDK_SHIFT_MASK |
				GDK_CONTROL_MASK |
				VTE_META_MASK;

	if (!_vte_keymap_key_gets_modifiers(keyval)) {
		return;
	}
	if (sun_mode || hp_mode || vt220_mode) {
		/* no modifiers for you! */
		return;
	}

	switch (modifiers & significant_modifiers) {
	case 0:
		modifier = 0;
		break;
	case GDK_SHIFT_MASK:
		modifier = 2;
		break;
	case VTE_META_MASK:
		modifier = 3;
		break;
	case GDK_SHIFT_MASK | VTE_META_MASK:
		modifier = 4;
		break;
	case GDK_CONTROL_MASK:
		modifier = 5;
		break;
	case GDK_SHIFT_MASK | GDK_CONTROL_MASK:
		modifier = 6;
		break;
	case VTE_META_MASK | GDK_CONTROL_MASK:
		modifier = 7;
		break;
	case GDK_SHIFT_MASK | VTE_META_MASK | GDK_CONTROL_MASK:
		modifier = 8;
		break;
	default:
		modifier = 8;
		break;
	}

	if (modifier == 0) {
		return;
	}

	nnormal = g_malloc0(*normal_length + 4);
	memcpy(nnormal, *normal, *normal_length);
	if (strlen(nnormal) > 1) {
		/* SS3 should have no modifiers so make it CSI instead. See
		 * http://cvsweb.xfree86.org/cvsweb/xc/programs/xterm/input.c.diff?r1=3.57&r2=3.58
		 */
		if (cursor_app_mode &&
			g_str_has_prefix(nnormal, _VTE_CAP_SS3)
			&& is_cursor_key(keyval)) {
			nnormal[1] = '[';
		}

		/* Get the offset of the last character. */
		offset = strlen(nnormal) - 1;
		if (g_ascii_isdigit(nnormal[offset - 1])) {
			/* Stuff a semicolon and the modifier in right before
			 * that last character. */
			nnormal[offset + 2] = nnormal[offset];
			nnormal[offset + 1] = modifier + '0';
			nnormal[offset + 0] = ';';
			*normal_length += 2;
		} else {
#if 1
			/* Stuff a "1", a semicolon and the modifier in right
			 * before that last character, matching Xterm. */
			nnormal[offset + 3] = nnormal[offset];
			nnormal[offset + 2] = modifier + '0';
			nnormal[offset + 1] = ';';
			nnormal[offset + 0] = '1';
			*normal_length += 3;
#else
			/* Stuff the modifier in right before that last
			 * character, matching what people expect. */
			nnormal[offset + 1] = nnormal[offset];
			nnormal[offset + 0] = modifier + '0';
			*normal_length += 1;
#endif
		}
		g_free(*normal);
		*normal = nnormal;
	} else {
		g_free(nnormal);
	}
}
