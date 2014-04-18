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
#include "trie.h"

struct _vte_matcher {
	_vte_matcher_match_func match; /* shortcut to the most common op */
	struct _vte_matcher_impl *impl;
	GValueArray *free_params;
};

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
static GStaticMutex _vte_matcher_mutex = G_STATIC_MUTEX_INIT;
static GCache *_vte_matcher_cache = NULL;
G_GNUC_END_IGNORE_DEPRECATIONS;

static struct _vte_matcher_impl dummy_vte_matcher_trie = {
	&_vte_matcher_trie
};
static struct _vte_matcher_impl dummy_vte_matcher_table = {
	&_vte_matcher_table
};

#ifdef VTE_COMPILATION
#include "vte-private.h"
#else
static gboolean _vte_terminal_can_handle_sequence(const char *name) { return TRUE; }
#endif

/* Add a string to the matcher. */
static void
_vte_matcher_add(const struct _vte_matcher *matcher,
		 const char *pattern, gssize length,
		 const char *result, GQuark quark)
{
	matcher->impl->klass->add(matcher->impl, pattern, length, result, quark);
}

static void
_vte_matcher_add_one(struct _vte_terminfo *terminfo,
                     const char *cap,
                     const char *compat_cap,
                     const char *value,
                     gpointer user_data)
{
        struct _vte_matcher *matcher = user_data;

        /* Skip key caps, which all start with 'k' in terminfo */
        if (cap[0] == 'k')
                return;

        /* Skip anything that doesn't start with a control character. This catches
         * ACS_CHARS and SGR, and the F0..F10 key labels (lf0..lf10).
         */
        if (value[0] >= 0x20 && value[0] < 0x7f){
                _vte_debug_print(VTE_DEBUG_PARSE,
                                 "Dropping caps %s with printable value '%s'\n",
                                 cap, _vte_terminfo_sequence_to_string(value));
                return;
        }

        /* We use the 2-character termcap code instead of the terminfo code
         * if it exists, since that makes matching faster by using vteseq-2.
         */
        if (compat_cap[0] != 0)
                cap = compat_cap;

        /* If there is no handler for it, it'd be pointless to continue. */
        if (!_vte_terminal_can_handle_sequence(cap)) {
                _vte_debug_print(VTE_DEBUG_PARSE, "No handler for cap %s with value '%s', skipping\n",
                                 cap, _vte_terminfo_sequence_to_string(value));
                return;
        }

        _vte_debug_print(VTE_DEBUG_PARSE,
                         "Adding caps %s with value '%s'\n", cap,
                         _vte_terminfo_sequence_to_string(value));

        _vte_matcher_add(matcher, value, strlen(value), cap, 0);
}

/* Loads all sequences into matcher */
static void
_vte_matcher_init(struct _vte_matcher *matcher,
                  struct _vte_terminfo *terminfo)
{
	const char *code, *value;
	int i;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "_vte_matcher_init()\n");

	if (terminfo != NULL) {
                _vte_terminfo_foreach_string(terminfo, TRUE, _vte_matcher_add_one, matcher);

                /* FIXME: we used to always add LF and CR to the matcher if they weren't in the
                 * termcap. However this seems unlikely to happen since if the terminfo is so
                 * broken it doesn't include CR and LF, everything else will be broken too.
                 */
        }

	/* Add emulator-specific sequences. */
        if (terminfo != NULL && _vte_terminfo_is_xterm_like(terminfo)) {
		/* Add all of the xterm-specific stuff. */

		for (i = 0;
		     _vte_xterm_capability_strings[i].value != NULL;
		     i++) {
			code = _vte_xterm_capability_strings[i].code;
			value = _vte_xterm_capability_strings[i].value;
			_vte_matcher_add(matcher, code, strlen (code),
					 value, 0);
		}
	}

	_VTE_DEBUG_IF(VTE_DEBUG_TRIE) {
		g_printerr("Trie contents:\n");
		_vte_matcher_print(matcher);
		g_printerr("\n");
	}
}

/* Allocates new matcher structure. */
static gpointer
_vte_matcher_create(gpointer key)
{
        struct _vte_terminfo *terminfo = key;
	struct _vte_matcher *ret = NULL;

	_vte_debug_print(VTE_DEBUG_LIFECYCLE, "_vte_matcher_create()\n");
	ret = g_slice_new(struct _vte_matcher);
	ret->impl = &dummy_vte_matcher_trie;
	ret->match = NULL;
	ret->free_params = NULL;

        /* FIXMEchpe: this means the trie one is always unused? It also seems totally broken
         * since when accidentally using it instead of table, all was messed up
         */
        if (_vte_terminfo_is_xterm_like(terminfo)) {
		ret->impl = &dummy_vte_matcher_table;
	}

	return ret;
}

/* Noone uses this matcher, free it. */
static void
_vte_matcher_destroy(gpointer value)
{
	struct _vte_matcher *matcher = value;

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
_vte_matcher_new(struct _vte_terminfo *terminfo)
{
	struct _vte_matcher *ret = NULL;

        g_return_val_if_fail(terminfo != NULL, NULL);

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	g_static_mutex_lock(&_vte_matcher_mutex);

	if (_vte_matcher_cache == NULL) {
		_vte_matcher_cache = g_cache_new((GCacheNewFunc)_vte_matcher_create,
                                                 (GCacheDestroyFunc)_vte_matcher_destroy,
                                                 (GCacheDupFunc)_vte_terminfo_ref,
                                                 (GCacheDestroyFunc)_vte_terminfo_unref,
                                                 g_direct_hash, g_direct_hash, g_direct_equal);
	}

	ret = g_cache_insert(_vte_matcher_cache, terminfo);

	if (ret->match == NULL) {
		ret->impl = ret->impl->klass->create();
		ret->match = ret->impl->klass->match;
		_vte_matcher_init(ret, terminfo);
	}

	g_static_mutex_unlock(&_vte_matcher_mutex);
        G_GNUC_END_IGNORE_DEPRECATIONS;
	return ret;
}

/* Free a matcher. */
void
_vte_matcher_free(struct _vte_matcher *matcher)
{
	g_assert(_vte_matcher_cache != NULL);
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	g_static_mutex_lock(&_vte_matcher_mutex);
	g_cache_remove(_vte_matcher_cache, matcher);
	g_static_mutex_unlock(&_vte_matcher_mutex);
        G_GNUC_END_IGNORE_DEPRECATIONS;
}

/* Check if a string matches a sequence the matcher knows about. */
const char *
_vte_matcher_match(struct _vte_matcher *matcher,
		   const gunichar *pattern, gssize length,
		   const char **res, const gunichar **consumed,
		   GQuark *quark, GValueArray **array)
{
	if (G_UNLIKELY (array != NULL && matcher->free_params != NULL)) {
		*array = matcher->free_params;
		matcher->free_params = NULL;
	}
	return matcher->match(matcher->impl, pattern, length,
					res, consumed, quark, array);
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
