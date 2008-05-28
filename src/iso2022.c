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
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include "debug.h"
#include "buffer.h"
#include "iso2022.h"
#include "matcher.h"
#include "vteconv.h"
#include "vtetree.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <glib/gi18n-lib.h>

/* Maps which jive with XTerm's ESC ()*+ ? sequences, RFC 1468.  Add the
 * PC437 map because despite knowing that XTerm doesn't support it, certain
 * applications try to use it anyway. */
#define NARROW_MAPS	"012AB4C5RQKYE6ZH7=" "J" "U"
/* Maps which jive with RFC 1468's ESC $ ? sequences. */
#define WIDE_MAPS	"@B"
/* Maps which jive with RFC 1557/1922/2237's ESC $ ()*+ ? sequences. */
#define WIDE_GMAPS	"C" "AGHIJKLM" "D"
/* Fudge factor we add to wide map identifiers to keep them distinct. */
#define WIDE_FUDGE	0x100000
/* An invalid codepoint. */
#define INVALID_CODEPOINT 0xFFFD

struct _vte_iso2022_map {
	gulong from;
	gunichar to;
};

struct _vte_iso2022_block {
	enum {
		_vte_iso2022_cdata,
		_vte_iso2022_preserve,
		_vte_iso2022_control
	} type;
	gulong start, end;
};

struct _vte_iso2022_state {
	gboolean nrc_enabled;
	int current, override;
	gunichar g[4];
	const gchar *codeset, *native_codeset, *utf8_codeset, *target_codeset;
	gint ambiguous_width;
	VteConv conv;
	_vte_iso2022_codeset_changed_cb_fn codeset_changed;
	gpointer codeset_changed_data;
	struct _vte_buffer *buffer;
};

/* DEC Special Character and Line Drawing Set.  VT100 and higher (per XTerm
 * docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_0[] = {
	{ 96, 0x25c6},	/* diamond */
	{'a', 0x2592},	/* checkerboard */
	{'b', 0x2409},	/* HT symbol */
	{'c', 0x240c},	/* FF symbol */
	{'d', 0x240d},	/* CR symbol */
	{'e', 0x240a},	/* LF symbol */
	{'f', 0x00b0},	/* degree */
	{'g', 0x00b1},	/* plus/minus */
	{'h', 0x2424},  /* NL symbol */
	{'i', 0x240b},  /* VT symbol */
	{'j', 0x2518},	/* downright corner */
	{'k', 0x2510},	/* upright corner */
	{'l', 0x250c},	/* upleft corner */
	{'m', 0x2514},	/* downleft corner */
	{'n', 0x253c},	/* cross */
	{'o', 0x23ba},  /* scan line 1/9 */
	{'p', 0x23bb},  /* scan line 3/9 */
	{'q', 0x2500},	/* horizontal line (also scan line 5/9) */
	{'r', 0x23bc},  /* scan line 7/9 */
	{'s', 0x23bd},  /* scan line 9/9 */
	{'t', 0x251c},	/* left t */
	{'u', 0x2524},	/* right t */
	{'v', 0x2534},	/* bottom t */
	{'w', 0x252c},	/* top t */
	{'x', 0x2502},	/* vertical line */
	{'y', 0x2264},  /* <= */
	{'z', 0x2265},  /* >= */
	{'{', 0x03c0},  /* pi */
	{'|', 0x2260},  /* not equal */
	{'}', 0x00a3},  /* pound currency sign */
	{'~', 0x00b7},	/* bullet */
};
/* United Kingdom.  VT100 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_A[] = {
	{'$', GDK_sterling},
};
/* US-ASCII (no conversions).  VT100 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_B[] = {
	{0, 0},
};
/* Dutch. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_4[] = {
	{'#',  GDK_sterling},
	{'@',  GDK_threequarters},
	{'[',  GDK_ydiaeresis},
	{'\\', GDK_onehalf},
	{']',  GDK_bar}, /* FIXME? not in XTerm 170 */
	{'{',  GDK_diaeresis},
	{'|',  0x192}, /* f with hook (florin) */ /* FIXME? not in XTerm 170 */
	{'}',  GDK_onequarter},
	{'~',  GDK_acute}
};
/* Finnish. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_C[] = {
	{'[',  GDK_Adiaeresis},
	{'\\', GDK_Odiaeresis},
	{']',  GDK_Aring},
	{'^',  GDK_Udiaeresis},
	{'`',  GDK_eacute},
	{'{',  GDK_adiaeresis},
	{'|',  GDK_odiaeresis},
	{'}',  GDK_aring},
	{'~',  GDK_udiaeresis},
};
/* French. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_R[] = {
	{'#',  GDK_sterling},
	{'@',  GDK_agrave},
	{'[',  GDK_degree},
	{'\\', GDK_ccedilla},
	{']',  GDK_section},
	{'{',  GDK_eacute},
	{'|',  GDK_ugrave},
	{'}',  GDK_egrave},
	{'~',  GDK_diaeresis},
};
/* French Canadian. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_Q[] = {
	{'@',  GDK_agrave},
	{'[',  GDK_acircumflex},
	{'\\', GDK_ccedilla},
	{']',  GDK_ecircumflex},
	{'^',  GDK_icircumflex},
	{'`',  GDK_ocircumflex},
	{'{',  GDK_eacute},
	{'|',  GDK_ugrave},
	{'}',  GDK_egrave},
	{'~',  GDK_ucircumflex},
};
/* German. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_K[] = {
	{'@',  GDK_section},
	{'[',  GDK_Adiaeresis},
	{'\\', GDK_Odiaeresis},
	{']',  GDK_Udiaeresis},
	{'{',  GDK_adiaeresis},
	{'|',  GDK_odiaeresis},
	{'}',  GDK_udiaeresis},
	{'~',  GDK_ssharp},
};
/* Italian. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_Y[] = {
	{'#',  GDK_sterling},
	{'@',  GDK_section},
	{'[',  GDK_degree},
	{'\\', GDK_ccedilla},
	{']',  GDK_eacute},
	{'`',  GDK_ugrave},
	{'{',  GDK_agrave},
	{'|',  GDK_ograve},
	{'}',  GDK_egrave},
	{'~',  GDK_igrave},
};
/* Norwegian and Danish. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_E[] = {
	{'@',  GDK_Adiaeresis},
	{'[',  GDK_AE},
	{'\\', GDK_Ooblique},
	{']',  GDK_Aring},
	{'^',  GDK_Udiaeresis},
	{'`',  GDK_adiaeresis},
	{'{',  GDK_ae},
	{'|',  GDK_oslash},
	{'}',  GDK_aring},
	{'~',  GDK_udiaeresis},
};
/* Spanish. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_Z[] = {
	{'#',  GDK_sterling},
	{'@',  GDK_section},
	{'[',  GDK_exclamdown},
	{'\\', GDK_Ntilde},
	{']',  GDK_questiondown},
	{'{',  GDK_degree},
	{'|',  GDK_ntilde},
	{'}',  GDK_ccedilla},
};
/* Swedish. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_H[] = {
	{'@',  GDK_Eacute},
	{'[',  GDK_Adiaeresis},
	{'\\', GDK_Odiaeresis},
	{']',  GDK_Aring},
	{'^',  GDK_Udiaeresis},
	{'`',  GDK_eacute},
	{'{',  GDK_adiaeresis},
	{'|',  GDK_odiaeresis},
	{'}',  GDK_aring},
	{'~',  GDK_udiaeresis},
};
/* Swiss. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_equal[] = {
	{'#',  GDK_ugrave},
	{'@',  GDK_agrave},
	{'[',  GDK_eacute},
	{'\\', GDK_ccedilla},
	{']',  GDK_ecircumflex},
	{'^',  GDK_icircumflex},
	{'_',  GDK_egrave},
	{'`',  GDK_ocircumflex},
	{'{',  GDK_adiaeresis},
	{'|',  GDK_odiaeresis},
	{'}',  GDK_udiaeresis},
	{'~',  GDK_ucircumflex},
};
/* Codepage 437. */
static const struct _vte_iso2022_map _vte_iso2022_map_U[] = {
#include "unitable.CP437"
};

