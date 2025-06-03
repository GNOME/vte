/*
 * Copyright © 2017, 2018, 2025 Christian Persch
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

#include <cstdint>
#include <algorithm>
#include <string>
#include <optional>

#include "parser.hh"

#include "debug.hh"

#include <fast_float/fast_float.h>

#include <fmt/format.h>

namespace vte {

namespace parser {

/* Helper classes to unify UTF-32 and UTF-8 versions of SequenceBuilder.
 * ::put will only be called with C1 controls, so it's ok to simplify
 * the UTF-8 version to simply prepend 0xc2.
 */
template<typename C>
class DirectEncoder {
public:
        using string_type = std::basic_string<C>;
        inline void put(string_type& s, C const c) const noexcept
        {
                s.push_back(c);
        }
}; // class DirectEncoder

class UTF8Encoder {
public:
        using string_type = std::basic_string<char>;
        inline void put(string_type& s, unsigned char const c) const noexcept
        {
                s.push_back(0xc2);
                s.push_back(c);
        }
}; // class UTF8Encoder

template<class S, class E = DirectEncoder<typename S::value_type>>
class SequenceBuilder {
public:
        using string_type = S;
        using encoder_type = E;

private:
        vte_seq_t m_seq;
        string_type m_arg_str;
        unsigned char m_intermediates[4];
        unsigned char m_n_intermediates{0};
        unsigned char m_param_intro{0};
        encoder_type m_encoder;

public:
        constexpr SequenceBuilder()
        {
                memset(&m_seq, 0, sizeof(m_seq));
        }

        constexpr SequenceBuilder(unsigned type,
                                  uint32_t f,
                                  unsigned char intermediate,
                                  unsigned char pintro)
                : SequenceBuilder{}
        {
                set_type(type);
                set_final(f);
                append_intermediate(intermediate);
                set_param_intro(pintro);
        }

        SequenceBuilder(SequenceBuilder const&) = delete;
        SequenceBuilder(SequenceBuilder&&) = delete;
        ~SequenceBuilder() = default;

        SequenceBuilder& operator= (SequenceBuilder const&) = delete;
        SequenceBuilder& operator= (SequenceBuilder&&) = delete;

        constexpr unsigned int type() const noexcept { return m_seq.type; }

        constexpr auto& set_type(unsigned int type) noexcept
        {
                m_seq.type = type;
                return *this;
        }

        constexpr auto& set_final(uint32_t t) noexcept
        {
                m_seq.terminator = t;
                return *this;
        }

        constexpr auto& append_intermediate(unsigned char i) noexcept
        {
                if (i == VTE_SEQ_INTERMEDIATE_CHAR_NONE) [[unlikely]]
                        return *this;

                assert(unsigned(m_n_intermediates + 1) <= (sizeof(m_intermediates)/sizeof(m_intermediates[0])));

                m_intermediates[m_n_intermediates++] = i;
                return *this;
        }

        constexpr auto& append_intermediates(std::initializer_list<unsigned char> l) noexcept
        {
                assert(m_n_intermediates + l.size() <= (sizeof(m_intermediates)/sizeof(m_intermediates[0])));

                for (uint32_t i : l) {
                        m_intermediates[m_n_intermediates++] = i;
                }

                return *this;
        }

        constexpr auto& set_param_intro(unsigned char p) noexcept
        {
                m_param_intro = p;
                return *this;
        }

        constexpr auto& append_param(int p) noexcept
        {
                assert(m_seq.n_args + 1 <= (sizeof(m_seq.args) / sizeof(m_seq.args[0])));
                m_seq.args[m_seq.n_args++] = vte_seq_arg_init(std::min(p, 0xffff));
                return *this;
        }

