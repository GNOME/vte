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

#include "config.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include "debug.h"
#include "table.h"
#include "iso2022.h"

/* Maps which jive with XTerm's ESC ()*+ ? sequences and RFC 1468. */
#define NARROW_MAPS	"0AB4C5RQKYE6ZH7=" "J"
/* Maps which jive with RFC 1468's ESC $ ? sequences. */
#define WIDE_MAPS	"@B"
/* Maps which jive with RFC 1557/1922/2237's ESC $ ()*+ ? sequences. */
#define WIDE_GMAPS	"C" "AGH" "D"
/* Fudge factor we add to wide map identifiers to keep them distinct. */
#define WIDE_FUDGE	0x10000

struct _vte_iso2022_map {
	gunichar from, to;
};

struct _vte_iso2022 {
	unsigned int current, override;
	gboolean ss2, ss3;
	gunichar g[4];
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
	{'f', GDK_degree},	/* degree */
	{'g', GDK_plusminus},	/* plus/minus */
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
	{'v', 0x252c},	/* down t */
	{'w', 0x2534},	/* up t */
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
};
/* Dutch. VT220 and higher (per XTerm docs). */
static const struct _vte_iso2022_map _vte_iso2022_map_4[] = {
	{'#',  GDK_sterling},
	{'@',  GDK_threequarters},
	{'[',  GDK_ydiaeresis},
	{'\\', GDK_onehalf},
	{']',  GDK_bar},
	{'{',  GDK_diaeresis},
	{'|',  0x192}, /* f with hook (florin) */
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
/* Japanese.  JIS X 0201-1976 ("Roman" set), per RFC 1468/2237. */
static const struct _vte_iso2022_map _vte_iso2022_map_J[] = {
	{'\\', 0x203e},
	{'~',  GDK_yen},
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
/* Korean.  KSC 5601, per RFC 1557. */
static const struct _vte_iso2022_map _vte_iso2022_map_wide_C[] = {
#include "unitable.KSC5601"
};
/* Japanese.  JIS X 0212-1990, per RFC 2237. */
static const struct _vte_iso2022_map _vte_iso2022_map_wide_D[] = {
#include "unitable.JIS0212"
};
/* Chinese.  CNS 11643-plane-1, per RFC 1922. */
static const struct _vte_iso2022_map _vte_iso2022_map_wide_G[] = {
#include "unitable.CNS11643"
};
/* Chinese.  CNS 11643-plane-2, per RFC 1922. */
static const struct _vte_iso2022_map _vte_iso2022_map_wide_H[] = {
#include "unitable.CNS11643"
};

struct _vte_iso2022 *
_vte_iso2022_new(void)
{
	struct _vte_iso2022 *ret = NULL;
	ret = g_malloc0(sizeof(struct _vte_iso2022));
	ret->current = 0;
	ret->override = 0;
	ret->ss2 = FALSE;
	ret->ss3 = FALSE;
	ret->g[0] = 'B';
	ret->g[1] = '0';
	ret->g[2] = 'B';
	ret->g[3] = 'B';
	return ret;
}

struct _vte_iso2022 *
_vte_iso2022_copy(struct _vte_iso2022 *original)
{
	struct _vte_iso2022 *ret;
	ret = _vte_iso2022_new();
	*ret = *original;
	return ret;
}

void
_vte_iso2022_free(struct _vte_iso2022 *p)
{
	p->current = 0;
	p->override = 0;
	p->ss2 = FALSE;
	p->ss3 = FALSE;
	p->g[0] = '\0';
	p->g[1] = '\0';
	p->g[2] = '\0';
	p->g[3] = '\0';
	g_free(p);
}

static gint
_vte_direct_compare(gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

static GTree *
_vte_iso2022_map_init(const struct _vte_iso2022_map *map, gssize length)
{
	GTree *ret;
	int i;
	if (length == 0) {
		return NULL;
	}
	ret = g_tree_new(_vte_direct_compare);
	for (i = 0; i < length; i++) {
		g_tree_insert(ret,
			      GINT_TO_POINTER(map[i].from),
			      GINT_TO_POINTER(map[i].to));
	}
	return ret;
}

static GTree *
_vte_iso2022_map_get(gunichar mapname)
{
	static GTree *maps = NULL, *ret;
	/* No conversions for ASCII. */
	if (mapname == 'B') {
		return NULL;
	}
	/* Make sure we have a map, erm, map. */
	if (maps == NULL) {
		maps = g_tree_new(_vte_direct_compare);
	}
	/* Check for a cached map for this charset. */
	ret = g_tree_lookup(maps, GINT_TO_POINTER(mapname));
	if (ret == NULL) {
		/* Construct a new one. */
		switch (mapname) {
		case '0':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_0,
						    G_N_ELEMENTS(_vte_iso2022_map_0));
			break;
		case 'A':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_A,
						    G_N_ELEMENTS(_vte_iso2022_map_A));
			break;
		case 'B':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_B,
						    G_N_ELEMENTS(_vte_iso2022_map_B));
			break;
		case '4':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_4,
						    G_N_ELEMENTS(_vte_iso2022_map_4));
			break;
		case 'C':
		case '5':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_C,
						    G_N_ELEMENTS(_vte_iso2022_map_C));
			break;
		case 'R':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_R,
						    G_N_ELEMENTS(_vte_iso2022_map_R));
			break;
		case 'Q':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_Q,
						    G_N_ELEMENTS(_vte_iso2022_map_Q));
			break;
		case 'K':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_K,
						    G_N_ELEMENTS(_vte_iso2022_map_K));
			break;
		case 'Y':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_Y,
						    G_N_ELEMENTS(_vte_iso2022_map_Y));
			break;
		case 'E':
		case '6':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_E,
						    G_N_ELEMENTS(_vte_iso2022_map_E));
			break;
		case 'Z':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_Z,
						    G_N_ELEMENTS(_vte_iso2022_map_Z));
			break;
		case 'H':
		case '7':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_H,
						    G_N_ELEMENTS(_vte_iso2022_map_H));
			break;
		case '=':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_equal,
						    G_N_ELEMENTS(_vte_iso2022_map_equal));
			break;
		case 'J':
			ret = _vte_iso2022_map_init(_vte_iso2022_map_J,
						    G_N_ELEMENTS(_vte_iso2022_map_J));
			break;
		case '@' + WIDE_FUDGE:
			ret = _vte_iso2022_map_init(_vte_iso2022_map_wide_at,
						    G_N_ELEMENTS(_vte_iso2022_map_wide_at));
			break;
		case 'A' + WIDE_FUDGE:
			ret = _vte_iso2022_map_init(_vte_iso2022_map_wide_A,
						    G_N_ELEMENTS(_vte_iso2022_map_wide_A));
			break;
		case 'B' + WIDE_FUDGE:
			ret = _vte_iso2022_map_init(_vte_iso2022_map_wide_B,
						    G_N_ELEMENTS(_vte_iso2022_map_wide_B));
			break;
		case 'C' + WIDE_FUDGE:
			ret = _vte_iso2022_map_init(_vte_iso2022_map_wide_C,
						    G_N_ELEMENTS(_vte_iso2022_map_wide_C));
			break;
		case 'D' + WIDE_FUDGE:
			ret = _vte_iso2022_map_init(_vte_iso2022_map_wide_D,
						    G_N_ELEMENTS(_vte_iso2022_map_wide_D));
			break;
		case 'G' + WIDE_FUDGE:
			ret = _vte_iso2022_map_init(_vte_iso2022_map_wide_G,
						    G_N_ELEMENTS(_vte_iso2022_map_wide_G));
			break;
		case 'H' + WIDE_FUDGE:
			ret = _vte_iso2022_map_init(_vte_iso2022_map_wide_H,
						    G_N_ELEMENTS(_vte_iso2022_map_wide_H));
			break;
		default:
			ret = NULL;
			break;
		}
		/* Save the new map. */
		if (ret != NULL) {
			g_tree_insert(maps, GINT_TO_POINTER(mapname), ret);
		}
	}
	return ret;
}

