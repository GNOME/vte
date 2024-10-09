/*
 * Copyright © 2024 Christian Persch
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

#include <cstdint>

#include <glib.h>

#define MINIFONT_TEST

#if VTE_GTK == 3
#include "minifont-coverage-gtk3.inc"
#include "minifont-coverage-tests-gtk3.inc"
#elif VTE_GTK == 4
#include "minifont-coverage-gtk4.inc"
#include "minifont-coverage-tests-gtk4.inc"
#endif

static void
test_minifont_coverage(void)
{
        for (auto i = 0u; i < G_N_ELEMENTS(minifont_coverage); ++i) {
                g_assert_true(unistr_is_local_graphic(minifont_coverage[i]));
        }
}

int
main(int argc, char *argv[])
{
        g_test_init(&argc, &argv, nullptr);

        g_test_add_func("/vte/minifont/coverage/gtk"
#if VTE_GTK == 3
                        "3"
#elif VTE_GTK == 4
                        "4"
#endif
                        , test_minifont_coverage);

        return g_test_run();
}
