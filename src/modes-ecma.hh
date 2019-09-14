/*
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

#if !defined(MODE) || !defined(MODE_FIXED)
#error "Must define MODE and MODE_FIXED before including this file"
#endif

/*
 * Modes for SM_ECMA/RM_ECMA.
 *
 * Most of these are not implemented in VTE.
 *
 * References: ECMA-48 § 7
 *             WY370
 */

/*
 * IRM - insertion replacement mode
 *
 * Default: reset
 *
 * References: ECMA-48 § 7.2.10
 *             VT525
 */
MODE(IRM,  4)

/*
 * BDSM - Bi-Directional Support Mode
 *
 * Reset state is explicit mode, set state is implicit mode
 *
 * References: ECMA-48
 *             ECMA TR/53
 *             Terminal-wg/bidi
 *
 * Default in ECMA: reset
 * Default in Terminal-wg/bidi and VTE: set
 */
MODE(BDSM, 8)

/* Unsupported */

MODE_FIXED(GATM,  1, ALWAYS_RESET)
MODE_FIXED(KAM,   2, ALWAYS_RESET)
MODE_FIXED(CRM,   3, ALWAYS_RESET)
MODE_FIXED(SRTM,  5, ALWAYS_RESET)
MODE_FIXED(ERM,   6, ALWAYS_RESET)
MODE_FIXED(VEM,   7, ALWAYS_RESET)
/* DCSM defaults to RESET in ECMA, forced to SET in Terminal-wg/bidi */
MODE_FIXED(DCSM,  9, ALWAYS_SET)
MODE_FIXED(HEM,  10, ALWAYS_RESET)
MODE_FIXED(PUM,  11, ALWAYS_RESET) /* ECMA-48 § F.4.1 Deprecated */

/*
 * SRM - local echo send/receive mode
 * If reset, characters entered by the keyboard are shown on the
 * screen as well as being sent to the host; if set, the
 * keyboard input is only sent to the host.
 *
 * Default: set
 *
 * References: ECMA-48 § 7.2.15
 *             VT525
 *
 * Removed in VTE 0.60: issue #69
 */
MODE_FIXED(SRM,  12, ALWAYS_SET)

MODE_FIXED(FEAM, 13, ALWAYS_RESET)
MODE_FIXED(FETM, 14, ALWAYS_RESET)
MODE_FIXED(MATM, 15, ALWAYS_RESET)
MODE_FIXED(TTM,  16, ALWAYS_RESET)
MODE_FIXED(SATM, 17, ALWAYS_RESET)
MODE_FIXED(TSM,  18, ALWAYS_RESET)
MODE_FIXED(EBM,  19, ALWAYS_RESET) /* ECMA-48 § F.5.1 Removed */

/*
 * LNM - line feed/newline mode
 * If set, the cursor moves to the first column on LF, FF, VT,
 * and a Return key press sends CRLF.
 * If reset, the cursor column is unchanged by LF, FF, VT,
 * and a Return key press sends CR only.
 *
 * Default: reset
 *
 * References: ECMA-48 § F.5.2 Removed!
 *             VT525
 */
MODE_FIXED(LNM,  20, ALWAYS_RESET)

MODE_FIXED(GRCM, 21, ALWAYS_SET)
MODE_FIXED(ZDM,  22, ALWAYS_RESET) /* ECMA-48 § F.4.2 Deprecated */

/*
 * WYDSCM - display disable mode
 * If set, blanks the screen; if reset, shows the data.
 *
 * Default: reset
 *
 * References: WY370
 */
MODE_FIXED(WYDSCM,    30, ALWAYS_RESET)

/*
 * WHYSTLINM - status line display mode
 *
 * Default: reset (set-up)
 *
 * References: WY370
 */
MODE_FIXED(WYSTLINM,  31, ALWAYS_RESET)

/*
 * WYCRTSAVM - screen saver mode
 * Like DECCRTSM.
 *
 * Default: reset (set-up)
 *
 * References: WY370
 */
MODE_FIXED(WYCRTSAVM, 32, ALWAYS_RESET)

/*
 * WYSTCURM - steady cursor mode
 *
 * Default: reset (set-up)
 *
 * References: WY370
 */
MODE_FIXED(WYSTCURM,  33, ALWAYS_RESET)

/*
 * WYULCURM - underline cursor mode
 *
 * Default: reset (set-up)
 *
 * References: WY370
 */
MODE_FIXED(WYULCURM,  34, ALWAYS_RESET)

/*
 * WYCLRM - width change clear disable mode
 * If set, the screen is not cleared when the column mode changes
 * by DECCOLM or WY161.
 * Note that this does not affect DECSCPP.
 * This is the same as DECNCSM mode.
 *
 * Default: set (set-up)
 *
 * References: WY370
 */
MODE_FIXED(WYCLRM,    35, ALWAYS_SET)

/*
 * WYDELKM - delete key definition
 *
 * Default: reset (set-up)
 *
 * References: WY370
 */
MODE_FIXED(WYDELKM,   36, ALWAYS_RESET) /* Same as DECBKM */

/*
 * WYGATM - send characters mode
 * If set, sends all characters; if reset, only erasable characters.
 * Like GATM above.
 *
 * Default: reset (set-up)
 *
 * References: WY370
 */
MODE_FIXED(WYGATM,    37, ALWAYS_RESET)

/*
 * WYTEXM - send full screen/scrolling region to printer
 * Like DECPEX mode.
 *
 * Default: reset (set-up)
 *
 * References: WY370
 */
MODE_FIXED(WYTEXM,    38, ALWAYS_RESET)

/*
 * WYEXTDM - extra data line
 * If set, the last line of the screen is used as data line and not
 * a status line; if reset, the last line of the screen is used
 * as a status line.
 *
 * Default: reset
 *
 * References: WY370
 */
MODE_FIXED(WYEXTDM,   40, ALWAYS_SET)

/*
 * WYASCII - WY350 personality mode
 * If set, switches to WY350 personality.
 *
 * Default: reset (set-up)
 *
 * References: WY370
 */
MODE_FIXED(WYASCII,   42, ALWAYS_SET)
