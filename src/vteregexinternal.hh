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

gboolean _vte_regex_get_jited(VteRegex *regex);

#ifdef WITH_PCRE2
const pcre2_code_8 *_vte_regex_get_pcre (VteRegex *regex);
#endif

typedef enum {
        /* Negative values are PCRE2 errors */

        /* VTE specific values */
        VTE_REGEX_ERROR_INCOMPATIBLE  = G_MAXINT-1,
        VTE_REGEX_ERROR_NOT_SUPPORTED = G_MAXINT
} VteRegexError;
