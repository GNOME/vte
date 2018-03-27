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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace std::literals;

#include <glib.h>

#include "parser.hh"
#include "parser-glue.hh"
#include "parser-charset-tables.hh"

static struct vte_parser* parser;

#if 0
static char const*
seq_to_str(unsigned int type)
{
        switch (type) {
        case VTE_SEQ_NONE: return "NONE";
        case VTE_SEQ_IGNORE: return "IGNORE";
        case VTE_SEQ_GRAPHIC: return "GRAPHIC";
        case VTE_SEQ_CONTROL: return "CONTROL";
        case VTE_SEQ_ESCAPE: return "ESCAPE";
        case VTE_SEQ_CSI: return "CSI";
        case VTE_SEQ_DCS: return "DCS";
        case VTE_SEQ_OSC: return "OSC";
        case VTE_SEQ_APC: return "APC";
        case VTE_SEQ_PM: return "PM";
        case VTE_SEQ_SOS: return "SOS";
        case VTE_SEQ_SCI: return "SCI";
        default:
                g_assert_not_reached();
        }
}

static char const*
cmd_to_str(unsigned int command)
{
        switch (command) {
#define _VTE_CMD(cmd) case VTE_CMD_##cmd: return #cmd;
#include "parser-cmd.hh"
#undef _VTE_CMD
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "UNKOWN(%u)", command);
                return buf;
        }
}

static char const*
charset_to_str(unsigned int cs)
{
        switch (cs) {
#define _VTE_CHARSET_PASTE(name) case VTE_CHARSET_##name: return #name;
#define _VTE_CHARSET(name) _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET_ALIAS_PASTE(name1,name2)
#define _VTE_CHARSET_ALIAS(name1,name2)
#include "parser-charset.hh"
#undef _VTE_CHARSET_PASTE
#undef _VTE_CHARSET
#undef _VTE_CHARSET_ALIAS_PASTE
#undef _VTE_CHARSET_ALIAS
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "UNKOWN(%u)", cs);
                return buf;
        }
}
#endif

static const char c0str[][6] = {
        "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
        "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
        "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
        "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US",
        "SPACE"
};

static const char c1str[][5] = {
        "DEL",
        "0x80", "0x81", "BPH", "NBH", "0x84", "NEL", "SSA", "ESA",
        "HTS", "HTJ", "VTS", "PLD", "PLU", "RI", "SS2", "SS3",
        "DCS", "PU1", "PU2", "STS", "CCH", "MW", "SPA", "EPA",
        "SOS", "0x99", "SCI", "CSI", "ST", "OSC", "PM", "APC"
};

static void
print_escaped(std::u32string const& s)
{
        for (auto it : s) {
                uint32_t c = (char32_t)it;

                if (c <= 0x20)
                        g_print("%s ", c0str[c]);
                else if (c < 0x7f)
                        g_print("%c ", c);
                else if (c < 0xa0)
                        g_print("%s ", c1str[c - 0x7f]);
                else
                        g_print("U+%04X", c);
        }
        g_print("\n");
}

#if 0
static void
print_seq(const struct vte_seq *seq)
{
        auto c = seq->terminator;
        if (seq->command == VTE_CMD_GRAPHIC) {
                char buf[7];
                buf[g_unichar_to_utf8(c, buf)] = 0;
                g_print("%s U+%04X [%s]\n", cmd_to_str(seq->command),
                        c,
                        g_unichar_isprint(c) ? buf : "�");
        } else {
                g_print("%s", cmd_to_str(seq->command));
                if (seq->n_args) {
                        g_print(" ");
                        for (unsigned int i = 0; i < seq->n_args; i++) {
                                if (i > 0)
                                        g_print(";");
                                g_print("%d", vte_seq_arg_value(seq->args[i]));
                        }
                }
                g_print("\n");
        }
}
#endif

class vte_seq_builder {
public:
        vte_seq_builder(unsigned int type,
                        uint32_t f) {
                memset(&m_seq, 0, sizeof(m_seq));
                m_seq.type = type;
                set_final(f);
        }

        vte_seq_builder(unsigned int type,
                        std::u32string const& str)
                : m_arg_str(str)
        {
                memset(&m_seq, 0, sizeof(m_seq));
                m_seq.type = type;
                set_final(0);
        }

        ~vte_seq_builder() = default;

        void set_final(uint32_t raw) { m_seq.terminator = raw; }
        void set_intermediates(uint32_t* i,
                               unsigned int ni)
        {
                unsigned int flags = 0;
                for (unsigned int n = 0; n < ni; n++) {
                        flags |= (1u << (i[n] - 0x20));
                        m_i[n] = i[n];
                }
                m_ni = ni;
                m_seq.intermediates = flags;
        }

        void set_params(vte_seq_arg_t params[16])
        {
                for (unsigned int i = 0; i < 16; i++)
                        m_seq.args[i] = vte_seq_arg_init(std::min(params[i], 0xffff));
        }

        void set_n_params(unsigned int n)
        {
                m_seq.n_args = n;
        }

        void set_param_byte(uint32_t p)
        {
                m_p = p;
                if (p != 0) {
                        m_seq.intermediates |= (1u << (p - 0x20));
                }
        }

        void set_string(std::u32string const& str)
        {
                m_arg_str = str;
        }

        enum VariantST {
                ST_NONE,
                ST_DEFAULT,
                ST_C0,
                ST_C1,
                ST_BEL
        };

        void to_string(std::u32string& s,
                       bool c1 = false,
                       ssize_t max_arg_str_len = -1,
                       VariantST st = ST_DEFAULT);

        void assert_equal(struct vte_seq* seq);
        void assert_equal_full(struct vte_seq* seq);

        void print(bool c1 = false);

private:
        uint32_t m_i[4]{0, 0, 0, 0};
        uint32_t m_p;
        unsigned int m_ni{0};
        std::u32string m_arg_str;
        struct vte_seq m_seq;
};

