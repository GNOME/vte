/*
 * Copyright © 2017 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
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

#include "parser.hh"

namespace vte {

namespace parser {

class Sequence {
public:

        typedef int number;

        char* ucs4_to_utf8(gunichar const* str) const;

        void print() const;

        inline unsigned int type() const { return m_seq->type; }

        inline unsigned int command() const { return m_seq->command; }

        inline uint32_t terminator() const { return m_seq->terminator; }

        inline unsigned int size() const
        {
                g_assert_nonnull(m_seq);
                return m_seq->n_args;
        }

        inline int operator[](int position) const
        {
                return G_LIKELY(position < (int)size()) ? m_seq->args[position] : -1;
        }

        inline bool has_number_at_unchecked(unsigned int position) const
        {
                return true;
        }

        inline bool number_at_unchecked(unsigned int position, number& v) const
        {
                v = m_seq->args[position];
                return true;
        }

        inline bool number_at(unsigned int position, number& v) const
        {
                if (G_UNLIKELY(position >= size()))
                        return false;

                return number_at_unchecked(position, v);
        }

        inline number number_or_default_at_unchecked(unsigned int position, number default_v = 0) const
        {
                number v;
                if (G_UNLIKELY(!number_at_unchecked(position, v)))
                        v = default_v;
                return v;
        }


        inline number number_or_default_at(unsigned int position, number default_v = 0) const
        {
                number v;
                if (G_UNLIKELY(!number_at(position, v)))
                        v = default_v;
                return v;
        }

        inline bool string_at_unchecked(unsigned int position, char*& str) const
        {
#if 0
                auto value = value_at_unchecked(position);
                if (G_LIKELY(G_VALUE_HOLDS_POINTER(value))) {
                        str = ucs4_to_utf8((gunichar const*)g_value_get_pointer (value));
                        return str != nullptr;
                }
                if (G_VALUE_HOLDS_STRING(value)) {
                        /* Copy the string into the buffer. */
                        str = g_value_dup_string(value);
                        return str != nullptr;
                }
                if (G_VALUE_HOLDS_LONG(value)) {
                        /* Convert the long to a string. */
                        str = g_strdup_printf("%ld", g_value_get_long(value));
                        return true;
                }
#endif
                str = nullptr;
                return false;
        }

        inline bool string_at(unsigned int position, char*& str) const
        {
#if 0
                if (G_UNLIKELY(position >= size()))
                        return false;

                return string_at_unchecked(position, str);
#endif
                str = nullptr;
                return false;
        }

        inline bool has_subparams_at_unchecked(unsigned int position) const
        {
                return false;
        }

        inline Sequence subparams_at_unchecked(unsigned int position) const
        {
                return Sequence{};
        }

        struct vte_seq** seq_ptr() { return &m_seq; }

        inline explicit operator bool() const { return m_seq != nullptr; }

private:
        struct vte_seq *m_seq{nullptr};

        char const* type_string() const;
        char const* command_string() const;
};

typedef Sequence Params;

} // namespace parser

} // namespace vte
