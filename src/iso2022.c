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
#include "iso2022.h"
#define WIDE94_FUDGE 0x100

struct vte_iso2022_map {
	gunichar from, to;
};

struct vte_iso2022 {
	int current;
	gboolean ss2, ss3;
	gunichar g[4];
};

/* DEC Special Character and Line Drawing Set.  VT100 and higher (per XTerm
 * docs). */
static struct vte_iso2022_map vte_iso2022_map_0[] = {
	{ 96, 0x25c6},	/* diamond */
	{'a', 0x2592},	/* checkerboard */
	{'f', 0x00b0},	/* degree */
	{'g', 0x00b1},	/* plus/minus */
	{'j', 0x2518},	/* downright corner */
	{'k', 0x2510},	/* upright corner */
	{'l', 0x250c},	/* upleft corner */
	{'m', 0x2514},	/* downleft corner */
	{'n', 0x253c},	/* cross */
	{'q', 0x2500},	/* horizontal line */
	{'t', 0x251c},	/* left t */
	{'u', 0x2524},	/* right t */
	{'v', 0x252c},	/* down t */
	{'w', 0x2534},	/* up t */
	{'x', 0x2502},	/* vertical line */
	{127, 0x00b7},	/* bullet */
};
/* United Kingdom.  VT100 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_A[] = {
	{'$', GDK_sterling},
};
/* US-ASCII (no conversions).  VT100 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_B[] = {
};
/* Dutch. VT220 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_4[] = {
	{0x23, GDK_sterling},
	{0x40, GDK_threequarters},
	{0x5b, GDK_ydiaeresis},
	{0x5c, GDK_onehalf},
	{0x5d, GDK_bar},
	{0x7b, GDK_diaeresis},
	{0x7c, GDK_f},
	{0x7d, GDK_onequarter},
	{0x7e, GDK_acute}
};
/* Finnish. VT220 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_C[] = {
	{0x5b, GDK_Adiaeresis},
	{0x5c, GDK_Odiaeresis},
	{0x5d, GDK_Aring},
	{0x5e, GDK_Udiaeresis},
	{0x60, GDK_eacute},
	{0x7b, GDK_adiaeresis},
	{0x7c, GDK_odiaeresis},
	{0x7d, GDK_aring},
	{0x7e, GDK_udiaeresis},
};
/* French. VT220 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_R[] = {
	{0x23, GDK_sterling},
	{0x40, GDK_agrave},
	{0x5b, GDK_degree},
	{0x5c, GDK_ccedilla},
	{0x5d, GDK_section},
	{0x7b, GDK_eacute},
	{0x7c, GDK_ugrave},
	{0x7d, GDK_egrave},
	{0x7e, GDK_diaeresis},
};
/* French Canadian. VT220 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_Q[] = {
	{0x40, GDK_agrave},
	{0x5b, GDK_acircumflex},
	{0x5c, GDK_ccedilla},
	{0x5d, GDK_ecircumflex},
	{0x5e, GDK_icircumflex},
	{0x60, GDK_ocircumflex},
	{0x7b, GDK_eacute},
	{0x7c, GDK_ugrave},
	{0x7d, GDK_egrave},
	{0x7e, GDK_ucircumflex},
};
/* German. VT220 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_K[] = {
	{0x40, GDK_section},
	{0x5b, GDK_Adiaeresis},
	{0x5c, GDK_Odiaeresis},
	{0x5d, GDK_Udiaeresis},
	{0x7b, GDK_adiaeresis},
	{0x7c, GDK_odiaeresis},
	{0x7d, GDK_udiaeresis},
	{0x7e, GDK_ssharp},
};
/* Italian. VT220 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_Y[] = {
	{0x23, GDK_sterling},
	{0x40, GDK_section},
	{0x5b, GDK_degree},
	{0x5c, GDK_ccedilla},
	{0x5d, GDK_eacute},
	{0x60, GDK_ugrave},
	{0x7b, GDK_agrave},
	{0x7c, GDK_ograve},
	{0x7d, GDK_egrave},
	{0x7e, GDK_igrave},
};
/* Norwegian and Danish. VT220 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_E[] = {
	{0x40, GDK_Adiaeresis},
	{0x5b, GDK_AE},
	{0x5c, GDK_Ooblique},
	{0x5d, GDK_Aring},
	{0x5e, GDK_Udiaeresis},
	{0x60, GDK_adiaeresis},
	{0x7b, GDK_ae},
	{0x7c, GDK_oslash},
	{0x7d, GDK_aring},
	{0x7e, GDK_udiaeresis},
};
/* Spanish. VT220 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_Z[] = {
	{0x23, GDK_sterling},
	{0x40, GDK_section},
	{0x5b, GDK_exclamdown},
	{0x5c, GDK_Ntilde},
	{0x5d, GDK_questiondown},
	{0x7b, GDK_degree},
	{0x7c, GDK_ntilde},
	{0x7d, GDK_ccedilla},
};
/* Swedish. VT220 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_H[] = {
	{0x40, GDK_Eacute},
	{0x5b, GDK_Adiaeresis},
	{0x5c, GDK_Odiaeresis},
	{0x5d, GDK_Aring},
	{0x5e, GDK_Udiaeresis},
	{0x60, GDK_eacute},
	{0x7b, GDK_adiaeresis},
	{0x7c, GDK_odiaeresis},
	{0x7d, GDK_aring},
	{0x7e, GDK_udiaeresis},
};
/* Swiss. VT220 and higher (per XTerm docs). */
static struct vte_iso2022_map vte_iso2022_map_equal[] = {
	{0x23, GDK_ugrave},
	{0x40, GDK_agrave},
	{0x5b, GDK_eacute},
	{0x5c, GDK_ccedilla},
	{0x5d, GDK_ecircumflex},
	{0x5e, GDK_icircumflex},
	{0x5f, GDK_egrave},
	{0x60, GDK_ocircumflex},
	{0x7b, GDK_adiaeresis},
	{0x7c, GDK_odiaeresis},
	{0x7d, GDK_udiaeresis},
	{0x7e, GDK_ucircumflex},
};
/* Japanese.  JIS X 0201-1976 ("Roman" set), per RFC 1468/2237. */
static struct vte_iso2022_map vte_iso2022_map_J[] = {
	{'\\', GDK_overline},
	{'~', GDK_yen},
};
/* Japanese.  JIS X 0208-1978, per RFC 1468/2237. */
static struct vte_iso2022_map vte_iso2022_map_dollar_at[] = {
};
/* Japanese.  JIS X 0208-1983, per RFC 1468/2237. */
static struct vte_iso2022_map vte_iso2022_map_dollar_B[] = {
};

