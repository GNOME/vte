/*
 * Copyright © 2024 Christian Persch
 *
 * The following code is copied from xterm/xtermcap.c where it is under the
 * licence below; and modified and used here under the GNU Lesser General Public
 * Licence, version 3 (or, at your option), any later version.
 */

/*
 * Copyright 2007-2020,2023 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

#include "xtermcap.hh"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

static constinit struct {
        char const* tc;
        char const* ti;
        int keycode;
        unsigned state;
        bool fkey;
} const termcap_table[] = {
        /* tcap terminfo code state */
        { "%1", "khlp", GDK_KEY_Help, 0, false },
        { "#1", "kHLP", GDK_KEY_Help, GDK_SHIFT_MASK, false },
        { "@0", "kfnd", GDK_KEY_Find, 0, false },
        { "*0", "kFND", GDK_KEY_Find, GDK_SHIFT_MASK, false },
        { "*6", "kslt", GDK_KEY_Select, 0, false },
        { "#6", "kSLT", GDK_KEY_Select, GDK_SHIFT_MASK, false },

        { "kh", "khome", GDK_KEY_Home, 0, false },
        { "#2", "kHOM", GDK_KEY_Home, GDK_SHIFT_MASK, false },
        { "@7", "kend", GDK_KEY_End, 0, false },
        { "*7", "kEND", GDK_KEY_End, GDK_SHIFT_MASK, false },

        { "kl", "kcub1", GDK_KEY_Left, 0, false },
        { "kr", "kcuf1", GDK_KEY_Right, 0, false },
        { "ku", "kcuu1", GDK_KEY_Up, 0, false },
        { "kd", "kcud1", GDK_KEY_Down, 0, false },

        { "#4", "kLFT", GDK_KEY_Left, GDK_SHIFT_MASK, false },
        { "%i", "kRIT", GDK_KEY_Right, GDK_SHIFT_MASK, false },
        { "kF", "kind", GDK_KEY_Down, GDK_SHIFT_MASK, false },
        { "kR", "kri", GDK_KEY_Up, GDK_SHIFT_MASK, false },

        { "k1", "kf1", GDK_KEY_F1, 0, true },
        { "k2", "kf2", GDK_KEY_F2, 0, true },
        { "k3", "kf3", GDK_KEY_F3, 0, true },
        { "k4", "kf4", GDK_KEY_F4, 0, true },
        { "k5", "kf5", GDK_KEY_F5, 0, true },
        { "k6", "kf6", GDK_KEY_F6, 0, true },
        { "k7", "kf7", GDK_KEY_F7, 0, true },
        { "k8", "kf8", GDK_KEY_F8, 0, true },
        { "k9", "kf9", GDK_KEY_F9, 0, true },
        { "k;", "kf10", GDK_KEY_F10, 0, true },

        { "F1", "kf11", GDK_KEY_F11, 0, true },
        { "F2", "kf12", GDK_KEY_F12, 0, true },
        { "F3", "kf13", GDK_KEY_F13, 0, true },
        { "F4", "kf14", GDK_KEY_F14, 0, true },
        { "F5", "kf15", GDK_KEY_F15, 0, true },
        { "F6", "kf16", GDK_KEY_F16, 0, true },
        { "F7", "kf17", GDK_KEY_F17, 0, true },
        { "F8", "kf18", GDK_KEY_F18, 0, true },
        { "F9", "kf19", GDK_KEY_F19, 0, true },
        { "FA", "kf20", GDK_KEY_F20, 0, true },
        { "FB", "kf21", GDK_KEY_F21, 0, true },
        { "FC", "kf22", GDK_KEY_F22, 0, true },
        { "FD", "kf23", GDK_KEY_F23, 0, true },
        { "FE", "kf24", GDK_KEY_F24, 0, true },
        { "FF", "kf25", GDK_KEY_F25, 0, true },
        { "FG", "kf26", GDK_KEY_F26, 0, true },
        { "FH", "kf27", GDK_KEY_F27, 0, true },
        { "FI", "kf28", GDK_KEY_F28, 0, true },
        { "FJ", "kf29", GDK_KEY_F29, 0, true },
        { "FK", "kf30", GDK_KEY_F30, 0, true },
        { "FL", "kf31", GDK_KEY_F31, 0, true },
        { "FM", "kf32", GDK_KEY_F32, 0, true },
        { "FN", "kf33", GDK_KEY_F33, 0, true },
        { "FO", "kf34", GDK_KEY_F34, 0, true },
        { "FP", "kf35", GDK_KEY_F35, 0, true },

        { "FQ", "kf36", XTERM_KEY_F36, 0, true },
        { "FR", "kf37", XTERM_KEY_F37, 0, true },
        { "FS", "kf38", XTERM_KEY_F38, 0, true },
        { "FT", "kf39", XTERM_KEY_F39, 0, true },
        { "FU", "kf40", XTERM_KEY_F40, 0, true },
        { "FV", "kf41", XTERM_KEY_F41, 0, true },
        { "FW", "kf42", XTERM_KEY_F42, 0, true },
        { "FX", "kf43", XTERM_KEY_F43, 0, true },
        { "FY", "kf44", XTERM_KEY_F44, 0, true },
        { "FZ", "kf45", XTERM_KEY_F45, 0, true },
        { "Fa", "kf46", XTERM_KEY_F46, 0, true },
        { "Fb", "kf47", XTERM_KEY_F47, 0, true },
        { "Fc", "kf48", XTERM_KEY_F48, 0, true },
        { "Fd", "kf49", XTERM_KEY_F49, 0, true },
        { "Fe", "kf50", XTERM_KEY_F50, 0, true },
        { "Ff", "kf51", XTERM_KEY_F51, 0, true },
        { "Fg", "kf52", XTERM_KEY_F52, 0, true },
        { "Fh", "kf53", XTERM_KEY_F53, 0, true },
        { "Fi", "kf54", XTERM_KEY_F54, 0, true },
        { "Fj", "kf55", XTERM_KEY_F55, 0, true },
        { "Fk", "kf56", XTERM_KEY_F56, 0, true },
        { "Fl", "kf57", XTERM_KEY_F57, 0, true },
        { "Fm", "kf58", XTERM_KEY_F58, 0, true },
        { "Fn", "kf59", XTERM_KEY_F59, 0, true },
        { "Fo", "kf60", XTERM_KEY_F60, 0, true },
        { "Fp", "kf61", XTERM_KEY_F61, 0, true },
        { "Fq", "kf62", XTERM_KEY_F62, 0, true },
        { "Fr", "kf63", XTERM_KEY_F63, 0, true },

        { "K1", "ka1", GDK_KEY_KP_Home, 0, false },
        { "K4", "kc1", GDK_KEY_KP_End, 0, false },
        { "K3", "ka3", GDK_KEY_KP_Prior, 0, false },
        { "K5", "kc3", GDK_KEY_KP_Next, 0, false },
        { "kB", "kcbt", GDK_KEY_ISO_Left_Tab, 0, false },
        { "kC", "kclr", GDK_KEY_Clear, 0, false },
        { "kD", "kdch1", GDK_KEY_Delete, 0, false },
        { "kI", "kich1", GDK_KEY_Insert, 0, false },

        { "kN", "knp", GDK_KEY_Next, 0, false },
        { "kP", "kpp", GDK_KEY_Prior, 0, false },
        { "%c", "kNXT", GDK_KEY_Next, GDK_SHIFT_MASK, false },
        { "%e", "kPRV", GDK_KEY_Prior, GDK_SHIFT_MASK, false },

        { "&8", "kund", GDK_KEY_Undo, 0, false },
        { "kb", "kbs", GDK_KEY_BackSpace, 0, false },
        { "Co", "colors", XTERM_KEY_COLORS, 0, false },
        /* note - termcap cannot support RGB */
        { "",   "RGB", XTERM_KEY_RGB, 0, false },
        { "TN", "name", XTERM_KEY_TCAPNAME, 0, false },

