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

#ident "$Id$"
#include "../config.h"
#include <stdlib.h>
#include <glib.h>
#include "caps.h"

#define ESC VTE_CAP_ESC
#define CSI VTE_CAP_CSI
#define ST  VTE_CAP_ST
#define OSC VTE_CAP_OSC
#define PM  VTE_CAP_PM
#define APC VTE_CAP_APC

#define BEL ""
#define BS  ""
#define TAB "	"
#define LF  "\n"
#define VT  ""
#define FF  ""
#define CR  "\r"
#define SO  ""
#define SI  ""

/* This list combined from the Linux termcap(5) man page, and
 * termcap_&_terminfo by Strang, Mui, and O'Reilly. */
struct vte_capability_quark vte_terminal_capability_strings[] = {
	{"!1", 0},
	{"!2", 0},
	{"!3", 0},

	{"#1", 0},
	{"#2", 0},
	{"#3", 0},
	{"#4", 0},

	{"%0", 0},
	{"%1", 0},
	{"%2", 0},
	{"%3", 0},
	{"%4", 0},
	{"%5", 0},
	{"%6", 0},
	{"%7", 0},
	{"%8", 0},
	{"%9", 0},
	{"%a", 0},
	{"%b", 0},
	{"%c", 0},
	{"%d", 0},
	{"%e", 0},
	{"%f", 0},
	{"%g", 0},
	{"%h", 0},
	{"%i", 0},
	{"%j", 0},

	{"&0", 0},
	{"&1", 0},
	{"&2", 0},
	{"&3", 0},
	{"&4", 0},
	{"&5", 0},
	{"&6", 0},
	{"&7", 0},
	{"&8", 0},
	{"&9", 0},

	{"*0", 0},
	{"*1", 0},
	{"*2", 0},
	{"*3", 0},
	{"*4", 0},
	{"*5", 0},
	{"*6", 0},
	{"*7", 0},
	{"*8", 0},
	{"*9", 0},

	{"@0", 0},
	{"@1", 0},
	{"@2", 0},
	{"@3", 0},
	{"@4", 0},
	{"@5", 0},
	{"@6", 0},
	{"@7", 0},
	{"@8", 0},
	{"@9", 0},

	{"ae", 0},
	{"al", 0},
	{"AL", 0},
	{"as", 0},

	{"bc", 0},
	{"bl", 0},
	{"bt", 0},

	{"cb", 0},
	{"cc", 0},
	{"cd", 0},
	{"ce", 0},
	{"ch", 0},
	{"cl", 0},
	{"cm", 0},
	{"CM", 0},
	{"cr", 0},
	{"cs", 0},
	{"ct", 0},
	{"cv", 0},

	{"dc", 0},
	{"DC", 0},
	{"dl", 0},
	{"DL", 0},
	{"dm", 0},
	{"do", 0},
	{"DO", 0},
	{"ds", 0},

	{"eA", 0},
	{"ec", 0},
	{"ed", 0},
	{"ei", 0},

	{"F1", 0},
	{"F2", 0},
	{"F3", 0},
	{"F4", 0},
	{"F5", 0},
	{"F6", 0},
	{"F7", 0},
	{"F8", 0},
	{"F9", 0},
	{"FA", 0},
	{"FB", 0},
	{"FC", 0},
	{"FD", 0},
	{"FE", 0},
	{"FF", 0},
	{"FG", 0},
	{"FH", 0},
	{"FI", 0},
	{"FJ", 0},
	{"FK", 0},
	{"FL", 0},
	{"FM", 0},
	{"FN", 0},
	{"FO", 0},
	{"FP", 0},
	{"FQ", 0},
	{"FR", 0},
	{"FS", 0},
	{"FT", 0},
	{"FU", 0},
	{"FV", 0},
	{"FW", 0},
	{"FX", 0},
	{"FY", 0},
	{"FZ", 0},

	{"Fa", 0},
	{"Fb", 0},
	{"Fc", 0},
	{"Fd", 0},
	{"Fe", 0},
	{"Ff", 0},
	{"Fg", 0},
	{"Fh", 0},
	{"Fi", 0},
	{"Fj", 0},
	{"Fk", 0},
	{"Fl", 0},
	{"Fm", 0},
	{"Fn", 0},
	{"Fo", 0},
	{"Fp", 0},
	{"Fq", 0},
	{"Fr", 0},