struct vte_iso2022 *
vte_iso2022_new(void)
{
	struct vte_iso2022 *ret = NULL;
	ret = g_malloc0(sizeof(struct vte_iso2022));
	ret->current = 0;
	ret->ss2 = FALSE;
	ret->ss3 = FALSE;
	ret->g[0] = 'B';
	ret->g[1] = '0';
	ret->g[2] = 'B';
	ret->g[3] = 'B';
}

struct vte_iso2022 *
vte_iso2022_copy(struct vte_iso2022 *original)
{
	struct vte_iso2022 *ret;
	ret = vte_iso2022_new();
	*ret = *original;
	return ret;
}

void
vte_iso2022_free(struct vte_iso2022 *p)
{
	p->current = 0;
	p->ss2 = FALSE;
	p->ss3 = FALSE;
	p->g[0] = '\0';
	p->g[1] = '\0';
	p->g[2] = '\0';
	p->g[3] = '\0';
	g_free(p);
}

static gint
vte_direct_compare(gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

static GTree *
vte_iso2022_map_init(struct vte_iso2022_map *map, gssize length)
{
	GTree *ret;
	int i;
	if (length == 0) {
		return NULL;
	}
	ret = g_tree_new(vte_direct_compare);
	for (i = 0; i < length; i++) {
		g_tree_insert(ret,
			      GINT_TO_POINTER(map[i].from),
			      GINT_TO_POINTER(map[i].to));
	}
	return ret;
}

static GTree *
vte_iso2022_map_get(gunichar mapname)
{
	static GTree *maps = NULL, *ret;
	/* No conversions for ASCII. */
	if (mapname == 'B') {
		return NULL;
	}
	/* Make sure we have a map, erm, map. */
	if (maps == NULL) {
		maps = g_tree_new(vte_direct_compare);
	}
	/* Check for a cached map for this charset. */
	ret = g_tree_lookup(maps, GINT_TO_POINTER(mapname));
	if (ret == NULL) {
		/* Construct a new one. */
		switch (mapname) {
		case '0':
			ret = vte_iso2022_map_init(vte_iso2022_map_0,
						   G_N_ELEMENTS(vte_iso2022_map_0));
			break;
		case 'A':
			ret = vte_iso2022_map_init(vte_iso2022_map_A,
						   G_N_ELEMENTS(vte_iso2022_map_A));
			break;
		case 'B':
			ret = vte_iso2022_map_init(vte_iso2022_map_B,
						   G_N_ELEMENTS(vte_iso2022_map_B));
			break;
		case '4':
			ret = vte_iso2022_map_init(vte_iso2022_map_4,
						   G_N_ELEMENTS(vte_iso2022_map_4));
			break;
		case 'C':
		case '5':
			ret = vte_iso2022_map_init(vte_iso2022_map_C,
						   G_N_ELEMENTS(vte_iso2022_map_C));
			break;
		case 'R':
			ret = vte_iso2022_map_init(vte_iso2022_map_R,
						   G_N_ELEMENTS(vte_iso2022_map_R));
			break;
		case 'Q':
			ret = vte_iso2022_map_init(vte_iso2022_map_Q,
						   G_N_ELEMENTS(vte_iso2022_map_Q));
			break;
		case 'K':
			ret = vte_iso2022_map_init(vte_iso2022_map_K,
						   G_N_ELEMENTS(vte_iso2022_map_K));
			break;
		case 'Y':
			ret = vte_iso2022_map_init(vte_iso2022_map_Y,
						   G_N_ELEMENTS(vte_iso2022_map_Y));
			break;
		case 'E':
		case '6':
			ret = vte_iso2022_map_init(vte_iso2022_map_E,
						   G_N_ELEMENTS(vte_iso2022_map_E));
			break;
		case 'Z':
			ret = vte_iso2022_map_init(vte_iso2022_map_Z,
						   G_N_ELEMENTS(vte_iso2022_map_Z));
			break;
		case 'H':
		case '7':
			ret = vte_iso2022_map_init(vte_iso2022_map_H,
						   G_N_ELEMENTS(vte_iso2022_map_H));
			break;
		case '=':
			ret = vte_iso2022_map_init(vte_iso2022_map_equal,
						   G_N_ELEMENTS(vte_iso2022_map_equal));
			break;
		case 'J':
			ret = vte_iso2022_map_init(vte_iso2022_map_J,
						   G_N_ELEMENTS(vte_iso2022_map_J));
			break;
		case '@' + WIDE94_FUDGE:
			ret = vte_iso2022_map_init(vte_iso2022_map_dollar_at,
						   G_N_ELEMENTS(vte_iso2022_map_dollar_at));
			break;
		case 'B' + WIDE94_FUDGE:
			ret = vte_iso2022_map_init(vte_iso2022_map_dollar_B,
						   G_N_ELEMENTS(vte_iso2022_map_dollar_B));
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
vte_iso2022_substitute(struct vte_iso2022 *outside_state,
		       gunichar *string, gssize length)
{
	int i, j;
	struct vte_iso2022 state;
	GTree *charmap = NULL;
	gpointer ptr;
	gunichar *buf, current_map = '\0', last_map = '\0', result;

	g_return_val_if_fail(outside_state != NULL, 0);
	g_return_val_if_fail(string != NULL, 0);
	g_return_val_if_fail(length != 0, 0);

	buf = g_malloc(sizeof(gunichar) * length);
	state = *outside_state;

	for (i = j = 0; i < length; i++)
	switch (string[i]) {
	case '':
		/* SO/LS1 */
		state.current = 1;
#ifdef VTE_DEBUG
		if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
			fprintf(stderr, "SO/LS1.\n");
		}
#endif
		continue;
		break;
	case '':
		/* SI/LS0 */
		state.current = 0;
#ifdef VTE_DEBUG
		if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
			fprintf(stderr, "SI/LS0.\n");
		}
#endif
		continue;
		break;
	case '':
		/* Begins a control sequence.  Make sure there's another
		 * character for us to read. */
		if (i + 1 >= length) {
			g_free(buf);
			return -1;
		}
		switch (string[i + 1]) {
		case '(':
			/* Designate G0.  Must be another character here. */
			if (i + 2 >= length) {
				g_free(buf);
				return -1;
			}
			state.g[0] = string[i + 2];
			i += 2;
#ifdef VTE_DEBUG
			if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
				fprintf(stderr, "G0 set to `%c'.\n",
					state.g[0]);
			}
#endif
			continue;
			break;
		case '$':
			/* Designate G0.  Must be another character here. */
			if (i + 2 >= length) {
				g_free(buf);
				return -1;
			}
			state.g[0] = string[i + 2] + WIDE94_FUDGE;
			i += 2;
#ifdef VTE_DEBUG
			if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
				fprintf(stderr, "G0 set to wide `%c'.\n",
					state.g[0] & 0xFF);
			}
#endif
			continue;
			break;
		case ')':
			/* Designate G1.  Must be another character here. */
			if (i + 2 >= length) {
				g_free(buf);
				return -1;
			}
			state.g[1] = string[i + 2];
			i += 2;
#ifdef VTE_DEBUG
			if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
				fprintf(stderr, "G1 set to `%c'.\n",
					state.g[1]);
			}
#endif
			continue;
			break;
		case '*':
			/* Designate G2.  Must be another character here. */
			if (i + 2 >= length) {
				g_free(buf);
				return -1;
			}
			state.g[2] = string[i + 2];
			i += 2;
#ifdef VTE_DEBUG
			if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
				fprintf(stderr, "G2 set to `%c'.\n",
					state.g[2]);
			}
#endif
			continue;
			break;
		case '+':
			/* Designate G3.  Must be another character here. */
			if (i + 2 >= length) {
				g_free(buf);
				return -1;
			}
			state.g[3] = string[i + 2];
			i += 2;
#ifdef VTE_DEBUG
			if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
				fprintf(stderr, "G3 set to `%c'.\n",
					state.g[3]);
			}
