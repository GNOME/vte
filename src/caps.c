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
#define DEL "\177"

/* From some really old XTerm docs we had at the office, and an updated
 * version at Moy, Gildea, and Dickey. */
struct _vte_capability_string _vte_xterm_capability_strings[] = {
	{ENQ, "return-terminal-status"},
        {BEL, "bell"},
        {BS,  "backspace"},
        {TAB, "tab"},
        {LF,  "line-feed"},
	{VT,  "vertical-tab"},
	{FF,  "form-feed"},
        {CR,  "carriage-return"},
        {DEL, "backspace"},

	{ESC " F", "7-bit-controls"},
	{ESC " G", "8-bit-controls"},
	{ESC " L", "ansi-conformance-level-1"},
	{ESC " M", "ansi-conformance-level-2"},
	{ESC " N", "ansi-conformance-level-3"},
	{ESC "#3", "double-height-top-half"},
	{ESC "#4", "double-height-bottom-half"},
	{ESC "#5", "single-width"},
	{ESC "#6", "double-width"},
	{ESC "#8", "screen-alignment-test"},

	/* These are actually designate-other-coding-system from ECMA 35,
	 * but we don't support the full repertoire.  Actually, we don't
	 * know what the full repertoire looks like. */
	{ESC "%%@", "default-character-set"},
	{ESC "%%G", "utf-8-character-set"},

        {ESC "(0", "alternate-character-set-start"},
        {ESC "(B", "alternate-character-set-end"},

	{ESC "7", "save-cursor"},
	{ESC "8", "restore-cursor"},
	{ESC "=", "application-keypad"},
	{ESC ">", "normal-keypad"},
	{ESC "D", "index"},
	{ESC "E", "next-line"},
        /* {ESC "F", "cursor-lower-left"}, */
	{ESC "H", "tab-set"},
	{ESC "M", "reverse-index"},
	/* {ESC "N", "single-shift-g2"}, */
	/* {ESC "O", "single-shift-g3"}, */
	{ESC "P%s" ESC "\\", "device-control-string"},
	{ESC "V", "start-of-guarded-area"},
	{ESC "W", "end-of-guarded-area"},
	{ESC "X", "start-of-string"},
	{ESC "\\", "end-of-string"},
	{ESC "Z", "return-terminal-id"},
	{ESC "c", "full-reset"},
	{ESC "l", "memory-lock"},
	{ESC "m", "memory-unlock"},
	/* {ESC "n", "invoke-g2-character-set"}, */
	/* {ESC "o", "invoke-g3-character-set"}, */
	/* {ESC "|", "invoke-g3-character-set-as-gr"}, */
	/* {ESC "}", "invoke-g2-character-set-as-gr"}, */
	/* {ESC "~", "invoke-g1-character-set-as-gr"}, */

	/* APC stuff omitted. */

	/* DCS stuff omitted. */

	{CSI "@", "insert-blank-characters"},
	{CSI "%d@", "insert-blank-characters"},
	{CSI "A", "cursor-up"},
	{CSI "%dA", "cursor-up"},
	{CSI "B", "cursor-down"},
	{CSI "%dB", "cursor-down"},
	{CSI "C", "cursor-forward"},
	{CSI "%dC", "cursor-forward"},
	{CSI "D", "cursor-backward"},
	{CSI "%dD", "cursor-backward"},
	{CSI "E", "cursor-next-line"},
	{CSI "%dE", "cursor-next-line"},
	{CSI "F", "cursor-preceding-line"},
	{CSI "%dF", "cursor-preceding-line"},
	{CSI "G", "cursor-character-absolute"},
	{CSI "%dG", "cursor-character-absolute"},
        {CSI "H", "cursor-position"},
	{CSI ";H", "cursor-position"},
	{CSI "%dH", "cursor-position"},
	{CSI "%d;H", "cursor-position"},
        {CSI ";%dH", "cursor-position-top-row"},
	{CSI "%d;%dH", "cursor-position"},
	{CSI "I", "cursor-forward-tabulation"},
        {CSI "%dI", "cursor-forward-tabulation"},
	{CSI "J", "erase-in-display"},
	{CSI "%dJ", "erase-in-display"},
	{CSI "?J", "selective-erase-in-display"},
	{CSI "?%dJ", "selective-erase-in-display"},
	{CSI "K", "erase-in-line"},
	{CSI "%dK", "erase-in-line"},
	{CSI "?K", "selective-erase-in-line"},
	{CSI "?%dK", "selective-erase-in-line"},
	{CSI "L", "insert-lines"},
	{CSI "%dL", "insert-lines"},
	{CSI "M", "delete-lines"},
	{CSI "%dM", "delete-lines"},
	{CSI "P", "delete-characters"},
	{CSI "%dP", "delete-characters"},
	{CSI "S", "scroll-up"},
	{CSI "%dS", "scroll-up"},
	{CSI "T", "scroll-down"},
	{CSI "%dT", "scroll-down"},
	{CSI "%d;%d;%d;%d;%dT", "initiate-hilite-mouse-tracking"},
	{CSI "X", "erase-characters"},
	{CSI "%dX", "erase-characters"},
	{CSI "Z", "cursor-back-tab"},
	{CSI "%dZ", "cursor-back-tab"},

