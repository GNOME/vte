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

// This is inspired by https://github.com/contour-terminal/boxed-cpp
// but we need this for non-enum/integral/floating types.

#include <type_traits>
#include <utility>

namespace vte {

template<typename T,
         typename Tag = void>
struct boxed {
protected:
        T m_value;

public:
        using element_type = T;

        constexpr boxed() = default;
        constexpr boxed(boxed const&) = default;
        constexpr boxed(boxed&& v) = default;
        explicit constexpr boxed(T v) : m_value{v} { }

        constexpr boxed(std::in_place_t, auto&&... args)
                : m_value(std::forward<decltype(args)>(args)...)
        {
        }

        ~boxed() = default;

        constexpr boxed& operator=(boxed const&) = default;
        constexpr boxed& operator=(boxed&&) = default;

        constexpr auto& get() noexcept { return m_value; }
        constexpr auto const& get() const noexcept { return m_value; }

        friend constexpr auto operator<=>(boxed const&, boxed const&) = default;

        template<std::three_way_comparable_with<T> U>
        friend constexpr auto operator<=>(boxed const& b, U const& u) { return b.m_value <=> u; }

}; // struct boxed

template<typename T,
         typename Tag>
constexpr auto make_boxed(auto&&... args)
{
        return boxed<T, Tag>(std::in_place, std::forward<decltype(args)>(args)...);
}

// Type traits

template<typename T>
struct is_boxed : std::false_type { };

template<typename T>
constexpr bool is_boxed_v = is_boxed<T>::value;

template<typename T,
         typename Tag>
struct is_boxed<boxed<T, Tag>> : std::true_type { };

// Concepts

template<typename T>
concept boxed_concept = requires { is_boxed_v<T>; };

} // namespace vte