/* Japanese.  JIS X 0201-1976 ("Roman" set), per RFC 1468/2237. */
static const struct _vte_iso2022_map _vte_iso2022_map_J[] = {
#include "unitable.JIS0201"
};
/* Japanese.  JIS X 0208-1978, per RFC 1468/2237. */
static const struct _vte_iso2022_map _vte_iso2022_map_wide_at[] = {
#include "unitable.JIS0208"
};
/* Chinese.  GB 2312-80, per RFC 1922. */
static const struct _vte_iso2022_map _vte_iso2022_map_wide_A[] = {
#include "unitable.GB2312"
};
/* Japanese.  JIS X 0208-1983, per RFC 1468/2237. */
static const struct _vte_iso2022_map _vte_iso2022_map_wide_B[] = {
#include "unitable.JIS0208"
};
/* Korean.  KS X 1001 (formerly KS C 5601), per Ken Lunde's
 * CJKV_Information_Processing. */
static const struct _vte_iso2022_map _vte_iso2022_map_wide_C[] = {
#include "unitable.KSX1001"
};
/* Japanese.  JIS X 0212-1990, per RFC 2237. */
static const struct _vte_iso2022_map _vte_iso2022_map_wide_D[] = {
#include "unitable.JIS0212"
};
/* Chinese.  CNS 11643-plane-1, per RFC 1922. */
static const struct _vte_iso2022_map _vte_iso2022_map_wide_G[] = {
#include "unitable.CNS11643"
};

#include "uniwidths"

static gint
_vte_direct_compare(gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

static gboolean
_vte_iso2022_is_ambiguous_ht(gunichar c)
{
	static GHashTable *ambiguous;
	if (G_UNLIKELY (ambiguous == NULL)) {
		gpointer p;
		gsize i;
		ambiguous = g_hash_table_new (NULL, NULL);
		for (i = 0; i < G_N_ELEMENTS(_vte_iso2022_ambiguous_chars); i++) {
			p = GINT_TO_POINTER(_vte_iso2022_ambiguous_chars[i]);
			g_hash_table_insert(ambiguous, p, p);
		}
	}

	return g_hash_table_lookup(ambiguous, GINT_TO_POINTER(c)) != NULL;
}
static inline gboolean
_vte_iso2022_is_ambiguous(gunichar c)
{
	gsize i;
	for (i = 0; i < G_N_ELEMENTS(_vte_iso2022_unambiguous_ranges); i++) {
		if ((c >= _vte_iso2022_unambiguous_ranges[i].start) &&
		    (c <= _vte_iso2022_unambiguous_ranges[i].end)) {
			return FALSE;
		}
	}
	for (i = 0; i < G_N_ELEMENTS(_vte_iso2022_ambiguous_ranges); i++) {
		if ((c >= _vte_iso2022_ambiguous_ranges[i].start) &&
		    (c <= _vte_iso2022_ambiguous_ranges[i].end)) {
			return TRUE;
		}
	}
	return _vte_iso2022_is_ambiguous_ht (c);
}

/* If we only have a codepoint, guess what the ambiguous width should be based
 * on the default region.  Just hope we don't do this too often. */
static int
_vte_iso2022_ambiguous_width_guess(void)
{
	static int guess;
	if (guess == 0) {
		const char *lang = NULL;
		guess = 1;
		if ((lang == NULL) && (g_getenv("LC_ALL") != NULL)) {
			lang = g_getenv("LC_ALL");
		}
		if ((lang == NULL) && (g_getenv("LC_CTYPE") != NULL)) {
			lang = g_getenv("LC_CTYPE");
		}
		if ((lang == NULL) && (g_getenv("LANG") != NULL)) {
			lang = g_getenv("LANG");
		}
		if (lang != NULL) {
			if (g_ascii_strncasecmp(lang, "ja", 2) == 0 ||
					g_ascii_strncasecmp(lang, "ko", 2) == 0 ||
					g_ascii_strncasecmp(lang, "vi", 2) == 0 ||
					g_ascii_strncasecmp(lang, "zh", 2) == 0) {
				guess = 2;
			}
		}
		_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
				"Ambiguous characters will have width = %d.\n",
				guess);
	}
	return guess;
}

/* If we have the encoding, decide how wide an ambiguously-wide character is
 * based on the encoding.  This is basically what GNU libc does, and it agrees
 * with my reading of Unicode UAX 11, so.... */
