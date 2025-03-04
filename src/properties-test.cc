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

#include "properties.hh"

#include <cassert>
#include <string>

using namespace std::literals;
using namespace vte::property;
using namespace vte::terminal;

static void
assert_name_valid(std::string_view const& str,
                  int required_components = 2,
                  int line = __builtin_LINE()) noexcept
{
        assert(validate_termprop_name(str, required_components));
}

static void
assert_name_invalid(std::string_view const& str,
                  int required_components = 2,
                  int line = __builtin_LINE()) noexcept
{
        assert(!validate_termprop_name(str, required_components));
}

static void
test_termprops_names(void)
{
        assert_name_valid("a.b"sv);
        assert_name_valid("a.b.c"sv);
        assert_name_valid("a.b.c.d"sv);
        assert_name_valid("a-a.b"sv);
        assert_name_valid("a0.b"sv);
        assert_name_valid("a-a1.b"sv);
        assert_name_valid("a.b-b"sv);
        assert_name_valid("a.b1-b"sv);
        assert_name_valid("a"sv, 1);
        assert_name_valid("a.b"sv, 2);
        assert_name_valid("a.b.c"sv, 3);
        assert_name_valid("a.b.c.d"sv, 4);
        assert_name_valid("a.b.c.d.e"sv, 5);
        assert_name_invalid("a"sv);
        assert_name_invalid("a"sv);
        assert_name_invalid("a."sv);
        assert_name_invalid(".a"sv);
        assert_name_invalid("-a.b"sv);
        assert_name_invalid("0.b"sv);
        assert_name_invalid("0.b0a"sv);
        assert_name_invalid("-0.b"sv);
        assert_name_invalid("a.0"sv);
        assert_name_invalid("a.b0a"sv);
        assert_name_invalid("a.-b"sv);
        assert_name_invalid("a.-0"sv);
        assert_name_invalid("a"sv, 2);
        assert_name_invalid("a.b"sv, 3);
        assert_name_invalid("a.b.c"sv, 4);
        assert_name_invalid("a.b.c.d"sv, 5);
        assert_name_invalid("a.b.c.d.e"sv, 6);
        assert_name_invalid("a..b");
        assert_name_invalid("a--b");
        assert_name_invalid("A.b"sv);
        assert_name_invalid("a.B"sv);

        assert_name_invalid("a.b.0"sv, 3);
        assert_name_valid("a.b.0"sv, 2);

        assert_name_invalid("a.b.0-1"sv, 2);
}

static void
assert_termprop_parse_nothing(vte::property::Type type,
                              std::string_view const& str,
                              int line = __builtin_LINE()) noexcept
{
        auto value = parse_termprop_value(type, str);
        assert(!value);
}

static void
assert_registered(vte::property::Registry& registry,
                  char const* name,
                  vte::property::Type type) noexcept
{
        registry.install(name, type);
        auto const info = registry.lookup(name);
        assert(info);
        assert(info->type() == type);
}

static void
test_termprops_register(void)
{
        auto registry = vte::property::Registry{};
        assert_registered(registry, "test.valueless", Type::VALUELESS);
        assert_registered(registry, "test.bool", Type::BOOL);
        assert_registered(registry, "test.uint", Type::UINT);
        assert_registered(registry, "test.string", Type::STRING);
        assert_registered(registry, "test.data", Type::DATA);
}

template<typename T>
static void
assert_property_value(std::optional<vte::property::Value> const& value,
                      T const& expected_value = {},
                      int line = __builtin_LINE()) noexcept
{
        assert(value);
        assert(!value->valueless_by_exception());
        assert(std::holds_alternative<T>(*value));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        assert(std::get<T>(*value) == expected_value);
#pragma GCC diagnostic pop
}

template<typename T>
static void
assert_termprop_parse_value(vte::property::Type type,
                            std::string_view const& str,
                            T const& expected_value = {},
                            int line = __builtin_LINE()) noexcept
{
        auto const value = parse_termprop_value(type, str);
        assert_property_value(value, expected_value);

        auto tstr = unparse_termprop_value(type, *value);
        assert(tstr);
        auto const tvalue = parse_termprop_value(type, *tstr);
        assert_property_value(tvalue, expected_value);
        assert(value == tvalue);
        assert(*value == *tvalue);
}

template<std::integral T>
static void
assert_termprop_parse_integral_value(vte::property::Type type,
                                     std::string_view const& str,
                                     T const& expected_value = {},
                                     int line = __builtin_LINE()) noexcept
{
        using V = std::conditional_t<std::is_unsigned_v<T>, uint64_t, int64_t>;
        assert_termprop_parse_value<V>(type, str, V{expected_value}, line);
}

