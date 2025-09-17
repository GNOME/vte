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

#pragma once

#include <glib.h>

#include "fwd.hh"
#include "uuid.hh"
#include "color.hh"
#include "color-parser.hh"
#include "glib-glue.hh"
#include "cairo-glue.hh"
#include "cxx-utils.hh"

#include <cassert>
#include <cmath> // for std::isfinite

#include <charconv>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <version>

#include <fast_float/fast_float.h>

namespace vte::property {

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

// Properties. Make sure the enum values are the same as in the
// public VtePropertyType enum.
enum class Type {
        VALUELESS,
        BOOL,
        INT,
        UINT,
        DOUBLE,
        RGB,
        RGBA,
        STRING,
        DATA,
        UUID,
        URI,
        IMAGE,
        INVALID = -1,
};

enum class Flags {
        // public
        NONE = 0u,
        EPHEMERAL = 1u << 0,

        // private
        NO_OSC = 1u << 1, // not settable via the termprop OSC
};

VTE_CXX_DEFINE_BITMASK(Flags)

#ifdef VTE_GTK
#if VTE_GTK == 3
using property_rgba = vte::color::rgba_base<double>;
#elif VTE_GTK == 4
using property_rgba = vte::color::rgba_base<float>;
#else
#error
#endif
#else // Properties test
using property_rgba = vte::color::rgba_base<double>;
#endif

using URIValue = std::pair<vte::Freeable<GUri>, std::string>;

using Value = std::variant<std::monostate,
                           bool,
                           int64_t,
                           uint64_t,
                           double,
                           property_rgba,
                           vte::uuid,
                           std::string,
                           URIValue,
                           vte::Freeable<cairo_surface_t>>;

namespace impl {

        template<class CharT,
                 class Traits = std::char_traits<CharT>>
        class BasicStringHash {
        public:

                using string_view_type = std::basic_string_view<CharT, Traits>;
                using string_type = std::basic_string<CharT, Traits>;
                using hash_type = std::hash<string_view_type>;
                using is_transparent = void;

                constexpr auto operator()(CharT const* str) const noexcept { return hash_type{}(str); }
                constexpr auto operator()(string_view_type const& str) const noexcept { return hash_type{}(str); }
                constexpr auto operator()(string_type const& str) const noexcept { return hash_type{}(str); }

        }; // class BasicStringHash

        using StringHash = BasicStringHash<char>;

        using map_type = std::unordered_map<std::string,
                                            int,
                                            StringHash,
                                            std::equal_to<>>;

} // namespace impl

class Registry {
public:
        static inline constexpr auto const k_max_string_len = 1024u;
        static inline constexpr auto const k_max_data_len = 2048u;

        using ParseFunc = std::function<std::optional<Value>(std::string_view)>;

        class Property {
        private:
                int m_id{-1};
                unsigned m_quark{0};
                Type m_type{Type::INVALID};
                Flags m_flags{Flags::NONE};
                ParseFunc m_parse;

        public:

                Property(int id,
                         unsigned quark,
                         Type type,
                         Flags flags,
                         ParseFunc pf = {}) noexcept
                        : m_id{id},
                          m_quark{quark},
                          m_type{type},
                          m_flags{flags},
                          m_parse{pf}
                {
                }

                Property(int id,
                         char const* name,
                         Type type,
                         Flags flags,
                         ParseFunc pf = {}) noexcept
                        : Property{id, g_quark_from_string(name), type, flags, pf}
                {
                }

                Property() noexcept = delete;
                Property(Property const&) = default;
                Property(Property&&) = default;
                ~Property() = default;

                Property& operator=(Property const&) = default;
                Property& operator=(Property&&) = default;

                constexpr auto id() const noexcept { return m_id; }
                constexpr auto quark() const noexcept { return m_quark; }
                constexpr auto type() const noexcept { return m_type; }
                constexpr auto flags() const noexcept { return m_flags; }
                constexpr auto& parse_func() const noexcept { return m_parse; }

