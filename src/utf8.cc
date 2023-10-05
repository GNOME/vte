/*
 * Copyright © 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "utf8.hh"

#define OK vte::base::UTF8Decoder::ACCEPT
#define RJ vte::base::UTF8Decoder::REJECT
#define RW vte::base::UTF8Decoder::REJECT_REWIND

constinit uint8_t const vte::base::UTF8Decoder::kTable[] = {
        // The first part of the table maps bytes to character classes that
        // to reduce the size of the transition table and create bitmasks.
        // The classes are as follows:
        // 0x00..0x7f: 0
        // 0x80..0x8f: 1
        // 0x90..0x9f: 2
        // 0xa0..0xbf: 3
        // 0xc0..0xc1: 9
        // 0xc2..0xdf: 4
        // 0xe0:       10
        // 0xe1..0xec: 5
        // 0xed:       6
        // 0xee..0xef: 5
        // 0xf0:       11
        // 0xf1..0xf3: 7
        // 0xf4:       8
        // 0xf5..0xff: 9
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x00..0x0f
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x10..0x1f
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x20..0x2f
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x30..0x3f
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x40..0x4f
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x50..0x5f
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x60..0x6f
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x70..0x7f
         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x80..0x8f
         2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0x90..0x9f
         3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // 0xa0..0xaf
         3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // 0xb0..0xbf
         9, 9, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, // 0xc0..0xcf
         4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, // 0xd0..0xdf
        10, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, // 0xe0..0xef
        11, 7, 7, 7, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 0xf0..0xff

        // To understand this DFA, see transitions graph on the website
        // linked above.
        //
        // The following translates the states of the DFA to the
        // algorithm of the UTF-8 decoder from the W3 Encodings spec
        // [https://www.w3.org/TR/encoding/#utf-8]:
        //
        // DFA   │ bytes   bytes   lower   upper
        // state │ seen    needed  bound   bound
        // ──────┼─────────────────────────────────
        //   0   │ 0       0       0x80    0xbf
        //  12   │
        //  24   │ 1,2,3   1       0x80    0xbf
        //  36   │ 1,2     2       0x80    0xbf
        //  48   │ 1       2       0xa0    0xbf
        //  60   │ 1       2       0x80    0x9f
        //  72   │ 1       3       0x90    0xbf
        //  84   │ 1       3       0x80    0xbf
        //  96   │ 1       3       0x80    0x8f
        // 108   │
        //
        // If an unexpected byte is read in a non-ACCEPT/REJECT* state,
        // transition to REJECT_REWIND so that the decoder will read that
        // byte again after being reset; this makes the decoder conform
        // to the Unicode recommendation for insering replacement
        // characters, and to the W3 Encoding TR spec.
        //
        // If an unexpected byte is read in the ACCEPT or a REJECT* state,
        // transition to REJECT; that byte must not be read again, since
        // that would lead to an infinite loop.
        //
        // For each state (row), the table records which state will
        // be transitioned to when consuming a character of the class
        // (column).
        /*
         0   1   2   3   4   5   6   7   8   9  10  11 // character class
        */
        OK, RJ, RJ, RJ, 24, 36, 60, 84, 96, RJ, 48, 72, // state 0 (accept)
        RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, // state 12 (reject)
        RW, OK, OK, OK, RW, RW, RW, RW, RW, RW, RW, RW, // state 24
        RW, 24, 24, 24, RW, RW, RW, RW, RW, RW, RW, RW, // state 36
        RW, RW, RW, 24, RW, RW, RW, RW, RW, RW, RW, RW, // state 48
        RW, 24, 24, RW, RW, RW, RW, RW, RW, RW, RW, RW, // state 60
        RW, RW, 36, 36, RW, RW, RW, RW, RW, RW, RW, RW, // state 72
        RW, 36, 36, 36, RW, RW, RW, RW, RW, RW, RW, RW, // state 84
        RW, 36, RW, RW, RW, RW, RW, RW, RW, RW, RW, RW, // state 96
        RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, RJ, // state 108 (reject-rewind)
};
