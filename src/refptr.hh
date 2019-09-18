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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>
#include <glib-object.h>

namespace vte {

namespace glib {

template <typename T>
class RefPtr : public std::unique_ptr<T, decltype(&g_object_unref)>
{
private:
        using base_type = std::unique_ptr<T, decltype(&g_object_unref)>;

public:
        RefPtr(T* obj = nullptr) : base_type{obj, &g_object_unref} { }
};

} // namespace glib
} // namespace vte