                auto name() const noexcept { return g_quark_to_string(quark()); }

                auto parse(std::string_view const& str) const -> std::optional<Value>
                {
                        return m_parse(str);
                }

        }; // class Property

private:
        std::vector<Property> m_registered_properties{};

        impl::map_type m_registered_properties_by_name{};

        inline auto
        append(char const* name,
               Type type,
               Flags flags = Flags::NONE,
               ParseFunc func = {}) -> auto
        {
                auto const id = int(m_registered_properties.size());
                m_registered_properties.emplace_back(id, name, type, flags,
                                                     func ? func : resolve_parse_func(type));
                m_registered_properties_by_name.try_emplace(std::string{name}, id);
                return id;
        }

        inline auto
        append(int prop_id,
               char const* name,
               Type type,
               Flags flags = Flags::NONE,
               ParseFunc func = {}) -> auto
        {
                auto const id = append(name, type, flags,
                                       func ? func : resolve_parse_func(type));
                assert(m_registered_properties[id].id() == prop_id);
                return id;
        }

        inline auto
        append_alias(char const* name,
                     Property const& info) -> auto
        {
                m_registered_properties_by_name.try_emplace(std::string{name}, info.id());
                return info.id();
        }

public:
        Registry() = default;
        virtual ~Registry() = default;

        Registry(Registry const&) = delete;
        Registry(Registry&&) = delete;

        Registry operator=(Registry const&) = delete;
        Registry operator=(Registry&&) = delete;

        // Can't make this a constructor since then it'd always call the
        // resolve_parse_func of the base class, not of the derived class.
        void install_many(std::initializer_list<Property> list)
        {
                for (auto&& info : list) {
                        m_registered_properties.emplace_back(info.id(),
                                                             info.quark(),
                                                             info.type(),
                                                             info.flags(),
                                                             info.parse_func() ? info.parse_func() : resolve_parse_func(info.type()));
                        m_registered_properties_by_name.try_emplace(std::string{info.name()},
                                                                   info.id());
                }
        }

        virtual int install(char const* name,
                            Type type,
                            Flags flags = Flags::NONE)
        {
                return append(name, type, flags, {});
        }

        virtual int install_alias(char const* name,
                                  char const* target_name)
        {
                return append_alias(name, *lookup(target_name));
        }

        constexpr auto&
        get_all() const noexcept
        {
                return m_registered_properties;
        }

        constexpr auto&
        get_all_by_name() const noexcept
        {
                return m_registered_properties_by_name;
        }

        inline constexpr auto
        size() const noexcept -> auto
        {
                return m_registered_properties.size();
        }

        inline Property const*
        lookup(int id) const
        {
                return std::addressof(m_registered_properties.at(id));
        }

        inline Property const*
        lookup(std::string_view const& str) const noexcept
        {
                auto const prop = m_registered_properties_by_name.find(str);
                return prop == std::end(m_registered_properties_by_name) ? nullptr : lookup(prop->second);
        }

        inline int
        lookup_id(std::string_view const& str) const noexcept
        {
                auto const prop = m_registered_properties_by_name.find(str);
                return prop == std::end(m_registered_properties_by_name) ? -1 : prop->second;
        }

