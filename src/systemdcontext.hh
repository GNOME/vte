
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

#include "vtemacros.h"
#include "vteenums.h"

#include "systemdpropsregistry.hh"

#include <string_view>

namespace vte::systemd {

using namespace std::literals::string_view_literals;

inline std::optional<VteSystemdContextOperation>
parse_context_operation(std::string_view str)
{
        if (str == "start"sv)
                return VTE_SYSTEMD_CONTEXT_OPERATION_START;
        if (str == "end"sv)
                return VTE_SYSTEMD_CONTEXT_OPERATION_END;
        return std::nullopt;
}

class Context {
public:
        Context(VteSystemdContextOperation op,
                vte::uuid const& id)
                : m_op(op),
                  m_id(id),
                  m_properties{properties_registry()}
        {
                auto const idinfo = m_properties.registry().lookup(VTE_SYSTEMD_PROPERTY_ID_CONTEXT_ID);
                assert(idinfo);
                *m_properties.value(*idinfo) = m_id;
        }

        constexpr auto op() const noexcept { return m_op; }
        constexpr auto const& id() const noexcept { return m_id; }
        constexpr auto const& properties() const noexcept { return m_properties; }
        constexpr auto& properties() noexcept { return m_properties; }

private:
        VteSystemdContextOperation m_op;
        vte::uuid m_id;
        vte::property::Store m_properties;
}; // class context

} // namespace vte::systemd