        /*
         * append_params:
         * @params:
         *
         * Appends the parameters from @params to @this. Parameter values must be
         * in the range -1..MAXUSHORT; use -2 to skip a parameter
         *
         */
        constexpr auto& append_params(std::initializer_list<int> params) noexcept
        {
                assert(m_seq.n_args + params.size() <= (sizeof(m_seq.args) / sizeof(m_seq.args[0])));
                for (auto p : params) {
                        if (p == -2)
                                continue;

                        m_seq.args[m_seq.n_args++] = vte_seq_arg_init(std::min(p, 0xffff));
                }

                return *this;
        }

        /*
         * append_subparms:
         * @subparams:
         *
         * Appends the subparameters from @params to @this. Subparameter values must be
         * in the range -1..MAXUSHORT; use -2 to skip a subparameter
         *
         */
        constexpr auto& append_subparams(std::initializer_list<int> subparams) noexcept
        {
                assert(m_seq.n_args + subparams.size() <= (sizeof(m_seq.args) / sizeof(m_seq.args[0])));
                for (auto p : subparams) {
                        if (p == -2)
                                continue;

                        int* arg = &m_seq.args[m_seq.n_args++];
                        *arg = vte_seq_arg_init(std::min(p, 0xffff));
                        vte_seq_arg_finish(arg, true);
                }
                vte_seq_arg_refinish(&m_seq.args[m_seq.n_args - 1], false);

                return *this;
        }

        constexpr auto& set_string(string_type const& str) noexcept
        {
                m_arg_str = str;
                return *this;
        }

        constexpr auto& set_string(string_type&& str) noexcept
        {
                m_arg_str = std::move(str);
                return *this;
        }

        constexpr auto&
        vformat(fmt::string_view fmt,
                fmt::format_args args)
        {
                m_arg_str = fmt::vformat(fmt, args);
                return *this;
        }

        template<typename... T>
        inline constexpr auto&
        format(fmt::format_string<T...> fmt,
               T&&... args)
        {
                return vformat(fmt, fmt::make_format_args(args...));
        }

        enum class Introducer {
                NONE,
                DEFAULT,
                C0,
                C1
        };

        enum class ST {
                NONE,
                DEFAULT,
                C0,
                C1,
                BEL
        };

        constexpr auto&
        set_builder(SequenceBuilder const& builder)
        {
                m_arg_str.clear();
                builder.to_string(m_arg_str, false, -1, Introducer::NONE, ST::NONE);
                return *this;
        }

private:
        void append_introducer_(string_type& s,
                                bool c1 = true) const noexcept
        {
                /* Introducer */
                if (c1) {
                        switch (m_seq.type) {
                        case VTE_SEQ_ESCAPE: m_encoder.put(s, 0x1b); break; // ESC
                        case VTE_SEQ_CSI:    m_encoder.put(s, 0x9b); break; // CSI
                        case VTE_SEQ_DCS:    m_encoder.put(s, 0x90); break; // DCS
                        case VTE_SEQ_OSC:    m_encoder.put(s, 0x9d); break; // OSC
                        case VTE_SEQ_APC:    m_encoder.put(s, 0x9f); break; // APC
                        case VTE_SEQ_PM:     m_encoder.put(s, 0x9e); break; // PM
                        case VTE_SEQ_SOS:    m_encoder.put(s, 0x98); break; // SOS
                        case VTE_SEQ_SCI:    m_encoder.put(s, 0x9a); break; // SCI
                        default: return;
                        }
                } else {
                        s.push_back(0x1B); // ESC
                        switch (m_seq.type) {
                        case VTE_SEQ_ESCAPE:                    break; // nothing more
                        case VTE_SEQ_CSI:    s.push_back(0x5b); break; // [
                        case VTE_SEQ_DCS:    s.push_back(0x50); break; // P
                        case VTE_SEQ_OSC:    s.push_back(0x5d); break; // ]
                        case VTE_SEQ_APC:    s.push_back(0x5f); break; // _
                        case VTE_SEQ_PM:     s.push_back(0x5e); break; // ^
                        case VTE_SEQ_SOS:    s.push_back(0x58); break; // X
                        case VTE_SEQ_SCI:    s.push_back(0x5a); break; // Z
                        default: return;
                        }
                }
        }