        virtual ParseFunc resolve_parse_func(Type type);

}; // class Registry

// validate_termprop_name:
// @str:
//
// Validates that @str is a valid termprop name. Valid name consists of at least two
// non-empty components delimited by dots ('.'), each component starting with a letter,
// and have least one letter after any dash ('-'), and does not contain consecutive
// dashes.
//
// Returns: %true iff the name is valid
//
inline bool
validate_termprop_name(std::string_view const& str,
                       int n_components_required = 2) noexcept
{
        auto allow_dot = false, allow_letter = true, allow_digit = false;
        auto n_dots = 0;
        auto len = 0;
        for (auto const c : str) {
                ++len;
                switch (c) {
                case '0' ... '9':
                        if (!allow_digit)
                                return false;
                        allow_letter = false;
                        allow_dot = true;
                        break;
                case 'a' ... 'z':
                        if (!allow_letter)
                                return false;

                        allow_dot = allow_digit = true;
                        break;
                case '.':
                        ++n_dots;
                        [[fallthrough]];
                case '-':
                        if (!allow_dot)
                                return false;
                        allow_dot = false;
                        allow_digit = (c == '.' && n_dots >= n_components_required);
                        allow_letter = true;
                        len = 0;
                        break;
                default:
                        return false;
                }
        }

        return (n_dots + 1) >= n_components_required && len > 0;
}

namespace impl {

inline std::optional<Value>
parse_termprop_base64(std::string_view const& str) noexcept
{
        auto const max_size = (str.size() / 4) * 3 + 3;
        auto buf = std::string{};
        auto state = 0;
        buf.resize_and_overwrite
                (max_size,
                 [&](char* data,
                     size_t buf_size) noexcept -> size_t {
                         auto save = 0u;
                         return g_base64_decode_step(str.data(),
                                                     str.size(),
                                                     reinterpret_cast<unsigned char*>(data),
                                                     &state,
                                                     &save);
                 });

        if (state != 0 || buf.size() > Registry::k_max_data_len)
                return std::nullopt;

        return buf;
}

inline std::optional<std::string>
unparse_termprop_base64(std::string_view const& str)
{
        return std::make_optional<std::string>
                (vte::glib::take_string
                 (g_base64_encode(reinterpret_cast<unsigned char const*>(str.data()),
                                  str.size())).get());
}

inline std::optional<Value>
parse_termprop_bool(std::string_view const& str) noexcept
{
        if (str == "1"sv ||
            str == "true"sv ||
            str == "True"sv ||
            str == "TRUE"sv)
                return true;
        else if (str == "0"sv ||
                 str == "false"sv ||
                 str == "False"sv ||
                 str == "FALSE"sv)
                return false;
        else
                return std::nullopt;
}

inline std::optional<std::string>
unparse_termprop_bool(bool v)
{
        return std::make_optional(v ? "1"s : "0"s);
}

template<bool with_alpha>
inline std::optional<Value>
parse_termprop_color(std::string_view const& str_view) noexcept
{
        if (auto value = vte::color::parse_any<property_rgba>(std::string{str_view})) {
                auto color = *value;
                if (!with_alpha)
                        color = {color.red(), color.green(), color.blue(), property_rgba::component_type(1.0)};

                return color;
        }

        return std::nullopt;
}

template<bool alpha>
inline std::optional<std::string>
unparse_termprop_color(property_rgba const& v)
{
        return std::make_optional(vte::color::to_string(v, alpha, vte::color::color_output_format::HEX));
}

template<std::integral T>
inline std::optional<Value>
parse_termprop_integral(std::string_view const& str) noexcept
{
        auto v = T{};
        if (auto [ptr, err] = fast_float::from_chars(std::begin(str),
                                                     std::end(str),
                                                     v);
            err == std::errc() && ptr == std::end(str)) {
                if constexpr (std::is_unsigned_v<T>) {
                        return uint64_t(v);
                } else {
                        return int64_t(v);
                }
        }

        return std::nullopt;
}

template<std::integral T>
inline std::optional<std::string>
unparse_termprop_integral(T v)
{
        char buf[64];
        if (auto [ptr, err] = std::to_chars(buf,
                                            buf + sizeof(buf),
                                            v);
            err == std::errc()) {
                return std::make_optional<std::string>(buf, ptr);
        }

        return std::nullopt;
}

template<std::integral T>
inline std::optional<Value>
parse_termprop_integral_range(std::string_view const& str,
                              T min_v,
                              T max_v) noexcept
{
        if (auto value = parse_termprop_integral<T>(str);
            value &&
            std::holds_alternative<T>(*value) &&
            std::get<T>(*value) >= min_v &&
            std::get<T>(*value) <= max_v) {
                return value;
        }

        return std::nullopt;
}

template<std::floating_point T>
inline std::optional<Value>
parse_termprop_floating(std::string_view const& str) noexcept
{
        auto v = T{};
        if (auto [ptr, err] = fast_float::from_chars(std::begin(str),
                                                     std::end(str),
                                                     v);
            err == std::errc() &&
            ptr == std::end(str) &&
            std::isfinite(v)) {
                return double(v);
        }

        return std::nullopt;
}

template<std::floating_point T>
inline std::optional<std::string>
unparse_termprop_floating(T v)
{
        char buf[64];
        if (auto [ptr, err] = std::to_chars(buf,
                                            buf + sizeof(buf),
                                            v,
                                            std::chars_format::scientific);
            err == std::errc()) {
                return std::make_optional<std::string>(buf, ptr);
        }

        return std::nullopt;
}

// On return, @pos will point to the first character not parsed yet.
inline std::optional<char8_t>
parse_string_escape(std::string_view const& str,
                    std::string_view::size_type& pos) noexcept
{
        if (pos == str.size())
                return std::nullopt;

        switch (char8_t(str[pos++])) {
        case 'n':  return u8'\n'; // U+000A LINE FEED (LF)
        case '\\': return u8'\\'; // U+005C REVERSE SOLIDUS
        case 's':  return u8';'; // U+003B SEMICOLON
        default:   return std::nullopt; // unsupported escape
        }
}

inline std::optional<Value>
parse_termprop_string(std::string_view str)
{
        static constinit auto const needle = std::string_view{"\\;"};

        auto unescaped = std::string{};
        unescaped.reserve(str.size());

        auto ok = true;
        while (str.size() != 0) {
                auto run = str.find_first_of(needle, 0);

                unescaped.append(str, 0, run);
                if (run == str.npos)
                        break;

                auto const c = str[run];
                if (c == '\\') [[likely]] {
                        ++run;
                        auto ec = parse_string_escape(str, run);
                        ok = bool(ec);
                        if (!ok)
                                break;

                        // c is a 7-bit character actually, so can just append
                        unescaped.push_back(*ec);
                } else if (c == ';') {
                        // unescaped semicolon
                        ok = false;
                        break;
                } else {
                        // Can't happen
                        __builtin_unreachable();
                        ++run;
                }

                str = str.substr(run);
        }

        if (ok &&
            g_utf8_strlen(unescaped.c_str(), unescaped.size()) <= Registry::k_max_string_len)
                return unescaped;

        return std::nullopt;
}

inline std::optional<std::string>
unparse_termprop_string(std::string_view str)
{
        static constinit auto const needle = ";\n\\"sv;

        auto ostr = std::string{};
        while (str.size() != 0) {
                auto run = str.find_first_of(needle, 0);

                ostr.append(str, 0, run);
                if (run == str.npos)
                        break;

                ostr.push_back('\\');
                switch (str[run]) {
                case '\n': ostr.push_back('n');  break;
                case '\\': ostr.push_back('\\'); break;
                case ';':  ostr.push_back('s');  break;
                default: __builtin_unreachable(); break;
                }

                ++run;
                str = str.substr(run);
        }

        return std::make_optional(std::move(ostr));
}

inline std::optional<Value>
parse_termprop_uuid(std::string_view str)
{
        try {
                return vte::uuid{str, vte::uuid::format::ANY};
        } catch (...) {
                return std::nullopt;
        }
}

inline std::optional<std::string>
unparse_termprop_uuid(vte::uuid const& u)
{
        return std::make_optional(u.str());
}

inline std::optional<Value>
parse_termprop_uri(std::string_view str)
{
        if (auto uri = vte::take_freeable(g_uri_parse(std::string{str}.c_str(),
                                                      GUriFlags(G_URI_FLAGS_NONE),
                                                      nullptr));
            uri &&
            g_uri_get_scheme(uri.get()) &&
            !g_str_equal(g_uri_get_scheme(uri.get()), "data")) {
                return std::make_optional<Value>(std::in_place_type<URIValue>, std::move(uri), str);
        }

        return std::nullopt;
}

inline std::optional<Value>
parse_termprop_file_uri(std::string_view str)
{
        if (auto value = parse_termprop_uri(str);
            value &&
            std::holds_alternative<URIValue>(*value) &&
            "file"sv == g_uri_get_scheme(std::get<URIValue>(*value).first.get())) {
                return value;
        }

        return std::nullopt;
}

inline std::optional<std::string>
unparse_termprop_uri(URIValue const& v)
{
        return std::make_optional<std::string>(v.second);
}

} // namespace impl

inline std::optional<Value>
parse_termprop_value(Type type,
                     std::string_view const& value) noexcept
{
        switch (type) {
                using enum Type;
        case Type::VALUELESS:
                return std::nullopt;

        case Type::BOOL:
                return impl::parse_termprop_bool(value);

        case Type::INT:
                return impl::parse_termprop_integral<int64_t>(value);

        case Type::UINT:
                return impl::parse_termprop_integral<uint64_t>(value);

        case Type::DOUBLE:
                return impl::parse_termprop_floating<double>(value);

        case Type::RGB:
                return impl::parse_termprop_color<false>(value);

        case Type::RGBA:
                return impl::parse_termprop_color<true>(value);

        case Type::STRING:
                return impl::parse_termprop_string(value);

        case Type::DATA:
                return impl::parse_termprop_base64(value);

        case Type::UUID:
                return impl::parse_termprop_uuid(value);

        case Type::URI:
                return impl::parse_termprop_uri(value);

        case Type::IMAGE: // not settable this way
                return std::nullopt;

        default:
                __builtin_unreachable();
                return std::nullopt;
        }
}

inline std::optional<std::string>
unparse_termprop_value(Type type,
                       Value const& value)
{
        switch (type) {
                using enum Type;
        case VALUELESS:
                break;

        case BOOL:
                if (std::holds_alternative<bool>(value)) {
                        return impl::unparse_termprop_bool(std::get<bool>(value));
                }
                break;

        case INT:
                if (std::holds_alternative<int64_t>(value)) {
                        return impl::unparse_termprop_integral(std::get<int64_t>(value));
                }
                break;

        case UINT:
                if (std::holds_alternative<uint64_t>(value)) {
                        return impl::unparse_termprop_integral(std::get<uint64_t>(value));
                }
                break;

        case DOUBLE:
                if (std::holds_alternative<double>(value)) {
                        return impl::unparse_termprop_floating(std::get<double>(value));
                }
                break;

        case RGB:
                if (std::holds_alternative<property_rgba>(value)) {
                        return impl::unparse_termprop_color<false>(std::get<property_rgba>(value));
                }
                break;

        case RGBA:
                if (std::holds_alternative<property_rgba>(value)) {
                        return impl::unparse_termprop_color<true>(std::get<property_rgba>(value));
                }
                break;

        case STRING:
                if (std::holds_alternative<std::string>(value)) {
                        return impl::unparse_termprop_string(std::get<std::string>(value));
                }
                break;

        case DATA:
                if (std::holds_alternative<std::string>(value)) {
                        return impl::unparse_termprop_base64(std::get<std::string>(value));
                }
                break;

        case UUID:
                if (std::holds_alternative<vte::uuid>(value)) {
                        return impl::unparse_termprop_uuid(std::get<vte::uuid>(value));
                }
                break;

        case URI:
                if (std::holds_alternative<URIValue>(value)) {
                        return impl::unparse_termprop_uri(std::get<URIValue>(value));
                }
                break;

        case IMAGE: // not serialisable
                break;

        default:
                __builtin_unreachable();
                break;
        }

        return std::nullopt;
}

inline Registry::ParseFunc
Registry::resolve_parse_func(Type type)
{
        switch (type) {
                using enum Type;
        case VALUELESS: return {};
        case BOOL: return &impl::parse_termprop_bool;
        case INT: return &impl::parse_termprop_integral<int64_t>;
        case UINT: return &impl::parse_termprop_integral<uint64_t>;
        case DOUBLE: return &impl::parse_termprop_floating<double>;
        case RGB: return &impl::parse_termprop_color<false>;
        case RGBA: return &impl::parse_termprop_color<true>;
        case STRING: return &impl::parse_termprop_string;
        case DATA: return &impl::parse_termprop_base64;
        case UUID: return &impl::parse_termprop_uuid;
        case URI: return &impl::parse_termprop_uri;
        case IMAGE: return {}; // not parseable
        default: __builtin_unreachable(); return {};
        }
}

class Store {
private:
        Registry const& m_registry;
        std::vector<Value> m_values{};
        bool m_ephemeral_values_observable{false};

public:
        Store() = delete;
        ~Store() = default;

