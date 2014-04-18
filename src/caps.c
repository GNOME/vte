/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>
#include <stdlib.h>
#include <glib.h>
#include "caps.h"

#define ESC _VTE_CAP_ESC
#define CSI _VTE_CAP_CSI
#define ST  _VTE_CAP_ST
#define OSC _VTE_CAP_OSC
#define PM  _VTE_CAP_PM
#define APC _VTE_CAP_APC

#define ENQ "\005"
#define BEL "\007"
#define BS  "\010"
#define TAB "\011"
#define LF  "\012"
#define VT  "\013"
#define FF  "\014"
#define CR  "\015"
#define SO  "\016"
#define SI  "\017"

/* From some really old XTerm docs we had at the office, and an updated
 * version at Moy, Gildea, and Dickey. */
struct _vte_capability_string _vte_xterm_capability_strings[] = {
	{ENQ, "return-terminal-status", 0},
	{VT,  "vertical-tab", 0},
	{FF,  "form-feed", 0},

	{ESC " F", "7-bit-controls", 0},
	{ESC " G", "8-bit-controls", 0},
	{ESC " L", "ansi-conformance-level-1", 0},
	{ESC " M", "ansi-conformance-level-2", 0},
	{ESC " N", "ansi-conformance-level-3", 0},
	{ESC "#3", "double-height-top-half", 0},
	{ESC "#4", "double-height-bottom-half", 0},
	{ESC "#5", "single-width", 0},
	{ESC "#6", "double-width", 0},
	{ESC "#8", "screen-alignment-test", 0},

	/* These are actually designate-other-coding-system from ECMA 35,
	 * but we don't support the full repertoire.  Actually, we don't
	 * know what the full repertoire looks like. */
	{ESC "%%@", "iso8859-1-character-set", 0},
	{ESC "%%G", "utf-8-character-set", 0},

	{ESC "7", "save-cursor", 0},
	{ESC "8", "restore-cursor", 0},
	{ESC "=", "application-keypad", 0},
	{ESC ">", "normal-keypad", 0},
	{ESC "D", "index", 0},
	{ESC "E", "next-line", 0},
	{ESC "F", "cursor-lower-left", 0},
	{ESC "H", "tab-set", 0},
	{ESC "M", "reverse-index", 0},
	/* {ESC "N", "single-shift-g2", 0}, */
	/* {ESC "O", "single-shift-g3", 0}, */
	{ESC "P%s" ESC "\\", "device-control-string", 0},
	{ESC "V", "start-of-guarded-area", 0},
	{ESC "W", "end-of-guarded-area", 0},
	{ESC "X", "start-of-string", 0},
	{ESC "\\", "end-of-string", 0},
	{ESC "Z", "return-terminal-id", 0},
	{ESC "c", "full-reset", 0},
	{ESC "l", "memory-lock", 0},
	{ESC "m", "memory-unlock", 0},
	/* {ESC "n", "invoke-g2-character-set", 0}, */
	/* {ESC "o", "invoke-g3-character-set", 0}, */
	/* {ESC "|", "invoke-g3-character-set-as-gr", 0}, */
	/* {ESC "}", "invoke-g2-character-set-as-gr", 0}, */
	/* {ESC "~", "invoke-g1-character-set-as-gr", 0}, */

	/* APC stuff omitted. */

	/* DCS stuff omitted. */

	{CSI "@", "insert-blank-characters", 0},
	{CSI "%d@", "insert-blank-characters", 0},
	{CSI "A", "cursor-up", 0},
	{CSI "%dA", "cursor-up", 0},
	{CSI "B", "cursor-down", 0},
	{CSI "%dB", "cursor-down", 0},
	{CSI "C", "cursor-forward", 0},
	{CSI "%dC", "cursor-forward", 0},
	{CSI "D", "cursor-backward", 0},
	{CSI "%dD", "cursor-backward", 0},
	{CSI "E", "cursor-next-line", 0},
	{CSI "%dE", "cursor-next-line", 0},
	{CSI "F", "cursor-preceding-line", 0},
	{CSI "%dF", "cursor-preceding-line", 0},
	{CSI "G", "cursor-character-absolute", 0},
	{CSI "%dG", "cursor-character-absolute", 0},
	{CSI ";H", "cursor-position", 0},
	{CSI "%dH", "cursor-position", 0},
	{CSI "%d;H", "cursor-position", 0},
	{CSI ";%dH", "cursor-position", 0},
	{CSI "%d;%dH", "cursor-position", 0},
	{CSI "I", "cursor-forward-tabulation", 0},
	{CSI "J", "erase-in-display", 0},
	{CSI "%dJ", "erase-in-display", 0},
	{CSI "?J", "selective-erase-in-display", 0},
	{CSI "?%dJ", "selective-erase-in-display", 0},
	{CSI "K", "erase-in-line", 0},
	{CSI "%dK", "erase-in-line", 0},
	{CSI "?K", "selective-erase-in-line", 0},
	{CSI "?%dK", "selective-erase-in-line", 0},
	{CSI "L", "insert-lines", 0},
	{CSI "%dL", "insert-lines", 0},
	{CSI "M", "delete-lines", 0},
	{CSI "%dM", "delete-lines", 0},
	{CSI "P", "delete-characters", 0},
	{CSI "%dP", "delete-characters", 0},
	{CSI "S", "scroll-up", 0},
	{CSI "%dS", "scroll-up", 0},
	{CSI "T", "scroll-down", 0},
	{CSI "%dT", "scroll-down", 0},
	{CSI "%d;%d;%d;%d;%dT", "initiate-hilite-mouse-tracking", 0},
	{CSI "X", "erase-characters", 0},
	{CSI "%dX", "erase-characters", 0},
	{CSI "Z", "cursor-back-tab", 0},
	{CSI "%dZ", "cursor-back-tab", 0},

