/*
 * Copyright © 2015, 2019 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>
#include <optional>
#include <string>

#include <glib.h>

#include "vtepcre2.h"

#include "cxx-utils.hh"

namespace vte {

namespace base {

class Regex {
public:
        enum class Purpose {
               eMatch,
               eSearch,
        };

        static bool check_pcre_config_unicode(GError** error);
        static bool check_pcre_config_jit(void);
        static Regex* compile(Purpose purpose,
                              std::string_view const& pattern,
                              uint32_t flags,
                              GError** error);

private:
        mutable volatile int m_refcount{1};

        using code_type = vte::FreeablePtr<pcre2_code_8, decltype(&pcre2_code_free_8), &pcre2_code_free_8>;
        code_type m_code{};

        Purpose m_purpose;

public:
        Regex(pcre2_code_8* code,
              Purpose purpose) noexcept :
                m_code{code},
                m_purpose{purpose}
        { }

        Regex(Regex const&) = delete;
        Regex(Regex&&) = delete;
        Regex operator=(Regex const&) = delete;
        Regex operator=(Regex&&) = delete;

        Regex* ref() noexcept;
        void unref() noexcept;

        pcre2_code_8* code() const noexcept { return m_code.get(); }
        constexpr inline bool has_purpose(Purpose purpose) const noexcept { return m_purpose == purpose; }
        bool has_compile_flags(uint32_t flags ) const noexcept;

        bool jit(uint32_t flags,
                 GError** error) noexcept;

        bool jited() const noexcept;

        std::optional<std::string> substitute(std::string_view const& subject,
                                              std::string_view const& replacement,
                                              uint32_t flags,
                                              GError** error) const;

}; // class Regex

} // namespace base

} // namespace vte
