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

#pragma once

#include <pango/pangocairo.h>
#include "cairo-glue.hh"

namespace vte {

namespace image {

class Image {
private:
        // Image data, device-independent
        vte::Freeable<cairo_surface_t> m_surface{};

        // Draw/prune priority, must be unique
        size_t m_priority;

        // Image dimensions in pixels
        int m_width_pixels;
        int m_height_pixels;

        // Top left corner offset in cell units
        int m_left_cells;
        int m_top_cells;

        // Cell dimensions in pixels at time of image creation
        int m_cell_width;
        int m_cell_height;

public:
        Image(vte::Freeable<cairo_surface_t> surface,
              size_t priority,
              int width_pixels,
              int height_pixels,
              int col,
              int row,
              int cell_width,
              int cell_height) noexcept
                : m_surface{std::move(surface)},
                  m_priority{priority},
                  m_width_pixels{width_pixels},
                  m_height_pixels{height_pixels},
                  m_left_cells{col},
                  m_top_cells{row},
                  m_cell_width{cell_width},
                  m_cell_height{cell_height}
        {
        }

        ~Image() = default;

        Image(Image const&) = delete;
        Image(Image&&) = delete;
        Image operator=(Image const&) = delete;
        Image operator=(Image&&) = delete;

        inline constexpr auto get_priority() const noexcept { return m_priority; }
        inline constexpr auto get_left() const noexcept { return m_left_cells; }
        inline auto get_top() const noexcept { return m_top_cells; }
        inline void set_top(int row) noexcept { m_top_cells = row; }
        inline constexpr auto get_width() const noexcept { return (m_width_pixels + m_cell_width - 1) / m_cell_width; }
        inline constexpr auto get_height() const noexcept { return (m_height_pixels + m_cell_height - 1) / m_cell_height; }
        inline auto get_bottom() const noexcept { return m_top_cells + get_height() - 1; }

        inline auto resource_size() const noexcept
        {
                if (cairo_image_surface_get_stride(m_surface.get()) != 0)
                        return cairo_image_surface_get_stride(m_surface.get()) * m_height_pixels;

                /* Not an image surface: Only the device knows for sure, so we guess */
                return m_width_pixels * m_height_pixels * 4;
        }

        void paint(cairo_t* cr,
                   int offset_x,
                   int offset_y,
                   int cell_width,
                   int cell_height) const noexcept;

}; // class Image

} // namespace image

} // namespace vte
