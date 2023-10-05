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

#pragma once

#include <cstdint>
#include <utility>

namespace vte {

namespace base {

/* See https://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for more
 * information on this branchless UTF-8 decoder.
 */
class UTF8Decoder {
public:
        enum {
                ACCEPT = 0,
                REJECT = 12,
                REJECT_REWIND = 108
        };

        constexpr UTF8Decoder() noexcept = default;
        constexpr ~UTF8Decoder() noexcept = default;

        UTF8Decoder(UTF8Decoder const&) noexcept = delete;
        UTF8Decoder(UTF8Decoder&&) noexcept = delete;
        UTF8Decoder& operator= (UTF8Decoder const&) = delete;
        UTF8Decoder& operator= (UTF8Decoder&&) = delete;

        // Returns: the UTF-32 codepoint.  This function may only be
        // called when the decoder is in ACCEPT state.  This will
        // reset the decoder's codepoint to 0, so may only be called
        // once before pushing more input data into the decoder.
        inline constexpr uint32_t codepoint() noexcept
        {
                return std::exchange(m_codepoint, 0U);
        }

        // Push input data into the decoder.
        // If returning ACCEPT, a UTF-32 codepoint is available
        // in .codepoint().
        // If returning REJECT, .reset_fallback() must be called,
        // and then .codepoint(), and the input stream must be advanced.
        // If returning REJECT_REWIND, .reset_fallback() must be
        // called, and then .codepoint(), and the input stream must
        // *NOT* be advanced, since the input byte was NOT consumed.
        // For all other return values, the input stream must be
        // advanced; but no codepoint has been completely decoded, and
        // so .codepoint() must not be called.
        inline uint8_t decode(uint32_t byte) noexcept {
                auto const type = kTable[byte];
                m_codepoint = (m_codepoint << 6) | ((0x7fU >> (type >> 1)) & byte);
                return m_state = kTable[256 + m_state + type];
        }

        // Resets the decoder; a replacement character (U+FFFD)
        // will be available in .codepoint() which must be called
        // before more input data is pushed into the decoder.
        inline constexpr void reset_fallback() noexcept {
                m_codepoint = 0xfffdU;
                m_state = ACCEPT;
        }

        // Resets the decoder.
        // .codepoint() must *not* be called before more
        // input data was pushed into the decoder.
        inline constexpr void reset_clear() noexcept {
                m_codepoint = 0;
                m_state = ACCEPT;
        }

        // Flushes pending output of the decoder, and resets it.
        // If returning true, a replacement character U+FFFD is
        // available in codepoint() which must be called before
        // more input data is pushed into the decoder; if returning
        // false, .codepoint() must not be called before more
        // input data was pushed into the decoder.
        inline constexpr bool flush() noexcept {
                auto const state = m_state;
                if (m_state != ACCEPT)
                        reset_fallback();
                else
                        reset_clear();
                return state != m_state;
        }

private:
        uint32_t m_codepoint{0};
        uint8_t m_state{ACCEPT};

        static uint8_t const kTable[];

}; // class UTF8Decoder

} // namespace base

} // namespace vte
