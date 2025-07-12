/*
 * Copyright © 2023, 2024, 2025 Christian Persch
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

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#include <glib.h>

#include "vtemacros.h"
#include "vteenums.h"
#include "vteuuid.h"

#include <cairo.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _VtePropertiesRegistry VtePropertiesRegistry;

#define VTE_TYPE_PROPERTIES_REGISTRY (vte_properties_registry_get_type())

_VTE_PUBLIC
GType vte_properties_registry_get_type(void);

_VTE_PUBLIC
char const** vte_properties_registry_get_properties(VtePropertiesRegistry const* registry,
                                                    size_t* length) _VTE_CXX_NOEXCEPT G_GNUC_MALLOC _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_properties_registry_query(VtePropertiesRegistry const* registry,
                                       char const* name,
                                       char const** resolved_name,
                                       int* prop,
                                       VtePropertyType* type,
                                       VtePropertyFlags* flags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
gboolean vte_properties_registry_query_by_id(VtePropertiesRegistry const* registry,
                                             int prop,
                                             char const** name,
                                             VtePropertyType* type,
                                             VtePropertyFlags* flags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
VtePropertiesRegistry const* vte_get_termprops_registry(void) _VTE_CXX_NOEXCEPT;

typedef struct _VteProperties VteProperties;

#define VTE_TYPE_PROPERTIES (vte_properties_get_type())

_VTE_PUBLIC
GType vte_properties_get_type(void);

_VTE_PUBLIC
VtePropertiesRegistry const* vte_properties_get_registry(VteProperties const* properties) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_properties_get_property_bool(VteProperties const* properties,
                                          char const* prop,
                                          gboolean* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
gboolean vte_properties_get_property_bool_by_id(VteProperties const* properties,
                                                int prop,
                                                gboolean* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_properties_get_property_int(VteProperties const* properties,
                                         char const* prop,
                                         int64_t* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
gboolean vte_properties_get_property_int_by_id(VteProperties const* properties,
                                               int prop,
                                               int64_t* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_properties_get_property_uint(VteProperties const* properties,
                                          char const* prop,
                                          uint64_t* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
gboolean vte_properties_get_property_uint_by_id(VteProperties const* properties,
                                                int prop,
                                                uint64_t* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_properties_get_property_double(VteProperties const* properties,
                                            char const* prop,
                                            double* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
gboolean vte_properties_get_property_double_by_id(VteProperties const* properties,
                                                  int prop,
                                                  double* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_properties_get_property_rgba(VteProperties const* properties,
                                          char const* prop,
                                          GdkRGBA* color) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
gboolean vte_properties_get_property_rgba_by_id(VteProperties const* properties,
                                                int prop,
                                                GdkRGBA* color) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
char const* vte_properties_get_property_string(VteProperties const* properties,
                                               char const* prop,
                                               size_t* size) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
char const* vte_properties_get_property_string_by_id(VteProperties const* properties,
                                                     int prop,
                                                     size_t* size) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
char* vte_properties_dup_property_string(VteProperties const* properties,
                                         char const* prop,
                                         size_t* size) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
char* vte_properties_dup_property_string_by_id(VteProperties const* properties,
                                               int prop,
                                               size_t* size) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
uint8_t const* vte_properties_get_property_data(VteProperties const* properties,
                                                char const* prop,
                                                size_t* size) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2, 3);

_VTE_PUBLIC
uint8_t const* vte_properties_get_property_data_by_id(VteProperties const* properties,
                                                      int prop,
                                                      size_t* size) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 3);

_VTE_PUBLIC
GBytes* vte_properties_ref_property_data_bytes(VteProperties const* properties,
                                               char const* prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
GBytes* vte_properties_ref_property_data_bytes_by_id(VteProperties const* properties,
                                                     int prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
VteUuid* vte_properties_dup_property_uuid(VteProperties const* properties,
                                          char const* prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
VteUuid* vte_properties_dup_property_uuid_by_id(VteProperties const* properties,
                                                int prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
GUri* vte_properties_ref_property_uri(VteProperties const* properties,
                                      char const* prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
GUri* vte_properties_ref_property_uri_by_id(VteProperties const* properties,
                                            int prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
cairo_surface_t* vte_properties_ref_property_image_surface(VteProperties const* properties,
                                                           char const* prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
cairo_surface_t* vte_properties_ref_property_image_surface_by_id(VteProperties const* properties,
                                                                 int prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

#if _VTE_GTK == 3

_VTE_PUBLIC
GdkPixbuf* vte_properties_ref_property_image_pixbuf(VteProperties const* properties,
                                                    char const* prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
GdkPixbuf* vte_properties_ref_property_image_pixbuf_by_id(VteProperties const* properties,
                                                          int prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

#elif _VTE_GTK == 4

_VTE_PUBLIC
GdkTexture* vte_properties_ref_property_image_texture(VteProperties const* properties,
                                                      char const* prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
GdkTexture* vte_properties_ref_property_image_texture_by_id(VteProperties const* properties,
                                                            int prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

#endif /* _VTE_GTK */

_VTE_PUBLIC
gboolean vte_properties_get_property_value(VteProperties const* properties,
                                           char const* prop,
                                           GValue* gvalue) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2, 3);

_VTE_PUBLIC
gboolean vte_properties_get_property_value_by_id(VteProperties const* properties,
                                                 int prop,
                                                 GValue* gvalue) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 3);

_VTE_PUBLIC
GVariant* vte_properties_ref_property_variant(VteProperties const* properties,
                                              char const* prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
GVariant* vte_properties_ref_property_variant_by_id(VteProperties const* properties,
                                                    int prop) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_properties_get_property_enum(VteProperties const* properties,
                                          char const* prop,
                                          GType gtype,
                                          int64_t* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
gboolean vte_properties_get_property_enum_by_id(VteProperties const* properties,
                                                int prop,
                                                GType gtype,
                                                int64_t* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_properties_get_property_flags(VteProperties const* properties,
                                           char const* prop,
                                           GType gtype,
                                           gboolean ignore_unknown_flags,
                                           uint64_t* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
gboolean vte_properties_get_property_flags_by_id(VteProperties const* properties,
                                                 int prop,
                                                 GType gtype,
                                                 gboolean ignore_unknown_flags,
                                                 uint64_t* valuep) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

G_END_DECLS
