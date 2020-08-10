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

_VTE_NOQ(DECDHL_TH,              ESCAPE, '3',  NONE,  1, HASH     ) /* double-width-double-height-line: top half */
_VTE_NOQ(DECDHL_BH,              ESCAPE, '4',  NONE,  1, HASH     ) /* double-width-double-height-line: bottom half */
_VTE_NOQ(DECSWL,                 ESCAPE, '5',  NONE,  1, HASH     ) /* single-width-single-height-line */
_VTE_SEQ(DECBI,                  ESCAPE, '6',  NONE,  0, NONE     ) /* back-index */
_VTE_NOQ(DECDWL,                 ESCAPE, '6',  NONE,  1, HASH     ) /* double-width-single-height-line */
_VTE_SEQ(DECSC,                  ESCAPE, '7',  NONE,  0, NONE     ) /* save-cursor */
_VTE_SEQ(DECRC,                  ESCAPE, '8',  NONE,  0, NONE     ) /* restore-cursor */
_VTE_SEQ(DECALN,                 ESCAPE, '8',  NONE,  1, HASH     ) /* screen-alignment-pattern */
_VTE_NOQ(DECFI,                  ESCAPE, '9',  NONE,  0, NONE     ) /* forward-index */
_VTE_NOQ(WYDHL_TH,               ESCAPE, ':',  NONE,  0, HASH     ) /* single width double height line: top half */
_VTE_NOQ(WYDHL_BH,               ESCAPE, ';',  NONE,  0, HASH     ) /* single width double height line: bottom half */
_VTE_NOQ(DECANM,                 ESCAPE, '<',  NONE,  0, NONE     ) /* ansi-mode */
_VTE_SEQ(DECKPAM,                ESCAPE, '=',  NONE,  0, NONE     ) /* keypad-application-mode */
_VTE_SEQ(DECKPNM,                ESCAPE, '>',  NONE,  0, NONE     ) /* keypad-numeric-mode */
_VTE_NOQ(BPH,                    ESCAPE, 'B',  NONE,  0, NONE     ) /* break permitted here */
_VTE_NOQ(NBH,                    ESCAPE, 'C',  NONE,  0, NONE     ) /* no break permitted here */
_VTE_SEQ(IND,                    ESCAPE, 'D',  NONE,  0, NONE     ) /* index */
_VTE_SEQ(NEL,                    ESCAPE, 'E',  NONE,  0, NONE     ) /* next-line */
_VTE_NOQ(SSA,                    ESCAPE, 'F',  NONE,  0, NONE     ) /* start of selected area */
_VTE_NOQ(ESA,                    ESCAPE, 'G',  NONE,  0, NONE     ) /* end of selected area */
_VTE_SEQ(HTS,                    ESCAPE, 'H',  NONE,  0, NONE     ) /* horizontal-tab-set */
_VTE_SEQ(HTJ,                    ESCAPE, 'I',  NONE,  0, NONE     ) /* character tabulation with justification */
_VTE_NOQ(VTS,                    ESCAPE, 'J',  NONE,  0, NONE     ) /* line tabulation set */
_VTE_NOQ(PLD,                    ESCAPE, 'K',  NONE,  0, NONE     ) /* partial line forward */
_VTE_NOQ(PLU,                    ESCAPE, 'L',  NONE,  0, NONE     ) /* partial line backward */
_VTE_SEQ(RI,                     ESCAPE, 'M',  NONE,  0, NONE     ) /* reverse-index */
_VTE_SEQ(SS2,                    ESCAPE, 'N',  NONE,  0, NONE     ) /* single-shift-2 */
_VTE_SEQ(SS3,                    ESCAPE, 'O',  NONE,  0, NONE     ) /* single-shift-3 */
_VTE_NOQ(PU1,                    ESCAPE, 'Q',  NONE,  0, NONE     ) /* private use 1 */
_VTE_NOQ(PU2,                    ESCAPE, 'R',  NONE,  0, NONE     ) /* private use 2 */
_VTE_NOQ(STS,                    ESCAPE, 'S',  NONE,  0, NONE     ) /* set transmit state */
_VTE_NOQ(CCH,                    ESCAPE, 'T',  NONE,  0, NONE     ) /* cancel character */
_VTE_NOQ(MW,                     ESCAPE, 'U',  NONE,  0, NONE     ) /* message waiting */
_VTE_NOQ(SPA,                    ESCAPE, 'V',  NONE,  0, NONE     ) /* start-of-protected-area */
_VTE_NOQ(EPA,                    ESCAPE, 'W',  NONE,  0, NONE     ) /* end-of-guarded-area */
_VTE_NOQ(ST,                     ESCAPE, '\\', NONE,  0, NONE     ) /* string-terminator */
_VTE_NOQ(DMI,                    ESCAPE, '`',  NONE,  0, NONE     ) /* disable manual input */
_VTE_NOQ(INT,                    ESCAPE, 'a',  NONE,  0, NONE     ) /* interrupt */
_VTE_NOQ(EMI,                    ESCAPE, 'b',  NONE,  0, NONE     ) /* enable manual input */
_VTE_SEQ(RIS,                    ESCAPE, 'c',  NONE,  0, NONE     ) /* reset-to-initial-state */
_VTE_NOQ(CMD,                    ESCAPE, 'd',  NONE,  0, NONE     ) /* coding-method-delimiter */
_VTE_NOQ(XTERM_MLHP,             ESCAPE, 'l',  NONE,  0, NONE     ) /* xterm-memory-lock-hp-bugfix */
_VTE_NOQ(XTERM_MUHP,             ESCAPE, 'm',  NONE,  0, NONE     ) /* xterm-memory-unlock-hp-bugfix */
_VTE_SEQ(LS2,                    ESCAPE, 'n',  NONE,  0, NONE     ) /* locking-shift-2 */
_VTE_SEQ(LS3,                    ESCAPE, 'o',  NONE,  0, NONE     ) /* locking-shift-3 */
_VTE_SEQ(LS3R,                   ESCAPE, '|',  NONE,  0, NONE     ) /* locking-shift-3-right */
_VTE_SEQ(LS2R,                   ESCAPE, '}',  NONE,  0, NONE     ) /* locking-shift-2-right */
_VTE_SEQ(LS1R,                   ESCAPE, '~',  NONE,  0, NONE     ) /* locking-shift-1-right */
