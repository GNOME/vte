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

#include <glib-object.h>

#include "std-glue.hh"
#include "glib-glue.hh"

#include <string>
#include <string_view>

namespace vte {

VTE_DECLARE_FREEABLE(GTypeClass, g_type_class_unref);

} // namespace vte

namespace vte::glib {

class FreezeObjectNotify {
public:
        explicit FreezeObjectNotify(void* object) noexcept
                : m_object{G_OBJECT(object)}
        {
                g_object_freeze_notify(m_object);
        }

        ~FreezeObjectNotify() noexcept
        {
                g_object_thaw_notify(m_object);
        }

        FreezeObjectNotify() = delete;
        FreezeObjectNotify(FreezeObjectNotify const&) = delete;
        FreezeObjectNotify(FreezeObjectNotify&&) = delete;
        FreezeObjectNotify& operator=(FreezeObjectNotify const&) = delete;
        FreezeObjectNotify& operator=(FreezeObjectNotify&&) = delete;

        auto get() const noexcept { return m_object; }

private:
        GObject* m_object;
}; // class FreezeObjectNotify

        // FIXME: reimplement the following two functions to take string_view

        inline bool parse_enum(std::string const& str,
                               GType type,
                               int64_t* valuep) noexcept
        {
                auto klass = vte::take_freeable
                        (reinterpret_cast<GTypeClass*>(g_type_class_ref(type)));
                auto enum_class = reinterpret_cast<GEnumClass*>(klass.get());
                if (auto const ev = g_enum_get_value_by_nick(enum_class, str.c_str())) {
                        if (valuep) [[likely]]
                                *valuep = int64_t(ev->value);

                        return true;
                }

                if (valuep) [[likely]]
                        *valuep = 0;
                return false;
        }

        inline bool parse_flags(std::string const& str,
                                GType type,
                                bool ignore_unknown_flags,
                                uint64_t* valuep) noexcept
        {
                auto klass = vte::take_freeable
                        (reinterpret_cast<GTypeClass*>(g_type_class_ref(type)));
                auto flags_class = reinterpret_cast<GFlagsClass*>(klass.get());

                auto flags = vte::glib::take_strv(g_strsplit(str.c_str(), "|", 0));
                auto value = uint64_t{0};
                for (auto flag = flags.get(); *flag; ++flag) {
                        if (auto const fv = g_flags_get_value_by_nick(flags_class, *flag)) {
                                value |= uint64_t(fv->value);
                        } else if (!ignore_unknown_flags) {
                                if (valuep) [[likely]]
                                        *valuep = 0;
                                return false;
                        }
                }

                if (valuep) [[likely]]
                        *valuep = value;
                return true;
        }

} // namespace vte::glib
