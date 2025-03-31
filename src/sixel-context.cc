/*
 * Copyright © 2020 Christian Persch
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

#include "sixel-context.hh"

#include <algorithm>
#include <cmath>
#include <cstdint>

#if VTE_DEBUG
#include "debug.hh"
#include "libc-glue.hh"
#endif

namespace vte::sixel {

/* BEGIN */

/* The following code is copied from xterm/graphics.c where it is under the
 * licence below; and modified and used here under the GNU Lesser General Public
 * Licence, version 3 (or, at your option), any later version.
 */

/*
 * Copyright 2013-2019,2020 by Ross Combs
 * Copyright 2013-2019,2020 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

/*
 * Context::make_color_hls:
 * @h: hue
 * @l: luminosity
 * @s: saturation
 *
 * Returns the colour specified by (h, l, s) as RGB, 8 bit per component.
 *
 * Primary color hues are blue: 0 degrees, red: 120 degrees, and green: 240 degrees.
 */
Context::color_t
Context::make_color_hls(int h,
                        int l,
                        int s) noexcept
{
        auto const c2p = std::abs(2 * l - 100);
        auto const cp = ((100 - c2p) * s) << 1;
        auto const hs = ((h + 240) / 60) % 6;
        auto const xp = (hs & 1) ? cp : 0;
        auto const mp = 200 * l - (cp >> 1);

        int r1p, g1p, b1p;
        switch (hs) {
        case 0:
                r1p = cp;
                g1p = xp;
                b1p = 0;
                break;
        case 1:
                r1p = xp;
                g1p = cp;
                b1p = 0;
                break;
        case 2:
                r1p = 0;
                g1p = cp;
                b1p = xp;
                break;
        case 3:
                r1p = 0;
                g1p = xp;
                b1p = cp;
                break;
        case 4:
                r1p = xp;
                g1p = 0;
                b1p = cp;
                break;
        case 5:
                r1p = cp;
                g1p = 0;
                b1p = xp;
                break;
        default:
                __builtin_unreachable();
        }

        auto const r = ((r1p + mp) * 255 + 10000) / 20000;
        auto const g = ((g1p + mp) * 255 + 10000) / 20000;
        auto const b = ((b1p + mp) * 255 + 10000) / 20000;

        return make_color(r, g, b);
}

/* END */

/* This is called when resetting the Terminal which is currently using
 * DataSyntax::DECSIXEL syntax. Clean up buffers, but don't reset colours
 * etc since they will be re-initialised anyway when the context is
 * used the next time.
 */
void
Context::reset() noexcept
{
        /* Keep buffer of default size */
        if (m_scanlines_data_capacity > minimum_capacity()) {
                m_scanlines_data.reset();
                m_scanlines_data_capacity = 0;
        }

        m_scanline_begin = m_scanline_pos = m_scanline_end = nullptr;
}

/*
 * Ensure that the scanlines buffer has space for the image (as specified
 * by the raster and actual dimensions) and at least one full k_max_width
 * scanline.
 *
 * The scanline offsets must be up-to-date before calling this function.
 *
 * On success, m_scanline_begin and m_scanline_pos will point to the start
 * of the current scanline (that is, m_scanline_data + *m_scanlines_offsets_pos),
 * and m_scanline_end will point to the end of the scanline of k_max_width sixels,
 * and %true returned.
 *
 * On failure, all of m_scanline_begin/pos/end will be set to nullptr, and
 * %false returned.
 */
 bool
 Context::ensure_scanlines_capacity() noexcept
 {
         auto const width = std::max(m_raster_width, m_width);
         auto const height = std::max(m_raster_height, m_height);

         /* This is guaranteed not to overflow since width and height
          * are limited by k_max_{width,height}.
          */
         auto const needed_capacity = capacity(width, height);
         auto const old_capacity = m_scanlines_data_capacity;

         if (needed_capacity <= old_capacity)
                 return true;

         /* Not enought space, so we need to enlarge the buffer. Don't
          * overallocate, but also don't reallocate too often; so try
          * doubling but use an upper limit.
          */
         auto const new_capacity = std::min(std::max({minimum_capacity(),
                                                      needed_capacity,
                                                      old_capacity * 2}),
                 capacity(k_max_width, k_max_height));

         m_scanlines_data = vte::glib::take_free_ptr(reinterpret_cast<color_index_t*>(g_try_realloc_n(m_scanlines_data.release(),
                                                                                                      new_capacity,
                                                                                                      sizeof(color_index_t))));
         if (!m_scanlines_data) {
                 m_scanlines_data_capacity = 0;
                 m_scanline_pos = m_scanline_begin = m_scanline_end = nullptr;
                 return false;
         }

         /* Clear newly allocated capacity */
         std::memset(m_scanlines_data.get() + old_capacity, 0,
                     (new_capacity - old_capacity) * sizeof(*m_scanlines_data.get()));

         m_scanlines_data_capacity = new_capacity;

         /* Relocate the buffer pointers. The update_scanline_offsets() above
          * made sure that m_scanlines_offsets is up to date.
          */
         auto const old_scanline_pos = m_scanline_pos - m_scanline_begin;
         m_scanline_begin = m_scanlines_data.get() + m_scanlines_offsets_pos[0];
         m_scanline_end = m_scanlines_data.get() + m_scanlines_offsets_pos[1];
         m_scanline_pos = m_scanline_begin + old_scanline_pos;

         assert(m_scanline_begin <= scanlines_data_end());
         assert(m_scanline_pos <= scanlines_data_end());
         assert(m_scanline_end <= scanlines_data_end());

         return true;
}

