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
 * Modes for SM_DEC/RM_DEC.
 *
 * Most of these are not implemented in VTE.
 *
 * References: VT525
 *             XTERM
 *             KITTY
 *             MINTTY
 *             MLTERM
 *             RLogin
 *             URXVT
 *             WY370
 */

/* Supported modes: */

/* DEC */

/*
 * DECCKM - cursor keys mode
 *
 * Controls whether the cursor keys send cursor sequences, or application
 * sequences.
 *
 * Default: reset
 *
 * References: VT525
 */
MODE(DEC_APPLICATION_CURSOR_KEYS, 1)

/*
 * DECCOLM: 132 column mode
 *
 * Sets page width to 132 (set) or 80 (reset) columns.
 *
 * Changing this mode resets the top, bottom, left, right margins;
 * clears the screen (unless DECNCSM is set); resets DECLRMM; and clears
 * the status line if host-writable.
 *
 * Default: reset
 *
 * References: VT525
 */
MODE(DEC_132_COLUMN, 3)

/*
 * DECSCNM - screen mode
 * If set, displays reverse; if reset, normal.
 *
 * Default: reset
 *
 * References: VT525
 */
MODE(DEC_REVERSE_IMAGE,  5)

/*
 * DECOM - origin mode
 * If set, the cursor is restricted to within the page margins.
 *
 * On terminal reset, DECOM is reset.
 *
 * Default: reset
 *
 * References: VT525
 */
MODE(DEC_ORIGIN, 6)

/*
 * DECAWM - auto wrap mode
 *
 * Controls whether text wraps to the next line when the
 * cursor reaches the right margin.
 *
 * Default: reset
 *
 * References: VT525
 */
MODE(DEC_AUTOWRAP, 7)

/*
 * DECTCEM - text cursor enable
 * If set, the text cursor is visible; if reset, invisible.
 *
 * Default: set
 *
 * References: VT525
 */
MODE(DEC_TEXT_CURSOR, 25)

/*
 * DECNKM - numeric/application keypad mode
 * Controls whether the numeric keypad sends application (set)
 * or keypad (reset) sequences.
 *
 * Default: reset
 *
 * References: VT525
 */
MODE(DEC_APPLICATION_KEYPAD, 66)

/* Terminal-wg */

/*
 * Whether to swap the Left and Right arrow keys if the cursor
 * stands over an RTL paragraph.
 *
 * Reference: Terminal-wg/bidi
 */
MODE(VTE_BIDI_SWAP_ARROW_KEYS, 1243)

/*
 * Whether box drawing characters in the U+2500..U+257F range
 * are to be mirrored in RTL context.
 *
 * Reference: Terminal-wg/bidi
 */
MODE(VTE_BIDI_BOX_MIRROR, 2500)

/*
 * Whether BiDi paragraph direction is autodetected.
 *
 * Reference: Terminal-wg/bidi
 */
MODE(VTE_BIDI_AUTO, 2501)

/* XTERM */

MODE(XTERM_MOUSE_X10,                   9)
MODE(XTERM_DECCOLM,                    40)
MODE(XTERM_ALTBUF,                     47)
MODE(XTERM_MOUSE_VT220,              1000)
MODE(XTERM_MOUSE_VT220_HIGHLIGHT,    1001)
MODE(XTERM_MOUSE_BUTTON_EVENT,       1002)
MODE(XTERM_MOUSE_ANY_EVENT,          1003)
MODE(XTERM_FOCUS,                    1004)
MODE(XTERM_MOUSE_EXT_SGR,            1006)
MODE(XTERM_ALTBUF_SCROLL,            1007)
MODE(XTERM_META_SENDS_ESCAPE,        1036)
MODE(XTERM_OPT_ALTBUF,               1047)
MODE(XTERM_SAVE_CURSOR,              1048)
MODE(XTERM_OPT_ALTBUF_SAVE_CURSOR,   1049)
MODE(XTERM_READLINE_BRACKETED_PASTE, 2004)

/* Not supported modes: */

/* DEC */

