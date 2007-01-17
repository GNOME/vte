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

#include "../config.h"
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>

#include "vteregex.h"

#if defined(USE_GNU_REGEX)
#include <regex.h>
#elif defined(USE_PCRE)
#include <pcre.h>
#else
#include <regex.h>
#endif

static gint
compare_matches(gconstpointer a, gconstpointer b)
{
	const struct _vte_regex_match *A, *B;
	A = a;
	B = b;
	if (B->rm_so != A->rm_so) {
		return B->rm_so - A->rm_so;
	}
	return B->rm_eo - A->rm_eo;
}

/* Sort match structures first by starting position, and then by ending
 * position.  We do this because some expression matching APIs sort their
 * results differently, or just plain don't sort them. */
static void
_vte_regex_sort_matches(struct _vte_regex_match *matches, gsize n_matches)
{
	GArray *array;
	if (n_matches <= 1) {
		return;
	}
	array = g_array_new(FALSE, FALSE, sizeof(struct _vte_regex_match));
	g_array_append_vals(array, matches, n_matches);
	g_array_sort(array, compare_matches);
	memmove(matches, array->data,
		n_matches * sizeof(struct _vte_regex_match));
	g_array_free(array, TRUE);
}

#if defined(USE_GNU_REGEX)

/* GNU regex-based matching.  The GNU regex library also provides POSIX
 * workalikes, so I don't see much of a win from using this chunk of code. */

struct _vte_regex {
	struct re_pattern_buffer buffer;
};

struct _vte_regex *
_vte_regex_compile(const char *pattern)
{
	struct _vte_regex *ret;
	const char *res;

	ret = g_slice_new0(struct _vte_regex);
	res = re_compile_pattern(pattern, strlen(pattern), &ret->buffer);
	if (res != NULL) {
		g_slice_free(struct _vte_regex, ret);
		return NULL;
	}
	return ret;
}

void
_vte_regex_free(struct _vte_regex *regex)
{
	regfree(&regex->buffer);
	g_slice_free(struct _vte_regex, regex);
}

int
_vte_regex_exec(struct _vte_regex *regex, const char *string,
		gsize nmatch, struct _vte_regex_match *matches)
{
	struct re_registers registers;
	int i, length, ret;

	length = strlen(string);
	registers.num_regs = 0;
	registers.start = NULL;
	registers.end = NULL;
	ret = re_search(&regex->buffer,
			string, length,
			0, length - 1,
			&registers);
	if (ret >= 0) {
		for (i = 0; i < nmatch; i++) {
			matches[i].rm_so = -1;
			matches[i].rm_eo = -1;
		}
		for (i = 0; (i < nmatch) && (i < registers.num_regs); i++) {
			matches[i].rm_so = registers.start[i];
			matches[i].rm_eo = registers.end[i];
		}
		if ((i == nmatch) || (matches[i].rm_so == -1)) {
			_vte_regex_sort_matches(matches, i);
		}
	}
	if (ret >= 0) {
		return 0;
	}
	return -1;
}

#elif defined(USE_PCRE)

/* PCRE-based matching.  In addition to not being "real" regexps, I'm seeing
 * problems matching non-ASCII portions of UTF-8 strings, even when compiling
 * the pattern with UTF-8 support enabled. */

struct _vte_regex {
	pcre *pcre;
	pcre_extra *extra;
};

struct _vte_regex *
_vte_regex_compile(const char *pattern)
{
	struct _vte_regex *ret;
	const char *err;
	int err_offset;

	ret = g_slice_new(struct _vte_regex);

	ret->pcre = pcre_compile(pattern, PCRE_UTF8, &err, &err_offset, NULL);
	if (ret->pcre == NULL) {
		g_slice_free(struct _vte_regex, ret);
		return NULL;
	}

	ret->extra = pcre_study(ret->pcre, 0, &err);
	if (ret->extra == NULL) {
		pcre_free(ret->pcre);
		g_slice_free(struct _vte_regex, ret);
		return NULL;
	}

	return ret;
}

void
_vte_regex_free(struct _vte_regex *regex)
{
	pcre_free(regex->pcre);
	pcre_free(regex->extra);
	g_slice_free(struct _vte_regex, regex);
}

int
_vte_regex_exec(struct _vte_regex *regex, const char *string,
		gsize nmatch, struct _vte_regex_match *matches)
{
	int i, n_matches, *ovector, ovector_length, length;

	for (i = 0; i < nmatch; i++) {
		matches[i].rm_so = -1;
		matches[i].rm_eo = -1;
	}

	length = strlen(string);
	ovector_length = 3 * (length + 1);
	ovector = g_malloc(sizeof(int) * ovector_length);

	i = pcre_exec(regex->pcre, regex->extra, string, length,
		      0, 0, ovector, ovector_length);

	if (i < 0) {
		g_free(ovector);
		return -1;
	}

	n_matches = i;
	while (i > 0) {
		i--;
		if (i < nmatch) {
			matches[i].rm_so = ovector[i * 2];
			matches[i].rm_eo = ovector[i * 2 + 1];
		}
	}
	_vte_regex_sort_matches(matches, n_matches);

	return 0;
}

#else

/* Ah, POSIX regex.  Kind of clunky, but I don't have anything better to
 * suggest.  Better still, it works on my machine. */

struct _vte_regex {
	regex_t posix_regex;
};

struct _vte_regex *
_vte_regex_compile(const char *pattern)
{
	struct _vte_regex *ret;
	int i;

	ret = g_slice_new(struct _vte_regex);
	i = regcomp(&ret->posix_regex, pattern, REG_EXTENDED);
	if (i != 0) {
		g_slice_free(struct _vte_regex, ret);
		return NULL;
	}
	return ret;
}

void
_vte_regex_free(struct _vte_regex *regex)
{
	regfree(&regex->posix_regex);
	g_slice_free(struct _vte_regex, regex);
}

int
_vte_regex_exec(struct _vte_regex *regex, const char *string,
		gsize nmatch, struct _vte_regex_match *matches)
{
	regmatch_t *posix_matches;
	int i, ret;

	posix_matches = g_malloc(nmatch * sizeof(regmatch_t));
	ret = regexec(&regex->posix_regex, string, nmatch, posix_matches, 0);
	if (ret == 0) {
		for (i = 0; i < nmatch; i++) {
			matches[i].rm_so = -1;
			matches[i].rm_eo = -1;
		}
		for (i = 0; i < nmatch; i++) {
			matches[i].rm_so = posix_matches[i].rm_so;
			matches[i].rm_eo = posix_matches[i].rm_eo;
			if (matches[i].rm_so == -1) {
				_vte_regex_sort_matches(matches, i);
				break;
			}
		}
	}
	g_free(posix_matches);
	if (ret == 0) {
		return 0;
	}
	return -1;
}

#endif
