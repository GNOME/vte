/*
 * Copyright © 2020 Christian Persch
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

#include "config.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <variant>
#include <vector>

#include <glib.h>

#include "sixel-parser.hh"
#include "sixel-context.hh"

using namespace std::literals;

using Command = vte::sixel::Command;
using Context = vte::sixel::Context;
using Mode = vte::sixel::Parser::Mode;
using ParseStatus = vte::sixel::Parser::ParseStatus;

// Parser tests

static inline constexpr auto
param_to_color_register(unsigned reg)
{
        return reg + 2; /* Public colour registers start at 2 */
}

static char const*
cmd_to_str(Command command)
{
        switch (command) {
        case Command::DECGRI: return "DECGRI";
        case Command::DECGRA: return "DECGRA";
        case Command::DECGCI: return "DECGCI";
        case Command::DECGCR: return "DECGCR";
        case Command::DECGCH: return "DECGCH";
        case Command::DECGNL: return "DECGNL";
        case Command::NONE:   return "NONE";
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "UNKOWN(%d/%02d)",
                         (int)command / 16,
                         (int)command % 16);
                return buf;
        }
}

enum class StType {
        C0,
        C1_UTF8,
        C1_EIGHTBIT
};

inline constexpr auto
ST(StType type)
{
        switch (type) {
        case StType::C0:          return "\e\\"sv;
        case StType::C1_UTF8:     return "\xc2\x9c"sv;
        case StType::C1_EIGHTBIT: return "\x9c"sv;
        default: __builtin_unreachable();
        }
}

inline constexpr auto
ST(Mode mode)
{
        switch (mode) {
        case Mode::UTF8:     return ST(StType::C1_UTF8);
        case Mode::EIGHTBIT: return ST(StType::C1_EIGHTBIT);
        case Mode::SEVENBIT: return ST(StType::C0);
        default: __builtin_unreachable();
        }
}

class Sequence : public vte::sixel::Sequence {
public:
        using Base = vte::sixel::Sequence;

        Sequence(Base const& seq)
                : Base{seq}
        {
        }

        Sequence(Command cmd,
                 std::vector<int> const& params) noexcept
                : Base{cmd}
        {
                assert(params.size() <= (sizeof(m_args) / sizeof(m_args[0])));
                for (auto p : params)
                        m_args[m_n_args++] = vte_seq_arg_init(std::min(p, 0xffff));
        }

        void append(std::string& str) const
        {
                if (command() != Command::NONE)
                        str.append(1, char(command()));
                for (auto i = 0u; i < size(); ++i) {
                        auto const p = param(i);
                        if (p != -1) {
                                char buf[12];
                                auto const len = g_snprintf(buf, sizeof(buf), "%d", p);
                                str.append(buf, len);
                        }
                        if ((i + 1) < size())
                                str.append(1, ';');
                }
        }

        void prettyprint(std::string& str) const
        {
                str.append("Sequence(");
                str.append(cmd_to_str(command()));
                if (size()) {
                        str.append(" ");
                        for (auto i = 0u; i < size(); ++i) {
                                auto const p = param(i);

                                char buf[12];
                                auto const len = g_snprintf(buf, sizeof(buf), "%d", p);
                                str.append(buf, len);

                                if ((i + 1) < size())
                                        str.append(1, ';');
                        }
                }
                str.append(")");
        }
};

constexpr bool operator==(Sequence const& lhs, Sequence const& rhs) noexcept
{
        if (lhs.command() != rhs.command())
                return false;

        auto const m = std::min(lhs.size(), rhs.size());
        for (auto n = 0u; n < m; ++n)
                if (lhs.param(n) != rhs.param(n))
                        return false;

        if (lhs.size() == rhs.size())
                return true;

        if ((lhs.size() == (rhs.size() + 1)) && lhs.param(rhs.size()) == -1)
                return true;

        if (((lhs.size() + 1) == rhs.size()) && rhs.param(lhs.size()) == -1)
                return true;

        return false;
}

class Sixel {
public:
        constexpr Sixel(uint8_t sixel)
                : m_sixel(sixel)
        {
                assert(m_sixel < 0b100'0000);
        }

        ~Sixel() = default;

        constexpr auto sixel() const noexcept { return m_sixel; }

        void append(std::string& str) const { str.append(1, char(m_sixel + 0x3f)); }

        void prettyprint(std::string& str) const
        {
                str.append("Sixel(");
                char buf[3];
                auto const len = g_snprintf(buf, sizeof(buf), "%02x", sixel());
                str.append(buf, len);
                str.append(")");
        }

private:
        uint8_t m_sixel{0};
};

constexpr bool operator==(Sixel const& lhs, Sixel const& rhs) noexcept
{
        return lhs.sixel() == rhs.sixel();
}

class Unicode {
public:
        Unicode(char32_t c) :
                m_c{c}
        {
                m_utf8_len = g_unichar_to_utf8(c, m_utf8_buf);
        }
        ~Unicode() = default;

        constexpr auto unicode() const noexcept { return m_c; }

        void append(std::string& str) const { str.append(m_utf8_buf, m_utf8_len); }

        void prettyprint(std::string& str) const
        {
                str.append("Unicode(");
                char buf[7];
                auto const len = g_snprintf(buf, sizeof(buf), "%04X", unicode());
                str.append(buf, len);
                str.append(")");
        }

private:
        char32_t m_c{0};
        size_t m_utf8_len{0};
        char m_utf8_buf[4]{0, 0, 0, 0};
};

constexpr bool operator==(Unicode const& lhs, Unicode const& rhs) noexcept
{
        return lhs.unicode() == rhs.unicode();
}

class C0Control {
public:
        C0Control(uint8_t c) :
                m_control{c}
        {
                assert(c < 0x20 || c == 0x7f);
        }
        ~C0Control() = default;

        constexpr auto control() const noexcept { return m_control; }

        void append(std::string& str) const { str.append(1, char(m_control)); }

