/*
 * Copyright © 2018 Christian Persch
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

#include <cstdint>
#include <list>
#include <memory>
#include <stack>

namespace vte {

namespace base {

class Chunk {
private:
        class Recycler {
        public:
                void operator()(Chunk* chunk) {
                        if (chunk == nullptr)
                                return;
                        chunk->recycle();
                }
        };

        void recycle() noexcept;

        static unsigned int const k_max_free_chunks = 16;

        enum class Flags : uint8_t {
                eSEALED = 1u << 0,
                eEOS    = 1u << 1,
        };

public:
        using unique_type = std::unique_ptr<Chunk, Recycler>;

        static unsigned int const k_chunk_size = 0x2000;

        unsigned int len{0};
        uint8_t m_flags{0};
        uint8_t dataminusone;    /* Hack: Keep it right before data, so that data[-1] is valid and usable */
        uint8_t data[k_chunk_size - 2 * sizeof(void*) - 2 - sizeof(unsigned int)];

        Chunk() = default;
        Chunk(Chunk const&) = delete;
        Chunk(Chunk&&) = delete;
        ~Chunk() = default;

        Chunk& operator= (Chunk const&) = delete;
        Chunk& operator= (Chunk&&) = delete;

        void reset() noexcept
        {
                len = 0;
                m_flags = 0;
        }

        inline constexpr size_t capacity() const noexcept { return sizeof(data); }
        inline constexpr size_t remaining_capacity() const noexcept { return capacity() - len; }

        static unique_type get() noexcept;
        static void prune(unsigned int max_size = k_max_free_chunks) noexcept;

        inline constexpr bool sealed() const noexcept { return m_flags & (uint8_t)Flags::eSEALED; }
        inline void set_sealed() noexcept { m_flags |= (uint8_t)Flags::eSEALED; }

        inline constexpr bool eos() const noexcept { return m_flags & (uint8_t)Flags::eEOS; }
        inline void set_eos() noexcept { m_flags |= (uint8_t)Flags::eEOS; }

private:

        /* Note that this is using the standard deleter, not Recycler */
        static std::stack<std::unique_ptr<Chunk>, std::list<std::unique_ptr<Chunk>>> g_free_chunks;
};

} // namespace base

} // namespace vte
