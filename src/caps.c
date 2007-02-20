/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "../config.h"
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

/* This list combined from the Linux termcap(5) man page, and
 * termcap_&_terminfo by Strang, Mui, and O'Reilly. */
struct _vte_capability_quark _vte_terminal_capability_strings[] = {
	{"!1", TRUE, 0},
	{"!2", TRUE, 0},
	{"!3", TRUE, 0},

	{"#1", TRUE, 0},
	{"#2", TRUE, 0},
	{"#3", TRUE, 0},
	{"#4", TRUE, 0},

	{"%0", TRUE, 0},
	{"%1", TRUE, 0},
	{"%2", TRUE, 0},
	{"%3", TRUE, 0},
	{"%4", TRUE, 0},
	{"%5", TRUE, 0},
	{"%6", TRUE, 0},
	{"%7", TRUE, 0},
	{"%8", TRUE, 0},
	{"%9", TRUE, 0},
	{"%a", TRUE, 0},
	{"%b", TRUE, 0},
	{"%c", TRUE, 0},
	{"%d", TRUE, 0},
	{"%e", TRUE, 0},
	{"%f", TRUE, 0},
	{"%g", TRUE, 0},
	{"%h", TRUE, 0},
	{"%i", TRUE, 0},
	{"%j", TRUE, 0},

	{"&0", TRUE, 0},
	{"&1", TRUE, 0},
	{"&2", TRUE, 0},
	{"&3", TRUE, 0},
	{"&4", TRUE, 0},
	{"&5", TRUE, 0},
	{"&6", TRUE, 0},
	{"&7", TRUE, 0},
	{"&8", TRUE, 0},
	{"&9", TRUE, 0},

	{"*0", TRUE, 0},
	{"*1", TRUE, 0},
	{"*2", TRUE, 0},
	{"*3", TRUE, 0},
	{"*4", TRUE, 0},
	{"*5", TRUE, 0},
	{"*6", TRUE, 0},
	{"*7", TRUE, 0},
	{"*8", TRUE, 0},
	{"*9", TRUE, 0},

	{"@0", TRUE, 0},
	{"@1", TRUE, 0},
	{"@2", TRUE, 0},
	{"@3", TRUE, 0},
	{"@4", TRUE, 0},
	{"@5", TRUE, 0},
	{"@6", TRUE, 0},
	{"@7", TRUE, 0},
	{"@8", TRUE, 0},
	{"@9", TRUE, 0},

	{"ae", FALSE, 0},
	{"al", FALSE, 0},
	{"AL", FALSE, 0},
	{"as", FALSE, 0},

	{"bc", FALSE, 0},
	{"bl", FALSE, 0},
	{"bt", FALSE, 0},

	{"cb", FALSE, 0},
	{"cc", FALSE, 0},
	{"cd", FALSE, 0},
	{"ce", FALSE, 0},
	{"ch", FALSE, 0},
	{"cl", FALSE, 0},
	{"cm", FALSE, 0},
	{"CM", FALSE, 0},
	{"cr", FALSE, 0},
	{"cs", FALSE, 0},
	{"ct", FALSE, 0},
	{"cv", FALSE, 0},

	{"dc", FALSE, 0},
	{"DC", FALSE, 0},
	{"dl", FALSE, 0},
	{"DL", FALSE, 0},
	{"dm", FALSE, 0},
	{"do", FALSE, 0},
	{"DO", FALSE, 0},
	{"ds", FALSE, 0},

	{"eA", FALSE, 0},
	{"ec", FALSE, 0},
	{"ed", FALSE, 0},
	{"ei", FALSE, 0},

	{"F1", TRUE, 0},
	{"F2", TRUE, 0},
	{"F3", TRUE, 0},
	{"F4", TRUE, 0},
	{"F5", TRUE, 0},
	{"F6", TRUE, 0},
	{"F7", TRUE, 0},
	{"F8", TRUE, 0},
	{"F9", TRUE, 0},
	{"FA", TRUE, 0},
	{"FB", TRUE, 0},
	{"FC", TRUE, 0},
	{"FD", TRUE, 0},
	{"FE", TRUE, 0},
	{"FF", TRUE, 0},
	{"FG", TRUE, 0},
	{"FH", TRUE, 0},
	{"FI", TRUE, 0},
	{"FJ", TRUE, 0},
	{"FK", TRUE, 0},
	{"FL", TRUE, 0},
	{"FM", TRUE, 0},
	{"FN", TRUE, 0},
	{"FO", TRUE, 0},
	{"FP", TRUE, 0},
	{"FQ", TRUE, 0},
	{"FR", TRUE, 0},
	{"FS", TRUE, 0},
	{"FT", TRUE, 0},
	{"FU", TRUE, 0},
	{"FV", TRUE, 0},
	{"FW", TRUE, 0},
	{"FX", TRUE, 0},
	{"FY", TRUE, 0},
	{"FZ", TRUE, 0},

