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

#include "config.h"

#include "parser-fmt.hh"

#include <memory>
#include <optional>
#include <string_view>

#include "fmt-glue.hh"
#include <fmt/color.h>
#include <fmt/compile.h>

using namespace std::literals::string_view_literals;

namespace vte::parser::detail {

auto
seq_to_sv(vte::parser::seq_t const& seq) noexcept -> std::string_view
{
        switch (seq.get()) {
        case VTE_SEQ_NONE: return "NONE"sv;
        case VTE_SEQ_IGNORE: return "IGNORE"sv;
        case VTE_SEQ_GRAPHIC: return "GRAPHIC"sv;
        case VTE_SEQ_CONTROL: return "CONTROL"sv;
        case VTE_SEQ_ESCAPE: return "ESCAPE"sv;
        case VTE_SEQ_CSI: return "CSI"sv;
        case VTE_SEQ_DCS: return "DCS"sv;
        case VTE_SEQ_OSC: return "OSC"sv;
        case VTE_SEQ_SCI: return "SCI"sv;
        case VTE_SEQ_APC: return "APC"sv;
        case VTE_SEQ_PM: return "PM"sv;
        case VTE_SEQ_SOS: return "SOS"sv;
        default: __builtin_unreachable(); return ""sv;
        }
}

auto
cmd_to_sv(vte::parser::cmd_t const& cmd) noexcept -> std::string_view
{
        switch (cmd.get()) {
#define _VTE_CMD(cmd) case VTE_CMD_##cmd: return #cmd##sv;
#define _VTE_NOP(cmd) _VTE_CMD(cmd)
#include "parser-cmd.hh"
#undef _VTE_CMD
#undef _VTE_NOP
        default:
                return ""sv;
        }
}

static auto
charset_alias_to_sv(unsigned cs) noexcept ->  std::optional<std::string_view>
{
        switch (cs) {
#define _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET(name) _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET_ALIAS_PASTE(name1,name2) case VTE_CHARSET_##name1: return #name1 "(" ## #name2 ## ")"sv;
#define _VTE_CHARSET_ALIAS(name1,name2)
#include "parser-charset.hh"
#undef _VTE_CHARSET_PASTE
#undef _VTE_CHARSET
#undef _VTE_CHARSET_ALIAS_PASTE
#undef _VTE_CHARSET_ALIAS
        default:
                return std::nullopt; /* not an alias */
        }
}

static auto
charset_name_to_sv(unsigned cs) noexcept ->  std::optional<std::string_view>
{
        switch (cs) {
#define _VTE_CHARSET_PASTE(name) case VTE_CHARSET_##name: return #name##sv;
#define _VTE_CHARSET(name) _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET_ALIAS_PASTE(name1,name2)
#define _VTE_CHARSET_ALIAS(name1,name2)
#include "parser-charset.hh"
#undef _VTE_CHARSET_PASTE
#undef _VTE_CHARSET
#undef _VTE_CHARSET_ALIAS_PASTE
#undef _VTE_CHARSET_ALIAS
        default:
                return std::nullopt;
        }
}

auto
charset_to_sv(vte::parser::charset_t const& cs) noexcept -> std::string_view
{
        if (auto alias = charset_alias_to_sv(cs.get()))
                return *alias;
        else if (auto name = charset_name_to_sv(cs.get()))
                return *name;
        else
                return ""sv;
}

auto
control_to_sv(vte::parser::control_t const& ctrl) noexcept -> std::string_view
{
        switch (ctrl.get()) {
#define _VTE_SEQ(cmd,type,f,pi,ni,i0,flags) case f: return #cmd##sv;
#define _VTE_NOQ(...) _VTE_SEQ(__VA_ARGS__)
#include "parser-c01.hh"
#undef _VTE_SEQ
#undef _VTE_NOQ
                // Not a control, but useful to have a name for
        case 0x20: return "SP"sv;
        default: return ""sv; // unreachable
        }
}

} // namespace vte::parser::detail