/*
 * DECANM - ansi-mode
 * Resetting this puts the terminal into VT52 compatibility mode.
 * Control sequences overlap with regular sequences so we have to
 * detect them early before dispatching them.
 * To return to ECMA-48 mode, use ESC < [1/11 3/12].
 *
 * Default: set
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECANM,      2, ALWAYS_SET)

/*
 * DECSCLM - scrolling mode
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECSCLM,     4, ALWAYS_RESET)

/*
 * DECARM - autorepeat mode
 * Controls whether keys auytomatically repeat while held pressed
 * for more than 0.5s.
 * Note that /some/ keys do not repeat regardless of this setting.
 *
 * Default: set
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECARM,      8, ALWAYS_SET)

MODE_FIXED(DECLTM,     11, ALWAYS_RESET)
MODE_FIXED(DECEKEM,    16, ALWAYS_RESET)

/*
 * DECPFF - print FF mode
 * Controls whether the terminal terminates a print command by
 * sending a FF to the printer.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECCPFF,    18, ALWAYS_RESET)

/*
 * DECPEX - print extent mode
 * If set, print page prints only the scrolling region;
 * if reset, the complete page.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECPEX,     19, ALWAYS_RESET)

/*
 * DECLRM - RTL mode
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECRLM,     34, ALWAYS_RESET)

/*
 * DECHEBM - hebrew/north-american keyboard mapping mode
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECHEBM,    35, ALWAYS_RESET)

/*
 * DECHEM - hebrew encoding mode
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECHEM,     36, ALWAYS_RESET)

/*
 * DECNRCM - NRCS mode
 * Operates in 7-bit (set) or 8-bit (reset) mode.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECNRCM,    42, ALWAYS_RESET)

MODE_FIXED(DECGEPM,    43, ALWAYS_RESET) /* from VT330 */
/* MODE_FIXED(DECGPCM,    44, ALWAYS_RESET) * from VT330, conflicts with XTERM_MARGIN_BELL */
/* MODE_FIXED(DECGPCS,    45, ALWAYS_RESET) * from VT330, conflicts with XTERM_REVERSE_WRAP */
/* MODE_FIXED(DECGPBM,    46, ALWAYS_RESET) * from VT330, conflicts with XTERM_LOGGING */
/* MODE_FIXED(DECGRPM,    47, ALWAYS_RESET) * from VT330, conflicts with XTERM_ALTBUF */
MODE_FIXED(DEC131TM,   53, ALWAYS_RESET)

/*
 * DECNAKB - greek/north-american keyboard mapping mode
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECNAKB,    57, ALWAYS_RESET)

/*
 * DECIPEM - enter/return to/from pro-printer emulation mode
 * Switches the terminal to (set)/from (reset) the ibm pro
 * printer protocol.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECIPEM,    58, ALWAYS_RESET)

/* MODE_FIXED(DECKKDM,    59, ALWAYS_SET) * Kanji/Katakana Display Mode, from VT382-Kanji */

/*
 * DECHCCM - horizontal cursor coupling mode
 * Controls what happens when the cursor moves out of the left or
 * right margins of the window.
 * If set, the window pans to keep the cursor in view; if reset,
 * the cursor disappears.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECHCCM,    60, ALWAYS_RESET)

/*
 * DECVCCM - vertical cursor coupling mode
 * Controls what happens when the cursor moves out of the top or
 * bottom of the window, When the height of the window is smaller
 * than the page.
 * If set, the window pans to keep the cursor in view; if reset,
 * the cursor disappears.
 *
 * Default: set
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECVCCM, 61, ALWAYS_SET)

/*
 * DECPCCM - page cursor coupling mode
 *
 * Default: set
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECPCCM, 64, ALWAYS_SET)

/*
 * DECBKM - backarrow key mode
 * WYDELKM
 *
 * If set, the Backspace key works as a backspace key
 * sending the BS control; if reset, it works as a Delete
 * key sending the DEL control.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECBKM,     67, ALWAYS_RESET)

/*
 * DECKBUM - typewriter/data rpocessing keys mode
 *
 * If set, the keyboard keys act as data processing keys;
 * if reset, as typewriter keys.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECKBUM,    68, ALWAYS_RESET)

/*
 * DECLRMM - vertical split-screen mode
 * Controls whether a DECSLRM is executed.
 * On set, resets line attributes to single width and single height,
 * and while set, the terminal ignores any changes to line attributes.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Needs to be implemented if DECSLRM is implemented, to resolve a
 * conflict between DECSLRM and SCOSC.
 */
MODE_FIXED(DECLRMM, 69, ALWAYS_RESET) /* aka DECVSSM */

/*
 * DECXRLM - transmit rate limit
 * If set, limits the transmit rate; if reset, the rate is
 * unlimited.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECXRLM,    73, ALWAYS_RESET)

/*
 * DECSDM - sixel display mode
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
/* MODE_FIXED(DECSDM,    80, ALWAYS_RESET) ! Conflicts with WY161 */

/*
 * DECKPM - key position mode
 * If set, the keyboard sends extended reports (DECEKBD) that include
 * the key position and modifier state; if reset, it sends character codes.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECKPM,     81, ALWAYS_RESET)

MODE_FIXED(DECTHAISCM, 90, ALWAYS_RESET) /* Thai Space Compensating Mode, from VT382-Thai */