        Store(Store const&) = delete;
        Store(Store&&) = delete;

        Store operator=(Store const&) = delete;
        Store operator=(Store&&) = delete;

        explicit Store(Registry const& registry)
                : m_registry{registry},
                  m_values(registry.size())
        {
                assert(m_values.size() == m_registry.size());
        }

        void set_ephemeral_values_observable(bool v) noexcept
        {
                m_ephemeral_values_observable = v;
        }

        auto const& registry() const noexcept { return m_registry; }

        auto lookup(int id) const -> decltype(auto)
        {
                return m_registry.lookup(id);
        }

        auto lookup(std::string_view name) const -> decltype(auto)
        {
                return m_registry.lookup(name);
        }

        auto lookup_checked(int id) const -> decltype(auto)
        {
                auto const info = m_registry.lookup(id);

                return info &&
                        (!(unsigned(info->flags()) & unsigned(Flags::EPHEMERAL)) ||
                         m_ephemeral_values_observable) ? info : nullptr;
        }

        constexpr auto size() const noexcept
        {
                return m_values.size();
        }

        constexpr auto value(Registry::Property const& info) const -> decltype(auto)
        {
                return std::addressof(m_values.at(info.id()));
        }

        constexpr auto value(Registry::Property const& info) -> decltype(auto)
        {
                return std::addressof(m_values.at(info.id()));
        }

