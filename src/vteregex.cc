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
#include "vtepcre2.h"

#include "regex.hh"
#include "vteregexinternal.hh"

#define WRAPPER(impl) (reinterpret_cast<VteRegex*>(impl))
#define IMPL(wrapper) (reinterpret_cast<vte::base::Regex*>(wrapper))

/* GRegex translation */

typedef struct {
        guint32 gflag;
        guint32 pflag;
} FlagTranslation;

static void
translate_flags(FlagTranslation const* const table,
                gsize table_len,
                guint32 *gflagsptr /* inout */,
                guint32 *pflagsptr /* inout */)
{
        auto gflags = *gflagsptr;
        auto pflags = *pflagsptr;
        for (guint i = 0; i < table_len; i++) {
                auto gflag = table[i].gflag;
                if ((gflags & gflag) == gflag) {
                        pflags |= table[i].pflag;
                        gflags &= ~gflag;
                }
        }

        *gflagsptr = gflags;
        *pflagsptr = pflags;
}

/* Type registration */

#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
G_DEFINE_BOXED_TYPE(VteRegex, vte_regex,
                    vte_regex_ref, (GBoxedFreeFunc)vte_regex_unref)
#pragma GCC diagnostic pop

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
        g_return_val_if_fail(regex != nullptr, nullptr);

        return WRAPPER(IMPL(regex)->ref());
}

/**
 * vte_regex_unref:
 * @regex: (transfer full): a #VteRegex
 *
 * Decreases the reference count of @regex by one, and frees @regex
 * if the refcount reaches zero.
 *
 * Returns: %NULL
 */
VteRegex *
vte_regex_unref(VteRegex* regex)
{
        g_return_val_if_fail(regex != nullptr, nullptr);

        IMPL(regex)->unref();
        return nullptr;
}

static VteRegex*
vte_regex_new(vte::base::Regex::Purpose purpose,
              char const* pattern,
              ssize_t pattern_length,
              uint32_t flags,
              GError** error)
{
        return WRAPPER(vte::base::Regex::compile(purpose, pattern, pattern_length, flags, error));
}

VteRegex*
_vte_regex_new_gregex(vte::base::Regex::Purpose purpose,
                      GRegex *gregex)
{
        g_return_val_if_fail(gregex != NULL, NULL);

        guint32 pflags = 0;

        static FlagTranslation const table[] = {
                { G_REGEX_CASELESS,        PCRE2_CASELESS        },
                { G_REGEX_MULTILINE,       PCRE2_MULTILINE       },
                { G_REGEX_DOTALL,          PCRE2_DOTALL          },
                { G_REGEX_EXTENDED,        PCRE2_EXTENDED        },
                { G_REGEX_ANCHORED,        PCRE2_ANCHORED        },
                { G_REGEX_DOLLAR_ENDONLY,  PCRE2_DOLLAR_ENDONLY  },
                { G_REGEX_UNGREEDY,        PCRE2_UNGREEDY        },
                { G_REGEX_NO_AUTO_CAPTURE, PCRE2_NO_AUTO_CAPTURE },
                { G_REGEX_OPTIMIZE,        0                     }, /* accepted but unused */
                { G_REGEX_FIRSTLINE,       PCRE2_FIRSTLINE       },
                { G_REGEX_DUPNAMES,        PCRE2_DUPNAMES        }
        };

        /* Always add the MULTILINE option */
        guint32 gflags = g_regex_get_compile_flags(gregex) | G_REGEX_MULTILINE;
        translate_flags(table, G_N_ELEMENTS(table), &gflags, &pflags);

        if (gflags != 0) {
                g_warning("Incompatible GRegex compile flags left untranslated: %08x", gflags);
        }

        GError* err = nullptr;
        auto regex = vte_regex_new(purpose, g_regex_get_pattern(gregex), -1, pflags, &err);
        if (regex == nullptr) {
                g_warning("Failed to translated GRegex: %s", err->message);
                g_error_free(err);
        }
        return regex;
}

guint32
_vte_regex_translate_gregex_match_flags(GRegexMatchFlags flags)
{
        static FlagTranslation const table[] = {
                { G_REGEX_MATCH_ANCHORED,         PCRE2_ANCHORED         },
                { G_REGEX_MATCH_NOTBOL,           PCRE2_NOTBOL           },
                { G_REGEX_MATCH_NOTEOL,           PCRE2_NOTEOL           },
                { G_REGEX_MATCH_NOTEMPTY,         PCRE2_NOTEMPTY         },
                { G_REGEX_MATCH_NOTEMPTY_ATSTART, PCRE2_NOTEMPTY_ATSTART }
        };

        guint32 gflags = flags;
        guint32 pflags = 0;
        translate_flags(table, G_N_ELEMENTS(table), &gflags, &pflags);
        if (gflags != 0) {
                g_warning("Incompatible GRegex match flags left untranslated: %08x", gflags);
        }

        return pflags;
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
        return vte_regex_new(vte::base::Regex::Purpose::eMatch,
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
        return vte_regex_new(vte::base::Regex::Purpose::eSearch,
                             pattern, pattern_length,
                             flags,
                             error);
}

/**
 * vte_regex_jit:
 * @regex: a #VteRegex
 *
 * If the platform supports JITing, JIT compiles @regex.
 *
 * Returns: %TRUE if JITing succeeded (or PCRE2 was built without
 *   JIT support), or %FALSE with @error filled in
 */
gboolean
vte_regex_jit(VteRegex *regex,
              guint     flags,
              GError  **error)
{
        g_return_val_if_fail(regex != nullptr, false);

        return IMPL(regex)->jit(flags, error);
}

bool
_vte_regex_has_purpose(VteRegex *regex,
                       vte::base::Regex::Purpose purpose)
{
        g_return_val_if_fail(regex != nullptr, false);

        return IMPL(regex)->has_purpose(purpose);
}

const pcre2_code_8 *
_vte_regex_get_pcre(VteRegex* regex)
{
        g_return_val_if_fail(regex != nullptr, nullptr);

        return IMPL(regex)->code();
}

bool
_vte_regex_get_jited(VteRegex *regex)
{
        g_return_val_if_fail(regex != nullptr, false);

        return IMPL(regex)->jited();
}

bool
_vte_regex_has_multiline_compile_flag(VteRegex *regex)
{
        g_return_val_if_fail(regex != nullptr, 0);

        return IMPL(regex)->has_compile_flags(PCRE2_MULTILINE);
}

/**
 * vte_regex_substitute:
 * @regex: a #VteRegex
 * @subject: the subject string
 * @replacement: the replacement string
 * @flags: PCRE2 match flags
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * See man:pcre2api(3) on pcre2_substitute() for more information.
 *
 * Returns: (transfer full): the substituted string, or %NULL
 *   if an error occurred
 *
 * Since: 0.56
 */
char *
vte_regex_substitute(VteRegex *regex,
                     const char *subject,
                     const char *replacement,
                     guint32 flags,
                     GError **error)
{
        g_return_val_if_fail(regex != nullptr, nullptr);
        g_return_val_if_fail(subject != nullptr, nullptr);
        g_return_val_if_fail(replacement != nullptr, nullptr);
        g_return_val_if_fail (!(flags & PCRE2_SUBSTITUTE_OVERFLOW_LENGTH), nullptr);

        return IMPL(regex)->substitute(subject, replacement, flags, error);
}
