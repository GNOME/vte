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

#include "uuid.hh"

#include <fmt/format.h>

FMT_BEGIN_NAMESPACE

template<>
struct formatter<vte::uuid> : public fmt::formatter<std::string> {
private:
        vte::uuid::format m_format{vte::uuid::format::SIMPLE};

public:

        constexpr formatter() = default;

        constexpr formatter(vte::uuid::format fmt)
                : m_format{fmt}
        {
        }

        constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
        {
                auto it = ctx.begin();
                auto const end = ctx.end();
                if (it == end || *it == '}')
                        return ctx.begin(); // default: SIMPLE format
                auto const fmt = *it;
                ++it;
                if (it != end && *it != '}')
                        throw format_error{"Invalid format"};

                if (fmt == 's')
                        m_format = vte::uuid::format::SIMPLE;
                else if (fmt == 'b')
                        m_format = vte::uuid::format::BRACED;
                else if (fmt == 'u')
                        m_format = vte::uuid::format::URN;
                //else if (fmt == 'i')
                //m_format = vte::uuid::format::ID128;
                else
                        throw format_error{"Invalid format"};

                return it;
        }

        auto format(vte::uuid const& u,
                    format_context& ctx) const -> format_context::iterator;
};

FMT_END_NAMESPACE