void
Context::reset_colors() noexcept
{
        /* DECPPLV2 says that on startup, and after DECSTR, DECSCL and RIS,
         * all colours are assigned to Black, *not* to a palette.
         * Instead, it says that devices may have 8- or 16-colour palettes,
         * and which HLS and RGB values used in DECGCI will result in which
         * of these 8 or 64 colours being actually used.
         *
         * It also says that between DECSIXEL invocations, colour registers
         * are preserved; in xterm, whether colours are kept or cleared,
         * is controlled by the XTERM_SIXEL_PRIVATE_COLOR_REGISTERS private
         * mode.
         */

        /* Background fill colour, fully transparent by default */
        m_colors[0] = 0u;

        /* This is the VT340 default colour palette of 16 colours.
         * PPLV2 defines 8- and 64-colour palettes; not sure
         * why everyone seems to use the VT340 one?
         *
         * Colours 9..14 (name marked with '*') are less saturated
         * versions of colours 1..6.
         */
        m_colors[0 + 2]  = make_color_rgb( 0,  0,  0); /* HLS(  0,  0,  0) */ /* Black    */
        m_colors[1 + 2]  = make_color_rgb(20, 20, 80); /* HLS(  0, 50, 60) */ /* Blue     */
        m_colors[2 + 2]  = make_color_rgb(80, 13, 13); /* HLS(120, 46, 72) */ /* Red      */
        m_colors[3 + 2]  = make_color_rgb(20, 80, 20); /* HLS(240, 50, 60) */ /* Green    */
        m_colors[4 + 2]  = make_color_rgb(80, 20, 80); /* HLS( 60, 50, 60) */ /* Magenta  */
        m_colors[5 + 2]  = make_color_rgb(20, 80, 80); /* HLS(300, 50, 60) */ /* Cyan     */
        m_colors[6 + 2]  = make_color_rgb(80, 80, 20); /* HLS(180, 50, 60) */ /* Yellow   */
        m_colors[7 + 2]  = make_color_rgb(53, 53, 53); /* HLS(  0, 53,  0) */ /* Grey 50% */
        m_colors[8 + 2]  = make_color_rgb(26, 26, 26); /* HLS(  0, 26,  0) */ /* Grey 25% */
        m_colors[9 + 2]  = make_color_rgb(33, 33, 60); /* HLS(  0, 46, 29) */ /* Blue*    */
        m_colors[10 + 2] = make_color_rgb(60, 26, 26); /* HLS(120, 43, 39) */ /* Red*     */
        m_colors[11 + 2] = make_color_rgb(33, 60, 33); /* HLS(240, 46, 29) */ /* Green*   */
        m_colors[12 + 2] = make_color_rgb(60, 33, 60); /* HLS( 60, 46, 29) */ /* Magenta* */
        m_colors[13 + 2] = make_color_rgb(33, 60, 60); /* HLS(300, 46, 29) */ /* Cyan*    */
        m_colors[14 + 2] = make_color_rgb(60, 60, 33); /* HLS(180, 46, 29) */ /* Yellow*  */
        m_colors[15 + 2] = make_color_rgb(80, 80, 80); /* HLS(  0, 80,  0) */ /* Grey 75% */

        /* Devices may use the same colour palette for DECSIXEL as for
         * text mode, so initialise colours 16..255 to the standard 256-colour
         * palette. I haven't seen any documentation from DEC that says
         * this is what they actually did, but this is what all the libsixel
         * related terminal emulator patches did, so let's copy that. Except
         * that they use a variant of the 666 colour cube which
         * uses make_color_rgb(r * 51, g * 51, b * 51) instead of the formula
         * below which is the same as for the text 256-colour palette's 666
         * colour cube, and make_color_rgb(i * 11, i * 11, i * 11) instead of
         * the formula below which is the same as for the text 256-colour palette
         * greyscale ramp.
         */
        /* 666-colour cube */
        auto make_cube_color = [&](unsigned r,
                                   unsigned g,
                                   unsigned b) constexpr noexcept -> auto
        {
                return make_color(r ? r * 40u + 55u : 0,
                                  g ? g * 40u + 55u : 0,
                                  b ? b * 40u + 55u : 0);
        };

        for (auto n = 0; n < 216; ++n)
                m_colors[n + 16 + 2] = make_cube_color(n / 36, (n / 6) % 6, n % 6);

        /* 24-colour greyscale ramp */
        for (auto n = 0; n < 24; ++n)
                m_colors[n + 16 + 216 + 2] = make_color(8 + n * 10, 8 + n * 10, 8 + n * 10);

        /* Set all other colours to black */
        for (auto n = 256 + 2; n < k_num_colors + 2; ++n)
                m_colors[n] = make_color(0, 0, 0);
}