	{"Fa", TRUE, 0},
	{"Fb", TRUE, 0},
	{"Fc", TRUE, 0},
	{"Fd", TRUE, 0},
	{"Fe", TRUE, 0},
	{"Ff", TRUE, 0},
	{"Fg", TRUE, 0},
	{"Fh", TRUE, 0},
	{"Fi", TRUE, 0},
	{"Fj", TRUE, 0},
	{"Fk", TRUE, 0},
	{"Fl", TRUE, 0},
	{"Fm", TRUE, 0},
	{"Fn", TRUE, 0},
	{"Fo", TRUE, 0},
	{"Fp", TRUE, 0},
	{"Fq", TRUE, 0},
	{"Fr", TRUE, 0},

	{"ff", FALSE, 0},
	{"fs", FALSE, 0},

	{"hd", FALSE, 0},
	{"ho", FALSE, 0},
	{"hu", FALSE, 0},

	{"i1", FALSE, 0},
	{"i3", FALSE, 0},

	{"ic", FALSE, 0},
	{"IC", FALSE, 0},
	{"if", FALSE, 0},
	{"im", FALSE, 0},
	{"ip", FALSE, 0},
	{"iP", FALSE, 0},
	{"is", FALSE, 0},

	{"K1", TRUE, 0},
	{"K2", TRUE, 0},
	{"K3", TRUE, 0},
	{"K4", TRUE, 0},
	{"K5", TRUE, 0},

	{"k0", TRUE, 0},
	{"k1", TRUE, 0},
	{"k2", TRUE, 0},
	{"k3", TRUE, 0},
	{"k4", TRUE, 0},
	{"k5", TRUE, 0},
	{"k6", TRUE, 0},
	{"k7", TRUE, 0},
	{"k8", TRUE, 0},
	{"k9", TRUE, 0},
	{"k;", TRUE, 0},
	{"ka", TRUE, 0},
	{"kA", TRUE, 0},
	{"kb", TRUE, 0},
	{"kB", TRUE, 0},
	{"kC", TRUE, 0},
	{"kd", TRUE, 0},
	{"kD", TRUE, 0},
	{"ke", TRUE, 0},
	{"kE", TRUE, 0},
	{"kF", TRUE, 0},
	{"kh", TRUE, 0},
	{"kH", TRUE, 0},
	{"kI", TRUE, 0},
	{"kl", TRUE, 0},
	{"kL", TRUE, 0},
	{"kM", TRUE, 0},
	{"kN", TRUE, 0},
	{"kP", TRUE, 0},
	{"kr", TRUE, 0},
	{"kR", TRUE, 0},
	{"ks", TRUE, 0},
	{"kS", TRUE, 0},
	{"kt", TRUE, 0},
	{"kT", TRUE, 0},
	{"ku", TRUE, 0},

	{"l0", FALSE, 0},
	{"l1", FALSE, 0},
	{"l2", FALSE, 0},
	{"l3", FALSE, 0},
	{"l4", FALSE, 0},
	{"l5", FALSE, 0},
	{"l6", FALSE, 0},
	{"l7", FALSE, 0},
	{"l8", FALSE, 0},
	{"l9", FALSE, 0},

	{"la", FALSE, 0},
	{"le", FALSE, 0},
	{"LE", FALSE, 0},
	{"LF", FALSE, 0},
	{"ll", FALSE, 0},
	{"LO", FALSE, 0},

	{"mb", FALSE, 0},
	{"MC", FALSE, 0},
	{"md", FALSE, 0},
	{"me", FALSE, 0},
	{"mh", FALSE, 0},
	{"mk", FALSE, 0},
	{"ml", FALSE, 0},
	{"ML", FALSE, 0},
	{"mm", FALSE, 0},
	{"mo", FALSE, 0},
	{"mp", FALSE, 0},
	{"mr", FALSE, 0},
	{"MR", FALSE, 0},
	{"mu", FALSE, 0},

	{"nd", FALSE, 0},
	{"nl", FALSE, 0},
	{"nw", FALSE, 0},

	{"pc", FALSE, 0},
	{"pf", FALSE, 0},
	{"pk", FALSE, 0},
	{"pl", FALSE, 0},
	{"pn", FALSE, 0},
	{"po", FALSE, 0},
	{"pO", FALSE, 0},
	{"ps", FALSE, 0},
	{"px", FALSE, 0},

	{"r1", FALSE, 0},
	{"r2", FALSE, 0},
	{"r3", FALSE, 0},

	{"RA", FALSE, 0},
	{"rc", FALSE, 0},
	{"rf", FALSE, 0},
	{"RF", FALSE, 0},
	{"RI", FALSE, 0},
	{"rp", FALSE, 0},
	{"rP", FALSE, 0},
	{"rs", FALSE, 0},
	{"RX", FALSE, 0},

