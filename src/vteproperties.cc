// Copyright © 2023, 2024, 2025 Christian Persch
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

/**
 * SECTION: vte-properties
 * @short_description: A property registry and property bag
 *
 * Since: 0.84
 */

#include "config.h"

#include <cstring>
#include <exception>

#include <cairo.h>
#include <cairo-gobject.h>

#include "vtemacros.h"
#include "vteenums.h"
#include "vteproperties.h"

#include "properties.hh"
#include "vtepropertiesinternal.hh"
#include "vteuuidinternal.hh"

#include "cairo-glue.hh"
#include "glib-glue.hh"
#include "gobject-glue.hh"
#include "vte-glue.hh"
#include "refptr.hh"
#include "debug.hh"

// VtePropertiesRegistry

G_DEFINE_POINTER_TYPE(VtePropertiesRegistry, vte_properties_registry);

template<typename T>
constexpr bool check_enum_value(T value) noexcept;

template<>
constexpr bool check_enum_value<VtePropertyType>(VtePropertyType value) noexcept
{
        switch (value) {
        case VTE_PROPERTY_VALUELESS:
        case VTE_PROPERTY_BOOL:
        case VTE_PROPERTY_INT:
        case VTE_PROPERTY_UINT:
        case VTE_PROPERTY_DOUBLE:
        case VTE_PROPERTY_RGB:
        case VTE_PROPERTY_RGBA:
        case VTE_PROPERTY_STRING:
        case VTE_PROPERTY_DATA:
        case VTE_PROPERTY_UUID:
                return true;

                // These are not installable via the public API
        case VTE_PROPERTY_URI:
        case VTE_PROPERTY_IMAGE:
                return false;

        default:
                return false;
        }
}

/**
 * VtePropertiesRegistry:
 *
 * A property registry.
 *
 * Since: 0.84
 */

#define VTE_IS_PROPERTIES_REGISTRY(r) (r != nullptr)

int
_vte_properties_registry_install(VtePropertiesRegistry* registry,
                                 char const* name,
                                 VtePropertyType type,
                                 VtePropertyFlags flags) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES_REGISTRY(registry), -1);
        g_return_val_if_fail(name, -1);
        g_return_val_if_fail(check_enum_value(type), -1);
        g_return_val_if_fail(flags == VTE_PROPERTY_FLAG_NONE ||
                             flags == VTE_PROPERTY_FLAG_EPHEMERAL, -1);

        return _vte_facade_unwrap_pp(registry)->install(name,
                                         vte::property::Type(type),
                                         vte::property::Flags(flags));
}
catch (...)
{
        vte::log_exception();
        return -1;
}

int
_vte_properties_registry_install_alias(VtePropertiesRegistry* registry,
                                       char const* name,
                                       char const* target_name) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES_REGISTRY(registry), -1);
        g_return_val_if_fail(name, -1);
        g_return_val_if_fail(target_name, -1);

        return _vte_facade_unwrap_pp(registry)->install_alias(name, target_name);
}
catch (...)
{
        vte::log_exception();
        return -1;
}

/**
 * vte_properties_registry_get_properties:
 * @registry: a #VtePropertiesRegistry
 * @length: (out) (optional): a location to store the length of the returned array
 *
 * Gets the names of the installed properties in an unspecified order.
 *
 * Returns: (transfer container) (array length=length) (nullable): the names of the
 *   installed properties, or %NULL if there are no properties
 *
 * Since: 0.84
 */
char const**
vte_properties_registry_get_properties(VtePropertiesRegistry const* registry,
                                       size_t* length) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES_REGISTRY(registry), nullptr);

        auto const impl = _vte_facade_unwrap_pp(registry);
        auto const n_properties = impl->size();
        auto strv = vte::glib::take_free_ptr(g_try_new0(char*, n_properties + 1));
        if (!strv || !n_properties) {
                if (length)
                        *length = 0;
                return nullptr;
        }

        auto i = 0;
        for (auto const& info : impl->get_all()) {
                strv.get()[i++] = const_cast<char*>(g_quark_to_string(info.quark()));
        }
        strv.get()[i] = nullptr;
        vte_assert_cmpint(i, ==, int(n_properties));

        if (length)
                *length = i;

        return const_cast<char const**>(strv.release());
}
catch (...)
{
        vte::log_exception();
        if (length)
                *length = 0;
        return nullptr;
}

/**
 * vte_properties_registry_query:
 * @registry: a #VtePropertiesRegistry
 * @name: a property name
 * @resolved_name: (out) (optional) (transfer none): a location to store the property's name
 * @prop: (out) (optional): a location to store the property's ID
 * @type: (out) (optional): a location to store the property's type as a #VtePropertyType
 * @flags: (out) (optional): a location to store the property's flags as a #VtePropertyFlags
 *
 * Gets the property type of the property. For properties installed by
 * vte_install_property(), the name starts with "vte.ext.".
 *
 * For an alias property (see vte_properties_registry_install_alias()),
 * @resolved_name will be name of the alias' target property; otherwise
 * it will be @name.
 *
 * Returns: %TRUE iff the property exists, and then @prop, @type and
 *   @flags will be filled in
 *
 * Since: 0.84
 */
gboolean
vte_properties_registry_query(VtePropertiesRegistry const* registry,
                              char const* name,
                              char const** resolved_name,
                              int* prop,
                              VtePropertyType* type,
                              VtePropertyFlags* flags) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES_REGISTRY(registry), false);

        auto const impl = _vte_facade_unwrap_pp(registry);
        if (auto const info = impl->lookup(name)) {
                if (resolved_name)
                        *resolved_name = g_quark_to_string(info->quark());
                if (prop)
                        *prop = info->id();
                if (type)
                        *type = VtePropertyType(info->type());
                if (flags)
                        *flags = VtePropertyFlags(info->flags());
                return true;
        }

        return false;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_properties_registry_query_by_id:
 * @registry: a #VtePropertiesRegistry
 * @prop: a property ID for the registry
 * @name: (out) (optional) (transfer none): a location to store the property's name
 * @type: (out) (optional): a location to store the property's type as a #VtePropertyType
 * @flags: (out) (optional): a location to store the property's flags as a #VtePropertyFlags
 *
 * Like vte_properties_registry_query_by_name() except that it takes the property by ID.
 * See that function for more information.
 *
 * For an alias property (see vte_properties_registry_install_alias()),
 * @resolved_name will be name of the alias' target property; otherwise
 * it will be @name.
 *
 * Returns: %TRUE iff the property exists, and then @name, @type and
 *   @flags will be filled in
 *
 * Since: 0.84
 */
