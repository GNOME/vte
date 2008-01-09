/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
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
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#include <glib.h>
#include <glib-object.h>
#include "debug.h"
#include "iso2022.h"
#include "matcher.h"
#include "trie.h"
#include "vteconv.h"

#ifndef HAVE_WINT_T
typedef gunichar wint_t;
#endif

#include <glib/gi18n-lib.h>

#ifndef TRIE_MAYBE_STATIC
#define TRIE_MAYBE_STATIC
#endif

/* Structures and whatnot for tracking character classes. */
struct char_class_data {
	gunichar c;			/* A character. */
	int i;				/* An integer. */
	char *s;			/* A string. */
	int inc;			/* An increment value. */
};

struct char_class {
	enum cclass {
		exact = 0,		/* Not a special class. */
		digit,			/* Multiple-digit special class. */
		multi,			/* Multiple-number special class. */
		any,			/* Any single character. */
		string,			/* Any string of characters. */
		invalid			/* A placeholder. */
	} type;
	gboolean multiple;		/* Whether a sequence of multiple
					   characters in this class should be
					   counted together. */
	gunichar *code;			/* A magic string that indicates this
					   class should be found here. */
	gsize code_length;
	gsize ccount;			/* The maximum number of characters
					   after the format specifier to
					   consume. */
	gboolean (*check)(const gunichar c, struct char_class_data *data);
					/* Function to check if a character
					   is in this class. */
	void (*setup)(const gunichar *s, struct char_class_data *data, int inc);
					/* Setup the data struct for use in the
					 * above check function. */
	gboolean (*extract)(const gunichar *s, gsize length,
			    struct char_class_data *data,
			    GValueArray *array);
					/* Extract a parameter. */
};

/* A trie to hold control sequences. */
struct _vte_trie {
	struct _vte_matcher_impl impl;
	const char *result;		/* If this is a terminal node, then this
					   field contains its "value". */
	GQuark quark;			/* The quark for the value of the
					   result. */
	gsize trie_path_count;		/* Number of children of this node. */
	struct trie_path {
		struct char_class *cclass;
		struct char_class_data data;
		struct _vte_trie *trie;	/* The child node corresponding to this
					   character. */
	} *trie_paths;
};

/* Functions for checking if a particular character is part of a class, and
 * for setting up a structure for use when determining matches. */
static gboolean
char_class_exact_check(gunichar c, struct char_class_data *data)
{
	return (c == data->c) ? TRUE : FALSE;
}
static void
char_class_exact_setup(const gunichar *s, struct char_class_data *data, int inc)
{
	data->c = s[0];
	return;
}
static void
char_class_percent_setup(const gunichar *s, struct char_class_data *data,
			 int inc)
{
	data->c = '%';
	return;
}
static gboolean
char_class_none_extract(const gunichar *s, gsize length,
			struct char_class_data *data, GValueArray *array)
{
	return FALSE;
}

static gboolean
char_class_digit_check(gunichar c, struct char_class_data *data)
{
	switch (c) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			return TRUE;
		default:
			return FALSE;
	}
}
static void
char_class_digit_setup(const gunichar *s, struct char_class_data *data, int inc)
{
	data->inc = inc;
	return;
}
static gboolean
char_class_digit_extract(const gunichar *s, gsize length,
			 struct char_class_data *data, GValueArray *array)
{
	long ret = 0;
	gsize i;
	GValue value;
	for (i = 0; i < length; i++) {
		ret *= 10;
		ret += g_unichar_digit_value(s[i]) == -1 ?
		       0 : g_unichar_digit_value(s[i]);
	}
	memset(&value, 0, sizeof(value));
	g_value_init(&value, G_TYPE_LONG);
	g_value_set_long(&value, ret - data->inc);
	g_value_array_append(array, &value);
	g_value_unset(&value);
	return TRUE;
}

