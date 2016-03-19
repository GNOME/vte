/*
 * Copyright © 2015 Christian Persch
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: vte-regex
 * @short_description: Regex for matching and searching. Uses PCRE2 internally.
 *
 * Since: 0.46
 */

#include "config.h"

#include "vtemacros.h"
#include "vteenums.h"
#include "vteregex.h"

#ifdef WITH_PCRE2
#include "vtepcre2.h"
#endif /* WITH_PCRE2 */

#include "vteregexinternal.hh"

struct _VteRegex {
        volatile int ref_count;
        VteRegexPurpose purpose;
#ifdef WITH_PCRE2
        pcre2_code_8 *code;
#endif /* WITH_PCRE2 */
};

#ifdef WITH_PCRE2
#define DEFAULT_COMPILE_OPTIONS (PCRE2_UTF)
#define JIT_OPTIONS (PCRE2_JIT_COMPLETE)
#define DEFAULT_MATCH_OPTIONS (0)
#else
#define DEFAULT_COMPILE_OPTIONS (0
#define JIT_OPTIONS (0)
#define DEFAULT_MATCH_OPTIONS (0)
#endif /* WITH_PCRE2 */

#ifdef WITH_PCRE2

static VteRegex *
regex_new(pcre2_code_8 *code,
          VteRegexPurpose purpose)
{
        VteRegex *regex;

        regex = g_slice_new(VteRegex);
        regex->ref_count = 1;
        regex->purpose = purpose;
        regex->code = code;

        return regex;
}

static void
regex_free(VteRegex *regex)
{
        pcre2_code_free_8(regex->code);
        g_slice_free(VteRegex, regex);
}

static gboolean
set_gerror_from_pcre_error(int errcode,
                           GError **error)
{
        PCRE2_UCHAR8 buf[128];
        int n;

        n = pcre2_get_error_message_8(errcode, buf, sizeof (buf));
        g_assert(n >= 0);
        g_set_error_literal(error, VTE_REGEX_ERROR, errcode, (const char*)buf);
        return FALSE;
}

#else

static gboolean
set_unsupported_error(GError **error)
{
        g_set_error_literal(error, VTE_REGEX_ERROR, VTE_REGEX_ERROR_NOT_SUPPORTED,
                            "PCRE2 not supported");
        return FALSE;
}

#endif /* WITH_PCRE2 */

G_DEFINE_BOXED_TYPE(VteRegex, vte_regex,
                    vte_regex_ref, (GBoxedFreeFunc)vte_regex_unref)

G_DEFINE_QUARK(vte-regex-error, vte_regex_error)

/**
 * vte_regex_ref:
 * @regex: (transfer none): a #VteRegex
 *
 * Increases the reference count of @regex by one.
 *
 * Returns: @regex
 */
VteRegex *
vte_regex_ref(VteRegex *regex)
{
        g_return_val_if_fail (regex, NULL);

#ifdef WITH_PCRE2
        g_atomic_int_inc (&regex->ref_count);
#endif
        return regex;
}

/**
 * vte_regex_ref:
 * @regex: (transfer full): a #VteRegex
 *
 * Decreases the reference count of @regex by one, and frees @regex
 * if the refcount reaches zero.
 *
 * Returns: %NULL
 */
VteRegex *
vte_regex_unref(VteRegex *regex)
{
        g_return_val_if_fail (regex, NULL);

#ifdef WITH_PCRE2
        if (g_atomic_int_dec_and_test (&regex->ref_count))
                regex_free (regex);
#endif
        return NULL;
}

static VteRegex *
vte_regex_new(VteRegexPurpose purpose,
              const char *pattern,
              gssize      pattern_length,
              guint32     flags,
              GError    **error)
{
#ifdef WITH_PCRE2
        pcre2_code_8 *code;
        int r, errcode;
        guint32 v;
        PCRE2_SIZE erroffset;

        g_return_val_if_fail(pattern != NULL, NULL);
        g_return_val_if_fail(pattern_length >= -1, NULL);
        g_return_val_if_fail(error == NULL || *error == NULL, NULL);

        /* Check library compatibility */
        r = pcre2_config_8(PCRE2_CONFIG_UNICODE, &v);
        if (r != 0 || v != 1) {
                g_set_error(error, VTE_REGEX_ERROR, VTE_REGEX_ERROR_INCOMPATIBLE,
                            "PCRE2 library was built without unicode support");
                return NULL;
        }

        code = pcre2_compile_8((PCRE2_SPTR8)pattern,
                               pattern_length >= 0 ? pattern_length : PCRE2_ZERO_TERMINATED,
                               (uint32_t)flags |
                               PCRE2_UTF |
                               (flags & PCRE2_UTF ? PCRE2_NO_UTF_CHECK : 0) |
                               PCRE2_NEVER_BACKSLASH_C |
                               PCRE2_USE_OFFSET_LIMIT,
                               &errcode, &erroffset,
                               NULL);

        if (code == nullptr) {
                set_gerror_from_pcre_error(errcode, error);
                g_prefix_error(error, "Failed to compile pattern to regex at offset %" G_GSIZE_FORMAT ":",
                               erroffset);
                return NULL;
        }

        return regex_new(code, purpose);
#else
        set_unsupported_error(error);
        return NULL;
#endif /* WITH_PCRE2 */
}

