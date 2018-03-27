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

#include <glib.h>

#include "parser.hh"

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
                        g_print("%d", seq->args[i]);
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

        void set_params(int params[16])
        {
                memcpy(&m_seq.args, params, 16*sizeof(params[0]));
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

        void to_string(std::u32string& s,
                       bool c1 = false);

        void assert_equal(struct vte_seq* seq);
        void assert_equal_full(struct vte_seq* seq);

        void print(bool c1 = false);

private:
        uint32_t m_i[4]{0, 0, 0, 0};
        uint32_t m_p;
        unsigned int m_ni{0};
        struct vte_seq m_seq;

};

void
vte_seq_builder::to_string(std::u32string& s,
                           bool c1)
{
        switch (m_seq.type) {
        case VTE_SEQ_ESCAPE:
                s.push_back(0x1B); // ESC
                break;
        case VTE_SEQ_CSI: {
                if (c1) {
                        s.push_back(0x9B); // CSI
                } else {
                        s.push_back(0x1B); // ESC
                        s.push_back(0x5B); // [
                }

                if (m_p != 0)
                        s.push_back(m_p);
                auto n_args = m_seq.n_args;
                for (unsigned int n = 0; n < n_args; n++) {
                        auto arg = m_seq.args[n];
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
                return;
        }

        for (unsigned int n = 0; n < m_ni; n++)
                s.push_back(m_i[n]);

        s.push_back(m_seq.terminator);
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
        g_assert_cmpuint(m_seq.terminator, ==, seq->terminator);
}

void
vte_seq_builder::assert_equal_full(struct vte_seq* seq)
{
        assert_equal(seq);
        /* We may get one arg less back, if it's at default */
        if (m_seq.n_args != seq->n_args) {
                g_assert_cmpuint(m_seq.n_args, ==, seq->n_args + 1);
                g_assert_cmpuint(m_seq.args[m_seq.n_args - 1], ==, -1);
        }
        for (unsigned int n = 0; n < seq->n_args; n++)
                g_assert_cmpint(std::min(m_seq.args[n], 0xffff), ==, seq->args[n]);
}

static int
feed_parser(vte_seq_builder& b,
            struct vte_seq** seq,
            bool c1 = false)
{
        std::u32string s;
        b.to_string(s, c1);

        //        print_escaped(s);

        int rv = VTE_SEQ_NONE;
        for (auto it : s) {
                rv = vte_parser_feed(parser, seq, (uint32_t)(char32_t)it);
                if (rv < 0)
                        break;
        }
        return rv;
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
                { 0x9a, VTE_SEQ_CONTROL, VTE_CMD_DECID   },
                { 0x9b, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
                { 0x9c, VTE_SEQ_IGNORE,  VTE_CMD_NONE    },
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
             int params[16],
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
             int params[16])
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
test_seq_csi(int params[16])
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
        int params1[16]{ -1, 0, 1, 9, 10, 99, 100, 999, 1000, 9999, 10000, 65534, 65535, 65536, -1, -1 };
        test_seq_csi(params1);

        int params2[16]{ 1, -1, -1, -1, 1, -1, 1, 1, 1, -1, -1, -1, -1, 1, 1, 1 };
        test_seq_csi(params2);
}

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        if (vte_parser_new(&parser) < 0)
                return 1;

        g_test_add_func("/vte/parser/sequences/control", test_seq_control);
        g_test_add_func("/vte/parser/sequences/escape/invalid", test_seq_esc_invalid);
        g_test_add_func("/vte/parser/sequences/escape/nF", test_seq_esc_nF);
        g_test_add_func("/vte/parser/sequences/escape/F[pes]", test_seq_esc_Fpes);
        g_test_add_func("/vte/parser/sequences/csi", test_seq_csi);

        auto rv = g_test_run();

        parser = vte_parser_free(parser);
        return rv;
}