static gboolean
char_class_multi_check(gunichar c, struct char_class_data *data)
{
	switch (c) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case ';':
			return TRUE;
		default:
			return FALSE;
	}
}
static void
char_class_multi_setup(const gunichar *s, struct char_class_data *data, int inc)
{
	data->inc = inc;
	return;
}
static gboolean
char_class_multi_extract(const gunichar *s, gsize length,
			 struct char_class_data *data, GValueArray *array)
{
	long ret = 0;
	gsize i;
	GValue value;
	memset(&value, 0, sizeof(value));
	g_value_init(&value, G_TYPE_LONG);
	for (i = 0; i < length; i++) {
		if (s[i] == ';') {
			g_value_set_long(&value, ret - data->inc);
			g_value_array_append(array, &value);
			ret = 0;
		} else {
			ret *= 10;
			ret += (s[i] - '0');
		}
	}
	g_value_set_long(&value, ret - data->inc);
	g_value_array_append(array, &value);
	g_value_unset(&value);
	return TRUE;
}

static gboolean
char_class_any_check(gunichar c, struct char_class_data *data)
{
	return (c >= data->c) ? TRUE : FALSE;
}
static void
char_class_any_setup(const gunichar *s, struct char_class_data *data, int inc)
{
	data->c = s[0] + inc;
	return;
}
static gboolean
char_class_any_extract(const gunichar *s, gsize length,
		       struct char_class_data *data, GValueArray *array)
{
	long ret = 0;
	GValue value;
	ret = s[0] - data->c;
	memset(&value, 0, sizeof(value));
	g_value_init(&value, G_TYPE_LONG);
	g_value_set_long(&value, ret - data->inc);
	g_value_array_append(array, &value);
	g_value_unset(&value);
	return TRUE;
}

static gboolean
char_class_string_check(gunichar c, struct char_class_data *data)
{
	return (c != data->c) ? TRUE : FALSE;
}
static void
char_class_string_setup(const gunichar *s, struct char_class_data *data, int inc)
{
	data->c = s[0];
	return;
}
static gsize
unichar_snlen(const gunichar *s, gsize length)
{
	gsize i;
	for (i = 0; i < length; i++) {
		if (s[i] == '\0') {
			return i;
		}
	}
	return length;
}
static void
unichar_sncpy(gunichar *d, const gunichar *s, gsize length)
{
	unsigned int i;
	for (i = 0; i < length; i++) {
		d[i] = s[i];
		if (s[i] == 0) {
			break;
		}
	}
}
static int
unichar_sncmp(const gunichar *a, const gunichar *b, gsize length)
{
	gsize i;
	for (i = 0; i < length; i++) {
		if (a[i] != b[i]) {
			return a[i] - b[i];
		}
		if (a[i] == 0) {
			break;
		}
	}
	return 0;
}
static gboolean
char_class_string_extract(const gunichar *s, gsize length,
			  struct char_class_data *data, GValueArray *array)
{
	gunichar *ret = NULL;
	gsize len;
	gsize i;
	GValue value;

	len = unichar_snlen(s, length);
	ret = g_malloc0((len + 1) * sizeof(gunichar));
	unichar_sncpy(ret, s, len);
	for (i = 0; i < len; i++) {
		ret[i] &= ~(VTE_ISO2022_ENCODED_WIDTH_MASK);
	}
	_vte_debug_print(VTE_DEBUG_PARSE,
			"Extracting string `%ls'.\n", (wchar_t*) ret);
	memset(&value, 0, sizeof(value));

	g_value_init(&value, G_TYPE_POINTER);
	g_value_set_pointer(&value, ret);
	g_value_array_append(array, &value);
	g_value_unset(&value);

	return TRUE;
}

static gunichar empty_wstring[] = {'\0'};
static gunichar digit_wstring1[] = {'%', '2', '\0'};
static gunichar digit_wstring2[] = {'%', 'd', '\0'};
static gunichar any_wstring[] = {'%', '+', '\0'};
static gunichar exact_wstring[] = {'%', '%', '\0'};
static gunichar string_wstring[] = {'%', 's', '\0'};
static gunichar multi_wstring[] = {'%', 'm', '\0'};