gssize
_vte_iso2022_substitute(struct _vte_iso2022 *outside_state,
		       gunichar *instring, gssize length,
		       gunichar *outstring, struct _vte_table *specials)
{
	int i, j, k, g;
	struct _vte_iso2022 state;
	GTree *charmap = NULL;
	gpointer ptr;
	gunichar *buf, current_map = '\0', last_map = '\0', result;
	unsigned int accumulator;
	const char *match;
	const gunichar *used;
	int chars_per_code = 1;

	g_return_val_if_fail(outside_state != NULL, 0);
	g_return_val_if_fail(instring != NULL, 0);
	g_return_val_if_fail(outstring != NULL, 0);
	g_return_val_if_fail(length != 0, 0);

	buf = g_malloc(sizeof(gunichar) * length);
	state = *outside_state;

	for (i = j = 0; i < length; i++) {
		if ((specials != NULL) &&
		    (_vte_table_match(specials, instring + i, length - i,
				      &match, &used, NULL, NULL) != NULL)) {
			if (strlen(match) > 0) {
				/* Aaargh, SI/SO masquerade as capabilities. */
				if ((strcmp(match, "as") != 0) &&
				    (strcmp(match, "ae") != 0)) {
					memcpy(buf + j, instring + i,
					       sizeof(gunichar) *
					       (used - (instring + i)));
					j += (used - (instring + i));
					i = (used - instring) - 1;
					continue;
				}
			} else {
				g_free(buf);
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
					fprintf(stderr,
						"Incomplete sequence: "
						"need %d bytes,\n", length);
				}
#endif
				return -1;
			}
		}
		switch (instring[i]) {
		case '':
			/* SO/LS1 */
			state.current = 1;
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
				fprintf(stderr, "SO/LS1.\n");
			}
#endif
			continue;
			break;
		case '':
			/* SI/LS0 */
			state.current = 0;
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
				fprintf(stderr, "SI/LS0.\n");
			}
