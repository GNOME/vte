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

/*
 * Intermediates (and, for CSI/DCS, the optional parameter character) are
 * stored efficiently in an unsigned int. Intermediates can be 2/00..2/15,
 * plus one value for 'no intermediate'; together that fits into 5 bits.
 * Parameter character can be 'no parameter character', or one from
 * 3/12..3/15; that fits into 3 bits.
 *
 * In @seq.intermediates, the nth intermediates is stored with shift n * 5,
 * plus (for CSI/DCS) an additional shift of 3 for the parameter character
 * which is stored at bits 0..2.
 *
 * VTE_SEQ_PARAMETER(u) extracts the parameter character
 *   of a CSI or DCS sequence
 * VTE_SEQ_REMOVE_PARAMETER(u) extracts the intermediates
 *   of a CSI or DCS sequence
 * VTE_SEQ_INTERMEDIATE(u) extracts the first intermediate from an
 *   intermediates value (for CSI/DCS, that must be without parameter
 *   character, see VTE_SEQ_REMOVE_PARAMETER)
 * VTE_SEQ_REMOVE_INTERMEDIATE(u) extracts the remaining intermediates
 *   after the first one; use VTE_SEQ_INTERMEDIATE on its return value
 *   to extract the 2nd intermediate, and so on
 */

#define VTE_SEQ_PARAMETER_BITS         (3)
#define VTE_SEQ_INTERMEDIATE_BITS      (5)
#define VTE_SEQ_INTERMEDIATE_MASK      ((1U << VTE_SEQ_INTERMEDIATE_BITS) - 1U)
#define VTE_SEQ_PARAMETER_MASK         ((1U << VTE_SEQ_PARAMETER_BITS) - 1U)
#define VTE_SEQ_PARAMETER(u)           ((u) & VTE_SEQ_PARAMETER_MASK)
#define VTE_SEQ_REMOVE_PARAMETER(u)    ((u) >> VTE_SEQ_PARAMETER_BITS)
#define VTE_SEQ_INTERMEDIATE(u)        ((u) & VTE_SEQ_INTERMEDIATE_MASK)
#define VTE_SEQ_REMOVE_INTERMEDIATE(u) ((u) >> VTE_SEQ_INTERMEDIATE_BITS)
#define VTE_MAKE_CHARSET(c,s)          ((c) | ((s) << VTE_CHARSET_SLOT_OFFSET))

/*
 * _VTE_SEQ_CODE_ESC(final, intermediates):
 *
 * Make a value combining the final character and the intermediates,
 * to be used to match a sequence against known sequences.
 *
 * Since this is only used with NONE or HASH as first intermediate,
 * we can reduce the size of the lookup table by slashing the least
 * significant bit off.
 *
 * Final characters is 3/0..7/14, needing 7 bits.
 */
#define _VTE_SEQ_CODE_ESC(f,i) (((f) - 0x30) | ((i) >> 1) << 7)

/*
 * _VTE_SEQ_CODE_COMBINE(parameter, intermediates)
 *
 * Combines intermediates and the parameter character into one
 * value to be used when matching a sequence against known sequences.
 */
#define _VTE_SEQ_CODE_COMBINE(p,i) ((p) | ((i) << VTE_SEQ_PARAMETER_BITS))

/*
 * _VTE_SEQ_CODE(final, intermediates):
 *
 * Make a value combining the final character and the intermediates,
 * to be used to match a sequence against known sequences. Used for
 * CSI and DCS sequences; use _VTE_SEQ_CODE_COMBINE to combine
 * parameter and intermediates into one to pass as 2nd argument here.
 *
 * Final character is 4/0..7/14, needing 6 bits.
 */
#define _VTE_SEQ_CODE(f,i) (((f) - 0x40) | ((i) << 6))

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
                                         unsigned int intermediates)
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94)))
                        return charset_graphic_94[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE:
                return VTE_CHARSET_DRCS;

        case VTE_SEQ_INTERMEDIATE_BANG:
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && (raw < 0x40 + G_N_ELEMENTS(charset_graphic_94_with_2_1)))
                        return charset_graphic_94_with_2_1[raw - 0x40];
                break;

        case VTE_SEQ_INTERMEDIATE_DQUOTE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_2)))
                        return charset_graphic_94_with_2_2[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_HASH:
        case VTE_SEQ_INTERMEDIATE_CASH:
                break;

        case VTE_SEQ_INTERMEDIATE_PERCENT:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_5)))
                        return charset_graphic_94_with_2_5[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_AND:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_with_2_6)))
                        return charset_graphic_94_with_2_6[raw - 0x30];
                break;
        }

        return VTE_CHARSET_NONE;
}

