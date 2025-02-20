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
#include "vteproperties.h"

#include "properties.hh"

#include "cxx-utils.hh"

VTE_CXX_DEFINE_FACADE_PR(VtePropertiesRegistry, vte::property::Registry);
VTE_CXX_DEFINE_FACADE_PP(VtePropertiesRegistry, vte::property::Registry);

VtePropertiesRegistry* _vte_get_termprops_registry(void) noexcept;

int _vte_properties_registry_install(VtePropertiesRegistry* registry,
                                     char const* name,
                                     VtePropertyType type,
                                     VtePropertyFlags flags) noexcept;

int _vte_properties_registry_install_alias(VtePropertiesRegistry* registry,
                                           char const* name,
                                           char const* target_name) noexcept;

GQuark _vte_properties_registry_get_quark_by_id(VtePropertiesRegistry const* registry,
                                                int prop) noexcept;

VTE_CXX_DEFINE_FACADE_PR(VteProperties, vte::property::Store);
VTE_CXX_DEFINE_FACADE_PP(VteProperties, vte::property::Store);

char const* _vte_properties_get_property_uri_string_by_id(VteProperties const* properties,
                                                          int prop) noexcept;
