/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
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

#include "parser.hh"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>

#include <glib.h>

#include "parser-charset-tables.hh"

#define WARN(num,str) do { } while (0)
#define hweight32(v) (__builtin_popcount(v))
#define kzalloc(n,v) calloc((n),1)
#define kfree(ptr) free(ptr)

/*
 * Terminal Parser
 * This file contains a bunch of UTF-8 helpers and the main ctlseq-parser. The
 * parser is a simple state-machine that correctly parses all CSI, DCS, OSC, ST
 * control sequences and generic escape sequences.
 * The parser itself does not perform any actions but lets the caller react to
 * detected sequences.
 */

struct vte_parser {
        struct vte_seq seq;
        unsigned int state;
};

/*
 * Command Parser
 * The ctl-seq parser "vte_parser" only detects whole sequences, it does not
 * detect the specific command. Once a sequence is parsed, the command-parsers
 * are used to figure out their meaning.
 */

static unsigned int vte_parse_host_control(const struct vte_seq *seq)
{
        switch (seq->terminator) {
        case 0x00: /* NUL */
                return VTE_CMD_NUL;
        case 0x05: /* ENQ */
                return VTE_CMD_ENQ;
        case 0x07: /* BEL */
                return VTE_CMD_BEL;
        case 0x08: /* BS */
                return VTE_CMD_BS;
        case 0x09: /* HT */
                return VTE_CMD_HT;
        case 0x0a: /* LF */
                return VTE_CMD_LF;
        case 0x0b: /* VT */
                return VTE_CMD_VT;
        case 0x0c: /* FF */
                return VTE_CMD_FF;
        case 0x0d: /* CR */
                return VTE_CMD_CR;
        case 0x0e: /* SO */
                return VTE_CMD_SO;
        case 0x0f: /* SI */
                return VTE_CMD_SI;
        case 0x11: /* DC1 */
                return VTE_CMD_DC1;
        case 0x13: /* DC3 */
                return VTE_CMD_DC3;
        case 0x18: /* CAN */
                /* this is already handled by the state-machine */
                break;
        case 0x1a: /* SUB */
                return VTE_CMD_SUB;
        case 0x1b: /* ESC */
                /* this is already handled by the state-machine */
                break;
        case 0x7f: /* DEL */
                /* this is already handled by the state-machine */
                break;
        case 0x84: /* IND */
                return VTE_CMD_IND;
        case 0x85: /* NEL */
                return VTE_CMD_NEL;
        case 0x88: /* HTS */
                return VTE_CMD_HTS;
        case 0x8d: /* RI */
                return VTE_CMD_RI;
        case 0x8e: /* SS2 */
                return VTE_CMD_SS2;
        case 0x8f: /* SS3 */
                return VTE_CMD_SS3;
        case 0x90: /* DCS */
                /* this is already handled by the state-machine */
                break;
        case 0x96: /* SPA */
                return VTE_CMD_SPA;
        case 0x97: /* EPA */
                return VTE_CMD_EPA;
        case 0x98: /* SOS */
                /* this is already handled by the state-machine */
                break;
        case 0x9a: /* SCI */
                /* this is already handled by the state-machine */
                break;
        case 0x9b: /* CSI */
                /* this is already handled by the state-machine */
                break;
        case 0x9c: /* ST */
                return VTE_CMD_ST;
        case 0x9d: /* OSC */
                /* this is already handled by the state-machine */
                break;
        case 0x9e: /* PM */
                /* this is already handled by the state-machine */
                break;
        case 0x9f: /* APC */
                /* this is already handled by the state-machine */
                break;
        }

        return VTE_CMD_NONE;
}

static unsigned int vte_parse_charset_94(uint32_t raw,
                                         unsigned int flags)
{
        assert (raw >= 0x30 && raw < 0x7f);

        if (flags & VTE_SEQ_FLAG_SPACE)
                return VTE_CHARSET_DRCS;

        switch (flags) {
        case 0:
                if (raw < (0x30 + G_N_ELEMENTS(charset_graphic_94)))
                        return charset_graphic_94[raw - 0x30];
                break;

        case VTE_SEQ_FLAG_BANG:
                if (raw >= 0x40 && (raw < 0x40 + G_N_ELEMENTS(charset_graphic_94_with_2_1)))
                        return charset_graphic_94_with_2_1[raw - 0x40];
                break;

        case VTE_SEQ_FLAG_DQUOTE:
                if (raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_2)))
                        return charset_graphic_94_with_2_2[raw - 0x30];
                break;

        case VTE_SEQ_FLAG_HASH:
                break;

        case VTE_SEQ_FLAG_CASH:
                if (raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_n)))
                        return charset_graphic_94_n[raw - 0x30];
                break;

        case VTE_SEQ_FLAG_PERCENT:
                if (raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_5)))
                        return charset_graphic_94_with_2_5[raw - 0x30];
                break;

        case VTE_SEQ_FLAG_AND:
                if (raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_6)))
                        return charset_graphic_94_with_2_6[raw - 0x30];
                break;
        }

        return VTE_CHARSET_NONE;
}

static unsigned int vte_parse_charset_96(uint32_t raw,
                                         unsigned int flags)
{
        assert (raw >= 0x30 && raw < 0x7f);

        if (flags == 0) { /* Graphic 96-set */
                if (raw < (0x30 + G_N_ELEMENTS(charset_graphic_96)))
                        return charset_graphic_96[raw - 0x30];
        } else if (flags & VTE_SEQ_FLAG_SPACE) { /* DRCS */
                return VTE_CHARSET_DRCS;
        }

        return VTE_CHARSET_NONE;
}

static unsigned int vte_parse_charset_ocs(uint32_t raw,
                                          unsigned int flags)
{
        assert (raw >= 0x30 && raw < 0x7f);

        if (flags == VTE_SEQ_FLAG_PERCENT) {
                /* OCS with standard return */
                if (raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_ocs_with_return)))
                        return charset_ocs_with_return[raw - 0x40];
        } else if (flags == (VTE_SEQ_FLAG_PERCENT | VTE_SEQ_FLAG_SLASH)) {
                /* OCS without standard return */
                if (raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_ocs_without_return)))
                        return charset_ocs_without_return[raw - 0x40];
        }

        return VTE_CHARSET_NONE;
}

static unsigned int vte_parse_charset_control(uint32_t raw,
                                              unsigned int flags)
{
        assert (raw >= 0x30 && raw < 0x7f);

        if (flags == VTE_SEQ_FLAG_BANG) { /* C0 controls */
                if (raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_control_c0)))
                        return charset_control_c0[raw - 0x40];
        } else if (flags == VTE_SEQ_FLAG_DQUOTE) { /* C1 controls */
                if (raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_control_c1)))
                        return charset_control_c1[raw - 0x40];
        }

        return VTE_CHARSET_NONE;
}