#endif
			continue;
			break;
		case '\r':
		case '\n':
			/* Reset overrides. */
			state.override = '\0';
			goto plain;
		case '':
			/* Reset overrides. */
			state.override = '\0';
			/* Begins a control sequence.  Make sure there's another
			 * character for us to read. */
			if (i + 1 >= length) {
				g_free(buf);
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
					fprintf(stderr,
						"Incomplete specifier: "
						"need %d bytes, have "
						"%d.\n", 2, length - i);
				}
#endif
				return -1;
			}
			switch (instring[i + 1]) {
			case '(':	/* Designate G0/GL. */
			case ')':	/* Designate G1/GR. */
			case '*':	/* Designate G2. */
			case '+':	/* Designate G3. */
				g = -1;
				if (instring[i + 1] == '(') {
					g = 0;
				} else
				if (instring[i + 1] == ')') {
					g = 1;
				} else
				if (instring[i + 1] == '*') {
					g = 2;
				} else
				if (instring[i + 1] == '+') {
					g = 3;
				} else {
					g_assert_not_reached();
				}
				/* Designate Gx.  Must be another character here. */
				if (i + 2 >= length) {
					g_free(buf);
#ifdef VTE_DEBUG
					if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
						fprintf(stderr,
							"Incomplete specifier: "
							"need %d bytes, have "
							"%d.\n", 3, length - i);
					}
#endif
					return -1;
				}
				/* We only handle maps we recognize. */
				if (strchr(NARROW_MAPS, instring[i + 2]) == NULL) {
					goto plain;
				}
				/* Set Gx. */
				state.g[g] = instring[i + 2];
				i += 2;
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
					fprintf(stderr, "G%d set to `%c'.\n",
						g, state.g[g]);
				}
#endif
				continue;
				break;
			case '$':
				/* Designate Gx.  Must be another character here. */
				if (i + 2 >= length) {
					g_free(buf);
#ifdef VTE_DEBUG
					if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
						fprintf(stderr,
							"Incomplete specifier: "
							"need %d bytes, have "
							"%d.\n", 3, length - i);
					}