gboolean
vte_properties_registry_query_by_id(VtePropertiesRegistry const* registry,
                                    int prop,
                                    char const** name,
                                    VtePropertyType* type,
                                    VtePropertyFlags* flags) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES_REGISTRY(registry), false);
        g_return_val_if_fail(prop >= 0, false);

        auto const impl = _vte_facade_unwrap_pp(registry);

        if (auto const info = impl->lookup(prop)) {
                if (name)
                        *name = g_quark_to_string(info->quark());
                if (type)
                        *type = VtePropertyType(info->type());
                if (flags)
                        *flags = VtePropertyFlags(info->flags());
                return true;
        }

        return false;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/*
 * _vte_properties_registry_get_quark_by_id:
 * @registry: a #VtePropertiesRegistry
 * @prop: a property ID for the registry
 *
 * Returns the #GQuark of the name of the property @prop, or 0
 * if @prop is not installed.
 *
 * Returns: the quark for @prop
 */
GQuark
_vte_properties_registry_get_quark_by_id(VtePropertiesRegistry const* registry,
                                         int prop) noexcept
try
{
        auto const impl = _vte_facade_unwrap_pp(registry);
        if (auto const info = impl->lookup(prop))
                return info->quark();

        return 0;
}
catch (...)
{
        vte::log_exception();
        return 0;
}

// VteProperties

G_DEFINE_POINTER_TYPE(VtePropertiues, vte_properties);

#define VTE_IS_PROPERTIES(p) (p != nullptr)

/**
 * VteProperties:
 *
 * A property bag.
 *
 * Since: 0.84
 */

static int
_vte_properties_get_property_id(VteProperties const* properties,
                                char const* prop) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), -1);

        if (auto const info = _vte_facade_unwrap_pp(properties)->registry().lookup(prop))
                return info->id();

        return -1;
}
catch (...)
{
        vte::log_exception();
        return -1;
}


/**
 * vte_properties_get_registry:
 * @properties: a #VteProperties
 *
 * Returns: (transfer none): the #VtePropertiesRegistry associated
 *   with @properties
 *
 * Since: 0.84
 */
VtePropertiesRegistry const*
vte_properties_get_registry(VteProperties const* properties) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);

        return _vte_facade_wrap_pr(_vte_facade_unwrap_pp(properties)->registry());
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_properties_get_property_bool_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * Like vte_properties_get_property_bool() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: %TRUE iff the property is set
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_bool_by_id(VteProperties const* properties,
                                       int prop,
                                       gboolean *valuep) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), false);
        g_return_val_if_fail(prop >= 0, false);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info) {
                if (valuep) [[likely]]
                        *valuep = false;
                return false;
        }

        g_return_val_if_fail(info->type() == vte::property::Type::BOOL, false);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<bool>(*value)) {
                if (valuep) [[likely]]
                        *valuep = std::get<bool>(*value);
                return true;
        }

        return false;
}
catch (...)
{
        vte::log_exception();

        if (valuep) [[likely]]
                *valuep = false;
        return false;
}