static unsigned int vte_parse_charset_94_n(uint32_t raw,
                                           unsigned int intermediates)
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_94_n)))
                        return charset_graphic_94_n[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE:
                return VTE_CHARSET_DRCS;
        }

        return VTE_CHARSET_NONE;
}

static unsigned int vte_parse_charset_96(uint32_t raw,
                                         unsigned int intermediates)
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:
                if (remaining_intermediates == 0 &&
                    raw < (0x30 + G_N_ELEMENTS(charset_graphic_96)))
                        return charset_graphic_96[raw - 0x30];
                break;

        case VTE_SEQ_INTERMEDIATE_SPACE:
                return VTE_CHARSET_DRCS;
        }

        return VTE_CHARSET_NONE;
}

static unsigned int vte_parse_charset_96_n(uint32_t raw,
                                           unsigned int intermediates)
{
        if (VTE_SEQ_INTERMEDIATE(intermediates) == VTE_SEQ_INTERMEDIATE_SPACE)
                return VTE_CHARSET_DRCS;

        return VTE_CHARSET_NONE;
}

static unsigned int vte_parse_charset_ocs(uint32_t raw,
                                          unsigned int intermediates)
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_NONE:  /* OCS with standard return */
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_ocs_with_return)))
                        return charset_ocs_with_return[raw - 0x40];
                break;

        case VTE_SEQ_INTERMEDIATE_SLASH: /* OCS without standard return */
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_ocs_without_return)))
                        return charset_ocs_without_return[raw - 0x40];
                break;
        }

        return VTE_CHARSET_NONE;
}

static unsigned int vte_parse_charset_control(uint32_t raw,
                                              unsigned int intermediates)
{
        assert (raw >= 0x30 && raw < 0x7f);

        unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);

        switch (VTE_SEQ_INTERMEDIATE(intermediates)) {
        case VTE_SEQ_INTERMEDIATE_BANG: /* C0 controls */
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_control_c0)))
                        return charset_control_c0[raw - 0x40];
                break;

        case VTE_SEQ_INTERMEDIATE_DQUOTE: /* C1 controls */
                if (remaining_intermediates == 0 &&
                    raw >= 0x40 && raw < (0x40 + G_N_ELEMENTS(charset_control_c1)))
                        return charset_control_c1[raw - 0x40];
                break;
        }

        return VTE_CHARSET_NONE;
}