static struct char_class char_classes[] = {
	{exact, FALSE, empty_wstring, 0, 1,
	 char_class_exact_check,
	 char_class_exact_setup,
	 char_class_none_extract},
	{digit, TRUE, digit_wstring1, 2, 0,
	 char_class_digit_check,
	 char_class_digit_setup,
	 char_class_digit_extract},
	{digit, TRUE, digit_wstring2, 2, 0,
	 char_class_digit_check,
	 char_class_digit_setup,
	 char_class_digit_extract},
	{multi, TRUE, multi_wstring, 2, 0,
	 char_class_multi_check,
	 char_class_multi_setup,
	 char_class_multi_extract},
	{any, FALSE, any_wstring, 2, 1,
	 char_class_any_check,
	 char_class_any_setup,
	 char_class_any_extract},
	{exact, FALSE, exact_wstring, 2, 0,
	 char_class_exact_check,
	 char_class_percent_setup,
	 char_class_none_extract},
	{string, TRUE, string_wstring, 2, 0,
	 char_class_string_check,
	 char_class_string_setup,
	 char_class_string_extract},
};

/* Create a new trie. */
TRIE_MAYBE_STATIC struct _vte_trie *
_vte_trie_new(void)
{
	struct _vte_trie *ret;
	ret = g_slice_new0(struct _vte_trie);
	ret->impl.klass = &_vte_matcher_trie;
	return ret;
}

TRIE_MAYBE_STATIC void
_vte_trie_free(struct _vte_trie *trie)
{
	unsigned int i;
	for (i = 0; i < trie->trie_path_count; i++) {
		_vte_trie_free(trie->trie_paths[i].trie);
	}
	if (trie->trie_path_count > 0) {
		g_free(trie->trie_paths);
	}
	g_slice_free(struct _vte_trie, trie);
}

/* Add the given pattern, with its own result string, to the trie, with the
 * given initial increment value. */
static void
_vte_trie_addx(struct _vte_trie *trie, gunichar *pattern, gsize length,
	       const char *result, GQuark quark, int inc)
{
	gsize i;
	struct char_class *cclass = NULL;
	struct char_class_data data;
	gunichar *code;
	gsize len = 0, ccount = 0;
	gunichar inc_wstring[] = {'%', 'i', '\0'};

	/* The trivial case -- we'll just set the result at this node. */
	if (length == 0) {
		if (trie->result == NULL) {
			trie->quark = g_quark_from_string(result);
			trie->result = g_quark_to_string(trie->quark);
		} else {
			_VTE_DEBUG_IF(VTE_DEBUG_PARSE)
				g_warning(_("Duplicate (%s/%s)!"),
					  result, trie->result);
		}
		return;
	}

	/* If this part of the control sequence indicates incrementing a
	 * parameter, keep track of the incrementing, skip over the increment
	 * substring, and keep going. */
	if ((length >= 2) && (unichar_sncmp(pattern, inc_wstring, 2) == 0)) {
		_vte_trie_addx(trie, pattern + 2, length - 2,
			       result, quark, inc + 1);
		return;
	}

	/* Now check for examples of character class specifiers, and use that
	 * to put this part of the pattern in a character class. */
	for (i = G_N_ELEMENTS(char_classes); i--; ) {
		len = char_classes[i].code_length;
		code = char_classes[i].code;
		ccount = char_classes[i].ccount;
		if ((len <= length) && (unichar_sncmp(pattern, code, len) == 0)) {
			cclass = &char_classes[i];
			break;
		}
	}

	/* Initialize the data item using the data we have here. */
	memset(&data, 0, sizeof(data));
	cclass->setup(pattern + len, &data, inc);

	/* Hunt for a subtrie which matches this class / data pair. */
	for (i = 0; i < trie->trie_path_count; i++) {
		struct char_class_data *tdata;
		tdata =  &trie->trie_paths[i].data;
		if ((trie->trie_paths[i].cclass == cclass) &&
		    (memcmp(&data, tdata, sizeof(data)) == 0)) {
			/* It matches, so insert the rest of the pattern into
			 * this subtrie. */
			_vte_trie_addx(trie->trie_paths[i].trie,
				       pattern + (len + ccount),
				       length - (len + ccount),
				       result,
				       quark,
				       inc);
			return;
		}
	}

	/* Add a new subtrie to contain the rest of this pattern. */
	trie->trie_path_count++;
	trie->trie_paths = g_realloc(trie->trie_paths,
				     trie->trie_path_count *
				     sizeof(trie->trie_paths[0]));
	i = trie->trie_path_count - 1;
	memset(&trie->trie_paths[i], 0, sizeof(trie->trie_paths[i]));
	trie->trie_paths[i].trie = _vte_trie_new();
	cclass->setup(pattern + len, &trie->trie_paths[i].data, inc);
	trie->trie_paths[i].cclass = cclass;

	/* Now insert the rest of the pattern into the node we just created. */
	_vte_trie_addx(trie->trie_paths[i].trie,
		       pattern + (len + ccount),
		       length - (len + ccount),
		       result,
		       quark,
		       inc);
}