        void prettyprint(std::string& str) const
        {
                str.append("C0(");
                char buf[3];
                auto const len = g_snprintf(buf, sizeof(buf), "%02X", control());
                str.append(buf, len);
                str.append(")");
        }

private:
        uint8_t m_control{0};
};

constexpr bool operator==(C0Control const& lhs, C0Control const& rhs) noexcept
{
        return lhs.control() == rhs.control();
}

class C1Control {
public:
        C1Control(uint8_t c) :
                m_control{c}
        {
                assert(c >= 0x80 && c < 0xa0);
                auto const len = g_unichar_to_utf8(c, m_utf8_buf);
                assert(len == 2);
        }
        ~C1Control() = default;

        constexpr auto control() const noexcept { return m_control; }

        void append(std::string& str,
                    Mode mode) const {
                switch (mode) {
                case Mode::UTF8:
                        str += std::string_view(m_utf8_buf, 2);
                        break;
                case Mode::EIGHTBIT:
                        str.append(1, char(m_control));
                        break;
                case Mode::SEVENBIT:
                        str.append(1, char(0x1b));
                        str.append(1, char(m_control - 0x40));
                        break;
                }
        }

        void prettyprint(std::string& str) const
        {
                str.append("C1(");
                char buf[3];
                auto const len = g_snprintf(buf, sizeof(buf), "%02X", control());
                str.append(buf, len);
                str.append(")");
        }

private:
        uint8_t m_control{0};
        char m_utf8_buf[2]{0, 0};
};

constexpr bool operator==(C1Control const& lhs, C1Control const& rhs) noexcept
{
        return lhs.control() == rhs.control();
}

class Raw {
public:
        Raw(uint8_t raw) :
                m_raw{raw}
        {
        }
        ~Raw() = default;

        constexpr auto raw() const noexcept { return m_raw; }

        void append(std::string& str) const { str += char(m_raw); }

        void prettyprint(std::string& str) const
        {
                str.append("Raw(");
                char buf[3];
                auto const len = g_snprintf(buf, sizeof(buf), "%02X", raw());
                str.append(buf, len);
                str.append(")");
        }

private:
        uint8_t m_raw{0};
};

constexpr bool operator==(Raw const& lhs, Raw const& rhs) noexcept
{
        return lhs.raw() == rhs.raw();
}

inline auto
DECGRI(int count) noexcept
{
        return Sequence{Command::DECGRI, {count}};
}

inline auto
DECGRA(int an,
       int ad,
       int w,
       int h) noexcept
{
        return Sequence{Command::DECGRA, {an, ad, w, h}};
}

inline auto
DECGCI(int reg) noexcept
{
        return Sequence{Command::DECGCI, {reg}};
}

inline auto
DECGCI_HLS(int reg,
           int h,
           int l,
           int s) noexcept
{
        return Sequence{Command::DECGCI, {reg, 1, h, l, s}};
}

inline auto
DECGCI_RGB(int reg,
           int r,
           int g,
           int b) noexcept
{
        return Sequence{Command::DECGCI, {reg, 2, r, g, b}};
}

inline auto
DECGCR() noexcept
{
        return Sequence{Command::DECGCR};
}

inline auto
DECGCH() noexcept
{
        return Sequence{Command::DECGCH};
}

inline auto
DECGNL() noexcept
{
        return Sequence{Command::DECGNL};
}

using Item = std::variant<Sequence, Sixel, C0Control, C1Control, Unicode, Raw>;
using ItemList = std::vector<Item>;

#if 0

#include <fmt/format.h>

class ItemPrinter {
public:
        ItemPrinter(Item const& item)
        {
                std::visit(*this, item);
        }

        ~ItemPrinter() = default;

        std::string const& string()    const noexcept { return m_str; }
        std::string_view string_view() const noexcept { return m_str; }

        void operator()(Sequence const& seq)      { seq.prettyprint(m_str);     }
        void operator()(Sixel const& sixel)       { sixel.prettyprint(m_str);   }
        void operator()(C0Control const& control) { control.prettyprint(m_str); }
        void operator()(C1Control const& control) { control.prettyprint(m_str); }
        void operator()(Unicode const& unicode)   { unicode.prettyprint(m_str); }
        void operator()(Raw const& raw)           { raw.prettyprint(m_str);     }

private:
        std::string m_str{};
};

static void
print_items(char const* intro,
            ItemList const& items)
{
        auto str = std::string{};

        for (auto const& item : items) {
                str += ItemPrinter{item}.string();
                str += " ";
        }

        fmt::println(stderr, "{}: {}", intro, str.c_str());
}

#endif

class ItemStringifier {
public:
        ItemStringifier(Mode mode = Mode::UTF8) :
                m_mode{mode}
        { }

        ItemStringifier(Item const& item,
                        Mode mode = Mode::UTF8) :
                m_mode{mode}
        {
                std::visit(*this, item);
        }

        ItemStringifier(ItemList const& items,
                        Mode mode = Mode::UTF8) :
                m_mode{mode}
        {
                for (auto&& i : items)
                        std::visit(*this, i);
        }

        ~ItemStringifier() = default;

        std::string string() const noexcept { return m_str; }
        std::string_view string_view() const noexcept { return m_str; }

        void operator()(Sequence const& seq)      { seq.append(m_str);             }
        void operator()(Sixel const& sixel)       { sixel.append(m_str);           }
        void operator()(C0Control const& control) { control.append(m_str);         }
        void operator()(C1Control const& control) { control.append(m_str, m_mode); }
        void operator()(Unicode const& unicode)   { unicode.append(m_str);         }
        void operator()(Raw const& raw)           { raw.append(m_str);             }

private:
        std::string m_str{};
        Mode m_mode;
};

class SimpleContext {

        friend class Parser;
public:
        SimpleContext() = default;
        ~SimpleContext() = default;