/*
 * DECNCSM - no clear screen on DECOLM
 * If set, the screen is not cleared when the column mode changes
 * by DECCOLM.
 * Note that this does not affect DECSCPP.
 *
 * Default: set
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECNCSM,    95, ALWAYS_RESET)

/*
 * DECRLCM - RTL copy mode
 * If set, copy/paste from RTL; if reset, from LTR.
 * Only enabled when the keyboard language is set to hebrew.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECRLCM,    96, ALWAYS_RESET)

/*
 * DECCRTSM - CRT save mode
 * When set, blanks the terminal after the inactivity timeout
 * (set with DECCRTST).
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECRCRTSM,  97, ALWAYS_RESET)

/*
 * DECARSM - auto resize mode
 * Sets whether changing page arrangements automatically
 * changes the lines per screen.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECARSM,    98, ALWAYS_RESET)

/*
 * DECMCM - modem control mode
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECMCM,     99, ALWAYS_RESET)

/*
 * DECAAM - auto answerback mode
 *
 * Default: reset
 *
 * References: VT525
 */
MODE_FIXED(DECAAM,    100, ALWAYS_RESET)

/*
 * DECCANSM - conceal answerback message mode
 *
 * Default: reset
 *
 * References: VT525
 *
 * Unimplemented, since we don't support answerback at all.
 */
MODE_FIXED(DECANSM,   101, ALWAYS_RESET)

/*
 * DECNULM - null mode
 * If set, pass NUL to the printer; if reset, discard NUL.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECNULM,   102, ALWAYS_RESET)

/*
 * DECHDPXM - half-duplex mode
 * Whether to use half-duplex (set) or full-duplex (reset) mode.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECHDPXM,  103, ALWAYS_RESET)

/*
 * DECESKM - enable secondary keyboard language mode
 * If set, use the secondary keyboard mapping (group 2); if reset,
 * use the primary (group 1).
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECESKM,   104, ALWAYS_RESET)

/*
 * DECOSCNM - overscan mode
 * (monochrome terminal only)
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECOSCNM,  106, ALWAYS_RESET)

/*
 * DECNUMLK - num lock mode
 *
 * Set the num lock state as if by acting the NumLock key.
 * Set means NumLock on; reset means off.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECNUMLK, 108, ALWAYS_RESET)

/*
 * DECCAPSLK - caps lock mode
 *
 * Set the caps lock state as if by acting the CapsLock key.
 * Set means CapsLock on; reset means off.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECCAPSLK, 109, ALWAYS_RESET)

/*
 * DECKLHIM - keyboard LED host indicator mode
 * If set, the keyboard LEDs show the state from the host
 * (see DECLL); if reset, the local state.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECKLHIM, 110, ALWAYS_RESET)

/*
 * DECFWM - framed window mode
 * If set, session window frames are drawn with frame border and icon.
 *
 * Default: reset
 *
 * References: VT525
 *
 * VTE does not support sessions.
 */
MODE_FIXED(DECFWM,    111, ALWAYS_RESET)

/*
 * DECRPL - review previous lines mode
 * If set, allows to view the scrollback.
 *
 * Default: set (VTE)
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECRPL, 112, ALWAYS_SET)

/*
 * DECHWUM - host wake-up mode
 * If set, the terminal exits CRT save and energy save mode
 * when a character is received from the host.
 *
 * Default: ?
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECHWUM,   113, ALWAYS_RESET)

/*
 * DECTCUM - alternate text color underline mode
 *
 * If set, text with the undeerline attribute is underlined as
 * well as being displayed in the alternate coolor (if
 * specified); if reset, it is only displayed in the
 * alternate color.
 *
 * Default: ?
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECATCUM,  114, ALWAYS_RESET)

/*
 * DECTCBM - alternate text color blink mode
 *
 * If set, text with the blink attribute blinks as well
 * as being displayed in the alternate color (if
 * specified); if reset, it is only displayed in the
 * alternate color.
 *
 * Default: ?
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECATCBM,  115, ALWAYS_RESET)

/*
 * DECBBSM - bold and blink style mode
 *
 * If set, the bold or blink attributes affect both foreground
 * and background color; if reset, those affect only the foreground
 * color.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECBBSM,   116, ALWAYS_RESET)

/*
 * DECECM - erase color mode
 *
 * If set, erased text or new cells appearing on the screen by scrolling
 * are assigned the screen background color; if reset, they are assigned
 * the text background color.
 *
 * Default: reset
 *
 * References: VT525
 *
 * Probably not worth implementing.
 */