static int
_vte_iso2022_ambiguous_width(struct _vte_iso2022_state *state)
{
	const char *wide_codelist[] = {
		"big5",
		"big5hkscs",
		"euccn",
		"eucjp",
		"euckr",
		"euctw",
		"gb18030",
		"gb2312",
		"gbk",
		"tcvn",
	};
	gsize i, j;
	char codeset[16];

	/* Catch weirdo cases. */
	if ((state->codeset == NULL) || (state->codeset[0] == '\0')) {
		return 1;
	}

	/* Sort-of canonify the encoding name. */
	i = j = 0;
	for (i = 0; state->codeset[i] != '\0'; i++) {
		if (g_ascii_isalnum(state->codeset[i])) {
			codeset[j++] = g_ascii_tolower(state->codeset[i]);
		}
		if (j >= sizeof(codeset) - 1) {
			break;
		}
	}
	codeset[j] = '\0';

	/* Check for the name in the list. */
	for (i = 0; i < G_N_ELEMENTS(wide_codelist); i++) {
		if (strcmp(codeset, wide_codelist[i]) == 0) {
			return 2;
		}
	}

	/*
	 * Decide the ambiguous width according to the default region if 
	 * current locale is UTF-8.
	 */
	if (strcmp (codeset, "utf8") == 0 && g_getenv("VTE_CJK_WIDTH") != NULL) {
	  const char *env = g_getenv ("VTE_CJK_WIDTH");
	  if (g_ascii_strcasecmp (env, "narrow"))
	    return 1;
	  if (g_ascii_strcasecmp (env, "wide"))
	    return 2;
	  else
	    return _vte_iso2022_ambiguous_width_guess ();
	}

	/* Not in the list => not wide. */
	return 1;
}

static GHashTable *
_vte_iso2022_map_init(const struct _vte_iso2022_map *map, gssize length)
{
	GHashTable *ret;
	int i;
	if (length == 0) {
		return NULL;
	}
	ret = g_hash_table_new(NULL, NULL);
	for (i = 0; i < length; i++) {
		g_hash_table_insert(ret,
			      GINT_TO_POINTER(map[i].from),
			      GINT_TO_POINTER(map[i].to));
	}
	return ret;
}

static void
_vte_iso2022_map_get(gunichar mapname,
		     GHashTable **_map, guint *bytes_per_char, guint *force_width,
		     gulong *or_mask, gulong *and_mask)
{
	static VteTree *maps = NULL;
	struct _vte_iso2022_map _vte_iso2022_map_NUL[256];
	gint bytes = 0, width = 0;
	GHashTable *map = NULL;
	gboolean new_map = FALSE;
	gsize i;

	if (or_mask) {
		*or_mask = 0;
	}
	if (and_mask) {
		*and_mask = (~(0));
	}

	/* Make sure we have a map, erm, map. */
	if (maps == NULL) {
		maps = _vte_tree_new(_vte_direct_compare);
	}

	/* Check for a cached map for this charset. */
	map = _vte_tree_lookup(maps, GINT_TO_POINTER(mapname));
	new_map = map == NULL;

	/* Construct a new one. */
	switch (mapname) {
	case '0':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_0,
					    G_N_ELEMENTS(_vte_iso2022_map_0));
		}
		width = 1;
		bytes = 1;
		break;
	case 'A':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_A,
					    G_N_ELEMENTS(_vte_iso2022_map_A));
		}
		width = 1;
		bytes = 1;
		break;
	case '1': /* treated as an alias in xterm */
	case '2': /* treated as an alias in xterm */
	case 'B':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_B,
					    G_N_ELEMENTS(_vte_iso2022_map_B));
		}
		width = 1;
		bytes = 1;
		break;
	case '4':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_4,
					    G_N_ELEMENTS(_vte_iso2022_map_4));
		}
		width = 1;
		bytes = 1;
		break;
	case 'C':
	case '5':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_C,
					    G_N_ELEMENTS(_vte_iso2022_map_C));
		}
		width = 1;
		bytes = 1;
		break;
	case 'R':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_R,
					    G_N_ELEMENTS(_vte_iso2022_map_R));
		}
		width = 1;
		bytes = 1;
		break;
	case 'Q':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_Q,
					    G_N_ELEMENTS(_vte_iso2022_map_Q));
		}
		width = 1;
		bytes = 1;
		break;
	case 'K':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_K,
					    G_N_ELEMENTS(_vte_iso2022_map_K));
		}
		width = 1;
		bytes = 1;
		break;
	case 'Y':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_Y,
					    G_N_ELEMENTS(_vte_iso2022_map_Y));
		}
		width = 1;
		bytes = 1;
		break;
	case 'E':
	case '6':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_E,
					    G_N_ELEMENTS(_vte_iso2022_map_E));
		}
		width = 1;
		bytes = 1;
		break;
	case 'Z':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_Z,
					    G_N_ELEMENTS(_vte_iso2022_map_Z));
		}
		width = 1;
		bytes = 1;
		break;
	case 'H':
	case '7':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_H,
					    G_N_ELEMENTS(_vte_iso2022_map_H));
		}
		width = 1;
		bytes = 1;
		break;
	case '=':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_equal,
					    G_N_ELEMENTS(_vte_iso2022_map_equal));
		}
		width = 1;
		bytes = 1;
		break;
	case 'U':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_U,
					    G_N_ELEMENTS(_vte_iso2022_map_U));
		}
		width = 1;
		bytes = 1;
		break;
	case 'J':
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_J,
					    G_N_ELEMENTS(_vte_iso2022_map_J));
		}
		width = 1;
		bytes = 1;
		break;
	case '@' + WIDE_FUDGE:
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_at,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_at));
		}
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		*and_mask = 0xf7f7f;
		break;
	case 'A' + WIDE_FUDGE:
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_A,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_A));
		}
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		*and_mask = 0xf7f7f;
		break;
	case 'B' + WIDE_FUDGE:
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_B,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_B));
		}
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		*and_mask = 0xf7f7f;
		break;
	case 'C' + WIDE_FUDGE:
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_C,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_C));
		}
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		*and_mask = 0xf7f7f;
		break;
	case 'D' + WIDE_FUDGE:
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_D,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_D));
		}
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		*and_mask = 0xf7f7f;
		break;
	case 'G' + WIDE_FUDGE:
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_G,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_G));
		}
		/* Return the plane number as part of the "or" mask. */
		g_assert(or_mask != NULL);
		*or_mask = 0x10000;
		*and_mask = 0xf7f7f;
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		break;
	case 'H' + WIDE_FUDGE:
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_G,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_G));
		}
		/* Return the plane number as part of the "or" mask. */
		g_assert(or_mask != NULL);
		*or_mask = 0x20000;
		*and_mask = 0xf7f7f;
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		break;
	case 'I' + WIDE_FUDGE:
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_G,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_G));
		}
		/* Return the plane number as part of the "or" mask. */
		g_assert(or_mask != NULL);
		*or_mask = 0x30000;
		*and_mask = 0xf7f7f;
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		break;
	case 'J' + WIDE_FUDGE:
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_G,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_G));
		}
		/* Return the plane number as part of the "or" mask. */
		g_assert(or_mask != NULL);
		*or_mask = 0x40000;
		*and_mask = 0xf7f7f;
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		break;
	case 'K' + WIDE_FUDGE:
		if (map == NULL) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_G,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_G));
		}
		/* Return the plane number as part of the "or" mask. */
		g_assert(or_mask != NULL);
		*or_mask = 0x50000;
		*and_mask = 0xf7f7f;
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		break;
	case 'L' + WIDE_FUDGE:
		if (G_UNLIKELY (map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_G,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_G));
		}
		/* Return the plane number as part of the "or" mask. */
		g_assert(or_mask != NULL);
		*or_mask = 0x60000;
		*and_mask = 0xf7f7f;
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		break;
	case 'M' + WIDE_FUDGE:
		if (G_UNLIKELY(map == NULL)) {
			map = _vte_iso2022_map_init(_vte_iso2022_map_wide_G,
					    G_N_ELEMENTS(_vte_iso2022_map_wide_G));
		}
		/* Return the plane number as part of the "or" mask. */
		g_assert(or_mask != NULL);
		*or_mask = 0x70000;
		*and_mask = 0xf7f7f;
		width = 2; /* CJKV expects 2 bytes -> 2 columns */
		bytes = 2;
		break;
	default:
		/* No such map.  Set up a ISO-8859-1 to UCS-4 map. */
		if (G_UNLIKELY (map == NULL)) {
			for (i = 0; i < G_N_ELEMENTS(_vte_iso2022_map_NUL); i++) {
				_vte_iso2022_map_NUL[i].from = (i & 0xff);
				_vte_iso2022_map_NUL[i].to = (i & 0xff);
			}
			map = _vte_iso2022_map_init(_vte_iso2022_map_NUL,
					    G_N_ELEMENTS(_vte_iso2022_map_NUL));
		}
		width = 1;
		bytes = 1;
		break;
	}
	/* Save the new map. */
	if (G_UNLIKELY(new_map && map != NULL)) {
		_vte_tree_insert(maps, GINT_TO_POINTER(mapname), map);
	}
	/* Return. */
	if (_map) {
		*_map = map;
	}
	if (bytes_per_char) {
		*bytes_per_char = bytes;
	}
	if (force_width) {
		*force_width = width;
	}
}