static void
assert_termprop_parse_uri(std::string_view const& str,
                          int line = __builtin_LINE()) noexcept
{
        auto const value = parse_termprop_value(Type::URI, str);
        assert(value);
        assert(!value->valueless_by_exception());
        assert(std::holds_alternative<vte::property::URIValue>(*value));

        assert(str == std::get<vte::property::URIValue>(*value).second);

        auto ustr = vte::glib::take_string(g_uri_to_string(std::get<vte::property::URIValue>(*value).first.get()));
        assert(ustr);
        assert(str == ustr.get());
}

static void
test_termprops_valueless(void)
{
        assert_termprop_parse_nothing(Type::VALUELESS, ""sv);
        assert_termprop_parse_nothing(Type::VALUELESS, "0"sv);
        assert_termprop_parse_nothing(Type::VALUELESS, "1"sv);
        assert_termprop_parse_nothing(Type::VALUELESS, "a"sv);
}

static void
test_termprops_bool(void)
{
        assert_termprop_parse_value<bool>(Type::BOOL, "0"sv, false);
        assert_termprop_parse_value<bool>(Type::BOOL, "1"sv, true);
        assert_termprop_parse_value<bool>(Type::BOOL, "false"sv, false);
        assert_termprop_parse_value<bool>(Type::BOOL, "true"sv, true);

        // Case variants
        assert_termprop_parse_value<bool>(Type::BOOL, "False"sv, false);
        assert_termprop_parse_value<bool>(Type::BOOL, "True"sv, true);
        assert_termprop_parse_value<bool>(Type::BOOL, "FALSE"sv, false);
        assert_termprop_parse_value<bool>(Type::BOOL, "TRUE"sv, true);

        // Invalid case variants
        assert_termprop_parse_nothing(Type::BOOL, "tRue"sv);
        assert_termprop_parse_nothing(Type::BOOL, "FaLSe"sv);

        // No other names
        assert_termprop_parse_nothing(Type::BOOL, "yes"sv);
        assert_termprop_parse_nothing(Type::BOOL, "no"sv);
}

static void
test_termprops_int(void)
{
        assert_termprop_parse_integral_value(Type::INT, "0"sv, 0ll);
        assert_termprop_parse_integral_value(Type::INT, "1"sv, 1ll);
        assert_termprop_parse_integral_value(Type::INT, "9223372036854775807"sv, 9223372036854775807ll);
        assert_termprop_parse_integral_value(Type::INT, "-1"sv, -1ll);
        assert_termprop_parse_integral_value(Type::INT, "-9223372036854775808"sv, INT64_MIN);
        assert_termprop_parse_nothing(Type::INT, "9223372036854775808"sv);
        assert_termprop_parse_nothing(Type::INT, "-9223372036854775809"sv);
        assert_termprop_parse_nothing(Type::INT, "0a"sv);
        assert_termprop_parse_nothing(Type::INT, "a0"sv);
        assert_termprop_parse_nothing(Type::INT, "-"sv);
        assert_termprop_parse_nothing(Type::INT, "-a"sv);
}

static void
test_termprops_uint(void)
{
        assert_termprop_parse_integral_value(Type::UINT, "0"sv, 0ull);
        assert_termprop_parse_integral_value(Type::UINT, "1"sv, 1ull);
        assert_termprop_parse_integral_value(Type::UINT, "18446744073709551614"sv, 18446744073709551614ull);
        assert_termprop_parse_integral_value(Type::UINT, "18446744073709551615"sv, 18446744073709551615ull);
        assert_termprop_parse_nothing(Type::UINT, "-1"sv);
        assert_termprop_parse_nothing(Type::UINT, "0a"sv);
        assert_termprop_parse_nothing(Type::UINT, "a0"sv);
        assert_termprop_parse_nothing(Type::UINT, "18446744073709551616"sv);
}

static void
test_termprops_double(void)
{
        assert_termprop_parse_value(Type::DOUBLE, "0"sv, 0.0);
        assert_termprop_parse_value(Type::DOUBLE, "0.1"sv, 0.1);
        assert_termprop_parse_value(Type::DOUBLE, "1.0"sv, 1.0);
        assert_termprop_parse_value(Type::DOUBLE, "2.0E8"sv, 2.0E8);

        // No leading whitespace
        assert_termprop_parse_nothing(Type::DOUBLE, " 1.0"sv);

        // No trailing whitespace
        assert_termprop_parse_nothing(Type::DOUBLE, "1.0 "sv);

        // No hex format
        assert_termprop_parse_nothing(Type::DOUBLE, "0x12345678"sv);

        // No infinities
        assert_termprop_parse_nothing(Type::DOUBLE, "Inf"sv);
        assert_termprop_parse_nothing(Type::DOUBLE, "-Inf"sv);

        // No NaNs
        assert_termprop_parse_nothing(Type::DOUBLE, "NaN"sv);
}