/**
 * vte_properties_get_property_bool:
 * @properties: a #VteProperties
 * @prop: a property name
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * For a %VTE_PROPERTY_BOOL property, sets @value to @prop's value,
 *   or to %FALSE if @prop is unset, or @prop is not a registered property.
 *
 * Returns: %TRUE iff the property is set
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_bool(VteProperties const* properties,
                                 char const* prop,
                                 gboolean *valuep) noexcept
{
        g_return_val_if_fail(prop != nullptr, false);

        return vte_properties_get_property_bool_by_id(properties,
                                                      _vte_properties_get_property_id(properties, prop),
                                                      valuep);
}

/**
 * vte_properties_get_property_int_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * Like vte_properties_get_property_int() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: %TRUE iff the property is set
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_int_by_id(VteProperties const* properties,
                                      int prop,
                                      int64_t *valuep) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), false);
        g_return_val_if_fail(prop >= 0, false);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info) {
                if (valuep) [[likely]]
                        *valuep = 0;
                return false;
        }

        g_return_val_if_fail(info->type() == vte::property::Type::INT, false);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<int64_t>(*value)) {
                if (valuep) [[likely]]
                        *valuep = std::get<int64_t>(*value);
                return true;
        }

        return false;
}
catch (...)
{
        vte::log_exception();

        if (valuep) [[likely]]
                *valuep = 0;
        return false;
}

/**
 * vte_properties_get_property_int:
 * @properties: a #VteProperties
 * @prop: a property name
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * For a %VTE_PROPERTY_INT property, sets @value to @prop's value,
 * or to 0 if @prop is unset, or if @prop is not a registered property.
 *
 * If only a subset or range of values are acceptable for the given property,
 * the caller must validate the returned value and treat any out-of-bounds
 * value as if the property had no value; in particular it *must not* clamp
 * the values to the expected range.
 *
 * Returns: %TRUE iff the property is set
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_int(VteProperties const* properties,
                                char const* prop,
                                int64_t *valuep) noexcept
{
        g_return_val_if_fail(prop != nullptr, false);

        return vte_properties_get_property_int_by_id(properties,
                                                     _vte_properties_get_property_id(properties, prop),
                                                     valuep);
}

/**
 * vte_properties_get_property_uint_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * Like vte_properties_get_property_uint() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: %TRUE iff the property is set
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_uint_by_id(VteProperties const* properties,
                                       int prop,
                                       uint64_t *valuep) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), false);
        g_return_val_if_fail(prop >= 0, false);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info) {
                if (valuep) [[likely]]
                        *valuep = 0;
                return false;
        }

        g_return_val_if_fail(info->type() == vte::property::Type::UINT, false);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<uint64_t>(*value)) {
                if (valuep) [[likely]]
                        *valuep = std::get<uint64_t>(*value);
                return true;
        }

        return false;
}
catch (...)
{
        vte::log_exception();

        if (valuep) [[likely]]
                *valuep = 0;
        return false;
}

/**
 * vte_properties_get_property_uint:
 * @properties: a #VteProperties
 * @prop: a property name
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * For a %VTE_PROPERTY_UINT property, sets @value to @prop's value,
 * or to 0 if @prop is unset, or @prop is not a registered property.
 *
 * If only a subset or range of values are acceptable for the given property,
 * the caller must validate the returned value and treat any out-of-bounds
 * value as if the property had no value; in particular it *must not* clamp
 * the values to the expected range.
 *
 * Returns: %TRUE iff the property is set
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_uint(VteProperties const* properties,
                                 char const* prop,
                                 uint64_t *valuep) noexcept
{
        g_return_val_if_fail(prop != nullptr, false);

        return vte_properties_get_property_uint_by_id(properties,
                                                      _vte_properties_get_property_id(properties, prop),
                                                      valuep);
}

/**
 * vte_properties_get_property_double_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * Like vte_properties_get_property_double() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: %TRUE iff the property is set
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_double_by_id(VteProperties const* properties,
                                         int prop,
                                         double* valuep) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), false);
        g_return_val_if_fail(prop >= 0, false);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info) {
                if (valuep) [[likely]]
                        *valuep = 0.0;
                return false;
        }

        g_return_val_if_fail(info->type() == vte::property::Type::DOUBLE, false);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<double>(*value)) {
                if (valuep) [[likely]]
                        *valuep = std::get<double>(*value);
                return true;
        }

        return false;
}
catch (...)
{
        vte::log_exception();

        if (valuep) [[likely]]
                *valuep = 0.0;
        return false;
}

/**
 * vte_properties_get_property_double:
 * @properties: a #VteProperties
 * @prop: a property name
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * For a %VTE_PROPERTY_DOUBLE property, sets @value to @prop's value,
 *   which is finite; or to 0.0 if @prop is unset, or @prop is not a
 *   registered property.
 *
 * Returns: %TRUE iff the property is set
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_double(VteProperties const* properties,
                                   char const* prop,
                                   double* valuep) noexcept
{
        g_return_val_if_fail(prop != nullptr, false);

        return vte_properties_get_property_double_by_id(properties,
                                                        _vte_properties_get_property_id(properties, prop),
                                                        valuep);
}

/**
 * vte_properties_get_property_rgba_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 * @color: (out) (optional): a #GdkRGBA to fill in
 *
 * Like vte_properties_get_property_rgba() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: %TRUE iff the property is set
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_rgba_by_id(VteProperties const* properties,
                                       int prop,
                                       GdkRGBA* color) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), false);
        g_return_val_if_fail(prop >= 0, false);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return false;

        g_return_val_if_fail(info->type() == vte::property::Type::RGB ||
                             info->type() == vte::property::Type::RGBA, false);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<vte::property::property_rgba>(*value)) {
                if (color) [[likely]] {
                        auto const& c = std::get<vte::property::property_rgba>(*value);
                        *color = GdkRGBA{c.red(), c.green(), c.blue(), c.alpha()};
                }
                return true;
        }

        if (color) [[likely]]
                *color = GdkRGBA{0., 0., 0., 1.};
        return false;
}
catch (...)
{
        vte::log_exception();
        if (color) [[likely]]
                *color = GdkRGBA{0., 0., 0., 1.};
        return false;
}

/**
 * vte_properties_get_property_rgba:
 * @properties: a #VteProperties
 * @prop: a property name
 * @color: (out) (optional): a #GdkRGBA to fill in
 *
 * Stores the value of a %VTE_PROPERTY_RGB or %VTE_PROPERTY_RGBA property in @color and
 * returns %TRUE if the property is set, or stores rgb(0,0,0) or rgba(0,0,0,1) in @color
 * and returns %FALSE if the property is unset.
 *
 * Returns: %TRUE iff the property is set
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_rgba(VteProperties const* properties,
                                 char const* prop,
                                 GdkRGBA* color) noexcept
{
        g_return_val_if_fail(prop != nullptr, false);

        return vte_properties_get_property_rgba_by_id(properties,
                                                      _vte_properties_get_property_id(properties, prop),
                                                      color);
}

/**
 * vte_properties_get_property_string_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 * @size: (out) (optional): a location to store the string length, or %NULL
 *
 * Like vte_properties_get_property_string() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: (transfer none) (nullable): the property's value, or %NULL
 *
 * Since: 0.84
 */
char const*
vte_properties_get_property_string_by_id(VteProperties const* properties,
                                         int prop,
                                         size_t* size) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);

        if (size)
                *size = 0;

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return nullptr;

        g_return_val_if_fail(info->type() == vte::property::Type::STRING, nullptr);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<std::string>(*value)) {
                auto const& str = std::get<std::string>(*value);
                if (size)
                        *size = str.size();
                return str.c_str();
        }

        return nullptr;
}
catch (...)
{
        vte::log_exception();
        if (size)
               *size = 0;
        return nullptr;
}

/**
 * vte_properties_get_property_string:
 * @properties: a #VteProperties
 * @prop: a property name
 * @size: (out) (optional): a location to store the string length, or %NULL
 *
 * Returns the value of a %VTE_PROPERTY_STRING property, or %NULL if
 *   @prop is unset, or @prop is not a registered property.
 *
 * Returns: (transfer none) (nullable): the property's value, or %NULL
 *
 * Since: 0.84
 */
char const*
vte_properties_get_property_string(VteProperties const* properties,
                                   char const* prop,
                                   size_t* size) noexcept
{
        g_return_val_if_fail(prop != nullptr, nullptr);

        return vte_properties_get_property_string_by_id(properties,
                                                        _vte_properties_get_property_id(properties, prop),
                                                        size);
}

/**
 * vte_properties_dup_property_string_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 * @size: (out) (optional): a location to store the string length, or %NULL
 *
 * Like vte_properties_dup_property_string() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: (transfer full) (nullable): the property's value, or %NULL
 *
 * Since: 0.84
 */
