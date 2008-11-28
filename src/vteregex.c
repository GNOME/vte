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

#include <config.h>

#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>

#include <regex.h>

#include "vteregex.h"

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
	guint i, ret;

	posix_matches = g_new(regmatch_t, nmatch);
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