MODE_FIXED(DECECM,    117, ALWAYS_RESET)

/* DRCSTerm */
/* Modes 8800…8804 */

/* KITTY */

MODE_FIXED(KITTY_STYLED_UNDERLINES, 2016, ALWAYS_SET)
MODE_FIXED(KITTY_EXTENDED_KEYBOARD, 2017, ALWAYS_RESET)

/* MinTTY */

MODE_FIXED(MINTTY_REPORT_CJK_AMBIGUOUS_WIDTH,           7700, ALWAYS_RESET)
MODE_FIXED(MINTTY_REPORT_SCROLL_MARKER_IN_CURRENT_LINE, 7711, ALWAYS_RESET)
MODE_FIXED(MINTTY_APPLICATION_ESCAPE,                   7727, ALWAYS_RESET)
MODE_FIXED(MINTTY_ESCAPE_SENDS_FS,                      7728, ALWAYS_RESET)
MODE_FIXED(MINTTY_SIXEL_SCROLLING_END_POSITION,         7730, ALWAYS_RESET)
MODE_FIXED(MINTTY_SCROLLBAR,                            7766, ALWAYS_RESET)
MODE_FIXED(MINTTY_REPORT_FONT_CHANGES,                  7767, ALWAYS_RESET)
MODE_FIXED(MINTTY_SHORTCUT_OVERRIDE,                    7783, ALWAYS_RESET)
MODE_FIXED(MINTTY_ALBUF_MOUSEWHEEL_TO_CURSORKEYS,       7786, ALWAYS_RESET)
MODE_FIXED(MINTTY_MOUSEWHEEL_APPLICATION_KEYS,          7787, ALWAYS_RESET)
MODE_FIXED(MINTTY_BIDI_DISABLE_IN_CURRENT_LINE,         7796, ALWAYS_RESET)
MODE_FIXED(MINTTY_SIXEL_SCROLL_CURSOR_RIGHT,            8452, ALWAYS_RESET)
/* MinTTY also knows mode 77096 'BIDI disable", and 77000..77031
 * "Application control key" which are outside of the supported range
 * for CSI parameters.
 */

/* RLogin */

/* RLogin appears to use many modes
 * [https://github.com/kmiya-culti/RLogin/blob/master/RLogin/TextRam.h#L131]:
 * 1406..1415, 1420..1425, 1430..1434, 1436, 1452..1481,
 * 8400..8406, 8416..8417, 8428..8429, 8435, 8437..8443,
 * 8446..8458,
 * and modes 7727, 7786, 8200 (home cursor on [ED 2]),
 * 8800 (some weird Unicode plane 17 mapping?), 8840 (same as 8428).
 *
 * We're not going to implement them, but avoid these ranges
 * when assigning new mode numbers.
 *
 * The following are the ones from RLogin that MLTerm knows about:
 */

/* MODE_FIXED(RLOGIN_APPLICATION_ESCAPE,                7727, ALWAYS_RESET) */
/* MODE_FIXED(RLOGIN_MOUSEWHEEL_TO_CURSORKEYS,          7786, ALWAYS_RESET) */

/* Ambiguous-width characters are wide (reset) or narrow (set) */
MODE_FIXED(RLOGIN_AMBIGUOUS_WIDTH_CHARACTERS_NARROW, 8428, ALWAYS_RESET)

/* MODE_FIXED(RLOGIN_CURSOR_TO_RIGHT_OF_SIXEL,          8452, ALWAYS_RESET) */

/* XTERM also knows this one */
/* MODE_FIXED(RLOGIN_SIXEL_SCROLL_CURSOR_RIGHT,         8452, ALWAYS_RESET) */

/* RXVT */

MODE_FIXED(RXVT_TOOLBAR,            10, ALWAYS_RESET)
MODE_FIXED(RXVT_SCROLLBAR,          30, ALWAYS_RESET)
/* MODE_FIXED(RXVT_SHIFT_KEYS,        35, ALWAYS_RESET) ! Conflicts with DECHEBM */
MODE_FIXED(RXVT_SCROLL_OUTPUT,    1010, ALWAYS_RESET)
MODE_FIXED(RXVT_SCROLL_KEYPRESS,  1011, ALWAYS_RESET)
MODE_FIXED(RXVT_MOUSE_EXT,        1015, ALWAYS_RESET)
/* Bold/blink uses normal (reset) or high intensity (set) colour */
MODE_FIXED(RXVT_INTENSITY_STYLES, 1021, ALWAYS_SET)