	{CSI "`", "character-position-absolute"},
	{CSI "%d`", "character-position-absolute"},
	{CSI "b", "repeat"},
	{CSI "%db", "repeat"},
	{CSI "c", "send-primary-device-attributes"},
	{CSI "%dc", "send-primary-device-attributes"},
	{CSI ">c", "send-secondary-device-attributes"},
	{CSI ">%dc", "send-secondary-device-attributes"},
	{CSI "=c", "send-tertiary-device-attributes"},
	{CSI "=%dc", "send-tertiary-device-attributes"},
	{CSI "?%mc", "linux-console-cursor-attributes"},
	{CSI "d", "line-position-absolute"},
	{CSI "%dd", "line-position-absolute"},
        {CSI "f", "cursor-position"},
        {CSI ";f", "cursor-position"},
        {CSI "%df", "cursor-position"},
        {CSI "%d;f", "cursor-position"},
        {CSI ";%df", "cursor-position-top-row"},
        {CSI "%d;%df", "cursor-position"},
	{CSI "g", "tab-clear"},
	{CSI "%dg", "tab-clear"},

	{CSI "%mh", "set-mode"},
	{CSI "?%mh", "decset"},

	{CSI "%mi", "media-copy"},
	{CSI "?%mi", "dec-media-copy"},

	{CSI "%ml", "reset-mode"},
	{CSI "?%ml", "decreset"},

	{CSI "%mm", "character-attributes"},

	{CSI "%dn", "device-status-report"},
	{CSI "?%dn", "dec-device-status-report"},
	{CSI "!p", "soft-reset"},
	{CSI "%d;%d\"p", "set-conformance-level"},
	{CSI "%d\"q", "select-character-protection"},

	{CSI "r", "set-scrolling-region"},
	{CSI ";r", "set-scrolling-region"},
	{CSI ";%dr", "set-scrolling-region-from-start"},
	{CSI "%dr", "set-scrolling-region-to-end"},
	{CSI "%d;r", "set-scrolling-region-to-end"},
	{CSI "%d;%dr", "set-scrolling-region"},

	{CSI "?%mr", "restore-mode"},
	{CSI "s", "save-cursor"},
	{CSI "?%ms", "save-mode"},
	{CSI "u", "restore-cursor"},

	{CSI "%mt", "window-manipulation"},

	{CSI "%d;%d;%d;%dw", "enable-filter-rectangle"},
	{CSI "%dx", "request-terminal-parameters"},
	{CSI "%d;%d'z", "enable-locator-reporting"},
	{CSI "%m'{", "select-locator-events"},
	{CSI "%d'|", "request-locator-position"},

