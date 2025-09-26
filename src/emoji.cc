/*
 * Copyright © 2025 Egmont Koblinger
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

#include <stdio.h>  // just for myself
#include <stdint.h>

#include "emoji.hh"

namespace vte {

#include "emoji-table-incl.hh"

/* Whether string s + char c would be an emoji prefix */
bool is_emoji_prefix(vteunistr s, gunichar c)
{
        int len, i;
        gunichar chars[VTE_UNISTR_MAX_LENGTH + 1];
        void *(*node) (gunichar) = emoji_lookup_node_0;

        /* Extract the characters of s, append c */
        len = _vte_unistr_dump(s, chars);
        g_assert(len <= VTE_UNISTR_MAX_LENGTH);
        chars[len++] = c;

        /* Jump through the nodes */
        for (i = 0; i < len; i++) {
                node = (void *(*) (gunichar)) (*node)(chars[i]);
                if (node == NULL) {
                        return false;
                }
        }
        return true;
}

} // namespace vte