static unsigned int vte_parse_host_escape(const struct vte_seq *seq,
                                          unsigned int *cs_out)
{
        unsigned int const flags = seq->intermediates;

        if (flags == 0) {
                switch (seq->terminator) {
                case '6': /* DECBI */
                        return VTE_CMD_DECBI;
                case '7': /* DECSC */
                        return VTE_CMD_DECSC;
                case '8': /* DECRC */
                        return VTE_CMD_DECRC;
                case '9': /* DECFI */
                        return VTE_CMD_DECFI;
                case '<': /* DECANM */
                        return VTE_CMD_DECANM;
                case '=': /* DECKPAM */
                        return VTE_CMD_DECKPAM;
                case '>': /* DECKPNM */
                        return VTE_CMD_DECKPNM;
                case 'D': /* IND */
                        return VTE_CMD_IND;
                case 'E': /* NEL */
                        return VTE_CMD_NEL;
                case 'F': /* Cursor to lower-left corner of screen */
                        return VTE_CMD_XTERM_CLLHP;
                case 'H': /* HTS */
                        return VTE_CMD_HTS;
                case 'M': /* RI */
                        return VTE_CMD_RI;
                case 'N': /* SS2 */
                        return VTE_CMD_SS2;
                case 'O': /* SS3 */
                        return VTE_CMD_SS3;
                case 'P': /* DCS */
                        /* this is already handled by the state-machine */
                        break;
                case 'V': /* SPA */
                        return VTE_CMD_SPA;
                case 'W': /* EPA */
                        return VTE_CMD_EPA;
                case 'X': /* SOS */
                        /* this is already handled by the state-machine */
                        break;
                case 'Z': /* SCI */
                        /* this is already handled by the state-machine */
                        break;
                case '[': /* CSI */
                        /* this is already handled by the state-machine */
                        break;
                case '\\': /* ST */
                        return VTE_CMD_ST;
                case ']': /* OSC */
                        /* this is already handled by the state-machine */
                        break;
                case '^': /* PM */
                        /* this is already handled by the state-machine */
                        break;
                case '_': /* APC */
                        /* this is already handled by the state-machine */
                        break;
                case 'c': /* RIS */
                        return VTE_CMD_RIS;
                case 'd': /* CMD */
                        return VTE_CMD_CMD;
                case 'l': /* Memory lock */
                        return VTE_CMD_XTERM_MLHP;
                case 'm': /* Memory unlock */
                        return VTE_CMD_XTERM_MUHP;
                case 'n': /* LS2 */
                        return VTE_CMD_LS2;
                case 'o': /* LS3 */
                        return VTE_CMD_LS3;
                case '|': /* LS3R */
                        return VTE_CMD_LS3R;
                case '}': /* LS2R */
                        return VTE_CMD_LS2R;
                case '~': /* LS1R */
                        return VTE_CMD_LS1R;
                }

                return VTE_CMD_NONE;
        }

        unsigned int const g_designators =
                VTE_SEQ_FLAG_POPEN | VTE_SEQ_FLAG_PCLOSE |
                VTE_SEQ_FLAG_MULT  | VTE_SEQ_FLAG_PLUS   |
                VTE_SEQ_FLAG_MINUS | VTE_SEQ_FLAG_DOT    |
                VTE_SEQ_FLAG_SLASH;

        if (hweight32(flags & g_designators) == 1) {
                unsigned int const remaining_flags = flags & ~g_designators;
                int cmd = (remaining_flags & VTE_SEQ_FLAG_CASH) ? VTE_CMD_GnDMm : VTE_CMD_GnDm;
                int cs = VTE_CHARSET_NONE;

                switch (flags & g_designators) {
                case VTE_SEQ_FLAG_POPEN:
                case VTE_SEQ_FLAG_PCLOSE:
                case VTE_SEQ_FLAG_MULT:
                case VTE_SEQ_FLAG_PLUS:
                        cs = vte_parse_charset_94(seq->terminator, remaining_flags);
                        break;
                case VTE_SEQ_FLAG_SLASH:
                        if (remaining_flags == VTE_SEQ_FLAG_PERCENT) { /* DOCS */
                                cmd = VTE_CMD_DOCS;
                                cs = vte_parse_charset_ocs(seq->terminator, /* all */ flags);
                                break;
                        }
                        /* fallthrough */
                case VTE_SEQ_FLAG_MINUS:
                case VTE_SEQ_FLAG_DOT:
                        cs = vte_parse_charset_96(seq->terminator, remaining_flags);
                        break;
                }

                if (cs_out)
                        *cs_out = cs;

                return cmd;
        }

        switch (flags) {
        case VTE_SEQ_FLAG_SPACE: /* ACS */
                return VTE_CMD_ACS;

        case VTE_SEQ_FLAG_BANG: /* C0-designate */
        case VTE_SEQ_FLAG_DQUOTE: /* C1-designate */
                if (cs_out)
                        *cs_out = vte_parse_charset_control(seq->terminator, flags);
                return VTE_CMD_CnD;

        case VTE_SEQ_FLAG_HASH:
                switch (seq->terminator) {
                case '3': /* DECDHL top-half */
                        return VTE_CMD_DECDHL_TH;
                case '4': /* DECDHL bottom-half */
                        return VTE_CMD_DECDHL_BH;
                case '5': /* DECSWL */
                        return VTE_CMD_DECSWL;
                case '6': /* DECDWL */
                        return VTE_CMD_DECDWL;
                case '8': /* DECALN */
                        return VTE_CMD_DECALN;
                }
                break;

        case VTE_SEQ_FLAG_CASH:
                /* For compatibility with an earlier version of ISO-2022,
                 * ESC 2/4 4/0, ESC 2/4 4/1 and ESC 2/4 4/2 designate G0
                 * sets (i.e., without the 2/8 as 2nd intermediate byte).
                 */
                switch (seq->terminator) {
                case '@':
                case 'A':
                case 'B': /* G0-designate multibyte charset */
                        if (cs_out)
                                *cs_out = vte_parse_charset_94(seq->terminator,
                                                               flags /* | VTE_SEQ_FLAG_POPEN */);
                        return VTE_CMD_GnDMm;
                }
                break;

        case VTE_SEQ_FLAG_PERCENT: /* DOCS */
        case VTE_SEQ_FLAG_PERCENT | VTE_SEQ_FLAG_SLASH: /* DOCS, but already handled above */
                if (cs_out)
                        *cs_out = vte_parse_charset_ocs(seq->terminator, flags);

                return VTE_CMD_DOCS;

        case VTE_SEQ_FLAG_AND: /* IRR */
                return VTE_CMD_IRR;
        }

        return VTE_CMD_NONE;
}

