/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 *
 * vte is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 */

#include "config.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>

//#include <linux/bitops.h>
//#include <linux/kernel.h>
//#include <linux/slab.h>
#include "parser.hh"

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

#define VTE_PARSER_ST_MAX (4096)

struct vte_parser {
        struct vte_seq seq;
        size_t st_alloc;
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
                return VTE_CMD_NULL;
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
        case 0x1f: /* DEL */
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
        case 0x9a: /* DECID */
                return VTE_CMD_DECID;
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

static int vte_charset_from_cmd(u32 raw, unsigned int flags, bool require_96)
{
#if 0
        static const struct {
                u32 raw;
                unsigned int flags;
        } charset_cmds[] = {
                /* 96-compat charsets */
                [VTE_CHARSET_ISO_LATIN1_SUPPLEMENTAL] =
                        { .raw = 'A', .flags = 0 },
                [VTE_CHARSET_ISO_LATIN2_SUPPLEMENTAL] =
                        { .raw = 'B', .flags = 0 },
                [VTE_CHARSET_ISO_LATIN5_SUPPLEMENTAL] =
                        { .raw = 'M', .flags = 0 },
                [VTE_CHARSET_ISO_GREEK_SUPPLEMENTAL] =
                        { .raw = 'F', .flags = 0 },
                [VTE_CHARSET_ISO_HEBREW_SUPPLEMENTAL] =
                        { .raw = 'H', .flags = 0 },
                [VTE_CHARSET_ISO_LATIN_CYRILLIC] =
                        { .raw = 'L', .flags = 0 },

                /* 94-compat charsets */
                [VTE_CHARSET_DEC_SPECIAL_GRAPHIC] =
                        { .raw = '0', .flags = 0 },
                [VTE_CHARSET_DEC_SUPPLEMENTAL] =
                        { .raw = '5', .flags = VTE_SEQ_FLAG_PERCENT },
                [VTE_CHARSET_DEC_TECHNICAL] =
                        { .raw = '>', .flags = 0 },
                [VTE_CHARSET_CYRILLIC_DEC] =
                        { .raw = '4', .flags = VTE_SEQ_FLAG_AND },
                [VTE_CHARSET_DUTCH_NRCS] =
                        { .raw = '4', .flags = 0 },
                [VTE_CHARSET_FINNISH_NRCS] =
                        { .raw = '5', .flags = 0 },
                [VTE_CHARSET_FRENCH_NRCS] =
                        { .raw = 'R', .flags = 0 },
                [VTE_CHARSET_FRENCH_CANADIAN_NRCS] =
                        { .raw = '9', .flags = 0 },
                [VTE_CHARSET_GERMAN_NRCS] =
                        { .raw = 'K', .flags = 0 },
                [VTE_CHARSET_GREEK_DEC] =
                        { .raw = '?', .flags = VTE_SEQ_FLAG_DQUOTE },
                [VTE_CHARSET_GREEK_NRCS] =
                        { .raw = '>', .flags = VTE_SEQ_FLAG_DQUOTE },
                [VTE_CHARSET_HEBREW_DEC] =
                        { .raw = '4', .flags = VTE_SEQ_FLAG_DQUOTE },
                [VTE_CHARSET_HEBREW_NRCS] =
                        { .raw = '=', .flags = VTE_SEQ_FLAG_PERCENT },
                [VTE_CHARSET_ITALIAN_NRCS] =
                        { .raw = 'Y', .flags = 0 },
                [VTE_CHARSET_NORWEGIAN_DANISH_NRCS] =
                        { .raw = '`', .flags = 0 },
                [VTE_CHARSET_PORTUGUESE_NRCS] =
                        { .raw = '6', .flags = VTE_SEQ_FLAG_PERCENT },
                [VTE_CHARSET_RUSSIAN_NRCS] =
                        { .raw = '5', .flags = VTE_SEQ_FLAG_AND },
                [VTE_CHARSET_SCS_NRCS] =
                        { .raw = '3', .flags = VTE_SEQ_FLAG_PERCENT },
                [VTE_CHARSET_SPANISH_NRCS] =
                        { .raw = 'Z', .flags = 0 },
                [VTE_CHARSET_SWEDISH_NRCS] =
                        { .raw = '7', .flags = 0 },
                [VTE_CHARSET_SWISS_NRCS] =
                        { .raw = '=', .flags = 0 },
                [VTE_CHARSET_TURKISH_DEC] =
                        { .raw = '0', .flags = VTE_SEQ_FLAG_PERCENT },
                [VTE_CHARSET_TURKISH_NRCS] =
                        { .raw = '2', .flags = VTE_SEQ_FLAG_PERCENT },

                /* special charsets */
                [VTE_CHARSET_USERPREF_SUPPLEMENTAL] =
                        { .raw = '<', .flags = 0 },

                /* secondary choices */
                [VTE_CHARSET_N + VTE_CHARSET_FINNISH_NRCS] =
                        { .raw = 'C', .flags = 0 },
                [VTE_CHARSET_N + VTE_CHARSET_FRENCH_NRCS] =
                        { .raw = 'f', .flags = 0 },
                [VTE_CHARSET_N + VTE_CHARSET_FRENCH_CANADIAN_NRCS] =
                        { .raw = 'Q', .flags = 0 },
                [VTE_CHARSET_N + VTE_CHARSET_NORWEGIAN_DANISH_NRCS] =
                        { .raw = 'E', .flags = 0 },
                [VTE_CHARSET_N + VTE_CHARSET_SWEDISH_NRCS] =
                        /* unused; conflicts with ISO_HEBREW */
                        { .raw = 'H', .flags = 0 },

                /* tertiary choices */
                [VTE_CHARSET_N + VTE_CHARSET_N +
                 VTE_CHARSET_NORWEGIAN_DANISH_NRCS] =
                        { .raw = '6', .flags = 0 },
        };
        size_t i, cs;

        /*
         * Secondary choice on SWEDISH_NRCS and primary choice on
         * ISO_HEBREW_SUPPLEMENTAL have a conflict: raw=="H", flags==0.
         * We always choose the ISO 96-compat set, which is what VT510 does.
         */

        for (i = 0; i < ARRAY_SIZE(charset_cmds); ++i) {
                if (charset_cmds[i].raw == raw &&
                    charset_cmds[i].flags == flags) {
                        cs = i;
                        while (cs >= VTE_CHARSET_N)
                                cs -= VTE_CHARSET_N;

                        if (!require_96 ||
                            cs < VTE_CHARSET_96_N ||
                            cs >= VTE_CHARSET_94_N)
                                return cs;
                }
        }
#endif
        return -ENOENT;
}

static unsigned int vte_parse_host_escape(const struct vte_seq *seq,
                                             unsigned int *cs_out)
{
        unsigned int t, flags;
        int cs;

        flags = seq->intermediates;
        t = VTE_SEQ_FLAG_POPEN | VTE_SEQ_FLAG_PCLOSE |
            VTE_SEQ_FLAG_MULT  | VTE_SEQ_FLAG_PLUS   |
            VTE_SEQ_FLAG_MINUS | VTE_SEQ_FLAG_DOT    |
            VTE_SEQ_FLAG_SLASH;

        if (hweight32(flags & t) == 1) {
                switch (flags & t) {
                case VTE_SEQ_FLAG_POPEN:
                case VTE_SEQ_FLAG_PCLOSE:
                case VTE_SEQ_FLAG_MULT:
                case VTE_SEQ_FLAG_PLUS:
                        cs = vte_charset_from_cmd(seq->terminator,
                                                     flags & ~t, false);
                        break;
                case VTE_SEQ_FLAG_MINUS:
                case VTE_SEQ_FLAG_DOT:
                case VTE_SEQ_FLAG_SLASH:
                        cs = vte_charset_from_cmd(seq->terminator,
                                                     flags & ~t, true);
                        break;
                default:
                        cs = -ENOENT;
                        break;
                }

                if (cs >= 0) {
                        if (cs_out)
                                *cs_out = cs;
                        return VTE_CMD_SCS;
                }

                /* looked like a charset-cmd but wasn't; continue */
        }

        switch (seq->terminator) {
        case '3':
                if (flags == VTE_SEQ_FLAG_HASH) /* DECDHL top-half */
                        return VTE_CMD_DECDHL_TH;
                break;
        case '4':
                if (flags == VTE_SEQ_FLAG_HASH) /* DECDHL bottom-half */
                        return VTE_CMD_DECDHL_BH;
                break;
        case '5':
                if (flags == VTE_SEQ_FLAG_HASH) /* DECSWL */
                        return VTE_CMD_DECSWL;
                break;
        case '6':
                if (flags == 0) /* DECBI */
                        return VTE_CMD_DECBI;
                else if (flags == VTE_SEQ_FLAG_HASH) /* DECDWL */
                        return VTE_CMD_DECDWL;
                break;
        case '7':
                if (flags == 0) /* DECSC */
                        return VTE_CMD_DECSC;
                break;
        case '8':
                if (flags == 0) /* DECRC */
                        return VTE_CMD_DECRC;
                else if (flags == VTE_SEQ_FLAG_HASH) /* DECALN */
                        return VTE_CMD_DECALN;
                break;
        case '9':
                if (flags == 0) /* DECFI */
                        return VTE_CMD_DECFI;
                break;
        case '<':
                if (flags == 0) /* DECANM */
                        return VTE_CMD_DECANM;
                break;
        case '=':
                if (flags == 0) /* DECKPAM */
                        return VTE_CMD_DECKPAM;
                break;
        case '>':
                if (flags == 0) /* DECKPNM */
                        return VTE_CMD_DECKPNM;
                break;
        case '@':
                if (flags == VTE_SEQ_FLAG_PERCENT) {
                        /* Select default char-set */
                        return VTE_CMD_XTERM_SDCS;
                }
                break;
        case 'D':
                if (flags == 0) /* IND */
                        return VTE_CMD_IND;
                break;
        case 'E':
                if (flags == 0) /* NEL */
                        return VTE_CMD_NEL;
                break;
        case 'F':
                if (flags == 0) /* Cursor to lower-left corner of screen */
                        return VTE_CMD_XTERM_CLLHP;
                else if (flags == VTE_SEQ_FLAG_SPACE) /* S7C1T */
                        return VTE_CMD_S7C1T;
                break;
        case 'G':
                if (flags == VTE_SEQ_FLAG_SPACE) { /* S8C1T */
                        return VTE_CMD_S8C1T;
                } else if (flags == VTE_SEQ_FLAG_PERCENT) {
                        /* Select UTF-8 character set */
                        return VTE_CMD_XTERM_SUCS;
                }
                break;
        case 'H':
                if (flags == 0) /* HTS */
                        return VTE_CMD_HTS;
                break;
        case 'L':
                if (flags == VTE_SEQ_FLAG_SPACE) {
                        /* Set ANSI conformance level 1 */
                        return VTE_CMD_XTERM_SACL1;
                }
                break;
        case 'M':
                if (flags == 0) { /* RI */
                        return VTE_CMD_RI;
                } else if (flags == VTE_SEQ_FLAG_SPACE) {
                        /* Set ANSI conformance level 2 */
                        return VTE_CMD_XTERM_SACL2;
                }
                break;
        case 'N':
                if (flags == 0) { /* SS2 */
                        return VTE_CMD_SS2;
                } else if (flags == VTE_SEQ_FLAG_SPACE) {
                        /* Set ANSI conformance level 3 */
                        return VTE_CMD_XTERM_SACL3;
                }
                break;
        case 'O':
                if (flags == 0) /* SS3 */
                        return VTE_CMD_SS3;
                break;
        case 'P':
                if (flags == 0) { /* DCS */
                        /* this is already handled by the state-machine */
                        break;
                }
                break;
        case 'V':
                if (flags == 0) /* SPA */
                        return VTE_CMD_SPA;
                break;
        case 'W':
                if (flags == 0) /* EPA */
                        return VTE_CMD_EPA;
                break;
        case 'X':
                if (flags == 0) { /* SOS */
                        /* this is already handled by the state-machine */
                        break;
                }
                break;
        case 'Z':
                if (flags == 0) /* DECID */
                        return VTE_CMD_DECID;
                break;
        case '[':
                if (flags == 0) { /* CSI */
                        /* this is already handled by the state-machine */
                        break;
                }
                break;
        case '\\':
                if (flags == 0) /* ST */
                        return VTE_CMD_ST;
                break;
        case ']':
                if (flags == 0) { /* OSC */
                        /* this is already handled by the state-machine */
                        break;
                }
                break;
        case '^':
                if (flags == 0) { /* PM */
                        /* this is already handled by the state-machine */
                        break;
                }
                break;
        case '_':
                if (flags == 0) { /* APC */
                        /* this is already handled by the state-machine */
                        break;
                }
                break;
        case 'c':
                if (flags == 0) /* RIS */
                        return VTE_CMD_RIS;
                break;
        case 'l':
                if (flags == 0) /* Memory lock */
                        return VTE_CMD_XTERM_MLHP;
                break;
        case 'm':
                if (flags == 0) /* Memory unlock */
                        return VTE_CMD_XTERM_MUHP;
                break;
        case 'n':
                if (flags == 0) /* LS2 */
                        return VTE_CMD_LS2;
                break;
        case 'o':
                if (flags == 0) /* LS3 */
                        return VTE_CMD_LS3;
                break;
        case '|':
                if (flags == 0) /* LS3R */
                        return VTE_CMD_LS3R;
                break;
        case '}':
                if (flags == 0) /* LS2R */
                        return VTE_CMD_LS2R;
                break;
        case '~':
                if (flags == 0) /* LS1R */
                        return VTE_CMD_LS1R;
                break;
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
                break;
        case 'B':
                if (flags == 0) /* CUD */
                        return VTE_CMD_CUD;
                break;
        case 'b':
                if (flags == 0) /* REP */
                        return VTE_CMD_REP;
                break;
        case 'C':
                if (flags == 0) /* CUF */
                        return VTE_CMD_CUF;
                break;
        case 'c':
                if (flags == 0) /* DA1 */
                        return VTE_CMD_DA1;
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
                if (flags == 0) /* SM ANSI */
                        return VTE_CMD_SM_ANSI;
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
                if (flags == 0) /* RM ANSI */
                        return VTE_CMD_RM_ANSI;
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
                if (flags == 0) /* DSR ANSI */
                        return VTE_CMD_DSR_ANSI;
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
                else if (flags == VTE_SEQ_FLAG_CASH) /* DECRQM-ANSI */
                        return VTE_CMD_DECRQM_ANSI;
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
                        if (seq->n_args <= 1) /* XTERM RPM */
                                return VTE_CMD_XTERM_RPM;
                        else if (seq->n_args >= 2) /* DECPCTERM */
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
                        if (seq->n_args < 5) {
                                /* SD */
                                return VTE_CMD_SD;
                        } else if (seq->n_args >= 5) {
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
                } else if (seq->args[0] == 1 && flags == VTE_SEQ_FLAG_CASH) {
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
                if (seq->args[0] == 5 && flags == VTE_SEQ_FLAG_WHAT) {
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

/*
 * State Machine
 * This parser controls the parser-state and returns any detected sequence to
 * the caller. The parser is based on this state-diagram from Paul Williams:
 *   http://vt100.net/emu/
 * It was written from scratch and extended where needed.
 * This parser is fully compatible up to the vt500 series. We expect UCS-4 as
 * input. It's the callers responsibility to do any UTF-8 parsing.
 */

enum parser_state {
        STATE_NONE,                /* placeholder */
        STATE_GROUND,                /* initial state and ground */
        STATE_ESC,                /* ESC sequence was started */
        STATE_ESC_INT,                /* intermediate escape characters */
        STATE_CSI_ENTRY,        /* starting CSI sequence */
        STATE_CSI_PARAM,        /* CSI parameters */
        STATE_CSI_INT,                /* intermediate CSI characters */
        STATE_CSI_IGNORE,        /* CSI error; ignore this CSI sequence */
        STATE_DCS_ENTRY,        /* starting DCS sequence */
        STATE_DCS_PARAM,        /* DCS parameters */
        STATE_DCS_INT,                /* intermediate DCS characters */
        STATE_DCS_PASS,                /* DCS data passthrough */
        STATE_DCS_IGNORE,        /* DCS error; ignore this DCS sequence */
        STATE_OSC_STRING,        /* parsing OSC sequence */
        STATE_ST_IGNORE,        /* unimplemented seq; ignore until ST */
        STATE_N,
};

enum parser_action {
        ACTION_NONE,                /* placeholder */
        ACTION_CLEAR,                /* clear parameters */
        ACTION_IGNORE,                /* ignore the character entirely */
        ACTION_PRINT,                /* print the character on the console */
        ACTION_EXECUTE,                /* execute single control character (C0/C1) */
        ACTION_COLLECT,                /* collect intermediate character */
        ACTION_PARAM,                /* collect parameter character */
        ACTION_ESC_DISPATCH,        /* dispatch escape sequence */
        ACTION_CSI_DISPATCH,        /* dispatch csi sequence */
        ACTION_DCS_START,        /* start of DCS data */
        ACTION_DCS_COLLECT,        /* collect DCS data */
        ACTION_DCS_CONSUME,        /* consume DCS terminator */
        ACTION_DCS_DISPATCH,        /* dispatch dcs sequence */
        ACTION_OSC_START,        /* start of OSC data */
        ACTION_OSC_COLLECT,        /* collect OSC data */
        ACTION_OSC_CONSUME,        /* consume OSC terminator */
        ACTION_OSC_DISPATCH,        /* dispatch osc sequence */
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

        parser->st_alloc = 64;
        parser->seq.st = (char*)kzalloc(parser->st_alloc + 1, GFP_KERNEL);
        if (!parser->seq.st) {
                kfree(parser);
                return -ENOMEM;
        }

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

        kfree(parser->seq.st);
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
        for (i = 0; i < VTE_PARSER_ARG_MAX; ++i)
                parser->seq.args[i] = -1;

        parser->seq.n_st = 0;
        parser->seq.st[0] = 0;
}

static int parser_ignore(struct vte_parser *parser, u32 raw)
{
        parser_clear(parser);
        parser->seq.type = VTE_SEQ_IGNORE;
        parser->seq.command = VTE_CMD_NONE;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;

        return parser->seq.type;
}

static int parser_print(struct vte_parser *parser, u32 raw)
{
        parser_clear(parser);
        parser->seq.type = VTE_SEQ_GRAPHIC;
        parser->seq.command = VTE_CMD_GRAPHIC;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;

        return parser->seq.type;
}

static int parser_execute(struct vte_parser *parser, u32 raw)
{
        parser_clear(parser);
        parser->seq.type = VTE_SEQ_CONTROL;
        parser->seq.command = VTE_CMD_GRAPHIC;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;
        parser->seq.command = vte_parse_host_control(&parser->seq);

        return parser->seq.type;
}

static void parser_collect(struct vte_parser *parser, u32 raw)
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

static void parser_param(struct vte_parser *parser, u32 raw)
{
        if (raw == ';') {
                if (parser->seq.n_args < VTE_PARSER_ARG_MAX)
                        ++parser->seq.n_args;

                return;
        }

        if (parser->seq.n_args >= VTE_PARSER_ARG_MAX)
                return;

        if (raw >= '0' && raw <= '9') {
                auto value = parser->seq.args[parser->seq.n_args];
                if (value < 0)
                        value = 0;
                value = value * 10 + raw - '0';

                /*
                 * VT510 tells us to clamp all values to [0, 9999], however, it
                 * also allows commands with values up to 2^15-1. We simply use
                 * 2^16 as maximum here to be compatible to all commands, but
                 * avoid overflows in any calculations.
                 */
                if (value > 0xffff)
                        value = 0xffff;

                parser->seq.args[parser->seq.n_args] = value;
        }
}

static int parser_esc(struct vte_parser *parser, u32 raw)
{
        parser->seq.type = VTE_SEQ_ESCAPE;
        parser->seq.command = VTE_CMD_NONE;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;
        parser->seq.command = vte_parse_host_escape(&parser->seq,
                                                       &parser->seq.charset);

        return parser->seq.type;
}

static int parser_csi(struct vte_parser *parser, u32 raw)
{
        /* parser->seq is cleared during CSI-ENTER state, thus there's no need
         * to clear invalid fields here. */

        if (parser->seq.n_args < VTE_PARSER_ARG_MAX) {
                if (parser->seq.n_args > 0 ||
                    parser->seq.args[parser->seq.n_args] >= 0)
                        ++parser->seq.n_args;
        }

        parser->seq.type = VTE_SEQ_CSI;
        parser->seq.command = VTE_CMD_NONE;
        parser->seq.terminator = raw;
        parser->seq.charset = VTE_CHARSET_NONE;
        parser->seq.command = vte_parse_host_csi(&parser->seq);

        return parser->seq.type;
}

/* perform state transition and dispatch related actions */
static int parser_transition(struct vte_parser *parser,
                             u32 raw,
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
                /* not implemented */
                return VTE_SEQ_NONE;
        case ACTION_DCS_COLLECT:
                /* not implemented */
                return VTE_SEQ_NONE;
        case ACTION_DCS_CONSUME:
                /* not implemented */
                return VTE_SEQ_NONE;
        case ACTION_DCS_DISPATCH:
                /* not implemented */
                return VTE_SEQ_NONE;
        case ACTION_OSC_START:
                /* not implemented */
                return VTE_SEQ_NONE;
        case ACTION_OSC_COLLECT:
                /* not implemented */
                return VTE_SEQ_NONE;
        case ACTION_OSC_CONSUME:
                /* not implemented */
                return VTE_SEQ_NONE;
        case ACTION_OSC_DISPATCH:
                /* not implemented */
                return VTE_SEQ_NONE;
        default:
                WARN(1, "invalid vte-parser action");
                return VTE_SEQ_NONE;
        }
}

static int parser_feed_to_state(struct vte_parser *parser, u32 raw)
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
                case 0x51 ... 0x57:        /* { 'P', 'X', '[', ']', '^', '_' } */
                case 0x59 ... 0x5a:
                case 0x5c:
                case 0x60 ... 0x7e:
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_ESC_DISPATCH);
                case 0x50:                /* 'P' */
                        return parser_transition(parser, raw, STATE_DCS_ENTRY,
                                                 ACTION_CLEAR);
                case 0x5b:                /* '[' */
                        return parser_transition(parser, raw, STATE_CSI_ENTRY,
                                                 ACTION_CLEAR);
                case 0x5d:                /* ']' */
                        return parser_transition(parser, raw, STATE_OSC_STRING,
                                                 ACTION_CLEAR);
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
                case 0x3a:                /* ':' */
                        return parser_transition(parser, raw, STATE_CSI_IGNORE,
                                                 ACTION_NONE);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                case 0x3b:                /* ';' */
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
                case 0x3b:                /* ';' */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_PARAM);
                case 0x3a:                /* ':' */
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
                case 0x3a:                /* ':' */
                        return parser_transition(parser, raw, STATE_DCS_IGNORE,
                                                 ACTION_NONE);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                case 0x3b:                /* ';' */
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
                case 0x3b:                /* ';' */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_PARAM);
                case 0x3a:                /* ':' */
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
        }

        WARN(1, "bad vte-parser state");
        return -EINVAL;
}

int vte_parser_feed(struct vte_parser *parser,
                       const struct vte_seq **seq_out,
                       u32 raw)
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
        case 0x80 ... 0x8f:        /* C1 \ {DCS, SOS, CSI, ST, OSC, PM, APC} */
        case 0x91 ... 0x97:
        case 0x99 ... 0x9a:
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
                                        STATE_DCS_ENTRY, ACTION_CLEAR);
                break;
        case 0x9d:                /* OSC */
                ret = parser_transition(parser, raw,
                                        STATE_OSC_STRING, ACTION_CLEAR);
                break;
        case 0x9b:                /* CSI */
                ret = parser_transition(parser, raw,
                                        STATE_CSI_ENTRY, ACTION_CLEAR);
                break;
        default:
                ret = parser_feed_to_state(parser, raw);
                break;
        }

        if (ret <= 0)
                *seq_out = NULL;
        else
                *seq_out = &parser->seq;

        return ret;
}
