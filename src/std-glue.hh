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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>

namespace vte {

template<typename T>
class FreeableDeleter;

template<typename T>
using Freeable = std::unique_ptr<T, FreeableDeleter<T>>;

template<typename T>
inline auto take_freeable(T* t) { return Freeable<T>{t}; }

#define VTE_DECLARE_FREEABLE(T, func) \
template<> \
class FreeableDeleter<T> { \
public: inline void operator()(T* t) { func(t); } \
}

} // namespace vte