        auto parse(std::string_view const& str,
                   size_t end_pos = size_t(-1))
        {
                auto const beginptr = reinterpret_cast<uint8_t const*>(str.data());
                auto const endptr = reinterpret_cast<uint8_t const*>(beginptr + str.size());
                return m_parser.parse(beginptr, endptr, true, *this);
        }

        auto parse(Item const& item,
                   Mode input_mode)
        {
                return parse(ItemStringifier{{item}, input_mode}.string_view());
        }

        auto parse(ItemList const& list,
                   Mode input_mode)
        {
                return parse(ItemStringifier{list, input_mode}.string_view());
        }

        void set_mode(Mode mode)
        {
                m_parser.set_mode(mode);
        }

        void reset_mode()
        {
                set_mode(Mode::UTF8);
        }

        void reset()
        {
                m_parser.reset();
                m_parsed_items.clear();
                m_st = 0;
        }

        auto const& parsed_items() const noexcept { return m_parsed_items; }

        void SIXEL(uint8_t raw) noexcept
        {
                m_parsed_items.push_back(Sixel(raw));
        }

        void SIXEL_CMD(vte::sixel::Sequence const& seq) noexcept
        {
                m_parsed_items.push_back(Sequence(seq));
        }

        void SIXEL_ST(char32_t st) noexcept
        {
                m_st = st;
        }