static unsigned int vte_parse_host_csi(const struct vte_seq *seq)
{
        unsigned int flags;

        flags = seq->intermediates;

        switch (seq->terminator) {
        case 'A':
                if (flags == 0) /* CUU */
                        return VTE_CMD_CUU;
                break;
        case 'a':
                if (flags == 0) /* HPR */
                        return VTE_CMD_HPR;
                else if (flags == VTE_SEQ_FLAG_SPACE) /* TALE */
                        return VTE_CMD_TALE;
                break;
        case 'B':
                if (flags == 0) /* CUD */
                        return VTE_CMD_CUD;
                break;
        case 'b':
                if (flags == 0) /* REP */
                        return VTE_CMD_REP;
                else if (flags == VTE_SEQ_FLAG_SPACE) /* TAC */
                        return VTE_CMD_TAC;
                break;
        case 'C':
                if (flags == 0) /* CUF */
                        return VTE_CMD_CUF;
                break;
        case 'c':
                if (flags == 0) /* DA1 */
                        return VTE_CMD_DA1;
                else if (flags == VTE_SEQ_FLAG_SPACE) /* TCC */
                        return VTE_CMD_TCC;
                else if (flags == VTE_SEQ_FLAG_GT) /* DA2 */
                        return VTE_CMD_DA2;
                else if (flags == VTE_SEQ_FLAG_EQUAL) /* DA3 */
                        return VTE_CMD_DA3;
                break;
        case 'D':
                if (flags == 0) /* CUB */
                        return VTE_CMD_CUB;
                break;
        case 'd':
                if (flags == 0) /* VPA */
                        return VTE_CMD_VPA;
                else if (flags == VTE_SEQ_FLAG_SPACE) /* TSR */
                        return VTE_CMD_TSR;
                break;
        case 'E':
                if (flags == 0) /* CNL */
                        return VTE_CMD_CNL;
                break;
        case 'e':
                if (flags == 0) /* VPR */
                        return VTE_CMD_VPR;
                break;
        case 'F':
                if (flags == 0) /* CPL */
                        return VTE_CMD_CPL;
                break;
        case 'f':
                if (flags == 0) /* HVP */
                        return VTE_CMD_HVP;
                break;
        case 'G':
                if (flags == 0) /* CHA */
                        return VTE_CMD_CHA;
                break;
        case 'g':
                if (flags == 0) /* TBC */
                        return VTE_CMD_TBC;
                else if (flags == VTE_SEQ_FLAG_MULT) /* DECLFKC */
                        return VTE_CMD_DECLFKC;
                break;
        case 'H':
                if (flags == 0) /* CUP */
                        return VTE_CMD_CUP;
                break;
        case 'h':
                if (flags == 0) /* SM ECMA */
                        return VTE_CMD_SM_ECMA;
                else if (flags == VTE_SEQ_FLAG_WHAT) /* SM DEC */
                        return VTE_CMD_SM_DEC;
                break;
        case 'I':
                if (flags == 0) /* CHT */
                        return VTE_CMD_CHT;
                break;
        case 'i':
                if (flags == 0) /* MC ANSI */
                        return VTE_CMD_MC_ANSI;
                else if (flags == VTE_SEQ_FLAG_WHAT) /* MC DEC */
                        return VTE_CMD_MC_DEC;
                break;
        case 'J':
                if (flags == 0) /* ED */
                        return VTE_CMD_ED;
                else if (flags == VTE_SEQ_FLAG_WHAT) /* DECSED */
                        return VTE_CMD_DECSED;
                break;
        case 'K':
                if (flags == 0) /* EL */
                        return VTE_CMD_EL;
                else if (flags == VTE_SEQ_FLAG_WHAT) /* DECSEL */
                        return VTE_CMD_DECSEL;
                break;
        case 'L':
                if (flags == 0) /* IL */
                        return VTE_CMD_IL;
                break;
        case 'l':
                if (flags == 0) /* RM ECMA */
                        return VTE_CMD_RM_ECMA;
                else if (flags == VTE_SEQ_FLAG_WHAT) /* RM DEC */
                        return VTE_CMD_RM_DEC;
                break;
        case 'M':
                if (flags == 0) /* DL */
                        return VTE_CMD_DL;
                break;
        case 'm':
                if (flags == 0) /* SGR */
                        return VTE_CMD_SGR;
                else if (flags == VTE_SEQ_FLAG_GT) /* XTERM SMR */
                        return VTE_CMD_XTERM_SRV;
                break;
        case 'n':
                if (flags == 0) /* DSR ECMA */
                        return VTE_CMD_DSR_ECMA;
                else if (flags == VTE_SEQ_FLAG_GT) /* XTERM RMR */
                        return VTE_CMD_XTERM_RRV;
                else if (flags == VTE_SEQ_FLAG_WHAT) /* DSR DEC */
                        return VTE_CMD_DSR_DEC;
                break;
        case 'P':
                if (flags == 0) /* DCH */
                        return VTE_CMD_DCH;
                else if (flags == VTE_SEQ_FLAG_SPACE) /* PPA */
                        return VTE_CMD_PPA;
                break;
        case 'p':
                if (flags == 0) /* DECSSL */
                        return VTE_CMD_DECSSL;
                else if (flags == VTE_SEQ_FLAG_SPACE) /* DECSSCLS */
                        return VTE_CMD_DECSSCLS;
                else if (flags == VTE_SEQ_FLAG_BANG) /* DECSTR */
                        return VTE_CMD_DECSTR;
                else if (flags == VTE_SEQ_FLAG_DQUOTE) /* DECSCL */
                        return VTE_CMD_DECSCL;
                else if (flags == VTE_SEQ_FLAG_CASH) /* DECRQM-ECMA */
                        return VTE_CMD_DECRQM_ECMA;
                else if (flags == (VTE_SEQ_FLAG_CASH |
                                   VTE_SEQ_FLAG_WHAT)) /* DECRQM-DEC */
                        return VTE_CMD_DECRQM_DEC;
                else if (flags == VTE_SEQ_FLAG_PCLOSE) /* DECSDPT */
                        return VTE_CMD_DECSDPT;
                else if (flags == VTE_SEQ_FLAG_MULT) /* DECSPPCS */
                        return VTE_CMD_DECSPPCS;
                else if (flags == VTE_SEQ_FLAG_PLUS) /* DECSR */
                        return VTE_CMD_DECSR;
                else if (flags == VTE_SEQ_FLAG_COMMA) /* DECLTOD */
                        return VTE_CMD_DECLTOD;
                else if (flags == VTE_SEQ_FLAG_GT) /* XTERM SPM */
                        return VTE_CMD_XTERM_SPM;
                break;
        case 'Q':
                if (flags == VTE_SEQ_FLAG_SPACE) /* PPR */
                        return VTE_CMD_PPR;
                break;
        case 'q':
                if (flags == 0) /* DECLL */
                        return VTE_CMD_DECLL;
                else if (flags == VTE_SEQ_FLAG_SPACE) /* DECSCUSR */
                        return VTE_CMD_DECSCUSR;
                else if (flags == VTE_SEQ_FLAG_DQUOTE) /* DECSCA */
                        return VTE_CMD_DECSCA;
                else if (flags == VTE_SEQ_FLAG_CASH) /* DECSDDT */
                        return VTE_CMD_DECSDDT;
                else if (flags == VTE_SEQ_FLAG_MULT) /* DECSRC */
                        return VTE_CMD_DECSR;
                else if (flags == VTE_SEQ_FLAG_PLUS) /* DECELF */
                        return VTE_CMD_DECELF;
                else if (flags == VTE_SEQ_FLAG_COMMA) /* DECTID */
                        return VTE_CMD_DECTID;
                break;
        case 'R':
                if (flags == VTE_SEQ_FLAG_SPACE) /* PPB */
                        return VTE_CMD_PPB;
                break;
        case 'r':
                if (flags == 0) {
                        /* DECSTBM */
                        return VTE_CMD_DECSTBM;
                } else if (flags == VTE_SEQ_FLAG_SPACE) {
                        /* DECSKCV */
                        return VTE_CMD_DECSKCV;
                } else if (flags == VTE_SEQ_FLAG_CASH) {
                        /* DECCARA */
                        return VTE_CMD_DECCARA;
                } else if (flags == VTE_SEQ_FLAG_MULT) {
                        /* DECSCS */
                        return VTE_CMD_DECSCS;
                } else if (flags == VTE_SEQ_FLAG_PLUS) {
                        /* DECSMKR */
                        return VTE_CMD_DECSMKR;
                } else if (flags == VTE_SEQ_FLAG_WHAT) {
                        /*
                         * There's a conflict between DECPCTERM and XTERM-RPM.
                         * XTERM-RPM takes a single argument, DECPCTERM takes 2.
                         * Split both up and forward the call to the closer
                         * match.
                         */
                        // FIXMEchpe!
                        if (seq->n_final_args <= 1) /* XTERM RPM */
                                return VTE_CMD_XTERM_RPM;
                        else if (seq->n_final_args >= 2) /* DECPCTERM */
                                return VTE_CMD_DECPCTERM;
                }
                break;
        case 'S':
                if (flags == 0) /* SU */
                        return VTE_CMD_SU;
                else if (flags == VTE_SEQ_FLAG_WHAT) /* XTERM SGFX */
                        return VTE_CMD_XTERM_SGFX;
                break;
        case 's':
                if (flags == 0) {
                        /*
                         * There's a conflict between DECSLRM and SC-ANSI which
                         * cannot be resolved without knowing the state of
                         * DECLRMM. We leave that decision up to the caller.
                         */
                        return VTE_CMD_DECSLRM_OR_SC;
                } else if (flags == VTE_SEQ_FLAG_CASH) {
                        /* DECSPRTT */
                        return VTE_CMD_DECSPRTT;
                } else if (flags == VTE_SEQ_FLAG_MULT) {
                        /* DECSFC */
                        return VTE_CMD_DECSFC;
                } else if (flags == VTE_SEQ_FLAG_WHAT) {
                        /* XTERM SPM */
                        return VTE_CMD_XTERM_SPM;
                }
                break;
        case 'T':
                if (flags == 0) {
                        /*
                         * There's a conflict between SD and XTERM IHMT that we
                         * have to resolve by checking the parameter count.
                         * XTERM_IHMT needs exactly 5 arguments, SD takes 0 or
                         * 1. We're conservative here and give both a wider
                         * range to allow unused arguments (compat...).
                         */
                        // FIXMEchpe!
                        if (seq->n_final_args < 5) {
                                /* SD */
                                return VTE_CMD_SD;
                        } else if (seq->n_final_args >= 5) {
                                /* XTERM IHMT */
                                return VTE_CMD_XTERM_IHMT;
                        }
                } else if (flags == VTE_SEQ_FLAG_GT) {
                        /* XTERM RTM */
                        return VTE_CMD_XTERM_RTM;
                }
                break;
        case 't':
                if (flags == 0) {
                        /*
                         * There's a conflict between XTERM_WM and DECSLPP. We
                         * cannot resolve it as some combinations are valid for
                         * both. We go with XTERM_WM for now.
                         *
                         * TODO: Figure out how to resolve that conflict and
                         *       return VTE_CMD_DECSLPP if possible.
                         */
                        return VTE_CMD_XTERM_WM; /* XTERM WM */
                } else if (flags == VTE_SEQ_FLAG_SPACE) {
                        /* DECSWBV */
                        return VTE_CMD_DECSWBV;
                } else if (flags == VTE_SEQ_FLAG_DQUOTE) {
                        /* DECSRFR */
                        return VTE_CMD_DECSRFR;
                } else if (flags == VTE_SEQ_FLAG_CASH) {
                        /* DECRARA */
                        return VTE_CMD_DECRARA;
                } else if (flags == VTE_SEQ_FLAG_GT) {
                        /* XTERM STM */
                        return VTE_CMD_XTERM_STM;
                }
                break;
        case 'U':
                if (flags == 0) /* NP */
                        return VTE_CMD_NP;
                break;
        case 'u':
                if (flags == 0) {
                        /* RC */
                        return VTE_CMD_RC;
                } else if (flags == VTE_SEQ_FLAG_SPACE) {
                        /* DECSMBV */
                        return VTE_CMD_DECSMBV;
                } else if (flags == VTE_SEQ_FLAG_DQUOTE) {
                        /* DECSTRL */
                        return VTE_CMD_DECSTRL;
                } else if (flags == VTE_SEQ_FLAG_WHAT) {
                        /* DECRQUPSS */
                        return VTE_CMD_DECRQUPSS;
                } else if (vte_seq_arg_value(seq->args[0]) == 1 && flags == VTE_SEQ_FLAG_CASH) {
                        /* DECRQTSR */
                        return VTE_CMD_DECRQTSR;
                } else if (flags == VTE_SEQ_FLAG_MULT) {
                        /* DECSCP */
                        return VTE_CMD_DECSCP;
                } else if (flags == VTE_SEQ_FLAG_COMMA) {
                        /* DECRQKT */
                        return VTE_CMD_DECRQKT;
                }
                break;
        case 'V':
                if (flags == 0) /* PP */
                        return VTE_CMD_PP;
                break;
        case 'v':
                if (flags == VTE_SEQ_FLAG_SPACE) /* DECSLCK */
                        return VTE_CMD_DECSLCK;
                else if (flags == VTE_SEQ_FLAG_DQUOTE) /* DECRQDE */
                        return VTE_CMD_DECRQDE;
                else if (flags == VTE_SEQ_FLAG_CASH) /* DECCRA */
                        return VTE_CMD_DECCRA;
                else if (flags == VTE_SEQ_FLAG_COMMA) /* DECRPKT */
                        return VTE_CMD_DECRPKT;
                break;
        case 'W':
                if (vte_seq_arg_value(seq->args[0]) == 5 && flags == VTE_SEQ_FLAG_WHAT) {
                        /* DECST8C */
                        return VTE_CMD_DECST8C;
                }
                break;
        case 'w':
                if (flags == VTE_SEQ_FLAG_CASH) /* DECRQPSR */
                        return VTE_CMD_DECRQPSR;
                else if (flags == VTE_SEQ_FLAG_SQUOTE) /* DECEFR */
                        return VTE_CMD_DECEFR;
                else if (flags == VTE_SEQ_FLAG_PLUS) /* DECSPP */
                        return VTE_CMD_DECSPP;
                break;
        case 'X':
                if (flags == 0) /* ECH */
                        return VTE_CMD_ECH;
                break;
        case 'x':
                if (flags == 0) /* DECREQTPARM */
                        return VTE_CMD_DECREQTPARM;
                else if (flags == VTE_SEQ_FLAG_CASH) /* DECFRA */
                        return VTE_CMD_DECFRA;
                else if (flags == VTE_SEQ_FLAG_MULT) /* DECSACE */
                        return VTE_CMD_DECSACE;
                else if (flags == VTE_SEQ_FLAG_PLUS) /* DECRQPKFM */
                        return VTE_CMD_DECRQPKFM;
                break;
        case 'y':
                if (flags == 0) /* DECTST */
                        return VTE_CMD_DECTST;
                else if (flags == VTE_SEQ_FLAG_MULT) /* DECRQCRA */
                        return VTE_CMD_DECRQCRA;
                else if (flags == VTE_SEQ_FLAG_PLUS) /* DECPKFMR */
                        return VTE_CMD_DECPKFMR;
                break;
        case 'Z':
                if (flags == 0) /* CBT */
                        return VTE_CMD_CBT;
                break;
        case 'z':
                if (flags == VTE_SEQ_FLAG_CASH) /* DECERA */
                        return VTE_CMD_DECERA;
                else if (flags == VTE_SEQ_FLAG_SQUOTE) /* DECELR */
                        return VTE_CMD_DECELR;
                else if (flags == VTE_SEQ_FLAG_MULT) /* DECINVM */
                        return VTE_CMD_DECINVM;
                else if (flags == VTE_SEQ_FLAG_PLUS) /* DECPKA */
                        return VTE_CMD_DECPKA;
                break;
        case '@':
                if (flags == 0) /* ICH */
                        return VTE_CMD_ICH;
                break;
        case '`':
                if (flags == 0) /* HPA */
                        return VTE_CMD_HPA;
                else if (flags == VTE_SEQ_FLAG_SPACE) /* TATE */
                        return VTE_CMD_TATE;
                break;
        case '{':
                if (flags == VTE_SEQ_FLAG_CASH) /* DECSERA */
                        return VTE_CMD_DECSERA;
                else if (flags == VTE_SEQ_FLAG_SQUOTE) /* DECSLE */
                        return VTE_CMD_DECSLE;
                break;
        case '|':
                if (flags == VTE_SEQ_FLAG_CASH) /* DECSCPP */
                        return VTE_CMD_DECSCPP;
                else if (flags == VTE_SEQ_FLAG_SQUOTE) /* DECRQLP */
                        return VTE_CMD_DECRQLP;
                else if (flags == VTE_SEQ_FLAG_MULT) /* DECSNLS */
                        return VTE_CMD_DECSNLS;
                break;
        case '}':
                if (flags == VTE_SEQ_FLAG_SPACE) /* DECKBD */
                        return VTE_CMD_DECKBD;
                else if (flags == VTE_SEQ_FLAG_CASH) /* DECSASD */
                        return VTE_CMD_DECSASD;
                else if (flags == VTE_SEQ_FLAG_SQUOTE) /* DECIC */
                        return VTE_CMD_DECIC;
                break;
        case '~':
                if (flags == VTE_SEQ_FLAG_SPACE) /* DECTME */
                        return VTE_CMD_DECTME;
                else if (flags == VTE_SEQ_FLAG_CASH) /* DECSSDT */
                        return VTE_CMD_DECSSDT;
                else if (flags == VTE_SEQ_FLAG_SQUOTE) /* DECDC */
                        return VTE_CMD_DECDC;
                break;
        }

        return VTE_CMD_NONE;
}