static unsigned int vte_parse_host_escape(const struct vte_seq *seq,
                                          unsigned int *cs_out)
{
        unsigned int intermediates = seq->intermediates;
        unsigned int intermediate0 = VTE_SEQ_INTERMEDIATE(intermediates);

        /* Switch on the first intermediate */
        switch (intermediate0) {
        case VTE_SEQ_INTERMEDIATE_NONE:
        case VTE_SEQ_INTERMEDIATE_HASH: {  /* Single control functions */
                switch (_VTE_SEQ_CODE_ESC(seq->terminator, intermediates)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i) \
                        case _VTE_SEQ_CODE_ESC(f, VTE_SEQ_INTERMEDIATE_##i): return VTE_CMD_##cmd;
#include "parser-esc.hh"
#undef _VTE_SEQ
                default: return VTE_CMD_NONE;
                }
                break;
        }

        case VTE_SEQ_INTERMEDIATE_SPACE:   /* Announce code structure */
                if (VTE_SEQ_REMOVE_INTERMEDIATE(intermediates) == 0)
                        return VTE_CMD_ACS;
                break;

        case VTE_SEQ_INTERMEDIATE_BANG:    /* C0-designate */
        case VTE_SEQ_INTERMEDIATE_DQUOTE:  /* C1-designate */
                *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_control(seq->terminator, intermediates),
                                           intermediate0 - VTE_SEQ_INTERMEDIATE_BANG);
                return VTE_CMD_CnD;

        case VTE_SEQ_INTERMEDIATE_CASH: {  /* Designate multi-byte character sets */
                unsigned int remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(intermediates);
                unsigned int intermediate1 = VTE_SEQ_INTERMEDIATE(remaining_intermediates);
                remaining_intermediates = VTE_SEQ_REMOVE_INTERMEDIATE(remaining_intermediates);

                /* Check the 2nd intermediate */
                switch (intermediate1) {
                case VTE_SEQ_INTERMEDIATE_NONE:
                        /* For compatibility with an earlier version of ISO-2022,
                         * ESC 2/4 4/0, ESC 2/4 4/1 and ESC 2/4 4/2 designate G0
                         * sets (i.e., without the 2/8 as 2nd intermediate byte).
                         */
                        switch (seq->terminator) {
                        case '@':
                        case 'A':
                        case 'B': /* G0-designate multibyte charset */
                                *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_94_n(seq->terminator,
                                                                                  remaining_intermediates),
                                                           0);
                                return VTE_CMD_GnDMm;
                        }
                        break;

                case VTE_SEQ_INTERMEDIATE_POPEN:  /* G0-designate 94^n-set */
                case VTE_SEQ_INTERMEDIATE_PCLOSE: /* G1-designate 94^n-set */
                case VTE_SEQ_INTERMEDIATE_MULT:   /* G2-designate 94^n-set */
                case VTE_SEQ_INTERMEDIATE_PLUS:   /* G3-designate 94^n-set */
                        *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_94_n(seq->terminator,
                                                                          remaining_intermediates),
                                                   intermediate1 - VTE_SEQ_INTERMEDIATE_POPEN);
                        return VTE_CMD_GnDMm;

                case VTE_SEQ_INTERMEDIATE_COMMA:  /* Reserved for future standardisation */
                        break;

                case VTE_SEQ_INTERMEDIATE_MINUS:  /* G1-designate 96^n-set */
                case VTE_SEQ_INTERMEDIATE_DOT:    /* G2-designate 96^n-set */
                case VTE_SEQ_INTERMEDIATE_SLASH:  /* G3-designate 96^n-set */
                        *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_96_n(seq->terminator,
                                                                          remaining_intermediates),
                                                   intermediate1 - VTE_SEQ_INTERMEDIATE_COMMA);
                        return VTE_CMD_GnDMm;
                }
                break;
        }

        case VTE_SEQ_INTERMEDIATE_PERCENT: /* Designate other coding system */
                *cs_out = vte_parse_charset_ocs(seq->terminator,
                                                VTE_SEQ_REMOVE_INTERMEDIATE(intermediates));
                return VTE_CMD_DOCS;

        case VTE_SEQ_INTERMEDIATE_AND:     /* Identify revised registration */
                if (VTE_SEQ_REMOVE_INTERMEDIATE(intermediates) == 0)
                        return VTE_CMD_IRR;
                break;

        case VTE_SEQ_INTERMEDIATE_SQUOTE:  /* Reserved for future standardisation */
                break;

        case VTE_SEQ_INTERMEDIATE_POPEN:   /* G0-designate 94-set */
        case VTE_SEQ_INTERMEDIATE_PCLOSE:  /* G1-designate 94-set */
        case VTE_SEQ_INTERMEDIATE_MULT:    /* G2-designate 94-set */
        case VTE_SEQ_INTERMEDIATE_PLUS:    /* G3-designate 94-set */
                *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_94(seq->terminator,
                                                                VTE_SEQ_REMOVE_INTERMEDIATE(intermediates)),
                                           intermediate0 - VTE_SEQ_INTERMEDIATE_POPEN);
                return VTE_CMD_GnDm;

        case VTE_SEQ_INTERMEDIATE_COMMA:   /* Reserved for future standardisation */
                break;

        case VTE_SEQ_INTERMEDIATE_MINUS:   /* G1-designate 96-set */
        case VTE_SEQ_INTERMEDIATE_DOT:     /* G2-designate 96-set */
        case VTE_SEQ_INTERMEDIATE_SLASH:   /* G3-designate 96-set */
                *cs_out = VTE_MAKE_CHARSET(vte_parse_charset_96(seq->terminator,
                                                                VTE_SEQ_REMOVE_INTERMEDIATE(intermediates)),
                                           intermediate0 - VTE_SEQ_INTERMEDIATE_COMMA);
                return VTE_CMD_GnDm;
        }

        return VTE_CMD_NONE;
}