/* Add the given pattern, with its own result string, to the trie. */
TRIE_MAYBE_STATIC void
_vte_trie_add(struct _vte_trie *trie, const char *pattern, gsize length,
	      const char *result, GQuark quark)
{
	const guchar *tpattern;
	guchar *wpattern, *wpattern_end;
	VteConv conv;
	gsize wlength;

	g_return_if_fail(trie != NULL);
	g_return_if_fail(pattern != NULL);
	g_return_if_fail(length > 0);
	g_return_if_fail(result != NULL);
	if (quark == 0) {
		quark = g_quark_from_string(result);
	}

	wlength = sizeof(gunichar) * (length + 1);
	wpattern = wpattern_end = g_malloc0(wlength + 1);

	conv = _vte_conv_open(VTE_CONV_GUNICHAR_TYPE, "UTF-8");
	g_assert(conv != VTE_INVALID_CONV);

	tpattern = (const guchar *)pattern;
	_vte_conv(conv, &tpattern, &length, &wpattern_end, &wlength);
	if (length == 0) {
		wlength = (wpattern_end - wpattern) / sizeof(gunichar);
		_vte_trie_addx(trie, (gunichar*)wpattern, wlength,
			       result, quark, 0);
	}
	_vte_conv_close(conv);

	g_free(wpattern);
}

/* Check if the given pattern matches part of the given trie, returning an
 * empty string on a partial initial match, a %NULL if there's no match in the
 * works, and the result string if we have an exact match. */
