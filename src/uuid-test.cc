/*
 * Copyright © 2013-2015 Red Hat, Inc.
 * Copyright © 2022, 2023 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 *(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "uuid.hh"
#include "uuid-fmt.hh"

#include <glib.h>

using namespace vte;

constinit auto const dummy_uuid = VTE_DEFINE_UUID(1, 2, 1003, 8004, 5);
constinit auto const gnome_uuid3 = VTE_DEFINE_UUID(eeec79ff, 4091, 3991, a17c, 1cbd847e92db);
constinit auto const gnome_uuid5 = VTE_DEFINE_UUID(362b097a, 0554, 5ee4, bb28, 6173eaf6bbef);

// The NIL UUID
constinit uuid const nil = uuid{};

// When constructing a v3 or v5 UUID using this namespace,
// the name string is a FQDN
constinit uuid const uuid_namespace_dns = VTE_DEFINE_UUID(6ba7b810, 9dad, 11d1, 80b4, 00c04fd430c8);

// When constructing a v3 or v5 UUID using this namespace,
// the name string is an URL
constinit uuid const uuid_namespace_url = VTE_DEFINE_UUID(6ba7b811, 9dad, 11d1, 80b4, 00c04fd430c8);

// When constructing a v3 or v5 UUID using this namespace,
// the name string is an ISO OID
constinit uuid const uuid_namespace_oid = VTE_DEFINE_UUID(6ba7b812, 9dad, 11d1, 80b4, 00c04fd430c8);

// When constructing a v3 or v5 UUID using this namespace,
// the name string is an X.500 DN in DER or text output format
constinit uuid const uuid_namespace_x500 = VTE_DEFINE_UUID(6ba7b814, 9dad, 11d1, 80b4, 00c04fd430c8);

static void
test_uuid_equal(void)
{
        auto const nil_uuid = uuid();
        g_assert_true(nil == nil_uuid);
        g_assert_true(nil_uuid.is_nil());
        g_assert_true(nil_uuid == nil_uuid);
        g_assert_false(nil_uuid == dummy_uuid);

        auto const copy_uuid = dummy_uuid;
        g_assert_true(copy_uuid == dummy_uuid);
}

static void
assert_bytes_equal(uint8_t const* bytes,
                   uint8_t const data[16],
                   int line = __builtin_LINE())
{
        for (auto i = 0u; i < 16; ++i)
                g_assert_cmphex(bytes[i], ==, data[i]);
}

static void
test_uuid_bytes(void)
{
        uint8_t const data[16] = {0x4c, 0x4e, 0xd7, 0xc6, 0x70, 0xc1, 0x41, 0xbd, 0xb7, 0x96, 0xb1, 0x86, 0xfc, 0xfc, 0xa3, 0xa9};

        auto const u1 = VTE_DEFINE_UUID(4c4ed7c6, 70c1, 41bd, b796, b186fcfca3a9);
        assert_bytes_equal(u1.bytes(), data);

        auto const u2 = uuid{"4c4ed7c6-70c1-41bd-b796-b186fcfca3a9", uuid::format::SIMPLE};
        assert_bytes_equal(u2.bytes(), data);
}

static void
test_uuid_string(void)
{
        g_assert_false(uuid_string_is_valid("6079c6d3-ffe3-42ac-a3cf"));
        g_assert_false(uuid_string_is_valid("zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz"));

        g_assert_false(uuid_string_is_valid("{6079c6d3-ffe3-42ac-a3cf-7137b101b6ca"));
        g_assert_false(uuid_string_is_valid("(6079c6d3-ffe3-42ac-a3cf-7137b101b6ca}"));
        g_assert_false(uuid_string_is_valid(" 6079c6d3-ffe3-42ac-a3cf-7137b101b6ca "));
        g_assert_false(uuid_string_is_valid("urn:uuid:{6079c6d3-ffe3-42ac-a3cf-7137b101b6ca}"));

        g_assert_true(uuid_string_is_valid("00000000-0000-0000-0000-000000000000"));
        g_assert_true(uuid_string_is_valid("{00000000-0000-0000-0000-000000000000}"));
        g_assert_true(uuid_string_is_valid("urn:uuid:00000000-0000-0000-0000-000000000000"));
        g_assert_true(uuid_string_is_valid("00000000000000000000000000000000", uuid::format::ANY_ID128));
        g_assert_true(uuid_string_is_valid("6079c6d3-ffe3-42ac-a3cf-7137b101b6ca"));
        g_assert_true(uuid_string_is_valid("{6079c6d3-ffe3-42ac-a3cf-7137b101b6ca}"));
        g_assert_true(uuid_string_is_valid("urn:uuid:6079c6d3-ffe3-42ac-a3cf-7137b101b6ca"));
        g_assert_true(uuid_string_is_valid("6079c6d3ffe342aca3cf7137b101b6ca", uuid::format::ID128));
        g_assert_true(uuid_string_is_valid("6079c6d3ffe342aca3cf7137b101b6ca", uuid::format::ANY_ID128));

        g_assert_false(uuid_string_is_valid("6079c6d3-ffe3-f2ac-a3cf-7137b101b6ca", uuid::format::SIMPLE));
        g_assert_true(uuid_string_is_valid("6079c6d3ffe342acc3cf7137b101b6ca", uuid::format::ID128));

        g_assert_false(uuid_string_is_valid("6079c6d3-ffe3-42ac-c3cf-7137b101b6ca", uuid::format::SIMPLE));
        g_assert_true(uuid_string_is_valid("6079c6d3ffe3f2aca3cf7137b101b6ca", uuid::format::ID128));

        try {
                auto u = uuid("00000001-0002-1003-8004-000000000005");
                g_assert_true(dummy_uuid == u);
        } catch (...) {
                g_assert_not_reached();
        }

        try {
                auto u = uuid("00000000-0000-0000-0000-000000000000");
                g_assert_true(nil == u);
                g_assert_true(u.is_nil());
        } catch (...) {
                g_assert_not_reached();
        }
}

static void
test_uuid_random(void)
{
        auto str = uuid_string_random();
        g_assert_cmpint(str.size(), ==, 36);
        g_assert_true(uuid_string_is_valid(str));
}

static void
test_uuid_namespace(void)
{
        auto str = uuid_namespace_dns.str();
        g_assert_cmpstr(str.c_str(), ==, "6ba7b810-9dad-11d1-80b4-00c04fd430c8");

        str = uuid_namespace_url.str();
        g_assert_cmpstr(str.c_str(), ==, "6ba7b811-9dad-11d1-80b4-00c04fd430c8");

        str = uuid_namespace_oid.str();
        g_assert_cmpstr(str.c_str(), ==, "6ba7b812-9dad-11d1-80b4-00c04fd430c8");

        str = uuid_namespace_x500.str();
        g_assert_cmpstr(str.c_str(), ==, "6ba7b814-9dad-11d1-80b4-00c04fd430c8");
}

static void
test_uuid_generate_v3(void)
{
        auto u = uuid(uuid_v3, uuid_namespace_dns, "gnome.org");
        g_assert_true(u == gnome_uuid3);
        g_assert_cmpint(u.version(), ==, 3);

        // Test vector from RFC9562
        u = uuid(uuid_v3, uuid_namespace_dns, "www.example.com");
        auto const uref = uuid("5df41881-3aed-3515-88a7-2f4a814cf09e");
        g_assert_true(u == uref);
}

static void
test_uuid_generate_v4(void)
{
        auto a = uuid(uuid_v4);
        auto b = uuid(uuid_v4);

        g_assert_false(a.is_nil());
        g_assert_false(b.is_nil());
        g_assert_false(a == b);
        g_assert_cmpint(a.version(), ==, 4);
        g_assert_cmpint(b.version(), ==, 4);
}

static void
test_uuid_generate_v5(void)
{
        auto u = uuid(uuid_v5, uuid_namespace_dns,"gnome.org");
        g_assert_true(u == gnome_uuid5);
        g_assert_cmpint(u.version(), ==, 5);

        // Test vector from RFC9562
        u = uuid(uuid_v5, uuid_namespace_dns, "www.example.com");
        auto const uref = uuid("2ed6657d-e927-568b-95e1-2665a8aea6a2");
        g_assert_true(u == uref);
}

static void
test_uuid_to_string(void)
{
        auto str = uuid_namespace_x500.str(uuid::format::SIMPLE);
        g_assert_cmpstr(str.c_str(), ==, "6ba7b814-9dad-11d1-80b4-00c04fd430c8");

        str = uuid_namespace_x500.str(uuid::format::BRACED);
        g_assert_cmpstr(str.c_str(), ==, "{6ba7b814-9dad-11d1-80b4-00c04fd430c8}");

        str = uuid_namespace_x500.str(uuid::format::URN);
        g_assert_cmpstr(str.c_str(), ==, "urn:uuid:6ba7b814-9dad-11d1-80b4-00c04fd430c8");

        str = uuid_namespace_x500.str(uuid::format::ID128);
        g_assert_cmpstr(str.c_str(), ==, "6ba7b8149dad11d180b400c04fd430c8");
}

static void
test_uuid_format(void)
{
        static constinit auto const u = VTE_DEFINE_UUID(7cb65faf, 4c02, 4593, a7cc, afc8129372b5);

        auto str = fmt::format("{}", u);
        g_assert_cmpstr(str.c_str(), ==, "7cb65faf-4c02-4593-a7cc-afc8129372b5");

        str = fmt::format("{:s}", u);
        g_assert_cmpstr(str.c_str(), ==, "7cb65faf-4c02-4593-a7cc-afc8129372b5");

        str = fmt::format("{:b}", u);
        g_assert_cmpstr(str.c_str(), ==, "{7cb65faf-4c02-4593-a7cc-afc8129372b5}");

        str = fmt::format("{:u}", u);
        g_assert_cmpstr(str.c_str(), ==, "urn:uuid:7cb65faf-4c02-4593-a7cc-afc8129372b5");

        str = fmt::format("{:i}", u);
        g_assert_cmpstr(str.c_str(), ==, "7cb65faf4c024593a7ccafc8129372b5");
}

int
main(int argc,
     char** argv)
{
        g_test_init(&argc, &argv, nullptr);

        /* uuid Tests */
        g_test_add_func("/vte/uuid/equal", test_uuid_equal);
        g_test_add_func("/vte/uuid/bytes", test_uuid_bytes);
        g_test_add_func("/vte/uuid/string", test_uuid_string);
        g_test_add_func("/vte/uuid/random", test_uuid_random);
        g_test_add_func("/vte/uuid/namespace", test_uuid_namespace);
        g_test_add_func("/vte/uuid/generate_v3", test_uuid_generate_v3);
        g_test_add_func("/vte/uuid/generate_v4", test_uuid_generate_v4);
        g_test_add_func("/vte/uuid/generate_v5", test_uuid_generate_v5);
        g_test_add_func("/vte/uuid/to-string", test_uuid_to_string);
        g_test_add_func("/vte/uuid/format", test_uuid_format);

        return g_test_run();
}
