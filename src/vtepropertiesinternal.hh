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

#include "properties.hh"

#include "cxx-utils.hh"

#include "vtemacros.h"
#include "vteenums.h"
#include "vteuuid.h"

#include <cairo.h>
#include <gtk/gtk.h>

typedef struct _VtePropertiesRegistry VtePropertiesRegistry;

#define VTE_TYPE_PROPERTIES_REGISTRY (vte_properties_registry_get_type())

GType vte_properties_registry_get_type(void);

char const** vte_properties_registry_get_properties(VtePropertiesRegistry const* registry,
                                                    size_t* length) noexcept;

gboolean vte_properties_registry_query(VtePropertiesRegistry const* registry,
                                       char const* name,
                                       char const** resolved_name,
                                       int* prop,
                                       VtePropertyType* type,
                                       VtePropertyFlags* flags) noexcept;

gboolean vte_properties_registry_query_by_id(VtePropertiesRegistry const* registry,
                                             int prop,
                                             char const** name,
                                             VtePropertyType* type,
                                             VtePropertyFlags* flags) noexcept;

VtePropertiesRegistry const* vte_get_termprops_registry(void) noexcept;

#define VTE_PROPERTY_FLAGS_ALL VtePropertyFlags(VTE_PROPERTY_FLAG_EPHEMERAL)

VTE_CXX_DEFINE_FACADE_PR(VtePropertiesRegistry, vte::property::Registry);
VTE_CXX_DEFINE_FACADE_PP(VtePropertiesRegistry, vte::property::Registry);

typedef struct _VteProperties VteProperties;

#define VTE_TYPE_PROPERTIES (vte_properties_get_type())

GType vte_properties_get_type(void);

VtePropertiesRegistry const* vte_properties_get_registry(VteProperties const* properties) noexcept;

gboolean vte_properties_get_property_bool(VteProperties const* properties,
                                          char const* prop,
                                          gboolean* valuep) noexcept;

gboolean vte_properties_get_property_bool_by_id(VteProperties const* properties,
                                                int prop,
                                                gboolean* valuep) noexcept;

gboolean vte_properties_get_property_int(VteProperties const* properties,
                                         char const* prop,
                                         int64_t* valuep) noexcept;

gboolean vte_properties_get_property_int_by_id(VteProperties const* properties,
                                               int prop,
                                               int64_t* valuep) noexcept;

gboolean vte_properties_get_property_uint(VteProperties const* properties,
                                          char const* prop,
                                          uint64_t* valuep) noexcept;

gboolean vte_properties_get_property_uint_by_id(VteProperties const* properties,
                                                int prop,
                                                uint64_t* valuep) noexcept;

gboolean vte_properties_get_property_double(VteProperties const* properties,
                                            char const* prop,
                                            double* valuep) noexcept;

gboolean vte_properties_get_property_double_by_id(VteProperties const* properties,
                                                  int prop,
                                                  double* valuep) noexcept;

gboolean vte_properties_get_property_rgba(VteProperties const* properties,
                                          char const* prop,
                                          GdkRGBA* color) noexcept;

gboolean vte_properties_get_property_rgba_by_id(VteProperties const* properties,
                                                int prop,
                                                GdkRGBA* color) noexcept;

char const* vte_properties_get_property_string(VteProperties const* properties,
                                               char const* prop,
                                               size_t* size) noexcept;

char const* vte_properties_get_property_string_by_id(VteProperties const* properties,
                                                     int prop,
                                                     size_t* size) noexcept;

char* vte_properties_dup_property_string(VteProperties const* properties,
                                         char const* prop,
                                         size_t* size) noexcept;

char* vte_properties_dup_property_string_by_id(VteProperties const* properties,
                                               int prop,
                                               size_t* size) noexcept;

uint8_t const* vte_properties_get_property_data(VteProperties const* properties,
                                                char const* prop,
                                                size_t* size) noexcept;

uint8_t const* vte_properties_get_property_data_by_id(VteProperties const* properties,
                                                      int prop,
                                                      size_t* size) noexcept;

GBytes* vte_properties_ref_property_data_bytes(VteProperties const* properties,
                                               char const* prop) noexcept;

GBytes* vte_properties_ref_property_data_bytes_by_id(VteProperties const* properties,
                                                     int prop) noexcept;

VteUuid* vte_properties_dup_property_uuid(VteProperties const* properties,
                                          char const* prop) noexcept;

VteUuid* vte_properties_dup_property_uuid_by_id(VteProperties const* properties,
                                                int prop) noexcept;

GUri* vte_properties_ref_property_uri(VteProperties const* properties,
                                      char const* prop) noexcept;

GUri* vte_properties_ref_property_uri_by_id(VteProperties const* properties,
                                            int prop) noexcept;

cairo_surface_t* vte_properties_ref_property_image_surface(VteProperties const* properties,
                                                           char const* prop) noexcept;

cairo_surface_t* vte_properties_ref_property_image_surface_by_id(VteProperties const* properties,
                                                                 int prop) noexcept;

#if _VTE_GTK == 3

GdkPixbuf* vte_properties_ref_property_image_pixbuf(VteProperties const* properties,
                                                    char const* prop) noexcept;

GdkPixbuf* vte_properties_ref_property_image_pixbuf_by_id(VteProperties const* properties,
                                                          int prop) noexcept;
#elif _VTE_GTK == 4

GdkTexture* vte_properties_ref_property_image_texture(VteProperties const* properties,
                                                      char const* prop) noexcept;

GdkTexture* vte_properties_ref_property_image_texture_by_id(VteProperties const* properties,
                                                            int prop) noexcept;
#endif /* _VTE_GTK */

gboolean vte_properties_get_property_value(VteProperties const* properties,
                                           char const* prop,
                                           GValue* gvalue) noexcept;

gboolean vte_properties_get_property_value_by_id(VteProperties const* properties,
                                                 int prop,
                                                 GValue* gvalue) noexcept;

GVariant* vte_properties_ref_property_variant(VteProperties const* properties,
                                              char const* prop) noexcept;

GVariant* vte_properties_ref_property_variant_by_id(VteProperties const* properties,
                                                    int prop) noexcept;

gboolean vte_properties_get_property_enum(VteProperties const* properties,
                                          char const* prop,
                                          GType gtype,
                                          int64_t* valuep) noexcept;

gboolean vte_properties_get_property_enum_by_id(VteProperties const* properties,
                                                int prop,
                                                GType gtype,
                                                int64_t* valuep) noexcept;

gboolean vte_properties_get_property_flags(VteProperties const* properties,
                                           char const* prop,
                                           GType gtype,
                                           gboolean ignore_unknown_flags,
                                           uint64_t* valuep) noexcept;

gboolean vte_properties_get_property_flags_by_id(VteProperties const* properties,
                                                 int prop,
                                                 GType gtype,
                                                 gboolean ignore_unknown_flags,
                                                 uint64_t* valuep) noexcept;

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
