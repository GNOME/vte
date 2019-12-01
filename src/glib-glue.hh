/*
 * Copyright © 2019 Christian Persch
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

#include <memory>

#include <glib.h>

namespace vte::glib {

class Error {
public:
        Error() = default;
        ~Error() { reset(); }

        Error(Error const&) = delete;
        Error(Error&&) = delete;
        Error& operator=(Error const&) = delete;
        Error& operator=(Error&&) = delete;

        operator GError* ()  noexcept { return m_error; }
        operator GError** () noexcept { return &m_error; }

        auto error()   const noexcept { return m_error != nullptr; }
        auto domain()  const noexcept { return error() ? m_error->domain : 0; }
        auto code()    const noexcept { return error() ? m_error->code : -1; }
        auto message() const noexcept { return error() ? m_error->message : nullptr; }

        void assert_no_error() const noexcept { g_assert_no_error(m_error); }

        bool matches(GQuark domain, int code) const noexcept
        {
                return error() && g_error_matches(m_error, domain, code);
        }

        void reset() noexcept { g_clear_error(&m_error); }

        bool propagate(GError** error) noexcept { *error = m_error; m_error = nullptr; return false; }

private:
        GError* m_error{nullptr};
};

} // namespace vte::glib
