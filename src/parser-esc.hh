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

_VTE_SEQ(DECDHL_TH,              ESCAPE, '3',  NONE,  1, HASH     ) /* double-width-double-height-line: top half */
_VTE_SEQ(DECDHL_BH,              ESCAPE, '4',  NONE,  1, HASH     ) /* double-width-double-height-line: bottom half */
_VTE_SEQ(DECSWL,                 ESCAPE, '5',  NONE,  1, HASH     ) /* single-width-single-height-line */
_VTE_SEQ(DECBI,                  ESCAPE, '6',  NONE,  0, NONE     ) /* back-index */
_VTE_SEQ(DECDWL,                 ESCAPE, '6',  NONE,  1, HASH     ) /* double-width-single-height-line */
_VTE_SEQ(DECSC,                  ESCAPE, '7',  NONE,  0, NONE     ) /* save-cursor */
_VTE_SEQ(DECRC,                  ESCAPE, '8',  NONE,  0, NONE     ) /* restore-cursor */
_VTE_SEQ(DECALN,                 ESCAPE, '8',  NONE,  1, HASH     ) /* screen-alignment-pattern */
_VTE_SEQ(DECFI,                  ESCAPE, '9',  NONE,  0, NONE     ) /* forward-index */
_VTE_SEQ(DECANM,                 ESCAPE, '<',  NONE,  0, NONE     ) /* ansi-mode */
_VTE_SEQ(DECKPAM,                ESCAPE, '=',  NONE,  0, NONE     ) /* keypad-application-mode */
_VTE_SEQ(DECKPNM,                ESCAPE, '>',  NONE,  0, NONE     ) /* keypad-numeric-mode */
_VTE_SEQ(IND,                    ESCAPE, 'D',  NONE,  0, NONE     ) /* index */
_VTE_SEQ(NEL,                    ESCAPE, 'E',  NONE,  0, NONE     ) /* next-line */
_VTE_SEQ(XTERM_CLLHP,            ESCAPE, 'F',  NONE,  0, NONE     ) /* xterm-cursor-lower-left-hp-bugfix */
_VTE_SEQ(HTS,                    ESCAPE, 'H',  NONE,  0, NONE     ) /* horizontal-tab-set */
_VTE_SEQ(RI,                     ESCAPE, 'M',  NONE,  0, NONE     ) /* reverse-index */
_VTE_SEQ(SS2,                    ESCAPE, 'N',  NONE,  0, NONE     ) /* single-shift-2 */
_VTE_SEQ(SS3,                    ESCAPE, 'O',  NONE,  0, NONE     ) /* single-shift-3 */
_VTE_SEQ(SPA,                    ESCAPE, 'V',  NONE,  0, NONE     ) /* start-of-protected-area */
_VTE_SEQ(EPA,                    ESCAPE, 'W',  NONE,  0, NONE     ) /* end-of-guarded-area */
_VTE_SEQ(ST,                     ESCAPE, '\\', NONE,  0, NONE     ) /* string-terminator */
_VTE_SEQ(RIS,                    ESCAPE, 'c',  NONE,  0, NONE     ) /* reset-to-initial-state */
_VTE_SEQ(CMD,                    ESCAPE, 'd',  NONE,  0, NONE     ) /* coding-method-delimiter */
_VTE_SEQ(XTERM_MLHP,             ESCAPE, 'l',  NONE,  0, NONE     ) /* xterm-memory-lock-hp-bugfix */
_VTE_SEQ(XTERM_MUHP,             ESCAPE, 'm',  NONE,  0, NONE     ) /* xterm-memory-unlock-hp-bugfix */
_VTE_SEQ(LS2,                    ESCAPE, 'n',  NONE,  0, NONE     ) /* locking-shift-2 */
_VTE_SEQ(LS3,                    ESCAPE, 'o',  NONE,  0, NONE     ) /* locking-shift-3 */
_VTE_SEQ(LS3R,                   ESCAPE, '|',  NONE,  0, NONE     ) /* locking-shift-3-right */
_VTE_SEQ(LS2R,                   ESCAPE, '}',  NONE,  0, NONE     ) /* locking-shift-2-right */
_VTE_SEQ(LS1R,                   ESCAPE, '~',  NONE,  0, NONE     ) /* locking-shift-1-right */