static unsigned int vte_parse_host_csi(const struct vte_seq *seq)
{
        switch (_VTE_SEQ_CODE(seq->terminator, seq->intermediates)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i) \
                case _VTE_SEQ_CODE(f, _VTE_SEQ_CODE_COMBINE(VTE_SEQ_PARAMETER_##p, VTE_SEQ_INTERMEDIATE_##i)): return VTE_CMD_##cmd;
#include "parser-csi.hh"
#undef _VTE_SEQ
        default: return VTE_CMD_NONE;
        }
}

static unsigned int vte_parse_host_dcs(const struct vte_seq *seq)
{
        switch (_VTE_SEQ_CODE(seq->terminator, seq->intermediates)) {
#define _VTE_SEQ(cmd,type,f,p,ni,i) \
                case _VTE_SEQ_CODE(f, _VTE_SEQ_CODE_COMBINE(VTE_SEQ_PARAMETER_##p, VTE_SEQ_INTERMEDIATE_##i)): return VTE_CMD_##cmd;
#include "parser-dcs.hh"
#undef _VTE_SEQ
        default: return VTE_CMD_NONE;
        }
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
        STATE_DCS_PASS_ESC,     /* ESC after DCS which may be ESC \ aka C0 ST */
        STATE_OSC_STRING_ESC,   /* ESC after OSC which may be ESC \ aka C0 ST */
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
        ACTION_NONE,              /* placeholder */
        ACTION_CLEAR,             /* clear parameters */
        ACTION_IGNORE,            /* ignore the character entirely */
        ACTION_PRINT,             /* print the character on the console */
        ACTION_EXECUTE,           /* execute single control character (C0/C1) */
        ACTION_COLLECT_ESC,       /* collect intermediate character of ESCAPE sequence */
        ACTION_COLLECT_CSI,       /* collect intermediate character of CSI or DCS sequence */
        ACTION_COLLECT_PARAMETER, /* collect parameter character of CSI or DCS sequence */
        ACTION_PARAM,             /* collect parameter character 0..9 */
        ACTION_FINISH_PARAM,      /* finish collecting a parameter */
        ACTION_FINISH_SUBPARAM,   /* finish collecting a subparameter */
        ACTION_ESC_DISPATCH,      /* dispatch escape sequence */
        ACTION_CSI_DISPATCH,      /* dispatch CSI sequence */
        ACTION_DCS_START,         /* start of DCS data */
        ACTION_DCS_CONSUME,       /* consume DCS terminator */
        ACTION_DCS_COLLECT,       /* collect DCS data */
        ACTION_DCS_DISPATCH,      /* dispatch DCS sequence */
        ACTION_OSC_START,         /* clear and clear string data */
        ACTION_OSC_COLLECT,       /* collect OSC data */
        ACTION_OSC_DISPATCH,      /* dispatch OSC sequence */
        ACTION_SCI_DISPATCH,      /* dispatch SCI sequence */
        ACTION_N,

        ACTION_COLLECT_DCS = ACTION_COLLECT_CSI, /* alias */
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
        parser->seq.n_intermediates = 0;
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

static void parser_collect_esc(struct vte_parser *parser, uint32_t raw)
{
        assert(raw >= 0x20 && raw <= 0x2f);

        /* ESCAPE sequences only have intermediates or 2/0..2/15, so there's no
         * need for the extra shift as below for CSI/DCS sequences
         */
        parser->seq.intermediates |= (VTE_SEQ_MAKE_INTERMEDIATE(raw) << (VTE_SEQ_INTERMEDIATE_BITS * parser->seq.n_intermediates++));
}

static void parser_collect_csi(struct vte_parser *parser, uint32_t raw)
{
        assert(raw >= 0x20 && raw <= 0x2f);

        /* In addition to 2/0..2/15 intermediates, CSI/DCS sequence
         * can also have one parameter byte 3/12..3/15 at the
         * start of the parameters (see parser_collect_parameter below);
         * that's what the extra shift is for.
         */
        parser->seq.intermediates |= (VTE_SEQ_MAKE_INTERMEDIATE(raw) << (VTE_SEQ_PARAMETER_BITS +
                                                                         VTE_SEQ_INTERMEDIATE_BITS * parser->seq.n_intermediates++));
}

static void parser_collect_parameter(struct vte_parser *parser, uint32_t raw)
{
        assert(raw >= 0x3c && raw <= 0x3f);

        /* CSI/DCS may optionally have one parameter byte from 3/12..3/15
         * at the start of the parameters; we put that into the lowest
         * part of @seq.intermediates.
         * Note that there can only be *one* such byte; the state machine
         * already enforces that, so we do not need any additional checks
         * here.
         */
        parser->seq.intermediates |= VTE_SEQ_MAKE_PARAMETER(raw);
}

static void parser_finish_param(struct vte_parser *parser, uint32_t raw)
{
        if (parser->seq.n_args < VTE_PARSER_ARG_MAX) {
                vte_seq_arg_finish(&parser->seq.args[parser->seq.n_args], false);
                ++parser->seq.n_args;
                ++parser->seq.n_final_args;
        }
}

static void parser_finish_subparam(struct vte_parser *parser, uint32_t raw)
{
        if (parser->seq.n_args < VTE_PARSER_ARG_MAX) {
                vte_seq_arg_finish(&parser->seq.args[parser->seq.n_args], true);
                ++parser->seq.n_args;
        }
}

static void parser_param(struct vte_parser *parser, uint32_t raw)
{
        /* assert(raw >= '0' && raw <= '9'); */

        if (parser->seq.n_args >= VTE_PARSER_ARG_MAX)
                return;

        vte_seq_arg_push(&parser->seq.args[parser->seq.n_args], raw);
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
        case ACTION_COLLECT_ESC:
                parser_collect_esc(parser, raw);
                return VTE_SEQ_NONE;
        case ACTION_COLLECT_CSI:
                parser_collect_csi(parser, raw);
                return VTE_SEQ_NONE;
        case ACTION_COLLECT_PARAMETER:
                parser_collect_parameter(parser, raw);
                return VTE_SEQ_NONE;
        case ACTION_PARAM:
                parser_param(parser, raw);
                return VTE_SEQ_NONE;
        case ACTION_FINISH_PARAM:
                parser_finish_param(parser, raw);
                return VTE_SEQ_NONE;
        case ACTION_FINISH_SUBPARAM:
                parser_finish_subparam(parser, raw);
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
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                case 0x80 ... 0x9f:        /* C1 */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_PRINT);

        case STATE_DCS_PASS_ESC:
        case STATE_OSC_STRING_ESC:
                if (raw == 0x5c /* '\' */) {
                        switch (parser->state) {
                        case STATE_DCS_PASS_ESC:
                                return parser_transition(parser, raw, STATE_GROUND,
                                                         ACTION_DCS_DISPATCH);
                        case STATE_OSC_STRING_ESC:
                                return parser_transition(parser, raw, STATE_GROUND,
                                                         ACTION_OSC_DISPATCH);
                        }
                }

                /* Do the deferred clear and fallthrough to STATE_ESC */
                parser_transition(parser, 0x1b /* ESC */, STATE_ESC,
                                  ACTION_CLEAR);
                /* fallthrough */
        case STATE_ESC:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_ESC_INT,
                                                 ACTION_COLLECT_ESC);
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
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw, STATE_GROUND,
                                         ACTION_IGNORE);
        case STATE_ESC_INT:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_COLLECT_ESC);
                case 0x30 ... 0x7e:        /* ['0' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_ESC_DISPATCH);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw, STATE_GROUND,
                                         ACTION_IGNORE);
        case STATE_CSI_ENTRY:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_CSI_INT,
                                                 ACTION_COLLECT_CSI);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                        return parser_transition(parser, raw, STATE_CSI_PARAM,
                                                 ACTION_PARAM);
                case 0x3a:                 /* ':' */
                        return parser_transition(parser, raw, STATE_CSI_PARAM,
                                                 ACTION_FINISH_SUBPARAM);
                case 0x3b:                 /* ';' */
                        return parser_transition(parser, raw, STATE_CSI_PARAM,
                                                 ACTION_FINISH_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition(parser, raw, STATE_CSI_PARAM,
                                                 ACTION_COLLECT_PARAMETER);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_CSI_DISPATCH);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_CSI_IGNORE, ACTION_NONE);
        case STATE_CSI_PARAM:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_CSI_INT,
                                                 ACTION_COLLECT_CSI);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_PARAM);
                case 0x3a:                 /* ':' */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_FINISH_SUBPARAM);
                case 0x3b:                 /* ';' */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_FINISH_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition(parser, raw, STATE_CSI_IGNORE,
                                                 ACTION_NONE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_CSI_DISPATCH);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_CSI_IGNORE, ACTION_NONE);
        case STATE_CSI_INT:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_COLLECT_CSI);
                case 0x30 ... 0x3f:        /* ['0' - '?'] */
                        return parser_transition(parser, raw, STATE_CSI_IGNORE,
                                                 ACTION_NONE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_CSI_DISPATCH);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_CSI_IGNORE, ACTION_NONE);
        case STATE_CSI_IGNORE:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_EXECUTE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x20 ... 0x3f:        /* [' ' - '?'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_NONE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_NONE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_NONE);
        case STATE_DCS_ENTRY:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ ESC */
                case 0x1c ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_DCS_INT,
                                                 ACTION_COLLECT_DCS);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                        return parser_transition(parser, raw, STATE_DCS_PARAM,
                                                 ACTION_PARAM);
                case 0x3a:                 /* ':' */
                        return parser_transition(parser, raw, STATE_DCS_PARAM,
                                                 ACTION_FINISH_SUBPARAM);
                case 0x3b:                 /* ';' */
                        return parser_transition(parser, raw, STATE_DCS_PARAM,
                                                 ACTION_FINISH_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition(parser, raw, STATE_DCS_PARAM,
                                                 ACTION_COLLECT_PARAMETER);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_DCS_PASS,
                                                 ACTION_DCS_CONSUME);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_DCS_PASS, ACTION_DCS_CONSUME);
        case STATE_DCS_PARAM:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_DCS_INT,
                                                 ACTION_COLLECT_DCS);
                case 0x30 ... 0x39:        /* ['0' - '9'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_PARAM);
                case 0x3a:                 /* ':' */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_FINISH_SUBPARAM);
                case 0x3b:                 /* ';' */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_FINISH_PARAM);
                case 0x3c ... 0x3f:        /* ['<' - '?'] */
                        return parser_transition(parser, raw, STATE_DCS_IGNORE,
                                                 ACTION_NONE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_DCS_PASS,
                                                 ACTION_DCS_CONSUME);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_DCS_PASS, ACTION_DCS_CONSUME);
        case STATE_DCS_INT:
                switch (raw) {
                case 0x00 ... 0x1a:        /* C0 \ { ESC } */
                case 0x1c ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_IGNORE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x20 ... 0x2f:        /* [' ' - '\'] */
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_COLLECT_DCS);
                case 0x30 ... 0x3f:        /* ['0' - '?'] */
                        return parser_transition(parser, raw, STATE_DCS_IGNORE,
                                                 ACTION_NONE);
                case 0x40 ... 0x7e:        /* ['@' - '~'] */
                        return parser_transition(parser, raw, STATE_DCS_PASS,
                                                 ACTION_DCS_CONSUME);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_DCS_PASS, ACTION_DCS_CONSUME);
        case STATE_DCS_PASS:
                switch (raw) {
                case 0x00 ... 0x1a:        /* ASCII \ { ESC } */
                case 0x1c ... 0x7f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_DCS_COLLECT);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_DCS_PASS_ESC,
                                                 ACTION_NONE);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_DCS_DISPATCH);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_DCS_COLLECT);
        case STATE_DCS_IGNORE:
                switch (raw) {
                case 0x00 ... 0x1a:        /* ASCII \ { ESC } */
                case 0x1c ... 0x7f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_NONE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_NONE);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_NONE);
        case STATE_OSC_STRING:
                switch (raw) {
                case 0x00 ... 0x06:        /* C0 \ { BEL, ESC } */
                case 0x08 ... 0x1a:
                case 0x1c ... 0x1f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_NONE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_OSC_STRING_ESC,
                                                 ACTION_NONE);
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
                case 0x00 ... 0x1a:        /* ASCII \ { ESC } */
                case 0x1c ... 0x7f:
                        return parser_transition(parser, raw, STATE_NONE,
                                                 ACTION_NONE);
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw, STATE_ESC,
                                                 ACTION_CLEAR);
                case 0x9c:                /* ST */
                        return parser_transition(parser, raw,
                                                 STATE_GROUND, ACTION_IGNORE);
                }

                return parser_transition(parser, raw,
                                         STATE_NONE, ACTION_NONE);
        case STATE_SCI:
                switch (raw) {
                case 0x1b:                /* ESC */
                        return parser_transition(parser, raw,
                                                 STATE_ESC, ACTION_CLEAR);
                case 0x08 ... 0x0d:        /* BS, HT, LF, VT, FF, CR */
                case 0x20 ... 0x7e:        /* [' ' - '~'] */
                        return parser_transition(parser, raw, STATE_GROUND,
                                                 ACTION_SCI_DISPATCH);
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
        case 0x7f:                 /* DEL */
                ret = parser_transition(parser, raw, STATE_NONE,
                                        ACTION_NONE);
                break;
        case 0x80 ... 0x8f:        /* C1 \ {DCS, SOS, SCI, CSI, ST, OSC, PM, APC} */
        case 0x91 ... 0x97:
        case 0x99:
                ret = parser_transition(parser, raw,
                                        STATE_GROUND, ACTION_EXECUTE);
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