#endif
					return -1;
				}
				switch (instring[i + 2]) {
				case '(':	/* Designate G0/GL wide. */
				case ')':	/* Designate G1/GR wide. */
				case '*':	/* Designate G2 wide. */
				case '+':	/* Designate G3 wide. */
					/* Need another character here. */
					if (i + 3 >= length) {
						g_free(buf);
#ifdef VTE_DEBUG
						if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
							fprintf(stderr,
								"Incomplete specifier: "
								"need %d bytes, have "
								"%d.\n", 4, length - i);
						}
#endif
						return -1;
					}
					g = -1;
					if (instring[i + 2] == '(') {
						g = 0;
					} else
					if (instring[i + 2] == ')') {
						g = 1;
					} else
					if (instring[i + 2] == '*') {
						g = 2;
					} else
					if (instring[i + 2] == '+') {
						g = 3;
					} else {
						g_assert_not_reached();
					}
					/* We only handle maps we recognize. */
					if (strchr(WIDE_GMAPS, instring[i + 3]) == NULL) {
						goto plain;
					}
					/* Set Gx. */
					state.g[g] = instring[i + 3] + WIDE_FUDGE;
					i += 3;
#ifdef VTE_DEBUG
					if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
						fprintf(stderr,
							"G%d set to wide `%c'.\n",
							g, state.g[g] - WIDE_FUDGE);
					}
#endif
					continue;
					break;
				default:
					/* Override. */
					if (strchr(WIDE_MAPS, instring[i + 2]) == NULL) {
						goto plain;
					}
					/* Set the current map. */
					state.override = instring[i + 2] + WIDE_FUDGE;
					i += 2;
#ifdef VTE_DEBUG
					if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
						fprintf(stderr,
							"Override set to wide `%c'.\n",
							state.override - WIDE_FUDGE);
					}
#endif
					continue;
				}
				break;

			case 'n':
				/* LS2 */
				state.current = 2;
				i++;
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
					fprintf(stderr, "LS2.\n");
				}
#endif
				continue;
				break;
			case 'o':
				/* LS3 */
				state.current = 3;
				i++;
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
					fprintf(stderr, "LS3.\n");
				}
#endif
				continue;
				break;
			case 'N':
				/* SS2 */
				state.ss2 = TRUE;
				i++;
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
					fprintf(stderr, "SS2.\n");
				}
#endif
				continue;
				break;
			case 'O':
				/* SS3 */
				state.ss3 = TRUE;
				i++;
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
					fprintf(stderr, "SS3.\n");
				}
#endif
				continue;
				break;
			default:
				goto plain;
				break;
			}
		plain:
		default:
			/* Reset override maps. */
			switch (instring[i]) {
			case '\n':
			case '\r':
			case '':
				state.override = 0;
				break;
			}
			/* Determine which map we should use here. */
			if (state.override != 0) {
				current_map = state.override;
			} else
			if (state.ss2) {
				current_map = state.g[2];
				state.ss2 = FALSE;
			} else
			if (state.ss3) {
				current_map = state.g[3];
				state.ss3 = FALSE;
			} else {
				g_assert(state.current < G_N_ELEMENTS(state.g));
				current_map = state.g[state.current];
			}
			/* Build. */
			if (current_map > WIDE_FUDGE) {
				switch (current_map) {
				case '@' + WIDE_FUDGE:
				case 'A' + WIDE_FUDGE:
				case 'B' + WIDE_FUDGE:
				case 'C' + WIDE_FUDGE:
				case 'D' + WIDE_FUDGE:
					chars_per_code = 2;
					break;
				case 'G' + WIDE_FUDGE:
				case 'H' + WIDE_FUDGE:
					chars_per_code = 3;
					break;
				default:
					chars_per_code = 1;
					break;
				}
			} else {
				chars_per_code = 1;
			}
			/* We need at least this many characters. */
			if (i + chars_per_code > length) {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
					fprintf(stderr, "Incomplete multibyte sequence "
						"at %d: need %d bytes, have %d.\n",
						i, chars_per_code, length - i);
				}
#endif
				g_free(buf);
				return -1;
			}
			/* Build up the character. */
			accumulator = 0;
			for (k = 0; k < chars_per_code; k++) {
				accumulator = (accumulator << 8) | instring[i + k];
			}
			/* Load a new map if need be. */
			if (current_map != last_map) {
#ifdef VTE_DEBUG
				if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
					if (last_map == '\0') {
						fprintf(stderr,
							"Charmap set to %s`%c'.\n",
							(current_map > WIDE_FUDGE) ?
							"wide " : "",
							current_map % WIDE_FUDGE);
					} else {
						fprintf(stderr,
							"Charmap changed to %s`%c'.\n",
							(current_map > WIDE_FUDGE) ?
							"wide " : "",
							current_map % WIDE_FUDGE);
					}
				}
