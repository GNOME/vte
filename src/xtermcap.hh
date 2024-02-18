/*
 * Copyright 2007-2011,2013 by Thomas E. Dickey
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

#pragma once

#include <string_view>
#include <utility>

enum {
        XTERM_KEY_F36 = -36,
        XTERM_KEY_F37 = -37,
        XTERM_KEY_F38 = -38,
        XTERM_KEY_F39 = -39,
        XTERM_KEY_F40 = -40,
        XTERM_KEY_F41 = -41,
        XTERM_KEY_F42 = -42,
        XTERM_KEY_F43 = -43,
        XTERM_KEY_F44 = -44,
        XTERM_KEY_F45 = -45,
        XTERM_KEY_F46 = -46,
        XTERM_KEY_F47 = -47,
        XTERM_KEY_F48 = -48,
        XTERM_KEY_F49 = -49,
        XTERM_KEY_F50 = -50,
        XTERM_KEY_F51 = -51,
        XTERM_KEY_F52 = -52,
        XTERM_KEY_F53 = -53,
        XTERM_KEY_F54 = -54,
        XTERM_KEY_F55 = -55,
        XTERM_KEY_F56 = -56,
        XTERM_KEY_F57 = -57,
        XTERM_KEY_F58 = -58,
        XTERM_KEY_F59 = -59,
        XTERM_KEY_F60 = -60,
        XTERM_KEY_F61 = -61,
        XTERM_KEY_F62 = -62,
        XTERM_KEY_F63 = -63,
        XTERM_KEY_COLORS   = -1024,
        XTERM_KEY_RGB      = -1025,
        XTERM_KEY_TCAPNAME = -1026,
};

std::pair<int, unsigned>
xtermcap_get_keycode(std::string_view const& str);

