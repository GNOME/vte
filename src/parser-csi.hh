/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2018 Christian Persch
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

#if !defined(_VTE_SEQ) || !defined(_VTE_NOQ)
#error "Must define _VTE_SEQ and _VTE_NOQ before including this file"
#endif

_VTE_SEQ(ICH,                    CSI,    '@',  NONE,  0, NONE     ) /* insert-character */
_VTE_NOQ(SL,                     CSI,    '@',  NONE,  1, SPACE    ) /* scroll left */
_VTE_SEQ(CUU,                    CSI,    'A',  NONE,  0, NONE     ) /* cursor-up */
_VTE_NOQ(SR,                     CSI,    'A',  NONE,  1, SPACE    ) /* scroll right */
_VTE_SEQ(CUD,                    CSI,    'B',  NONE,  0, NONE     ) /* cursor-down */
_VTE_NOQ(GSM,                    CSI,    'B',  NONE,  1, SPACE    ) /* graphic size modification */
_VTE_SEQ(CUF,                    CSI,    'C',  NONE,  0, NONE     ) /* cursor-forward */
_VTE_NOQ(GSS,                    CSI,    'C',  NONE,  1, SPACE    ) /* graphic size selection */
_VTE_SEQ(CUB,                    CSI,    'D',  NONE,  0, NONE     ) /* cursor-backward */
_VTE_NOQ(FNT,                    CSI,    'D',  NONE,  1, SPACE    ) /* font selection */
_VTE_SEQ(CNL,                    CSI,    'E',  NONE,  0, NONE     ) /* cursor-next-line */
_VTE_SEQ(CPL,                    CSI,    'F',  NONE,  0, NONE     ) /* cursor-previous-line */
_VTE_NOQ(JFY,                    CSI,    'F',  NONE,  1, SPACE    ) /* justify */
_VTE_NOQ(TSS,                    CSI,    'E',  NONE,  1, SPACE    ) /* thine space specification */
_VTE_SEQ(CHA,                    CSI,    'G',  NONE,  0, NONE     ) /* cursor-horizontal-absolute */
_VTE_NOQ(SPI,                    CSI,    'G',  NONE,  1, SPACE    ) /* spacing increment */
_VTE_SEQ(CUP,                    CSI,    'H',  NONE,  0, NONE     ) /* cursor-position */
_VTE_NOQ(QUAD,                   CSI,    'H',  NONE,  1, SPACE    ) /* quad */
_VTE_SEQ(CHT,                    CSI,    'I',  NONE,  0, NONE     ) /* cursor-horizontal-forward-tabulation */
_VTE_NOQ(SSU,                    CSI,    'I',  NONE,  1, SPACE    ) /* set size unit */
_VTE_SEQ(ED,                     CSI,    'J',  NONE,  0, NONE     ) /* erase-in-display */
_VTE_NOQ(PFS,                    CSI,    'J',  NONE,  1, SPACE    ) /* page format selection */
_VTE_SEQ(DECSED,                 CSI,    'J',  WHAT,  0, NONE     ) /* selective-erase-in-display */
_VTE_SEQ(EL,                     CSI,    'K',  NONE,  0, NONE     ) /* erase-in-line */
_VTE_NOQ(SHS,                    CSI,    'K',  NONE,  1, SPACE    ) /* select character spacing */
_VTE_SEQ(DECSEL,                 CSI,    'K',  WHAT,  0, NONE     ) /* selective-erase-in-line */
_VTE_SEQ(IL,                     CSI,    'L',  NONE,  0, NONE     ) /* insert-line */
_VTE_NOQ(SVS,                    CSI,    'L',  NONE,  1, SPACE    ) /* select line spacing */
_VTE_SEQ(DL,                     CSI,    'M',  NONE,  0, NONE     ) /* delete-line */
_VTE_NOQ(IGS,                    CSI,    'M',  NONE,  1, SPACE    ) /* identify graphic subrepertoire */
_VTE_NOQ(EF,                     CSI,    'N',  NONE,  0, NONE     ) /* erase in field */
_VTE_NOQ(EA,                     CSI,    'O',  NONE,  0, NONE     ) /* erase in area */
_VTE_NOQ(IDCS,                   CSI,    'O',  NONE,  1, SPACE    ) /* identify DCS */
_VTE_SEQ(DCH,                    CSI,    'P',  NONE,  0, NONE     ) /* delete-character */
_VTE_NOQ(PPA,                    CSI,    'P',  NONE,  1, SPACE    ) /* page-position-absolute */
_VTE_NOQ(SEE,                    CSI,    'Q',  NONE,  0, NONE     ) /* select editing extent */
_VTE_NOQ(PPR,                    CSI,    'Q',  NONE,  1, SPACE    ) /* page-position-relative */
_VTE_NOQ(PPB,                    CSI,    'R',  NONE,  1, SPACE    ) /* page-position-backward */
_VTE_SEQ(SU,                     CSI,    'S',  NONE,  0, NONE     ) /* scroll-up */
_VTE_SEQ(SPD,                    CSI,    'S',  NONE,  1, SPACE    ) /* select presentation directions */
_VTE_NOQ(XTERM_SGFX,             CSI,    'S',  WHAT,  0, NONE     ) /* xterm-sixel-graphics */
_VTE_SEQ(SD_OR_XTERM_IHMT,       CSI,    'T',  NONE,  0, NONE     ) /* scroll-down or xterm-initiate-highlight-mouse-tracking */
_VTE_NOQ(DTA,                    CSI,    'T',  NONE,  1, SPACE    ) /* dimension text area */
_VTE_NOQ(XTERM_RTM,              CSI,    'T',  GT,    0, NONE     ) /* xterm-reset-title-mode */
_VTE_NOQ(NP,                     CSI,    'U',  NONE,  0, NONE     ) /* next-page */
_VTE_NOQ(SLH,                    CSI,    'U',  NONE,  1, SPACE    ) /* set line home */
_VTE_NOQ(PP,                     CSI,    'V',  NONE,  0, NONE     ) /* preceding-page */
_VTE_NOQ(SLL,                    CSI,    'V',  NONE,  1, SPACE    ) /* set line limit */
_VTE_SEQ(CTC,                    CSI,    'W',  NONE,  0, NONE     ) /* cursor tabulation control */
_VTE_NOQ(FNK,                    CSI,    'W',  NONE,  1, SPACE    ) /* function key */
_VTE_SEQ(DECST8C,                CSI,    'W',  WHAT,  0, NONE     ) /* set-tab-at-every-8-columns */
_VTE_SEQ(ECH,                    CSI,    'X',  NONE,  0, NONE     ) /* erase-character */
_VTE_NOQ(SPQR,                   CSI,    'X',  NONE,  1, SPACE    ) /* select print quality and rapidity */
_VTE_NOQ(CVT,                    CSI,    'Y',  NONE,  0, NONE     ) /* cursor line tabulation */
_VTE_NOQ(SEF,                    CSI,    'Y',  NONE,  1, SPACE    ) /* sheet eject and feed */
_VTE_SEQ(CBT,                    CSI,    'Z',  NONE,  0, NONE     ) /* cursor-backward-tabulation */
_VTE_NOQ(PEC,                    CSI,    'Z',  NONE, 1, SPACE     ) /* presentation expand or contract */
_VTE_NOQ(SRS,                    CSI,    '[',  NONE,  0, NONE     ) /* start reversed string */
_VTE_NOQ(SSW,                    CSI,    '[',  NONE,  1, SPACE    ) /* set space width */
_VTE_NOQ(PTX,                    CSI,    '\\', NONE,  0, NONE     ) /* parallel texts */
_VTE_NOQ(SACS,                   CSI,    '\\', NONE,  1, SPACE    ) /* set additional character separation */
_VTE_NOQ(SDS,                    CSI,    ']',  NONE,  0, NONE     ) /* start directed string */
_VTE_NOQ(SAPV,                   CSI,    ']',  NONE,  1, SPACE    ) /* select alternative presentation variants */
_VTE_NOQ(SIMD,                   CSI,    '^',  NONE,  0, NONE     ) /* select implicit movement direction */
_VTE_NOQ(STAB,                   CSI,    '^',  NONE,  1, SPACE    ) /* selective tabulation */
_VTE_NOQ(GCC,                    CSI,    '_',  NONE,  1, SPACE    ) /* graphic character combination */
_VTE_SEQ(HPA,                    CSI,    '`',  NONE,  0, NONE     ) /* horizontal-position-absolute */
_VTE_NOQ(TATE,                   CSI,    '`',  NONE,  1, SPACE    ) /* tabulation-aligned-trailing-edge */
_VTE_SEQ(HPR,                    CSI,    'a',  NONE,  0, NONE     ) /* horizontal-position-relative */
_VTE_NOQ(TALE,                   CSI,    'a',  NONE,  1, SPACE    ) /* tabulation-aligned-leading-edge */
_VTE_SEQ(REP,                    CSI,    'b',  NONE,  0, NONE     ) /* repeat */
_VTE_NOQ(TAC,                    CSI,    'b',  NONE,  1, SPACE    ) /* tabulation-aligned-centre */
_VTE_SEQ(DA1,                    CSI,    'c',  NONE,  0, NONE     ) /* primary-device-attributes */
_VTE_SEQ(TCC,                    CSI,    'c',  NONE,  1, SPACE    ) /* tabulation-centred-on-character */
_VTE_SEQ(DA3,                    CSI,    'c',  EQUAL, 0, NONE     ) /* tertiary-device-attributes */
_VTE_SEQ(DA2,                    CSI,    'c',  GT,    0, NONE     ) /* secondary-device-attributes */
_VTE_SEQ(VPA,                    CSI,    'd',  NONE,  0, NONE     ) /* vertical-line-position-absolute */
_VTE_SEQ(TSR,                    CSI,    'd',  NONE,  1, SPACE    ) /* tabulation-stop-remove */
_VTE_SEQ(VPR,                    CSI,    'e',  NONE,  0, NONE     ) /* vertical-line-position-relative */
_VTE_NOQ(SCO,                    CSI,    'e',  NONE,  1, SPACE    ) /* select character orientation */
_VTE_SEQ(HVP,                    CSI,    'f',  NONE,  0, NONE     ) /* horizontal-and-vertical-position */
_VTE_NOQ(SRCS,                   CSI,    'f',  NONE,  1, SPACE    ) /* set reduced character separation */
_VTE_SEQ(TBC,                    CSI,    'g',  NONE,  0, NONE     ) /* tab-clear */
_VTE_NOQ(SCS,                    CSI,    'g',  NONE,  1, SPACE    ) /* set character spacing */
_VTE_NOQ(DECLFKC,                CSI,    'g',  NONE,  1, MULT     ) /* local-function-key-control */
_VTE_SEQ(SM_ECMA,                CSI,    'h',  NONE,  0, NONE     ) /* set-mode-ecma */
_VTE_NOQ(SLS,                    CSI,    'h' , NONE,  1, SPACE    ) /* set line spacing */
_VTE_SEQ(SM_DEC,                 CSI,    'h',  WHAT,  0, NONE     ) /* set-mode-dec */
_VTE_NOQ(MC_ECMA,                CSI,    'i',  NONE,  0, NONE     ) /* media-copy-ecma */
_VTE_NOQ(SPH,                    CSI,    'i',  NONE,  1, SPACE    ) /* set page home */
_VTE_NOQ(MC_DEC,                 CSI,    'i',  WHAT,  0, NONE     ) /* media-copy-dec */
_VTE_NOQ(HPB,                    CSI,    'j',  NONE,  0, NONE     ) /* horizontal position backward */
_VTE_NOQ(SPL,                    CSI,    'j',  NONE,  1, SPACE    ) /* set page limit */
_VTE_NOQ(VPB,                    CSI,    'k',  NONE,  0, NONE     ) /* line position backward */
_VTE_SEQ(SCP,                    CSI,    'k',  NONE,  1, SPACE    ) /* select character path */
_VTE_SEQ(RM_ECMA,                CSI,    'l',  NONE,  0, NONE     ) /* reset-mode-ecma */
_VTE_SEQ(RM_DEC,                 CSI,    'l',  WHAT,  0, NONE     ) /* reset-mode-dec */
_VTE_SEQ(SGR,                    CSI,    'm',  NONE,  0, NONE     ) /* select-graphics-rendition */
_VTE_NOQ(DECSGR,                 CSI,    'm',  WHAT,  0, NONE     ) /* DEC select graphics rendition */
_VTE_NOQ(XTERM_SRV,              CSI,    'm',  GT,    0, NONE     ) /* xterm-set-resource-value */
_VTE_SEQ(DSR_ECMA,               CSI,    'n',  NONE,  0, NONE     ) /* device-status-report-ecma */
_VTE_NOQ(XTERM_RRV,              CSI,    'n',  GT,    0, NONE     ) /* xterm-reset-resource-value */
_VTE_SEQ(DSR_DEC,                CSI,    'n',  WHAT,  0, NONE     ) /* device-status-report-dec */
_VTE_NOQ(DAQ,                    CSI,    'o',  NONE,  0, NONE     ) /* define area qualification */
_VTE_NOQ(DECSSL,                 CSI,    'p',  NONE,  0, NONE     ) /* select-setup-language */
_VTE_NOQ(DECSSCLS,               CSI,    'p',  NONE,  1, SPACE    ) /* set-scroll-speed */
_VTE_SEQ(DECSTR,                 CSI,    'p',  NONE,  1, BANG     ) /* soft-terminal-reset */
_VTE_SEQ(DECSCL,                 CSI,    'p',  NONE,  1, DQUOTE   ) /* select-conformance-level */
_VTE_SEQ(DECRQM_ECMA,            CSI,    'p',  NONE,  1, CASH     ) /* request-mode-ecma */
_VTE_NOQ(DECSDPT,                CSI,    'p',  NONE,  1, PCLOSE   ) /* select-digital-printed-data-type */
_VTE_NOQ(DECSPPCS,               CSI,    'p',  NONE,  1, MULT     ) /* select-pro-printer-character-set */
_VTE_SEQ(DECSR,                  CSI,    'p',  NONE,  1, PLUS     ) /* secure-reset */
_VTE_NOQ(DECLTOD,                CSI,    'p',  NONE,  1, COMMA    ) /* load-time-of-day */
_VTE_NOQ(DECARR,                 CSI,    'p',  NONE,  1, MINUS    ) /* auto repeat rate */
_VTE_NOQ(XTERM_PTRMODE,          CSI,    'p',  GT,    0, NONE     ) /* xterm set pointer mode */
_VTE_SEQ(DECRQM_DEC,             CSI,    'p',  WHAT,  1, CASH     ) /* request-mode-dec */
_VTE_NOQ(DECLL,                  CSI,    'q',  NONE,  0, NONE     ) /* load-leds */
_VTE_SEQ(DECSCUSR,               CSI,    'q',  NONE,  1, SPACE    ) /* set-cursor-style */
_VTE_NOQ(DECSCA,                 CSI,    'q',  NONE,  1, DQUOTE   ) /* select-character-protection-attribute */
_VTE_NOQ(DECSDDT,                CSI,    'q',  NONE,  1, CASH     ) /* select-disconnect-delay-time */
_VTE_SEQ(DECSR,                  CSI,    'q',  NONE,  1, MULT     ) /* secure-reset */
_VTE_NOQ(DECELF,                 CSI,    'q',  NONE,  1, PLUS     ) /* enable-local-functions */
_VTE_NOQ(DECTID,                 CSI,    'q',  NONE,  1, COMMA    ) /* select-terminal-id */
_VTE_NOQ(DECCRTST,               CSI,    'q',  NONE,  1, MINUS    ) /* CRT saver time */
_VTE_SEQ(DECSTBM,                CSI,    'r',  NONE,  0, NONE     ) /* set-top-and-bottom-margins */
_VTE_NOQ(DECSKCV,                CSI,    'r',  NONE,  1, SPACE    ) /* set-key-click-volume */
_VTE_NOQ(DECCARA,                CSI,    'r',  NONE,  1, CASH     ) /* change-attributes-in-rectangular-area */
_VTE_NOQ(DECSCS,                 CSI,    'r',  NONE,  1, MULT     ) /* select-communication-speed */
_VTE_NOQ(DECSMKR,                CSI,    'r',  NONE,  1, PLUS     ) /* select-modifier-key-reporting */
_VTE_NOQ(DECSEST,                CSI,    'r',  NONE,  1, MINUS    ) /* energy saver time */
_VTE_SEQ(DECPCTERM_OR_XTERM_RPM, CSI,    'r',  WHAT,  0, NONE     ) /* pcterm or xterm restore DEC private mode */
_VTE_SEQ(DECSLRM_OR_SCOSC,       CSI,    's',  NONE,  0, NONE     ) /* set left and right margins or SCO save cursor */
_VTE_NOQ(DECSPRTT,               CSI,    's',  NONE,  1, CASH     ) /* select-printer-type */
_VTE_NOQ(DECSFC,                 CSI,    's',  NONE,  1, MULT     ) /* select-flow-control */
_VTE_SEQ(XTERM_SPM,              CSI,    's',  WHAT,  0, NONE     ) /* xterm save private mode */
_VTE_SEQ(XTERM_WM,               CSI,    't',  NONE,  0, NONE     ) /* xterm-window-management */
_VTE_NOQ(DECSWBV,                CSI,    't',  NONE,  1, SPACE    ) /* set-warning-bell-volume */
_VTE_NOQ(DECSRFR,                CSI,    't',  NONE,  1, DQUOTE   ) /* select-refresh-rate */
_VTE_NOQ(DECRARA,                CSI,    't',  NONE,  1, CASH     ) /* reverse-attributes-in-rectangular-area */
_VTE_NOQ(XTERM_STM,              CSI,    't',  GT,    0, NONE     ) /* xterm-set-title-mode */
_VTE_SEQ(SCORC,                  CSI,    'u',  NONE,  0, NONE     ) /* SCO restore cursor */
_VTE_NOQ(DECSMBV,                CSI,    'u',  NONE,  1, SPACE    ) /* set-margin-bell-volume */
_VTE_NOQ(DECSTRL,                CSI,    'u',  NONE,  1, DQUOTE   ) /* set-transmit-rate-limit */
_VTE_SEQ(DECRQTSR,               CSI,    'u',  NONE,  1, CASH     ) /* request-terminal-state-report */
_VTE_NOQ(DECSCP,                 CSI,    'u',  NONE,  1, MULT     ) /* select-communication-port */
_VTE_NOQ(DECRQKT,                CSI,    'u',  NONE,  1, COMMA    ) /* request-key-type */
_VTE_NOQ(DECRQUPSS,              CSI,    'u',  WHAT,  0, NONE     ) /* request-user-preferred-supplemental-set */
_VTE_NOQ(DECSLCK,                CSI,    'v',  NONE,  1, SPACE    ) /* set-lock-key-style */
_VTE_NOQ(DECRQDE,                CSI,    'v',  NONE,  1, DQUOTE   ) /* request-display-extent */
_VTE_NOQ(DECCRA,                 CSI,    'v',  NONE,  1, CASH     ) /* copy-rectangular-area */
_VTE_NOQ(DECRPKT,                CSI,    'v',  NONE,  1, COMMA    ) /* report-key-type */
_VTE_NOQ(WYCAA,                  CSI,    'w',  NONE,  0, NONE     ) /* redefine character display attribute association */
_VTE_NOQ(DECRPDE,                CSI,    'w',  NONE,  1, DQUOTE   ) /* report displayed extent */
_VTE_NOQ(DECRQPSR,               CSI,    'w',  NONE,  1, CASH     ) /* request-presentation-state-report */
_VTE_NOQ(DECEFR,                 CSI,    'w',  NONE,  1, SQUOTE   ) /* enable-filter-rectangle */
_VTE_NOQ(DECSPP,                 CSI,    'w',  NONE,  1, PLUS     ) /* set-port-parameter */
_VTE_SEQ(DECREQTPARM,            CSI,    'x',  NONE,  0, NONE     ) /* request-terminal-parameters */
_VTE_NOQ(DECFRA,                 CSI,    'x',  NONE,  1, CASH     ) /* fill-rectangular-area */
_VTE_NOQ(DECES,                  CSI,    'x',  NONE,  1, AND      ) /* enable session */
_VTE_NOQ(DECSACE,                CSI,    'x',  NONE,  1, MULT     ) /* select-attribute-change-extent */
_VTE_NOQ(DECRQPKFM,              CSI,    'x',  NONE,  1, PLUS     ) /* request-program-key-free-memory */
_VTE_NOQ(DECSPMA,                CSI,    'x',  NONE,  1, COMMA    ) /* session page memory allocation */
_VTE_NOQ(DECTST,                 CSI,    'y',  NONE,  0, NONE     ) /* invoke-confidence-test */
_VTE_NOQ(XTERM_CHECKSUM_MODE,    CSI,    'y',  NONE,  1, HASH     ) /* xterm DECRQCRA checksum mode */
_VTE_SEQ(DECRQCRA,               CSI,    'y',  NONE,  1, MULT     ) /* request-checksum-of-rectangular-area */
_VTE_NOQ(DECPKFMR,               CSI,    'y',  NONE,  1, PLUS     ) /* program-key-free-memory-report */
_VTE_NOQ(DECUS,                  CSI,    'y',  NONE,  1, COMMA    ) /* update session */
_VTE_NOQ(WYSCRATE,               CSI,    'z',  NONE,  0, NONE     ) /* set smooth scroll rate */
_VTE_NOQ(DECERA,                 CSI,    'z',  NONE,  1, CASH     ) /* erase-rectangular-area */
_VTE_NOQ(DECELR,                 CSI,    'z',  NONE,  1, SQUOTE   ) /* enable-locator-reporting */
_VTE_NOQ(DECINVM,                CSI,    'z',  NONE,  1, MULT     ) /* invoke-macro */
_VTE_NOQ(DECPKA,                 CSI,    'z',  NONE,  1, PLUS     ) /* program-key-action */
_VTE_NOQ(DECDLDA,                CSI,    'z',  NONE,  1, COMMA    ) /* down line load allocation */
_VTE_NOQ(XTERM_SGR_STACK_PUSH,   CSI,    '{',  NONE,  1, HASH     ) /* push SGR stack */
_VTE_NOQ(DECSERA,                CSI,    '{',  NONE,  1, CASH     ) /* selective-erase-rectangular-area */
_VTE_NOQ(DECSLE,                 CSI,    '{',  NONE,  1, SQUOTE   ) /* select-locator-events */
_VTE_NOQ(DECSTGLT,               CSI,    '{',  NONE,  1, PCLOSE   ) /* select color lookup table */
_VTE_NOQ(DECSZS,                 CSI,    '{',  NONE,  1, COMMA    ) /* select zero symbol */
_VTE_NOQ(XTERM_SGR_REPORT,       CSI,    '|',  NONE,  1, HASH     ) /* SGR report */
_VTE_NOQ(DECSCPP,                CSI,    '|',  NONE,  1, CASH     ) /* select-columns-per-page */
_VTE_NOQ(DECRQLP,                CSI,    '|',  NONE,  1, SQUOTE   ) /* request-locator-position */
_VTE_NOQ(DECSNLS,                CSI,    '|',  NONE,  1, MULT     ) /* set-lines-per-screen */
_VTE_NOQ(DECAC,                  CSI,    '|',  NONE,  1, COMMA    ) /* assign color */
_VTE_NOQ(DECKBD,                 CSI,    '}',  NONE,  1, SPACE    ) /* keyboard-language-selection */
_VTE_NOQ(XTERM_SGR_STACK_POP,    CSI,    '}',  NONE,  1, HASH     ) /* pop SGR stack */
_VTE_NOQ(DECSASD,                CSI,    '}',  NONE,  1, CASH     ) /* select-active-status-display */
_VTE_NOQ(DECIC,                  CSI,    '}',  NONE,  1, SQUOTE   ) /* insert-column */
_VTE_NOQ(DECATC,                 CSI,    '}',  NONE,  1, COMMA    ) /* alternate text color */
_VTE_NOQ(DECTME,                 CSI,    '~',  NONE,  1, SPACE    ) /* terminal-mode-emulation */
_VTE_NOQ(DECSSDT,                CSI,    '~',  NONE,  1, CASH     ) /* select-status-display-line-type */
_VTE_NOQ(DECDC,                  CSI,    '~',  NONE,  1, SQUOTE   ) /* delete-column */
_VTE_NOQ(DECPS,                  CSI,    '~',  NONE,  1, COMMA    ) /* play sound */
