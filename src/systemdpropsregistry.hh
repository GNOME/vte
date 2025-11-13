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

#include "uuid.hh"
#include "properties.hh"

#include <string_view>

namespace vte::systemd {

using namespace std::literals::string_view_literals;

class SystemdPropertyRegistry final : public vte::property::Registry {
public:
        using base_type = vte::property::Registry;
        SystemdPropertyRegistry();
        ~SystemdPropertyRegistry() override = default;

        ParseFunc resolve_parse_func(vte::property::Type type) override
        {
                switch (type) {
                        using enum vte::property::Type;
                case STRING: return &vte::property::impl::parse_systemd_property_string;
                case UUID: return &vte::property::impl::parse_systemd_property_uuid;
                default:
                        return vte::property::Registry::resolve_parse_func(type);
                }
        }

}; // class SystemdPropertyRegistry

SystemdPropertyRegistry const& properties_registry() noexcept;

} // namespace vte::systemd
