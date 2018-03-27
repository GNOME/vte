/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
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

#pragma once

#include <cstdint>
#include <cstdio>

struct vte_parser;
struct vte_seq;
struct vte_utf8;

/*
 * Charsets
 * The DEC-compatible terminals require non-standard charsets for g0/g1/g2/g3
 * registers. We only provide the basic sets for compatibility. New
 * applications better use the full UTF-8 range for that.
 */

typedef uint32_t vte_charset[96];

extern vte_charset vte_unicode_lower;
extern vte_charset vte_unicode_upper;
extern vte_charset vte_dec_supplemental_graphics;
extern vte_charset vte_dec_special_graphics;

/*
 * UTF-8
 * All stream data must be encoded as UTF-8. As we need to do glyph-rendering,
 * we require a UTF-8 parser so we can map the characters to UCS codepoints.
 */

struct vte_utf8 {
        uint32_t chars[5];
        uint32_t ucs4;

        unsigned int i_bytes : 3;
        unsigned int n_bytes : 3;
        unsigned int valid : 1;
};

size_t vte_utf8_decode(struct vte_utf8 *p, const uint32_t **out_buf, char c);
size_t vte_utf8_encode(char *out_utf8, uint32_t g);

/*
 * Parsers
 * The vte_parser object parses control-sequences for both host and terminal
 * side. Based on this parser, there is a set of command-parsers that take a
 * vte_seq sequence and returns the command it represents. This is different
 * for host and terminal side, and so far we only provide the terminal side, as
 * host side is not used by anyone.
 */

#define VTE_PARSER_ARG_MAX (16)

enum {
        VTE_SEQ_NONE,        /* placeholder, no sequence parsed */

        VTE_SEQ_IGNORE,      /* no-op character */
        VTE_SEQ_GRAPHIC,     /* graphic character */
        VTE_SEQ_CONTROL,     /* control character */
        VTE_SEQ_ESCAPE,      /* escape sequence */
        VTE_SEQ_CSI,         /* control sequence function */
        VTE_SEQ_DCS,         /* device control string */
        VTE_SEQ_OSC,         /* operating system control */

        VTE_SEQ_N,
};

enum {
        /* these must be kept compatible to (1U << (ch - 0x20)) */

        VTE_SEQ_FLAG_SPACE              = (1U <<  0),        /* char:   */
        VTE_SEQ_FLAG_BANG               = (1U <<  1),        /* char: ! */
        VTE_SEQ_FLAG_DQUOTE             = (1U <<  2),        /* char: " */
        VTE_SEQ_FLAG_HASH               = (1U <<  3),        /* char: # */
        VTE_SEQ_FLAG_CASH               = (1U <<  4),        /* char: $ */
        VTE_SEQ_FLAG_PERCENT            = (1U <<  5),        /* char: % */
        VTE_SEQ_FLAG_AND                = (1U <<  6),        /* char: & */
        VTE_SEQ_FLAG_SQUOTE             = (1U <<  7),        /* char: ' */
        VTE_SEQ_FLAG_POPEN              = (1U <<  8),        /* char: ( */
        VTE_SEQ_FLAG_PCLOSE             = (1U <<  9),        /* char: ) */
        VTE_SEQ_FLAG_MULT               = (1U << 10),        /* char: * */
        VTE_SEQ_FLAG_PLUS               = (1U << 11),        /* char: + */
        VTE_SEQ_FLAG_COMMA              = (1U << 12),        /* char: , */
        VTE_SEQ_FLAG_MINUS              = (1U << 13),        /* char: - */
        VTE_SEQ_FLAG_DOT                = (1U << 14),        /* char: . */
        VTE_SEQ_FLAG_SLASH              = (1U << 15),        /* char: / */

        /* 16-25 is reserved for numbers; unused */