        void append_introducer(string_type& s,
                               bool c1 = true,
                               Introducer introducer = Introducer::DEFAULT) const noexcept
        {
                switch (introducer) {
                case Introducer::NONE:
                        break;
                case Introducer::DEFAULT:
                        append_introducer_(s, c1);
                        break;
                case Introducer::C0:
                        append_introducer_(s, false);
                        break;
                case Introducer::C1:
                        append_introducer_(s, true);
                }
        }

        void append_params(string_type& s) const noexcept
        {
                /* Parameters */
                switch (m_seq.type) {
                case VTE_SEQ_CSI:
                case VTE_SEQ_DCS: {

                        if (m_param_intro != 0)
                                s.push_back(m_param_intro);
                        auto n_args = m_seq.n_args;
                        auto it = std::back_inserter(s);
                        for (auto n = 0u; n < n_args; n++) {
                                auto arg = vte_seq_arg_value(m_seq.args[n]);
                                if (arg != -1) {
                                        fmt::format_to(it, "{}", arg);
                                }
                                if (n + 1 < n_args) {
                                        *it = vte_seq_arg_nonfinal(m_seq.args[n]) ? ':' : ';';
                                }
                        }
                        break;
                }
                default:
                        break;
                }
        }

        void append_intermediates_and_final(string_type& s) const noexcept
        {
                /* Intermediates and Final */
                switch (m_seq.type) {
                case VTE_SEQ_ESCAPE:
                case VTE_SEQ_CSI:
                case VTE_SEQ_DCS:
                        for (unsigned char n = 0; n < m_n_intermediates; n++)
                                s.push_back(m_intermediates[n]);
                        [[fallthrough]];
                case VTE_SEQ_SCI:
                        s.push_back(m_seq.terminator);
                        break;
                default:
                        break;
                }
        }

        void append_arg_string(string_type& s,
                               bool c1 = false,
                               ssize_t max_arg_str_len = -1,
                               ST st = ST::DEFAULT) const noexcept
        {
                /* String and ST */
                switch (m_seq.type) {
                case VTE_SEQ_DCS:
                case VTE_SEQ_OSC:
                case VTE_SEQ_APC:
                case VTE_SEQ_PM:
                case VTE_SEQ_SOS:

                        if (max_arg_str_len < 0)
                                s.append(m_arg_str, 0, max_arg_str_len);
                        else
                                s.append(m_arg_str);

                        switch (st) {
                        case ST::NONE:
                                // omit ST
                                break;
                        case ST::DEFAULT:
                                if (c1) {
                                        m_encoder.put(s, 0x9c); // ST
                                } else {
                                        s.push_back(0x1b); // ESC
                                        s.push_back(0x5c); // BACKSLASH
                                }
                                break;
                        case ST::C0:
                                s.push_back(0x1b); // ESC
                                s.push_back(0x5c); // BACKSLASH
                                break;
                        case ST::C1:
                                m_encoder.put(s, 0x9c); // ST
                                break;
                        case ST::BEL:
                                s.push_back(0x7); // BEL
                                break;
                        default:
                                break;
                        }
                }
        }

public:
        void to_string(string_type& s,
                       bool c1 = false,
                       ssize_t max_arg_str_len = -1z,
                       Introducer introducer = Introducer::DEFAULT,
                       ST st = ST::DEFAULT) const
        {
                append_introducer(s, c1, introducer);
                append_params(s);
                append_intermediates_and_final(s);
                append_arg_string(s, c1, max_arg_str_len, st);
        }

        /* The following are only used in the test suite */
        void reset_params() noexcept
        {
                m_seq.n_args = 0;
        }

        void assert_equal(Sequence const& seq) const noexcept
        {
                vte_assert_cmpuint(seq.type(), ==, m_seq.type);
                vte_assert_cmphex(seq.terminator(), ==, m_seq.terminator);
        }

