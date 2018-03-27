/*
 * Copyright © 2017, 2018 Christian Persch
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

#include <cstdint>
#include <algorithm>

#include "parser.hh"

namespace vte {

namespace parser {

class Sequence {
public:

        typedef int number;

        char* ucs4_to_utf8(gunichar const* str) const noexcept;

        void print() const noexcept;

        /* type:
         *
         *
         * Returns: the type of the sequence, a value from the VTE_SEQ_* enum
         */
        inline constexpr unsigned int type() const noexcept
        {
                return m_seq->type;
        }

        /* command:
         *
         * Returns: the command the sequence codes for, a value
         *   from the VTE_CMD_* enum, or %VTE_CMD_NONE if the command is
         *   unknown
         */
        inline constexpr unsigned int command() const noexcept
        {
                return m_seq->command;
        }

        /* charset:
         *
         * This is the charset to use in a %VTE_CMD_GnDm, %VTE_CMD_GnDMm,
         * %VTE_CMD_CnD or %VTE_CMD_DOCS command.
         *
         * Returns: the charset, a value from the VTE_CHARSET_* enum.
         */
        inline constexpr unsigned int charset() const noexcept
        {
                return m_seq->charset;
        }

        /* intermediates:
         *
         * The intermediate bytes of the ESCAPE, CSI or DCS sequence.
         *
         * Returns: the immediates as flag values from the VTE_SEQ_FLAG_* enum
         */
        inline constexpr unsigned int intermediates() const noexcept
        {
                return m_seq->intermediates;
        }

        /* terminator:
         *
         * This is the character terminating the sequence, or, for a
         * %VTE_SEQ_GRAPHIC sequence, the graphic character.
         *
         * Returns: the terminating character
         */
        inline constexpr uint32_t terminator() const noexcept
        {
                return m_seq->terminator;
        }

        /* size:
         *
         * Returns: the number of parameters
         */
        inline constexpr unsigned int size() const noexcept
        {
                return m_seq->n_args;
        }


        /* size:
         *
         * Returns: the number of parameter blocks, counting runs of subparameters
         *   as only one parameter
         */
        inline constexpr unsigned int size_final() const noexcept
        {
                return m_seq->n_final_args;
        }

        /* capacity:
         *
         * Returns: the number of parameter blocks, counting runs of subparameters
         *   as only one parameter
         */
        inline constexpr unsigned int capacity() const noexcept
        {
                return G_N_ELEMENTS(m_seq->args);
        }

        /* param:
         * @idx:
         * @default_v: the value to use for default parameters
         *
         * Returns: the value of the parameter at index @idx, or @default_v if
         *   the parameter at this index has default value, or the index
         *   is out of bounds
         */
        inline constexpr int param(unsigned int idx,
                                   int default_v = -1) const noexcept
        {
                return __builtin_expect(idx < size(), 1) ? vte_seq_arg_value(m_seq->args[idx], default_v) : default_v;
        }

        /* param:
         * @idx:
         * @default_v: the value to use for default parameters
         * @min_v: the minimum value
         * @max_v: the maximum value
         *
         * Returns: the value of the parameter at index @idx, or @default_v if
         *   the parameter at this index has default value, or the index
         *   is out of bounds. The returned value is clamped to the
         *   range @min_v..@max_v.
         */
        inline constexpr int param(unsigned int idx,
                                   int default_v,
                                   int min_v,
                                   int max_v) const noexcept
        {
                auto v = param(idx, default_v);
                return std::min(std::max(v, min_v), max_v);
        }

        /* param_nonfinal:
         * @idx:
         *
         * Returns: whether the parameter at @idx is nonfinal, i.e.
         * there are more subparameters after it.
         */
        inline constexpr bool param_nonfinal(unsigned int idx) const noexcept
        {
                return __builtin_expect(idx < size(), 1) ? vte_seq_arg_nonfinal(m_seq->args[idx]) : false;
        }

        /* param_default:
         * @idx:
         *
         * Returns: whether the parameter at @idx has default value
         */
        inline constexpr bool param_default(unsigned int idx) const noexcept
        {
                return __builtin_expect(idx < size(), 1) ? vte_seq_arg_default(m_seq->args[idx]) : true;
        }

        /* next:
         * @idx:
         *
         * Returns: the index of the next parameter block
         */
        inline constexpr unsigned int next(unsigned int idx) const noexcept
        {
                /* Find the final parameter */
                while (param_nonfinal(idx))
                        ++idx;
                /* And return the index after that one */
                return ++idx;
        }

        inline constexpr unsigned int cbegin() const noexcept
        {
                return 0;
        }

        inline constexpr unsigned int cend() const noexcept
        {
                return size();
        }

        /* collect:
         *
         * Collects some final parameters.
         *
         * Returns: %true if the sequence parameter list begins with
         *  a run of final parameters that were collected.
         */
        inline constexpr bool collect(unsigned int start_idx,
                                      std::initializer_list<int*> params,
                                      int default_v = -1) const noexcept
        {
                unsigned int idx = start_idx;
                for (auto i : params) {
                        *i = param(idx, default_v);
                        idx = next(idx);
                }

                return (idx - start_idx) == params.size();
        }

        /* collect1:
         * @idx:
         * @default_v:
         *
         * Collects one final parameter.
         *
         * Returns: the parameter value, or @default_v if the parameter has
         *   default value or is not a final parameter
         */
        inline constexpr int collect1(unsigned int idx,
                                      int default_v = -1) const noexcept
        {
                return __builtin_expect(idx < size(), 1) ? vte_seq_arg_value_final(m_seq->args[idx], default_v) : default_v;
        }

        /* collect1:
         * @idx:
         * @default_v:
         * @min_v:
         * @max_v
         *
         * Collects one final parameter.
         *
         * Returns: the parameter value clamped to the @min_v .. @max_v range,
         *   or @default_v if the parameter has default value or is not a final parameter
         */
        inline constexpr int collect1(unsigned int idx,
                                      int default_v,
                                      int min_v,
                                      int max_v) const noexcept
        {
                int v = __builtin_expect(idx < size(), 1) ? vte_seq_arg_value_final(m_seq->args[idx], default_v) : default_v;
                return std::min(std::max(v, min_v), max_v);
        }

        /* collect_subparams:
         *
         * Collects some subparameters.
         *
         * Returns: %true if the sequence parameter list contains enough
         *   subparams at @start_idx
         */
        inline constexpr bool collect_subparams(unsigned int start_idx,
                                                std::initializer_list<int*> params,
                                                int default_v = -1) const noexcept
        {
                unsigned int idx = start_idx;
                for (auto i : params) {
                        *i = param(idx++, default_v);
                }

                return idx <= next(start_idx);
        }

        //FIMXE remove this one
        inline constexpr int operator[](int position) const
        {
                return __builtin_expect(position < (int)size(), 1) ? vte_seq_arg_value(m_seq->args[position]) : -1;
        }

        inline bool has_number_at_unchecked(unsigned int position) const
        {
                return true;
        }

        inline bool number_at_unchecked(unsigned int position, number& v) const
        {
                v = vte_seq_arg_value(m_seq->args[position]);
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