static void
test_termprops_rgb(void)
{
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGB, "#123456"sv, vte::color::from_bits<vte::property::property_rgba>(0x123456, 8, false));
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGB, "#abcdef"sv, vte::color::from_bits<vte::property::property_rgba>(0xabcdef, 8, false));
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGB, "#ABCDEF"sv, vte::color::from_bits<vte::property::property_rgba>(0xabcdef, 8, false));
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGB, "#000"sv, vte::color::from_bits<vte::property::property_rgba>(0, 8, false));
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGB, "#0000"sv, vte::color::from_bits<vte::property::property_rgba>(0, 8, false));
        assert_termprop_parse_nothing(Type::RGB, "0"sv);
        assert_termprop_parse_nothing(Type::RGB, "00"sv);
        assert_termprop_parse_nothing(Type::RGB, "000"sv);
        assert_termprop_parse_nothing(Type::RGB, "0000"sv);
        assert_termprop_parse_nothing(Type::RGB, "00000"sv);
        assert_termprop_parse_nothing(Type::RGB, "000000"sv);
        assert_termprop_parse_nothing(Type::RGB, "#0"sv);
        assert_termprop_parse_nothing(Type::RGB, "#00"sv);
        assert_termprop_parse_nothing(Type::RGB, "#00000"sv);
        //assert_termprop_parse_nothing(Type::RGB, "rgb(1,2,3)"sv);
}

static void
test_termprops_rgba(void)
{
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGBA, "#123456"sv, vte::color::from_bits<vte::property::property_rgba>(0x123456ff, 8, true));
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGBA, "#abcdef"sv, vte::color::from_bits<vte::property::property_rgba>(0xabcdefff, 8, true));
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGBA, "#ABCDEF"sv, vte::color::from_bits<vte::property::property_rgba>(0xabcdefff, 8, true));

        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGBA, "#12345678"sv, vte::color::from_bits<vte::property::property_rgba>(0x12345678, 8, true));
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGBA, "#abcdef01"sv, vte::color::from_bits<vte::property::property_rgba>(0xabcdef01, 8, true));
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGBA, "#ABCDEF76"sv, vte::color::from_bits<vte::property::property_rgba>(0xabcdef76, 8, true));
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGBA, "#000"sv, vte::color::from_bits<vte::property::property_rgba>(0, 8, false));
        assert_termprop_parse_value<vte::property::property_rgba>(Type::RGBA, "#0000"sv, vte::color::from_bits<vte::property::property_rgba>(0, 8, true));
        assert_termprop_parse_nothing(Type::RGBA, "0"sv);
        assert_termprop_parse_nothing(Type::RGBA, "00"sv);
        assert_termprop_parse_nothing(Type::RGBA, "000"sv);
        assert_termprop_parse_nothing(Type::RGBA, "0000"sv);
        assert_termprop_parse_nothing(Type::RGBA, "00000"sv);
        assert_termprop_parse_nothing(Type::RGBA, "000000"sv);
        assert_termprop_parse_nothing(Type::RGBA, "0000000"sv);
        assert_termprop_parse_nothing(Type::RGBA, "00000000"sv);
        assert_termprop_parse_nothing(Type::RGBA, "#0"sv);
        assert_termprop_parse_nothing(Type::RGBA, "#00"sv);
        assert_termprop_parse_nothing(Type::RGBA, "#00000"sv);
        //assert_termprop_parse_nothing(Type::RGBA, "rgb(1,2,3)"sv);
        //assert_termprop_parse_nothing(Type::RGBA, "rgba(1,2,3,4)"sv);
}

// Note that our OSC parser makes sure no C0 and C1 controls are
// present in the control string, so we do not need to test how the
// termprop parser handles these.

static void
test_termprops_string(void)
{
        assert_termprop_parse_value<std::string>(Type::STRING, ""sv, ""s);
        assert_termprop_parse_value<std::string>(Type::STRING, "abc"sv, "abc"s);

        auto const max_len = vte::property::Registry::k_max_string_len;

        auto str = std::string{};
        str.resize(max_len, 'a');
        assert_termprop_parse_value<std::string>(Type::STRING, str, str);

        str.push_back('a');
        assert_termprop_parse_nothing(Type::STRING, str);

        // Test escapes
        assert_termprop_parse_value<std::string>(Type::STRING, "a\\sb\\nc\\\\d"sv, "a;b\nc\\d"s);

        // Test string value containing the termprop assignment characters ! or =
        assert_termprop_parse_value<std::string>(Type::STRING, "a=b"sv, "a=b"s);
        assert_termprop_parse_value<std::string>(Type::STRING, "a!"sv, "a!"s);

        // Missing or invalid escapes
        assert_termprop_parse_nothing(Type::STRING, "a;b");
        assert_termprop_parse_nothing(Type::STRING, "a\\");
        assert_termprop_parse_nothing(Type::STRING, "a\\"sv);
        assert_termprop_parse_nothing(Type::STRING, "a\\a"sv);
}