char*
vte_properties_dup_property_string_by_id(VteProperties const* properties,
                                         int prop,
                                         size_t* size) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return nullptr;

        g_return_val_if_fail(info->type() == vte::property::Type::STRING, nullptr);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<std::string>(*value)) {
                auto const& str = std::get<std::string>(*value);
                if (size)
                        *size = str.size();
                return g_strndup(str.c_str(), str.size());
        }

        return nullptr;
}
catch (...)
{
        vte::log_exception();
        if (size)
               *size = 0;
        return nullptr;
}

/**
 * vte_properties_dup_property_string:
 * @properties: a #VteProperties
 * @prop: a property name
 * @size: (out) (optional): a location to store the string length, or %NULL
 *
 * Returns the value of a %VTE_PROPERTY_STRING property, or %NULL if
 *   @prop is unset, or @prop is not a registered property.
 *
 * Returns: (transfer full) (nullable): the property's value, or %NULL
 *
 * Since: 0.84
 */
char*
vte_properties_dup_property_string(VteProperties const* properties,
                                   char const* prop,
                                   size_t* size) noexcept
{
        g_return_val_if_fail(prop != nullptr, nullptr);

        return vte_properties_dup_property_string_by_id(properties,
                                                        _vte_properties_get_property_id(properties, prop),
                                                        size);
}

/**
 * vte_properties_get_property_data_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 * @size: (out): a location to store the size of the data
 *
 * Like vte_properties_get_property_data() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: (transfer none) (element-type guint8) (array length=size) (nullable): the
 *   property's value, or %NULL
 *
 * Since: 0.84
 */
uint8_t const*
vte_properties_get_property_data_by_id(VteProperties const* properties,
                                       int prop,
                                       size_t* size) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);
        g_return_val_if_fail(size != nullptr, nullptr);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return nullptr;

        g_return_val_if_fail(info->type() == vte::property::Type::DATA, nullptr);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<std::string>(*value)) {
                auto const& str = std::get<std::string>(*value);
                *size = str.size();
                return reinterpret_cast<uint8_t const*>(str.data());
        }

        *size = 0;
        return nullptr;

}
catch (...)
{
        vte::log_exception();
        *size = 0;
        return nullptr;
}

/**
 * vte_properties_get_property_data:
 * @properties: a #VteProperties
 * @prop: a property name
 * @size: (out): a location to store the size of the data
 *
 * Returns the value of a %VTE_PROPERTY_DATA property, or %NULL if
 *   @prop is unset, or @prop is not a registered property.
 *
 * Returns: (transfer none) (element-type guint8) (array length=size) (nullable): the
 *   property's value, or %NULL
 *
 * Since: 0.84
 */
uint8_t const*
vte_properties_get_property_data(VteProperties const* properties,
                                 char const* prop,
                                 size_t* size) noexcept
{
        g_return_val_if_fail(prop != nullptr, nullptr);

        return vte_properties_get_property_data_by_id(properties,
                                                      _vte_properties_get_property_id(properties, prop),
                                                      size);
}

/**
 * vte_properties_ref_property_data_bytes_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 *
 * Like vte_properties_ref_property_data_bytes() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: (transfer full) (nullable): the property's value as a #GBytes, or %NULL
 *
 * Since: 0.84
 */
GBytes*
vte_properties_ref_property_data_bytes_by_id(VteProperties const* properties,
                                             int prop) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return nullptr;

        g_return_val_if_fail(info->type() == vte::property::Type::DATA, nullptr);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<std::string>(*value)) {
                auto const& str = std::get<std::string>(*value);
                return g_bytes_new(str.data(), str.size());
        }

        return nullptr;
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_properties_ref_property_data_bytes:
 * @properties: a #VteProperties
 * @prop: a property name
 *
 * Returns the value of a %VTE_PROPERTY_DATA property as a #GBytes, or %NULL if
 *   @prop is unset, or @prop is not a registered property.
 *
 * Returns: (transfer full) (nullable): the property's value as a #GBytes, or %NULL
 *
 * Since: 0.84
 */
GBytes*
vte_properties_ref_property_data_bytes(VteProperties const* properties,
                                       char const* prop) noexcept
{
        g_return_val_if_fail(prop != nullptr, nullptr);

        return vte_properties_ref_property_data_bytes_by_id(properties,
                                                            _vte_properties_get_property_id(properties, prop));
}

/**
 * vte_properties_dup_property_uuid_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 *
 * Like vte_properties_dup_property_uuid() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: (transfer full) (nullable): the property's value as a #VteUuid, or %NULL
 *
 * Since: 0.84
 */
VteUuid*
vte_properties_dup_property_uuid_by_id(VteProperties const* properties,
                                       int prop) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return nullptr;

        g_return_val_if_fail(info->type() == vte::property::Type::DATA, nullptr);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<vte::uuid>(*value)) {
                return _vte_uuid_new_from_uuid(std::get<vte::uuid>(*value));
        }

        return nullptr;
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_properties_dup_property_uuid:
 * @properties: a #VteProperties
 * @prop: a property name
 *
 * Returns the value of a %VTE_PROPERTY_UUID property as a #VteUuid, or %NULL if
 *   @prop is unset, or @prop is not a registered property.
 *
 * Returns: (transfer full) (nullable): the property's value as a #VteUuid, or %NULL
 *
 * Since: 0.84
 */
VteUuid*
vte_properties_dup_property_uuid(VteProperties const* properties,
                                 char const* prop) noexcept
{
        g_return_val_if_fail(prop != nullptr, nullptr);

        return vte_properties_dup_property_uuid_by_id(properties,
                                                      _vte_properties_get_property_id(properties, prop));
}

/**
 * vte_properties_ref_property_uri_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 *
 * Like vte_properties_ref_property_uri() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: (transfer full) (nullable): the property's value as a #GUri, or %NULL
 *
 * Since: 0.84
 */
GUri*
vte_properties_ref_property_uri_by_id(VteProperties const* properties,
                                      int prop) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return nullptr;

        g_return_val_if_fail(info->type() == vte::property::Type::URI, nullptr);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<vte::property::URIValue>(*value)) {
                return g_uri_ref(std::get<vte::property::URIValue>(*value).first.get());
        }

        return nullptr;
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_properties_ref_property_uri:
 * @properties: a #VteProperties
 * @prop: a property name
 *
 * Returns the value of a %VTE_PROPERTY_URI property as a #GUri, or %NULL if
 *   @prop is unset, or @prop is not a registered property.
 *
 * Returns: (transfer full) (nullable): the property's value as a #GUri, or %NULL
 *
 * Since: 0.84
 */