#define DEXT(name, parm, code) { "", name, code, parm, false }
#define D1ST(name, parm, code) DEXT("k" #name, parm, code)
#define DMOD(name, parm, code) DEXT("k" #name #parm, parm, code)

#define DGRP(name, code)                        \
        D1ST(name, 2, code),                    \
        DMOD(name, 3, code),                    \
        DMOD(name, 4, code),                    \
        DMOD(name, 5, code),                    \
        DMOD(name, 6, code),                    \
        DMOD(name, 7, code),                    \
        DMOD(name, 8, code)

        /* the terminfo codes here are ncurses extensions */
        /* ignore the termcap names, which are empty */
        { "", "kUP", GDK_KEY_Up, GDK_SHIFT_MASK, false },
        { "", "kDN", GDK_KEY_Up, GDK_SHIFT_MASK, false },

        DGRP(DN, GDK_KEY_Down),
        DGRP(LFT, GDK_KEY_Left),
        DGRP(RIT, GDK_KEY_Right),
        DGRP(UP, GDK_KEY_Up),
        DGRP(DC, GDK_KEY_Delete),
        DGRP(END, GDK_KEY_End),
        DGRP(HOM, GDK_KEY_Home),
        DGRP(IC, GDK_KEY_Insert),
        DGRP(NXT, GDK_KEY_Next),
        DGRP(PRV, GDK_KEY_Prior),
};
#undef DATA
#undef DEXT
#undef D1ST
#undef DMOD
#undef DGRP

/*
 * Parse the termcap/terminfo name from the string, returning a positive number
 * (the keysym) if found, otherwise -1.  Update the string pointer.
 * Returns the (shift, control) state in *state.
 *
 * This does not attempt to construct control/shift modifiers to construct
 * function-key values.  Instead, it sets the *fkey flag to pass to Input()
 * and bypass the lookup of keysym altogether.
 */
std::pair<int, unsigned>
xtermcap_get_keycode(std::string_view const& str)
{
        if (!str.size())
                return {-1, 0};

        for (auto i = 0u; i < G_N_ELEMENTS(termcap_table); ++i) {
                auto const data = &termcap_table[i];

                if (str == data->ti || str == data->tc) {
                        return {data->keycode, data->state};
                }
        }

        return {-1, 0};
}