void
Context::prepare(int id,
                 uint32_t introducer,
                 unsigned fg_red,
                 unsigned fg_green,
                 unsigned fg_blue,
                 unsigned bg_red,
                 unsigned bg_green,
                 unsigned bg_blue,
                 bool bg_transparent,
                 bool private_color_registers,
                 double pixel_aspect) noexcept
{
        m_id = id;
        m_introducer = introducer;
        m_st = 0;
        m_width = m_height = 0;
        m_raster_width = m_raster_height = 0;

        if (private_color_registers)
                reset_colors();

        if (bg_transparent)
                m_colors[0] = 0u; /* fully transparent */
        else
                m_colors[0] = make_color(bg_red, bg_green, bg_blue);

        m_colors[1] = make_color(fg_red, fg_green, fg_blue);

        /*
         * DEC PPLV2 says that on entering DECSIXEL mode, the active colour
         * is set to colour register 0. Xterm defaults to register 3.
         * We use the current foreground color in our special register 1.
         */
        set_current_color(1);

        /* Clear buffer and scanline offsets */
        std::memset(m_scanlines_offsets, 0, sizeof(m_scanlines_offsets));

        if (m_scanlines_data)
                std::memset(m_scanlines_data.get(), 0,
                            m_scanlines_data_capacity * sizeof(color_index_t));

        m_scanlines_offsets_pos = scanlines_offsets_begin();
        m_scanlines_offsets[0] = 0;

        ensure_scanline();
}

template<typename C,
         typename P>