        vte::sixel::Parser m_parser{};
        ItemList m_parsed_items{};
        char32_t m_st{0};

}; // class SimpleContext

/*
 * assert_parse:
 * @context:
 * @mode:
 * @str:
 * @str_size:
 * @expected_parsed_len:
 * @expected_status:
 *
 * Asserts that parsing @str (up to @str_size, or until its size if @str_size is -1)
 * in mode @mode results in @expected_status, with the endpointer pointing to the end
 * of @str if @expected_parsed_len is -1, or to @expected_parsed_len otherwise.
 */
template<class C>
static void
assert_parse(C& context,
             Mode mode,
             std::string_view const& str,
             size_t str_size = size_t(-1),
             size_t expected_parse_end = size_t(-1),
             ParseStatus expected_status = ParseStatus::COMPLETE,
             int line = __builtin_LINE())
{
        context.reset();
        context.set_mode(mode);

        auto const beginptr = reinterpret_cast<uint8_t const*>(str.data());
        auto const len = str_size == size_t(-1) ? str.size() : str_size;
        auto const [status, ip] = context.parse(str, len);
        auto const parsed_len = size_t(ip - beginptr);

        g_assert_cmpint(int(status), ==, int(expected_status));
        g_assert_cmpint(parsed_len, ==, expected_parse_end == size_t(-1) ? len : expected_parse_end);
}

/*
 * assert_parse:
 * @context:
 * @mode:
 * @str:
 * @expected_items:
 * @str_size:
 * @expected_parsed_len:
 * @expected_status:
 *
 * Asserts that parsing @str (up to @str_size, or until its size if @str_size is -1)
 * in mode @mode results in @expected_status, with the parsed items equal to
 * @expected_items, and the endpointer pointing to the end of @str if @expected_parsed_len
 * is -1, or to @expected_parsed_len otherwise.
 */
template<class C>
static void
assert_parse(C& context,
             Mode mode,
             std::string_view const& str,
             ItemList const& expected_items,
             size_t str_size = size_t(-1),
             size_t expected_parse_end = size_t(-1),
             ParseStatus expected_status = ParseStatus::COMPLETE,
             int line = __builtin_LINE())
{
        assert_parse(context, mode, str, str_size, expected_parse_end, expected_status, line);

        g_assert_true(context.parsed_items() == expected_items);
}

/*
 * assert_parse_st:
 *
 * Like assert_parse above, but ST-terminates the passed string.
 */
template<class C>
static void
assert_parse_st(C& context,
                Mode mode,
                std::string_view const& str,
                size_t str_size = size_t(-1),
                size_t expected_parse_end = size_t(-1),
                ParseStatus expected_status = ParseStatus::COMPLETE,
                StType st = StType::C0,
                int line = __builtin_LINE())
{
        auto str_st = std::string{str};
        str_st.append(ST(st));
        auto str_st_size = str_size;

        assert_parse(context, mode, str_st, str_st_size, expected_parse_end, expected_status, line);
}

/*
 * assert_parse_st:
 *
 * Like assert_parse above, but ST-terminates the passed string.
 */
template<class C>
static void
assert_parse_st(C& context,
                Mode mode,
                std::string_view const& str,
                ItemList const& expected_items,
                size_t str_size = size_t(-1),
                size_t expected_parse_end = size_t(-1),
                ParseStatus expected_status = ParseStatus::COMPLETE,
                StType st = StType::C0,
                int line = __builtin_LINE())
{
        auto str_st = std::string{str};
        str_st.append(ST(st));
        auto str_st_size = str_size == size_t(-1) ? str_st.size() : str_size;

        assert_parse(context, mode, str_st, expected_items, str_st_size, expected_parse_end, expected_status, line);
}

/*
 * assert_parse_st:
 *
 * Like assert_parse above, but ST-terminates the passed string.
 */
template<class C>
static void
assert_parse_st(C& context,
                Mode mode,
                ItemList const& items,
                ItemList const& expected_items,
                ParseStatus expected_status = ParseStatus::COMPLETE,
                StType st = StType::C0,
                int line = __builtin_LINE())
{
        assert_parse_st(context, mode, ItemStringifier{items, mode}.string_view(), expected_items, -1, -1, expected_status, st, line);
}

static void
test_parser_seq_params(SimpleContext& context,
                       Mode mode,
                       std::vector<int> const& params)
{
        for (auto i = 0x20; i < 0x3f; ++i) {
                if (i >= 0x30 && i < 0x3c) // Parameter characters
                        continue;


                auto const items = ItemList{Sequence{Command(i), params}};
                assert_parse_st(context, mode, items,
                                (i == 0x20) ? ItemList{} /* 0x20 is ignored */ : items);
        }
}

static void
test_parser_seq_params(SimpleContext& context,
                       vte_seq_arg_t params[8],
                       bool as_is = false)
{
        for (auto mode : {Mode::UTF8, Mode::EIGHTBIT, Mode::SEVENBIT}) {
                context.set_mode(mode);

                for (auto n = 0; n <= 8; ++n) {
                        auto pv = std::vector<int>(&params[0], &params[n]);

                        test_parser_seq_params(context, mode, pv);

                        if (n > 0 && !as_is) {
                                pv[n - 1] = -1;
                                test_parser_seq_params(context, mode, pv);
                        }
                }
        }

        context.reset_mode();
}

static void
test_parser_seq_params(void)
{
        auto context = SimpleContext{};

        /* Tests sixel commands, which have the form I P...P with an initial byte
         * in the 2/0..2/15, 3/12..3/14 range, and parameter bytes P from 3/0..3/11.
         */
        vte_seq_arg_t params1[8]{1, 0, 1000, 10000, 65534, 65535, 65536, 1};
        test_parser_seq_params(context, params1);

        vte_seq_arg_t params2[8]{1, -1, -1, -1, 1, -1, 1, 1};
        test_parser_seq_params(context, params2, true);
}

static void
test_parser_seq_subparams(void)
{
        // Test that subparams cause the whole sequence to be ignored

        auto context = SimpleContext{};

        for (auto mode : {Mode::UTF8, Mode::EIGHTBIT, Mode::SEVENBIT}) {

                assert_parse_st(context, mode, "#0;1:2;#:#;1;3:#;:;;"sv, ItemList{});
        }
}

static void
test_parser_seq_params_clear(void)
{
        /* Check that parameters are cleared from the last sequence */

        auto context = SimpleContext{};

        for (auto mode : {Mode::UTF8, Mode::EIGHTBIT, Mode::SEVENBIT}) {
                auto items = ItemList{Sequence{Command::DECGCI, {0, 1, 2, 3, 4, 5, 6, 7}},
                                      Sequence{Command::DECGRI, {5, 3}},
                                      Sequence{Command::DECGNL}};
                assert_parse_st(context, mode, items, items);

                auto parsed_items = context.parsed_items();

                /* Verify that non-specified paramaters have default value */
                auto& item1 = std::get<Sequence>(parsed_items[1]);
                for (auto n = 2; n < 8; ++n)
                        g_assert_cmpint(item1.param(n), ==, -1);


                auto& item2 = std::get<Sequence>(parsed_items[2]);
                for (auto n = 0; n < 8; ++n)
                        g_assert_cmpint(item2.param(n), ==, -1);
        }
}

static void
test_parser_seq_params_max(void)
{
        /* Check that an excessive number of parameters causes the
         * sequence to be ignored.
         */

        auto context = SimpleContext{};

        auto items = ItemList{Sequence{Command::DECGRA, {0, 1, 2, 3, 4, 5, 6, 7}}};
        auto str = ItemStringifier{items, Mode::SEVENBIT}.string();

        /* The sequence with VTE_SIXEL_PARSER_ARG_MAX args must be parsed */
        assert_parse_st(context, Mode::UTF8, str, items);

        /* Now test that adding one more parameter (whether with an
         * explicit value, or default), causes the sequence to be ignored.
         */
        assert_parse_st(context, Mode::UTF8, str + ";8"s, ItemList{});
        assert_parse_st(context, Mode::UTF8, str + ";"s, ItemList{});
}

static void
test_parser_seq_glue_arg(void)
{
        /* The sixel Sequence's parameter accessors are copied from the main parser's
         * Sequence class, so we don't need to test them here again.
         */
}

static void
test_parser_st(void)
{
        /* Test that ST is recognised in all forms and from all states, and
         * that different-mode C1 ST is not recognised.
         */

        auto context = SimpleContext{};

        assert_parse(context, Mode::UTF8, "?\x9c\e\\"sv, {Sixel{0}});
        assert_parse(context, Mode::UTF8, "!5\x9c\e\\"sv, {Sequence{Command::DECGRI, {5}}});
        assert_parse(context, Mode::UTF8, "5\x9c\e\\"sv, ItemList{});
        assert_parse(context, Mode::UTF8, "\x9c\xc2\e\\"sv, ItemList{});

        assert_parse(context, Mode::UTF8, "?\x9c\xc2\x9c"sv, {Sixel{0}});
        assert_parse(context, Mode::UTF8, "!5\x9c\xc2\x9c"sv, {Sequence{Command::DECGRI, {5}}});
        assert_parse(context, Mode::UTF8, "5\x9c\xc2\x9c"sv, ItemList{});
        assert_parse(context, Mode::UTF8, "\x9c\xc2\xc2\x9c"sv, ItemList{});

        assert_parse(context, Mode::EIGHTBIT, "?\e\\"sv, {Sixel{0}});
        assert_parse(context, Mode::EIGHTBIT, "!5\e\\"sv, {Sequence{Command::DECGRI, {5}}});
        assert_parse(context, Mode::EIGHTBIT, "5\e\\"sv, ItemList{});
        assert_parse(context, Mode::EIGHTBIT, "\xc2\e\\"sv, ItemList{});

        assert_parse(context, Mode::EIGHTBIT, "?\xc2\x9c"sv, {Sixel{0}});
        assert_parse(context, Mode::EIGHTBIT, "!5\xc2\x9c"sv, {Sequence{Command::DECGRI, {5}}});
        assert_parse(context, Mode::EIGHTBIT, "5\xc2\x9c"sv, ItemList{});
        assert_parse(context, Mode::EIGHTBIT, "\xc2\xc2\x9c"sv, ItemList{});

        assert_parse(context, Mode::SEVENBIT, "?\xc2\x9c\e\\"sv, {Sixel{0}});
        assert_parse(context, Mode::SEVENBIT, "!5\xc2\x9c\e\\"sv, {Sequence{Command::DECGRI, {5}}});
        assert_parse(context, Mode::SEVENBIT, "5\xc2\x9c\e\\"sv, ItemList{});
        assert_parse(context, Mode::SEVENBIT, "\xc2\x9c\xc2\e\\"sv, ItemList{});
}

static constexpr auto
test_string()
{
        return "a#22a#22\xc2z22a22\xc2"sv;
}

template<class C>
static void
test_parser_insert(C& context,
                   Mode mode,
                   std::string_view const& str,
                   std::string_view const& insert_str,
                   ParseStatus expected_status = ParseStatus::COMPLETE,
                   int line = __builtin_LINE())
{
        for (auto pos = 0u; pos <= str.size(); ++pos) {
                auto estr = std::string{str};
                estr.insert(pos, insert_str);

                assert_parse_st(context, mode, estr, -1,
                                expected_status == ParseStatus::COMPLETE ? size_t(-1) : size_t(pos),
                                expected_status, StType::C0, line);

                if (expected_status == ParseStatus::COMPLETE) {
                        auto items = context.parsed_items(); // copy

                        assert_parse_st(context, mode, str);
                        assert(items == context.parsed_items());
                }
        }
}

template<class C>
static void
test_parser_insert(C& context,
                   std::string_view const& str,
                   std::string_view const& insert_str,
                   ParseStatus expected_status = ParseStatus::COMPLETE,
                   int line = __builtin_LINE())
{
        for (auto mode : {Mode::UTF8, Mode::EIGHTBIT, Mode::SEVENBIT}) {
                test_parser_insert(context, mode, str, insert_str, expected_status, line);
        }
}

static void
test_parser_controls_c0_esc(void)
{
        /* Test that ESC (except C0 ST) always aborts the parsing at the position of the ESC */

        auto context = SimpleContext{};
        auto const str = test_string();

        for (auto c = 0x20; c < 0x7f; ++c) {
                if (c == 0x5c) /* '\' */
                        continue;

                char esc[2] = {0x1b, char(c)};
                test_parser_insert(context, str, {esc, 2}, ParseStatus::ABORT);
        }
}

static void
test_parser_controls_c0_can(void)
{
        /* Test that CAN is handled correctly in all states */

        auto context = SimpleContext{};

        for (auto mode : {Mode::UTF8, Mode::EIGHTBIT, Mode::SEVENBIT}) {

                assert_parse_st(context, mode, "@\x18"sv, {Sixel{1}}, -1, 1, ParseStatus::ABORT);
                assert_parse_st(context, mode, "!5\x18"sv, {Sequence{Command::DECGRI, {5}}}, -1, 2, ParseStatus::ABORT);
                assert_parse_st(context, mode, "5\x18"sv, ItemList{}, -1, 1, ParseStatus::ABORT);
                assert_parse_st(context, mode, "\xc2\x18"sv, ItemList{}, -1, 1, ParseStatus::ABORT);
        }
}

static void
test_parser_controls_c0_sub(void)
{
        /* Test that SUB is handled correctly in all states */

        auto context = SimpleContext{};

        for (auto mode : {Mode::UTF8, Mode::EIGHTBIT, Mode::SEVENBIT}) {

                assert_parse_st(context, mode, "@\x1a"sv, {Sixel{1}, Sixel{0}});

                /* The parser chooses to not dispatch the current sequence on SUB; see the
                 * comment in the Parser class. Otherwise there'd be a
                 * Sequence{Command::DECGRI, {5}} as the first expected item here.
                 */
                assert_parse_st(context, mode, "!5\x1a"sv, {Sixel{0}});

                assert_parse_st(context, mode, "5\x1a"sv, {Sixel{0}});
                assert_parse_st(context, mode, "\xc2\x1a"sv, {Sixel{0}});
        }
}

static void
test_parser_controls_c0_ignored(void)
{
        /* Test that all C0 controls except ESC, CAN, and SUB, are ignored,
         * that is, parsing a string results in the same parsed item when inserting
         * the C0 control at any position (except after \xc2 + 0x80..0x9f in UTF-8 mode,
         * where the \xc2 + C0 produces an U+FFFD (which is ignored) plus the raw C1 which
         * is itself ignored).
         */

        auto context = SimpleContext{};
        auto const str = test_string();

        for (auto c0 = 0; c0 < 0x20; ++c0) {
                if (c0 == 0x18 /* CAN */ ||
                    c0 == 0x1a /* SUB */ ||
                    c0 == 0x1b /* ESC */)
                        continue;

                char c[1] = {char(c0)};
                test_parser_insert(context, str, {c, 1});

                assert_parse_st(context, Mode::UTF8, "?\xc2"s + std::string{c, 1} + "\x80@"s, {Sixel{0}, Sixel{1}});
        }
}

static void
test_parser_controls_del(void)
{
        /* Test that DEL is ignored (except between 0xc2 and 0x80..0x9f in UTF-8 mode) */

        auto context = SimpleContext{};

        for (auto mode : {Mode::UTF8, Mode::EIGHTBIT, Mode::SEVENBIT}) {

                assert_parse_st(context, mode, "!2\x7f;3"sv, {Sequence{Command::DECGRI, {2, 3}}});
                assert_parse_st(context, mode, "2\x7f;3"sv, ItemList{});
        }

        assert_parse_st(context, Mode::UTF8, "?\xc2\x7f\x9c", {Sixel{0}});
}

static void
test_parser_controls_c1(void)
{
        /* Test that any C1 control aborts the parsing at the insertion position,
         * except in 7-bit mode where C1 controls are ignored.
         */

        auto context = SimpleContext{};
        auto const str = test_string();
        for (auto c1 = 0x80; c1 < 0xa0; ++c1) {
                if (c1 == 0x9c /* ST */)
                        continue;

                char c1_utf8[2] = {char(0xc2), char(c1)};
                test_parser_insert(context, Mode::UTF8, str, {c1_utf8, 2}, ParseStatus::ABORT);
                test_parser_insert(context, Mode::SEVENBIT, str, {c1_utf8, 2});

                char c1_raw[1] = {char(c1)};
                test_parser_insert(context, Mode::EIGHTBIT, str, {c1_raw, 1}, ParseStatus::ABORT);
                test_parser_insert(context, Mode::SEVENBIT, str, {c1_raw, 1});
        }
}

// Context tests

class TestContext: public Context {
public:
        using base_type = Context;
        using base_type::base_type;

