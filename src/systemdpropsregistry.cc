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

#include "systemdpropsregistry.hh"

#include <optional>
#include <string>
#include <string_view>

#include "vtemacros.h"
#include "vteenums.h"
#include "vteglobals.h"

#include "cxx-utils.hh"

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

namespace vte::systemd {

namespace impl {

inline constexpr auto const k_max_string_len = 255u;

inline std::optional<VteSystemdContextType>
context_type_from_string(std::string_view const& str) noexcept
{
        static constinit struct {
                char const* str;
                VteSystemdContextType value;
        } const table[] = {
                { "app",        VTE_SYSTEMD_CONTEXT_TYPE_APP        },
                { "boot",       VTE_SYSTEMD_CONTEXT_TYPE_BOOT       },
                { "chpriv",     VTE_SYSTEMD_CONTEXT_TYPE_CHPRIV     },
                { "command",    VTE_SYSTEMD_CONTEXT_TYPE_COMMAND    },
                { "container",  VTE_SYSTEMD_CONTEXT_TYPE_CONTAINER  },
                { "elevate",    VTE_SYSTEMD_CONTEXT_TYPE_ELEVATE    },
                { "remote",     VTE_SYSTEMD_CONTEXT_TYPE_REMOTE     },
                { "service",    VTE_SYSTEMD_CONTEXT_TYPE_SERVICE    },
                { "session",    VTE_SYSTEMD_CONTEXT_TYPE_SESSION    },
                { "shell",      VTE_SYSTEMD_CONTEXT_TYPE_SHELL      },
                { "subcontext", VTE_SYSTEMD_CONTEXT_TYPE_SUBCONTEXT },
                { "vm",         VTE_SYSTEMD_CONTEXT_TYPE_VM         },
        };

        for (auto i = 0u; i < G_N_ELEMENTS(table); ++i) {
                if (str == table[i].str)
                        return table[i].value;
        }

        return std::nullopt;
}

inline std::optional<vte::property::Value>
parse_context_type(std::string_view str)
{
        if (auto const value = context_type_from_string(str))
                return int64_t(vte::to_integral(*value));
        return std::nullopt;
}

inline std::optional<VteSystemdContextExitCondition>
exit_condition_from_string(std::string_view const& str) noexcept
{
        static constinit struct {
                char const* str;
                VteSystemdContextExitCondition value;
        } const table[] = {
                { "crash",     VTE_SYSTEMD_CONTEXT_EXIT_CONDITION_CRASH     },
                { "failure",   VTE_SYSTEMD_CONTEXT_EXIT_CONDITION_FAILURE   },
                { "interrupt", VTE_SYSTEMD_CONTEXT_EXIT_CONDITION_INTERRUPT },
                { "success",   VTE_SYSTEMD_CONTEXT_EXIT_CONDITION_SUCCESS   },
        };

        for (auto i = 0u; i < G_N_ELEMENTS(table); ++i) {
                if (str == table[i].str)
                        return table[i].value;
        }

        return std::nullopt;
}

inline std::optional<vte::property::Value>
parse_exit_condition(std::string_view str)
{
        if (auto const value = exit_condition_from_string(str))
                return int64_t(vte::to_integral(*value));
        return std::nullopt;
}

inline std::optional<vte::property::Value>
parse_cwd(std::string_view str)
{
        auto value = vte::property::impl::parse_systemd_property_string(str);
        if (!value ||
            !std::holds_alternative<std::string>(*value))
                return value;

        auto const& uristr = std::get<std::string>(*value);
        if (auto uri = vte::take_freeable(g_uri_parse(uristr.c_str(),
                                                      GUriFlags(G_URI_FLAGS_NONE),
                                                      nullptr));
            uri &&
            g_uri_get_scheme(uri.get()) &&
            g_str_equal(g_uri_get_scheme(uri.get()), "file")) {
                return std::make_optional<vte::property::Value>(std::in_place_type<vte::property::URIValue>, std::move(uri), uristr);
        }

        return std::make_optional<vte::property::Value>
                (std::in_place_type<vte::property::URIValue>,
                 vte::take_freeable(g_uri_build(GUriFlags(G_URI_FLAGS_NONE),
                                                "file",
                                                nullptr, // no userinfo
                                                nullptr, // no host
                                                -1, // no port
                                                std::get<std::string>(*value).c_str(), // path
                                                nullptr, // no query,
                                                nullptr)), // no fragment
                 uristr); // stored URI string
}

} // namespace impl

SystemdPropertyRegistry::SystemdPropertyRegistry()
{
        install_many({
                        {
                                VTE_SYSTEMD_PROPERTY_ID_CONTEXT_ID,
                                VTE_SYSTEMD_PROPERTY_CONTEXT_ID,
                                vte::property::Type::UUID,
                                vte::property::Flags::NO_OSC
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_BOOT_ID,
                                VTE_SYSTEMD_PROPERTY_BOOT_ID,
                                vte::property::Type::UUID,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_COMM,
                                VTE_SYSTEMD_PROPERTY_COMM,
                                vte::property::Type::STRING,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_COMMAND_LINE,
                                VTE_SYSTEMD_PROPERTY_COMMAND_LINE,
                                vte::property::Type::STRING,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_CONTAINER,
                                VTE_SYSTEMD_PROPERTY_CONTAINER,
                                vte::property::Type::STRING,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_CONTEXT_TYPE,
                                VTE_SYSTEMD_PROPERTY_CONTEXT_TYPE,
                                vte::property::Type::INT,
                                vte::property::Flags::SYSTEMD_START,
                                &impl::parse_context_type
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_CURRENT_DIRECTORY,
                                VTE_SYSTEMD_PROPERTY_CURRENT_DIRECTORY,
                                vte::property::Type::URI,
                                vte::property::Flags::SYSTEMD_START,
                                &impl::parse_cwd
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_EXIT_CONDITION,
                                VTE_SYSTEMD_PROPERTY_EXIT_CONDITION,
                                vte::property::Type::INT,
                                vte::property::Flags::SYSTEMD_END,
                                &impl::parse_exit_condition
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_EXIT_SIGNAL,
                                VTE_SYSTEMD_PROPERTY_EXIT_SIGNAL,
                                vte::property::Type::STRING,
                                vte::property::Flags::SYSTEMD_END
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_EXIT_STATUS,
                                VTE_SYSTEMD_PROPERTY_EXIT_STATUS,
                                vte::property::Type::UINT,
                                vte::property::Flags::SYSTEMD_END,
                                [](std::string_view const& str) -> auto {
                                        return vte::property::impl::parse_termprop_integral_range<uint64_t>(str, 0, 255);
                                }
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_HOSTNAME,
                                VTE_SYSTEMD_PROPERTY_HOSTNAME,
                                vte::property::Type::STRING,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_MACHINE_ID,
                                VTE_SYSTEMD_PROPERTY_MACHINE_ID,
                                vte::property::Type::UUID,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_PID,
                                VTE_SYSTEMD_PROPERTY_PID,
                                vte::property::Type::UINT,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_PIDFD_INODE,
                                VTE_SYSTEMD_PROPERTY_PIDFD_INODE,
                                vte::property::Type::UINT,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_TARGET_HOST,
                                VTE_SYSTEMD_PROPERTY_TARGET_HOST,
                                vte::property::Type::STRING,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_TARGET_USER,
                                VTE_SYSTEMD_PROPERTY_TARGET_USER,
                                vte::property::Type::STRING,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_USER,
                                VTE_SYSTEMD_PROPERTY_USER,
                                vte::property::Type::STRING,
                                vte::property::Flags::SYSTEMD_START
                        },
                        {
                                VTE_SYSTEMD_PROPERTY_ID_VM,
                                VTE_SYSTEMD_PROPERTY_VM,
                                vte::property::Type::STRING,
                                vte::property::Flags::SYSTEMD_START
                        },
                });
}

SystemdPropertyRegistry const&
properties_registry() noexcept
{
        static auto s_systemd_properties_registry = SystemdPropertyRegistry{};

        return s_systemd_properties_registry;
}

} // namespace vte::systemd
