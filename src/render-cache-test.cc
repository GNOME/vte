/*
 * Copyright © 2026 Christian Hergert
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

#include "config.h"

#include "render-cache.hh"

using namespace vte::view;

static RenderNodePtr
make_node()
{
        auto const color = GdkRGBA{1, 0, 0, 1};
        auto const bounds = GRAPHENE_RECT_INIT(0, 0, 1, 1);

        return RenderNodePtr{gsk_color_node_new(&color, &bounds)};
}

static void
test_identity_and_invalidation()
{
        RenderCache cache;
        int first_screen = 0;
        int second_screen = 0;

        auto& first = cache.row(&first_screen, 7);
        first.background_dirty = false;
        first.foreground_dirty = false;

        auto& same = cache.row(&first_screen, 7);
        g_assert_true(&same == &first);

        auto& other = cache.row(&second_screen, 7);
        g_assert_true(&other != &first);
        g_assert_cmpuint(cache.size(), ==, 2);
        other.background_dirty = false;
        other.foreground_dirty = false;

        cache.invalidate(&first_screen, 7, 7, RenderInvalidation::eFOREGROUND);
        g_assert_false(first.background_dirty);
        g_assert_true(first.foreground_dirty);
        g_assert_false(other.background_dirty);
        g_assert_false(other.foreground_dirty);

        first.foreground_dirty = false;
        cache.invalidate(&first_screen, 7, 7, RenderInvalidation::eBACKGROUND);
        g_assert_true(first.background_dirty);
        g_assert_false(first.foreground_dirty);
}

static void
test_lru_and_resize()
{
        RenderCache cache;
        int screen = 0;

        cache.set_row_limit(4);
        for (int row = 0; row < 4; row++)
                cache.row(&screen, row);

        cache.row(&screen, 0);
        cache.row(&screen, 4);

        g_assert_true(cache.contains(&screen, 0));
        g_assert_false(cache.contains(&screen, 1));
        g_assert_cmpuint(cache.size(), ==, 4);

        cache.set_row_limit(2);
        g_assert_cmpuint(cache.size(), ==, 2);

        cache.clear();
        g_assert_cmpuint(cache.size(), ==, 0);
}

static void
test_viewports()
{
        RenderCache cache;
        int screen = 0;

        auto node = make_node();
        auto const saved = node.get();
        cache.set_viewport(&screen, 10, 20, -3, true, std::move(node));

        g_assert_true(cache.viewport(&screen, 10, 20, -3, true) == saved);
        g_assert_null(cache.viewport(&screen, 10, 20, -3, false));
        g_assert_null(cache.viewport(&screen, 11, 20, -3, true));

        cache.invalidate(&screen, 30, 40, RenderInvalidation::eBACKGROUND);
        g_assert_true(cache.viewport(&screen, 10, 20, -3, true) == saved);

        cache.row(&screen, 10);
        cache.invalidate(&screen, 10, 10, RenderInvalidation::eBACKGROUND);
        g_assert_null(cache.viewport(&screen, 10, 20, -3, true));
}

int
main(int argc,
     char** argv)
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/render-cache/identity-invalidation", test_identity_and_invalidation);
        g_test_add_func("/vte/render-cache/lru-resize", test_lru_and_resize);
        g_test_add_func("/vte/render-cache/viewports", test_viewports);

        return g_test_run();
}
