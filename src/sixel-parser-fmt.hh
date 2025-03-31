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

#include "sixel-parser.hh"

#include <string_view>

#include <fmt/format.h>

namespace vte::sixel
{
        namespace detail {


        } // namespace detail

        namespace detail {


        } // namespace detail
} // namespace vte::sixel

FMT_BEGIN_NAMESPACE

template<>
struct formatter<vte::sixel::Command, char> : public formatter<std::string_view> {
public:
        auto format(vte::sixel::Command cmd,
                    format_context& ctx) const -> format_context::iterator
        {
                using namespace std::literals::string_view_literals;

                auto str = ""sv;
                switch (cmd) {
                        using enum vte::sixel::Command;
                case DECGRI: str = "DECGRI"sv; break;
                case DECGRA: str = "DECGRA"sv; break;
                case DECGCI: str = "DECGCI"sv; break;
                case DECGCR: str = "DECGCR"sv; break;
                case DECGCH: str = "DECGCH"sv; break;
                case DECGNL: str = "DECGNL"sv; break;
                case RESERVED_2_05: str = "UNK 2/5"sv; break;
                case RESERVED_2_06: str = "UNK 2/6"sv; break;
                case RESERVED_2_07: str = "UNK 2/7"sv; break;
                case RESERVED_2_08: str = "UNK 2/8"sv; break;
                case RESERVED_2_09: str = "UNK 2/9"sv; break;
                case RESERVED_2_10: str = "UNK 2/10"sv; break;
                case RESERVED_2_12: str = "UNK 2/12"sv; break;
                case RESERVED_2_14: str = "UNK 2/14"sv; break;
                case RESERVED_2_15: str = "UNK 2/15"sv; break;
                case RESERVED_3_12: str = "UNK 3/12"sv; break;
                case RESERVED_3_13: str = "UNK 3/13"sv; break;
                case RESERVED_3_14: str = "UNK 3/14"sv; break;
                default: __builtin_unreachable();
                }

                return formatter<std::string_view, char>::format(str, ctx);
        }
};

template<>
struct formatter<vte::sixel::Sequence> : public formatter<std::string_view> {
public:

        auto format(vte::sixel::Sequence const& seq,
                    format_context& ctx) const -> format_context::iterator
        {
                auto&& it = ctx.out();

                *it = '{'; ++it;
                it = format_to(it, "{}", seq.command());

                if (auto const size = seq.size();
                    size > 0) {
                        *it = ' '; ++it;

                        for (auto i = 0u; i < size; i++) {
                                if (!seq.param_default(i))
                                        it = format_to(it, "{}", seq.param(i));
                                if (i + 1 < size) {
                                        *it = ';'; ++it;
                                }
                        }
                }

                *it = '}'; ++it;
                ctx.advance_to(it);
                return it;
        }

}; // class formatter

FMT_END_NAMESPACE