GUri*
vte_properties_ref_property_uri(VteProperties const* properties,
                                char const* prop) noexcept
{
        g_return_val_if_fail(prop != nullptr, nullptr);

        return vte_properties_ref_property_uri_by_id(properties,
                                                     _vte_properties_get_property_id(properties, prop));
}

/**
 * vte_properties_get_property_uri_string_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 *
 * Like vte_properties_ref_property_uri_by_id() except that it returns
 * the URI as a const string.
 *
 * Returns: (transfer none) (nullable): the property's value as a string, or %NULL
 */
char const*
_vte_properties_get_property_uri_string_by_id(VteProperties const* properties,
                                              int prop) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return nullptr;

        g_return_val_if_fail(info->type() == vte::property::Type::URI, nullptr);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<vte::property::URIValue>(*value)) {
                return std::get<vte::property::URIValue>(*value).second.c_str();
        }

        return nullptr;
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_properties_ref_property_image_surface_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 *
 * Like vte_properties_ref_property_image_surface() except that it takes the
 * property by ID. See that function for more information.
 *
 * Returns: (transfer full) (nullable): the property's value as a #cairo_surface_t, or %NULL
 *
 * Since: 0.84
 */
cairo_surface_t*
vte_properties_ref_property_image_surface_by_id(VteProperties const* properties,
                                                int prop) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return nullptr;

        g_return_val_if_fail(info->type() == vte::property::Type::IMAGE, nullptr);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<vte::Freeable<cairo_surface_t>>(*value)) {
                return cairo_surface_reference(std::get<vte::Freeable<cairo_surface_t>>(*value).get());
        }

        return nullptr;
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_properties_ref_property_image_surface:
 * @properties: a #VteProperties
 * @prop: a property name
 *
 * Returns the value of a %VTE_PROPERTY_IMAGE property as a #cairo_surface_t,
 *   or %NULL if @prop is unset, or @prop is not a registered property.
 *
 * The surface will be a %CAIRO_SURFACE_TYPE_IMAGE with format
 * %CAIRO_FORMAT_ARGB32 or %CAIRO_FORMAT_RGB24.
 *
 * Note that the returned surface is owned by @properties and its contents
 * must not be modified.
 *
 * Returns: (transfer full) (nullable): the property's value as
 *   a #cairo_surface_t, or %NULL
 *
 * Since: 0.84
 */
cairo_surface_t*
vte_properties_ref_property_image_surface(VteProperties const* properties,
                                          char const* prop) noexcept
{
        g_return_val_if_fail(prop != nullptr, nullptr);

        return vte_properties_ref_property_image_surface_by_id(properties,
                                                               _vte_properties_get_property_id(properties, prop));
}

#if VTE_GTK == 3

/**
 * vte_properties_ref_property_image_pixbuf_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 *
 * Like vte_properties_ref_property_image_pixbuf() except that it takes the
 * property by ID. See that function for more information.
 *
 * Returns: (transfer full) (nullable): the property's value as a #GdkPixbuf,
 *   or %NULL
 *
 * Since: 0.84
 */
GdkPixbuf*
vte_properties_ref_property_image_pixbuf_by_id(VteProperties const* properties,
                                               int prop) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return nullptr;

        g_return_val_if_fail(info->type() == vte::property::Type::IMAGE, nullptr);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<vte::Freeable<cairo_surface_t>>(*value)) {
                auto const surface = std::get<vte::Freeable<cairo_surface_t>>(*value).get();
                if (cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE) {
                        return gdk_pixbuf_get_from_surface(surface,
                                                           0, 0,
                                                           cairo_image_surface_get_width(surface),
                                                           cairo_image_surface_get_height(surface));
                }
        }

        return nullptr;
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_properties_ref_property_image_pixbuf:
 * @properties: a #VteProperties
 * @prop: a property name
 *
 * Returns the value of a %VTE_PROPERTY_IMAGE property as a #GdkPixbuf, or %NULL if
 *   @prop is unset, or @prop is not a registered property.
 *
 * Returns: (transfer full) (nullable): the property's value as a #GdkPixbuf, or %NULL
 *
 * Since: 0.84
 */
GdkPixbuf*
vte_properties_ref_property_image_pixbuf(VteProperties const* properties,
                                         char const* prop) noexcept
{
        g_return_val_if_fail(prop != nullptr, nullptr);

        return vte_properties_ref_property_image_pixbuf_by_id(properties,
                                                              _vte_properties_get_property_id(properties, prop));
}

#elif VTE_GTK == 4

static vte::glib::RefPtr<GdkTexture>
texture_from_surface(cairo_surface_t* surface)
{
        if (cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE)
                return nullptr;

        auto const format = cairo_image_surface_get_format(surface);
        if (format != CAIRO_FORMAT_ARGB32 &&
            format != CAIRO_FORMAT_RGB24)
                return nullptr;

        auto const bytes = vte::take_freeable
                (g_bytes_new_with_free_func(cairo_image_surface_get_data(surface),
                                            size_t(cairo_image_surface_get_height(surface)) *
                                            size_t(cairo_image_surface_get_stride(surface)),
                                            GDestroyNotify(cairo_surface_destroy),
                                            cairo_surface_reference(surface)));

        auto memory_format = [](auto fmt) constexpr noexcept -> auto
        {
                if constexpr (std::endian::native == std::endian::little) {
                        return fmt == CAIRO_FORMAT_ARGB32
                                ? GDK_MEMORY_B8G8R8A8_PREMULTIPLIED
                                : GDK_MEMORY_B8G8R8;
                } else if constexpr (std::endian::native == std::endian::big) {
                        return fmt == CAIRO_FORMAT_ARGB32
                                ? GDK_MEMORY_A8R8G8B8_PREMULTIPLIED
                                : GDK_MEMORY_R8G8B8;
                } else {
                        __builtin_unreachable();
                }
        };

        return vte::glib::take_ref(gdk_memory_texture_new(cairo_image_surface_get_width(surface),
                                                          cairo_image_surface_get_height(surface),
                                                          memory_format(format),
                                                          bytes.get(),
                                                          cairo_image_surface_get_stride(surface)));
}

