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

#include "config.h"

#include "fmt-glue.hh"

#include <glib.h>

#include <memory>

#include <simdutf.h>

FMT_BEGIN_NAMESPACE

auto
formatter<vte::boxed<std::u32string_view>, char>::format(vte::boxed<std::u32string_view> const& str,
                                                         format_context& ctx) const -> format_context::iterator
{
        auto u8len = simdutf::utf8_length_from_utf32(str.get());
        // alternatively: auto u8len = 4 * u32str.size();
        auto u8buf = std::make_unique_for_overwrite<char[]>(u8len);
        if (!u8buf)
                return ctx.out();
        u8len = simdutf::convert_utf32_to_utf8
                (str.get(), std::span<char>(u8buf.get(), u8len));

        return formatter<std::string_view>::format(std::string_view{u8buf.get(), u8len}, ctx);
};

auto
formatter<vte::boxed<char32_t>, char>::format(vte::boxed<char32_t> const& boxchar,
                                              format_context& ctx) const -> format_context::iterator
{
        auto const c = boxchar.get();
        auto const printable = g_unichar_isprint(c);

        auto&& it = ctx.out();
        if (printable) [[likely]] {
                char ubuf[8];
                auto const len = g_unichar_to_utf8(c, ubuf);
                if (m_codepoint) {
                        it = format_to(it, "<U+{:04X} {}>",
                                       uint32_t(c),
                                       std::string_view(ubuf, len));
                } else {
                        it = format_to(it, "{}",
                                       std::string_view(ubuf, len));
                }
        } else {
                it = format_to(it, "<U+{:04X}>", uint32_t(c));
        }

        ctx.advance_to(it);
        return it;
}

FMT_END_NAMESPACE
