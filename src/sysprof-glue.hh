/*
 * Copyright © 2026 Christian Hergert
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

#if WITH_SYSPROF

#include <version>

#include <sysprof-capture.h>

namespace vte::sysprof {

class InputTraceMark {
public:
        explicit InputTraceMark(char const* name) noexcept :
                m_name{name},
                m_start{sysprof_collector_is_active() ? SYSPROF_CAPTURE_CURRENT_TIME : 0}
        {
        }

        ~InputTraceMark() noexcept
        {
                if (m_start != 0)
                        sysprof_collector_mark(m_start,
                                               SYSPROF_CAPTURE_CURRENT_TIME - m_start,
                                               "VTE", m_name, nullptr);
        }

        InputTraceMark(InputTraceMark const&) = delete;
        InputTraceMark& operator=(InputTraceMark const&) = delete;

private:
        char const* m_name;
        int64_t m_start;
};

} // namespace vte::sysprof

#if defined( __cpp_placeholder_variables) && __cpp_placeholder_variables >= 202306L
#define VTE_SYSPROF_MARK(name) auto const _ = vte::sysprof::InputTraceMark{name}
#else
#define VTE_SYSPROF_MARK(name) auto const G_PASTE(__mark_, __LINE__) = vte::sysprof::InputTraceMark{name}
#endif

#else // !WITH_SYSPROF

#define VTE_SYSPROF_MARK(name)

#endif // WITH_SYSPROF
