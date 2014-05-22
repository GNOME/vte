/*
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <config.h>
#include <sys/types.h>
#include <string.h>
#include <glib-object.h>
#include "debug.h"
#include "caps.h"
#include "matcher.h"
#include "table.h"

struct _vte_matcher {
	_vte_matcher_match_func match; /* shortcut to the most common op */
	struct _vte_matcher_impl *impl;
	GValueArray *free_params;
};

static GMutex _vte_matcher_mutex;
static struct _vte_matcher *_vte_matcher_singleton = NULL;
static int _vte_matcher_ref_count = 0;

static struct _vte_matcher_impl dummy_vte_matcher_table = {
	&_vte_matcher_table
};

/* Add a string to the matcher. */
static void
_vte_matcher_add(const struct _vte_matcher *matcher,
		 const char *pattern, gssize length,
		 const char *result)
{
	matcher->impl->klass->add(matcher->impl, pattern, length, result);
}

/* Loads all sequences into matcher */
static void
_vte_matcher_init(struct _vte_matcher *matcher)
{
	const char *code, *value;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "_vte_matcher_init()\n");

        code = _vte_xterm_capability_strings;
        do {
                value = strchr(code, '\0') + 1;
                _vte_matcher_add(matcher, code, strlen(code), value);
                code = strchr(value, '\0') + 1;
        } while (*code);

	_VTE_DEBUG_IF(VTE_DEBUG_MATCHER) {
		g_printerr("Matcher contents:\n");
		_vte_matcher_print(matcher);
		g_printerr("\n");
	}
}

/* Allocates new matcher structure. */
static struct _vte_matcher *
_vte_matcher_create(void)
{
	struct _vte_matcher *ret = NULL;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "_vte_matcher_create()\n");
	ret = g_slice_new(struct _vte_matcher);
        ret->impl = &dummy_vte_matcher_table;
	ret->match = NULL;
	ret->free_params = NULL;

	return ret;
}

/* Noone uses this matcher, free it. */
static void
_vte_matcher_destroy(struct _vte_matcher *matcher)
{
	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "_vte_matcher_destroy()\n");
	if (matcher->free_params != NULL) {
		g_value_array_free (matcher->free_params);
	}
	if (matcher->match != NULL) /* do not call destroy on dummy values */
		matcher->impl->klass->destroy(matcher->impl);
	g_slice_free(struct _vte_matcher, matcher);
}

/* Create and init matcher. */
struct _vte_matcher *
_vte_matcher_new(void)
{
	struct _vte_matcher *ret = NULL;

        g_mutex_lock(&_vte_matcher_mutex);

        if (_vte_matcher_ref_count++ == 0) {
                g_assert(_vte_matcher_singleton == NULL);
                ret = _vte_matcher_create();

                if (ret->match == NULL) {
                        ret->impl = ret->impl->klass->create();
                        ret->match = ret->impl->klass->match;
                        _vte_matcher_init(ret);
                }
                _vte_matcher_singleton = ret;
	}

        g_mutex_unlock(&_vte_matcher_mutex);
        return _vte_matcher_singleton;
}

/* Free a matcher. */
void
_vte_matcher_free(struct _vte_matcher *matcher)
{
        g_assert(_vte_matcher_singleton != NULL);
        g_mutex_lock(&_vte_matcher_mutex);
        if (--_vte_matcher_ref_count == 0) {
                _vte_matcher_destroy(matcher);
                _vte_matcher_singleton = NULL;
        }
        g_mutex_unlock(&_vte_matcher_mutex);
}

/* Check if a string matches a sequence the matcher knows about. */
const char *
_vte_matcher_match(struct _vte_matcher *matcher,
		   const gunichar *pattern, gssize length,
		   const char **res, const gunichar **consumed,
		   GValueArray **array)
{
	if (G_UNLIKELY (array != NULL && matcher->free_params != NULL)) {
		*array = matcher->free_params;
		matcher->free_params = NULL;
	}
	return matcher->match(matcher->impl, pattern, length,
					res, consumed, array);
}

/* Dump out the contents of a matcher, mainly for debugging. */
void
_vte_matcher_print(struct _vte_matcher *matcher)
{
	matcher->impl->klass->print(matcher->impl);
}

/* Free a parameter array.  Most of the GValue elements can clean up after
 * themselves, but we're using gpointers to hold unicode character strings, and
 * we need to free those ourselves. */
void
_vte_matcher_free_params_array(struct _vte_matcher *matcher,
		               GValueArray *params)
{
	guint i;
	for (i = 0; i < params->n_values; i++) {
		GValue *value = &params->values[i];
		if (G_UNLIKELY (g_type_is_a (value->g_type, G_TYPE_POINTER))) {
			g_free (g_value_get_pointer (value));
		}
	}
	if (G_UNLIKELY (matcher == NULL || matcher->free_params != NULL)) {
		g_value_array_free (params);
	} else {
		matcher->free_params = params;
		params->n_values = 0;
	}
}