static const char *
_vte_trie_matchx(struct _vte_trie *trie, const gunichar *pattern, gsize length,
		 gboolean greedy,
		 const char **res, const gunichar **consumed,
		 GQuark *quark, GValueArray *array)
{
	unsigned int i;
	const char *hres;
	enum cclass cc;
	const char *best = NULL;
	GValueArray *bestarray = NULL;
	GQuark bestquark = 0;
	const gunichar *bestconsumed = pattern;

	/* Make sure that attempting to save output values doesn't kill us. */
	if (res == NULL) {
		res = &hres;
	}

	/* Trivial cases.  We've matched an entire pattern, or we're out of
	 * pattern to match. */
	if (trie->result != NULL) {
		*res = trie->result;
		*quark = trie->quark;
		*consumed = pattern;
		return *res;
	}
	if (length <= 0) {
		if (trie->trie_path_count > 0) {
			*res = "";
			*quark = g_quark_from_static_string("");
			*consumed = pattern;
			return *res;
		} else {
			*res = NULL;
			*quark = 0;
			*consumed = pattern;
			return *res;
		}
	}

	/* Now figure out which (if any) subtrees to search.  First, see
	 * which character class this character matches. */
	for (cc = exact; cc < invalid; cc++)
	for (i = 0; i < trie->trie_path_count; i++) {
		struct _vte_trie *subtrie = trie->trie_paths[i].trie;
		struct char_class *cclass = trie->trie_paths[i].cclass;
		struct char_class_data *data = &trie->trie_paths[i].data;
		if (trie->trie_paths[i].cclass->type == cc) {
			/* If it matches this character class... */
			if (cclass->check(pattern[0], data)) {
				const gunichar *prospect = pattern + 1;
				const char *tmp;
				GQuark tmpquark = 0;
				GValueArray *tmparray;
				gboolean better = FALSE;
				/* Move past characters which might match this
				 * part of the string... */
				while (cclass->multiple &&
				       ((prospect - pattern) < length) &&
				       cclass->check(prospect[0], data)) {
					prospect++;
				}
				/* ... see if there's a parameter here, ... */
				tmparray = g_value_array_new(0);
				cclass->extract(pattern,
						prospect - pattern,
						data,
						tmparray);
				/* ... and check if the subtree matches the
				 * rest of the input string.  Any parameters
				 * further on will be appended to the array. */
				_vte_trie_matchx(subtrie,
						 prospect,
						 length - (prospect - pattern),
						 greedy,
						 &tmp,
						 consumed,
						 &tmpquark,
						 tmparray);
				/* If we haven't seen any matches yet, go ahead
				 * and go by this result. */
				if (best == NULL) {
					better = TRUE;
				} else
				/* If we have a match, and we didn't have one
				 * already, go by this result. */
				if ((best != NULL) &&
				    (best[0] == '\0') &&
				    (tmp != NULL) &&
				    (tmp[0] != '\0')) {
					better = TRUE;
				} else
				/* If we already have a match, and this one's
				 * better (longer if we're greedy, shorter if
				 * we're not), then go by this result. */
				if ((tmp != NULL) &&
				    (tmp[0] != '\0') &&
				    (bestconsumed != NULL) &&
				    (consumed != NULL) &&
				    (*consumed != NULL)) {
					if (greedy &&
					    (bestconsumed < *consumed)) {
						better = TRUE;
					} else
					if (!greedy &&
					    (bestconsumed > *consumed)) {
						better = TRUE;
					}
				}
				if (better) {
					best = tmp;
					if (bestarray != NULL) {
						_vte_matcher_free_params_array(
								NULL, bestarray);
					}
					bestarray = tmparray;
					bestquark = tmpquark;
					bestconsumed = *consumed;
				} else {
					_vte_matcher_free_params_array(
							NULL, tmparray);
					tmparray = NULL;
				}
			}
		}
	}

	/* We're done searching.  Copy out any parameters we picked up. */
	if (bestarray != NULL) {
		for (i = 0; i < bestarray->n_values; i++) {
			GValue *value = g_value_array_get_nth(bestarray, i);
			g_value_array_append(array, value);

			if (G_VALUE_HOLDS_POINTER(value)) {
				g_value_set_pointer(value, NULL);
			}
		}
		_vte_matcher_free_params_array(NULL, bestarray);
	}
#if 0
	printf("`%s' ", best);
	dump_array(array);
#endif
	*quark = bestquark;
	*res = best;
	*consumed = bestconsumed;
	return *res;
}

/* Check if the given pattern matches part of the given trie, returning an
 * empty string on a partial initial match, a %NULL if there's no match in the
 * works, and the result string if we have an exact match. */
TRIE_MAYBE_STATIC const char *
_vte_trie_match(struct _vte_trie *trie, const gunichar *pattern, gsize length,
		const char **res, const gunichar **consumed,
		GQuark *quark, GValueArray **array)
{
	const char *ret = NULL;
	GQuark tmpquark;
	GValueArray *valuearray;
	GValue *value;
	const gunichar *dummyconsumed;
	gboolean greedy = FALSE;
	guint i;

	if (array != NULL && *array != NULL) {
		valuearray = *array;
	} else {
		valuearray = g_value_array_new(0);
	}
	if (quark == NULL) {
		quark = &tmpquark;
	}
	*quark = 0;

	if (consumed == NULL) {
		consumed = &dummyconsumed;
	}
	*consumed = pattern;

	ret = _vte_trie_matchx(trie, pattern, length, greedy,
			       res, consumed, quark, valuearray);

	if (((ret == NULL) || (ret[0] == '\0')) || (valuearray->n_values == 0)){
		if (valuearray != NULL) {
			for (i = 0; i < valuearray->n_values; i++) {
				value = g_value_array_get_nth(valuearray, i);
				if (G_VALUE_HOLDS_POINTER(value)) {
					g_free(g_value_get_pointer(value));
					g_value_set_pointer(value, NULL);
				}
			}
			if (array == NULL || valuearray != *array) {
				_vte_matcher_free_params_array(NULL, valuearray);
			}
		}
	} else {
		if (array == NULL) {
			_vte_matcher_free_params_array(NULL, valuearray);
		}
	}

	return ret;
}