void
vte_seq_builder::to_string(std::u32string& s,
                           bool c1,
                           ssize_t max_arg_str_len,
                           VariantST st)
{
        /* Introducer */
        if (c1) {
                switch (m_seq.type) {
                case VTE_SEQ_ESCAPE: s.push_back(0x1B); break; // ESC
                case VTE_SEQ_CSI:    s.push_back(0x9B); break; // CSI
                case VTE_SEQ_DCS:    s.push_back(0x90); break; // DCS
                case VTE_SEQ_OSC:    s.push_back(0x9D); break; // OSC
                case VTE_SEQ_SCI:    s.push_back(0x9A); break; // SCI
                default: return;
                }
        } else {
                s.push_back(0x1B); // ESC
                switch (m_seq.type) {
                case VTE_SEQ_ESCAPE:                    break; // nothing more
                case VTE_SEQ_CSI:    s.push_back(0x5B); break; // [
                case VTE_SEQ_DCS:    s.push_back(0x50); break; // P
                case VTE_SEQ_OSC:    s.push_back(0x5D); break; // ]
                case VTE_SEQ_SCI:    s.push_back(0x5A); break; // Z
                default: return;
                }
        }

        /* Parameters */
        switch (m_seq.type) {
        case VTE_SEQ_CSI:
        case VTE_SEQ_DCS: {

                if (m_p != 0)
                        s.push_back(m_p);
                auto n_args = m_seq.n_args;
                for (unsigned int n = 0; n < n_args; n++) {
                        auto arg = vte_seq_arg_value(m_seq.args[n]);
                        if (n > 0)
                                s.push_back(0x3B); // semicolon
                        if (arg >= 0) {
                                char buf[16];
                                int l = g_snprintf(buf, sizeof(buf), "%d", arg);
                                for (int j = 0; j < l; j++)
                                        s.push_back(buf[j]);
                        }
                }
                break;
        }
        default:
                break;
        }

        /* Intermediates and Final */
        switch (m_seq.type) {
        case VTE_SEQ_ESCAPE:
        case VTE_SEQ_CSI:
        case VTE_SEQ_DCS:
                for (unsigned int n = 0; n < m_ni; n++)
                        s.push_back(m_i[n]);
                /* [[fallthrough]]; */
        case VTE_SEQ_SCI:
                s.push_back(m_seq.terminator);
                break;
        default:
                break;
        }

        /* String and ST */
        switch (m_seq.type) {
        case VTE_SEQ_DCS:
        case VTE_SEQ_OSC:

                if (max_arg_str_len < 0)
                        s.append(m_arg_str, 0, max_arg_str_len);
                else
                        s.append(m_arg_str);

                switch (st) {
                case ST_NONE:
                        // omit ST
                        break;
                case ST_DEFAULT:
                        if (c1) {
                                s.push_back(0x9C); // ST
                        } else {
                                s.push_back(0x1B); // ESC
                                s.push_back(0x5C); // BACKSLASH
                        }
                        break;
                case ST_C0:
                        s.push_back(0x1B); // ESC
                        s.push_back(0x5C); // BACKSLASH
                        break;
                case ST_C1:
                        s.push_back(0x9C); // ST
                        break;
                case ST_BEL:
                        s.push_back(0x7); // BEL
                        break;
                default:
                        break;
                }
        }
}

void
vte_seq_builder::print(bool c1)
{
        std::u32string s;
        to_string(s, c1);
        print_escaped(s);
}

void
vte_seq_builder::assert_equal(struct vte_seq* seq)
{
        g_assert_cmpuint(m_seq.type, ==, seq->type);
        g_assert_cmphex(m_seq.terminator, ==, seq->terminator);
}

void
vte_seq_builder::assert_equal_full(struct vte_seq* seq)
{
        assert_equal(seq);
        /* We may get one arg less back, if it's at default */
        if (m_seq.n_args != seq->n_args) {
                g_assert_cmpuint(m_seq.n_args, ==, seq->n_args + 1);
                g_assert_cmpint(vte_seq_arg_value(m_seq.args[m_seq.n_args - 1]), ==, -1);
        }
        for (unsigned int n = 0; n < seq->n_args; n++)
                g_assert_cmpint(vte_seq_arg_value(m_seq.args[n]),
                                ==,
                                vte_seq_arg_value(seq->args[n]));
}

static int
feed_parser(std::u32string const& s,
            struct vte_seq** seq)
{
        int rv = VTE_SEQ_NONE;
        for (auto it : s) {
                rv = vte_parser_feed(parser, seq, (uint32_t)(char32_t)it);
                if (rv < 0)
                        break;
        }
        return rv;
}

static int
feed_parser(vte_seq_builder& b,
            struct vte_seq** seq,
            bool c1 = false)
{
        std::u32string s;
        b.to_string(s, c1);

        return feed_parser(s, seq);
}

static void
test_seq_arg(void)
{
        /* Basic test */
        vte_seq_arg_t arg = VTE_SEQ_ARG_INIT_DEFAULT;
        g_assert_false(vte_seq_arg_started(arg));
        g_assert_true(vte_seq_arg_default(arg));

        vte_seq_arg_push(&arg, '1');
        vte_seq_arg_push(&arg, '2');
        vte_seq_arg_push(&arg, '3');
        vte_seq_arg_finish(&arg);

        g_assert_cmpint(vte_seq_arg_value(arg), ==, 123);
        g_assert_false(vte_seq_arg_default(arg));

        /* Test max value */
        arg = VTE_SEQ_ARG_INIT_DEFAULT;
        vte_seq_arg_push(&arg, '6');
        vte_seq_arg_push(&arg, '5');
        vte_seq_arg_push(&arg, '5');
        vte_seq_arg_push(&arg, '3');
        vte_seq_arg_push(&arg, '6');
        vte_seq_arg_finish(&arg);

        g_assert_cmpint(vte_seq_arg_value(arg), ==, 65535);
}

static void
test_seq_string(void)
{
        vte_seq_string_t str;
        vte_seq_string_init(&str);

        size_t len;
        auto buf = vte_seq_string_get(&str, &len);
        g_assert_cmpuint(len, ==, 0);

        for (unsigned int i = 0; i < VTE_SEQ_STRING_MAX_CAPACITY; ++i) {
                auto rv = vte_seq_string_push(&str, 0xfffdU);
                g_assert_true(rv);

                buf = vte_seq_string_get(&str, &len);
                g_assert_cmpuint(len, ==, i + 1);
        }

        /* Try one more */
        auto rv = vte_seq_string_push(&str, 0xfffdU);
        g_assert_false(rv);

        buf = vte_seq_string_get(&str, &len);
        for (unsigned int i = 0; i < len; i++)
                g_assert_cmpuint(buf[i], ==, 0xfffdU);

        vte_seq_string_reset(&str);
        buf = vte_seq_string_get(&str, &len);
        g_assert_cmpuint(len, ==, 0);

        vte_seq_string_free(&str);
}