gssize
_vte_iso2022_get_encoded_width(gunichar c)
{
	gssize width;
	width = (c & VTE_ISO2022_ENCODED_WIDTH_MASK) >> VTE_ISO2022_ENCODED_WIDTH_BIT_OFFSET;
	return CLAMP(width, 0, 2);
}

static gunichar
_vte_iso2022_set_encoded_width(gunichar c, gssize width)
{
	width = CLAMP(width, 0, 2);
	c &= ~(VTE_ISO2022_ENCODED_WIDTH_MASK);
	c |= (width << VTE_ISO2022_ENCODED_WIDTH_BIT_OFFSET);
	return c;
}

struct _vte_iso2022_state *
_vte_iso2022_state_new(const char *native_codeset,
		       _vte_iso2022_codeset_changed_cb_fn fn,
		       gpointer data)
{
	struct _vte_iso2022_state *state;
	state = g_slice_new0(struct _vte_iso2022_state);
	state->nrc_enabled = TRUE;
	state->current = 0;
	state->override = -1;
	state->g[0] = 'B';
	state->g[1] = '0';
	state->g[2] = 'J';
	state->g[3] = WIDE_FUDGE + 'D';
	state->codeset = native_codeset;
	state->native_codeset = state->codeset;
	if (native_codeset == NULL) {
		g_get_charset(&state->codeset);
		state->native_codeset = state->codeset;
	}
	state->utf8_codeset = "UTF-8";
	state->target_codeset = VTE_CONV_GUNICHAR_TYPE;
	_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
			"Native codeset \"%s\", currently %s\n",
			state->native_codeset, state->codeset);
	state->conv = _vte_conv_open(state->target_codeset, state->codeset);
	state->codeset_changed = fn;
	state->codeset_changed_data = data;
	state->buffer = _vte_buffer_new();
	if (state->conv == VTE_INVALID_CONV) {
		g_warning(_("Unable to convert characters from %s to %s."),
			  state->codeset, state->target_codeset);
		_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
				"Using UTF-8 instead.\n");
		state->codeset = state->utf8_codeset;
		state->conv = _vte_conv_open(state->target_codeset,
					     state->codeset);
		if (state->conv == VTE_INVALID_CONV) {
			g_error(_("Unable to convert characters from %s to %s."),
				state->codeset, state->target_codeset);
		}
	}
	state->ambiguous_width = _vte_iso2022_ambiguous_width(state);
	return state;
}

void
_vte_iso2022_state_free(struct _vte_iso2022_state *state)
{
	_vte_buffer_free(state->buffer);
	if (state->conv != VTE_INVALID_CONV) {
		_vte_conv_close(state->conv);
	}
	g_slice_free(struct _vte_iso2022_state, state);
}

void
_vte_iso2022_state_set_codeset(struct _vte_iso2022_state *state,
			       const char *codeset)
{
	VteConv conv;

	g_return_if_fail(state != NULL);
	g_return_if_fail(codeset != NULL);
	g_return_if_fail(strlen(codeset) > 0);

	_vte_debug_print(VTE_DEBUG_SUBSTITUTION, "%s\n", codeset);
	conv = _vte_conv_open(state->target_codeset, codeset);
	if (conv == VTE_INVALID_CONV) {
		g_warning(_("Unable to convert characters from %s to %s."),
			  codeset, state->target_codeset);
		return;
	}
	if (state->conv != VTE_INVALID_CONV) {
		_vte_conv_close(state->conv);
	}
	state->codeset = g_intern_string (codeset);
	state->conv = conv;
	state->ambiguous_width = _vte_iso2022_ambiguous_width(state);
}

