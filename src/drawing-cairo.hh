/*
 * Copyright (C) 2003 Red Hat, Inc.
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

#pragma once

#include "drawing-context.hh"

namespace vte {
namespace view {

class DrawingCairo : public DrawingContext {
public:
        DrawingCairo() noexcept = default;
        ~DrawingCairo() noexcept = default;

        DrawingCairo(DrawingCairo const&) = delete;
        DrawingCairo(DrawingCairo&&) = delete;
        DrawingCairo& operator=(DrawingCairo const&) = delete;
        DrawingCairo& operator=(DrawingCairo&&) = delete;

        virtual cairo_t* begin_cairo(int x, int y, int width, int height) const;
        virtual void end_cairo(cairo_t *cr) const;

        virtual void clip(Rectangle const* rect) const;
        virtual void unclip() const;

        virtual void translate(double x, double y) const;
        virtual void untranslate() const;

        virtual void clear(int x,
                           int y,
                           int width,
                           int height,
                           vte::color::rgb const* color,
                           double alpha) const;
        virtual void fill_rectangle(int x,
                                    int y,
                                    int width,
                                    int height,
                                    vte::color::rgb const* color,
                                    double alpha) const;
        virtual void draw_rectangle(int x,
                                    int y,
                                    int width,
                                    int height,
                                    vte::color::rgb const* color,
                                    double alpha) const;

        void set_cairo(cairo_t* cr) noexcept;

        virtual void draw_surface_with_color_mask(cairo_surface_t *surface,
                                                  int x,
                                                  int y,
                                                  int width,
                                                  int height,
                                                  vte::color::rgb const* color) const;

protected:
        virtual void draw_text_internal(TextRequest* requests,
                                        gsize n_requests,
                                        uint32_t attr,
                                        vte::color::rgb const* color,
                                        double alpha);

private:
        cairo_t *m_cr{nullptr}; // unowned
};

} // namespace view
} // namespace vte