        void assert_equal_full(Sequence const& seq) const noexcept
        {
                assert_equal(seq);

                auto type = seq.type();
                if (type == VTE_SEQ_CSI ||
                    type == VTE_SEQ_DCS) {
                        /* We may get one arg less back, if it's at default */
                        if (m_seq.n_args != seq.size()) {
                                vte_assert_cmpuint(m_seq.n_args, ==, seq.size() + 1);
                                vte_assert_true(vte_seq_arg_default(m_seq.args[m_seq.n_args - 1]));
                        }
                        for (unsigned int n = 0; n < seq.size(); n++)
                                vte_assert_cmpint(vte_seq_arg_value(m_seq.args[n]), ==, seq.param(n));
                }
        }
}; // class SequenceBuilder

using u8SequenceBuilder = SequenceBuilder<std::string, UTF8Encoder>;
using u32SequenceBuilder = SequenceBuilder<std::u32string>;

namespace reply {

#define _VTE_REPLY(cmd,type,final_char,pintro,intermediate,code) \
inline constexpr auto cmd() noexcept {     \
        return u8SequenceBuilder{VTE_SEQ_##type, \
                                 final_char, \
                                 VTE_SEQ_INTERMEDIATE_CHAR_##intermediate, \
                                 VTE_SEQ_PARAMETER_CHAR_##pintro}; \
}

#include "parser-reply.hh"

#undef _VTE_REPLY

} // namespace reply

template<typename CharT>
class StringTokeniserBase {
public:
        using string_type = std::basic_string<CharT>;
        using string_view_type = std::basic_string_view<CharT>;
        using char_type = CharT;

        // Can be string_type or string_view_type
        using tokenstr_type = string_type;

private:
        tokenstr_type const& m_string;
        char_type m_separator{';'};

public:
        StringTokeniserBase(tokenstr_type const& s,
                            char_type separator = ';')
                : m_string{s},
                  m_separator{separator}
        {
        }

        StringTokeniserBase(tokenstr_type&& s,
                            char_type separator = ';')
                : m_string{std::move(s)},
                  m_separator{separator}
        {
        }

        StringTokeniserBase(StringTokeniserBase const&) = delete;
        StringTokeniserBase(StringTokeniserBase&&) = delete;
        ~StringTokeniserBase() = default;

        StringTokeniserBase& operator=(StringTokeniserBase const&) = delete;
        StringTokeniserBase& operator=(StringTokeniserBase&&) = delete;

        /*
         * const_iterator:
         *
         * InputIterator for string tokens.
         */
        class const_iterator {
        public:
                using difference_type = ptrdiff_t;
                using value_type = tokenstr_type;
                using pointer = tokenstr_type;
                using reference = tokenstr_type;
                using iterator_category = std::input_iterator_tag;
                using size_type = string_view_type::size_type;

        private:
                tokenstr_type const* m_string;
                char_type m_separator{';'};
                string_view_type::size_type m_position;
                string_view_type::size_type m_next_separator;

        public:
                const_iterator(tokenstr_type const* str,
                               char_type separator,
                               size_type position)
                        : m_string{str},
                          m_separator{separator},
                          m_position{position},
                          m_next_separator{m_string->find(m_separator, m_position)}
                {
                }

                const_iterator(tokenstr_type const* str,
                               char_type separator)
                        : m_string{str},
                          m_separator{separator},
                          m_position{string_type::npos},
                          m_next_separator{string_type::npos}
                {
                }

                const_iterator(const_iterator const&) = default;
                const_iterator(const_iterator&& o)
                        : m_string{o.m_string},
                          m_separator{o.m_separator},
                          m_position{o.m_position},
                          m_next_separator{o.m_next_separator}
                {
                }

                ~const_iterator() = default;

