/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 *
 * vte is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 */

#pragma once

#include <cstdint>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

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
        u32 chars[5];
        u32 ucs4;

        unsigned int i_bytes : 3;
        unsigned int n_bytes : 3;
        unsigned int valid : 1;
};

size_t vte_utf8_decode(struct vte_utf8 *p, const u32 **out_buf, char c);
size_t vte_utf8_encode(char *out_utf8, u32 g);

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
        VTE_CMD_NONE,                /* placeholder */
        VTE_CMD_GRAPHIC,                /* graphics character */

        VTE_CMD_BEL,                        /* bell */
        VTE_CMD_BS,                        /* backspace */
        VTE_CMD_CBT,                        /* cursor-backward-tabulation */
        VTE_CMD_CHA,                        /* cursor-horizontal-absolute */
        VTE_CMD_CHT,                        /* cursor-horizontal-forward-tabulation */
        VTE_CMD_CNL,                        /* cursor-next-line */
        VTE_CMD_CPL,                        /* cursor-previous-line */
        VTE_CMD_CR,                        /* carriage-return */
        VTE_CMD_CUB,                        /* cursor-backward */
        VTE_CMD_CUD,                        /* cursor-down */
        VTE_CMD_CUF,                        /* cursor-forward */
        VTE_CMD_CUP,                        /* cursor-position */
        VTE_CMD_CUU,                        /* cursor-up */
        VTE_CMD_DA1,                        /* primary-device-attributes */
        VTE_CMD_DA2,                        /* secondary-device-attributes */
        VTE_CMD_DA3,                        /* tertiary-device-attributes */
        VTE_CMD_DC1,                        /* device-control-1 or XON */
        VTE_CMD_DC3,                        /* device-control-3 or XOFF */
        VTE_CMD_DCH,                        /* delete-character */
        VTE_CMD_DECALN,                /* screen-alignment-pattern */
        VTE_CMD_DECANM,                /* ansi-mode */
        VTE_CMD_DECBI,                /* back-index */
        VTE_CMD_DECCARA,                /* change-attributes-in-rectangular-area */
        VTE_CMD_DECCRA,                /* copy-rectangular-area */
        VTE_CMD_DECDC,                /* delete-column */
        VTE_CMD_DECDHL_BH,                /* double-width-double-height-line: bottom half */
        VTE_CMD_DECDHL_TH,                /* double-width-double-height-line: top half */
        VTE_CMD_DECDWL,                /* double-width-single-height-line */
        VTE_CMD_DECEFR,                /* enable-filter-rectangle */
        VTE_CMD_DECELF,                /* enable-local-functions */
        VTE_CMD_DECELR,                /* enable-locator-reporting */
        VTE_CMD_DECERA,                /* erase-rectangular-area */
        VTE_CMD_DECFI,                /* forward-index */
        VTE_CMD_DECFRA,                /* fill-rectangular-area */
        VTE_CMD_DECIC,                /* insert-column */
        VTE_CMD_DECID,                /* return-terminal-id */
        VTE_CMD_DECINVM,                /* invoke-macro */
        VTE_CMD_DECKBD,                /* keyboard-language-selection */
        VTE_CMD_DECKPAM,                /* keypad-application-mode */
        VTE_CMD_DECKPNM,                /* keypad-numeric-mode */
        VTE_CMD_DECLFKC,                /* local-function-key-control */
        VTE_CMD_DECLL,                /* load-leds */
        VTE_CMD_DECLTOD,                /* load-time-of-day */
        VTE_CMD_DECPCTERM,                /* pcterm-mode */
        VTE_CMD_DECPKA,                /* program-key-action */
        VTE_CMD_DECPKFMR,                /* program-key-free-memory-report */
        VTE_CMD_DECRARA,                /* reverse-attributes-in-rectangular-area */
        VTE_CMD_DECRC,                /* restore-cursor */
        VTE_CMD_DECREQTPARM,                /* request-terminal-parameters */
        VTE_CMD_DECRPKT,                /* report-key-type */
        VTE_CMD_DECRQCRA,                /* request-checksum-of-rectangular-area */
        VTE_CMD_DECRQDE,                /* request-display-extent */
        VTE_CMD_DECRQKT,                /* request-key-type */
        VTE_CMD_DECRQLP,                /* request-locator-position */
        VTE_CMD_DECRQM_ANSI,                /* request-mode-ansi */
        VTE_CMD_DECRQM_DEC,                /* request-mode-dec */
        VTE_CMD_DECRQPKFM,                /* request-program-key-free-memory */
        VTE_CMD_DECRQPSR,                /* request-presentation-state-report */
        VTE_CMD_DECRQTSR,                /* request-terminal-state-report */
        VTE_CMD_DECRQUPSS,                /* request-user-preferred-supplemental-set */
        VTE_CMD_DECSACE,                /* select-attribute-change-extent */
        VTE_CMD_DECSASD,                /* select-active-status-display */
        VTE_CMD_DECSC,                /* save-cursor */
        VTE_CMD_DECSCA,                /* select-character-protection-attribute */
        VTE_CMD_DECSCL,                /* select-conformance-level */
        VTE_CMD_DECSCP,                /* select-communication-port */
        VTE_CMD_DECSCPP,                /* select-columns-per-page */
        VTE_CMD_DECSCS,                /* select-communication-speed */
        VTE_CMD_DECSCUSR,                /* set-cursor-style */
        VTE_CMD_DECSDDT,                /* select-disconnect-delay-time */
        VTE_CMD_DECSDPT,                /* select-digital-printed-data-type */
        VTE_CMD_DECSED,                /* selective-erase-in-display */
        VTE_CMD_DECSEL,                /* selective-erase-in-line */
        VTE_CMD_DECSERA,                /* selective-erase-rectangular-area */
        VTE_CMD_DECSFC,                /* select-flow-control */
        VTE_CMD_DECSKCV,                /* set-key-click-volume */
        VTE_CMD_DECSLCK,                /* set-lock-key-style */
        VTE_CMD_DECSLE,                /* select-locator-events */
        VTE_CMD_DECSLPP,                /* set-lines-per-page */
        VTE_CMD_DECSLRM_OR_SC,        /* set-left-and-right-margins or save-cursor */
        VTE_CMD_DECSMBV,                /* set-margin-bell-volume */
        VTE_CMD_DECSMKR,                /* select-modifier-key-reporting */
        VTE_CMD_DECSNLS,                /* set-lines-per-screen */
        VTE_CMD_DECSPP,                /* set-port-parameter */
        VTE_CMD_DECSPPCS,                /* select-pro-printer-character-set */
        VTE_CMD_DECSPRTT,                /* select-printer-type */
        VTE_CMD_DECSR,                /* secure-reset */
        VTE_CMD_DECSRFR,                /* select-refresh-rate */
        VTE_CMD_DECSSCLS,                /* set-scroll-speed */
        VTE_CMD_DECSSDT,                /* select-status-display-line-type */
        VTE_CMD_DECSSL,                /* select-setup-language */
        VTE_CMD_DECST8C,                /* set-tab-at-every-8-columns */
        VTE_CMD_DECSTBM,                /* set-top-and-bottom-margins */
        VTE_CMD_DECSTR,                /* soft-terminal-reset */
        VTE_CMD_DECSTRL,                /* set-transmit-rate-limit */
        VTE_CMD_DECSWBV,                /* set-warning-bell-volume */
        VTE_CMD_DECSWL,                /* single-width-single-height-line */
        VTE_CMD_DECTID,                /* select-terminal-id */
        VTE_CMD_DECTME,                /* terminal-mode-emulation */
        VTE_CMD_DECTST,                /* invoke-confidence-test */
        VTE_CMD_DL,                        /* delete-line */
        VTE_CMD_DSR_ANSI,                /* device-status-report-ansi */
        VTE_CMD_DSR_DEC,                /* device-status-report-dec */
        VTE_CMD_ECH,                        /* erase-character */
        VTE_CMD_ED,                        /* erase-in-display */
        VTE_CMD_EL,                        /* erase-in-line */
        VTE_CMD_ENQ,                        /* enquiry */
        VTE_CMD_EPA,                        /* end-of-guarded-area */
        VTE_CMD_FF,                        /* form-feed */
        VTE_CMD_HPA,                        /* horizontal-position-absolute */
        VTE_CMD_HPR,                        /* horizontal-position-relative */
        VTE_CMD_HT,                        /* horizontal-tab */
        VTE_CMD_HTS,                        /* horizontal-tab-set */
        VTE_CMD_HVP,                        /* horizontal-and-vertical-position */
        VTE_CMD_ICH,                        /* insert-character */
        VTE_CMD_IL,                        /* insert-line */
        VTE_CMD_IND,                        /* index */
        VTE_CMD_LF,                        /* line-feed */
        VTE_CMD_LS1R,                /* locking-shift-1-right */
        VTE_CMD_LS2,                        /* locking-shift-2 */
        VTE_CMD_LS2R,                /* locking-shift-2-right */
        VTE_CMD_LS3,                        /* locking-shift-3 */
        VTE_CMD_LS3R,                /* locking-shift-3-right */
        VTE_CMD_MC_ANSI,                /* media-copy-ansi */
        VTE_CMD_MC_DEC,                /* media-copy-dec */
        VTE_CMD_NEL,                        /* next-line */
        VTE_CMD_NP,                        /* next-page */
        VTE_CMD_NULL,                /* null */
        VTE_CMD_PP,                        /* preceding-page */
        VTE_CMD_PPA,                        /* page-position-absolute */
        VTE_CMD_PPB,                        /* page-position-backward */
        VTE_CMD_PPR,                        /* page-position-relative */
        VTE_CMD_RC,                        /* restore-cursor */
        VTE_CMD_REP,                        /* repeat */
        VTE_CMD_RI,                        /* reverse-index */
        VTE_CMD_RIS,                        /* reset-to-initial-state */
        VTE_CMD_RM_ANSI,                /* reset-mode-ansi */
        VTE_CMD_RM_DEC,                /* reset-mode-dec */
        VTE_CMD_S7C1T,                /* set-7bit-c1-terminal */
        VTE_CMD_S8C1T,                /* set-8bit-c1-terminal */
        VTE_CMD_SCS,                        /* select-character-set */
        VTE_CMD_SD,                        /* scroll-down */
        VTE_CMD_SGR,                        /* select-graphics-rendition */
        VTE_CMD_SI,                        /* shift-in */
        VTE_CMD_SM_ANSI,                /* set-mode-ansi */
        VTE_CMD_SM_DEC,                /* set-mode-dec */
        VTE_CMD_SO,                        /* shift-out */
        VTE_CMD_SPA,                        /* start-of-protected-area */
        VTE_CMD_SS2,                        /* single-shift-2 */
        VTE_CMD_SS3,                        /* single-shift-3 */
        VTE_CMD_ST,                        /* string-terminator */
        VTE_CMD_SU,                        /* scroll-up */
        VTE_CMD_SUB,                        /* substitute */
        VTE_CMD_TBC,                        /* tab-clear */
        VTE_CMD_VPA,                        /* vertical-line-position-absolute */
        VTE_CMD_VPR,                        /* vertical-line-position-relative */
        VTE_CMD_VT,                        /* vertical-tab */
        VTE_CMD_XTERM_CLLHP,                /* xterm-cursor-lower-left-hp-bugfix */
        VTE_CMD_XTERM_IHMT,                /* xterm-initiate-highlight-mouse-tracking */
        VTE_CMD_XTERM_MLHP,                /* xterm-memory-lock-hp-bugfix */
        VTE_CMD_XTERM_MUHP,                /* xterm-memory-unlock-hp-bugfix */
        VTE_CMD_XTERM_RPM,                /* xterm-restore-private-mode */
        VTE_CMD_XTERM_RRV,                /* xterm-reset-resource-value */
        VTE_CMD_XTERM_RTM,                /* xterm-reset-title-mode */
        VTE_CMD_XTERM_SACL1,                /* xterm-set-ansi-conformance-level-1 */
        VTE_CMD_XTERM_SACL2,                /* xterm-set-ansi-conformance-level-2 */
        VTE_CMD_XTERM_SACL3,                /* xterm-set-ansi-conformance-level-3 */
        VTE_CMD_XTERM_SDCS,                /* xterm-set-default-character-set */
        VTE_CMD_XTERM_SGFX,                /* xterm-sixel-graphics */
        VTE_CMD_XTERM_SPM,                /* xterm-set-private-mode */
        VTE_CMD_XTERM_SRV,                /* xterm-set-resource-value */
        VTE_CMD_XTERM_STM,                /* xterm-set-title-mode */
        VTE_CMD_XTERM_SUCS,                /* xterm-set-utf8-character-set */
        VTE_CMD_XTERM_WM,                /* xterm-window-management */

        VTE_CMD_N,
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
        u32 terminator;
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
                    const struct vte_seq **seq_out,
                    u32 raw);