        constexpr auto value(int id) const -> decltype(auto)
        {
                return std::addressof(m_values.at(id));
        }

        constexpr auto value(int id) -> decltype(auto)
        {
                return std::addressof(m_values.at(id));
        }
}; // class Store

class TrackingStore final : public Store {
private:
        std::vector<bool> m_dirty{}; // FIMXE: make this a dynamic_bitset

public:

        explicit TrackingStore(Registry const& registry)
                : Store{registry},
                  m_dirty(registry.size())
        {
                assert(m_dirty.size() == registry.size());
        }

        ~TrackingStore() = default;

        constexpr auto dirty(Registry::Property const& info) const -> decltype(auto)
        {
                return m_dirty.at(info.id());
        }

        constexpr auto dirty(Registry::Property const& info) -> decltype(auto)
        {
                return m_dirty.at(info.id());
        }

        constexpr auto dirty(unsigned id) const -> decltype(auto)
        {
                return m_dirty.at(id);
        }

        constexpr auto dirty(unsigned id) -> decltype(auto)
        {
                return m_dirty.at(id);
        }

        void reset(Registry::Property const& info)
        {
                auto const is_valueless = info.type() == Type::VALUELESS;
                auto v = value(info);
                if (v &&
                    !std::holds_alternative<std::monostate>(*v)) {
                        *v = {};
                        dirty(info) = !is_valueless;
                } else if (is_valueless) {
                        dirty(info) = false;
                }
        }

        void reset_termprops()
        {
                for (auto const& info: registry().get_all()) {
                        reset(info);
                }
        }

}; // class TrackingStore

} // namespace vte::property
