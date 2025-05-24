//
//  Copyright © 2025 Tau Gärtli
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 3 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "config.h"
#include <glib.h>
#include "color-lightness.hh"

using namespace vte::color;

static void
test_white(void)
{
        auto const white = rgb{G_MAXUINT16, G_MAXUINT16, G_MAXUINT16};
        g_assert_cmpfloat_with_epsilon(perceived_lightness(white), 1., FLT_TRUE_MIN);
}

static void
test_black(void)
{
        auto const black = rgb{0, 0, 0};
        g_assert_cmpfloat_with_epsilon(perceived_lightness(black), 0., FLT_TRUE_MIN);
}

static auto assert_is_dark(rgb const& color) -> void
{
        g_assert_cmpfloat(perceived_lightness(color), <=, 0.5);
}

static void
test_dark_colors(void)
{
        auto const dark_gray = rgb{0x1d1d, 0x1d1d, 0x1d1d};
        assert_is_dark(dark_gray);
        auto const solarized_dark_bg = rgb{0x0000, 0x2b2b, 0x3636};
        assert_is_dark(solarized_dark_bg);
        auto const dark_green = rgb{0x1f1f, 0x2d2d, 0x3a3a};
        assert_is_dark(dark_green);
        auto const borland_blue = rgb{0x0000, 0x0000, 0xa4a4};
        assert_is_dark(borland_blue);
        auto const fairy_floss_bg = rgb{0x5a5a, 0x5454, 0x7575};
        assert_is_dark(fairy_floss_bg);
        auto const grass_green = rgb{0x1313, 0x7777, 0x3d3d};
        assert_is_dark(grass_green);
}

static auto assert_is_light(rgb const& color) -> void
{
        g_assert_cmpfloat(perceived_lightness(color), >=, 0.5);
}

static void
test_light_colors(void)
{
        auto const light_gray = rgb{0xcfcf, 0xcfcf, 0xcfcf};
        assert_is_light(light_gray);
        // Background color of the Horizon theme.
        auto const rose = rgb{0xfdfd, 0xf0f0, 0xeded};
        assert_is_light(rose);
        auto const solarized_light_bg = rgb{0xfdfd, 0xf6f6, 0xe3e3};
        assert_is_light(solarized_light_bg);
        auto const belafonte_bg = rgb{0xd5d5, 0xcccc, 0xbaba};
        assert_is_light(belafonte_bg);
}

auto main(int argc, char *argv[]) -> int
{
        g_test_init (&argc, &argv, nullptr);

        g_test_add_func("/vte/color-lightness/white", test_white);
        g_test_add_func("/vte/color-lightness/black", test_black);
        g_test_add_func("/vte/color-lightness/dark-colors", test_dark_colors);
        g_test_add_func("/vte/color-lightness/light-colors", test_light_colors);

        return g_test_run();
}
