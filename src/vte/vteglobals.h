/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
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

#include <glib.h>

#include "vtemacros.h"
#include "vteenums.h"

G_BEGIN_DECLS

_VTE_PUBLIC
char *vte_get_user_shell(void) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
const char *vte_get_features (void) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
VteFeatureFlags vte_get_feature_flags(void) _VTE_CXX_NOEXCEPT;

#define VTE_TEST_FLAGS_NONE (G_GUINT64_CONSTANT(0))
#define VTE_TEST_FLAGS_ALL (~G_GUINT64_CONSTANT(0))

_VTE_PUBLIC
void vte_set_test_flags(guint64 flags) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
guint64 vte_get_test_flags(void) _VTE_CXX_NOEXCEPT;

/**
 * VTE_TERMPROP_NAME_PREFIX:
 *
 * The string prefix that any termprop's name must start with to be installed
 * by vte_install_termprop().
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_NAME_PREFIX "vte.ext."

_VTE_PUBLIC
int vte_install_termprop(char const* name,
                         VtePropertyType type,
                         VtePropertyFlags flags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
int vte_install_termprop_alias(char const* name,
                               char const* target_name) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
gboolean vte_query_termprop(char const* name,
                            int* prop,
                            VtePropertyType* type,
                            VtePropertyFlags* flags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_query_termprop_by_id(int prop,
                                  char const** name,
                                  VtePropertyType* type,
                                  VtePropertyFlags* flags) _VTE_CXX_NOEXCEPT;

/**
 * VTE_TERMPROP_TITLE:
 *
 * A %VTE_PROPERTY_STRING termprop that stores the window title
 * as set by OSC 0 and OSC 2.
 * Use this with vte_termprop_get_string() instead of using
 * vte_terminal_get_window_title().
 *
 * Note that this termprop is not settable via the termprop OSC.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_TITLE "vte.title"

/**
 * VTE_TERMPROP_CURRENT_DIRECTORY_URI_STRING:
 *
 * A %VTE_PROPERTY_STRING termprop that stores the current directory
 * URI as set by OSC 7.
 * Use this with vte_termprop_get_string() instead of using
 * vte_terminal_get_current_directory_uri().
 *
 * Note that this termprop is not settable via the termprop OSC.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CURRENT_DIRECTORY_URI_STRING "vte.cwd"

/**
 * VTE_TERMPROP_CURRENT_FILE_URI_STRING:
 *
 * A %VTE_PROPERTY_STRING termprop that stores the current file URI
 * as set by OSC 6.
 * Use this with vte_termprop_get_string() instead of using
 * vte_terminal_get_current_file_uri().
 *
 * Note that this termprop is not settable via the termprop OSC.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CURRENT_FILE_URI_STRING "vte.cwf"

G_END_DECLS
