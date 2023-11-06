/*
 * Copyright © 2016-2020 Hayaki Saito <saitoha@me.com>
 * Copyright © 2020 Hans Petter Jansson <hpj@cl.no>
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

#include <glib.h>
#include <stdio.h>
#include "vteinternal.hh"

#include "image.hh"

namespace vte {

namespace image {

/* Paint the image with provided cairo context */
void
Image::paint(cairo_t* cr,
             int offset_x,
             int offset_y,
             int cell_width,
             int cell_height) const noexcept
{
        auto scale_x = 1.0;
        auto scale_y = 1.0;

        auto real_offset_x = double(offset_x);
        auto real_offset_y = double(offset_y);

        if (cell_width != m_cell_width || cell_height != m_cell_height) {
                scale_x = cell_width / (double) m_cell_width;
                scale_y = cell_height / (double) m_cell_height;

                real_offset_x /= scale_x;
                real_offset_y /= scale_y;
        }

        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

        if (!(_vte_double_equal (scale_x, 1.0) && _vte_double_equal (scale_y, 1.0)))
                cairo_scale (cr, scale_x, scale_y);

        cairo_rectangle(cr, real_offset_x, real_offset_y, m_width_pixels, m_height_pixels);
        cairo_clip(cr);
        cairo_set_source_surface(cr, m_surface.get(), real_offset_x, real_offset_y);
        cairo_paint(cr);
        cairo_restore(cr);
}

} // namespace image

} // namespace vte
