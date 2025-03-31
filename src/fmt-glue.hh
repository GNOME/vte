// Copyright © 2025 Christian Persch
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

// Vte would like to use char8_t (and/or char32_t, where appropriate)
// throughout, but unfortunately std::format doesn't support them at all.
// While fmt is slightly better in that it has at least some support for
// char{8,16,32}_t, it still misses many important things, like
// std::print, locale support, and compile-time format string checking,
// and more.
// Therefore, for the time being, or until the C++ standard or fmt adds
// support, vte will use char for UTF-8 strings in places where a
// formatted string will be needed.

#include <fmt/format.h>

// fmt doesn't just not support formatting a u32 string/string_view
// to string, but purposefully makes it impossible to implement
// formatter<std::u32_string[_view], char> yourself. Wrapping the
// u32_string[_view] works around that.

#include <string_view>

#include "boxed.hh"

FMT_BEGIN_NAMESPACE

template<>
struct formatter<vte::boxed<std::u32string_view>, char>
        : public formatter<std::string_view, char> {
public:
        auto format(vte::boxed<std::u32string_view> const& str,
                    format_context& ctx) const -> format_context::iterator;
};

template<>
struct formatter<vte::boxed<char32_t>, char> {
private:
        bool m_codepoint{false};

public:
        constexpr formatter() = default;

        constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
        {
                auto it = ctx.begin();
                while (it != ctx.end()) {
                        if (*it == 'u')
                                m_codepoint = true;
                        else if (*it == '}')
                                break;
                        else
                                throw format_error{"Invalid format string"};
                        ++it;
                }

                return it;
        }


        auto format(vte::boxed<char32_t> const& str,
                    format_context& ctx) const -> format_context::iterator;
};

FMT_END_NAMESPACE
