/*
 * Copyright © 2015, 2020 Christian Persch
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cassert>
#include <cerrno>

#include <unistd.h>

namespace vte::libc {

class ErrnoSaver {
public:
        ErrnoSaver() noexcept : m_errsv{errno} { }
        ~ErrnoSaver() noexcept { errno = m_errsv; }

        ErrnoSaver(ErrnoSaver const&) = delete;
        ErrnoSaver(ErrnoSaver&&) = delete;
        ErrnoSaver& operator=(ErrnoSaver const&) = delete;
        ErrnoSaver& operator=(ErrnoSaver&&) = delete;

        inline constexpr operator int () const noexcept { return m_errsv; }

        inline void reset() noexcept { m_errsv = 0; }

private:
        int m_errsv;
}; // class ErrnoSaver

class FD {
public:
        constexpr FD() noexcept = default;
        explicit constexpr FD(int fd) noexcept : m_fd{fd} { } // adopts the FD
        constexpr FD(FD const&) = delete;
        constexpr FD(FD&& rhs) noexcept : m_fd{rhs.release()} { }

        ~FD() { reset(); }

        // adopt the file descriptor
        FD& operator=(int rhs) noexcept
        {
                reset();
                m_fd = rhs;
                return *this;
        }

        FD& operator=(FD& rhs) = delete;

        FD& operator=(FD&& rhs) noexcept
        {
                reset();
                m_fd = rhs.release();
                return *this;
        }

        constexpr operator bool() const noexcept { return m_fd != -1; }

        constexpr int get() const noexcept { return m_fd; }

        constexpr int release() noexcept
        {
                auto fd = m_fd;
                m_fd = -1;
                return fd;
        }

        void reset()
        {
                if (m_fd != -1) {
                        auto errsv = ErrnoSaver{};
                        close(m_fd);
                        m_fd = -1;
                }
        }

private:
        int m_fd{-1};

}; // class FD

constexpr bool operator==(FD const& lhs, FD const& rhs) { return lhs.get() == rhs.get(); }
constexpr bool operator==(FD const& lhs, int rhs) { return lhs.get() == rhs; }
constexpr bool operator!=(FD const& lhs, FD const& rhs) { return !(lhs == rhs); }
constexpr bool operator!=(FD const& lhs, int rhs) { return !(lhs == rhs); }

} // namespace vte::libc
