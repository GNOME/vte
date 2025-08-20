/*
 * Copyright © 2020 Christian Persch
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

#include <algorithm>
#include <exception>
#include <type_traits>
#include <memory>
#include <utility>

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

#if VTE_DEBUG
void log_exception(char const* func = __builtin_FUNCTION(),
                   char const* filename = __builtin_FILE(),
                   int const line = __builtin_LINE()) noexcept;
#else
void log_exception() noexcept;
#endif

template <typename T, typename D, D func>
class FreeablePtrDeleter {
public:
        void operator()(T* obj) const
        {
                if (obj)
                        func(obj);
        }
};

template <typename T, typename D, D func>
using FreeablePtr = std::unique_ptr<T, FreeablePtrDeleter<T, D, func>>;

} // namespace vte

#define VTE_CXX_DEFINE_BITMASK(Type) \
  inline constexpr Type \
  operator&(Type lhs, Type rhs) noexcept \
  { return (Type)(std::to_underlying(lhs) & std::to_underlying(rhs)); } \
  \
  inline constexpr Type \
  operator~(Type v) noexcept \
  { return (Type)~std::to_underlying(v); }        \
  \
  inline constexpr Type \
  operator|(Type lhs, Type rhs) noexcept \
  { return (Type)(std::to_underlying(lhs) | std::to_underlying(rhs)); } \
  \
  inline constexpr Type \
  operator^(Type lhs, Type rhs) noexcept \
  { return (Type)(std::to_underlying(lhs) ^ std::to_underlying(rhs)); } \
  \
  inline constexpr Type& \
  operator&=(Type& lhs, Type rhs) noexcept \
  { return lhs = lhs & rhs; } \
  \
  inline constexpr Type& \
  operator|=(Type& lhs, Type rhs) noexcept \
  { return lhs = lhs | rhs; } \
  \
  inline constexpr Type& \
  operator^=(Type& lhs, Type rhs) noexcept \
  { return lhs = lhs ^ rhs; }

#define VTE_CXX_DEFINE_FACADE_PR(FType, IType) \
        static inline FType* \
        _vte_facade_wrap_pr(IType& ref) noexcept \
        { \
                return reinterpret_cast<FType*>(std::addressof(ref)); \
        } \
        \
        static inline FType const* \
        _vte_facade_wrap_pr(IType const& ref) noexcept \
        { \
                return reinterpret_cast<FType const*>(std::addressof(ref)); \
        } \
        \
        static inline IType& \
        _vte_facade_unwrap_pr(FType* ptr) noexcept \
        { \
                return *std::launder(reinterpret_cast<IType*>(ptr));    \
        } \
        \
        static inline IType const& \
        _vte_facade_unwrap_pr(FType const* ptr) noexcept \
        { \
                return *std::launder(reinterpret_cast<IType const*>(ptr));    \
        }

#define VTE_CXX_DEFINE_FACADE_PP(FType, IType) \
        static inline FType* \
        _vte_facade_wrap_pp(IType* ptr) noexcept \
        { \
                return reinterpret_cast<FType*>(ptr); \
        } \
        \
        static inline FType const* \
        _vte_facade_wrap_pp(IType const* ptr) noexcept \
        { \
                return reinterpret_cast<FType const*>(ptr); \
        } \
        \
        static inline IType* \
        _vte_facade_unwrap_pp(FType* ptr) noexcept \
        { \
                return std::launder(reinterpret_cast<IType*>(ptr));     \
        } \
        \
        static inline IType const* \
        _vte_facade_unwrap_pp(FType const* ptr) noexcept \
        { \
                return std::launder(reinterpret_cast<IType const*>(ptr));     \
        }
