/*
 * Copyright (C) 2002 Red Hat, Inc.
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
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include "caps.h"
#include "keymap.h"

enum _vte_cursor_mode {
	cursor_normal =	1 << 0,
	cursor_app =	1 << 1,
};

enum _vte_keypad_mode {
	keypad_normal =	1 << 0,
	keypad_app =	1 << 1,
	keypad_vt220 =	1 << 2,
};

enum _vte_fkey_mode {
	fkey_normal =	1 << 0,
	fkey_sun =	1 << 1,
	fkey_hp =	1 << 2,
	fkey_legacy =	1 << 3,
};

#define cursor_all	(cursor_normal | cursor_app)
#define keypad_all	(keypad_normal | keypad_app | keypad_vt220)
#define fkey_all	(fkey_normal | fkey_sun | fkey_hp | fkey_legacy)

struct _vte_keymap_entry {
	enum _vte_cursor_mode cursor_mode;
	enum _vte_keypad_mode keypad_mode;
	enum _vte_fkey_mode fkey_mode;
	GdkModifierType mod_mask;
	const char *normal;
	ssize_t normal_length;
	const char *special;
};

/* Normal keys unaffected by modes. */
static struct _vte_keymap_entry _vte_keymap_GDK_space[] = {
	/* Control+space = NUL */
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, "", 1, NULL},
	/* Regular space. */
	{cursor_all, keypad_all, fkey_all, 0, " ", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_Tab[] = {
	/* Shift+Tab = BackTab */
	{cursor_all, keypad_all, fkey_all, GDK_SHIFT_MASK, NULL, 1, "kB"},
	/* Regular tab. */
	{cursor_all, keypad_all, fkey_all, 0, "\t", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_Insert[] = {
	{cursor_all, keypad_app, fkey_all, 0, NULL, 0, "kI"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_ISO_Left_Tab[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 1, "kB"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_Home[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "kh"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_End[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "@7"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_Page_Up[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "kP"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_Page_Down[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "kN"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

/* Keys affected by the cursor key mode. */
static struct _vte_keymap_entry _vte_keymap_GDK_Up[] = {
	{cursor_normal, keypad_all, fkey_all, 0, NULL, 0, "ku"},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "A", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_Down[] = {
	{cursor_normal, keypad_all, fkey_all, 0, NULL, 0, "kd"},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "B", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_Right[] = {
	{cursor_normal, keypad_all, fkey_all, 0, NULL, 0, "kr"},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "C", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_Left[] = {
	{cursor_normal, keypad_all, fkey_all, 0, NULL, 0, "kl"},
	{cursor_app, keypad_all, fkey_all, 0, _VTE_CAP_SS3 "D", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

/* Keys (potentially) affected by the keypad key mode. */
static struct _vte_keymap_entry _vte_keymap_GDK_KP_Space[] = {
	{cursor_all, keypad_normal, fkey_all, 0, " ", 1, NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 " ", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 " ", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Tab[] = {
	{cursor_all, keypad_normal, fkey_all, 0, "\t", 1, NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "I", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "I", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Enter[] = {
	{cursor_all, keypad_normal, fkey_all, 0, NULL, 0, "@8"},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "M", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "M", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_F1[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k1"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_F2[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k2"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_F3[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k3"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_F4[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k4"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Multiply[] = {
	{cursor_all, keypad_normal, fkey_all, 0, "*", 1, NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "j", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Add[] = {
	{cursor_all, keypad_normal, fkey_all, 0, "+", 1, NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "k", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Separator[] = {
	{cursor_all, keypad_normal, fkey_all, 0, ",", 1, NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "l", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Subtract[] = {
	{cursor_all, keypad_normal, fkey_all, 0, "-", 1, NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "m", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Decimal_Delete[] = {
	{cursor_all, keypad_normal, fkey_all, 0, ".", 1, NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "3~", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Divide[] = {
	{cursor_all, keypad_normal, fkey_all, 0, "/", 1, NULL},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_SS3 "o", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

/* GDK already separates keypad "0" from keypad "Insert", so the only time
 * we'll see this key is when NumLock is on, and that means that we're in
 * "normal" mode. */
static struct _vte_keymap_entry _vte_keymap_GDK_KP_0[] = {
	{cursor_all, keypad_all, fkey_all, 0, "0", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_1[] = {
	{cursor_all, keypad_all, fkey_all, 0, "1", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_2[] = {
	{cursor_all, keypad_all, fkey_all, 0, "2", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_3[] = {
	{cursor_all, keypad_all, fkey_all, 0, "3", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_4[] = {
	{cursor_all, keypad_all, fkey_all, 0, "4", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_5[] = {
	{cursor_all, keypad_all, fkey_all, 0, "5", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_6[] = {
	{cursor_all, keypad_all, fkey_all, 0, "6", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_7[] = {
	{cursor_all, keypad_all, fkey_all, 0, "7", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_8[] = {
	{cursor_all, keypad_all, fkey_all, 0, "8", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_9[] = {
	{cursor_all, keypad_all, fkey_all, 0, "9", 1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

/* These are the same keys as above, but without numlock.  If there's a
 * capability associated with the key, then we send that, unless we're in
 * application mode. */
static struct _vte_keymap_entry _vte_keymap_GDK_KP_Insert[] = {
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_CSI "2~", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "p", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_End[] = {
	{cursor_all, keypad_normal, fkey_all, 0, NULL, 0, "K4"},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_CSI "4~", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "q", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Down[] = {
	{cursor_all, keypad_normal, fkey_all, 0, NULL, 0, "kd"},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_CSI "B", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "r", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Page_Down[] = {
	{cursor_all, keypad_normal, fkey_all, 0, NULL, 0, "K5"},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_CSI "5~", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "s", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Left[] = {
	{cursor_all, keypad_normal, fkey_all, 0, NULL, 0, "kl"},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_CSI "D", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "t", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Begin[] = {
	{cursor_all, keypad_normal, fkey_all, 0, NULL, 0, "K2"},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_CSI "E", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "u", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Right[] = {
	{cursor_all, keypad_normal, fkey_all, 0, NULL, 0, "kr"},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_CSI "C", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "v", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Home[] = {
	{cursor_all, keypad_normal, fkey_all, 0, NULL, 0, "K1"},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_CSI "1~", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "w", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Up[] = {
	{cursor_all, keypad_normal, fkey_all, 0, NULL, 0, "ku"},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_CSI "A", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "x", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_KP_Page_Up[] = {
	{cursor_all, keypad_normal, fkey_all, 0, NULL, 0, "K3"},
	{cursor_all, keypad_app, fkey_all, 0, _VTE_CAP_CSI "5~", -1, NULL},
	{cursor_all, keypad_vt220, fkey_all, 0, _VTE_CAP_SS3 "y", -1, NULL},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};


/* Keys affected by the function key mode. */
static struct _vte_keymap_entry _vte_keymap_GDK_F1[] = {
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "11~", -1, NULL},
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, NULL, 0, "F3"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k1"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F2[] = {
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "12~", -1, NULL},
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, NULL, 0, "F4"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k2"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F3[] = {
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "13~", -1, NULL},
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, NULL, 0, "F5"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k3"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F4[] = {
	{cursor_all, keypad_all, fkey_legacy, 0, _VTE_CAP_CSI "14~", -1, NULL},
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, NULL, 0, "F6"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k4"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F5[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, NULL, 0, "F7"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k5"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F6[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, NULL, 0, "F8"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k6"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F7[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, NULL, 0, "F9"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k7"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F8[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, NULL, 0, "FA"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k8"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F9[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, NULL, 0, "FB"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k9"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F10[] = {
	{cursor_all, keypad_all, fkey_all, GDK_CONTROL_MASK, NULL, 0, "FC"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "k;"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F11[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "F1"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F12[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "F2"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F13[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "F3"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F14[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "F4"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F15[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "F5"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F16[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "F6"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F17[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "F7"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F18[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "F8"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F19[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "F9"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F20[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FA"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F21[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FB"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F22[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FC"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F23[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FD"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F24[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FE"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F25[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FF"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F26[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FG"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F27[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FH"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F28[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FI"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F29[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FJ"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F30[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FK"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F31[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FL"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F32[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FM"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F33[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FN"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F34[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FO"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_entry _vte_keymap_GDK_F35[] = {
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, "FP"},
	{cursor_all, keypad_all, fkey_all, 0, NULL, 0, NULL},
};

static struct _vte_keymap_group {
	guint keyval;
	struct _vte_keymap_entry *entries;
} _vte_keymap[] = {
	{GDK_space,		_vte_keymap_GDK_space},
	{GDK_Tab,		_vte_keymap_GDK_Tab},
	{GDK_ISO_Left_Tab,	_vte_keymap_GDK_ISO_Left_Tab},
	{GDK_Home,		_vte_keymap_GDK_Home},
	{GDK_End,		_vte_keymap_GDK_End},
	{GDK_Insert,		_vte_keymap_GDK_Insert},
	/* GDK_Delete is all handled in code, due to funkiness. */
	{GDK_Page_Up,		_vte_keymap_GDK_Page_Up},
	{GDK_Page_Down,		_vte_keymap_GDK_Page_Down},

	{GDK_Up,		_vte_keymap_GDK_Up},
	{GDK_Down,		_vte_keymap_GDK_Down},
	{GDK_Right,		_vte_keymap_GDK_Right},
	{GDK_Left,		_vte_keymap_GDK_Left},

	{GDK_KP_Space,		_vte_keymap_GDK_KP_Space},
	{GDK_KP_Tab,		_vte_keymap_GDK_KP_Tab},
	{GDK_KP_Enter,		_vte_keymap_GDK_KP_Enter},
	{GDK_KP_F1,		_vte_keymap_GDK_KP_F1},
	{GDK_KP_F2,		_vte_keymap_GDK_KP_F2},
	{GDK_KP_F3,		_vte_keymap_GDK_KP_F3},
	{GDK_KP_F4,		_vte_keymap_GDK_KP_F4},
	{GDK_KP_Multiply,	_vte_keymap_GDK_KP_Multiply},
	{GDK_KP_Add,		_vte_keymap_GDK_KP_Add},
	{GDK_KP_Separator,	_vte_keymap_GDK_KP_Separator},
	{GDK_KP_Subtract,	_vte_keymap_GDK_KP_Subtract},
	{GDK_KP_Decimal,	_vte_keymap_GDK_KP_Decimal_Delete},
	{GDK_KP_Delete,		_vte_keymap_GDK_KP_Decimal_Delete},
	{GDK_KP_Divide,		_vte_keymap_GDK_KP_Divide},
	{GDK_KP_0,		_vte_keymap_GDK_KP_0},
	{GDK_KP_Insert,		_vte_keymap_GDK_KP_Insert},
	{GDK_KP_1,		_vte_keymap_GDK_KP_1},
	{GDK_KP_End,		_vte_keymap_GDK_KP_End},
	{GDK_KP_2,		_vte_keymap_GDK_KP_2},
	{GDK_KP_Down,		_vte_keymap_GDK_KP_Down},
	{GDK_KP_3,		_vte_keymap_GDK_KP_3},
	{GDK_KP_Page_Down,	_vte_keymap_GDK_KP_Page_Down},
	{GDK_KP_4,		_vte_keymap_GDK_KP_4},
	{GDK_KP_Left,		_vte_keymap_GDK_KP_Left},
	{GDK_KP_5,		_vte_keymap_GDK_KP_5},
	{GDK_KP_Begin,		_vte_keymap_GDK_KP_Begin},
	{GDK_KP_6,		_vte_keymap_GDK_KP_6},
	{GDK_KP_Right,		_vte_keymap_GDK_KP_Right},
	{GDK_KP_7,		_vte_keymap_GDK_KP_7},
	{GDK_KP_Home,		_vte_keymap_GDK_KP_Home},
	{GDK_KP_8,		_vte_keymap_GDK_KP_8},
	{GDK_KP_Up,		_vte_keymap_GDK_KP_Up},
	{GDK_KP_9,		_vte_keymap_GDK_KP_9},
	{GDK_KP_Page_Up,	_vte_keymap_GDK_KP_Page_Up},

	{GDK_F1,		_vte_keymap_GDK_F1},
	{GDK_F2,		_vte_keymap_GDK_F2},
	{GDK_F3,		_vte_keymap_GDK_F3},
	{GDK_F4,		_vte_keymap_GDK_F4},
	{GDK_F5,		_vte_keymap_GDK_F5},
	{GDK_F6,		_vte_keymap_GDK_F6},
	{GDK_F7,		_vte_keymap_GDK_F7},
	{GDK_F8,		_vte_keymap_GDK_F8},
	{GDK_F9,		_vte_keymap_GDK_F9},
	{GDK_F10,		_vte_keymap_GDK_F10},
	{GDK_F11,		_vte_keymap_GDK_F11},
	{GDK_F12,		_vte_keymap_GDK_F12},
	{GDK_F13,		_vte_keymap_GDK_F13},
	{GDK_F14,		_vte_keymap_GDK_F14},
	{GDK_F15,		_vte_keymap_GDK_F15},
	{GDK_F16,		_vte_keymap_GDK_F16},
	{GDK_F17,		_vte_keymap_GDK_F17},
	{GDK_F18,		_vte_keymap_GDK_F18},
	{GDK_F19,		_vte_keymap_GDK_F19},
	{GDK_F20,		_vte_keymap_GDK_F20},
	{GDK_F21,		_vte_keymap_GDK_F21},
	{GDK_F22,		_vte_keymap_GDK_F22},
	{GDK_F23,		_vte_keymap_GDK_F23},
	{GDK_F24,		_vte_keymap_GDK_F24},
	{GDK_F25,		_vte_keymap_GDK_F25},
	{GDK_F26,		_vte_keymap_GDK_F26},
	{GDK_F27,		_vte_keymap_GDK_F27},
	{GDK_F28,		_vte_keymap_GDK_F28},
	{GDK_F29,		_vte_keymap_GDK_F29},
	{GDK_F30,		_vte_keymap_GDK_F30},
	{GDK_F31,		_vte_keymap_GDK_F31},
	{GDK_F32,		_vte_keymap_GDK_F32},
	{GDK_F33,		_vte_keymap_GDK_F33},
	{GDK_F34,		_vte_keymap_GDK_F34},
	{GDK_F35,		_vte_keymap_GDK_F35},
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
		char **normal,
		size_t *normal_length,
		const char **special)
{
	int i, modifier_value;
	gboolean fkey;
	struct _vte_keymap_entry *entries;
	enum _vte_cursor_mode cursor_mode;
	enum _vte_keypad_mode keypad_mode;
	enum _vte_fkey_mode fkey_mode;

	g_return_if_fail(normal != NULL);
	g_return_if_fail(normal_length != NULL);
	g_return_if_fail(special != NULL);

	/* Start from scratch. */
	*normal = NULL;
	*special = NULL;
	*normal_length = 0;

	/* Search for the list for this key. */
	entries = NULL;
	fkey = FALSE;
	for (i = 0; i < G_N_ELEMENTS(_vte_keymap); i++) {
#ifdef VTE_DEBUG
		int j;
		entries = _vte_keymap[i].entries;
		for (j = 0; entries[j].normal || entries[j].special; j++) {
			if (entries[j].normal != NULL) {
				g_assert(entries[j].normal_length != 0);
			}
		}
		entries = NULL;
#endif
		if (_vte_keymap[i].keyval == GDK_F1) {
			/* Every entry after this point is a function key. */
			fkey = TRUE;
		}
		if (_vte_keymap[i].keyval == keyval) {
			/* Found it! */
			entries = _vte_keymap[i].entries;
			break;
		}
	}
	if (entries == NULL) {
		return;
	}

	/* Build mode masks.  Numlock negates application cursor mode. */
	if (modifiers & VTE_NUMLOCK_MASK) {
		cursor_mode = cursor_normal;
	} else {
		cursor_mode = app_cursor_keys ? cursor_app : cursor_normal;
	}
	keypad_mode = app_keypad_keys ?
		      (vt220_mode ?  keypad_vt220 : keypad_app) :
		      keypad_normal;
	if (sun_mode) {
		fkey_mode = fkey_sun;
	} else
	if (hp_mode) {
		fkey_mode = fkey_hp;
	} else
	if (legacy_mode) {
		fkey_mode = fkey_legacy;
	} else {
		fkey_mode = fkey_normal;
	}
	modifiers &= (GDK_SHIFT_MASK | GDK_CONTROL_MASK | VTE_META_MASK);

	/* Search for the conditions. */
	for (i = 0; entries[i].normal || entries[i].special; i++)
	if ((entries[i].cursor_mode & cursor_mode) &&
	    (entries[i].keypad_mode & keypad_mode) &&
	    (entries[i].fkey_mode & fkey_mode))
	if ((modifiers & entries[i].mod_mask) == entries[i].mod_mask) {
		if (entries[i].normal) {
			if (entries[i].normal_length != -1) {
				*normal_length = entries[i].normal_length;
			} else {
				*normal_length = strlen(entries[i].normal);
			}
			/* Return a copy of the normal string. */
			if (fkey && (fkey_mode == fkey_sun)) {
				/* Append the modifier state. */
				modifier_value = 0;
				switch (modifiers) {
				case GDK_SHIFT_MASK:
					modifier_value = 2;
					break;
				case VTE_META_MASK:
					modifier_value = 3;
					break;
				case GDK_SHIFT_MASK | VTE_META_MASK:
					modifier_value = 4;
					break;
				case GDK_CONTROL_MASK:
					modifier_value = 5;
					break;
				case GDK_CONTROL_MASK | GDK_SHIFT_MASK:
					modifier_value = 6;
					break;
				case GDK_CONTROL_MASK | VTE_META_MASK:
					modifier_value = 7;
					break;
				case GDK_CONTROL_MASK | GDK_SHIFT_MASK |
				     VTE_META_MASK:
					modifier_value = 8;
					break;
				default:
					modifier_value = 8;
					break;
				}
				if (modifier_value == 0) {
					/* Copy it verbatim. */
					*normal = g_memdup(entries[i].normal,
							   *normal_length + 1);
				} else {
					/* Copy and stuff in the modifiers. */
					*normal = g_malloc0(*normal_length + 3);
					memcpy(*normal, entries[i].normal,
					       entries[i].normal_length);
					(*normal)[entries[i].normal_length + 1] = (*normal)[entries[i].normal_length - 1];
					(*normal)[entries[i].normal_length] = '0' + modifier_value;
					(*normal)[entries[i].normal_length - 1] = ';';
				}
			} else {
				/* Copy it verbatim. */
				*normal = g_memdup(entries[i].normal,
						   *normal_length + 1);
			}
			return;
		}
		if (entries[i].special) {
			/* Return the original special string. */
			*special = entries[i].special;
			return;
		}
	}
}

gboolean
_vte_keymap_key_is_modifier(guint keyval)
{
	gboolean modifier = FALSE;
	/* Determine if this is just a modifier key. */
	switch (keyval) {
	case GDK_Alt_L:
	case GDK_Alt_R:
	case GDK_Caps_Lock:
	case GDK_Control_L:
	case GDK_Control_R:
	case GDK_Eisu_Shift:
	case GDK_Hyper_L:
	case GDK_Hyper_R:
	case GDK_ISO_First_Group_Lock:
	case GDK_ISO_Group_Lock:
	case GDK_ISO_Group_Shift:
	case GDK_ISO_Last_Group_Lock:
	case GDK_ISO_Level3_Lock:
	case GDK_ISO_Level3_Shift:
	case GDK_ISO_Lock:
	case GDK_ISO_Next_Group_Lock:
	case GDK_ISO_Prev_Group_Lock:
	case GDK_Kana_Lock:
	case GDK_Kana_Shift:
	case GDK_Meta_L:
	case GDK_Meta_R:
	case GDK_Num_Lock:
	case GDK_Scroll_Lock:
	case GDK_Shift_L:
	case GDK_Shift_Lock:
	case GDK_Shift_R:
	case GDK_Super_L:
	case GDK_Super_R:
		modifier = TRUE;
		break;
	default:
		modifier = FALSE;
		break;
	}
	return modifier;
}