static unsigned int vte_parse_host_dcs(const struct vte_seq *seq)
{
        unsigned int const flags = seq->intermediates;

        switch (seq->terminator) {
        case 'p':
                if (flags == 0) /* DECREGIS */
                        return VTE_CMD_DECREGIS;
                else if (flags == VTE_SEQ_FLAG_CASH) /* DECRSTS */
                        return VTE_CMD_DECRSTS;
                break;
        case 'q':
                if (flags == 0) /* DECSIXEL */
                        return VTE_CMD_DECSIXEL;
                else if (flags == VTE_SEQ_FLAG_CASH) /* DECRQSS */
                        return VTE_CMD_DECRQSS;
                break;
        case 'r':
                if (flags == 0) /* DECLBAN */
                        return VTE_CMD_DECLBAN;
                else if (flags == VTE_SEQ_FLAG_CASH) /* DECRQSS */
                        return VTE_CMD_DECRQSS; // FIXMEchpe really??
                break;
        case 's':
                if (flags == VTE_SEQ_FLAG_CASH) /* DECRQTSR */
                        return VTE_CMD_DECRQTSR;
                break;
        case 't':
                if (flags == VTE_SEQ_FLAG_CASH) /* DECRSPS */
                        return VTE_CMD_DECRSPS;
                break;
        case 'u':
                if (flags == VTE_SEQ_FLAG_BANG) /* DECAUPSS */
                        return VTE_CMD_DECAUPSS;
                break;
        case 'v':
                if (flags == 0) /* DECLANS */
                        return VTE_CMD_DECLANS;
                break;
        case 'w':
                if (flags == 0) /* DECLBD */
                        return VTE_CMD_DECLBD;
                break;
        case 'x':
                if (flags == VTE_SEQ_FLAG_DQUOTE) /* DECPFK */
                        return VTE_CMD_DECPFK;
                break;
        case 'y':
                if (flags == VTE_SEQ_FLAG_DQUOTE) /* DECPAK */
                        return VTE_CMD_DECPAK;
                break;
        case 'z':
                if (flags == VTE_SEQ_FLAG_BANG) /* DECDMAC */
                        return VTE_CMD_DECDMAC;
                else if (flags == VTE_SEQ_FLAG_DQUOTE) /* DECCKD */
                        return VTE_CMD_DECCKD;
                break;
        case '{':
                if (flags == 0) /* DECDLD */
                        return VTE_CMD_DECDLD;
                else if (flags == VTE_SEQ_FLAG_BANG) /* DECSTUI */
                        return VTE_CMD_DECSTUI;
                break;
        case '|':
                if (flags == 0) /* DECUDK */
                        return VTE_CMD_DECUDK;
                break;
        }

        return VTE_CMD_NONE;
}

