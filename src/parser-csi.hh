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

_VTE_SEQ(ICH,                    CSI,    '@',  NONE,  0, NONE     ) /* insert-character */
_VTE_SEQ(CUU,                    CSI,    'A',  NONE,  0, NONE     ) /* cursor-up */
_VTE_SEQ(CUD,                    CSI,    'B',  NONE,  0, NONE     ) /* cursor-down */
_VTE_SEQ(CUF,                    CSI,    'C',  NONE,  0, NONE     ) /* cursor-forward */
_VTE_SEQ(CUB,                    CSI,    'D',  NONE,  0, NONE     ) /* cursor-backward */
_VTE_SEQ(CNL,                    CSI,    'E',  NONE,  0, NONE     ) /* cursor-next-line */
_VTE_SEQ(CPL,                    CSI,    'F',  NONE,  0, NONE     ) /* cursor-previous-line */
_VTE_SEQ(CHA,                    CSI,    'G',  NONE,  0, NONE     ) /* cursor-horizontal-absolute */
_VTE_SEQ(CUP,                    CSI,    'H',  NONE,  0, NONE     ) /* cursor-position */
_VTE_SEQ(CHT,                    CSI,    'I',  NONE,  0, NONE     ) /* cursor-horizontal-forward-tabulation */
_VTE_SEQ(ED,                     CSI,    'J',  NONE,  0, NONE     ) /* erase-in-display */
_VTE_SEQ(DECSED,                 CSI,    'J',  WHAT,  0, NONE     ) /* selective-erase-in-display */
_VTE_SEQ(EL,                     CSI,    'K',  NONE,  0, NONE     ) /* erase-in-line */
_VTE_SEQ(DECSEL,                 CSI,    'K',  WHAT,  0, NONE     ) /* selective-erase-in-line */
_VTE_SEQ(IL,                     CSI,    'L',  NONE,  0, NONE     ) /* insert-line */
_VTE_SEQ(DL,                     CSI,    'M',  NONE,  0, NONE     ) /* delete-line */
_VTE_SEQ(DCH,                    CSI,    'P',  NONE,  0, NONE     ) /* delete-character */
_VTE_SEQ(PPA,                    CSI,    'P',  NONE,  1, SPACE    ) /* page-position-absolute */
_VTE_SEQ(PPR,                    CSI,    'Q',  NONE,  1, SPACE    ) /* page-position-relative */
_VTE_SEQ(PPB,                    CSI,    'R',  NONE,  1, SPACE    ) /* page-position-backward */
_VTE_SEQ(SU,                     CSI,    'S',  NONE,  0, NONE     ) /* scroll-up */
_VTE_SEQ(XTERM_SGFX,             CSI,    'S',  WHAT,  0, NONE     ) /* xterm-sixel-graphics */
_VTE_SEQ(SD_OR_XTERM_IHMT,       CSI,    'T',  NONE,  0, NONE     ) /* scroll-down or xterm-initiate-highlight-mouse-tracking */
_VTE_SEQ(XTERM_RTM,              CSI,    'T',  GT,    0, NONE     ) /* xterm-reset-title-mode */
_VTE_SEQ(NP,                     CSI,    'U',  NONE,  0, NONE     ) /* next-page */
_VTE_SEQ(PP,                     CSI,    'V',  NONE,  0, NONE     ) /* preceding-page */
_VTE_SEQ(DECST8C,                CSI,    'W',  WHAT,  0, NONE     ) /* set-tab-at-every-8-columns */
_VTE_SEQ(ECH,                    CSI,    'X',  NONE,  0, NONE     ) /* erase-character */
_VTE_SEQ(CBT,                    CSI,    'Z',  NONE,  0, NONE     ) /* cursor-backward-tabulation */
_VTE_SEQ(HPA,                    CSI,    '`',  NONE,  0, NONE     ) /* horizontal-position-absolute */
_VTE_SEQ(TATE,                   CSI,    '`',  NONE,  1, SPACE    ) /* tabulation-aligned-trailing-edge */
_VTE_SEQ(HPR,                    CSI,    'a',  NONE,  0, NONE     ) /* horizontal-position-relative */
_VTE_SEQ(TALE,                   CSI,    'a',  NONE,  1, SPACE    ) /* tabulation-aligned-leading-edge */
_VTE_SEQ(REP,                    CSI,    'b',  NONE,  0, NONE     ) /* repeat */
_VTE_SEQ(TAC,                    CSI,    'b',  NONE,  1, SPACE    ) /* tabulation-aligned-centre */
_VTE_SEQ(DA1,                    CSI,    'c',  NONE,  0, NONE     ) /* primary-device-attributes */
_VTE_SEQ(TCC,                    CSI,    'c',  NONE,  1, SPACE    ) /* tabulation-centred-on-character */
_VTE_SEQ(DA3,                    CSI,    'c',  EQUAL, 0, NONE     ) /* tertiary-device-attributes */
_VTE_SEQ(DA2,                    CSI,    'c',  GT,    0, NONE     ) /* secondary-device-attributes */
_VTE_SEQ(VPA,                    CSI,    'd',  NONE,  0, NONE     ) /* vertical-line-position-absolute */
_VTE_SEQ(TSR,                    CSI,    'd',  NONE,  1, SPACE    ) /* tabulation-stop-remove */
_VTE_SEQ(VPR,                    CSI,    'e',  NONE,  0, NONE     ) /* vertical-line-position-relative */
_VTE_SEQ(HVP,                    CSI,    'f',  NONE,  0, NONE     ) /* horizontal-and-vertical-position */
_VTE_SEQ(TBC,                    CSI,    'g',  NONE,  0, NONE     ) /* tab-clear */
_VTE_SEQ(DECLFKC,                CSI,    'g',  NONE,  1, MULT     ) /* local-function-key-control */
_VTE_SEQ(SM_ECMA,                CSI,    'h',  NONE,  0, NONE     ) /* set-mode-ecma */
_VTE_SEQ(SM_DEC,                 CSI,    'h',  WHAT,  0, NONE     ) /* set-mode-dec */
_VTE_SEQ(MC_ANSI,                CSI,    'i',  NONE,  0, NONE     ) /* media-copy-ansi */
_VTE_SEQ(MC_DEC,                 CSI,    'i',  WHAT,  0, NONE     ) /* media-copy-dec */
_VTE_SEQ(RM_ECMA,                CSI,    'l',  NONE,  0, NONE     ) /* reset-mode-ecma */
_VTE_SEQ(RM_DEC,                 CSI,    'l',  WHAT,  0, NONE     ) /* reset-mode-dec */
_VTE_SEQ(SGR,                    CSI,    'm',  NONE,  0, NONE     ) /* select-graphics-rendition */
_VTE_SEQ(XTERM_SRV,              CSI,    'm',  GT,    0, NONE     ) /* xterm-set-resource-value */
_VTE_SEQ(DSR_ECMA,               CSI,    'n',  NONE,  0, NONE     ) /* device-status-report-ecma */
_VTE_SEQ(XTERM_RRV,              CSI,    'n',  GT,    0, NONE     ) /* xterm-reset-resource-value */
_VTE_SEQ(DSR_DEC,                CSI,    'n',  WHAT,  0, NONE     ) /* device-status-report-dec */
_VTE_SEQ(DECSSL,                 CSI,    'p',  NONE,  0, NONE     ) /* select-setup-language */
_VTE_SEQ(DECSSCLS,               CSI,    'p',  NONE,  1, SPACE    ) /* set-scroll-speed */
_VTE_SEQ(DECSTR,                 CSI,    'p',  NONE,  1, BANG     ) /* soft-terminal-reset */
_VTE_SEQ(DECSCL,                 CSI,    'p',  NONE,  1, DQUOTE   ) /* select-conformance-level */
_VTE_SEQ(DECRQM_ECMA,            CSI,    'p',  NONE,  1, CASH     ) /* request-mode-ecma */
_VTE_SEQ(DECSDPT,                CSI,    'p',  NONE,  1, PCLOSE   ) /* select-digital-printed-data-type */
_VTE_SEQ(DECSPPCS,               CSI,    'p',  NONE,  1, MULT     ) /* select-pro-printer-character-set */
_VTE_SEQ(DECSR,                  CSI,    'p',  NONE,  1, PLUS     ) /* secure-reset */
_VTE_SEQ(DECLTOD,                CSI,    'p',  NONE,  1, COMMA    ) /* load-time-of-day */
_VTE_SEQ(XTERM_SPM,              CSI,    'p',  GT,    0, NONE     ) /* xterm-set-private-mode */
_VTE_SEQ(DECRQM_DEC,             CSI,    'p',  WHAT,  1, CASH     ) /* request-mode-dec */
_VTE_SEQ(DECLL,                  CSI,    'q',  NONE,  0, NONE     ) /* load-leds */
_VTE_SEQ(DECSCUSR,               CSI,    'q',  NONE,  1, SPACE    ) /* set-cursor-style */
_VTE_SEQ(DECSCA,                 CSI,    'q',  NONE,  1, DQUOTE   ) /* select-character-protection-attribute */
_VTE_SEQ(DECSDDT,                CSI,    'q',  NONE,  1, CASH     ) /* select-disconnect-delay-time */
_VTE_SEQ(DECSR,                  CSI,    'q',  NONE,  1, MULT     ) /* secure-reset */
_VTE_SEQ(DECELF,                 CSI,    'q',  NONE,  1, PLUS     ) /* enable-local-functions */
_VTE_SEQ(DECTID,                 CSI,    'q',  NONE,  1, COMMA    ) /* select-terminal-id */
_VTE_SEQ(DECSTBM,                CSI,    'r',  NONE,  0, NONE     ) /* set-top-and-bottom-margins */
_VTE_SEQ(DECSKCV,                CSI,    'r',  NONE,  1, SPACE    ) /* set-key-click-volume */
_VTE_SEQ(DECCARA,                CSI,    'r',  NONE,  1, CASH     ) /* change-attributes-in-rectangular-area */
_VTE_SEQ(DECSCS,                 CSI,    'r',  NONE,  1, MULT     ) /* select-communication-speed */
_VTE_SEQ(DECSMKR,                CSI,    'r',  NONE,  1, PLUS     ) /* select-modifier-key-reporting */
_VTE_SEQ(DECPCTERM_OR_XTERM_RPM, CSI,    'r',  WHAT,  0, NONE     ) /* pcterm or xterm-restore-private-mode */
_VTE_SEQ(DECSLRM_OR_SC,          CSI,    's',  NONE,  0, NONE     ) /* set-left-and-right-margins or save-cursor */
_VTE_SEQ(DECSPRTT,               CSI,    's',  NONE,  1, CASH     ) /* select-printer-type */
_VTE_SEQ(DECSFC,                 CSI,    's',  NONE,  1, MULT     ) /* select-flow-control */
_VTE_SEQ(XTERM_SPM,              CSI,    's',  WHAT,  0, NONE     ) /* xterm-set-private-mode */
_VTE_SEQ(XTERM_WM,               CSI,    't',  NONE,  0, NONE     ) /* xterm-window-management */
_VTE_SEQ(DECSWBV,                CSI,    't',  NONE,  1, SPACE    ) /* set-warning-bell-volume */
_VTE_SEQ(DECSRFR,                CSI,    't',  NONE,  1, DQUOTE   ) /* select-refresh-rate */
_VTE_SEQ(DECRARA,                CSI,    't',  NONE,  1, CASH     ) /* reverse-attributes-in-rectangular-area */
_VTE_SEQ(XTERM_STM,              CSI,    't',  GT,    0, NONE     ) /* xterm-set-title-mode */
_VTE_SEQ(RC,                     CSI,    'u',  NONE,  0, NONE     ) /* restore-cursor */
_VTE_SEQ(DECSMBV,                CSI,    'u',  NONE,  1, SPACE    ) /* set-margin-bell-volume */
_VTE_SEQ(DECSTRL,                CSI,    'u',  NONE,  1, DQUOTE   ) /* set-transmit-rate-limit */
_VTE_SEQ(DECRQTSR,               CSI,    'u',  NONE,  1, CASH     ) /* request-terminal-state-report */
_VTE_SEQ(DECSCP,                 CSI,    'u',  NONE,  1, MULT     ) /* select-communication-port */
_VTE_SEQ(DECRQKT,                CSI,    'u',  NONE,  1, COMMA    ) /* request-key-type */
_VTE_SEQ(DECRQUPSS,              CSI,    'u',  WHAT,  0, NONE     ) /* request-user-preferred-supplemental-set */
_VTE_SEQ(DECSLCK,                CSI,    'v',  NONE,  1, SPACE    ) /* set-lock-key-style */
_VTE_SEQ(DECRQDE,                CSI,    'v',  NONE,  1, DQUOTE   ) /* request-display-extent */
_VTE_SEQ(DECCRA,                 CSI,    'v',  NONE,  1, CASH     ) /* copy-rectangular-area */
_VTE_SEQ(DECRPKT,                CSI,    'v',  NONE,  1, COMMA    ) /* report-key-type */
_VTE_SEQ(DECRQPSR,               CSI,    'w',  NONE,  1, CASH     ) /* request-presentation-state-report */
_VTE_SEQ(DECEFR,                 CSI,    'w',  NONE,  1, SQUOTE   ) /* enable-filter-rectangle */
_VTE_SEQ(DECSPP,                 CSI,    'w',  NONE,  1, PLUS     ) /* set-port-parameter */
_VTE_SEQ(DECREQTPARM,            CSI,    'x',  NONE,  0, NONE     ) /* request-terminal-parameters */
_VTE_SEQ(DECFRA,                 CSI,    'x',  NONE,  1, CASH     ) /* fill-rectangular-area */
_VTE_SEQ(DECSACE,                CSI,    'x',  NONE,  1, MULT     ) /* select-attribute-change-extent */
_VTE_SEQ(DECRQPKFM,              CSI,    'x',  NONE,  1, PLUS     ) /* request-program-key-free-memory */
_VTE_SEQ(DECTST,                 CSI,    'y',  NONE,  0, NONE     ) /* invoke-confidence-test */
_VTE_SEQ(DECRQCRA,               CSI,    'y',  NONE,  1, MULT     ) /* request-checksum-of-rectangular-area */
_VTE_SEQ(DECPKFMR,               CSI,    'y',  NONE,  1, PLUS     ) /* program-key-free-memory-report */
_VTE_SEQ(DECERA,                 CSI,    'z',  NONE,  1, CASH     ) /* erase-rectangular-area */
_VTE_SEQ(DECELR,                 CSI,    'z',  NONE,  1, SQUOTE   ) /* enable-locator-reporting */
_VTE_SEQ(DECINVM,                CSI,    'z',  NONE,  1, MULT     ) /* invoke-macro */
_VTE_SEQ(DECPKA,                 CSI,    'z',  NONE,  1, PLUS     ) /* program-key-action */
_VTE_SEQ(DECSERA,                CSI,    '{',  NONE,  1, CASH     ) /* selective-erase-rectangular-area */
_VTE_SEQ(DECSLE,                 CSI,    '{',  NONE,  1, SQUOTE   ) /* select-locator-events */
_VTE_SEQ(DECSCPP,                CSI,    '|',  NONE,  1, CASH     ) /* select-columns-per-page */
_VTE_SEQ(DECRQLP,                CSI,    '|',  NONE,  1, SQUOTE   ) /* request-locator-position */
_VTE_SEQ(DECSNLS,                CSI,    '|',  NONE,  1, MULT     ) /* set-lines-per-screen */
_VTE_SEQ(DECKBD,                 CSI,    '}',  NONE,  1, SPACE    ) /* keyboard-language-selection */
_VTE_SEQ(DECSASD,                CSI,    '}',  NONE,  1, CASH     ) /* select-active-status-display */
_VTE_SEQ(DECIC,                  CSI,    '}',  NONE,  1, SQUOTE   ) /* insert-column */
_VTE_SEQ(DECTME,                 CSI,    '~',  NONE,  1, SPACE    ) /* terminal-mode-emulation */
_VTE_SEQ(DECSSDT,                CSI,    '~',  NONE,  1, CASH     ) /* select-status-display-line-type */
_VTE_SEQ(DECDC,                  CSI,    '~',  NONE,  1, SQUOTE   ) /* delete-column */