#endif
				charmap = _vte_iso2022_map_get(current_map);
				last_map = current_map;
			}
			/* Translate. */
			if (charmap == NULL) {
				result = accumulator;
			} else {
				ptr = GINT_TO_POINTER(accumulator);
				result = GPOINTER_TO_INT(g_tree_lookup(charmap, ptr));
				if (result == 0) {
					result = accumulator;
#ifdef VTE_DEBUG
				} else {
					if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
						if (accumulator != result) {
							fprintf(stderr,
								"0x%x -> 0x%x\n",
								accumulator, result);
						}
					}
#endif
				}
			}
			/* Store. */
			buf[j++] = result;
			accumulator = 0;
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_SUBSTITUTION) && 0) {
				fprintf(stderr, "`%c' (%d)\n", result, result);
			}
#endif
			i += (chars_per_code - 1);
			break;
		}
	}

	if (j > 0) {
		g_assert(j <= length);
		memcpy(outstring, buf, j * sizeof(gunichar));
	}
	*outside_state = state;
	g_free(buf);
	return j;
}

#ifdef ISO2022_MAIN
static void
debug_print(FILE *fp, const char *string)
{
	int i;
	for (i = 0; string[i] != '\0'; i++) {
		if (((guint8)string[i]) < 32) {
			fprintf(fp, "^%c", string[i] + 64);
		} else
		if (((guint8)string[i]) < 128) {
			fprintf(fp, "%c", string[i]);
		} else {
			fprintf(fp, "{0x%02x}", string[i]);
		}
	}
	fprintf(fp, "\n");
}
static void
debug_printu(FILE *fp, const gunichar *string, gssize length)
{
	int i;
	for (i = 0; i < length; i++) {
		if (string[i] < 32) {
			fprintf(fp, "^%c", string[i] + 64);
		} else
		if (string[i] < 128) {
			fprintf(fp, "%c", string[i]);
		} else {
			fprintf(fp, "{0x%02x}", string[i]);
		}
	}
	fprintf(fp, "\n");
}

int
main(int argc, char **argv)
{
	char *samples[] = {
		"ABC$Dx$EFG",
		"ABC(A$Dx(B$EFG",
		"ABC)A$Dx)B$EFG",
		"ABC$Dx$EFG",
		"ABCn$Dxo$EFG",
		"ABCN$DxO$EFG",
		"ABC[0m$Dx$EFG",
	};
	int i, j, length;
	gunichar *sample;
	struct _vte_iso2022 *state;

	if (getenv("VTE_DEBUG_FLAGS") != NULL) {
		_vte_debug_parse_string(getenv("VTE_DEBUG_FLAGS"));
	}

	if (argc > 1) {
		putc('', stdout);
		switch (argv[1][0]) {
		case '0':
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
			putc('(', stdout);
			putc(argv[1][0], stdout);
			break;
		case '-':
			switch (argv[1][1]) {
			case '@':
			case 'B':
				putc('$', stdout);
				putc(argv[1][1], stdout);
				break;
			case 'A':
			case 'C':
			case 'D':
			case 'G':
			case 'H':
				putc('$', stdout);
				putc('(', stdout);
				putc(argv[1][1], stdout);
				break;
			}
			break;
		}
		if (argc > 2) {
			printf("%s(B\n", argv[2]);
		}
		fflush(NULL);
		return 0;
	}

	for (i = 0; i < G_N_ELEMENTS(samples); i++) {
		length = strlen(samples[i]);
		sample = g_malloc(sizeof(gunichar) * (length + 1));
		for (j = 0; j < length; j++) {
			sample[j] = samples[i][j];
		}
		sample[j] = '\0';

#ifdef VTE_DEBUG
		if (i > 0) {
			fprintf(stderr, "\n");
		}
#endif
		state = _vte_iso2022_new();
		debug_print(stderr, samples[i]);
		length = _vte_iso2022_substitute(state, sample, j, sample, NULL);
		debug_printu(stderr, sample, length);
		_vte_iso2022_free(state);
		g_free(sample);
	}
	return 0;
}
#endif