static void
test_termprops_data(void)
{
        assert_termprop_parse_value<std::string>(Type::DATA, ""sv, ""s);
        assert_termprop_parse_value<std::string>(Type::DATA, "YQ=="sv, "a"s);
        assert_termprop_parse_value<std::string>(Type::DATA, "YWE="sv, "aa"s);
        assert_termprop_parse_value<std::string>(Type::DATA, "YWFh"sv, "aaa"s);
        assert_termprop_parse_value<std::string>(Type::DATA, "AA=="sv, "\0"s);
        assert_termprop_parse_value<std::string>(Type::DATA, "YQBi"sv, "a\0b"s);
        assert_termprop_parse_value<std::string>(Type::DATA, "YQBi"sv, "a\0b"s);
        assert_termprop_parse_value<std::string>(Type::DATA, "gMH/YWJj"sv, "\x80\xc1\xff""abc"s); // Note: not valid UTF-8 after decoding
        assert_termprop_parse_nothing(Type::DATA, "YQ="sv);
        assert_termprop_parse_nothing(Type::DATA, "YQ"sv);
        assert_termprop_parse_nothing(Type::DATA, "Y"sv);

        // Test max size
        for (auto size = Registry::k_max_data_len - 3; size < Registry::k_max_data_len + 3; ++size) {
                auto str = std::string(size, 'a');
                auto b64 = vte::glib::take_string
                        (g_base64_encode
                         (reinterpret_cast<unsigned char const*>(str.data()), str.size()));
                if (size <= Registry::k_max_data_len)
                        assert_termprop_parse_value<std::string>(Type::DATA, b64.get(), str);
                else
                        assert_termprop_parse_nothing(Type::DATA, b64.get());
        }
}

static void
test_termprops_uuid(void)
{
        auto const uuid = VTE_DEFINE_UUID(49ec5248, 2d9a, 493f, 99fa, 9e1cfb95b430);
        assert_termprop_parse_value<vte::uuid>(Type::UUID, "49ec5248-2d9a-493f-99fa-9e1cfb95b430"sv, uuid);
        assert_termprop_parse_value<vte::uuid>(Type::UUID, "{49ec5248-2d9a-493f-99fa-9e1cfb95b430}"sv, uuid);
        assert_termprop_parse_value<vte::uuid>(Type::UUID, "urn:uuid:49ec5248-2d9a-493f-99fa-9e1cfb95b430"sv, uuid);
        assert_termprop_parse_nothing(Type::UUID, "49ec5248-2d9a-493f-99fa-9e1cfb95b43"sv);
        assert_termprop_parse_nothing(Type::UUID, "{49ec5248-2d9a-493f-99fa-9e1cfb95b430"sv);
        assert_termprop_parse_nothing(Type::UUID, "urn:49ec5248-2d9a-493f-99fa-9e1cfb95b430"sv);
        assert_termprop_parse_nothing(Type::UUID, "{urn:uuid:49ec5248-2d9a-493f-99fa-9e1cfb95b430}"sv);
        assert_termprop_parse_nothing(Type::UUID, "49ec52482d9a493f99fa9e1cfb95b430"sv); // ID128 form not accepted here
}

static void
test_termprops_uri(void)
{
        assert_termprop_parse_uri("https://www.gnome.org/index.html"sv);
        assert_termprop_parse_uri("file:///uri/bin"sv);
        assert_termprop_parse_nothing(Type::URI, "data:text/plain;base64,QQo=");
        assert_termprop_parse_nothing(Type::URI, "data:text/plain%3BQbase64,Qo=");
}

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/property/termprops/names", test_termprops_names);
        g_test_add_func("/vte/property/termprops/register", test_termprops_register);
        g_test_add_func("/vte/property/termprops/type/valueless", test_termprops_valueless);
        g_test_add_func("/vte/property/termprops/type/bool", test_termprops_bool);
        g_test_add_func("/vte/property/termprops/type/int", test_termprops_int);
        g_test_add_func("/vte/property/termprops/type/uint", test_termprops_uint);
        g_test_add_func("/vte/property/termprops/type/double", test_termprops_double);
        g_test_add_func("/vte/property/termprops/type/rgb", test_termprops_rgb);
        g_test_add_func("/vte/property/termprops/type/rgba", test_termprops_rgba);
        g_test_add_func("/vte/property/termprops/type/string", test_termprops_string);
        g_test_add_func("/vte/property/termprops/type/data", test_termprops_data);
        g_test_add_func("/vte/property/termprops/type/uuid", test_termprops_uuid);
        g_test_add_func("/vte/property/termprops/type/uri", test_termprops_uri);
;

        return g_test_run();
}
