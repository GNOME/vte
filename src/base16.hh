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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// FIXME: replace this with simdutf once it implements base16,
// see https://github.com/simdutf/simdutf/issues/565

namespace vte {

namespace impl {

inline constexpr auto u4_to_hex(uint8_t v) noexcept -> char
{
        switch (v) {
        case 0x0 ... 0x9: return '0' + v;
        case 0xa ... 0xf: return 'A' + (v - 0xa);
        default: __builtin_unreachable();
        }
};

inline constexpr auto hex_to_u4(char const c) noexcept -> std::optional<uint8_t>
{
        switch (c) {
        case '0' ... '9': return uint8_t(c - '0');
        case 'A' ... 'F': return uint8_t(c - 'A' + 10);
        case 'a' ... 'f': return uint8_t(c - 'a' + 10);
        default: return std::nullopt;
        }
};

} // namespace impl

constexpr inline auto base16_encode(std::span<char const> data) -> std::string
{
        auto buf = std::string{};
        buf.resize_and_overwrite
                (data.size() * 2,
                 [&](char* ptr,
                     std::size_t size) constexpr noexcept -> std::size_t {
                         for (auto&& c : data) {
                                 *ptr++ = impl::u4_to_hex(uint8_t(c) >> 4);
                                 *ptr++ = impl::u4_to_hex(uint8_t(c) & 0xf);
                         }

                         return size;
                 });

        return buf;
}

constexpr inline auto base16_decode(std::span<char const> data,
                                    bool allow_8bit = true) -> std::optional<std::string>
{
        if (data.size() % 2) [[unlikely]]
                return std::nullopt;

        auto buf = std::string{};
        buf.resize_and_overwrite
                (data.size() / 2,
                 [&](char* ptr,
                     std::size_t size) constexpr noexcept -> std::size_t {
                         auto i = 0uz;
                         while (i < data.size()) {
                                 auto const hi = impl::hex_to_u4(data[i++]);
                                 auto const lo = impl::hex_to_u4(data[i++]);
                                 if (!lo || !hi || (!allow_8bit && *hi >= 8))
                                         return 0;

                                 *ptr++ = char(*hi << 4 | *lo);
                         }

                         return size;
                 });

        return (buf.size() || !data.size()) ? std::make_optional(std::move(buf)) : std::nullopt;
}

} // namespace vte
