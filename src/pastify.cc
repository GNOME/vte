/*
 * Copyright © 2015, 2019, Egmont Koblinger
 * Copyright © 2015, 2018, 2019, 2020, 2021 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "pastify.hh"

namespace vte::terminal {

using namespace std::literals;

/*
 * pastify_string:
 * @str:
 * @insert_brackets:
 * @c1:
 *
 * Converts @str into a form safe for pasting to the child. Elide
 * C0 controls except NUL, HT, CR, LF, and C1 controls.
 * We also convert newlines to carriage returns, which more software
 * is able to cope with (cough, pico, cough).
 *
 * Also insert bracketed paste controls around the string if
 * @insert_brackets is true, using C1 CSI if @c1 is true or C0 controls
 * otherwise.
 */
std::string
pastify_string(std::string_view str,
               bool insert_brackets,
               bool c1)
{
        auto rv = std::string{};
        rv.reserve(str.size() + 1 + insert_brackets ? 12 : 0);

        if (insert_brackets) {
                if (c1)
                        rv.append("\xc2\x9b" "200~");
                else
                        rv.append("\e[200~");
        }

        /* C0 \ { NUL, HT, CR, LF } + { DEL } + { C1 control start byte } */
        auto const controls = "\x01\x02\x03\x04\x05\x06\x07\x08\x0a\x0b\x0c\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f\xc2"sv;

        auto next_char = [&str](size_t pos) constexpr noexcept -> unsigned char
        {
                return pos + 1 < str.size() ? str[pos + 1] : 0;
        };

        while (str.size() != 0) {
                auto run = str.find_first_of(controls, 0);

                rv.append(str, 0, run);
                if (run == str.npos)
                        break;

                switch (str[run]) {
                case '\x0a':
                        rv.push_back('\x0d');
                        ++run;
                        break;
                case '\xc2': {
                        auto const c = next_char(run);
                        if (c >= 0x80 && c <= 0x9f) {
                                /* Skip both bytes of a C1 */
                                run += 2;
                        } else {
                                /* Move along, nothing to see here */
                                rv.push_back('\xc2');
                                ++run;
                        }
                        break;
                }
                default:
                        /* Swallow this byte */
                        ++run;
                        break;
                }

                str = str.substr(run);
        }

        if (insert_brackets) {
                if (c1)
                        rv.append("\xc2\x9b" "201~");
                else
                        rv.append("\e[201~");
        }

        return rv;
}

} // namespace vte::terminal