        auto parse(std::string_view const& str)
        {
                auto const beginptr = reinterpret_cast<uint8_t const*>(str.data());
                auto const endptr = reinterpret_cast<uint8_t const*>(beginptr + str.size());
                return Context::parse(beginptr, endptr, true);
        }

}; // class TestContext

template<class C>
static void
parse_image(C& context,
            std::string_view const& str,
            unsigned fg_red,
            unsigned fg_green,
            unsigned fg_blue,
            unsigned bg_red,
            unsigned bg_green,
            unsigned bg_blue,
            bool private_color_registers = true,
            int line = __builtin_LINE())
{
        context.reset();
        context.prepare(-1, /* no ID */
                        0x50 /* C0 DCS */,
                        fg_red, fg_green, fg_blue,
                        bg_red, bg_green, bg_blue,
                        false /* bg transparent */,
                        private_color_registers);

        auto str_st = std::string{str};
        str_st.append(ST(StType::C0));
        auto [status, ip] = context.parse(str_st);
        g_assert_cmpint(int(status), ==, int(ParseStatus::COMPLETE));
}

template<class C>
static void
parse_image(C& context,
            ItemList const& items,
            unsigned fg_red,
            unsigned fg_green,
            unsigned fg_blue,
            unsigned bg_red,
            unsigned bg_green,
            unsigned bg_blue,
            bool private_color_registers = true,
            int line = __builtin_LINE())
{
        parse_image(context, ItemStringifier(items).string(),
                    fg_red, fg_green, fg_blue,
                    bg_red, bg_green, bg_blue,
                    private_color_registers,
                    line);
}

template<class C>
static void
parse_image(C& context,
            std::string_view const& str,
            int line = __builtin_LINE())
{
        parse_image(context, str, 0xffu, 0xffu, 0xffu, 0xff8, 0xffu, 0xffu, true, line);
}

template<class C>
static void
parse_image(C& context,
            ItemList const& items,
            int line = __builtin_LINE())
{
        parse_image(context, ItemStringifier{items, Mode::UTF8}.string_view(), line);
}

template<class C>
static auto
parse_pixels(C& context,
             std::string_view const& str,
             unsigned extra_width_stride = 0,
             int line = __builtin_LINE())
{
        parse_image(context, str, line);
        auto size = size_t{};
        auto ptr = vte::glib::take_free_ptr(context.image_data_indexed(&size, extra_width_stride));
        return std::pair{std::move(ptr), size};
}

/* BEGIN */

/* The following code is copied from xterm/graphics.c where it is under the
 * licence below; and modified and used here under the GNU Lesser General Public
 * Licence, version 3 (or, at your option), any later version.
 */

/*
 * Copyright 2013-2019,2020 by Ross Combs
 * Copyright 2013-2019,2020 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

static void
hls2rgb_double(int
               h,
               int l,
               int s,
               int* r,
               int* g,
               int* b) noexcept
{
    const int hs = ((h + 240) / 60) % 6;
    const double lv = l / 100.0;
    const double sv = s / 100.0;
    double c, x, m, c2;
    double r1, g1, b1;

    if (s == 0) {
            *r = *g = *b = (short) (lv * 255. + 0.5);
        return;
    }

    c2 = (2.0 * lv) - 1.0;
    if (c2 < 0.0)
        c2 = -c2;
    c = (1.0 - c2) * sv;
    x = (hs & 1) ? c : 0.0;
    m = lv - 0.5 * c;

    switch (hs) {
    case 0:
        r1 = c;
        g1 = x;
        b1 = 0.0;
        break;
    case 1:
        r1 = x;
        g1 = c;
        b1 = 0.0;
        break;
    case 2:
        r1 = 0.0;
        g1 = c;
        b1 = x;
        break;
    case 3:
        r1 = 0.0;
        g1 = x;
        b1 = c;
        break;
    case 4:
        r1 = x;
        g1 = 0.0;
        b1 = c;
        break;
    case 5:
        r1 = c;
        g1 = 0.0;
        b1 = x;
        break;
    default:
        *r = (short) 255;
        *g = (short) 255;
        *b = (short) 255;
        return;
    }

    *r = (short) ((r1 + m) * 255.0 + 0.5);
    *g = (short) ((g1 + m) * 255.0 + 0.5);
    *b = (short) ((b1 + m) * 255.0 + 0.5);

    if (*r < 0)
        *r = 0;
    else if (*r > 255)
        *r = 255;
    if (*g < 0)
        *g = 0;
    else if (*g > 255)
        *g = 255;
    if (*b < 0)
        *b = 0;
    else if (*b > 255)
        *b = 255;
}

/* This is essentially Context::make_color_hls from sixel-context.cc,
 * only changed to return the colour components separately.
 */
static void
hls2rgb_int(int h,
            int l,
            int s,
            int* r,
            int* g,
            int* b) noexcept
{
        auto const c2p = std::abs(2 * l - 100);
        auto const cp = ((100 - c2p) * s) << 1;
        auto const hs = ((h + 240) / 60) % 6;
        auto const xp = (hs & 1) ? cp : 0;
        auto const mp = 200 * l - (cp >> 1);

        int r1p, g1p, b1p;
        switch (hs) {
        case 0:
                r1p = cp;
                g1p = xp;
                b1p = 0;
                break;
        case 1:
                r1p = xp;
                g1p = cp;
                b1p = 0;
                break;
        case 2:
                r1p = 0;
                g1p = cp;
                b1p = xp;
                break;
        case 3:
                r1p = 0;
                g1p = xp;
                b1p = cp;
                break;
        case 4:
                r1p = xp;
                g1p = 0;
                b1p = cp;
                break;
        case 5:
                r1p = cp;
                g1p = 0;
                b1p = xp;
                break;
        default:
                __builtin_unreachable();
        }

        *r = ((r1p + mp) * 255 + 10000) / 20000;
        *g = ((g1p + mp) * 255 + 10000) / 20000;
        *b = ((b1p + mp) * 255 + 10000) / 20000;
}

/* END */

static void
test_context_color_hls(void)
{
        /* Test that our HLS colour conversion gives the right results
         * by comparing it against the xterm/libsixel implementation.
         *
         * The values may differ by 1, which happen only for (L, S) in
         * {(5, 100), (40, 75), (50, 80), (60, 75), (75, 60), (95, 100)}.
         * There, one or more of the R, G, B components' unscaled values,
         * times 255, produces an exact fraction of .5 in hsl2rgb_double,
         * which, plus 0.5,, and due to inexactness, result in the truncated
         * value "(short)v" being one less than the result of the integer
         * computation.
         */

        for (auto h = 0; h <= 360; ++h) {
                for (auto l = 0; l <= 100; ++l) {
                        for (auto s = 0; s <= 100; ++s) {
                                int rd, gd, bd, ri, gi, bi;

                                hls2rgb_double(h, l, s, &rd, &gd, &bd);
                                hls2rgb_int(h, l, s, &ri, &gi, &bi);

                                g_assert_true((rd == ri || (rd + 1) == ri) &&
                                              (gd == gi || (gd + 1) == gi) &&
                                              (bd == bi || (bd + 1) == bi));
                        }
                }
        }
}

template<class C>
static void
assert_image_dimensions(C& context,
                        unsigned width,
                        unsigned height,
                        int line = __builtin_LINE())
{
        g_assert_cmpuint(context.image_width(), ==, width);
        g_assert_cmpuint(context.image_height(), ==, height);
}

static void
test_context_raster_attributes(void)
{
        /* Test that DECGRA sets the image dimensions */

        auto context = TestContext{};
        parse_image(context, "\"0;0;64;128"sv);
        assert_image_dimensions(context, 64, 128);
}

static void
test_context_repeat(void)
{
        /* Test that DECGRI repetition works */

        auto context = TestContext{};
        auto [pixels, size] = parse_pixels(context, "#1!5@"sv);
        assert_image_dimensions(context, 5, 1);

        auto data = pixels.get();
        auto const v = *data++;
        for (auto x = 1u; x < context.image_width(); ++x)
                g_assert_cmpuint(*data++, ==, v);

        g_assert_cmpuint(size_t(data - pixels.get()), <=, size);

	/* Check that repeat param 0 is trated as 1 */
	parse_image(context, {DECGRI(0), Sixel(1u << 0)});
	assert_image_dimensions(context, 1, 1);

	/* Check that omitted param is treated as default */
	parse_image(context, {DECGRI(-1), Sixel(1u << 0)});
	assert_image_dimensions(context, 1, 1);
}

static void
test_context_scanlines_grow(void)
{
        /* Test that scanlines grow on demand */

        auto context = TestContext{};
        parse_image(context, "@$AA$?$??~-~"sv);
        assert_image_dimensions(context, 3, 12);
}

static void
test_context_scanlines_underfull(void)
{
        /* Test that the image height is determined by the last set sixel, not
         * necessarily the number of scanlines.
         */

        auto context = TestContext{};

        parse_image(context, "?"sv);
        assert_image_dimensions(context, 1, 0);

        for (auto n = 0; n < 6; ++n) {
                parse_image(context, {Sixel(1u << n)});
                assert_image_dimensions(context, 1, n + 1);

                parse_image(context, {Sixel(0), Sixel(0), DECGNL(), Sixel(1u << n)});
                assert_image_dimensions(context, 2, 6 + n + 1);
        }
}

static void
test_context_scanlines_max_width(void)
{
        /* Test that scanlines up to max_width() work, and scanlines longer than that
         * are accepted but do not write outside the maximum width.
         */

        auto context = TestContext{};

        parse_image(context, {Sixel(1u << 0), DECGNL(), DECGRI(context.max_width() - 1), Sixel(0x3f)});
        assert_image_dimensions(context, context.max_width() - 1, 12);

        parse_image(context, {Sixel(1u << 0), DECGNL(), DECGRI(context.max_width()), Sixel(0x3f)});
        assert_image_dimensions(context, context.max_width(), 12);

        parse_image(context, {Sixel(1u << 0), DECGNL(), DECGRI(context.max_width() + 1), Sixel(0x3f)});
        assert_image_dimensions(context, context.max_width(), 12);
}

static void
test_context_scanlines_max_height(void)
{
        /* Test that scanlines up to max_height() work, and scanlines beyond that
         * are accepted but do nothing.
         */

        auto context = TestContext{};

        auto items = ItemList{};
        for (auto n = 0u; n < (context.max_height() / 6 - 1); ++n) {
                if (n > 0)
                        items.emplace_back(DECGNL());
                items.emplace_back(Sixel(1u << 5));
        }

        parse_image(context, items);
        assert_image_dimensions(context, 1, context.max_height() - 6);

        items.emplace_back(DECGNL());
        items.emplace_back(Sixel(1u << 4));

        parse_image(context, items);
        assert_image_dimensions(context, 1, context.max_height() - 1);

        items.emplace_back(DECGCR());
        items.emplace_back(Sixel(1u << 5));

        parse_image(context, items);
        assert_image_dimensions(context, 1, context.max_height());

        /* Image cannot grow further */

        items.emplace_back(DECGNL());
        items.emplace_back(Sixel(1u << 0));

        parse_image(context, items);
        assert_image_dimensions(context, 1, context.max_height());

        items.emplace_back(DECGNL());
        items.emplace_back(Sixel(1u << 5));

        parse_image(context, items);
        assert_image_dimensions(context, 1, context.max_height());
}

static void
test_context_image_stride(void)
{
        /* Test that data in the stride padding is set to background */

        auto context = TestContext{};

        auto const extra_stride = 3u;
        auto [pixels, size] = parse_pixels(context, "#1~~-~~"sv, extra_stride);
        assert_image_dimensions(context, 2, 12);

        auto data = pixels.get();
        auto const reg = param_to_color_register(1);

        for (auto y = 0u; y < context.image_height(); ++y) {
                for (auto x = 0u; x < context.image_width(); ++x)
                        g_assert_cmpuint(*data++, ==, unsigned(reg));
                for (auto e = 0u; e < extra_stride; ++e)
                        g_assert_cmpuint(*data++, ==, 0);
        }

        g_assert_cmpuint(size_t(data - pixels.get()), <=, size);
}

class RGB {
public:
        uint8_t r{0};
        uint8_t g{0};
        uint8_t b{0};