	{"s0", FALSE, 0},
	{"s1", FALSE, 0},
	{"s2", FALSE, 0},
	{"s3", FALSE, 0},

	{"sa", FALSE, 0},
	{"SA", FALSE, 0},
	{"sc", FALSE, 0},
	{"se", FALSE, 0},
	{"sf", FALSE, 0},
	{"SF", FALSE, 0},
	/* {"so", 0}, standout is always the same as another attribute. */
	{"sr", FALSE, 0},
	{"SR", FALSE, 0},
	{"st", FALSE, 0},
	{"SX", FALSE, 0},

	{"ta", FALSE, 0},
	/* {"te", 0}, terminal end-use is "logical". */
	/* {"ti", 0}, terminal init is "logical". */
	{"ts", FALSE, 0},

	{"uc", FALSE, 0},
	{"ue", FALSE, 0},
	{"up", FALSE, 0},
	{"UP", FALSE, 0},
	{"us", FALSE, 0},

	{"vb", FALSE, 0},
	/* {"ve", FALSE, 0}, */
	{"vi", FALSE, 0},
	/* {"vs", FALSE, 0}, */

	{"wi", FALSE, 0},

	{"XF", FALSE, 0},
	{"XN", FALSE, 0},


	{"", FALSE, 0}
};

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
	{OSC "4;%s" BEL, "change-color", 0},
	{OSC "10;%s" BEL, "change-foreground-colors", 0},
	{OSC "11;%s" BEL, "change-background-colors", 0},
	{OSC "12;%s" BEL, "change-cursor-colors", 0},
	{OSC "13;%s" BEL, "change-mouse-cursor-foreground-colors", 0},
	{OSC "14;%s" BEL, "change-mouse-cursor-foreground-colors", 0},
	{OSC "15;%s" BEL, "change-tek-background-colors", 0},
	{OSC "16;%s" BEL, "change-tek-background-colors", 0},
	{OSC "17;%s" BEL, "change-highlight-colors", 0},
	{OSC "46;%s" BEL, "change-logfile", 0},
	{OSC "50;#%d" BEL, "change-font-number", 0},
	{OSC "50;%s" BEL, "change-font-name", 0},

	/* Set text parameters, ST-terminated versions. */
	{OSC ";%s" ST, "set-icon-and-window-title", 0}, /* undocumented default */
	{OSC "0;%s" ST, "set-icon-and-window-title", 0},
	{OSC "1;%s" ST, "set-icon-title", 0},
	{OSC "2;%s" ST, "set-window-title", 0},
	{OSC "3;%s" ST, "set-xproperty", 0},
	{OSC "4;%s" ST, "change-color", 0},
	{OSC "10;%s" ST, "change-foreground-colors", 0},
	{OSC "11;%s" ST, "change-background-colors", 0},
	{OSC "12;%s" ST, "change-cursor-colors", 0},
	{OSC "13;%s" ST, "change-mouse-cursor-foreground-colors", 0},
	{OSC "14;%s" ST, "change-mouse-cursor-foreground-colors", 0},
	{OSC "15;%s" ST, "change-tek-background-colors", 0},
	{OSC "16;%s" ST, "change-tek-background-colors", 0},
	{OSC "17;%s" ST, "change-highlight-colors", 0},
	{OSC "46;%s" ST, "change-logfile", 0},
	{OSC "50;#%d" ST, "change-font-number", 0},
	{OSC "50;%s" ST, "change-font-name", 0},

	/* These may be bogus, I can't find docs for them anywhere (#104154). */
	{OSC "21;%s" BEL, "set-text-property-21", 0},
	{OSC "2L;%s" BEL, "set-text-property-2L", 0},
	{OSC "21;%s" ST, "set-text-property-21", 0},
	{OSC "2L;%s" ST, "set-text-property-2L", 0},

	{NULL, NULL, 0},
};

/**
 * vte_capability_init:
 *
 * Initializes the vte_terminal_capability_strings and
 * vte_xterm_capability_strings structures used by the terminal.  Can
 * be called multiple times without ill effect.
 *
 * Returns: void
 */

void
_vte_capability_init(void)
{
	unsigned int i;
	for (i = 0; _vte_terminal_capability_strings[i].capability[0]; i++) {
		const char *tmp;
		GQuark quark;
		tmp = _vte_terminal_capability_strings[i].capability;
		quark = g_quark_from_static_string(tmp);
		_vte_terminal_capability_strings[i].quark = quark;
	}
	for (i = 0; i < G_N_ELEMENTS(_vte_xterm_capability_strings); i++) {
		const char *tmp;
		GQuark quark;
		tmp = _vte_xterm_capability_strings[i].value;
		if (tmp != NULL) {
			quark = g_quark_from_static_string(tmp);
			_vte_xterm_capability_strings[i].quark = quark;
		}
	}
}