/* Print the next layer of the trie, indented by length spaces. */
static void
_vte_trie_printx(struct _vte_trie *trie, const char *previous,
		 gsize *nodecount)
{
	unsigned int i;
	char buf[LINE_MAX];

	if ((nodecount) && (trie->trie_path_count > 0)) {
		(*nodecount)++;
	}

	for (i = 0; i < trie->trie_path_count; i++) {
		memset(buf, '\0', sizeof(buf));
		snprintf(buf, sizeof(buf), "%s", previous);
		switch (trie->trie_paths[i].cclass->type) {
			case exact:
				if (trie->trie_paths[i].data.c < 32) {
					snprintf(buf + strlen(buf),
						 sizeof(buf) - strlen(buf),
						 "^%lc",
						 (wint_t)trie->trie_paths[i].data.c +
						 64);
				} else
				if (trie->trie_paths[i].data.c > 126) {
					snprintf(buf + strlen(buf),
						 sizeof(buf) - strlen(buf),
						 "[:%ld:]",
						 (long)trie->trie_paths[i].data.c);
				} else {
					snprintf(buf + strlen(buf),
						 sizeof(buf) - strlen(buf),
						 "%lc",
						 (wint_t)trie->trie_paths[i].data.c);
				}
				break;
			case digit:
				snprintf(buf + strlen(buf),
					 sizeof(buf) - strlen(buf),
					 "{num+%d}",
					 trie->trie_paths[i].data.inc);
				break;
			case multi:
				snprintf(buf + strlen(buf),
					 sizeof(buf) - strlen(buf),
					 "{multinum+%d}",
					 trie->trie_paths[i].data.inc);
				break;
			case any:
				if (trie->trie_paths[i].data.c < 32) {
					snprintf(buf + strlen(buf),
						 sizeof(buf) - strlen(buf),
						 "{char+0x%02lx}",
						 (long)trie->trie_paths[i].data.c);
				} else {
					snprintf(buf + strlen(buf),
						 sizeof(buf) - strlen(buf),
						 "{char+`%lc'}",
						 (wint_t)trie->trie_paths[i].data.c);
				}
				break;
			case string:
				snprintf(buf + strlen(buf),
					 sizeof(buf) - strlen(buf),
					 "{string}");
				break;
			case invalid:
				break;
		}
		if (trie->trie_paths[i].trie->result != NULL) {
			printf("%s = `%s'\n", buf,
			       trie->trie_paths[i].trie->result);
		}
		_vte_trie_printx(trie->trie_paths[i].trie, buf, nodecount);
	}
}

/* Print the trie. */
TRIE_MAYBE_STATIC void
_vte_trie_print(struct _vte_trie *trie)
{
	gsize nodecount = 0;
	_vte_trie_printx(trie, "", &nodecount);
	printf("Trie has %ld nodes.\n", (long) nodecount);
}

#ifdef TRIE_MAIN
static void
dump_array(GValueArray *array)
{
	unsigned int i;
	if (array != NULL) {
		printf("args = {");
		for (i = 0; i < array->n_values; i++) {
			GValue *value;
			value = g_value_array_get_nth(array, i);
			if (i > 0) {
				printf(", ");
			}
			if (G_VALUE_HOLDS_LONG(value)) {
				printf("%ld", g_value_get_long(value));
			}
			if (G_VALUE_HOLDS_STRING(value)) {
				printf("`%s'", g_value_get_string(value));
			}
			if (G_VALUE_HOLDS_POINTER(value)) {
				printf("`%ls'",
				       (wchar_t*) g_value_get_pointer(value));
			}
		}
		printf("}\n");
	}
}

static void
convert_mbstowcs(const char *i, gsize ilen,
		 gunichar *o, gsize *olen, gsize max_olen)
{
	VteConv conv;
	gsize outlen;
	conv = _vte_conv_open(VTE_CONV_GUNICHAR_TYPE, "UTF-8");
	g_assert(conv != VTE_INVALID_CONV);

	memset(o, 0, max_olen);
	outlen = max_olen;
	_vte_conv_cu(conv, (char**)&i, &ilen, &o, &outlen);
	_vte_conv_close(conv);

	*olen = (max_olen - outlen) / sizeof(gunichar);
}