	{CSI "`", "character-position-absolute", 0},
	{CSI "%d`", "character-position-absolute", 0},
	{CSI "b", "repeat", 0},
	{CSI "%db", "repeat", 0},
	{CSI "c", "send-primary-device-attributes", 0},
	{CSI "%dc", "send-primary-device-attributes", 0},
	{CSI ">c", "send-secondary-device-attributes", 0},
	{CSI ">%dc", "send-secondary-device-attributes", 0},
	{CSI "=c", "send-tertiary-device-attributes", 0},
	{CSI "=%dc", "send-tertiary-device-attributes", 0},
	{CSI "?%mc", "linux-console-cursor-attributes", 0},
	{CSI "d", "line-position-absolute", 0},
	{CSI "%dd", "line-position-absolute", 0},
	{CSI ";f", "horizontal-and-vertical-position", 0},
	{CSI "%d;f", "horizontal-and-vertical-position", 0},
	{CSI ";%df", "horizontal-and-vertical-position", 0},
	{CSI "%d;%df", "horizontal-and-vertical-position", 0},
	{CSI "g", "tab-clear", 0},
	{CSI "%dg", "tab-clear", 0},

	{CSI "%mh", "set-mode", 0},
	{CSI "?%mh", "decset", 0},

	{CSI "%mi", "media-copy", 0},
	{CSI "?%mi", "dec-media-copy", 0},

	{CSI "%ml", "reset-mode", 0},
	{CSI "?%ml", "decreset", 0},

	{CSI "%mm", "character-attributes", 0},

	{CSI "%dn", "device-status-report", 0},
	{CSI "?%dn", "dec-device-status-report", 0},
	{CSI "!p", "soft-reset", 0},
	{CSI "%d;%d\"p", "set-conformance-level", 0},
	{CSI "%d\"q", "select-character-protection", 0},

	{CSI "r", "set-scrolling-region", 0},
	{CSI ";r", "set-scrolling-region", 0},
	{CSI ";%dr", "set-scrolling-region-from-start", 0},
	{CSI "%dr", "set-scrolling-region-to-end", 0},
	{CSI "%d;r", "set-scrolling-region-to-end", 0},
	{CSI "%d;%dr", "set-scrolling-region", 0},

	{CSI "?%mr", "restore-mode", 0},
	{CSI "s", "save-cursor", 0},
	{CSI "?%ms", "save-mode", 0},
	{CSI "u", "restore-cursor", 0},

	{CSI "%mt", "window-manipulation", 0},

	{CSI "%d;%d;%d;%dw", "enable-filter-rectangle", 0},
	{CSI "%dx", "request-terminal-parameters", 0},
	{CSI "%d;%d'z", "enable-locator-reporting", 0},
	{CSI "%m'{", "select-locator-events", 0},
	{CSI "%d'|", "request-locator-position", 0},

