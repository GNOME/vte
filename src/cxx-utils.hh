/*
 * Copyright © 2020 Christian Persch
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

#include <algorithm>
#include <exception>
#include <type_traits>

namespace vte {

// This is like std::clamp, except that it doesn't throw when
// max_v<min_v, but instead returns min_v in that case.
template<typename T>
constexpr inline T const&
clamp(T const& v,
      T const& min_v,
      T const& max_v)
{
        return std::max(std::min(v, max_v), min_v);
}

#ifdef VTE_DEBUG
void log_exception(char const* func = __builtin_FUNCTION(),
                   char const* filename = __builtin_FILE(),
                   int const line = __builtin_LINE()) noexcept;
#else
inline void log_exception() noexcept { }
#endif

} // namespace vte
