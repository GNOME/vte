// Copyright © 2021, 2022, 2023, 2025 Christian Persch
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

#include "properties.hh"

namespace vte::terminal {

class TermpropsRegistry final : public vte::property::Registry
{
public:
        using base_type = vte::property::Registry;

        TermpropsRegistry();
        ~TermpropsRegistry() override = default;

        int install(char const* name,
                    vte::property::Type type,
                    vte::property::Flags flags = vte::property::Flags::NONE) override;

        int install_alias(char const* name,
                          char const* target_name) override;

private:

        bool check_termprop_wellknown(char const* name,
                                      vte::property::Type* type,
                                      vte::property::Flags* flags) noexcept;

        char const* check_termprop_wellknown_alias(char const* name) noexcept;

        bool check_termprop_blocklisted(char const* name) noexcept;

        bool check_termprop_blocklisted_alias(char const* name) noexcept;

}; // class TermpropRegistry

TermpropsRegistry& termprops_registry() noexcept;

} // namespace vte::terminal