const char *
_vte_iso2022_state_get_codeset(struct _vte_iso2022_state *state)
{
	return state->codeset;
}

static const guchar *
_vte_iso2022_find_nextctl(const guchar *p, const guchar * const q)
{
	do {
		switch (*p) {
			case '\033':
			case '\n':
			case '\r':
			case '\016':
			case '\017':
#ifdef VTE_ISO2022_8_BIT_CONTROLS
		    /* This breaks UTF-8 and other encodings which
		     * use the high bits.
		     */
			case '0x8e':
			case '0x8f':
#endif
				return p;
		}
	} while (++p < q);
	return NULL;
}

static long
_vte_iso2022_sequence_length(const unsigned char *nextctl, gsize length)
{
	long sequence_length = -1;
	gsize i;

	switch (nextctl[0]) {
	case '\n':
	case '\r':
	case '\016':
	case '\017':
		/* LF */
		/* CR */
		/* SO */
		/* SI */
		sequence_length = 1;
		break;
	case 0x8e:
	case 0x8f:
		/* SS2 - 8bit */
		/* SS3 - 8bit */
		sequence_length = 1;
		break;
	case '\033':
		if (length < 2) {
			/* Inconclusive. */
			sequence_length = 0;
		} else {
			switch (nextctl[1]) {
			case '[':
				/* ESC [, the CSI.  The first letter
				 * is the end of the sequence, */
				for (i = 2; i < length; i++) {
					if (g_unichar_isalpha(nextctl[i])) {
						break;
					}
					if ((nextctl[i] == '@') ||
					    (nextctl[i] == '`') ||
					    (nextctl[i] == '{') ||
					    (nextctl[i] == '|')) {
						break;
					}
				}
				if (i < length) {
					/* Return the length of this
					 * sequence. */
					sequence_length = i + 1;
				} else {
					/* Inconclusive. */
					sequence_length = 0;
				}
				break;
#if 0
			case ']':
				/* ESC ], the OSC.  Search for a string
				 * terminator or a BEL. */
				for (i = 2; i < q - nextctl - 1; i++) {
					if ((nextctl[i] == '\033') &&
					    (nextctl[i + 1] == '\\')) {
						break;
					}
				}
				if (i < length - 1) {
					/* Return the length of this
					 * sequence. */
					sequence_length = i + 1;
				} else {
					for (i = 2; i < length; i++) {
						if (nextctl[i] == '\007') {
							break;
						}
					}
					if (i < length) {
						/* Return the length of
						 * this sequence. */
						sequence_length = i + 1;
					} else {
						/* Inconclusive. */
						sequence_length = 0;
					}
				}
				break;
#endif
#if 0
			case '^':
				/* ESC ^, the PM.  Search for a string
				 * terminator. */
#endif
			case 'P':
				/* ESC P, the DCS.  Search for a string
				 * terminator. */
				for (i = 2; i < length - 1; i++) {
					if ((nextctl[i] == '\033') &&
					    (nextctl[i + 1] == '\\')) {
						break;
					}
				}
				if (i < length - 1) {
					/* Return the length of this
					 * sequence. */
					sequence_length = i + 1;
				} else {
					/* Inconclusive. */
					sequence_length = 0;
				}
				break;
			case 'N':
			case 'O':
			case 'n':
			case 'o':
				/* ESC N */
				/* ESC O */
				/* ESC n */
				/* ESC o */
				sequence_length = 2;
				break;
			case '(':
			case ')':
			case '*':
			case '+':
				if (length < 3) {
					/* Inconclusive. */
					sequence_length = 0;
				} else {
					/* ESC ) x */
					/* ESC ( x */
					/* ESC * x */
					/* ESC + x */
					/* Just accept whatever. */
					sequence_length = 3;
				}
				break;
			case '%':
				if (length < 3) {
					/* Inconclusive. */
					sequence_length = 0;
				} else {
					/* ESC % @ */
					/* ESC % G */
					switch (nextctl[2]) {
					case '@':
					case 'G':
						sequence_length = 3;
						break;
					default:
						break;
					}
				}
				break;
			case '$':
				if (length < 3) {
					/* Inconclusive. */
					sequence_length = 0;
				} else {
					switch (nextctl[2]) {
					case '@':
					case 'B':
						/* ESC $ @ */
						/* ESC $ B */
						sequence_length = 3;
						break;
					case '(':
					case ')':
					case '*':
					case '+':
						/* ESC $ ( x */
						/* ESC $ ) x */
						/* ESC $ * x */
						/* ESC $ + x */
						if (length < 4) {
							/* Inconclusive. */
							sequence_length = 0;
						} else {
							/* strchr(WIDE_GMAPS, nextctl[3]) */
							switch (nextctl[3]) {
							case 'C':
							case 'A':
							case 'G':
							case 'H':
							case 'I':
							case 'J':
							case 'K':
							case 'L':
							case 'M':
							case 'D':
								sequence_length = 4;
								break;
							default:
								break;
							}
						}
						break;
					}
				}
				break;
			default:
				break;
			}
		}
		break;
	}
	return sequence_length;
}

static int
process_8_bit_sequence(struct _vte_iso2022_state *state,
		       const guchar **inbuf, gsize *inbytes,
		       gunichar **outbuf, gsize *outbytes)
{
	guint i, width;
	gpointer p;
	gunichar c, *outptr;
	const guchar *inptr;
	gulong acc, or_mask, and_mask;
	GHashTable *map;
	guint bytes_per_char, force_width, current;

	/* Check if it's an 8-bit escape.  If it is, take a note of which map
	 * it's for, and if it isn't, fail. */
	current = 0;
	switch (**inbuf) {
	case 0x8e:
		current = 2;
		break;
	case 0x8f:
		current = 3;
		break;
	default:
		/* We processed 0 bytes, and we have no intention of looking
		 * at this byte again. */
		return 0;
	}

	inptr = *inbuf;
	outptr = *outbuf;

	/* Find the map, and ensure that in addition to the escape byte, we
	 * have enough information to construct the character. */
	_vte_iso2022_map_get(state->g[current],
			     &map, &bytes_per_char, &force_width,
			     &or_mask, &and_mask);
	if (*inbytes < (bytes_per_char + 1)) {
		/* We need more information to work with. */
		return -1;
	}

	for (acc = 0, i = 0; i < bytes_per_char; i++) {
		acc = (acc << 8) | ((guint8*)inptr)[i + 1];
	}
	*inbuf += (bytes_per_char + 1);
	*inbytes -= (bytes_per_char + 1);

	acc &= and_mask;
	acc |= or_mask;
	p = GINT_TO_POINTER(acc);
	c = GPOINTER_TO_INT(g_hash_table_lookup(map, p));
	if ((c == 0) && (acc != 0)) {
		_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
				"%04lx -(%c)-> %04lx(?)\n",
				acc, state->g[current] & 0xff, acc);
	} else {
		width = 0;
		if (force_width != 0) {
			width = force_width;
		} else {
			if (G_UNLIKELY (_vte_iso2022_is_ambiguous(c))) {
				width = state->ambiguous_width;
			}
		}
		_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
				"%05lx -> " "%04x\n", acc, c);
		c = _vte_iso2022_set_encoded_width(c, width);
	}
	/* Save the unichar. */
	g_assert(*outbytes >= sizeof(c));
	*outbytes -= sizeof(c);
	*outptr++ = c;
	*outbuf = outptr;
	/* Return the number of input bytes consumed. */
	return bytes_per_char + 1;
}

