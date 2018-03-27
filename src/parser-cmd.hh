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

_VTE_CMD(NONE) /* placeholder */
_VTE_CMD(GRAPHIC) /* graphics character */
_VTE_CMD(ACS) /* announce-code-structure */
_VTE_CMD(BEL) /* bell */
_VTE_CMD(BS) /* backspace */
_VTE_CMD(CBT) /* cursor-backward-tabulation */
_VTE_CMD(CHA) /* cursor-horizontal-absolute */
_VTE_CMD(CHT) /* cursor-horizontal-forward-tabulation */
_VTE_CMD(CMD) /* coding-method-delimiter */
_VTE_CMD(CNL) /* cursor-next-line */
_VTE_CMD(CPL) /* cursor-previous-line */
_VTE_CMD(CR) /* carriage-return */
_VTE_CMD(CUB) /* cursor-backward */
_VTE_CMD(CUD) /* cursor-down */
_VTE_CMD(CUF) /* cursor-forward */
_VTE_CMD(CUP) /* cursor-position */
_VTE_CMD(CUU) /* cursor-up */
_VTE_CMD(CnD) /* Cn-designate */
_VTE_CMD(DA1) /* primary-device-attributes */
_VTE_CMD(DA2) /* secondary-device-attributes */
_VTE_CMD(DA3) /* tertiary-device-attributes */
_VTE_CMD(DC1) /* device-control-1 or XON */
_VTE_CMD(DC3) /* device-control-3 or XOFF */
_VTE_CMD(DCH) /* delete-character */
_VTE_CMD(DECALN) /* screen-alignment-pattern */
_VTE_CMD(DECANM) /* ansi-mode */
_VTE_CMD(DECAUPSS) /* assign-user-preferred-supplemental-sets */
_VTE_CMD(DECBI) /* back-index */
_VTE_CMD(DECCARA) /* change-attributes-in-rectangular-area */
_VTE_CMD(DECCKD) /* copy-key-default */
_VTE_CMD(DECCRA) /* copy-rectangular-area */
_VTE_CMD(DECDC) /* delete-column */
_VTE_CMD(DECDHL_BH) /* double-width-double-height-line: bottom half */
_VTE_CMD(DECDHL_TH) /* double-width-double-height-line: top half */
_VTE_CMD(DECDLD) /* dynamically-redefinable-character-sets-extension */
_VTE_CMD(DECDMAC) /* define-macro */
_VTE_CMD(DECDWL) /* double-width-single-height-line */
_VTE_CMD(DECEFR) /* enable-filter-rectangle */
_VTE_CMD(DECELF) /* enable-local-functions */
_VTE_CMD(DECELR) /* enable-locator-reporting */
_VTE_CMD(DECERA) /* erase-rectangular-area */
_VTE_CMD(DECFI) /* forward-index */
_VTE_CMD(DECFRA) /* fill-rectangular-area */
_VTE_CMD(DECIC) /* insert-column */
_VTE_CMD(DECINVM) /* invoke-macro */
_VTE_CMD(DECKBD) /* keyboard-language-selection */
_VTE_CMD(DECKPAM) /* keypad-application-mode */
_VTE_CMD(DECKPNM) /* keypad-numeric-mode */
_VTE_CMD(DECLANS) /* load-answerback-message */
_VTE_CMD(DECLBAN) /* load-banner-message */
_VTE_CMD(DECLBD) /* locator-button-define */
_VTE_CMD(DECLFKC) /* local-function-key-control */
_VTE_CMD(DECLL) /* load-leds */
_VTE_CMD(DECLTOD) /* load-time-of-day */
_VTE_CMD(DECPCTERM) /* pcterm-mode */
_VTE_CMD(DECPAK) /* program-alphanumeric-key */
_VTE_CMD(DECPFK) /* program-function-key */
_VTE_CMD(DECPKA) /* program-key-action */
_VTE_CMD(DECPKFMR) /* program-key-free-memory-report */
_VTE_CMD(DECRARA) /* reverse-attributes-in-rectangular-area */
_VTE_CMD(DECRC) /* restore-cursor */
_VTE_CMD(DECREGIS) /* ReGIS-graphics */
_VTE_CMD(DECREQTPARM) /* request-terminal-parameters */
_VTE_CMD(DECRPKT) /* report-key-type */
_VTE_CMD(DECRQCRA) /* request-checksum-of-rectangular-area */
_VTE_CMD(DECRQDE) /* request-display-extent */
_VTE_CMD(DECRQKT) /* request-key-type */
_VTE_CMD(DECRQLP) /* request-locator-position */
_VTE_CMD(DECRQM_ECMA) /* request-mode-ecma */
_VTE_CMD(DECRQM_DEC) /* request-mode-dec */
_VTE_CMD(DECRQPKFM) /* request-program-key-free-memory */
_VTE_CMD(DECRQPSR) /* request-presentation-state-report */
_VTE_CMD(DECRQSS) /* request-selection-or-setting */
_VTE_CMD(DECRQTSR) /* request-terminal-state-report */
_VTE_CMD(DECRQUPSS) /* request-user-preferred-supplemental-set */
_VTE_CMD(DECRSPS) /* restore-presentation-state */
_VTE_CMD(DECRSTS) /* restore-terminal-state */
_VTE_CMD(DECSACE) /* select-attribute-change-extent */
_VTE_CMD(DECSASD) /* select-active-status-display */
_VTE_CMD(DECSC) /* save-cursor */
_VTE_CMD(DECSCA) /* select-character-protection-attribute */
_VTE_CMD(DECSCL) /* select-conformance-level */
_VTE_CMD(DECSCP) /* select-communication-port */
_VTE_CMD(DECSCPP) /* select-columns-per-page */
_VTE_CMD(DECSCS) /* select-communication-speed */
_VTE_CMD(DECSCUSR) /* set-cursor-style */
_VTE_CMD(DECSDDT) /* select-disconnect-delay-time */
_VTE_CMD(DECSDPT) /* select-digital-printed-data-type */
_VTE_CMD(DECSED) /* selective-erase-in-display */
_VTE_CMD(DECSEL) /* selective-erase-in-line */
_VTE_CMD(DECSERA) /* selective-erase-rectangular-area */
_VTE_CMD(DECSFC) /* select-flow-control */
_VTE_CMD(DECSIXEL) /* SIXEL-graphics */
_VTE_CMD(DECSKCV) /* set-key-click-volume */
_VTE_CMD(DECSLCK) /* set-lock-key-style */
_VTE_CMD(DECSLE) /* select-locator-events */
_VTE_CMD(DECSLPP) /* set-lines-per-page */
_VTE_CMD(DECSLRM_OR_SC) /* set-left-and-right-margins or save-cursor */
_VTE_CMD(DECSMBV) /* set-margin-bell-volume */
_VTE_CMD(DECSMKR) /* select-modifier-key-reporting */
_VTE_CMD(DECSNLS) /* set-lines-per-screen */
_VTE_CMD(DECSPP) /* set-port-parameter */
_VTE_CMD(DECSPPCS) /* select-pro-printer-character-set */
_VTE_CMD(DECSPRTT) /* select-printer-type */
_VTE_CMD(DECSR) /* secure-reset */
_VTE_CMD(DECSRFR) /* select-refresh-rate */
_VTE_CMD(DECSSCLS) /* set-scroll-speed */
_VTE_CMD(DECSSDT) /* select-status-display-line-type */
_VTE_CMD(DECSSL) /* select-setup-language */
_VTE_CMD(DECST8C) /* set-tab-at-every-8-columns */
_VTE_CMD(DECSTBM) /* set-top-and-bottom-margins */
_VTE_CMD(DECSTUI) /* set-terminal-unit-id */
_VTE_CMD(DECSTR) /* soft-terminal-reset */
_VTE_CMD(DECSTRL) /* set-transmit-rate-limit */
_VTE_CMD(DECSWBV) /* set-warning-bell-volume */
_VTE_CMD(DECSWL) /* single-width-single-height-line */
_VTE_CMD(DECTID) /* select-terminal-id */
_VTE_CMD(DECTME) /* terminal-mode-emulation */
_VTE_CMD(DECTST) /* invoke-confidence-test */
_VTE_CMD(DECUDK) /* user-defined-keys */
_VTE_CMD(DL) /* delete-line */
_VTE_CMD(DOCS) /* designate-other-coding-system */
_VTE_CMD(DSR_DEC) /* device-status-report-dec */
_VTE_CMD(DSR_ECMA) /* device-status-report-ecma */
_VTE_CMD(ECH) /* erase-character */
_VTE_CMD(ED) /* erase-in-display */
_VTE_CMD(EL) /* erase-in-line */
_VTE_CMD(ENQ) /* enquiry */
_VTE_CMD(EPA) /* end-of-guarded-area */
_VTE_CMD(FF) /* form-feed */
_VTE_CMD(GnDm) /* Gn-designate-9m-charset */
_VTE_CMD(GnDMm) /* Gn-designate-multibyte-9m-charset */
_VTE_CMD(HPA) /* horizontal-position-absolute */
_VTE_CMD(HPR) /* horizontal-position-relative */
_VTE_CMD(HT) /* horizontal-tab */
_VTE_CMD(HTS) /* horizontal-tab-set */
_VTE_CMD(HVP) /* horizontal-and-vertical-position */
_VTE_CMD(ICH) /* insert-character */
_VTE_CMD(IL) /* insert-line */
_VTE_CMD(IND) /* index */
_VTE_CMD(IRR) /* identify-revised-registration */
_VTE_CMD(LF) /* line-feed */
_VTE_CMD(LS1R) /* locking-shift-1-right */
_VTE_CMD(LS2) /* locking-shift-2 */
_VTE_CMD(LS2R) /* locking-shift-2-right */
_VTE_CMD(LS3) /* locking-shift-3 */
_VTE_CMD(LS3R) /* locking-shift-3-right */
_VTE_CMD(MC_ANSI) /* media-copy-ansi */
_VTE_CMD(MC_DEC) /* media-copy-dec */
_VTE_CMD(NEL) /* next-line */
_VTE_CMD(NP) /* next-page */
_VTE_CMD(NUL) /* nul */
_VTE_CMD(OSC) /* operating-system-command */
_VTE_CMD(PP) /* preceding-page */
_VTE_CMD(PPA) /* page-position-absolute */
_VTE_CMD(PPB) /* page-position-backward */
_VTE_CMD(PPR) /* page-position-relative */
_VTE_CMD(RC) /* restore-cursor */
_VTE_CMD(REP) /* repeat */
_VTE_CMD(RI) /* reverse-index */
_VTE_CMD(RIS) /* reset-to-initial-state */
_VTE_CMD(RM_ECMA) /* reset-mode-ecma */
_VTE_CMD(RM_DEC) /* reset-mode-dec */
_VTE_CMD(SD) /* scroll-down */
_VTE_CMD(SGR) /* select-graphics-rendition */
_VTE_CMD(SI) /* shift-in */
_VTE_CMD(SM_ECMA) /* set-mode-ecma */
_VTE_CMD(SM_DEC) /* set-mode-dec */
_VTE_CMD(SO) /* shift-out */
_VTE_CMD(SPA) /* start-of-protected-area */
_VTE_CMD(SS2) /* single-shift-2 */
_VTE_CMD(SS3) /* single-shift-3 */
_VTE_CMD(ST) /* string-terminator */
_VTE_CMD(SU) /* scroll-up */
_VTE_CMD(SUB) /* substitute */
_VTE_CMD(TBC) /* tab-clear */
_VTE_CMD(VPA) /* vertical-line-position-absolute */
_VTE_CMD(VPR) /* vertical-line-position-relative */
_VTE_CMD(VT) /* vertical-tab */
_VTE_CMD(XTERM_CLLHP) /* xterm-cursor-lower-left-hp-bugfix */
_VTE_CMD(XTERM_IHMT) /* xterm-initiate-highlight-mouse-tracking */
_VTE_CMD(XTERM_MLHP) /* xterm-memory-lock-hp-bugfix */
_VTE_CMD(XTERM_MUHP) /* xterm-memory-unlock-hp-bugfix */
_VTE_CMD(XTERM_RPM) /* xterm-restore-private-mode */
_VTE_CMD(XTERM_RRV) /* xterm-reset-resource-value */
_VTE_CMD(XTERM_RTM) /* xterm-reset-title-mode */
_VTE_CMD(XTERM_SGFX) /* xterm-sixel-graphics */
_VTE_CMD(XTERM_SPM) /* xterm-set-private-mode */
_VTE_CMD(XTERM_SRV) /* xterm-set-resource-value */
_VTE_CMD(XTERM_STM) /* xterm-set-title-mode */
_VTE_CMD(XTERM_WM) /* xterm-window-management */
