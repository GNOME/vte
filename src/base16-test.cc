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

#include <cassert>

#include <glib.h>

#include "base16.hh"

using namespace std::literals;

static bool
assert_streq(std::string_view const& str1,
             std::string_view const& str2)
{
        auto s1 = std::string{str1};
        auto s2 = std::string{str2};

        g_assert_cmpstr(s1.c_str(), ==, s2.c_str());
        return true;
}

static void
assert_base16_encode(std::string_view const& str,
                     std::string_view const& expected,
                     int line = __builtin_LINE())
{
        auto rv = vte::base16_encode(str);
        assert_streq(rv, expected);
}

static void
test_base16_encode(void)
{
        assert_base16_encode(""sv, ""sv);

        auto in = std::string{};
        assert_base16_encode("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"sv, "000102030405060708090A0B0C0D0E0F"sv);
        assert_base16_encode("\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"sv, "101112131415161718191A1B1C1D1E1F"sv);
        assert_base16_encode("\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"sv, "202122232425262728292A2B2C2D2E2F"sv);
        assert_base16_encode("\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"sv, "303132333435363738393A3B3C3D3E3F"sv);
        assert_base16_encode("\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"sv, "404142434445464748494A4B4C4D4E4F"sv);
        assert_base16_encode("\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"sv, "505152535455565758595A5B5C5D5E5F"sv);
        assert_base16_encode("\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"sv, "606162636465666768696A6B6C6D6E6F"sv);
        assert_base16_encode("\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"sv, "707172737475767778797A7B7C7D7E7F"sv);
        assert_base16_encode("\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"sv, "808182838485868788898A8B8C8D8E8F"sv);
        assert_base16_encode("\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"sv, "909192939495969798999A9B9C9D9E9F"sv);
        assert_base16_encode("\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"sv, "A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"sv);
        assert_base16_encode("\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"sv, "B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"sv);
        assert_base16_encode("\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"sv, "C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF"sv);
        assert_base16_encode("\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"sv, "D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"sv);
        assert_base16_encode("\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"sv, "E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF"sv);
        assert_base16_encode("\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"sv, "F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF"sv);
}

static void
assert_base16_decode_nothing(std::string_view const& str,
                             bool allow_8bit = true,
                             int line = __builtin_LINE()) noexcept
{
        auto value = vte::base16_decode(str, allow_8bit);
        assert(!value);
}

static void
assert_base16_decode(std::string_view const& str,
                     std::string_view const& expected,
                     bool allow_8bit = true,
                     int line = __builtin_LINE())
{
        auto rv = vte::base16_decode(str, allow_8bit);
        assert(rv);
        assert_streq(*rv, expected);
}

static void
test_base16_decode(void)
{
        assert_base16_decode(""sv, ""sv);
        assert_base16_decode("000102030405060708090A0B0C0D0E0F"sv, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"sv);
        assert_base16_decode("101112131415161718191A1B1C1D1E1F"sv, "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"sv);
        assert_base16_decode("202122232425262728292A2B2C2D2E2F"sv, "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"sv);
        assert_base16_decode("303132333435363738393A3B3C3D3E3F"sv, "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"sv);
        assert_base16_decode("404142434445464748494A4B4C4D4E4F"sv, "\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"sv);
        assert_base16_decode("505152535455565758595A5B5C5D5E5F"sv, "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"sv);
        assert_base16_decode("606162636465666768696A6B6C6D6E6F"sv, "\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"sv);
        assert_base16_decode("707172737475767778797A7B7C7D7E7F"sv, "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"sv);
        assert_base16_decode("808182838485868788898A8B8C8D8E8F"sv, "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"sv);
        assert_base16_decode("909192939495969798999A9B9C9D9E9F"sv, "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"sv);
        assert_base16_decode("a0a1a2a3a4a5a6a7a8a9aAaBaCaDaEaF"sv, "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"sv);
        assert_base16_decode("b0b1b2b3b4b5b6b7b8b9bAbBbCbDbEbF"sv, "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"sv);
        assert_base16_decode("c0c1c2c3c4c5c6c7c8c9cAcBcCcDcEcF"sv, "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"sv);
        assert_base16_decode("d0d1d2d3d4d5d6d7d8d9dAdBdCdDdEdF"sv, "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"sv);
        assert_base16_decode("e0e1e2e3e4e5e6e7e8e9eAeBeCeDeEeF"sv, "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"sv);
        assert_base16_decode("f0f1f2f3f4f5f6f7f8f9fAfBfCfDfEfF"sv, "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"sv);
        assert_base16_decode("A0A1A2A3A4A5A6A7A8A9AAABACADAEAF"sv, "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"sv);
        assert_base16_decode("B0B1B2B3B4B5B6B7B8B9BABBBCBDBEBF"sv, "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"sv);
        assert_base16_decode("C0C1C2C3C4C5C6C7C8C9CACBCCCDCECF"sv, "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"sv);
        assert_base16_decode("D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF"sv, "\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"sv);
        assert_base16_decode("E0E1E2E3E4E5E6E7E8E9EAEBECEDEEEF"sv, "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"sv);
        assert_base16_decode("F0F1F2F3F4F5F6F7F8F9FAFBFCFDFEFF"sv, "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"sv);

        assert_base16_decode("808182838485868788898A8B8C8D8E8F"sv, "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"sv);
        assert_base16_decode_nothing("808182838485868788898A8B8C8D8E8F"sv, false);

        assert_base16_decode_nothing("GHIJKLMNOPQRSTUVWXYZ"sv);
}

int
main(int argc,
     char* argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/base16/encode", test_base16_encode);
        g_test_add_func("/vte/base16/decode", test_base16_decode);

        return g_test_run();
}