static unsigned int vte_parse_host_sci(const struct vte_seq *seq)
{
        return VTE_CMD_NONE;
}

/*
 * State Machine
 * This parser controls the parser-state and returns any detected sequence to
 * the caller. The parser is based on this state-diagram from Paul Williams:
 *   https://vt100.net/emu/
 * It was written from scratch and extended where needed.
 * This parser is fully compatible up to the vt500 series. We expect UCS-4 as
 * input. It's the callers responsibility to do any UTF-8 parsing.
 */

enum parser_state {
        STATE_NONE,             /* placeholder */
        STATE_GROUND,           /* initial state and ground */
        STATE_ESC,              /* ESC sequence was started */
        STATE_ESC_INT,          /* intermediate escape characters */
        STATE_CSI_ENTRY,        /* starting CSI sequence */
        STATE_CSI_PARAM,        /* CSI parameters */
        STATE_CSI_INT,          /* intermediate CSI characters */
        STATE_CSI_IGNORE,       /* CSI error; ignore this CSI sequence */
        STATE_DCS_ENTRY,        /* starting DCS sequence */
        STATE_DCS_PARAM,        /* DCS parameters */
        STATE_DCS_INT,          /* intermediate DCS characters */
        STATE_DCS_PASS,         /* DCS data passthrough */
        STATE_DCS_IGNORE,       /* DCS error; ignore this DCS sequence */
        STATE_OSC_STRING,       /* parsing OSC sequence */
        STATE_ST_IGNORE,        /* unimplemented seq; ignore until ST */
        STATE_SCI,              /* single character introducer sequence was started */

