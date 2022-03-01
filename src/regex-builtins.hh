/*
 * Copyright © 2019 Christian Persch
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

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "regex.hh"
#include "refptr.hh"

namespace vte {

namespace base {

class RegexBuiltins {
private:
        static inline std::weak_ptr<RegexBuiltins> s_weak_ptr{};

        std::vector<std::pair<RefPtr<Regex>, int>> m_builtins{};

        enum class InternalBuiltinsTag : int {
                eURL      = -2,
                eHTTP     = -3,
                eFILE     = -4,
                eVOIP     = -5,
                eEMAIL    = -6,
                eNEWS_MAN = -7
        };

        void compile_builtin(std::string_view const& pattern,
                             InternalBuiltinsTag tag) noexcept;

public:
        // these must have the same values as the public VteBuiltinMatchTag
        enum class BuiltinsTag : int {
                eURI = -2
        };

        RegexBuiltins();
        ~RegexBuiltins() { }
        RegexBuiltins(RegexBuiltins const&) = delete;
        RegexBuiltins(RegexBuiltins&&) = delete;

        RegexBuiltins& operator= (RegexBuiltins const&) = delete;
        RegexBuiltins& operator= (RegexBuiltins&&) = delete;

        inline constexpr auto const& builtins() const noexcept { return m_builtins; }

        int transform_match(char*& match,
                            int tag) const noexcept;

        static std::shared_ptr<RegexBuiltins> get()
        {
                auto inst = s_weak_ptr.lock();
                if (!inst)
                        s_weak_ptr = inst = std::make_shared<RegexBuiltins>();
                return inst;
        }
};

} // namespace base

} // namespace vte