        RGB() = default;
        ~RGB() = default;

        RGB(int rv, int gv, int bv)
                : r(rv), g(gv), b(bv)
        {
        }
};

static void
test_context_image_palette(void)
{
        /* Test that the colour palette is recognised, and that colour registers
         * wrap around.
         */

        auto make_color_rgb = [](unsigned rp,
                                 unsigned gp,
                                 unsigned bp) constexpr noexcept -> auto
        {
                auto scale = [](unsigned value) constexpr noexcept -> auto
                {
                        return (value * 255u + 50u) / 100u;
                };

                auto make_color = [](unsigned r,
                                     unsigned g,
                                     unsigned b) constexpr noexcept -> Context::color_t
                {
                        if constexpr (std::endian::native == std::endian::little) {
                                return b | g << 8 | r << 16 | 0xffu << 24 /* opaque */;
                        } else if constexpr (std::endian::native == std::endian::big) {
                                return 0xffu /* opaque */ | r << 8 | g << 16 | b << 24;
                        } else {
                                __builtin_unreachable();
                        }
                };

                return make_color(scale(rp), scale(gp), scale(bp));
        };

        auto context = TestContext{};

        std::array<RGB, context.num_colors()> palette;
        for (auto& p : palette) {
                p = RGB(g_test_rand_int_range(0, 100),
                        g_test_rand_int_range(0, 100),
                        g_test_rand_int_range(0, 100));
        }

        auto items = ItemList{};
        auto reg = context.num_colors();
        for (auto const& p : palette) {
                items.emplace_back(DECGCI_RGB(reg++, p.r, p.g, p.b));
        }

        parse_image(context, items);

        for (auto n = 0; n < context.num_colors(); ++n) {
                g_assert_cmpuint(make_color_rgb(palette[n].r, palette[n].g, palette[n].b),
                                 ==,
                                 context.color(param_to_color_register(n)));
        }
}

static void
test_context_image_compositing(void)
{
        /* Test that multiple sixels in different colours are composited. */

        auto context = TestContext{};

        auto [pixels, size] = parse_pixels(context,
                                           "#256!24F$#257!24w-#258!24F$#259!24w-#260!24F$#261!24w"sv);

        auto data = pixels.get();
        for (auto y = 0u; y < context.image_height(); ++y) {
                auto const reg = param_to_color_register((256 + y / 3));
                for (auto x = 0u; x < context.image_width(); ++x)
                        g_assert_cmpuint(*data++, ==, reg);
        }


        g_assert_cmpuint(size_t(data - pixels.get()), <=, size);
}

// Main

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/sixel/parser/sequences/parameters", test_parser_seq_params);
        g_test_add_func("/vte/sixel/parser/sequences/subparameters", test_parser_seq_subparams);
        g_test_add_func("/vte/sixel/parser/sequences/parameters-clear", test_parser_seq_params_clear);
        g_test_add_func("/vte/sixel/parser/sequences/parameters-max", test_parser_seq_params_max);
        g_test_add_func("/vte/sixel/parser/sequences/glue/arg", test_parser_seq_glue_arg);
        g_test_add_func("/vte/sixel/parser/st", test_parser_st);
        g_test_add_func("/vte/sixel/parser/controls/c0/escape", test_parser_controls_c0_esc);
        g_test_add_func("/vte/sixel/parser/controls/c0/can", test_parser_controls_c0_can);
        g_test_add_func("/vte/sixel/parser/controls/c0/sub", test_parser_controls_c0_sub);
        g_test_add_func("/vte/sixel/parser/controls/c0/ignored", test_parser_controls_c0_ignored);
        g_test_add_func("/vte/sixel/parser/controls/del", test_parser_controls_del);
        g_test_add_func("/vte/sixel/parser/controls/c1", test_parser_controls_c1);
        g_test_add_func("/vte/sixel/context/color/hls", test_context_color_hls);
        g_test_add_func("/vte/sixel/context/raster-attributes", test_context_raster_attributes);
        g_test_add_func("/vte/sixel/context/repeat", test_context_repeat);
        g_test_add_func("/vte/sixel/context/scanlines/grow", test_context_scanlines_grow);
        g_test_add_func("/vte/sixel/context/scanlines/underfull", test_context_scanlines_underfull);
        g_test_add_func("/vte/sixel/context/scanlines/max-width", test_context_scanlines_max_width);
        g_test_add_func("/vte/sixel/context/scanlines/max-height", test_context_scanlines_max_height);
        g_test_add_func("/vte/sixel/context/image/stride", test_context_image_stride);
        g_test_add_func("/vte/sixel/context/image/palette", test_context_image_palette);
        g_test_add_func("/vte/sixel/context/image/compositing", test_context_image_compositing);

        return g_test_run();
}