	{"ff", 0},
	{"fs", 0},

	{"hd", 0},
	{"ho", 0},
	{"hu", 0},

	{"i1", 0},
	{"i3", 0},

	{"ic", 0},
	{"IC", 0},
	{"if", 0},
	{"im", 0},
	{"ip", 0},
	{"iP", 0},
	{"is", 0},

	{"K1", 0},
	{"K2", 0},
	{"K3", 0},
	{"K4", 0},
	{"K5", 0},

	{"k0", 0},
	{"k1", 0},
	{"k2", 0},
	{"k3", 0},
	{"k4", 0},
	{"k5", 0},
	{"k6", 0},
	{"k7", 0},
	{"k8", 0},
	{"k9", 0},
	{"k;", 0},
	{"ka", 0},
	{"kA", 0},
	{"kb", 0},
	{"kB", 0},
	{"kC", 0},
	{"kd", 0},
	{"kD", 0},
	{"ke", 0},
	{"kE", 0},
	{"kF", 0},
	{"kh", 0},
	{"kH", 0},
	{"kI", 0},
	{"kl", 0},
	{"kL", 0},
	{"kM", 0},
	{"kN", 0},
	{"kP", 0},
	{"kr", 0},
	{"kR", 0},
	{"ks", 0},
	{"kS", 0},
	{"kt", 0},
	{"kT", 0},
	{"ku", 0},

	{"l0", 0},
	{"l1", 0},
	{"l2", 0},
	{"l3", 0},
	{"l4", 0},
	{"l5", 0},
	{"l6", 0},
	{"l7", 0},
	{"l8", 0},
	{"l9", 0},

	{"la", 0},
	{"le", 0},
	{"LE", 0},
	{"LF", 0},
	{"ll", 0},
	{"LO", 0},

	{"mb", 0},
	{"MC", 0},
	{"md", 0},
	{"me", 0},
	{"mh", 0},
	{"mk", 0},
	{"ml", 0},
	{"ML", 0},
	{"mm", 0},
	{"mo", 0},
	{"mp", 0},
	{"mr", 0},
	{"MR", 0},
	{"mu", 0},

	{"nd", 0},
	{"nl", 0},
	{"nw", 0},

	{"pc", 0},
	{"pf", 0},
	{"pk", 0},
	{"pl", 0},
	{"pn", 0},
	{"po", 0},
	{"pO", 0},
	{"ps", 0},
	{"px", 0},

	{"r1", 0},
	{"r2", 0},
	{"r3", 0},

	{"RA", 0},
	{"rc", 0},
	{"rf", 0},
	{"RF", 0},
	{"RI", 0},
	{"rp", 0},
	{"rP", 0},
	{"rs", 0},
	{"RX", 0},

	{"s0", 0},
	{"s1", 0},
	{"s2", 0},
	{"s3", 0},

	{"sa", 0},
	{"SA", 0},
	{"sc", 0},
	{"se", 0},
	{"sf", 0},
	{"SF", 0},
	/* {"so", 0}, standout is always the same as another attribute. */
	{"sr", 0},
	{"SR", 0},
	{"st", 0},
	{"SX", 0},

	{"ta", 0},
	/* {"te", 0}, terminal end-use is "logical". */
	/* {"ti", 0}, terminal init is "logical". */
	{"ts", 0},

	{"uc", 0},
	{"ue", 0},
	{"up", 0},
	{"UP", 0},
	{"us", 0},

	{"vb", 0},
	/* {"ve", 0}, */
	{"vi", 0},
	/* {"vs", 0}, */

	{"wi", 0},

	{"XF", 0},
	{"XN", 0},

	{NULL, 0},
};

/* From some really old XTerm docs we had at the office, and an updated
 * version at Moy, Gildea, and Dickey. */
struct vte_capability_string vte_xterm_capability_strings[] = {
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

	{ESC "%@", "iso8859-1-character-set", 0},
	{ESC "%G", "utf-8-character-set", 0},

	{ESC "(%+\\0", "designate-g0-character-set", 0},
	{ESC ")%+\\0", "designate-g1-character-set", 0},
	{ESC "*%+\\0", "designate-g2-character-set", 0},
	{ESC "+%+\\0", "designate-g3-character-set", 0},