        /* COLON is reserved            = (1U << 26),           char: : */
        /* SEMICOLON is reserved        = (1U << 27),           char: ; */
        VTE_SEQ_FLAG_LT                 = (1U << 28),        /* char: < */
        VTE_SEQ_FLAG_EQUAL              = (1U << 29),        /* char: = */
        VTE_SEQ_FLAG_GT                 = (1U << 30),        /* char: > */
        VTE_SEQ_FLAG_WHAT               = (1U << 31),        /* char: ? */
};

enum {
#define _VTE_CMD(cmd) VTE_CMD_##cmd,
#include "parser-cmd.hh"
#undef _VTE_CMD

        VTE_CMD_N
};

enum {
        /*
         * Charsets: DEC marks charsets according to "Digital Equ. Corp.".
         *           NRCS marks charsets according to the "National Replacement
         *           Character Sets". ISO marks charsets according to ISO-8859.
         * The USERDEF charset is special and can be modified by the host.
         */

        VTE_CHARSET_NONE,

        /* 96-compat charsets */
        VTE_CHARSET_ISO_LATIN1_SUPPLEMENTAL,
        VTE_CHARSET_BRITISH_NRCS = VTE_CHARSET_ISO_LATIN1_SUPPLEMENTAL,
        VTE_CHARSET_ISO_LATIN2_SUPPLEMENTAL,
        VTE_CHARSET_AMERICAN_NRCS = VTE_CHARSET_ISO_LATIN2_SUPPLEMENTAL,
        VTE_CHARSET_ISO_LATIN5_SUPPLEMENTAL,
        VTE_CHARSET_ISO_GREEK_SUPPLEMENTAL,
        VTE_CHARSET_ISO_HEBREW_SUPPLEMENTAL,
        VTE_CHARSET_ISO_LATIN_CYRILLIC,

        VTE_CHARSET_96_N,

        /* 94-compat charsets */
        VTE_CHARSET_DEC_SPECIAL_GRAPHIC = VTE_CHARSET_96_N,
        VTE_CHARSET_DEC_SUPPLEMENTAL,
        VTE_CHARSET_DEC_TECHNICAL,
        VTE_CHARSET_CYRILLIC_DEC,
        VTE_CHARSET_DUTCH_NRCS,
        VTE_CHARSET_FINNISH_NRCS,
        VTE_CHARSET_FRENCH_NRCS,
        VTE_CHARSET_FRENCH_CANADIAN_NRCS,
        VTE_CHARSET_GERMAN_NRCS,
        VTE_CHARSET_GREEK_DEC,
        VTE_CHARSET_GREEK_NRCS,
        VTE_CHARSET_HEBREW_DEC,
        VTE_CHARSET_HEBREW_NRCS,
        VTE_CHARSET_ITALIAN_NRCS,
        VTE_CHARSET_NORWEGIAN_DANISH_NRCS,
        VTE_CHARSET_PORTUGUESE_NRCS,
        VTE_CHARSET_RUSSIAN_NRCS,
        VTE_CHARSET_SCS_NRCS,
        VTE_CHARSET_SPANISH_NRCS,
        VTE_CHARSET_SWEDISH_NRCS,
        VTE_CHARSET_SWISS_NRCS,
        VTE_CHARSET_TURKISH_DEC,
        VTE_CHARSET_TURKISH_NRCS,

        VTE_CHARSET_94_N,

        /* special charsets */
        VTE_CHARSET_USERPREF_SUPPLEMENTAL = VTE_CHARSET_94_N,

        VTE_CHARSET_N,
};

struct vte_seq {
        unsigned int type;
        unsigned int command;
        uint32_t terminator;
        unsigned int intermediates;
        unsigned int charset;
        unsigned int n_args;
        int args[VTE_PARSER_ARG_MAX];
        unsigned int n_st;
        char *st;
};

int vte_parser_new(struct vte_parser **out);
struct vte_parser *vte_parser_free(struct vte_parser *parser);
int vte_parser_feed(struct vte_parser *parser,
                    /* const */ struct vte_seq **seq_out,
                    uint32_t raw);
void vte_parser_reset(struct vte_parser *parser);