	/* Set text parameters, BEL-terminated versions. */
	{OSC ";%s" BEL, "set-icon-and-window-title", 0}, /* undocumented default */
	{OSC "0;%s" BEL, "set-icon-and-window-title", 0},
	{OSC "1;%s" BEL, "set-icon-title", 0},
	{OSC "2;%s" BEL, "set-window-title", 0},
	{OSC "3;%s" BEL, "set-xproperty", 0},
	{OSC "4;%s" BEL, "change-color-bel", 0},
        {OSC "6;%s" BEL, "set-current-file-uri", 0},
        {OSC "7;%s" BEL, "set-current-directory-uri", 0},
	{OSC "10;%s" BEL, "change-foreground-colors-bel", 0},
	{OSC "11;%s" BEL, "change-background-colors-bel", 0},
	{OSC "12;%s" BEL, "change-cursor-colors-bel", 0},
	{OSC "13;%s" BEL, "change-mouse-cursor-foreground-colors-bel", 0},
	{OSC "14;%s" BEL, "change-mouse-cursor-background-colors-bel", 0},
	{OSC "15;%s" BEL, "change-tek-foreground-colors-bel", 0},
	{OSC "16;%s" BEL, "change-tek-background-colors-bel", 0},
	{OSC "17;%s" BEL, "change-highlight-background-colors-bel", 0},
	{OSC "18;%s" BEL, "change-tek-cursor-colors-bel", 0},
	{OSC "19;%s" BEL, "change-highlight-foreground-colors-bel", 0},
	{OSC "46;%s" BEL, "change-logfile", 0},
	{OSC "50;#%d" BEL, "change-font-number", 0},
	{OSC "50;%s" BEL, "change-font-name", 0},
	{OSC "104" BEL, "reset-color", 0},
	{OSC "104;%m" BEL, "reset-color", 0},
	{OSC "110" BEL, "reset-foreground-colors", 0},
	{OSC "111" BEL, "reset-background-colors", 0},
	{OSC "112" BEL, "reset-cursor-colors", 0},
	{OSC "113" BEL, "reset-mouse-cursor-foreground-colors", 0},
	{OSC "114" BEL, "reset-mouse-cursor-background-colors", 0},
	{OSC "115" BEL, "reset-tek-foreground-colors", 0},
	{OSC "116" BEL, "reset-tek-background-colors", 0},
	{OSC "117" BEL, "reset-highlight-background-colors", 0},
	{OSC "118" BEL, "reset-tek-cursor-colors", 0},
	{OSC "119" BEL, "reset-highlight-foreground-colors", 0},

	/* Set text parameters, ST-terminated versions. */
	{OSC ";%s" ST, "set-icon-and-window-title", 0}, /* undocumented default */
	{OSC "0;%s" ST, "set-icon-and-window-title", 0},
	{OSC "1;%s" ST, "set-icon-title", 0},
	{OSC "2;%s" ST, "set-window-title", 0},
	{OSC "3;%s" ST, "set-xproperty", 0},
	{OSC "4;%s" ST, "change-color-st", 0},
        {OSC "6;%s" ST, "set-current-file-uri", 0},
        {OSC "7;%s" ST, "set-current-directory-uri", 0},
	{OSC "10;%s" ST, "change-foreground-colors-st", 0},
	{OSC "11;%s" ST, "change-background-colors-st", 0},
	{OSC "12;%s" ST, "change-cursor-colors-st", 0},
	{OSC "13;%s" ST, "change-mouse-cursor-foreground-colors-st", 0},
	{OSC "14;%s" ST, "change-mouse-cursor-background-colors-st", 0},
	{OSC "15;%s" ST, "change-tek-foreground-colors-st", 0},
	{OSC "16;%s" ST, "change-tek-background-colors-st", 0},
	{OSC "17;%s" ST, "change-highlight-background-colors-st", 0},
	{OSC "18;%s" ST, "change-tek-cursor-colors-st", 0},
	{OSC "19;%s" ST, "change-highlight-foreground-colors-st", 0},
	{OSC "46;%s" ST, "change-logfile", 0},
	{OSC "50;#%d" ST, "change-font-number", 0},
	{OSC "50;%s" ST, "change-font-name", 0},
	{OSC "104" ST, "reset-color", 0},
	{OSC "104;%m" ST, "reset-color", 0},
	{OSC "110" ST, "reset-foreground-colors", 0},
	{OSC "111" ST, "reset-background-colors", 0},
	{OSC "112" ST, "reset-cursor-colors", 0},
	{OSC "113" ST, "reset-mouse-cursor-foreground-colors", 0},
	{OSC "114" ST, "reset-mouse-cursor-background-colors", 0},
	{OSC "115" ST, "reset-tek-foreground-colors", 0},
	{OSC "116" ST, "reset-tek-background-colors", 0},
	{OSC "117" ST, "reset-highlight-background-colors", 0},
	{OSC "118" ST, "reset-tek-cursor-colors", 0},
	{OSC "119" ST, "reset-highlight-foreground-colors", 0},

	/* These may be bogus, I can't find docs for them anywhere (#104154). */
	{OSC "21;%s" BEL, "set-text-property-21", 0},
	{OSC "2L;%s" BEL, "set-text-property-2L", 0},
	{OSC "21;%s" ST, "set-text-property-21", 0},
	{OSC "2L;%s" ST, "set-text-property-2L", 0},

	{NULL, NULL, 0},
};