        STATE_N,
};

enum parser_action {
        ACTION_NONE,            /* placeholder */
        ACTION_CLEAR,           /* clear parameters */
        ACTION_IGNORE,          /* ignore the character entirely */
        ACTION_PRINT,           /* print the character on the console */
        ACTION_EXECUTE,         /* execute single control character (C0/C1) */
        ACTION_COLLECT,         /* collect intermediate character */
        ACTION_PARAM,           /* collect parameter character */
        ACTION_ESC_DISPATCH,    /* dispatch escape sequence */
        ACTION_CSI_DISPATCH,    /* dispatch CSI sequence */
        ACTION_DCS_START,       /* start of DCS data */
        ACTION_DCS_CONSUME,     /* consume DCS terminator */
        ACTION_DCS_COLLECT,     /* collect DCS data */
        ACTION_DCS_DISPATCH,    /* dispatch DCS sequence */
        ACTION_OSC_START,       /* clear and clear string data */
        ACTION_OSC_COLLECT,     /* collect OSC data */
        ACTION_OSC_DISPATCH,    /* dispatch OSC sequence */
        ACTION_SCI_DISPATCH,    /* dispatch SCI sequence */
        ACTION_N,
};

/**
 * vte_parser_new() - Allocate parser object
 * @out:        output variable for new parser object
 *
 * Return: 0 on success, negative error code on failure.
 */
int vte_parser_new(struct vte_parser **out)
{
        struct vte_parser *parser;

        parser = (struct vte_parser*)kzalloc(sizeof(*parser), GFP_KERNEL);
        if (!parser)
                return -ENOMEM;

        vte_seq_string_init(&parser->seq.arg_str);

        *out = parser;
        return 0;
}

/**
 * vte_parser_free() - Free parser object
 * @parser:        parser object to free, or NULL
 *
 * Return: NULL is returned.
 */
struct vte_parser *vte_parser_free(struct vte_parser *parser)
{
        if (!parser)
                return NULL;

        vte_seq_string_free(&parser->seq.arg_str);
        kfree(parser);
        return NULL;
}

static inline void parser_clear(struct vte_parser *parser)
{
        unsigned int i;

        parser->seq.command = VTE_CMD_NONE;
        parser->seq.terminator = 0;
        parser->seq.intermediates = 0;
        parser->seq.charset = VTE_CHARSET_NONE;
        parser->seq.n_args = 0;
        parser->seq.n_final_args = 0;
        /* FIXME: we only really need to clear 0..n_args+1 since all others have not been touched */
        // FIXMEchpe: now that DEFAULT is all-zero, use memset here
        for (i = 0; i < VTE_PARSER_ARG_MAX; ++i)
                parser->seq.args[i] = VTE_SEQ_ARG_INIT_DEFAULT;
}

static int parser_ignore(struct vte_parser *parser, uint32_t raw)
{
        parser_clear(parser);
        parser->seq.type = VTE_SEQ_IGNORE;
        parser->seq.command = VTE_CMD_NONE;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;

        return parser->seq.type;
}

static int parser_print(struct vte_parser *parser, uint32_t raw)
{
        parser_clear(parser);
        parser->seq.type = VTE_SEQ_GRAPHIC;
        parser->seq.command = VTE_CMD_GRAPHIC;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;

        return parser->seq.type;
}

static int parser_execute(struct vte_parser *parser, uint32_t raw)
{
        parser->seq.type = VTE_SEQ_CONTROL;
        parser->seq.command = VTE_CMD_GRAPHIC;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;
        parser->seq.command = vte_parse_host_control(&parser->seq);

        return parser->seq.type;
}

static void parser_collect(struct vte_parser *parser, uint32_t raw)
{
        /*
         * Usually, characters from 0x30 to 0x3f are only allowed as leading
         * markers (or as part of the parameters), characters from 0x20 to 0x2f
         * are only allowed as trailing markers. However, our state-machine
         * already verifies those restrictions so we can handle them the same
         * way here. Note that we safely allow markers to be specified multiple
         * times.
         */

        if (raw >= 0x20 && raw <= 0x3f)
                parser->seq.intermediates |= 1 << (raw - 0x20);
}

static void parser_param(struct vte_parser *parser, uint32_t raw)
{
        if (raw == ';') {
                if (parser->seq.n_args < VTE_PARSER_ARG_MAX) {
                        vte_seq_arg_finish(&parser->seq.args[parser->seq.n_args], false);
                        ++parser->seq.n_args;
                        ++parser->seq.n_final_args;
                }

                return;
        }
        if (raw == ':') {
                if (parser->seq.n_args < VTE_PARSER_ARG_MAX) {
                        vte_seq_arg_finish(&parser->seq.args[parser->seq.n_args], true);
                        ++parser->seq.n_args;
                }

                return;
        }

        if (parser->seq.n_args >= VTE_PARSER_ARG_MAX)
                return;

        if (raw >= '0' && raw <= '9') {
                vte_seq_arg_push(&parser->seq.args[parser->seq.n_args], raw);
        }
}

static inline void parser_osc_start(struct vte_parser *parser)
{
        parser_clear(parser);

        vte_seq_string_reset(&parser->seq.arg_str);
}

static void parser_osc_collect(struct vte_parser *parser, uint32_t raw)
{
        /*
         * Only characters from 0x20..0x7e and >= 0xa0 are allowed here.
         * Our state-machine already verifies those restrictions.
         */

        vte_seq_string_push(&parser->seq.arg_str, raw);
}

static inline void parser_dcs_start(struct vte_parser *parser)
{
        parser_clear(parser);

        vte_seq_string_reset(&parser->seq.arg_str);
}