static glong
process_cdata(struct _vte_iso2022_state *state, const guchar *cdata, gsize length,
	      GArray *gunichars)
{
	int ambiguous_width;
	glong processed = 0;
	GHashTable *map;
	guint bytes_per_char, force_width, current;
	gsize converted;
	const guchar *inbuf;
	gunichar *outbuf, *buf;
	gsize inbytes, outbytes;
	guint i, j, width;
	gulong acc, or_mask, and_mask;
	gunichar c;
	gboolean single, stop;

	ambiguous_width = state->ambiguous_width;

	single = (state->override != -1);
	current = single ? state->override : state->current;
	state->override = -1;
	g_assert(current < G_N_ELEMENTS(state->g));

	_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
			"Current map = %d (%c).\n",
			current, (state->g[current] & 0xff));

	if (!state->nrc_enabled || (state->g[current] == 'B')) {
		inbuf = cdata;
		inbytes = length;
		_vte_buffer_set_minimum_size(state->buffer,
					     sizeof(gunichar) * length * 2);
		buf = (gunichar *)state->buffer->bytes;
		outbuf = buf;
		outbytes = sizeof(gunichar) * length * 2;
		do {
			converted = _vte_conv_cu(state->conv,
					         &inbuf, &inbytes,
					         &outbuf, &outbytes);
			stop = FALSE;
			switch (converted) {
			case ((gsize)-1):
				switch (errno) {
				case EILSEQ:
					/* Check if it's an 8-bit sequence. */
					i = process_8_bit_sequence(state,
								   &inbuf,
								   &inbytes,
								   &outbuf,
								   &outbytes);
					switch (i) {
					case 0:
						/* Nope, munge the input. */
						inbuf++;
						inbytes--;
						*outbuf++ = INVALID_CODEPOINT;
						outbytes -= sizeof(gunichar);
						break;
					case -1:
						/* Looks good so far, try again
						 * later. */
						stop = TRUE;
						break;
					default:
						/* Processed n bytes, just keep
						 * going. */
						break;
					}
					break;
				case EINVAL:
					/* Incomplete. Save for later. */
					stop = TRUE;
					break;
				case E2BIG:
					/* Should never happen. */
					g_assert_not_reached();
					break;
				default:
					/* Should never happen. */
					g_assert_not_reached();
					break;
				}
			default:
				break;
			}
		} while ((inbytes > 0) && !stop);

		/* encode any ambiguous widths and skip blanks */
		for (i = j = 0; buf + i < outbuf; i++) {
			c = buf[i];
			if (G_UNLIKELY (c == '\0')) {
				/* Skip the padding character. */
				continue;
			}
			if (G_UNLIKELY (_vte_iso2022_is_ambiguous(c))) {
				width = ambiguous_width;
				c = _vte_iso2022_set_encoded_width(c, width);
			}
			buf[j++] = c;
		}
		/* And append the unichars to the GArray. */
		g_array_append_vals(gunichars, buf, j);

		/* Done. */
		processed = length - inbytes;
	} else {
		_vte_iso2022_map_get(state->g[current],
				     &map, &bytes_per_char, &force_width,
				     &or_mask, &and_mask);
		i = 0;
		acc = 0;
		do {
			if (i < length) {
				acc = (acc << 8) | cdata[i];
			}
			i++;
			if ((i % bytes_per_char) == 0) {
				acc &= and_mask;
				acc |= or_mask;
				c = GPOINTER_TO_INT(g_hash_table_lookup(map,
							GINT_TO_POINTER(acc)));
				if ((c == 0) && (acc != 0)) {
					_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
							"%04lx -(%c)-> "
							"%04lx(?)\n",
							acc,
							state->g[current] & 0xff,
							acc);
					c = acc;
				} else {
					width = 0;
					if (force_width != 0) {
						width = force_width;
					} else {
						if (G_UNLIKELY (_vte_iso2022_is_ambiguous(c))) {
							width = ambiguous_width;
						}
					}
					_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
							"%05lx -> "
							"%04x\n", acc, c);
					c = _vte_iso2022_set_encoded_width(c, width);
				}
				g_array_append_val(gunichars, c);
				if (single) {
					break;
				}
				acc = 0;
			}
		} while (i < length);
		processed = i;
	}
	return processed;
}

gunichar
_vte_iso2022_process_single(struct _vte_iso2022_state *state,
			    gunichar c, gunichar map)
{
	GHashTable *hash;
	gunichar ret = c;
	gpointer p;
	guint bytes_per_char, force_width;
	gulong or_mask, and_mask;

	_vte_iso2022_map_get(map,
			     &hash, &bytes_per_char, &force_width,
			     &or_mask, &and_mask);

	p = GINT_TO_POINTER((c & and_mask) | or_mask);
	if (hash != NULL) {
		p = g_hash_table_lookup(hash, p);
	}
	if (p != NULL) {
		ret = GPOINTER_TO_INT(p);
	}
	if (force_width) {
		ret = _vte_iso2022_set_encoded_width(ret, force_width);
	}
	return ret;
}

