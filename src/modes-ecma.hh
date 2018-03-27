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

/*
 * Modes for SM_ECMA/RM_ECMA.
 *
 * Most of these are not implemented in VTE.
 *
 * References: ECMA-48 § 7
 *             WY370
 */

MODE(IRM,  4)
MODE(SRM, 12)

/* Unsupported */

MODE_FIXED(GATM,  1, ALWAYS_RESET)
MODE_FIXED(KAM,   2, ALWAYS_RESET)
MODE_FIXED(CRM,   3, ALWAYS_RESET)
MODE_FIXED(SRTM,  5, ALWAYS_RESET)
MODE_FIXED(ERM,   6, ALWAYS_RESET)
MODE_FIXED(VEM,   7, ALWAYS_RESET)
MODE_FIXED(BDSM,  8, ALWAYS_RESET)
MODE_FIXED(DCSM,  9, ALWAYS_RESET)
MODE_FIXED(HEM,  10, ALWAYS_RESET)
MODE_FIXED(PUM,  11, ALWAYS_RESET) /* ECMA-48 § F.4.1 Deprecated */
MODE_FIXED(FEAM, 13, ALWAYS_RESET)
MODE_FIXED(FETM, 14, ALWAYS_RESET)
MODE_FIXED(MATM, 15, ALWAYS_RESET)
MODE_FIXED(TTM,  16, ALWAYS_RESET)
MODE_FIXED(SATM, 17, ALWAYS_RESET)
MODE_FIXED(TSM,  18, ALWAYS_RESET)
MODE_FIXED(EBM,  19, ALWAYS_RESET) /* ECMA-48 § F.5.1 Removed */
MODE_FIXED(LNM,  20, ALWAYS_RESET) /* ECMA-48 § F.5.2 Removed */
MODE_FIXED(GRCM, 21, ALWAYS_SET)
MODE_FIXED(ZDM,  22, ALWAYS_RESET) /* ECMA-48 § F.4.2 Deprecated */

#if 0
MODE_FIXED(WYDSCM,    30, ALWAYS_SET)
MODE_FIXED(WYSTLINM,  31, ALWAYS_RESET)
MODE_FIXED(WYCRTSAVM, 32, ALWAYS_RESET)
MODE_FIXED(WYSTCURM,  33, ?)
MODE_FIXED(WYULCURM,  34, ?)
MODE_FIXED(WYCLRM,    35, ALWAYS_SET)
MODE_FIXED(WYDELKM,   36, ALWAYS_RESET) /* Same as DECBKM */
MODE_FIXED(WYGATM,    37, ?)
MODE_FIXED(WYTEXM,    38, ?)
MODE_FIXED(WYEXTDM,   40, ALWAYS_SET)
MODE_FIXED(WYASCII,   42, ALWAYS_SET)
#endif