                const_iterator& operator=(const_iterator const& o)
                {
                        m_string = o.m_string;
                        m_separator = o.m_separator;
                        m_position = o.m_position;
                        m_next_separator = o.m_next_separator;
                        return *this;
                }

                const_iterator& operator=(const_iterator&& o)
                {
                        m_string = o.m_string;
                        m_separator = o.m_separator;
                        m_position = o.m_position;
                        m_next_separator = o.m_next_separator;
                        return *this;
                }

                inline bool operator==(const_iterator const& o) const noexcept
                {
                        return m_position == o.m_position;
                }

                inline bool operator!=(const_iterator const& o) const noexcept
                {
                        return m_position != o.m_position;
                }

                inline const_iterator& operator++() noexcept
                {
                        if (m_next_separator != string_view_type::npos) {
                                m_position = ++m_next_separator;
                                m_next_separator = m_string->find(m_separator, m_position);
                        } else
                                m_position = string_type::npos;

                        return *this;
                }

                /*
                 * number:
                 *
                 * Returns true and stores the value of the iterator as a number
                 *   (-1 for a default param), or false if the string could not
                 *   be parsed as a number, or the parsed values exceeds the
                 *   uint16_t range.
                 *
                 * Returns: true if a number was parsed
                 */
                bool number(int& v) const noexcept
                {
                        auto const s = size();
                        if (s == 0) {
                                v = -1;
                                return true;
                        }

                        auto const str = string_view();
                        auto value = uint16_t{0};
                        if (auto [ptr, err] = fast_float::from_chars(std::begin(str),
                                                                     std::end(str),
                                                                     value);
                            err == std::errc() && ptr == std::end(str)) {
                                v = int(value);
                                return true;
                        }

                        return false;
                }

                /*
                 * number:
                 *
                 * Returns the value of the iterator as an optional containing
                 *   the number (or -1 for a default param), or nullopt
                 *   if the string could not be parsed as a number, or
                 *   the parsed values exceeds the uint16_t range.
                 *
                 * Returns: a valued optional if a number was parsed, or nullopt
                 */
                auto number() const noexcept -> std::optional<int> {
                        auto v = 0;
                        if (number(v)) [[likely]]
                                return std::make_optional(v);
                        return std::nullopt;
                }

                inline size_type size() const noexcept
                {
                        if (m_next_separator != string_view_type::npos)
                                return m_next_separator - m_position;
                        else
                                return m_string->size() - m_position;
                }

                inline size_type size_remaining() const noexcept
                {
                        return m_string->size() - m_position;
                }

                inline string_type string() const
                {
                        return m_string->substr(m_position, size());
                }

                inline string_view_type string_view() const noexcept
                {
                        return string_view_type{*m_string}.substr(m_position, size());
                }

                inline string_type operator*() const
                {
                        return string();
                }

                /*
                 * string_remaining:
                 *
                 * Returns the whole string left, including possibly more separators.
                 */
                inline string_type string_remaining() const noexcept
                {
                        return m_string->substr(m_position);
                }

                inline string_view_type string_view_remaining() const noexcept
                {
                        return string_view_type{*m_string}.substr(m_position);
                }

                inline void append(string_type& str) const noexcept
                {
                        str.append(m_string->substr(m_position, size()));
                }

                inline void append_remaining(string_type& str) const noexcept
                {
                        str.append(m_string->substr(m_position));
                }

        }; // class const_iterator

        inline const_iterator cbegin(char_type c = ';') const noexcept
        {
                return const_iterator(&m_string, m_separator, 0);
        }

        inline const_iterator cend() const noexcept
        {
                return const_iterator(&m_string, m_separator);
        }

        inline const_iterator begin(char_type c = ';') const noexcept
        {
                return cbegin();
        }

        inline const_iterator end() const noexcept
        {
                return cend();
        }

}; // class StringTokeniserBase

using StringTokeniser = StringTokeniserBase<char>;

} // namespace parser

} // namespace vte
