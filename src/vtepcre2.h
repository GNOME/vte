/*
 * Copyright © 2015 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>
#include <cstdint>

/* Assert compatibility of PCRE2 and GLib types */
static_assert(sizeof(PCRE2_UCHAR8) == sizeof (uint8_t), "PCRE2_UCHAR2 has wrong size");
static_assert(sizeof(PCRE2_SIZE) == sizeof (size_t), "PCRE2_SIZE has wrong size");
static_assert(PCRE2_UNSET == (size_t)-1, "PCRE2_UNSET has wrong value");
static_assert(PCRE2_ZERO_TERMINATED == (size_t)-1, "PCRE2_ZERO_TERMINATED has wrong value");