static void
process_control(struct _vte_iso2022_state *state, guchar *ctl, gsize length,
		GArray *gunichars)
{
	gunichar c;
	gsize i;
	if (length >= 1) {
		switch (ctl[0]) {
		case '\r':  /* CR */
			c = '\r';
			g_array_append_val(gunichars, c);
			_vte_debug_print(VTE_DEBUG_SUBSTITUTION, "\tCR\n");
			break;
		case '\n':  /* LF */
			c = '\n';
			g_array_append_val(gunichars, c);
			_vte_debug_print(VTE_DEBUG_SUBSTITUTION, "\tLF\n");
			break;
		case '\016': /* SO */
			state->current = 1;
			state->override = -1;
			_vte_debug_print(VTE_DEBUG_SUBSTITUTION, "\tSO (^N)\n");
			break;
		case '\017': /* SI */
			state->current = 0;
			state->override = -1;
			_vte_debug_print(VTE_DEBUG_SUBSTITUTION, "\tSI (^O)\n");
			break;
		case 0x8e:
			/* SS2 - 8bit */
			state->override = 2;
			_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
					"\tSS2 (8-bit)\n");
			break;
		case 0x8f:
			/* SS3 - 8bit */
			state->override = 3;
			_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
				"\tSS3 (8-bit)\n");
			break;
		case '\033':
			if (length >= 2) {
				switch (ctl[1]) {
				case '[': /* CSI */
				case ']': /* OSC */
				case '^': /* PM */
				case 'P': /* DCS */
					/* Pass it through. */
					for (i = 0; i < length; i++) {
						c = (guchar) ctl[i];
						g_array_append_val(gunichars,
								   c);
					}
					_VTE_DEBUG_IF(VTE_DEBUG_SUBSTITUTION) {
						g_printerr("\t");
						for (i = 0; i < length; i++) {
							c = (guchar) ctl[i];
							g_printerr(
								"(%s%c)",
								c < 0x20 ?
								"^" : "",
								c < 0x20 ?
								c : c + 64);
						}
						g_printerr("\n");
					}
					break;
				case 'N': /* SS2 */
					state->override = 2;
					_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
						"\tSS2\n");
					break;
				case 'O': /* SS3 */
					state->override = 3;
					_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
						"\tSS3\n");
					break;
				case 'n': /* LS2 */
					state->current = 2;
					state->override = -1;
					_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
							"\tLS2\n");
					break;
				case 'o': /* LS3 */
					state->current = 3;
					state->override = -1;
					_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
							"\tLS3\n");
					break;
				case '(':
				case ')':
				case '*':
				case '+':
					if (length >= 3) {
						int g = -1;
						switch (ctl[1]) {
						case '(':
							g = 0;
							break;
						case ')':
							g = 1;
							break;
						case '*':
							g = 2;
							break;
						case '+':
							g = 3;
							break;
						default:
							g_assert_not_reached();
							break;
						}
						/* strchr(NARROW_MAPS, c) */
						switch (ctl[2]) {
						case '0':
						case '1':
						case '2':
						case 'A':
						case 'B':
						case '4':
						case 'C':
						case '5':
						case 'R':
						case 'Q':
						case 'K':
						case 'Y':
						case 'E':
						case '6':
						case 'Z':
						case 'H':
						case '7':
						case '=':
						case 'J':
						case 'U':
							state->g[g] = ctl[2];
							break;

						default:
							g_warning(_("Attempt to set invalid NRC map '%c'."), ctl[2]);
							break;
						}
						_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
								"\tG[%d] = %c.\n",
								g, c);
					}
					break;
				case '%':
					if (length >= 3) {
						gboolean notify = FALSE;
						switch (ctl[2]) {
						case '@':
							if (strcmp(state->codeset, state->native_codeset) != 0) {
								notify = TRUE;
							}
							_vte_iso2022_state_set_codeset(state, state->native_codeset);
							_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
									"\tNative encoding.\n");
							break;
						case 'G':
							if (strcmp(state->codeset, state->utf8_codeset) != 0) {
								notify = TRUE;
							}
							_vte_iso2022_state_set_codeset(state, state->utf8_codeset);
							_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
									"\tUTF-8 encoding.\n");
							break;
						default:
							/* Application signalled an "identified coding system" we haven't heard of.  See ECMA-35 for gory details. */
							g_warning(_("Unrecognized identified coding system."));
							break;
						}
						if (notify &&
						    state->codeset_changed) {
							state->codeset_changed(state, state->codeset_changed_data);
						}
					}
					break;
				case '$':
					if (length >= 4) {
						int g = -1;
						c = 0;
						switch (ctl[2]) {
						case '@':
							g = 0;
							c = '@';
							break;
						case 'B':
							g = 0;
							c = 'B';
							break;
						case '(':
							g = 0;
							break;
						case ')':
							g = 1;
							break;
						case '*':
							g = 2;
							break;
						case '+':
							g = 3;
							break;
						default:
							g_assert_not_reached();
							break;
						}
						if (c == 0) {
							c = ctl[3];
						}
						/* strchr(WIDE_MAPS WIDE_GMAPS, c) */
						switch (c) {
						case '@':
						case 'B':
						case 'C':
						case 'A':
						case 'G':
						case 'H':
						case 'I':
						case 'J':
						case 'K':
						case 'L':
						case 'M':
						case 'D':
							state->g[g] = c + WIDE_FUDGE;
							break;

						default:
							g_warning(_("Attempt to set invalid wide NRC map '%c'."), c);
							break;
						}
						_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
								"\tG[%d] = wide %c.\n",
								g, c);
					} else
					if (length >= 3) {
						switch (ctl[2]) {
						case '@':
							c = '@';
							break;
						case 'B':
							c = 'B';
							break;
						default:
							c = ctl[2];
							break;
						}
						/* strchr(WIDE_MAPS, c) */
						switch (c){
						case '@':
						case 'B':
							state->g[0] = c + WIDE_FUDGE;
							break;

						default:
							g_warning(_("Attempt to set invalid wide NRC map '%c'."), c);
						}
						_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
								"\tG[0] = wide %c.\n",
								c);
					}
					break;
				default:
					g_assert_not_reached();
					break;
				}
				break;
			}
			break;
		default:
			g_assert_not_reached();
			break;
		}
	}
}