/**
 * vte_properties_ref_property_image_texture_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 *
 * Like vte_properties_ref_property_image_texture() except that it takes the
 * property by ID. See that function for more information.
 *
 * Returns: (transfer full) (nullable): the property's value as a #GdkTexture,
 *   or %NULL
 *
 * Since: 0.84
 */
GdkTexture*
vte_properties_ref_property_image_texture_by_id(VteProperties const* properties,
                                                int prop) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return nullptr;

        g_return_val_if_fail(info->type() == vte::property::Type::IMAGE, nullptr);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<vte::Freeable<cairo_surface_t>>(*value)) {

                return texture_from_surface(std::get<vte::Freeable<cairo_surface_t>>(*value).get()).release();
        }

        return nullptr;
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_properties_ref_property_image_texture:
 * @properties: a #VteProperties
 * @prop: a property name
 *
 * Returns the value of a %VTE_PROPERTY_IMAGE property as a #GdkTexture, or %NULL if
 *   @prop is unset, or @prop is not a registered property.
 *
 * Returns: (transfer full) (nullable): the property's value as a #GdkTexture,
 *   or %NULL
 *
 * Since: 0.84
 */
GdkTexture*
vte_properties_ref_property_image_texture(VteProperties const* properties,
                                          char const* prop) noexcept
{
        g_return_val_if_fail(prop != nullptr, nullptr);

        return vte_properties_ref_property_image_texture_by_id(properties,
                                                               _vte_properties_get_property_id(properties, prop));
}

#endif /* VTE_GTK == 4 */

/**
 * vte_properties_get_property_value_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 * @gvalue: (out) (allow-none): a #GValue to be filled in
 *
 * Like vte_properties_get_property_value() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: %TRUE iff the property has a value, with @gvalue containig
 *   the property's value.
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_value_by_id(VteProperties const* properties,
                                        int prop,
                                        GValue* gvalue) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), false);
        g_return_val_if_fail(prop >= 0, false);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info || info->type() == vte::property::Type::VALUELESS)
                return false;

        auto const value = impl->value(*info);
        if (!value)
                return false;

        auto rv = false;

        switch (info->type()) {
                using enum vte::property::Type;
        case vte::property::Type::VALUELESS:
                break;

        case vte::property::Type::BOOL:
                if (std::holds_alternative<bool>(*value)) {
                        rv = true;
                        if (gvalue) [[likely]] {
                                g_value_init(gvalue, G_TYPE_BOOLEAN);
                                g_value_set_boolean(gvalue, std::get<bool>(*value));
                        }
                }
                break;

        case vte::property::Type::INT:
                if (std::holds_alternative<int64_t>(*value)) {
                        rv = true;
                        if (gvalue) [[likely]] {
                                g_value_init(gvalue, G_TYPE_INT64);
                                g_value_set_int64(gvalue, int64_t(std::get<int64_t>(*value)));
                        }
                }
                break;

        case vte::property::Type::UINT:
                if (std::holds_alternative<uint64_t>(*value)) {
                        rv = true;
                        if (gvalue) [[likely]] {
                                g_value_init(gvalue, G_TYPE_UINT64);
                                g_value_set_uint64(gvalue, uint64_t(std::get<uint64_t>(*value)));
                        }
                }
                break;

        case vte::property::Type::DOUBLE:
                if (std::holds_alternative<double>(*value)) {
                        rv = true;
                        if (gvalue) [[likely]] {
                                g_value_init(gvalue, G_TYPE_DOUBLE);
                                g_value_set_double(gvalue, std::get<double>(*value));
                        }
                }
                break;

        case vte::property::Type::RGB:
        case vte::property::Type::RGBA:
#if VTE_GTK == 3
                if (std::holds_alternative<vte::property::property_rgba>(*value)) {
                        rv = true;
                        if (gvalue) [[likely]] {
                                auto const& c = std::get<vte::property::property_rgba>(*value);
                                auto color = GdkRGBA{c.red(), c.green(), c.blue(), c.alpha()};
                                g_value_init(gvalue, GDK_TYPE_RGBA);
                                g_value_set_boxed(gvalue, &color);
                        }
                }
#elif VTE_GTK == 4
                if (std::holds_alternative<vte::property::property_rgba>(*value) &&
                    !gvalue) {
                        rv = true;
                }
#endif // VTE_GTK
                break;

        case vte::property::Type::STRING:
                if (std::holds_alternative<std::string>(*value)) {
                        rv = true;
                        if (gvalue) [[likely]] {
                                g_value_init(gvalue, G_TYPE_STRING);
                                g_value_set_string(gvalue, std::get<std::string>(*value).c_str());
                        }
                }
                break;

        case vte::property::Type::DATA:
                if (std::holds_alternative<std::string>(*value)) {
                        rv = true;
                        if (gvalue) [[likely]] {
                                auto const& str = std::get<std::string>(*value);
                                g_value_init(gvalue, G_TYPE_BYTES);
                                g_value_take_boxed(gvalue, g_bytes_new(str.data(), str.size()));
                        }
                }
                break;

        case vte::property::Type::UUID:
                if (std::holds_alternative<vte::uuid>(*value)) {
                        rv = true;
                        if (gvalue) [[likely]] {
                                g_value_init(gvalue, VTE_TYPE_UUID);
                                g_value_take_boxed(gvalue, _vte_uuid_new_from_uuid(std::get<vte::uuid>(*value)));
                        }
                }
                break;

        case vte::property::Type::URI:
                if (std::holds_alternative<vte::property::URIValue>(*value)) {
                        rv = true;
                        if (gvalue) [[likely]] {
                                g_value_init(gvalue, G_TYPE_URI);
                                g_value_set_boxed(gvalue, std::get<vte::property::URIValue>(*value).first.get());
                        }
                }
                break;

        case vte::property::Type::IMAGE:
                if (std::holds_alternative<vte::Freeable<cairo_surface_t>>(*value)) {
                        rv = true;
                        if (gvalue) [[likely]] {
#if VTE_GTK == 3
                                g_value_init(gvalue, CAIRO_GOBJECT_TYPE_SURFACE);
                                g_value_set_boxed(gvalue, std::get<vte::Freeable<cairo_surface_t>>(*value).get());
#elif VTE_GTK == 4
                                g_value_init(gvalue, GDK_TYPE_TEXTURE);
                                g_value_take_boxed(gvalue, texture_from_surface(std::get<vte::Freeable<cairo_surface_t>>(*value).get()).release());
#endif
                        }
                }
                break;
        default:
                __builtin_unreachable(); break;
        }

        return rv;
}
catch (...)
{
        vte::log_exception();
        return false;
}

/**
 * vte_properties_get_property_value:
 * @properties: a #VteProperties
 * @prop: a property name
 * @gvalue: (out) (allow-none): a #GValue to be filled in, or %NULL
 *
 * Returns %TRUE with the value of @prop stored in @value (if not %NULL) if,
 *   the property has a value, or %FALSE if @prop is unset, or @prop is not
 *   a registered property; in that case @value will not be set.
 *
 * The value type returned depends on the property type:
 * * A %VTE_PROPERTY_VALUELESS property stores no value, and returns %FALSE
 *   from this function.
 * * A %VTE_PROPERTY_BOOL property stores a %G_TYPE_BOOLEAN value.
 * * A %VTE_PROPERTY_INT property stores a %G_TYPE_INT64 value.
 * * A %VTE_PROPERTY_UINT property stores a %G_TYPE_UINT64 value.
 * * A %VTE_PROPERTY_DOUBLE property stores a %G_TYPE_DOUBLE value.
 * * A %VTE_PROPERTY_RGB property stores a boxed #GdkRGBA value with alpha 1.0 on gtk3,
 *    and nothing on gtk4.
 * * A %VTE_PROPERTY_RGBA property stores a boxed #GdkRGBA value on gtk3,
 *    and nothing on gtk4.
 * * A %VTE_PROPERTY_STRING property stores a %G_TYPE_STRING value.
 * * A %VTE_PROPERTY_DATA property stores a boxed #GBytes value.
 * * A %VTE_PROPERTY_UUID property stores a boxed #VteUuid value.
 * * A %VTE_PROPERTY_URI property stores a boxed #GUri value.
 * * A %VTE_PROPERTY_IMAGE property stores a boxed #cairo_surface_t value on gtk3,
 *     and a boxed #GdkTexture on gtk4
 *
 * Returns: %TRUE iff the property has a value, with @gvalue containig
 *   the property's value.
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_value(VteProperties const* properties,
                                  char const* prop,
                                  GValue* gvalue) noexcept
{
        g_return_val_if_fail(prop != nullptr, false);

        return vte_properties_get_property_value_by_id(properties,
                                                       _vte_properties_get_property_id(properties, prop),
                                                       gvalue);
}

/**
 * vte_properties_ref_property_variant_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID
 *
 * Like vte_properties_ref_property_variant() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: (transfer full) (nullable): a floating #GVariant, or %NULL
 *
 * Since: 0.84
 */
