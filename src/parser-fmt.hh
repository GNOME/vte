// Copyright © 2025 Christian Persch
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "parser.hh"

#include <string_view>

#include "fmt-glue.hh"
#include <fmt/color.h>
#include <fmt/compile.h>

#include "boxed.hh"

namespace vte::parser
{
        namespace detail {

                struct seq_tag {};
                struct cmd_tag {};
                struct charset_tag {};
                struct control_tag {};

        } // namespace detail

        using seq_t = vte::boxed<unsigned, detail::seq_tag>;
        using cmd_t = vte::boxed<unsigned, detail::cmd_tag>;
        using charset_t = vte::boxed<unsigned, detail::charset_tag>;
        using control_t = vte::boxed<unsigned, detail::control_tag>;

        namespace detail {

                auto seq_to_sv(seq_t const& seq) noexcept -> std::string_view;
                auto cmd_to_sv(cmd_t const& cmd) noexcept -> std::string_view;
                auto charset_to_sv(charset_t const& cs) noexcept -> std::string_view;
                auto control_to_sv(control_t const& ctrl) noexcept -> std::string_view;

        } // namespace detail
} // namespace vte::parser

FMT_BEGIN_NAMESPACE

template<>
struct formatter<vte::parser::seq_t, char> : public formatter<std::string_view> {
public:
         auto format(vte::parser::seq_t const& seq,
                              format_context& ctx) const -> format_context::iterator
        {
                return formatter<std::string_view, char>::format(vte::parser::detail::seq_to_sv(seq), ctx);
        }
};

template<>
struct formatter<vte::parser::cmd_t, char> : public formatter<std::string_view> {
public:
        auto format(vte::parser::cmd_t const& cmd,
                              format_context& ctx) const -> format_context::iterator
        {
                return formatter<std::string_view, char>::format(vte::parser::detail::cmd_to_sv(cmd), ctx);
        }
};

template<>
struct formatter<vte::parser::charset_t, char> : public formatter<std::string_view> {
public:
        auto format(vte::parser::charset_t const& cs,
                              format_context& ctx) const -> format_context::iterator
        {
                return formatter<std::string_view, char>::format(vte::parser::detail::charset_to_sv(cs), ctx);
        }
};

template<>
struct formatter<vte::parser::control_t, char> : public formatter<std::string_view> {
public:
        auto format(vte::parser::control_t const& ctrl,
                              format_context& ctx) const -> format_context::iterator
        {
                return formatter<std::string_view, char>::format(vte::parser::detail::control_to_sv(ctrl), ctx);
        }
};

template<>
struct formatter<vte::parser::Sequence> {

private:
        bool m_codepoints{false};

        auto format_params(vte::parser::Sequence const& seq,
                           format_context& ctx) const -> format_context::iterator
        {
                auto&& it = ctx.out();

                auto const size = seq.size();
                if (size > 0) {
                        *it = ' ';
                        ++it;
                }

                for (auto i = 0u; i < size; i++) {
                        if (!seq.param_default(i))
                                it = fmt::format_to(it, "{}", seq.param(i));
                        if (i + 1 < size) {
                                *it = seq.param_nonfinal(i) ? ':' : ';';
                                ++it;
                        }
                }

                ctx.advance_to(it);
                return it;
        }

        auto format_pintro(vte::parser::Sequence const& seq,
                           format_context& ctx) const -> format_context::iterator
        {
                auto&& it = ctx.out();

                auto const type = seq.type();
                if (type != VTE_SEQ_CSI &&
                    type != VTE_SEQ_DCS)
                        return it;

                auto const p = seq.intermediates() & 0x7;
                if (p == 0)
                        return it;

                *it = ' '; ++it;
                *it = char(0x40 - p); ++it;
                ctx.advance_to(it);
                return it;
        }

        auto format_intermediates(vte::parser::Sequence const& seq,
                                  format_context& ctx) const -> format_context::iterator
        {
                auto const type = seq.type();
                auto intermediates = seq.intermediates();
                if (type == VTE_SEQ_CSI ||
                    type == VTE_SEQ_DCS)
                        intermediates = intermediates >> 3; /* remove pintro */

                auto&& it = ctx.out();

                while (intermediates != 0) {
                        unsigned i = intermediates & 0x1f;
                        auto c = char(0x20 + i - 1);

                        *it = ' '; ++it;
                        if (c == 0x20) {
                                *it = 'S'; ++it;
                                *it = 'P'; ++it;
                        } else {
                                *it = c; ++it;
                        }

                        intermediates = intermediates >> 5;
                }

                ctx.advance_to(it);
                return it;
        }