static guint
process_block (struct _vte_iso2022_state *state,
	       guchar *input,
	       struct _vte_iso2022_block *block,
	       gboolean last,
	       GArray *gunichars)
{
	guint preserve_last = -1;
	guint initial;

	switch (block->type) {
	case _vte_iso2022_cdata:
		_VTE_DEBUG_IF(VTE_DEBUG_SUBSTITUTION) {
			guint j;
			g_printerr("%3ld %3ld CDATA \"%.*s\"",
				block->start, block->end,
				(int) (block->end - block->start),
				input + block->start);
			g_printerr(" (");
			for (j = block->start; j < block->end; j++) {
				if (j > block->start) {
					g_printerr(", ");
				}
				g_printerr("0x%02x",
					input[j]);
			}
			g_printerr(")\n");
		}
		initial = 0;
		while (initial < block->end - block->start) {
			int j;
			j = process_cdata(state,
					  input +
					  block->start +
					  initial,
					  block->end -
					  block->start -
					  initial,
					  gunichars);
			if (j == 0) {
				break;
			}
			initial += j;
		}
		if (initial < block->end - block->start && last) {
			preserve_last = block->start + initial;
		}
		break;
	case _vte_iso2022_control:
		_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
				"%3ld %3ld CONTROL ",
				block->start, block->end);
		process_control(state,
				input + block->start,
				block->end - block->start,
				gunichars);
		break;
	case _vte_iso2022_preserve:
		_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
				"%3ld %3ld PRESERVE\n",
				block->start, block->end);
		preserve_last = block->start;
		break;
	default:
		g_assert_not_reached();
		break;
	}

	return preserve_last;
}

gsize
_vte_iso2022_process(struct _vte_iso2022_state *state,
		     guchar *input, gsize length,
		     GArray *gunichars)
{
	struct _vte_iso2022_block block;
	guint preserve_last = -1;
	const guchar *nextctl, *p, *q;
	glong sequence_length = 0;

	p = input;
	q = input + length;
	do {
		nextctl = _vte_iso2022_find_nextctl(p, q);
		if (nextctl == NULL) {
			/* It's all garden-variety data. */
			block.type = _vte_iso2022_cdata;
			block.start = p - input;
			block.end = q - input;
			preserve_last = process_block (state,
					               input, &block,
						       TRUE,
					               gunichars);
			break;
		}
		/* We got some garden-variety data. */
		if (nextctl != p) {
			block.type = _vte_iso2022_cdata;
			block.start = p - input;
			block.end = nextctl - input;
			process_block (state, input, &block, FALSE, gunichars);
		}
		/* Move on to the control data. */
		p = nextctl;
		sequence_length = _vte_iso2022_sequence_length(nextctl,
							       q - nextctl);
		switch (sequence_length) {
		case -1:
			/* It's just garden-variety data. */
			block.type = _vte_iso2022_cdata;
			block.start = p - input;
			block.end = nextctl + 1 - input;
			/* Continue at the next byte. */
			p = nextctl + 1;
			break;
		case 0:
			/* Inconclusive.  Save this data and try again later. */
			block.type = _vte_iso2022_preserve;
			block.start = nextctl - input;
			block.end = q - input;
			/* Trigger an end-of-loop. */
			p = q;
			break;
		default:
			/* It's a control sequence. */
			block.type = _vte_iso2022_control;
			block.start = nextctl - input;
			block.end = nextctl + sequence_length - input;
			/* Continue after the sequence. */
			p = nextctl + sequence_length;
			break;
		}
		preserve_last = process_block (state,
				               input, &block,
					       FALSE,
					       gunichars);
	} while (p < q);
	if (preserve_last != (guint) -1) {
		length = preserve_last;
	}
	_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
			"Consuming %ld bytes.\n", (long) length);
	return length;
}

gssize
_vte_iso2022_unichar_width(gunichar c)
{
	c = c & ~(VTE_ISO2022_ENCODED_WIDTH_MASK); /* just in case */
	if (G_UNLIKELY (_vte_iso2022_is_ambiguous(c))) {
		return _vte_iso2022_ambiguous_width_guess();
	}
	if (g_unichar_iswide(c)) {
		return 2;
	}
	return 1;
}

#ifdef ISO2022_MAIN
#include <stdio.h>
int
main(int argc, char **argv)
{
	struct _vte_buffer *buffer;
	struct _vte_iso2022_state *state;
	GString *string;
	GArray *gunichars;
	struct {
		const char *s;
		gboolean process;
	} strings[] = {
		{"abcd\033$(Cefgh\ri\nj\033)0k\017lmn\033Nopqrst\033%G", TRUE},
		{"ABCD\033$(C\033)", TRUE},
		{"0", TRUE},
		{"\014\033[33;41m", TRUE},
		{"\015", TRUE},
		{"\014{|}\015\r\n", TRUE},
		{"\033(B\033)0\033*B\033+B", TRUE},
		{"\033$B$+$J4A;z\033(J~", TRUE},
		{"\033(B\033)0\033*B\033+B", TRUE},
		{"\033$)C\0161hD!\017", TRUE},
		{"\033$*C\033N1hD!", TRUE},
		{"\033$(G\043\071", TRUE},
		{"\033(B\033)0\033*B\033+B", TRUE},
		{"\r\n", TRUE},
	};
	int i;
	FILE *fp;
	guchar b;

	state = _vte_iso2022_state_new(NULL, NULL, NULL);
	buffer = _vte_buffer_new();
	gunichars = g_array_new(FALSE, FALSE, sizeof(gunichar));
	if (argc > 1) {
		string = g_string_new(NULL);
		for (i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-") == 0) {
				fp = stdin;
			} else {
				fp = fopen(argv[i], "r");
			}
			while (fread(&b, sizeof(guchar), 1, fp) == sizeof(b)) {
				g_string_append_c(string, b);
			}
			if (fp != stdin) {
				fclose(fp);
			}
		}
		_vte_buffer_append(buffer, string->str, string->len);
		_vte_iso2022_process(state, buffer, gunichars);
		g_string_free(string, TRUE);
	} else {
		for (i = 0; i < G_N_ELEMENTS(strings); i++) {
			string = g_string_new(strings[i].s);
			_vte_buffer_append(buffer, string->str, string->len);
			g_string_free(string, TRUE);
			if (strings[i].process) {
				_vte_iso2022_process(state, buffer, gunichars);
			}
		}
	}
	_vte_buffer_free(buffer);
	_vte_iso2022_state_free(state);

	string = g_string_new(NULL);
	for (i = 0; i < gunichars->len; i++) {
		g_string_append_unichar(string,
					g_array_index(gunichars, gunichar, i));
	}
	write(STDOUT_FILENO, string->str, string->len);
	g_string_free(string, TRUE);

	return 0;
}
#endif