static void parser_dcs_consume(struct vte_parser *parser, uint32_t raw)
{
        /* parser->seq is cleared during DCS-START state, thus there's no need
         * to clear invalid fields here. */

        if (parser->seq.n_args < VTE_PARSER_ARG_MAX) {
                if (parser->seq.n_args > 0 ||
                    vte_seq_arg_started(parser->seq.args[parser->seq.n_args])) {
                        vte_seq_arg_finish(&parser->seq.args[parser->seq.n_args], false);
                        ++parser->seq.n_args;
                        ++parser->seq.n_final_args;
                }
        }

        parser->seq.type = VTE_SEQ_DCS;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;
        parser->seq.command = vte_parse_host_dcs(&parser->seq);
}

static void parser_dcs_collect(struct vte_parser *parser, uint32_t raw)
{
        vte_seq_string_push(&parser->seq.arg_str, raw);
}

static int parser_esc(struct vte_parser *parser, uint32_t raw)
{
        parser->seq.type = VTE_SEQ_ESCAPE;
        parser->seq.command = VTE_CMD_NONE;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;
        parser->seq.command = vte_parse_host_escape(&parser->seq,
                                                    &parser->seq.charset);

        return parser->seq.type;
}

static int parser_csi(struct vte_parser *parser, uint32_t raw)
{
        /* parser->seq is cleared during CSI-ENTER state, thus there's no need
         * to clear invalid fields here. */

        if (parser->seq.n_args < VTE_PARSER_ARG_MAX) {
                if (parser->seq.n_args > 0 ||
                    vte_seq_arg_started(parser->seq.args[parser->seq.n_args])) {
                        vte_seq_arg_finish(&parser->seq.args[parser->seq.n_args], false);
                        ++parser->seq.n_args;
                        ++parser->seq.n_final_args;
                }
        }

        parser->seq.type = VTE_SEQ_CSI;
        parser->seq.command = VTE_CMD_NONE;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;
        parser->seq.command = vte_parse_host_csi(&parser->seq);

        return parser->seq.type;
}

static int parser_osc(struct vte_parser *parser, uint32_t raw)
{
        /* parser->seq is cleared during OSC_START state, thus there's no need
         * to clear invalid fields here. */

        vte_seq_string_finish(&parser->seq.arg_str);

        parser->seq.type = VTE_SEQ_OSC;
        parser->seq.command = VTE_CMD_OSC;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;

        return parser->seq.type;
}

static int parser_dcs(struct vte_parser *parser, uint32_t raw)
{
        /* parser->seq was already filled in parser_dcs_consume() */

        return parser->seq.type;
}

static int parser_sci(struct vte_parser *parser, uint32_t raw)
{
        parser->seq.type = VTE_SEQ_SCI;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;
        parser->seq.command = vte_parse_host_sci(&parser->seq);

        return parser->seq.type;
}

/* perform state transition and dispatch related actions */
static int parser_transition(struct vte_parser *parser,
                             uint32_t raw,
                             unsigned int state,
                             unsigned int action)
{
        if (state != STATE_NONE)
                parser->state = state;

        switch (action) {
        case ACTION_NONE:
                return VTE_SEQ_NONE;
        case ACTION_CLEAR:
                parser_clear(parser);
                return VTE_SEQ_NONE;
        case ACTION_IGNORE:
                return parser_ignore(parser, raw);
        case ACTION_PRINT:
                return parser_print(parser, raw);
        case ACTION_EXECUTE:
                return parser_execute(parser, raw);
        case ACTION_COLLECT:
                parser_collect(parser, raw);
                return VTE_SEQ_NONE;
        case ACTION_PARAM:
                parser_param(parser, raw);
                return VTE_SEQ_NONE;
        case ACTION_ESC_DISPATCH:
                return parser_esc(parser, raw);
        case ACTION_CSI_DISPATCH:
                return parser_csi(parser, raw);
        case ACTION_DCS_START:
                parser_dcs_start(parser);
                return VTE_SEQ_NONE;
        case ACTION_DCS_CONSUME:
                parser_dcs_consume(parser, raw);
                return VTE_SEQ_NONE;
        case ACTION_DCS_COLLECT:
                parser_dcs_collect(parser, raw);
                return VTE_SEQ_NONE;
        case ACTION_DCS_DISPATCH:
                return parser_dcs(parser, raw);
        case ACTION_OSC_START:
                parser_osc_start(parser);
                return VTE_SEQ_NONE;
        case ACTION_OSC_COLLECT:
                parser_osc_collect(parser, raw);
                return VTE_SEQ_NONE;
        case ACTION_OSC_DISPATCH:
                return parser_osc(parser, raw);
        case ACTION_SCI_DISPATCH:
                return parser_sci(parser, raw);
        default:
                WARN(1, "invalid vte-parser action");
                return VTE_SEQ_NONE;
        }
}