        auto format_seq_and_params(vte::parser::Sequence const& seq,
                                   format_context& ctx) const -> format_context::iterator
        {
                auto&& it = ctx.out();

                *it = '{'; ++it;
                if (seq.command() != VTE_CMD_NONE) {
                        it = fmt::format_to(it, "{}", vte::parser::cmd_t(seq.command()));
                        it = format_params(seq, ctx);
                } else {
                        it = fmt::format_to(it, "{}", vte::parser::seq_t(seq.type()));
                        it = format_pintro(seq, ctx);
                        it = format_params(seq, ctx);
                        it = format_intermediates(seq, ctx);
                        *it = ' '; ++it;
                        *it = char(seq.terminator()); ++it;
                }
                *it = '}'; ++it;

                ctx.advance_to(it);
                return it;
        }

public:

        constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
        {
                auto it = ctx.begin();
                while (it != ctx.end()) {
                        if (*it == 'u')
                                m_codepoints = true;
                        else if (*it == '}')
                                break;
                        else
                                throw format_error{"Invalid format string"};
                        ++it;
                }

                return it;
        }

        auto format(vte::parser::Sequence const& seq,
                    format_context& ctx) const -> format_context::iterator
        {
                switch (seq.type()) {
                case VTE_SEQ_NONE: {
                        return fmt::format_to(ctx.out(), "{{NONE}}");
                }

                case VTE_SEQ_IGNORE: {
                        return fmt::format_to(ctx.out(), "{{IGNORE}}");
                }

                case VTE_SEQ_GRAPHIC: [[likely]] {
                        auto const terminator = seq.terminator();
                        auto const printable = g_unichar_isprint(terminator);

                        if (printable) [[likely]] {
                                char ubuf[7];
                                auto const len = g_unichar_to_utf8(terminator, ubuf);
                                if (m_codepoints) {
                                        return fmt::format_to(ctx.out(), "<U+{:04X} {}>",
                                                              terminator,
                                                              std::string_view(ubuf, len));
                                } else {
                                        return fmt::format_to(ctx.out(), "{}",
                                                              std::string_view(ubuf, len));
                                }
                        } else {
                                return fmt::format_to(ctx.out(), "<U+{:04X}>", terminator);
                        }
                        break;
                }

                case VTE_SEQ_CONTROL:
                        return fmt::format_to(ctx.out(),
                                              "{{{}}}",
                                              vte::parser::cmd_t(seq.command()));

                case VTE_SEQ_ESCAPE: {
                        switch (seq.command()) {
                        case VTE_CMD_GnDm:
                                return fmt::format_to(ctx.out(),
                                                      "{{G{}D{} {}}}",
                                                      seq.slot(),
                                                      seq.charset_type() == VTE_CHARSET_TYPE_GRAPHIC_94 ? 4 : 6,
                                                      vte::parser::charset_t(seq.charset()));
                        case VTE_CMD_GnDMm:
                                return fmt::format_to(ctx.out(),
                                                      "{{G{}DM{} {}}}",
                                                      seq.slot(),
                                                      seq.charset_type() == VTE_CHARSET_TYPE_GRAPHIC_94 ? 4 : 6,
                                                      vte::parser::charset_t(seq.charset()));
                        case VTE_CMD_CnD:
                                return fmt::format_to(ctx.out(),
                                                      "{{C{}D {}}}",
                                                      seq.slot(),
                                                      vte::parser::charset_t(seq.charset()));
                        case VTE_CMD_DOCS:
                                return fmt::format_to(ctx.out(),
                                                      "{{DOCS {}}}",
                                                      vte::parser::charset_t(seq.charset()));
                        default: [[likely]]
                                return fmt::format_to(ctx.out(),
                                                      "{{{}}}",
                                                      vte::parser::cmd_t(seq.command()));
                        }
                }

                case VTE_SEQ_CSI:
                case VTE_SEQ_DCS:
                        return format_seq_and_params(seq, ctx);

                case VTE_SEQ_APC:
                case VTE_SEQ_OSC:
                case VTE_SEQ_PM:
                case VTE_SEQ_SOS:
                        return fmt::format_to(ctx.out(),
                                              "{{{} {}}}",
                                              vte::parser::seq_t(seq.type()),
                                              vte::boxed<std::u32string_view>(seq.string()));

                case VTE_SEQ_SCI: {
                        auto const terminator = seq.terminator();
                        if (terminator <= 0x20) {
                                if (m_codepoints) {
                                        return fmt::format_to(ctx.out(),
                                                              "{{SCI {:02}/{:02}}}",
                                                              terminator / 16,
                                                              terminator % 16);
                                } else {
                                        return fmt::format_to(ctx.out(),
                                                              "{{SCI {}}}",
                                                              vte::parser::control_t(terminator));
                                }
                        } else if (terminator < 0x7f) { // Note: terminator *is* < 0x7F
                                return fmt::format_to(ctx.out(),
                                                      "{{SCI {:c}}}",
                                                      char(terminator));
                        } else {
                                __builtin_unreachable();
                                return ctx.out();
                        }
                        break;
                }

                default: [[unlikely]]
                        __builtin_unreachable();
                        return ctx.out();
                }
        }

}; // class formatter

FMT_END_NAMESPACE