static void
test_seq_control(void)
{
        static struct {
                uint32_t c;
                unsigned int type;
                unsigned int cmd;
        } const controls [] = {
                { 0x0,  VTE_SEQ_CONTROL, VTE_CMD_NUL     },
                { 0x1,  VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x2,  VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x3,  VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x4,  VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x5,  VTE_SEQ_CONTROL, VTE_CMD_ENQ     },
                { 0x6,  VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x7,  VTE_SEQ_CONTROL, VTE_CMD_BEL     },
                { 0x8,  VTE_SEQ_CONTROL, VTE_CMD_BS      },
                { 0x9,  VTE_SEQ_CONTROL, VTE_CMD_HT      },
                { 0xa,  VTE_SEQ_CONTROL, VTE_CMD_LF      },
                { 0xb,  VTE_SEQ_CONTROL, VTE_CMD_VT      },
                { 0xc,  VTE_SEQ_CONTROL, VTE_CMD_FF      },
                { 0xd,  VTE_SEQ_CONTROL, VTE_CMD_CR      },
                { 0xe,  VTE_SEQ_CONTROL, VTE_CMD_SO      },
                { 0xf,  VTE_SEQ_CONTROL, VTE_CMD_SI      },
                { 0x10, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x11, VTE_SEQ_CONTROL, VTE_CMD_DC1     },
                { 0x12, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x13, VTE_SEQ_CONTROL, VTE_CMD_DC3     },
                { 0x14, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x15, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x16, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x17, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x18, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
                { 0x19, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x1a, VTE_SEQ_CONTROL, VTE_CMD_SUB     },
                { 0x1b, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
                { 0x1c, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x1d, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x1e, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x1f, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x7f, VTE_SEQ_GRAPHIC, VTE_CMD_GRAPHIC }, // FIXMEchpe
                { 0x80, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x81, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x82, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x83, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x84, VTE_SEQ_CONTROL, VTE_CMD_IND     },
                { 0x85, VTE_SEQ_CONTROL, VTE_CMD_NEL     },
                { 0x86, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x87, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x88, VTE_SEQ_CONTROL, VTE_CMD_HTS     },
                { 0x89, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x8a, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x8b, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x8c, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x8d, VTE_SEQ_CONTROL, VTE_CMD_RI      },
                { 0x8e, VTE_SEQ_CONTROL, VTE_CMD_SS2     },
                { 0x8f, VTE_SEQ_CONTROL, VTE_CMD_SS3     },
                { 0x90, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
                { 0x91, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x92, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x93, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x94, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x95, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x96, VTE_SEQ_CONTROL, VTE_CMD_SPA     },
                { 0x97, VTE_SEQ_CONTROL, VTE_CMD_EPA     },
                { 0x98, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
                { 0x99, VTE_SEQ_CONTROL, VTE_CMD_NONE    },
                { 0x9a, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
                { 0x9b, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
                { 0x9c, VTE_SEQ_CONTROL, VTE_CMD_ST      },
                { 0x9d, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
                { 0x9e, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
                { 0x9f, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
        };

        for (unsigned int i = 0; i < G_N_ELEMENTS(controls); i++) {
                vte_parser_reset(parser);
                struct vte_seq* seq;
                auto rv = vte_parser_feed(parser, &seq, controls[i].c);
                g_assert_cmpuint(rv, >=, 0);
                g_assert_cmpuint(controls[i].type, ==, seq->type);
                g_assert_cmpuint(controls[i].cmd, ==, seq->command);
        }
}

static void
test_seq_esc_invalid(void)
{
        /* Tests invalid ESC 0/n and ESC 1/n sequences, which should never result in
         * an VTE_SEQ_ESCAPE type sequence, but instead always in the C0 control.
         */
        for (uint32_t f = 0x0; f < 0x20; f++) {
                vte_parser_reset(parser);

                vte_seq_builder b{VTE_SEQ_ESCAPE, f};
                struct vte_seq* seq;
                auto rv = feed_parser(b, &seq);
                g_assert_cmpint(rv, !=, VTE_SEQ_ESCAPE);
        }
}

static void
test_seq_esc(uint32_t f,
             uint32_t i[],
             unsigned int ni)
{
        vte_seq_builder b{VTE_SEQ_ESCAPE, f};
        b.set_intermediates(i, ni);

        vte_parser_reset(parser);
        struct vte_seq* seq;
        auto rv = feed_parser(b, &seq);
        if (rv == VTE_SEQ_ESCAPE)
                b.assert_equal(seq);
}

static void
test_seq_esc_nF(void)
{
        /* Tests nF sequences, that is ESC 2/n [2/m..] F with F being 3/0..7/14.
         * They could have any number of itermediates, but we only test up to 4.
         */

        uint32_t i[4];
        for (uint32_t f = 0x30; f < 0x7f; f++) {
                test_seq_esc(f, i, 0);
                for (i[0] = 0x20; i[0] < 0x30; i[0]++) {
                        test_seq_esc(f, i, 1);
                        for (i[1] = 0x20; i[1] < 0x30; i[1]++) {
                                test_seq_esc(f, i, 2);
                                for (i[2] = 0x20; i[2] < 0x30; i[2]++) {
                                        test_seq_esc(f, i, 3);
                                        for (i[3] = 0x20; i[3] < 0x30; i[3]++) {
                                                test_seq_esc(f, i, 4);
                                        }
                                }
                        }
                }
        }
}

static void
test_seq_esc_charset(uint32_t f, /* final */
                     uint32_t i[], /* intermediates */
                     unsigned int ni, /* number of intermediates */
                     unsigned int cmd, /* expected command */
                     unsigned int cs /* expected charset */)
{
        vte_seq_builder b{VTE_SEQ_ESCAPE, f};
        b.set_intermediates(i, ni);

        vte_parser_reset(parser);
        struct vte_seq* seq;
        auto rv = feed_parser(b, &seq);
        g_assert_cmpint(rv, ==, VTE_SEQ_ESCAPE);
        b.assert_equal(seq);

        g_assert_cmpint(seq->command, ==, cmd);
        g_assert_cmpint(seq->charset, ==, cs);
}

static void
test_seq_esc_charset(uint32_t i[], /* intermediates */
                     unsigned int ni, /* number of intermediates */
                     uint8_t const* const table, /* table */
                     unsigned int ntable, /* number of table entries */
                     uint32_t ts, /* start of table */
                     unsigned int cmd, /* expected command */
                     unsigned int defaultcs /* default charset */)
{
        for (uint32_t f = 0x30; f < 0x7f; f++) {
                int cs;

                if (f >= ts && f < (ts + ntable))
                        cs = table[f - ts];
                else
                        cs = defaultcs;

                test_seq_esc_charset(f, i, ni, cmd, cs);
        }
}

static void
test_seq_esc_charset_94(void)
{
        uint32_t i[4];

        /* Single byte 94-sets */
        for (i[0] = 0x28; i[0] <= 0x2b; i[0]++) {
                test_seq_esc_charset(i, 1,
                                     charset_graphic_94,
                                     G_N_ELEMENTS(charset_graphic_94),
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE);

                i[1] = 0x20;
                test_seq_esc_charset(i, 2, nullptr, 0, 0,
                                     VTE_CMD_GnDm, VTE_CHARSET_DRCS);

                i[1] = 0x21;
                test_seq_esc_charset(i, 2,
                                     charset_graphic_94_with_2_1,
                                     G_N_ELEMENTS(charset_graphic_94_with_2_1),
                                     0x40, VTE_CMD_GnDm, VTE_CHARSET_NONE);

                i[1] = 0x22;
                test_seq_esc_charset(i, 2,
                                     charset_graphic_94_with_2_2,
                                     G_N_ELEMENTS(charset_graphic_94_with_2_2),
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE);

                i[1] = 0x23;
                test_seq_esc_charset(i, 2, nullptr, 0,
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE);

                /* 2/4 is multibyte charsets */

                i[1] = 0x25;
                test_seq_esc_charset(i, 2,
                                     charset_graphic_94_with_2_5,
                                     G_N_ELEMENTS(charset_graphic_94_with_2_5),
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE);

                i[1] = 0x26;
                test_seq_esc_charset(i, 2,
                                     charset_graphic_94_with_2_6,
                                     G_N_ELEMENTS(charset_graphic_94_with_2_6),
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE);

                i[1] = 0x27;
                test_seq_esc_charset(i, 2, nullptr, 0, 0,
                                     VTE_CMD_GnDm, VTE_CHARSET_NONE);
        }
}

static void
test_seq_esc_charset_96(void)
{
        uint32_t i[4];

        /* Single byte 96-sets */
        for (i[0] = 0x2d; i[0] <= 0x2f; i[0]++) {
                test_seq_esc_charset(i, 1,
                                     charset_graphic_96,
                                     G_N_ELEMENTS(charset_graphic_96),
                                     0x30, VTE_CMD_GnDm, VTE_CHARSET_NONE);

                i[1] = 0x20;
                test_seq_esc_charset(i, 2, nullptr, 0, 0,
                                     VTE_CMD_GnDm, VTE_CHARSET_DRCS);

                /* 2/4 is multibyte charsets, 2/5 is DOCS. Other indermediates may be present
                 * in Fp sequences, but none are actually in use.
                 */
                for (i[1] = 0x21; i[1] < 0x28; i[1]++) {
                        if (i[1] == 0x24 || i[1] == 0x25)
                                continue;

                        test_seq_esc_charset(i, 2, nullptr, 0, 0,
                                             VTE_CMD_GnDm, VTE_CHARSET_NONE);
                }
        }
}

static void
test_seq_esc_charset_94_n(void)
{
        uint32_t i[4];

        /* Multibyte 94-sets */
        i[0] = 0x24;
        for (i[1] = 0x28; i[1] <= 0x2b; i[1]++) {
                test_seq_esc_charset(i, 2,
                                     charset_graphic_94_n,
                                     G_N_ELEMENTS(charset_graphic_94_n),
                                     0x30, VTE_CMD_GnDMm, VTE_CHARSET_NONE);

                i[2] = 0x20;
                test_seq_esc_charset(i, 3, nullptr, 0, 0,
                                     VTE_CMD_GnDMm, VTE_CHARSET_DRCS);

                /* There could be one more intermediate byte. */
                for (i[2] = 0x21; i[2] < 0x28; i[2]++) {
                        if (i[2] == 0x24) /* TODO */
                                continue;

                        test_seq_esc_charset(i, 3, nullptr, 0, 0,
                                             VTE_CMD_GnDMm, VTE_CHARSET_NONE);
                }
        }

        /* As a special exception, ESC 2/4 4/[012] are also possible */
        test_seq_esc_charset(0x40, i, 1, VTE_CMD_GnDMm, charset_graphic_94_n[0x40 - 0x30]);
        test_seq_esc_charset(0x41, i, 1, VTE_CMD_GnDMm, charset_graphic_94_n[0x41 - 0x30]);
        test_seq_esc_charset(0x42, i, 1, VTE_CMD_GnDMm, charset_graphic_94_n[0x42 - 0x30]);
}

static void
test_seq_esc_charset_96_n(void)
{
        uint32_t i[4];

        /* Multibyte 94-sets */
        i[0] = 0x24;
        for (i[1] = 0x2d; i[1] <= 0x2f; i[1]++) {
                test_seq_esc_charset(i, 2, nullptr, 0, 0,
                                     VTE_CMD_GnDMm, VTE_CHARSET_NONE);

                i[2] = 0x20;
                test_seq_esc_charset(i, 3, nullptr, 0, 0,
                                     VTE_CMD_GnDMm, VTE_CHARSET_DRCS);

                /* There could be one more intermediate byte. */
                for (i[2] = 0x21; i[2] < 0x28; i[2]++) {
                        test_seq_esc_charset(i, 3, nullptr, 0, 0,
                                             VTE_CMD_GnDMm, VTE_CHARSET_NONE);
                }
        }
}

static void
test_seq_esc_charset_control(void)
{
        uint32_t i[4];

        /* C0 controls: ESC 2/1 F */
        i[0] = 0x21;
        test_seq_esc_charset(i, 1,
                             charset_control_c0,
                             G_N_ELEMENTS(charset_control_c0),
                             0x40, VTE_CMD_CnD, VTE_CHARSET_NONE);

        /* C1 controls: ESC 2/2 F */
        i[0] = 0x22;
        test_seq_esc_charset(i, 1,
                             charset_control_c1,
                             G_N_ELEMENTS(charset_control_c1),
                             0x40, VTE_CMD_CnD, VTE_CHARSET_NONE);
}

static void
test_seq_esc_charset_other(void)
{
        uint32_t i[4];

        /* Other coding systems: ESC 2/5 F or ESC 2/5 2/15 F */
        i[0] = 0x25;
        test_seq_esc_charset(i, 1,
                             charset_ocs_with_return,
                             G_N_ELEMENTS(charset_ocs_with_return),
                             0x40, VTE_CMD_DOCS, VTE_CHARSET_NONE);

        i[1] = 0x2f;
        test_seq_esc_charset(i, 2,
                             charset_ocs_without_return,
                             G_N_ELEMENTS(charset_ocs_without_return),
                             0x40, VTE_CMD_DOCS, VTE_CHARSET_NONE);
}

static void
test_seq_esc_Fpes(void)
{
        /* Tests Fp, Fe and Ft sequences, that is ESC 3/n .. ESC 7/14 */

        for (uint32_t f = 0x30; f < 0x7f; f++) {
                vte_parser_reset(parser);

                vte_seq_builder b{VTE_SEQ_ESCAPE, f};

                struct vte_seq* seq;
                auto rv = feed_parser(b, &seq);
                int expected_rv;
                switch (f) {
                case 'P': /* DCS */
                case 'X': /* SOS */
                case 'Z': /* SCI */
                case '_': /* APC */
                case '[': /* CSI */
                case ']': /* OSC */
                case '^': /* PM */
                        expected_rv = VTE_SEQ_NONE;
                        break;
                default:
                        expected_rv = VTE_SEQ_ESCAPE;
                        break;
                }
                g_assert_cmpint(rv, ==, expected_rv);
                if (rv != VTE_SEQ_NONE)
                        b.assert_equal(seq);
        }
}

static void
test_seq_csi(uint32_t f,
             uint32_t p,
             vte_seq_arg_t params[16],
             uint32_t i[4],
             unsigned int ni)
{
        vte_seq_builder b{VTE_SEQ_CSI, f};
        b.set_intermediates(i, ni);
        b.set_param_byte(p);
        b.set_params(params);

        int expected_rv = (f & 0xF0) == 0x30 ? VTE_SEQ_NONE : VTE_SEQ_CSI;

        for (unsigned int n = 0; n <= 16; n++) {
                b.set_n_params(n);

                vte_parser_reset(parser);
                struct vte_seq* seq;
                /* First with C0 CSI */
                auto rv = feed_parser(b, &seq, false);
                g_assert_cmpint(rv, ==, expected_rv);
                if (rv != VTE_SEQ_NONE)
                        b.assert_equal_full(seq);

                /* Now with C1 CSI */
                rv = feed_parser(b, &seq, true);
                if (rv != VTE_SEQ_NONE)
                        b.assert_equal_full(seq);
        }
}

static void
test_seq_csi(uint32_t p,
             vte_seq_arg_t params[16])
{
        uint32_t i[4];
        for (uint32_t f = 0x30; f < 0x7f; f++) {
                test_seq_csi(f, p, params, i, 0);
                for (i[0] = 0x20; i[0] < 0x30; i[0]++) {
                        test_seq_csi(f, p, params, i, 1);
                        for (i[1] = 0x20; i[1] < 0x30; i[1]++) {
                                test_seq_csi(f, p, params, i, 2);
                        }
                }
        }
}

static void
test_seq_csi(vte_seq_arg_t params[16])
{
        test_seq_csi(0, params);
        for (uint32_t p = 0x3c; p <= 0x3f; p++)
                test_seq_csi(p, params);
}

static void
test_seq_csi(void)
{
        /* Tests CSI sequences, that is sequences of the form
         * CSI P...P I...I F
         * with parameter bytes P from 3/0..3/15, intermediate bytes I from 2/0..2/15 and
         * final byte F from 4/0..7/14.
         * There could be any number of intermediate bytes, but we only test up to 2.
         * There could be any number of extra params bytes, but we only test up to 1.
         * CSI can be either the C1 control itself, or ESC [
         */
        vte_seq_arg_t params1[16]{ -1, 0, 1, 9, 10, 99, 100, 999,
                        1000, 9999, 10000, 65534, 65535, 65536, -1, -1 };
        test_seq_csi(params1);

        vte_seq_arg_t params2[16]{ 1, -1, -1, -1, 1, -1, 1, 1,
                        1, -1, -1, -1, -1, 1, 1, 1 };
        test_seq_csi(params2);
}

static void
test_seq_sci(uint32_t f,
             bool valid)
{
        vte_seq_builder b{VTE_SEQ_SCI, f};

        struct vte_seq* seq;
        /* First with C0 SCI */
        auto rv = feed_parser(b, &seq, false);
        if (valid) {
                g_assert_cmpint(rv, ==, VTE_SEQ_SCI);
                b.assert_equal_full(seq);
        } else
                g_assert_cmpint(rv, !=, VTE_SEQ_SCI);

        /* Now with C1 SCI */
        rv = feed_parser(b, &seq, true);
        if (valid) {
                g_assert_cmpint(rv, ==, VTE_SEQ_SCI);
                b.assert_equal_full(seq);
        } else
                g_assert_cmpint(rv, !=, VTE_SEQ_SCI);
}

static void
test_seq_sci(void)
{
        /* Tests SCI sequences, that is sequences of the form SCI F
         * with final byte 0/8..0/13 or 2/0..7/14
         * SCI can be either the C1 control itself, or ESC Z
         */
        vte_parser_reset(parser);

        for (uint32_t f = 0x8; f <= 0xd; ++f)
                test_seq_sci(f, true);
        for (uint32_t f = 0x20; f <= 0x7e; ++f)
                test_seq_sci(f, true);
        for (uint32_t f = 0x7f; f <= 0xff; ++f)
                test_seq_sci(f, false);
}

static void
test_seq_dcs(uint32_t f,
             uint32_t p,
             vte_seq_arg_t params[16],
             uint32_t i[4],
             unsigned int ni,
             std::u32string const& str)
{
        vte_seq_builder b{VTE_SEQ_DCS, f};
        b.set_intermediates(i, ni);
        b.set_param_byte(p);
        b.set_params(params);
        b.set_string(str);

        int expected_rv0 = (f & 0xF0) == 0x30 ? VTE_SEQ_ESCAPE /* the C0 ST */ : VTE_SEQ_DCS;
        int expected_rv1 = (f & 0xF0) == 0x30 ? VTE_SEQ_NONE : VTE_SEQ_DCS;

        for (unsigned int n = 0; n <= 16; n++) {
                b.set_n_params(n);

                vte_parser_reset(parser);
                struct vte_seq* seq;

                /* First with C0 DCS */
                auto rv0 = feed_parser(b, &seq, false);
                g_assert_cmpint(rv0, ==, expected_rv0);
                if (rv0 != VTE_SEQ_ESCAPE)
                        b.assert_equal_full(seq);
                if (rv0 == VTE_SEQ_ESCAPE)
                        g_assert_cmpint(seq->command, ==, VTE_CMD_ST);

                /* Now with C1 DCS */
                auto rv1 = feed_parser(b, &seq, true);
                g_assert_cmpint(rv1, ==, expected_rv1);
                if (rv1 != VTE_SEQ_NONE)
                        b.assert_equal_full(seq);
        }
}

static void
test_seq_dcs(uint32_t p,
             vte_seq_arg_t params[16],
             std::u32string const& str)
{
        uint32_t i[4];
        for (uint32_t f = 0x30; f < 0x7f; f++) {
                for (i[0] = 0x20; i[0] < 0x30; i[0]++) {
                        test_seq_dcs(f, p, params, i, 1, str);
                        for (i[1] = 0x20; i[1] < 0x30; i[1]++) {
                                test_seq_dcs(f, p, params, i, 2, str);
                        }
                }
        }
}

static void
test_seq_dcs(vte_seq_arg_t params[16],
             std::u32string const& str)
{
        test_seq_dcs(0, params, str);
        for (uint32_t p = 0x3c; p <= 0x3f; p++)
                test_seq_dcs(p, params, str);
}

static void
test_seq_dcs(std::u32string const& str)
{
        /* Tests DCS sequences, that is sequences of the form
         * DCS P...P I...I F D...D ST
         * with parameter bytes P from 3/0..3/15, intermediate bytes I from 2/0..2/15 and
         * final byte F from 4/0..7/14.
         * There could be any number of intermediate bytes, but we only test up to 2.
         * There could be any number of extra params bytes, but we only test up to 1.
         * DCS can be either the C1 control itself, or ESC [; ST can be either the C1
         * control itself, or ESC \\
         */
        vte_seq_arg_t params1[16]{ -1, 0, 1, 9, 10, 99, 100, 999,
                        1000, 9999, 10000, 65534, 65535, 65536, -1, -1 };
        test_seq_dcs(params1, str);

        vte_seq_arg_t params2[16]{ 1, -1, -1, -1, 1, -1, 1, 1,
                        1, -1, -1, -1, -1, 1, 1, 1 };
        test_seq_dcs(params2, str);
}

static void
test_seq_dcs(void)
{
        test_seq_dcs(U""s);
        test_seq_dcs(U"123;TESTING"s);
}

static void
test_seq_parse(char const* str,
               struct vte_seq** seq)
{
        std::u32string s;
        s.push_back(0x9B); /* CSI */
        for (unsigned int i = 0; str[i]; i++)
                s.push_back(str[i]);
        s.push_back(0x6d); /* m = SGR */

        vte_parser_reset(parser);
        auto rv = feed_parser(s, seq);
        g_assert_cmpint(rv, ==, VTE_SEQ_CSI);
}

static void
test_seq_csi_param(char const* str,
                   std::vector<int> args,
                   std::vector<bool> args_nonfinal)
{
        g_assert_cmpuint(args.size(), ==, args_nonfinal.size());

        struct vte_seq* seq;
        test_seq_parse(str, &seq);

        if (seq->n_args < VTE_PARSER_ARG_MAX)
                g_assert_cmpuint(seq->n_args, ==, args.size());

        unsigned int n_final_args = 0;
        for (unsigned int i = 0; i < seq->n_args; i++) {
                g_assert_cmpint(vte_seq_arg_value(seq->args[i]), ==, args[i]);

                auto is_nonfinal = args_nonfinal[i];
                if (!is_nonfinal)
                        n_final_args++;

                g_assert_cmpint(!!vte_seq_arg_nonfinal(seq->args[i]), ==, is_nonfinal);
        }

        g_assert_cmpuint(seq->n_final_args, ==, n_final_args);
}

static void
test_seq_csi_param(void)
{
        /* Tests that CSI parameters and subparameters are parsed correctly. */

        test_seq_csi_param("", { }, { });
        test_seq_csi_param(";", { -1, -1 }, { false, false });
        test_seq_csi_param(":", { -1, -1 }, { true, false });
        test_seq_csi_param(";:", { -1, -1, -1 }, { false, true, false });
        test_seq_csi_param("::;;", { -1, -1, -1, -1, -1 }, { true, true, false, false, false });

        test_seq_csi_param("1;2:3:4:5:6;7:8;9:0",
                           { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0 },
                           { false, true, true, true, true, false, true, false, true, false });

        test_seq_csi_param("1;1;1;1;1;1;1;1;1;1;1;1;1;1;1;1",
                           { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
                           { false, false, false, false, false, false, false, false,
                                           false, false, false, false, false, false, false, false });

        test_seq_csi_param("1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1",
                           { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
                           { true, true, true, true, true, true, true, true,
                                           true, true, true, true, true, true, true, false });

}

static void
test_seq_glue_arg(char const* str,
                  unsigned int n_args,
                  unsigned int n_final_args,
                  vte::parser::Sequence& seq)
{
        test_seq_parse(str, seq.seq_ptr());

        auto raw_seq = *seq.seq_ptr();
        g_assert_cmpuint(seq.size(), ==, n_args);
        g_assert_cmpuint(raw_seq->n_args, ==, n_args);
        g_assert_cmpuint(seq.size_final(), ==, n_final_args);
        g_assert_cmpuint(raw_seq->n_final_args, ==, n_final_args);

        g_assert_cmpuint(seq.type(), ==, raw_seq->type);
        g_assert_cmpuint(seq.command(), ==, raw_seq->command);
        g_assert_cmpuint(seq.terminator(), ==, raw_seq->terminator);
        g_assert_cmpuint(seq.intermediates(), ==, raw_seq->intermediates);

        for (unsigned int i = 0; i < raw_seq->n_args; i++)
                g_assert_cmpuint(seq.param(i), ==, vte_seq_arg_value(raw_seq->args[i]));
}

static void
test_seq_glue_arg(void)
{
        vte::parser::Sequence seq{};

        test_seq_glue_arg(":0:1000;2;3;4;:;", 9, 6, seq);
        g_assert_cmpuint(seq.cbegin(), ==, 0);
        g_assert_cmpuint(seq.cend(), ==, 9);

        auto it = seq.cbegin();
        g_assert_cmpuint(it, ==, 0);
        it = seq.next(it);
        g_assert_cmpuint(it, ==,  3);
        it = seq.next(it);
        g_assert_cmpuint(it, ==, 4);
        it = seq.next(it);
        g_assert_cmpuint(it, ==, 5);
        it = seq.next(it);
        g_assert_cmpuint(it, ==, 6);
        it = seq.next(it);
        g_assert_cmpuint(it, ==, 8);
        it = seq.next(it);
        g_assert_cmpuint(it, ==, 9);

        it = seq.cbegin();
        g_assert_cmpint(seq.param(it++), ==, -1);
        g_assert_cmpint(seq.param(it++), ==, 0);
        g_assert_cmpint(seq.param(it++), ==, 1000);
        g_assert_cmpint(seq.param(it++), ==, 2);
        g_assert_cmpint(seq.param(it++), ==, 3);
        g_assert_cmpint(seq.param(it++), ==, 4);
        g_assert_cmpint(seq.param(it++), ==, -1);
        g_assert_cmpint(seq.param(it++), ==, -1);
        g_assert_cmpint(seq.param(it++), ==, -1);
        g_assert_cmpint(it, ==, seq.cend());

        it = seq.cbegin();
        g_assert_cmpint(seq.param(it, -2), ==, -2);
        g_assert_cmpint(seq.param(it, -2, 0, 100), ==, 0);
        it++; it++;
        g_assert_cmpint(seq.param(it, -2), ==, seq.param(it));
        g_assert_cmpint(seq.param(it, -2, 20, 100), ==, 100);
        g_assert_cmpint(seq.param(it, -2, 200, 2000), ==, 1000);
        g_assert_cmpint(seq.param(it, -2, 2000, 4000), ==, 2000);

        int a, b, c,d ;
        it = seq.cbegin();
        g_assert_false(seq.collect(it, {&a, &b, &c}));
        g_assert_true(seq.collect_subparams(it, {&a}));
        g_assert_true(seq.collect_subparams(it, {&a, &b}));
        g_assert_true(seq.collect_subparams(it, {&a, &b, &c}));
        g_assert_cmpint(a, ==, -1);
        g_assert_cmpint(b, ==, 0);
        g_assert_cmpint(c, ==, 1000);
        g_assert_false(seq.collect_subparams(it, {&a, &b, &c, &d}));

        it = seq.next(it);
        g_assert_true(seq.collect(it, {&a}));
        g_assert_true(seq.collect(it, {&a, &b}));
        g_assert_true(seq.collect(it, {&a, &b, &c}));
        g_assert_cmpint(a, ==, 2);
        g_assert_cmpint(b, ==, 3);
        g_assert_cmpint(c, ==, 4);
        g_assert_false(seq.collect(it, {&a, &b, &c, &d}));

        it = seq.next(it);
        it = seq.next(it);
        it = seq.next(it);
        g_assert_false(seq.collect(it, {&a}));
        g_assert_true(seq.collect_subparams(it, {&a}));
        g_assert_true(seq.collect_subparams(it, {&a, &b}));
        g_assert_cmpint(a, ==, -1);
        g_assert_cmpint(b, ==, -1);
        g_assert_false(seq.collect_subparams(it, {&a, &b, &c}));
        it = seq.next(it);
        g_assert_true(seq.collect(it, {&a}));
        g_assert_cmpint(a, ==, -1);
        g_assert_true(seq.collect(it, {&a, &b})); /* past-the-end params are final and default */
        g_assert_cmpint(a, ==, -1);
        g_assert_cmpint(b, ==, -1);
        g_assert_true(seq.collect(it, {&a, &b, &c})); /* past-the-end params are final and default */
        g_assert_cmpint(a, ==, -1);
        g_assert_cmpint(b, ==, -1);
        g_assert_cmpint(c, ==, -1);

        it = seq.cbegin();
        g_assert_cmpint(seq.collect1(it, -2), ==, -2);
        it = seq.next(it);
        g_assert_cmpint(seq.collect1(it), ==, 2);
        g_assert_cmpint(seq.collect1(it), ==, 2);
        it = seq.next(it);
        g_assert_cmpint(seq.collect1(it), ==, 3);
        it = seq.next(it);
        g_assert_cmpint(seq.collect1(it), ==, 4);
        it = seq.next(it);
        g_assert_cmpint(seq.collect1(it, -3), ==, -3);
        it = seq.next(it);
        g_assert_cmpint(seq.collect1(it), ==, -1);
        g_assert_cmpint(seq.collect1(it, 42), ==, 42);
        g_assert_cmpint(seq.collect1(it, -1, 0, 100), ==, 0);
        g_assert_cmpint(seq.collect1(it, 42, 0, 100), ==, 42);
        g_assert_cmpint(seq.collect1(it, 42, 0, 10), ==, 10);
        g_assert_cmpint(seq.collect1(it, 42, 100, 200), ==, 100);
}

static int
feed_parser_st(vte_seq_builder& b,
               struct vte_seq** seq,
               bool c1 = false,
               ssize_t max_arg_str_len = -1,
               vte_seq_builder::VariantST st = vte_seq_builder::ST_DEFAULT)
{
        std::u32string s;
        b.to_string(s, c1, max_arg_str_len, st);

        auto rv = feed_parser(s, seq);
        if (rv == VTE_SEQ_NONE)
                return rv;

        switch (st) {
        case vte_seq_builder::VariantST::ST_NONE:
                g_assert_cmpuint((*seq)->terminator, ==, 0);
                break;
        case vte_seq_builder::VariantST::ST_DEFAULT:
                g_assert_cmpuint((*seq)->terminator, ==, c1 ? 0x9c /* ST */ : 0x5c /* BACKSLASH */);
                break;
        case vte_seq_builder::VariantST::ST_C0:
                g_assert_cmpuint((*seq)->terminator, ==, 0x5c /* BACKSLASH */);
                break;
        case vte_seq_builder::VariantST::ST_C1:
                g_assert_cmpuint((*seq)->terminator, ==, 0x9c /* ST */);
                break;
        case vte_seq_builder::VariantST::ST_BEL:
                g_assert_cmpuint((*seq)->terminator, ==, 0x7 /* BEL */);
                break;
        }

        return rv;
}

static void
test_seq_osc(std::u32string const& str,
             struct vte_seq** seq,
             int expected_rv = VTE_SEQ_OSC,
             bool c1 = true,
             ssize_t max_arg_str_len = -1,
             vte_seq_builder::VariantST st = vte_seq_builder::ST_DEFAULT)
{
        vte_seq_builder b{VTE_SEQ_OSC, str};

        vte_parser_reset(parser);
        auto rv = feed_parser_st(b, seq, c1, max_arg_str_len, st);
        g_assert_cmpint(rv, ==, expected_rv);
        #if 0
        if (rv != VTE_SEQ_NONE)
                b.assert_equal(*seq);
        #endif
}

static void
test_seq_osc(std::u32string const& str,
             vte::parser::Sequence& seq,
             int expected_rv = VTE_SEQ_OSC,
             bool c1 = true,
             ssize_t max_arg_str_len = -1,
             vte_seq_builder::VariantST st = vte_seq_builder::ST_DEFAULT)
{
        test_seq_osc(str, seq.seq_ptr(), expected_rv, c1, max_arg_str_len, st);
        if (expected_rv != VTE_SEQ_OSC)
                return;

        if (max_arg_str_len < 0 || size_t(max_arg_str_len) == str.size())
                g_assert_true(seq.string() == str);
        else
                g_assert_true(seq.string() == str.substr(0, max_arg_str_len));
}

static void
test_seq_osc(void)
{
        vte::parser::Sequence seq{};

        /* Simple */
        test_seq_osc(U""s, seq);
        test_seq_osc(U"TEST"s, seq);

        /* String of any supported length */
        for (unsigned int len = 0; len < VTE_SEQ_STRING_MAX_CAPACITY; ++len)
                test_seq_osc(std::u32string(len, 0x10000+len), seq);

        /* Length exceeded */
        //        test_seq_osc(std::u32string(VTE_SEQ_STRING_MAX_CAPACITY + 1, 0x100000), seq, VTE_SEQ_NONE);

        /* Test all introducer/ST combinations */
        test_seq_osc(U"TEST"s, seq, VTE_SEQ_NONE, false, -1, vte_seq_builder::ST_NONE);
        test_seq_osc(U"TEST"s, seq, VTE_SEQ_NONE, true, -1, vte_seq_builder::ST_NONE);
        test_seq_osc(U"TEST"s, seq, VTE_SEQ_OSC, false, -1, vte_seq_builder::ST_DEFAULT);
        test_seq_osc(U"TEST"s, seq, VTE_SEQ_OSC, true, -1, vte_seq_builder::ST_DEFAULT);
        test_seq_osc(U"TEST"s, seq, VTE_SEQ_OSC, false, -1, vte_seq_builder::ST_C0);
        test_seq_osc(U"TEST"s, seq, VTE_SEQ_OSC, true, -1, vte_seq_builder::ST_C0);
        test_seq_osc(U"TEST"s, seq, VTE_SEQ_OSC, false, -1, vte_seq_builder::ST_C1);
        test_seq_osc(U"TEST"s, seq, VTE_SEQ_OSC, true, -1, vte_seq_builder::ST_C1);
        test_seq_osc(U"TEST"s, seq, VTE_SEQ_OSC, false, -1, vte_seq_builder::ST_BEL);
        test_seq_osc(U"TEST"s, seq, VTE_SEQ_OSC, true, -1, vte_seq_builder::ST_BEL);
}

static void
test_seq_glue_string(void)
{
        vte::parser::Sequence seq{};

        std::u32string str{U"TEST"s};
        test_seq_osc(str, seq);

        g_assert_true(seq.string() == str);
}

static void
test_seq_glue_string_tokeniser(void)
{
        std::string str{"a;1b:17:test::b:;3;5;def;17 a;ghi;"s};

        vte::parser::StringTokeniser tokeniser{str, ';'};

        auto start = tokeniser.cbegin();
        auto end = tokeniser.cend();

        auto pit = start;
        for (auto it : {"a"s, "1b:17:test::b:"s, "3"s, "5"s, "def"s, "17 a"s, "ghi"s, ""s}) {
                g_assert_true(it == *pit);

                /* Use std::find to see if the InputIterator implementation
                 * is complete and correct.
                 */
                auto fit = std::find(start, end, it);
                g_assert_true(fit == pit);

                ++pit;
        }
        g_assert_true(pit == end);

        auto len = str.size();
        size_t pos = 0;
        pit = start;
        for (auto it : {1, 14, 1, 1, 3, 4, 3, 0}) {
                g_assert_cmpuint(it, ==, pit.size());
                g_assert_cmpuint(len, ==, pit.size_remaining());

                g_assert_true(pit.string_remaining() == str.substr(pos, std::string::npos));

                len -= it + 1;
                pos += it + 1;

                ++pit;
        }
        g_assert_cmpuint(len + 1, ==, 0);
        g_assert_cmpuint(pos, ==, str.size() + 1);

        pit = start;
        for (auto it : {-2, -2, 3, 5, -2, -2, -2, -1}) {
                int num;
                bool v = pit.number(num);
                if (it == -2)
                        g_assert_false(v);
                else
                        g_assert_cmpint(it, ==, num);

                ++pit;
        }

        /* Test range for */
        for (auto it : tokeniser)
                ;

        /* Test different separator */
        pit = start;
        ++pit;

        auto substr = *pit;
        vte::parser::StringTokeniser subtokeniser{substr, ':'};

        auto subpit = subtokeniser.cbegin();
        for (auto it : {"1b"s, "17"s, "test"s, ""s, "b"s, ""s}) {
                g_assert_true(it == *subpit);

                ++subpit;
        }
        g_assert_true(subpit == subtokeniser.cend());

        /* Test another string, one that doesn't end with an empty token */
        std::string str2{"abc;defghi"s};
        vte::parser::StringTokeniser tokeniser2{str2, ';'};

        g_assert_cmpint(std::distance(tokeniser2.cbegin(), tokeniser2.cend()), ==, 2);
        auto pit2 = tokeniser2.cbegin();
        g_assert_true(*pit2 == "abc"s);
        ++pit2;
        g_assert_true(*pit2 == "defghi"s);
        ++pit2;
        g_assert_true(pit2 == tokeniser2.cend());

        /* Test another string, one that starts with an empty token */
        std::string str3{";abc"s};
        vte::parser::StringTokeniser tokeniser3{str3, ';'};

        g_assert_cmpint(std::distance(tokeniser3.cbegin(), tokeniser3.cend()), ==, 2);
        auto pit3 = tokeniser3.cbegin();
        g_assert_true(*pit3 == ""s);
        ++pit3;
        g_assert_true(*pit3 == "abc"s);
        ++pit3;
        g_assert_true(pit3 == tokeniser3.cend());

        /* And try an empty string, which should split into one empty token */
        std::string str4{""s};
        vte::parser::StringTokeniser tokeniser4{str4, ';'};

        g_assert_cmpint(std::distance(tokeniser4.cbegin(), tokeniser4.cend()), ==, 1);
        auto pit4 = tokeniser4.cbegin();
        g_assert_true(*pit4 == ""s);
        ++pit4;
        g_assert_true(pit4 == tokeniser4.cend());
}

static void
test_seq_glue_sequence_builder(void)
{
}

static void
test_seq_glue_reply_builder(void)
{
}

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        if (vte_parser_new(&parser) < 0)
                return 1;

        g_test_add_func("/vte/parser/sequences/arg", test_seq_arg);
        g_test_add_func("/vte/parser/sequences/string", test_seq_string);
        g_test_add_func("/vte/parser/sequences/glue/arg", test_seq_glue_arg);
        g_test_add_func("/vte/parser/sequences/glue/string", test_seq_glue_string);
        g_test_add_func("/vte/parser/sequences/glue/string-tokeniser", test_seq_glue_string_tokeniser);
        g_test_add_func("/vte/parser/sequences/glue/sequence-builder", test_seq_glue_sequence_builder);
        g_test_add_func("/vte/parser/sequences/glue/reply-builder", test_seq_glue_reply_builder);
        g_test_add_func("/vte/parser/sequences/control", test_seq_control);
        g_test_add_func("/vte/parser/sequences/escape/invalid", test_seq_esc_invalid);
        g_test_add_func("/vte/parser/sequences/escape/charset/94", test_seq_esc_charset_94);
        g_test_add_func("/vte/parser/sequences/escape/charset/96", test_seq_esc_charset_96);
        g_test_add_func("/vte/parser/sequences/escape/charset/94^n", test_seq_esc_charset_94_n);
        g_test_add_func("/vte/parser/sequences/escape/charset/96^n", test_seq_esc_charset_96_n);
        g_test_add_func("/vte/parser/sequences/escape/charset/control", test_seq_esc_charset_control);
        g_test_add_func("/vte/parser/sequences/escape/charset/other", test_seq_esc_charset_other);
        g_test_add_func("/vte/parser/sequences/escape/nF", test_seq_esc_nF);
        g_test_add_func("/vte/parser/sequences/escape/F[pes]", test_seq_esc_Fpes);
        g_test_add_func("/vte/parser/sequences/csi", test_seq_csi);
        g_test_add_func("/vte/parser/sequences/csi/parameters", test_seq_csi_param);
        g_test_add_func("/vte/parser/sequences/sci", test_seq_sci);
        g_test_add_func("/vte/parser/sequences/dcs", test_seq_dcs);
        g_test_add_func("/vte/parser/sequences/osc", test_seq_osc);

        auto rv = g_test_run();

        parser = vte_parser_free(parser);
        return rv;
}