#endif
			continue;
			break;
		case 'n':
			/* LS2 */
			state.current = 2;
			i++;
#ifdef VTE_DEBUG
			if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
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
			if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
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
			if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
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
			if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
				fprintf(stderr, "SS3.\n");
			}
#endif
			continue;
			break;
		/* default:
			fall through */
		}
	default:
		/* Determine which map we should use here. */
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
		/* Load a new map if need be. */
		if (current_map != last_map) {
#ifdef VTE_DEBUG
			if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
				if (last_map == '\0') {
					fprintf(stderr,
						"Charmap set to `%c'.\n",
						current_map);
				} else {
					fprintf(stderr,
						"Charmap changed to `%c'.\n",
						current_map);
				}
			}
#endif
			charmap = vte_iso2022_map_get(current_map);
			last_map = current_map;
		}
		/* Translate. */
		if (charmap == NULL) {
			result = string[i];
		} else {
			ptr = GINT_TO_POINTER(string[i]);
			result = GPOINTER_TO_INT(g_tree_lookup(charmap, ptr));
			if (result == 0) {
				result = string[i];
#ifdef VTE_DEBUG
			} else {
				if (vte_debug_on(VTE_DEBUG_SUBSTITUTION)) {
					if (string[i] != result) {
						fprintf(stderr, "%d -> 0x%x\n",
							string[i], result);
					}
				}
#endif
			}
		}
		/* Store. */
		buf[j++] = result;
#ifdef VTE_DEBUG
		if (vte_debug_on(VTE_DEBUG_SUBSTITUTION) && 0) {
			fprintf(stderr, "`%c' (%d)\n", result, result);
		}
#endif
		break;
	}

	if (j > 0) {
		g_assert(j <= length);
		memcpy(string, buf, j * sizeof(gunichar));
	}
	*outside_state = state;
	g_free(buf);
	return j;
}

#ifdef ISO2022_MAIN
static void
debug_print(FILE *fp, const unsigned char *string)
{
	int i;
	for (i = 0; string[i] != '\0'; i++) {
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
	struct vte_iso2022 *state;

	if (getenv("VTE_DEBUG_FLAGS") != NULL) {
		vte_debug_parse_string(getenv("VTE_DEBUG_FLAGS"));
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
		state = vte_iso2022_new();
		debug_print(stderr, samples[i]);
		length = vte_iso2022_substitute(state, sample, j);
		debug_printu(stderr, sample, length);
		vte_iso2022_free(state);
		g_free(sample);
	}
	return 0;
}
#endif
