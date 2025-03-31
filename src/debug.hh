/*
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

/* The interfaces in this file are subject to change at any time. */

#pragma once

#include <stdint.h>
#include <glib.h>

#include "cxx-utils.hh"

#if VTE_DEBUG
#include <fmt/format.h>
#endif

namespace vte::debug {

enum class category : unsigned {
        NONE          = 0,
        ALL           = ~0u,
        MISC          = 1u << 0,
        PARSER        = 1u << 1,
        IO            = 1u << 2,
        UPDATES       = 1u << 3,
        EVENTS        = 1u << 4,
        SIGNALS       = 1u << 5,
        SELECTION     = 1u << 6,
        SUBSTITUTION  = 1u << 7,
        RING          = 1u << 8,
        PTY           = 1u << 9,
        KEYBOARD      = 1u << 11,
        CELLS         = 1u << 14,
        DRAW          = 1u << 16,
        ALLY          = 1u << 17,
        ADJ           = 1u << 18,
        PANGOCAIRO    = 1u << 19,
        WIDGET_SIZE   = 1u << 20,
        RESIZE        = 1u << 22,
        REGEX         = 1u << 23,
        HYPERLINK     = 1u << 24,
        MODES         = 1u << 25,
        RINGVIEW      = 1u << 27,
        BIDI          = 1u << 28,
        CONVERSION    = 1u << 29,
        EXCEPTIONS    = 1u << 30,
        IMAGE         = 1u << 31,
};

VTE_CXX_DEFINE_BITMASK(category);

#if VTE_DEBUG
inline category debug_categories = category::NONE;
#endif

static inline bool
check_categories(category cats)
{
#if VTE_DEBUG
	return (debug_categories & cats) != category::NONE;
#else
        return false;
#endif
}

#if VTE_DEBUG

namespace detail {

static inline void
log(fmt::string_view fmt,
    fmt::format_args args)
{
        fmt::vprintln(stderr, fmt, args);
}

} // namespace detail

template<typename... T>
static inline void
println(fmt::format_string<T...> fmt,
        T&&... args) noexcept
try
{
        detail::log(fmt, fmt::make_format_args(args...));
}
catch (...)
{
}

#endif // VTE_DEBUG

} // namspace vte::debug

void _vte_debug_init(void);
const char *_vte_debug_sequence_to_string(const char *str,
                                          gssize length);

void _vte_debug_hexdump(char const* str,
                        uint8_t const* buf,
                        size_t len);

#if VTE_DEBUG
#define _VTE_DEBUG_IF(cats) if (vte::debug::check_categories(cats)) [[unlikely]]
#else
#define _VTE_DEBUG_IF(cats) if constexpr (false)
#endif

#if VTE_DEBUG
#define _vte_debug_print(cats, ...) \
	G_STMT_START { _VTE_DEBUG_IF(cats) { \
                        vte::debug::println(__VA_ARGS__); \
                } \
        } G_STMT_END
#else
#define _vte_debug_print(...) do { } while(0)
#endif // VTE_DEBUG

#ifdef G_DISABLE_ASSERT
# define vte_assert_cmpint(a,op,b) G_STMT_START {} G_STMT_END
# define vte_assert_cmpuint(a,op,b) G_STMT_START {} G_STMT_END
# define vte_assert_cmphex(a,op,b) G_STMT_START {} G_STMT_END
# define vte_assert_true(v) G_STMT_START {} G_STMT_END
# define vte_assert_false(v) G_STMT_START {} G_STMT_END
#else
# define vte_assert_cmpint(a,op,b) g_assert_cmpint(a,op,b)
# define vte_assert_cmpuint(a,op,b) g_assert_cmpuint(a,op,b)
# define vte_assert_cmphex(a,op,b) g_assert_cmphex(a,op,b)
# define vte_assert_true(v) g_assert_true(v)
# define vte_assert_false(v) g_assert_false(v)
#endif