	{ESC "7", "save-cursor", 0},
	{ESC "8", "restore-cursor", 0},
	{ESC "=", "application-keypad", 0},
	{ESC ">", "normal-keypad", 0},
	{ESC "D", "index", 0},
	{ESC "E", "next-line", 0},
	{ESC "F", "cursor-lower-left", 0},
	{ESC "H", "tab-set", 0},
	{ESC "M", "reverse-index", 0},
	{ESC "N", "single-shift-g2", 0},
	{ESC "O", "single-shift-g3", 0},
	{ESC "P%s" ESC "\\", "device-control-string", 0},
	{ESC "V", "start-of-guarded-area", 0},
	{ESC "W", "end-of-guarded-area", 0},
	{ESC "X", "start-of-string", 0},
	{ESC "Z", "return-terminal-id", 0},
	{ESC "c", "full-reset", 0},
	{ESC "l", "memory-lock", 0},
	{ESC "m", "memory-unlock", 0},
	{ESC "n", "invoke-g2-character-set", 0},
	{ESC "o", "invoke-g3-character-set", 0},
	{ESC "|", "invoke-g3-character-set-as-gr", 0},
	{ESC "}", "invoke-g2-character-set-as-gr", 0},
	{ESC "~", "invoke-g1-character-set-as-gr", 0},

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
	{CSI "d", "line-position-absolute", 0},
	{CSI "%dd", "line-position-absolute", 0},
	{CSI ";f", "horizontal-and-vertical-position", 0},
	{CSI "%d;f", "horizontal-and-vertical-position", 0},
	{CSI ";%df", "horizontal-and-vertical-position", 0},
	{CSI "%d;%df", "horizontal-and-vertical-position", 0},
	{CSI "g", "tab-clear", 0},
	{CSI "%dg", "tab-clear", 0},

	{CSI "h", "set-mode", 0},
	{CSI "%mh", "set-mode", 0},
	{CSI "?h", "decset", 0},
	{CSI "?%mh", "decset", 0},

	{CSI "i", "media-copy", 0},
	{CSI "%mi", "media-copy", 0},
	{CSI "?i", "dec-media-copy", 0},
	{CSI "?%mi", "dec-media-copy", 0},

	{CSI "l", "reset-mode", 0},
	{CSI "%ml", "reset-mode", 0},
	{CSI "?l", "decreset", 0},
	{CSI "?%ml", "decreset", 0},

	{CSI "m", "character-attributes", 0},
	{CSI "%mm", "character-attributes", 0},

	{CSI "%dn", "device-status-report", 0},
	{CSI "?%dn", "dec-device-status-report", 0},
	{CSI "!p", "soft-reset", 0},
	{CSI "%d;%d\"p", "set-conformance-level", 0},
	{CSI "%d\"q", "select-character-protection", 0},
	{CSI "%d;%dr", "set-scrolling-region", 0},
	{CSI "?%mr", "restore-mode", 0},
	{CSI "?%ms", "save-mode", 0},

	{CSI "%mt", "window-manipulation", 0},

	{CSI "%d;%d;%d;%dw", "enable-filter-rectangle", 0},
	{CSI "%dx", "request-terminal-parameters", 0},
	{CSI "%d;%d'z", "enable-locator-reporting", 0},
	{CSI "%m'{", "select-locator-events", 0},
	{CSI "%d'|", "request-locator-position", 0},

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

	{NULL, NULL, 0},
};

void
vte_capability_init(void)
{
	unsigned int i;
	GQuark quark;
	for (i = 0; i < G_N_ELEMENTS(vte_terminal_capability_strings); i++) {
		const char *tmp;
		tmp = vte_terminal_capability_strings[i].capability;
		if (tmp != NULL) {
			quark = g_quark_from_static_string(tmp);
			vte_terminal_capability_strings[i].quark = quark;
		}
	}
	for (i = 0; i < G_N_ELEMENTS(vte_xterm_capability_strings); i++) {
		const char *tmp;
		tmp = vte_xterm_capability_strings[i].value;
		if (tmp != NULL) {
			quark = g_quark_from_static_string(tmp);
			vte_xterm_capability_strings[i].quark = quark;
		}
	}
}