static int parser_feed_to_state(struct vte_parser *parser, uint32_t raw)
{
        switch (parser->state) {
        case STATE_NONE:
                /*
                 * During initialization, parser->state is cleared. Treat this
                 * as STATE_GROUND. We will then never get to STATE_NONE again.
                 */
        case STATE_GROUND:
                switch (raw) {
                case 0x00 ... 0x1f:        /* C0 */
                case 0x80 ... 0x9b:        /* C1 \ { ST } */
                case 0x9d ... 0x9f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_PRINT);
        case STATE_ESC:
                switch (raw) {
                case 0x00 ... 0x1f:        /* C0 */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_ESC_INT,
                                                 ACTION_COLLECT);
                case 0x30 ... 0x4f:        /* ['0' - '~'] \ */
                case 0x51 ... 0x57:        /* { 'P', 'X', 'Z' '[', ']', '^', '_' } */
                case 0x59:
                case 0x5c:
                case 0x60 ... 0x7e:
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_ESC_DISPATCH);
                case 0x50:                /* 'P' */
                        return parser_transition(parser, raw, STATE_DCS_ENTRY,
                                                 ACTION_DCS_START);
                case 0x5a:                /* 'Z' */
                        return parser_transition(parser, raw, STATE_SCI,
                                                 ACTION_CLEAR);
                case 0x5b:                /* '[' */
                        return parser_transition(parser, raw, STATE_CSI_ENTRY,
                                                 ACTION_CLEAR);
                case 0x5d:                /* ']' */
                        return parser_transition(parser, raw, STATE_OSC_STRING,
                                                 ACTION_OSC_START);
                case 0x58:                /* 'X' */
                case 0x5e:                /* '^' */
                case 0x5f:                /* '_' */
                        return parser_transition(parser, raw, STATE_ST_IGNORE,
                                                 ACTION_NONE);
                case 0x7f:                /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_ESC_INT, ACTION_COLLECT);
        case STATE_ESC_INT:
                switch (raw) {
                case 0x00 ... 0x1f:        /* C0 */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_COLLECT);
                case 0x30 ... 0x7e:        /* ['0' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_ESC_DISPATCH);
                case 0x7f:                /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_COLLECT);
        case STATE_CSI_ENTRY:
                switch (raw) {
                case 0x00 ... 0x1f:        /* C0 */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_CSI_INT,
                                                 ACTION_COLLECT);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                case 0x3a ... 0x3b:        /* [':' - ';'] */
                        return parser_transition(parser, raw, STATE_CSI_PARAM,
                                                 ACTION_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition(parser, raw, STATE_CSI_PARAM,
                                                 ACTION_COLLECT);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_CSI_DISPATCH);
                case 0x7f:                /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_CSI_IGNORE, ACTION_NONE);
        case STATE_CSI_PARAM:
                switch (raw) {
                case 0x00 ... 0x1f:        /* C0 */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_CSI_INT,
                                                 ACTION_COLLECT);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                case 0x3a ... 0x3b:        /* [':' - ';'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition(parser, raw, STATE_CSI_IGNORE,
                                                 ACTION_NONE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_CSI_DISPATCH);
                case 0x7f:                /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_CSI_IGNORE, ACTION_NONE);
        case STATE_CSI_INT:
                switch (raw) {
                case 0x00 ... 0x1f:        /* C0 */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_COLLECT);
                case 0x30 ... 0x3f:        /* ['0' - '?'] */
                        return parser_transition(parser, raw, STATE_CSI_IGNORE,
                                                 ACTION_NONE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_CSI_DISPATCH);
                case 0x7f:                /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_CSI_IGNORE, ACTION_NONE);
        case STATE_CSI_IGNORE:
                switch (raw) {
                case 0x00 ... 0x1f:        /* C0 */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x20 ... 0x3f:        /* [' ' - '?'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_NONE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_NONE);
                case 0x7f:                /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_NONE);
        case STATE_DCS_ENTRY:
                switch (raw) {
                case 0x00 ... 0x1f:        /* C0 */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_DCS_INT,
                                                 ACTION_COLLECT);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                case 0x3a ... 0x3b:        /* [':' - ';'] */
                        return parser_transition(parser, raw, STATE_DCS_PARAM,
                                                 ACTION_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition(parser, raw, STATE_DCS_PARAM,
                                                 ACTION_COLLECT);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_DCS_PASS,
                                                 ACTION_DCS_CONSUME);
                case 0x7f:                /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_DCS_PASS, ACTION_DCS_CONSUME);
        case STATE_DCS_PARAM:
                switch (raw) {
                case 0x00 ... 0x1f:        /* C0 */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_DCS_INT,
                                                 ACTION_COLLECT);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                case 0x3a ... 0x3b:        /* [':' - ';'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition(parser, raw, STATE_DCS_IGNORE,
                                                 ACTION_NONE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_DCS_PASS,
                                                 ACTION_DCS_CONSUME);
                case 0x7f:                /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_DCS_PASS, ACTION_DCS_CONSUME);
        case STATE_DCS_INT:
                switch (raw) {
                case 0x00 ... 0x1f:        /* C0 */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_COLLECT);
                case 0x30 ... 0x3f:        /* ['0' - '?'] */
                        return parser_transition(parser, raw, STATE_DCS_IGNORE,
                                                 ACTION_NONE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_DCS_PASS,
                                                 ACTION_DCS_CONSUME);
                case 0x7f:                /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_DCS_PASS, ACTION_DCS_CONSUME);
        case STATE_DCS_PASS:
                switch (raw) {
                case 0x00 ... 0x7e:        /* ASCII \ { DEL } */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_DCS_COLLECT);
                case 0x7f:                /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_DCS_DISPATCH);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_DCS_COLLECT);
        case STATE_DCS_IGNORE:
                switch (raw) {
                case 0x00 ... 0x7f:        /* ASCII */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_NONE);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_NONE);
        case STATE_OSC_STRING:
                switch (raw) {
                case 0x00 ... 0x06:        /* C0 \ { BEL } */
                case 0x08 ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x20 ... 0x7f:        /* [' ' - DEL] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_OSC_COLLECT);
                case 0x07:                /* BEL */
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_OSC_DISPATCH);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_OSC_COLLECT);
        case STATE_ST_IGNORE:
                switch (raw) {
                case 0x00 ... 0x7f:        /* ASCII */
                        return parser_transition(parser, raw,
                                                 STATE_NONE, ACTION_IGNORE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw,
                                                 STATE_GROUND, ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_NONE);
        case STATE_SCI:
                switch (raw) {
                case 0x08 ... 0x0d:        /* BS, HT, LF, VT, FF, CR */
                case 0x20 ... 0x7e:        /* [' ' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_SCI_DISPATCH);
                case 0x7f:                 /* DEL */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw, STATE_GROUND,
                                         ACTION_IGNORE);
        }

        WARN(1, "bad vte-parser state");
        return -EINVAL;
}

int vte_parser_feed(struct vte_parser *parser,
                    /* const */ struct vte_seq **seq_out,
                    uint32_t raw)
{
        int ret;

        /*
         * Notes:
         *  * DEC treats GR codes as GL. We don't do that as we require UTF-8
         *    as charset and, thus, it doesn't make sense to treat GR special.
         *  * During control sequences, unexpected C1 codes cancel the sequence
         *    and immediately start a new one. C0 codes, however, may or may not
         *    be ignored/executed depending on the sequence.
         */

        switch (raw) {
        case 0x18:                /* CAN */
                ret = parser_transition(parser, raw,
                                        STATE_GROUND, ACTION_IGNORE);
                break;
        case 0x1a:                /* SUB */
                ret = parser_transition(parser, raw,
                                        STATE_GROUND, ACTION_EXECUTE);
                break;
        case 0x80 ... 0x8f:        /* C1 \ {DCS, SOS, SCI, CSI, ST, OSC, PM, APC} */
        case 0x91 ... 0x97:
        case 0x99:
                ret = parser_transition(parser, raw,
                                        STATE_GROUND, ACTION_EXECUTE);
                break;
        case 0x1b:                /* ESC */
                ret = parser_transition(parser, raw,
                                        STATE_ESC, ACTION_CLEAR);
                break;
        case 0x98:                /* SOS */
        case 0x9e:                /* PM */
        case 0x9f:                /* APC */
                ret = parser_transition(parser, raw,
                                        STATE_ST_IGNORE, ACTION_NONE);
                break;
        case 0x90:                /* DCS */
                ret = parser_transition(parser, raw,
                                        STATE_DCS_ENTRY, ACTION_DCS_START);
                break;
        case 0x9a:                /* SCI */
                ret = parser_transition(parser, raw,
                                        STATE_SCI, ACTION_CLEAR);
                break;
        case 0x9d:                /* OSC */
                ret = parser_transition(parser, raw,
                                        STATE_OSC_STRING, ACTION_OSC_START);
                break;
        case 0x9b:                /* CSI */
                ret = parser_transition(parser, raw,
                                        STATE_CSI_ENTRY, ACTION_CLEAR);
                break;
        default:
                ret = parser_feed_to_state(parser, raw);
                break;
        }

        if (G_UNLIKELY(ret < 0))
                *seq_out = NULL;
        else
                *seq_out = &parser->seq;

        return ret;
}

void vte_parser_reset(struct vte_parser *parser)
{
        /* const */ struct vte_seq *seq;
        vte_parser_feed(parser, &seq, 0x18 /* CAN */);
}