inline C*
Context::image_data(size_t* size,
                    unsigned stride,
                    P pen) noexcept
{
        auto const height = image_height();
        auto const width = image_width();
        if (height == 0 || width == 0 || !m_scanlines_data)
                return nullptr;

        if (size)
                *size = height * stride;

        auto wdata = vte::glib::take_free_ptr(reinterpret_cast<C*>(g_try_malloc_n(height, stride)));
        if (!wdata)
                return nullptr;

        /* FIXMEchpe: this can surely be optimised, perhaps using SIMD, and
         * being more cache-friendly.
         */

        assert((stride % sizeof(C)) == 0);
        auto wstride = stride / sizeof(C);
        assert(wstride >= width);
        // auto wdata_end = wdata + wstride * height;

        /* There may be one scanline at the bottom that extends below the image's height,
         * and needs to be handled specially. First convert all the full scanlines, then
         * the last partial one.
         */
        auto scanlines_offsets_pos = scanlines_offsets_begin();
        auto wdata_pos = wdata.get();
        auto y = 0u;
        for (;
             (scanlines_offsets_pos + 1) < scanlines_offsets_end() && (y + 6) <= height;
             ++scanlines_offsets_pos, wdata_pos += 6 * wstride, y += 6) {
                auto const scanline_begin = m_scanlines_data.get() + scanlines_offsets_pos[0];
                auto const scanline_end = m_scanlines_data.get() + scanlines_offsets_pos[1];
                auto x = 0u;
                for (auto scanline_pos = scanline_begin; scanline_pos < scanline_end; ++x) {
                        for (auto n = 0; n < 6; ++n) {
                                wdata_pos[n * wstride + x] = pen(*scanline_pos++);
                        }
                }

                /* Clear leftover space */
                if (x < wstride) {
                        auto const bg = pen(0);
                        for (auto n = 0; n < 6; ++n) {
                                std::fill(&wdata_pos[n * wstride + x],
                                          &wdata_pos[(n + 1) * wstride],
                                          bg);
                        }
                }
        }

        if (y < height && (y + 6) > height &&
            (scanlines_offsets_pos + 1) < scanlines_offsets_end()) {
                auto const h = height - y;
                auto const scanline_begin = m_scanlines_data.get() + scanlines_offsets_pos[0];
                auto const scanline_end = m_scanlines_data.get() + scanlines_offsets_pos[1];
                auto x = 0u;
                for (auto scanline_pos = scanline_begin; scanline_pos < scanline_end; ++x) {
                        for (auto n = 0u; n < h; ++n) {
                                wdata_pos[n * wstride + x] = pen(*scanline_pos++);
                        }

                        scanline_pos += 6 - h;
                }

                /* Clear leftover space */
                if (x < wstride) {
                        auto const bg = pen(0);
                        for (auto n = 0u; n < h; ++n) {
                                std::fill(&wdata_pos[n * wstride + x],
                                          &wdata_pos[(n + 1) * wstride],
                                          bg);
                        }
                }
        }

        /* We drop the scanlines buffer here if it's bigger than the default buffer size,
         * so that parsing a big image doesn't retain the large buffer forever.
         */
        if (m_scanlines_data_capacity > minimum_capacity()) {
                m_scanlines_data.reset();
                m_scanlines_data_capacity = 0;
        }

        return wdata.release();
}

// This is only used in the test suite
Context::color_index_t*
Context::image_data_indexed(size_t* size,
                            unsigned extra_width_stride) noexcept
{
        return image_data<color_index_t>(size,
                                         (image_width() + extra_width_stride) * sizeof(color_index_t),
                                         [](color_index_t pen) constexpr noexcept -> color_index_t { return pen; });
}

#ifdef VTE_COMPILATION

uint8_t*
Context::image_data() noexcept
{
        return reinterpret_cast<uint8_t*>(image_data<color_t>(nullptr,
                                                              cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, image_width()),
                                                              [&](color_index_t pen) constexpr noexcept -> color_t { return m_colors[pen]; }));
}

vte::Freeable<cairo_surface_t>
Context::image_cairo() noexcept
{
        static cairo_user_data_key_t s_data_key;

        auto data = image_data();
        if (!data)
                return nullptr;

        auto const stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, image_width());
        auto surface = vte::take_freeable(cairo_image_surface_create_for_data(data,
                                                                              CAIRO_FORMAT_ARGB32,
                                                                              image_width(),
                                                                              image_height(),
                                                                              stride));

#if VTE_DEBUG
        _VTE_DEBUG_IF(vte::debug::category::IMAGE) {
                static auto num = 0;

                auto tmpl = fmt::format("vte-image-sixel-{:05}-XXXXXX.png", ++num);
                auto err = vte::glib::Error{};
                char* path = nullptr;
                auto fd = vte::libc::FD{g_file_open_tmp(tmpl.c_str(), &path, err)};
                if (fd) {
                        auto rv = cairo_surface_write_to_png(surface.get(), path);
                        if (rv == CAIRO_STATUS_SUCCESS)
                                vte::debug::println("SIXEL Image written to \"{}\"",
                                                    path);
                        else
                                vte::debug::println("Failed to write SIXEL image to \"{}\": {}",
                                                    path, unsigned(rv));
                } else {
                        vte::debug::println("Failed to create tempfile for SIXEL image: {}",
                                            err.message());
                }
                g_free(path);
        }
#endif /* VTE_DEBUG */

        if (cairo_surface_set_user_data(surface.get(),
                                        &s_data_key,
                                        data,
                                        (cairo_destroy_func_t)&g_free) != CAIRO_STATUS_SUCCESS) {
                /* When this fails, it's not documented whether the destroy func
                 * will have been called; reading cairo code, it appears it is *not*.
                 */
                cairo_surface_finish(surface.get()); // drop data buffer
                g_free(data);

                return nullptr;
        }

        return surface;
}

#endif /* VTE_COMPILATION */

} // namespace vte::sixel