int
main(int argc, char **argv)
{
	struct _vte_trie *trie;
	GValueArray *array = NULL;
	GQuark quark;
	gunichar buf[LINE_MAX];
	const gunichar *consumed;
	gsize buflen;

	_vte_debug_parse_string(getenv("VTE_DEBUG_FLAGS"));

	g_type_init();
	trie = _vte_trie_new();

	_vte_trie_add(trie, "abcdef", 6, "abcdef",
		      g_quark_from_static_string("abcdef"));
	_vte_trie_add(trie, "abcde", 5, "abcde",
		      g_quark_from_static_string("abcde"));
	_vte_trie_add(trie, "abcdeg", 6, "abcdeg",
		      g_quark_from_static_string("abcdeg"));
	_vte_trie_add(trie, "abc%+Aeg", 8, "abc%+Aeg",
		      g_quark_from_static_string("abc%+Aeg"));
	_vte_trie_add(trie, "abc%deg", 7, "abc%deg",
		      g_quark_from_static_string("abc%deg"));
	_vte_trie_add(trie, "abc%%eg", 7, "abc%%eg",
		      g_quark_from_static_string("abc%%eg"));
	_vte_trie_add(trie, "abc%%%i%deg", 11, "abc%%%i%deg",
		      g_quark_from_static_string("abc%%%i%deg"));
	_vte_trie_add(trie, "<esc>[%i%d;%dH", 14, "vtmatch",
		      g_quark_from_static_string("vtmatch"));
	_vte_trie_add(trie, "<esc>[%i%mL", 11, "multimatch",
		      g_quark_from_static_string("multimatch"));
	_vte_trie_add(trie, "<esc>[%mL<esc>[%mL", 18, "greedy",
		      g_quark_from_static_string("greedy"));
	_vte_trie_add(trie, "<esc>]2;%sh", 11, "decset-title",
		      g_quark_from_static_string("decset-title"));

	printf("Wide encoding is `%s'.\n", VTE_CONV_GUNICHAR_TYPE);

	_vte_trie_print(trie);
	printf("\n");

	quark = 0;
	convert_mbstowcs("abc", 3, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "abc",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("abcdef", 6, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "abcdef",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("abcde", 5, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "abcde",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("abcdeg", 6, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "abcdeg",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("abc%deg", 7, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "abc%deg",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("abc10eg", 7, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "abc10eg",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("abc%eg", 6, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "abc%eg",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("abc%10eg", 8, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "abc%10eg",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("abcBeg", 6, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "abcBeg",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("<esc>[25;26H", 12, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "<esc>[25;26H",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("<esc>[25;2", 10, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "<esc>[25;2",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
	}

	quark = 0;
	convert_mbstowcs("<esc>[25L", 9, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "<esc>[25L",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
	}

	quark = 0;
	convert_mbstowcs("<esc>[25L<esc>[24L", 18, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "<esc>[25L<esc>[24L",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
	}

	quark = 0;
	convert_mbstowcs("<esc>[25;26L", 12, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "<esc>[25;26L",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
	}

	quark = 0;
	convert_mbstowcs("<esc>]2;WoofWoofh", 17, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "<esc>]2;WoofWoofh",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("<esc>]2;WoofWoofh<esc>]2;WoofWoofh", 34,
			 buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "<esc>]2;WoofWoofh<esc>]2;WoofWoofh",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	quark = 0;
	convert_mbstowcs("<esc>]2;WoofWoofhfoo", 20, buf, &buflen, sizeof(buf));
	printf("`%s' = `%s'\n", "<esc>]2;WoofWoofhfoo",
	       _vte_trie_match(trie, buf, buflen,
			       NULL, &consumed, &quark, &array));
	printf("=> `%s' (%d)\n", g_quark_to_string(quark), (int)(consumed - buf));
	if (array != NULL) {
		dump_array(array);
		_vte_matcher_free_params_array(NULL, array);
		array = NULL;
	}

	_vte_trie_free(trie);

	return 0;
}
#endif

const struct _vte_matcher_class _vte_matcher_trie = {
	(_vte_matcher_create_func)_vte_trie_new,
	(_vte_matcher_add_func)_vte_trie_add,
	(_vte_matcher_print_func)_vte_trie_print,
	(_vte_matcher_match_func)_vte_trie_match,
	(_vte_matcher_destroy_func)_vte_trie_free
};
