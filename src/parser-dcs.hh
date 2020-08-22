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

_VTE_NOQ(RLOGIN_MML,             DCS,    'm',  NONE,  1, HASH     ) /* RLogin music macro language */
_VTE_NOQ(DECREGIS,               DCS,    'p',  NONE,  0, NONE     ) /* ReGIS-graphics */
_VTE_NOQ(DECRSTS,                DCS,    'p',  NONE,  1, CASH     ) /* restore-terminal-state */
_VTE_NOQ(XTERM_STCAP,            DCS,    'p',  NONE,  1, PLUS     ) /* xterm set termcap/terminfo */
_VTE_NOQ(DECSIXEL,               DCS,    'q',  NONE,  0, NONE     ) /* SIXEL-graphics */
_VTE_SEQ(DECRQSS,                DCS,    'q',  NONE,  1, CASH     ) /* request-selection-or-setting */
_VTE_NOQ(XTERM_RQTCAP,           DCS,    'q',  NONE,  1, PLUS     ) /* xterm request termcap/terminfo */
_VTE_NOQ(DECLBAN,                DCS,    'r',  NONE,  0, NONE     ) /* load-banner-message */
_VTE_SEQ(DECRQTSR,               DCS,    's',  NONE,  1, CASH     ) /* request-terminal-state-report */
_VTE_SEQ(XDGSYNC,                DCS,    's',  EQUAL, 0, NONE     ) /* synchronous update */
_VTE_NOQ(DECRSPS,                DCS,    't',  NONE,  1, CASH     ) /* restore-presentation-state */
_VTE_NOQ(DECAUPSS,               DCS,    'u',  NONE,  1, BANG     ) /* assign-user-preferred-supplemental-sets */
_VTE_NOQ(DECLANS,                DCS,    'v',  NONE,  0, NONE     ) /* load-answerback-message */
_VTE_NOQ(DECLBD,                 DCS,    'w',  NONE,  0, NONE     ) /* locator-button-define */
_VTE_NOQ(DECPFK,                 DCS,    'x',  NONE,  1, DQUOTE   ) /* program-function-key */
_VTE_NOQ(DECPAK,                 DCS,    'y',  NONE,  1, DQUOTE   ) /* program-alphanumeric-key */
_VTE_NOQ(DECDMAC,                DCS,    'z',  NONE,  1, BANG     ) /* define-macro */
_VTE_NOQ(DECCKD,                 DCS,    'z',  NONE,  1, DQUOTE   ) /* copy-key-default */
_VTE_NOQ(DECDLD,                 DCS,    '{',  NONE,  0, NONE     ) /* dynamically-redefinable-character-sets-extension */
_VTE_NOQ(DECSTUI,                DCS,    '{',  NONE,  1, BANG     ) /* set-terminal-unit-id */
_VTE_NOQ(DECUDK,                 DCS,    '|',  NONE,  0, NONE     ) /* user-defined-keys */
_VTE_NOQ(WYLSFNT,                DCS,    '}',  NONE,  0, NONE     ) /* load soft font */
_VTE_NOQ(DECRPFK,                DCS,    '}',  NONE,  1, DQUOTE   ) /* report function key definition */
_VTE_NOQ(DECRPAK,                DCS,    '~',  NONE,  1, DQUOTE   ) /* report all modifier/alphanumeric key state */