/**
 * vte_regex_new_for_match:
 * @pattern: a regex pattern string
 * @pattern_length: the length of @pattern in bytes, or -1 if the
 *  string is NUL-terminated and the length is unknown
 * @flags: PCRE2 compile flags
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Compiles @pattern into a regex for use as a match regex
 * with vte_terminal_match_add_regex() or
 * vte_terminal_event_check_regex_simple().
 *
 * See man:pcre2pattern(3) for information
 * about the supported regex language.
 *
 * The regex will be compiled using %PCRE2_UTF and possibly other flags, in
 * addition to the flags supplied in @flags.
 *
 * Returns: (transfer full): a newly created #VteRegex, or %NULL with @error filled in
 */
VteRegex *
vte_regex_new_for_match(const char *pattern,
                        gssize      pattern_length,
                        guint32     flags,
                        GError    **error)
{
        return vte_regex_new(VteRegexPurpose::match,
                             pattern, pattern_length,
                             flags,
                             error);
}

/**
 * vte_regex_new_for_search:
 * @pattern: a regex pattern string
 * @pattern_length: the length of @pattern in bytes, or -1 if the
 *  string is NUL-terminated and the length is unknown
 * @flags: PCRE2 compile flags
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Compiles @pattern into a regex for use as a search regex
 * with vte_terminal_search_set_regex().
 *
 * See man:pcre2pattern(3) for information
 * about the supported regex language.
 *
 * The regex will be compiled using %PCRE2_UTF and possibly other flags, in
 * addition to the flags supplied in @flags.
 *
 * Returns: (transfer full): a newly created #VteRegex, or %NULL with @error filled in
 */
VteRegex *
vte_regex_new_for_search(const char *pattern,
                         gssize      pattern_length,
                         guint32     flags,
                         GError    **error)
{
        return vte_regex_new(VteRegexPurpose::search,
                             pattern, pattern_length,
                             flags,
                             error);
}

#if 0
/*
 * vte_regex_new_pcre:
 * @code: a #pcre2_code_8
 *
 * Creates a new #VteRegex for @code. @code must have been compiled with
 * %PCRE2_UTF and %PCRE2_NEVER_BACKSLASH_C.
 *
 * Returns: (transfer full): a newly created #VteRegex, or %NULL if VTE
 *   was not compiled with PCRE2 support.
 */
VteRegex *
vte_regex_new_pcre(pcre2_code_8 *code,
                   GError      **error)
{
#ifdef WITH_PCRE2
        guint32 flags;

        g_return_val_if_fail(code != NULL, NULL);
        g_return_val_if_fail(error == NULL || *error == NULL, NULL);

        pcre2_pattern_info_8(code, PCRE2_INFO_ALLOPTIONS, &flags);
        g_return_val_if_fail(flags & PCRE2_UTF, NULL);
        g_return_val_if_fail(flags & PCRE2_NEVER_BACKSLASH_C, NULL);

        return regex_new(code);
#else
        set_unsupported_error(error);
        return NULL;
#endif
}
#endif

gboolean
_vte_regex_has_purpose(VteRegex *regex,
                       VteRegexPurpose purpose)
{
        return regex->purpose == purpose;
}

#ifdef WITH_PCRE2
/*
 * _vte_regex_get_pcre:
 * @regex: a #VteRegex
 *
 *
 * Returns: the #pcre2_code_8 from @regex
 */
const pcre2_code_8 *
_vte_regex_get_pcre(VteRegex *regex)
{
#ifdef WITH_PCRE2
        g_return_val_if_fail(regex != NULL, NULL);

        return regex->code;
#else
        return NULL;
#endif
}
#endif

/**
 * vte_regex_jit:
 * @regex: a #VteRegex
 *
 * If the platform supports JITing, JIT compiles @regex.
 *
 * Returns: %TRUE if JITing succeeded, or %FALSE with @error filled in
 */
gboolean
vte_regex_jit(VteRegex *regex,
              guint     flags,
              GError  **error)
{
#ifdef WITH_PCRE2
        int r;

        g_return_val_if_fail(regex != NULL, FALSE);

        r = pcre2_jit_compile_8(regex->code, flags);
        if (r < 0)
                return set_gerror_from_pcre_error(r, error);

        return TRUE;
#else
        return set_unsupported_error(error);
#endif /* WITH_PCRE2 */
}

/*
 * _vte_regex_get_jited:
 *
 * Note: We can't tell if the regex has been JITed for a particular mode,
 * just if it has been JITed at all.
 *
 * Returns: %TRUE iff the regex has been JITed
 */
gboolean
_vte_regex_get_jited(VteRegex *regex)
{
#ifdef WITH_PCRE2
        PCRE2_SIZE s;
        int r;

        g_return_val_if_fail(regex != NULL, FALSE);

        r = pcre2_pattern_info_8(regex->code, PCRE2_INFO_JITSIZE, &s);

        return r == 0 && s != 0;
#else
        return FALSE;
#endif
}