GVariant*
vte_properties_ref_property_variant_by_id(VteProperties const* properties,
                                          int prop) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), nullptr);
        g_return_val_if_fail(prop >= 0, nullptr);

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info || info->type() == vte::property::Type::VALUELESS)
                return nullptr;

        auto const value = impl->value(*info);
        if (!value)
                return nullptr;

        switch (info->type()) {
                using enum vte::property::Type;
        case vte::property::Type::VALUELESS:
                return g_variant_new("()");

        case vte::property::Type::BOOL:
                if (std::holds_alternative<bool>(*value)) {
                        return g_variant_new_boolean(std::get<bool>(*value));
                }
                break;

        case vte::property::Type::INT:
                if (std::holds_alternative<int64_t>(*value)) {
                        return g_variant_new_int64(int64_t(std::get<int64_t>(*value)));
                }
                break;
        case vte::property::Type::UINT:
                if (std::holds_alternative<uint64_t>(*value)) {
                        return g_variant_new_uint64(uint64_t(std::get<uint64_t>(*value)));
                }
                break;

        case vte::property::Type::DOUBLE:
                if (std::holds_alternative<double>(*value)) {
                        return g_variant_new_double(std::get<double>(*value));
                }
                break;

        case vte::property::Type::RGB:
        case vte::property::Type::RGBA:
                if (std::holds_alternative<vte::property::property_rgba>(*value)) {
                        auto const& color = std::get<vte::property::property_rgba>(*value);
                        return g_variant_new("(ddddv)",
                                             color.red(),
                                             color.green(),
                                             color.blue(),
                                             color.alpha(),
                                             g_variant_new_boolean(false)); // placeholder
                }
                break;

        case vte::property::Type::STRING:
                if (std::holds_alternative<std::string>(*value)) {
                        return g_variant_new_string(std::get<std::string>(*value).c_str());
                }
                break;

        case vte::property::Type::DATA:
                if (std::holds_alternative<std::string>(*value)) {
                        auto const& str = std::get<std::string>(*value);
                        return g_variant_new_fixed_array(G_VARIANT_TYPE("y"),
                                                         str.data(), str.size(), 1);
                }
                break;

        case vte::property::Type::UUID:
                if (std::holds_alternative<vte::uuid>(*value)) {
                        return g_variant_new_string(std::get<vte::uuid>(*value).str().c_str());
                }
                break;

        case vte::property::Type::URI:
                if (std::holds_alternative<vte::property::URIValue>(*value)) {
                        return g_variant_new_string(std::get<vte::property::URIValue>(*value).second.c_str());
                }
                break;

        case vte::property::Type::IMAGE:
                // no variant representation
                break;

        default:
                __builtin_unreachable(); break;
        }

        return nullptr;
}
catch (...)
{
        vte::log_exception();
        return nullptr;
}

