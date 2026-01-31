/*
 * Copyright © 2013-2015 Red Hat, Inc.
 * Copyright © 2022 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 *(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Authors: Marc-André Lureau <marcandre.lureau@redhat.com>
 */

#include "config.h"

#include <cstring>
#include <stdexcept>

#include "uuid.hh"
#include "uuid-fmt.hh"

#include "cxx-utils.hh"

using namespace std::literals::string_view_literals;

/*
 * SECTION:uuid
 * @title: vte::uuid
 * @short_description: a universal unique identifier
 *
 * A UUID, or Universally unique identifier, is intended to uniquely
 * identify information in a distributed environment. For the
 * definition of UUID, see <ulink
 * url="tools.ietf.org/html/rfc4122.html">RFC 4122</ulink>.
 *
 * The creation of UUIDs does not require a centralized authority.
 *
 * UUIDs are of relatively small size(128 bits, or 16 bytes). The
 * common string representation(ex:
 * 1d6c0810-2bd6-45f3-9890-0268422a6f14) needs 37 bytes.
 *
 * There are different mechanisms to generate UUIDs. The UUID
 * specification defines 5 versions. If all you want is a unique ID,
 * you should probably call uuid_string_random() or
 * uuid(uuid_v4_t);
 *
 * If you want to generate UUID based on a name within a namespace
 *(%G_UUID_NAMESPACE_DNS for fully-qualified domain name for
 * example), you may want to use version 5, uuid(uuid_v5_t); using a
 * SHA-1 hash, or its alternative based on a MD5 hash, version 3
 * uuid(uuid_v3_t);
 */

namespace vte {

std::string
uuid::str(uuid::format fmt) const
{
        auto fmtstr = ""sv;
        switch (fmt) {
                using enum vte::uuid::format;
        case SIMPLE: fmtstr = "{:s}"sv; break;
        case BRACED: fmtstr = "{:b}"sv; break;
        case URN:    fmtstr = "{:u}"sv; break;
                //case ID128:  fmtstr = "{:i}"sv;
        default: __builtin_unreachable(); return {};
        }
        return fmt::format(fmt::runtime(fmtstr), *this);
}

// FIXME: replace g_ascii_xdigit_value use to make this constructor constexpr
uuid::uuid(std::string_view str,
           uuid::format fmt) // throws
{
        auto dashes = 0b0000'0010'1010'1000u;

        if (str.starts_with("urn:uuid:")) {
                if ((fmt & uuid::format::URN) == uuid::format{})
                        throw std::invalid_argument{"urn format not accepted"};

                str.remove_prefix(strlen("urn:uuid:"));
        } else if (str.starts_with('{')) {
                if (!str.ends_with('}'))
                        throw std::invalid_argument{"Closing brace not found"};

                if ((fmt & uuid::format::BRACED) == uuid::format{})
                    throw std::invalid_argument{"braced format not accepted"};

                str.remove_prefix(1);
                str.remove_suffix(1);
        } else {
                if ((fmt & uuid::format::SIMPLE) == uuid::format{})
                        throw std::invalid_argument{"simple format not accepted"};
        }

        if (str.size() != 36)
                throw std::invalid_argument{"Invalid length"};

        for (auto i = 0u, j = 0u; i < 16; ) {
                auto const hi = g_ascii_xdigit_value(str[j++]);
                auto const lo = g_ascii_xdigit_value(str[j++]);

                if (dashes & (1u << i)) {
                        if (str[j++] != '-')
                                throw std::invalid_argument{"Invalid character"};
                }

                if (hi == -1 || lo == -1)
                        throw std::invalid_argument{"Invalid value"};

                m_bytes[i++] = hi << 4 | lo;
        }

        if (is_nil()) [[unlikely]] // special exception, don't check version/variant
                return;

        if (auto const v = version(); v == 0 || v > 5) [[unlikely]]
                throw std::invalid_argument{"Invalid version"};

        if (auto const v = variant(); v != 2) [[unlikely]]
                throw std::invalid_argument{"Invalid variant"};
}

bool
uuid_string_is_valid(std::string_view const& str,
                     uuid::format fmt) noexcept
{
        try {
                uuid{str, fmt};
                return true;
        } catch (...) {
                return false;
        }
}

uuid::uuid(uuid_v4_t) noexcept
{
        auto ints = (uint32_t*)m_bytes;
        for (auto i = 0; i < 4; i++)
                ints[i] = g_random_int();

        set_version(4);
}

/*
 * uuid_string_random:
 *
 * Generates a random UUID(RFC 4122 version 4) as a string.
 */
std::string
uuid_string_random(uuid::format fmt)
{
        auto const u = uuid{uuid_v4};
        return u.str(fmt);
}

uuid::uuid(int version,
           uuid const& name_space,
           std::string_view const& name) noexcept
{
        auto checksum_type = [](int v) -> auto {
                switch(v) {
                case 3: return G_CHECKSUM_MD5;
                case 5: return G_CHECKSUM_SHA1;
                default: __builtin_unreachable();
                }
        };

        auto const type = checksum_type(version);
        auto digest_len = g_checksum_type_get_length(type);
        assert(digest_len != -1);

        auto checksum = vte::take_freeable(g_checksum_new(type));
        assert(checksum);

        g_checksum_update(checksum.get(), name_space.m_bytes, sizeof(name_space.m_bytes));
        g_checksum_update(checksum.get(), reinterpret_cast<guchar const*>(name.data()), name.size());

        auto digest = reinterpret_cast<uint8_t*>(g_alloca(digest_len));
        g_checksum_get_digest(checksum.get(), digest, (gsize*)&digest_len);
        assert(digest_len >= 16);

        std::memcpy(m_bytes, digest, 16);
        set_version(version);
}

} // namespace vte

FMT_BEGIN_NAMESPACE

auto
formatter<vte::uuid>::format(vte::uuid const& u,
                             format_context& ctx) const -> format_context::iterator
{
        auto&& it = ctx.out();

        auto dashes = 0b0000'0010'1010'1000u;
        switch (m_format) {
                using enum vte::uuid::format;
        case SIMPLE: break;
        case BRACED: *it = '{'; ++it; break;
        case URN: it = fmt::format_to(it, "urn:uuid:"); break;
                // case ID128: dashes = 0u; break;
        default: __builtin_unreachable();
                g_assert_not_reached();
                break;
        }

        auto const bytes = u.bytes();
        for (auto i = 0u; i < 16; ++i) {
                it = fmt::format_to(it, "{:02x}", bytes[i]);

                if (dashes & (1u << i)) {
                        *it = '-';
                        ++it;
                }
        }

        if (m_format == vte::uuid::format::BRACED) {
                *it = '}';
                ++it;
        }

        ctx.advance_to(it);
        return it;
}

FMT_END_NAMESPACE
