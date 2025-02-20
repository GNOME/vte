// Copyright © 2021, 2022, 2023, 2025 Christian Persch
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

#include "termpropsregistry.hh"

#include <glib.h>

#include "vtemacros.h"
#include "vteenums.h"
#include "vteglobals.h"

namespace vte::terminal {

bool
TermpropsRegistry::check_termprop_wellknown(char const* name,
                                            vte::property::Type* type,
                                            vte::property::Flags* flags) noexcept
{
#if 0 // remove this when adding the first well-known termprop
        static constinit struct {
                char const* name;
                vte::property::Type type;
                vte::property::Flags flags;
        } const well_known_termprops[] = {
        };

        for (auto i = 0u; i < G_N_ELEMENTS(well_known_termprops); ++i) {
                auto const* wkt = &well_known_termprops[i];
                if (!g_str_equal(name, wkt->name))
                        continue;

                if (type)
                        *type = wkt->type;
                if (flags)
                        *flags = wkt->flags;
                return true;
        }
#endif // 0

        return false;
}

char const*
TermpropsRegistry::check_termprop_wellknown_alias(char const* name) noexcept
{
#if 0 // remove this when adding the first well-known alias
        static constinit struct {
                char const* name;
                char const* target_name;
        } const well_known_termprop_aliases[] = {
        };

        for (auto i = 0u; i < G_N_ELEMENTS(well_known_termprop_aliases); ++i) {
                auto const* wkta = &well_known_termprop_aliases[i];
                if (!g_str_equal(name, wkta->name))
                        continue;

                return wkta->target_name;
        }
#endif // 0

        return nullptr;
}

bool
TermpropsRegistry::check_termprop_blocklisted(char const* name) noexcept
{
#if 0 // remove this when adding the first blocked name
        // blocked termprop names
        static constinit char const* blocked_names[] = {
        };
        for (auto i = 0u; i < G_N_ELEMENTS(blocked_names); ++i) {
                if (g_str_equal(name, blocked_names[i])) [[unlikely]]
                        return true;
        }
#endif

        return false;
}

bool
TermpropsRegistry::check_termprop_blocklisted_alias(char const* name) noexcept
{
#if 0 // remove this when adding the first blocked alias
        // blocked termprop names
        static constinit char const* blocked_aliases[] = {
        };
        for (auto i = 0u; i < G_N_ELEMENTS(blocked_aliases); ++i) {
                if (g_str_equal(name, blocked_aliases[i])) [[unlikely]]
                        return true;
        }
#endif // 0

        return false;
}

int
TermpropsRegistry::install(char const* name,
                           vte::property::Type type,
                           vte::property::Flags flags)
{
        // Cannot install an existing termprop but with a different type
        // than the existing one; and installing the termprop with the same
        // type/flags as before is a no-op.
        if (auto const info = lookup(name)) {
                if (info->type() != type ||
                    info->flags() != flags) [[unlikely]] {
                        g_warning("Termprop \"%s\" already installed with different type or flags",
                                  name);
                }

                return -1;
        }

        auto wkt_type = vte::property::Type{};
        auto wkt_flags = vte::property::Flags{};
        auto const well_known = check_termprop_wellknown(name, &wkt_type, &wkt_flags);

        // Check type
        if (well_known && (type != wkt_type || flags != wkt_flags)) [[unlikely]] {
                g_warning("Denying to install well-known termprop \"%s\" with incorrect type or flags", name);
                return -1;
        }

        // If not well-known, the name needs to start with "vte.ext."
        if (!well_known) {
                g_return_val_if_fail(g_str_has_prefix(name, VTE_TERMPROP_NAME_PREFIX), -1);
                g_return_val_if_fail(vte::property::validate_termprop_name(name, 4), -1);
        }

        // Name blocklisted?
        if (check_termprop_blocklisted(name)) {
                g_warning("Denying to install blocklisted termprop \"%s\"", name);
                return -1;
        }

        return vte::property::Registry::install(name, type, flags);
}

int
TermpropsRegistry::install_alias(char const* name,
                                 char const* target_name)
{
        if (check_termprop_wellknown(name, nullptr, nullptr)) {
                g_warning("Denying to install well-known termprop \"%s\" as an alias", name);
                return -1;
        }

        if (check_termprop_blocklisted(name) ||
            check_termprop_blocklisted_alias(name)) {
                g_warning("Denying to install blocklisted termprop alias \"%s\"", name);
                return -1;
        }

        if (auto const info = vte::terminal::termprops_registry().lookup(name)) {
                g_warning("Termprop \"%s\" already registered", name);
                return -1;
        }

        auto const wk_target = check_termprop_wellknown_alias(name);

        if (wk_target && !g_str_equal(wk_target, target_name)) {
                g_warning("Denying to install well-known termprop alias \"%s\" to invalid target \"%s\"",
                          name, target_name);
                return -1;
        }

        // If not well-known alias, the name needs to start with "vte.ext."
        if (!wk_target) {
                g_return_val_if_fail(g_str_has_prefix(name, VTE_TERMPROP_NAME_PREFIX), -1);
                g_return_val_if_fail(vte::property::validate_termprop_name(name, 4), -1);
        }

        if (auto const info = lookup(target_name)) {
                return vte::property::Registry::install_alias(name, target_name);
        }

        g_warning("Cannot install termprop %s to unknown target %s\n",
                  name, target_name);
        return -1;
}

TermpropsRegistry::TermpropsRegistry()
{
        install_many({
                        {
                                VTE_PROPERTY_ID_CURRENT_DIRECTORY_URI,
                                VTE_TERMPROP_CURRENT_DIRECTORY_URI,
                                vte::property::Type::URI,
                                vte::property::Flags::NO_OSC
                        },
                        {
                                VTE_PROPERTY_ID_CURRENT_FILE_URI,
                                VTE_TERMPROP_CURRENT_FILE_URI,
                                vte::property::Type::URI,
                                vte::property::Flags::NO_OSC
                        },
                        {
                                VTE_PROPERTY_ID_XTERM_TITLE,
                                VTE_TERMPROP_XTERM_TITLE,
                                vte::property::Type::STRING,
                                vte::property::Flags::NO_OSC
                        },
                        {
                                VTE_PROPERTY_ID_CONTAINER_NAME,
                                VTE_TERMPROP_CONTAINER_NAME,
                                vte::property::Type::STRING,
                                vte::property::Flags::NONE
                        },
                        {
                                VTE_PROPERTY_ID_CONTAINER_RUNTIME,
                                VTE_TERMPROP_CONTAINER_RUNTIME,
                                vte::property::Type::STRING,
                                vte::property::Flags::NONE
                        },
                        {
                                VTE_PROPERTY_ID_CONTAINER_UID,
                                VTE_TERMPROP_CONTAINER_UID,
                                vte::property::Type::UINT,
                                vte::property::Flags::NONE
                        },
                        {
                                VTE_PROPERTY_ID_SHELL_PRECMD,
                                VTE_TERMPROP_SHELL_PRECMD,
                                vte::property::Type::VALUELESS,
                                vte::property::Flags::NONE
                        },
                        {
                                VTE_PROPERTY_ID_SHELL_PREEXEC,
                                VTE_TERMPROP_SHELL_PREEXEC,
                                vte::property::Type::VALUELESS,
                                vte::property::Flags::NONE
                        },
                        {
                                VTE_PROPERTY_ID_SHELL_POSTEXEC,
                                VTE_TERMPROP_SHELL_POSTEXEC,
                                vte::property::Type::UINT,
                                vte::property::Flags::EPHEMERAL
                        },
                        {
                                VTE_PROPERTY_ID_PROGRESS_HINT,
                                VTE_TERMPROP_PROGRESS_HINT,
                                vte::property::Type::INT,
                                vte::property::Flags::NONE,
                                [](std::string_view const& str) -> auto {
                                        return vte::property::impl::parse_termprop_integral_range<int64_t>(str, 0, 4);
                                }
                        },
                        {
                                VTE_PROPERTY_ID_PROGRESS_VALUE,
                                VTE_TERMPROP_PROGRESS_VALUE,
                                vte::property::Type::UINT,
                                vte::property::Flags::NONE,
                                [](std::string_view const& str) -> auto {
                                        return vte::property::impl::parse_termprop_integral_range<uint64_t>(str, 0, 100);
                                }
                        },
                        {
                                VTE_PROPERTY_ID_ICON_COLOR,
                                VTE_TERMPROP_ICON_COLOR,
                                vte::property::Type::RGB,
                                vte::property::Flags::NONE
                        },
                        {
                                VTE_PROPERTY_ID_ICON_IMAGE,
                                VTE_TERMPROP_ICON_IMAGE,
                                vte::property::Type::IMAGE,
                                vte::property::Flags::NONE,
                        },
                });
}

TermpropsRegistry& termprops_registry() noexcept
{
        static auto s_termprops_registry = TermpropsRegistry{};

        return s_termprops_registry;
}

} // namespace vte::systemd