/**
 * vte_properties_ref_property_variant:
 * @properties: a #VteProperties
 * @prop: a property name
 *
 * Returns the value of @prop as a #GVariant, or %NULL if
 *   @prop unset, or @prop is not a registered property.
 *
 * The #GVariantType of the returned #GVariant depends on the property type:
 * * A %VTE_PROPERTY_VALUELESS property returns a %G_VARIANT_TYPE_UNIT variant.
 * * A %VTE_PROPERTY_BOOL property returns a %G_VARIANT_TYPE_BOOLEAN variant.
 * * A %VTE_PROPERTY_INT property returns a %G_VARIANT_TYPE_INT64 variant.
 * * A %VTE_PROPERTY_UINT property returns a %G_VARIANT_TYPE_UINT64 variant.
 * * A %VTE_PROPERTY_DOUBLE property returns a %G_VARIANT_TYPE_DOUBLE variant.
 * * A %VTE_PROPERTY_RGB or %VTE_PROPERTY_RGBA property returns a "(ddddv)"
 *   tuple containing the red, green, blue, and alpha (1.0 for %VTE_PROPERTY_RGB)
 *   components of the color and a variant of unspecified contents
 * * A %VTE_PROPERTY_STRING property returns a %G_VARIANT_TYPE_STRING variant.
 * * A %VTE_PROPERTY_DATA property returns a "ay" variant (which is *not* a bytestring!).
 * * A %VTE_PROPERTY_UUID property returns a %G_VARIANT_TYPE_STRING variant
 *   containing a string representation of the UUID in simple form.
 * * A %VTE_PROPERTY_URI property returns a %G_VARIANT_TYPE_STRING variant
 *   containing a string representation of the URI
 * * A %VTE_PROPERTY_IMAGE property returns %NULL since an image has no
 *   variant representation.
 *
 * Returns: (transfer full) (nullable): a floating #GVariant, or %NULL
 *
 * Since: 0.84
 */
GVariant*
vte_properties_ref_property_variant(VteProperties const* properties,
                                    char const* prop) noexcept
{
        g_return_val_if_fail(prop != nullptr, nullptr);

        return vte_properties_ref_property_variant_by_id(properties,
                                                         _vte_properties_get_property_id(properties, prop));
}

/**
 * vte_properties_get_property_enum_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID of a %VTE_PROPERTY_STRING property
 * @gtype: a #GType of an enum type
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * Like vte_properties_get_property_enum() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: %TRUE iff the property was set and could be parsed a
 *   a value of enumeration type @type
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_enum_by_id(VteProperties const* properties,
                                       int prop,
                                       GType gtype,
                                       int64_t* valuep) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), false);
        g_return_val_if_fail(prop >= 0, false);

        if (valuep) [[likely]]
                *valuep = 0;

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return false;

        g_return_val_if_fail(info->type() == vte::property::Type::STRING, false);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<std::string>(*value)) {
                return vte::glib::parse_enum(std::get<std::string>(*value),
                                             gtype,
                                             valuep);
        }

        return false;
}
catch (...)
{
        vte::log_exception();
        if (valuep)
               *valuep = 0;
        return false;
}

/**
 * vte_properties_get_property_enum:
 * @properties: a #VteProperties
 * @prop: a property name of a %VTE_PROPERTY_STRING property
 * @gtype: a #GType of an enum type
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * For a %VTE_PROPERTY_STRING property, sets @value to @prop's value,
 * parsed as a value of the enumeration type @gtype, or to 0 if
 * @prop is unset, or if its value could not be parsed as a value of
 * the enumeration type, or if @prop is not a registered property.
 *
 * Returns: %TRUE iff the property was set and could be parsed a
 *   a value of the enumeration type
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_enum(VteProperties const* properties,
                                 char const* prop,
                                 GType gtype,
                                 int64_t* valuep) noexcept
{
        g_return_val_if_fail(prop != nullptr, false);

        return vte_properties_get_property_enum_by_id(properties,
                                                      _vte_properties_get_property_id(properties, prop),
                                                      gtype,
                                                      valuep);
}


/**
 * vte_properties_get_property_flags_by_id:
 * @properties: a #VteProperties
 * @prop: a property ID of a %VTE_PROPERTY_STRING property
 * @gtype: a #GType of a flags type
 * @ignore_unknown_flags: whether to ignore unknown flags
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * Like vte_properties_get_property_flags() except that it takes the property
 * by ID. See that function for more information.
 *
 * Returns: %TRUE iff the property was set and could be parsed a
 *   a value of flags type @type
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_flags_by_id(VteProperties const* properties,
                                        int prop,
                                        GType gtype,
                                        gboolean ignore_unknown_flags,
                                        uint64_t* valuep) noexcept
try
{
        g_return_val_if_fail(VTE_IS_PROPERTIES(properties), false);
        g_return_val_if_fail(prop >= 0, false);

        if (valuep) [[likely]]
                *valuep = 0;

        auto const impl = _vte_facade_unwrap_pp(properties);
        auto const info = impl->lookup_checked(prop);
        if (!info)
                return false;

        g_return_val_if_fail(info->type() == vte::property::Type::STRING, false);

        auto const value = impl->value(*info);
        if (value &&
            std::holds_alternative<std::string>(*value)) {
                return vte::glib::parse_flags(std::get<std::string>(*value),
                                              gtype,
                                              ignore_unknown_flags,
                                              valuep);
        }

        return false;
}
catch (...)
{
        vte::log_exception();
        if (valuep)
               *valuep = 0;
        return false;
}


/**
 * vte_properties_get_property_flags:
 * @prop: a property name of a %VTE_PROPERTY_STRING property
 * @gtype: a #GType of a flags type
 * @ignore_unknown_flags: whether to ignore unknown flags
 * @valuep: (out) (optional): a location to store the value, or %NULL
 *
 * For a %VTE_PROPERTY_STRING property, sets @value to @prop's value,
 * parsed as a '|'-separated list of values of the flags type @gtype,
 * or to 0 if @prop is unset, or if its value could not be parsed as
 * a list of values of the flags type, if @prop is not a registered
 * property.
 * If @ignore_unknown_flags is %TRUE, flags that are unknown are
 * ignored instead of causing this function to return %FALSE.
 *
 * Returns: %TRUE iff the property was set and could be parsed a
 *   a value of the flags type
 *
 * Since: 0.84
 */
gboolean
vte_properties_get_property_flags(VteProperties const* properties,
                                  char const* prop,
                                  GType gtype,
                                  gboolean ignore_unknown_flags,
                                  uint64_t* valuep) noexcept
{
        g_return_val_if_fail(prop != nullptr, false);

        return vte_properties_get_property_flags_by_id(properties,
                                                       _vte_properties_get_property_id(properties, prop),
                                                       gtype,
                                                       ignore_unknown_flags,
                                                       valuep);
}