/* Wyse */

/*
 * WYTEK - TEK 4010/4014 personality
 * If set, switches to TEK 4010/4014 personality.
 *
 * Default: reset
 *
 * References: WY370
 */
MODE_FIXED(WYTEK,  38, ALWAYS_RESET)

/*
 * WY161 - 161 column mode
 * If set, switches the terminal to 161 columns; if reset,
 * to 80 columns.
 *
 * Default: reset
 *
 * References: WY370
 */
MODE_FIXED(WY161,  80, ALWAYS_RESET)

/*
 * WY52 - 52 lines mode
 * If set, switches the terminal to 52 lines; if reset,
 * to 24 lines.
 *
 * Default: reset
 *
 * References: WY370
 */
MODE_FIXED(WY52,   83, ALWAYS_RESET)

/*
 * WYENAT - enable separate attributes
 * If set, SGR attributes may be set separately for eraseable
 * and noneraseable characters. If reset, the same SGR attributes
 * apply to both eraseable and noneraseable characters.
 *
 *
 * Default: reset
 *
 * References: WY370
 */
MODE_FIXED(WYENAT, 84, ALWAYS_RESET)

/*
 * WYREPL - replacement character color
 *
 * Default: reset
 *
 * References: WY370
 */
MODE_FIXED(WYREPL, 85, ALWAYS_RESET)

/* XTERM */

MODE_FIXED(XTERM_ATT610_BLINK,                    12, ALWAYS_RESET)
MODE_FIXED(XTERM_CURSOR_BLINK,                    13, ALWAYS_RESET)
MODE_FIXED(XTERM_CURSOR_BLINK_XOR,                14, ALWAYS_RESET)
MODE_FIXED(XTERM_CURSES_HACK,                     41, ALWAYS_RESET)
MODE_FIXED(XTERM_MARGIN_BELL,                     44, ALWAYS_RESET)
MODE_FIXED(XTERM_REVERSE_WRAP,                    45, ALWAYS_RESET)
MODE_FIXED(XTERM_LOGGING,                         46, ALWAYS_RESET)
MODE_FIXED(XTERM_MOUSE_EXT,                     1005, ALWAYS_RESET)
MODE_FIXED(XTERM_8BIT_META,                     1034, ALWAYS_RESET)
MODE_FIXED(XTERM_NUMLOCK,                       1035, ALWAYS_RESET)
MODE_FIXED(XTERM_DELETE_IS_DEL,                 1037, ALWAYS_RESET)
MODE_FIXED(XTERM_ALT_SENDS_ESCAPE,              1039, ALWAYS_RESET)
MODE_FIXED(XTERM_KEEP_SELECTION,                1040, ALWAYS_RESET)
MODE_FIXED(XTERM_KEEP_CLIPBOARD,                1044, ALWAYS_RESET)
MODE_FIXED(XTERM_SELECT_TO_CLIPBOARD,           1041, ALWAYS_RESET)
MODE_FIXED(XTERM_BELL_URGENT,                   1042, ALWAYS_RESET)
MODE_FIXED(XTERM_PRESENT_ON_BELL,               1043, ALWAYS_RESET)
MODE_FIXED(XTERM_ALLOW_ALTBUF,                  1046, ALWAYS_SET)
MODE_FIXED(XTERM_FKEYS_TERMCAP,                 1050, ALWAYS_RESET)
MODE_FIXED(XTERM_FKEYS_SUN,                     1051, ALWAYS_RESET)
MODE_FIXED(XTERM_FKEYS_HP,                      1052, ALWAYS_RESET)
MODE_FIXED(XTERM_FKEYS_SCO,                     1053, ALWAYS_RESET)
MODE_FIXED(XTERM_FKEYS_LEGACY,                  1060, ALWAYS_RESET)
MODE_FIXED(XTERM_FKEYS_VT220,                   1061, ALWAYS_RESET)
MODE_FIXED(XTERM_SIXEL_PRIVATE_COLOR_REGISTERS, 1070, ALWAYS_SET)
MODE_FIXED(XTERM_READLINE_BUTTON1_MOVE_POINT,   2001, ALWAYS_RESET)
MODE_FIXED(XTERM_READLINE_BUTTON2_MOVE_POINT,   2002, ALWAYS_RESET)
MODE_FIXED(XTERM_READLINE_DBLBUTTON3_DELETE,    2003, ALWAYS_RESET)
MODE_FIXED(XTERM_READLINE_PASTE_QUOTE,          2005, ALWAYS_RESET)
MODE_FIXED(XTERM_READLINE_PASTE_LITERAL_NL,     2006, ALWAYS_RESET)