	/* Set text parameters, BEL-terminated versions. */
	{OSC ";%s" BEL, "set-icon-and-window-title"}, /* undocumented default */
	{OSC "0;%s" BEL, "set-icon-and-window-title"},
	{OSC "1;%s" BEL, "set-icon-title"},
	{OSC "2;%s" BEL, "set-window-title"},
	{OSC "3;%s" BEL, "set-xproperty"},
	{OSC "4;%s" BEL, "change-color-bel"},
        {OSC "6;%s" BEL, "set-current-file-uri"},
        {OSC "7;%s" BEL, "set-current-directory-uri"},
	{OSC "10;%s" BEL, "change-foreground-color-bel"},
	{OSC "11;%s" BEL, "change-background-color-bel"},
	{OSC "12;%s" BEL, "change-cursor-color-bel"},
	{OSC "13;%s" BEL, "change-mouse-cursor-foreground-color-bel"},
	{OSC "14;%s" BEL, "change-mouse-cursor-background-color-bel"},
	{OSC "15;%s" BEL, "change-tek-foreground-color-bel"},
	{OSC "16;%s" BEL, "change-tek-background-color-bel"},
	{OSC "17;%s" BEL, "change-highlight-background-color-bel"},
	{OSC "18;%s" BEL, "change-tek-cursor-color-bel"},
	{OSC "19;%s" BEL, "change-highlight-foreground-color-bel"},
	{OSC "46;%s" BEL, "change-logfile"},
	{OSC "50;#%d" BEL, "change-font-number"},
	{OSC "50;%s" BEL, "change-font-name"},
	{OSC "104" BEL, "reset-color"},
	{OSC "104;%m" BEL, "reset-color"},
	{OSC "110" BEL, "reset-foreground-color"},
	{OSC "111" BEL, "reset-background-color"},
	{OSC "112" BEL, "reset-cursor-color"},
	{OSC "113" BEL, "reset-mouse-cursor-foreground-color"},
	{OSC "114" BEL, "reset-mouse-cursor-background-color"},
	{OSC "115" BEL, "reset-tek-foreground-color"},
	{OSC "116" BEL, "reset-tek-background-color"},
	{OSC "117" BEL, "reset-highlight-background-color"},
	{OSC "118" BEL, "reset-tek-cursor-color"},
	{OSC "119" BEL, "reset-highlight-foreground-color"},

	/* Set text parameters, ST-terminated versions. */
	{OSC ";%s" ST, "set-icon-and-window-title"}, /* undocumented default */
	{OSC "0;%s" ST, "set-icon-and-window-title"},
	{OSC "1;%s" ST, "set-icon-title"},
	{OSC "2;%s" ST, "set-window-title"},
	{OSC "3;%s" ST, "set-xproperty"},
	{OSC "4;%s" ST, "change-color-st"},
        {OSC "6;%s" ST, "set-current-file-uri"},
        {OSC "7;%s" ST, "set-current-directory-uri"},
	{OSC "10;%s" ST, "change-foreground-color-st"},
	{OSC "11;%s" ST, "change-background-color-st"},
	{OSC "12;%s" ST, "change-cursor-color-st"},
	{OSC "13;%s" ST, "change-mouse-cursor-foreground-color-st"},
	{OSC "14;%s" ST, "change-mouse-cursor-background-color-st"},
	{OSC "15;%s" ST, "change-tek-foreground-color-st"},
	{OSC "16;%s" ST, "change-tek-background-color-st"},
	{OSC "17;%s" ST, "change-highlight-background-color-st"},
	{OSC "18;%s" ST, "change-tek-cursor-color-st"},
	{OSC "19;%s" ST, "change-highlight-foreground-color-st"},
	{OSC "46;%s" ST, "change-logfile"},
	{OSC "50;#%d" ST, "change-font-number"},
	{OSC "50;%s" ST, "change-font-name"},
	{OSC "104" ST, "reset-color"},
	{OSC "104;%m" ST, "reset-color"},
	{OSC "110" ST, "reset-foreground-color"},
	{OSC "111" ST, "reset-background-color"},
	{OSC "112" ST, "reset-cursor-color"},
	{OSC "113" ST, "reset-mouse-cursor-foreground-color"},
	{OSC "114" ST, "reset-mouse-cursor-background-color"},
	{OSC "115" ST, "reset-tek-foreground-color"},
	{OSC "116" ST, "reset-tek-background-color"},
	{OSC "117" ST, "reset-highlight-background-color"},
	{OSC "118" ST, "reset-tek-cursor-color"},
	{OSC "119" ST, "reset-highlight-foreground-color"},

	/* These may be bogus, I can't find docs for them anywhere (#104154). */
	{OSC "21;%s" BEL, "set-text-property-21"},
	{OSC "2L;%s" BEL, "set-text-property-2L"},
	{OSC "21;%s" ST, "set-text-property-21"},
	{OSC "2L;%s" ST, "set-text-property-2L"},

	{NULL, NULL},
};
