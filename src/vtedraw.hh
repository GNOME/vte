/*
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright © 2020 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <memory>

#include <glib.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include "vteunistr.h"
#include "vtetypes.hh"

#define VTE_DRAW_OPAQUE (1.0)

#define VTE_DRAW_NORMAL 0
#define VTE_DRAW_BOLD   1
#define VTE_DRAW_ITALIC 2

/* A request to draw a particular character spanning a given number of columns
   at the given location.  Unlike most APIs, (x,y) specifies the top-left
   corner of the cell into which the character will be drawn instead of the
   left end of the baseline. */
struct _vte_draw_text_request {
	vteunistr c;
	gshort x, y, columns;
        guint8 mirror : 1;      /* Char has RTL resolved directionality, mirror if mirrorable. */
        guint8 box_mirror : 1;  /* Add box drawing chars to the set of mirrorable characters. */
};

struct font_info;

namespace vte {
namespace view {

class DrawingContext {
public:
        DrawingContext() noexcept = default;
        ~DrawingContext();

        DrawingContext(DrawingContext const&) = delete;
        DrawingContext(DrawingContext&&) = delete;
        DrawingContext& operator=(DrawingContext const&) = delete;
        DrawingContext& operator=(DrawingContext&&) = delete;

        void set_cairo(cairo_t* cr) noexcept;

        void clip(cairo_rectangle_int_t const* rect);
        void unclip();

        void clear(int x,
                   int y,
                   int width,
                   int height,
                   vte::color::rgb const* color,
                   double alpha);
        void clear_font_cache();
        void set_text_font(GtkWidget* widget,
                           PangoFontDescription const* fontdesc,
                           double cell_width_scale,
                           double cell_height_scale);
        void get_text_metrics(int* cell_width,
                              int* cell_height,
                              int* char_ascent,
                              int* char_descent,
                              GtkBorder* char_spacing);
        void get_char_edges(vteunistr c,
                            int columns,
                            guint style,
                            int* left,
                            int* right);
        bool has_bold(guint style);

        void draw_text(struct _vte_draw_text_request *requests,
                       gsize n_requests,
                       uint32_t attr,
                       vte::color::rgb const* color,
                       double alpha,
                       guint style);
        bool draw_char(struct _vte_draw_text_request *request,
                       uint32_t attr,
                       vte::color::rgb const* color,
                       double alpha,
                       guint style);
        bool has_char(vteunistr c,
                      guint style);
        void fill_rectangle(int x,
                            int y,
                            int width,
                            int height,
                            vte::color::rgb const* color,
                            double alpha);
        void draw_rectangle(int x,
                            int y,
                            int width,
                            int height,
                            vte::color::rgb const* color,
                            double alpha);
        void draw_line(int x,
                       int y,
                       int xp,
                       int yp,
                       int line_width,
                       vte::color::rgb const *color,
                       double alpha);

        void draw_undercurl(int x,
                            double y,
                            double line_width,
                            int count,
                            vte::color::rgb const* color,
                            double alpha);

private:
        void set_source_color_alpha (vte::color::rgb const* color,
                                     double alpha);
        void draw_graphic(vteunistr c,
                          uint32_t attr,
                          vte::color::rgb const* fg,
                          int x,
                          int y,
                          int font_width,
                          int columns,
                          int font_height);
        void draw_text_internal(struct _vte_draw_text_request *requests,
                                gsize n_requests,
                                uint32_t attr,
                                vte::color::rgb const* color,
                                double alpha,
                                guint style);

	struct font_info *m_fonts[4]{nullptr, nullptr, nullptr, nullptr};
        int m_cell_width{1};
        int m_cell_height{1};
        GtkBorder m_char_spacing{1, 1, 1, 1};

	cairo_t *m_cr{nullptr}; // unowned

        /* Cache the undercurl's rendered look. */
        std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> m_undercurl_surface{nullptr, nullptr};
};

} // namespace view
} // namespace vte

/* Create and destroy a draw structure. */

static inline void _vte_draw_set_cairo(vte::view::DrawingContext& ctx,
                                       cairo_t *cr)
{
        ctx.set_cairo(cr);
}

static inline void _vte_draw_clip(vte::view::DrawingContext& ctx,
                                  cairo_rectangle_int_t const* rect)
{
        ctx.clip(rect);
}

static inline void _vte_draw_unclip(vte::view::DrawingContext& ctx)
{
        ctx.unclip();
}

static inline void _vte_draw_clear(vte::view::DrawingContext& ctx,
                                   int x,
                                   int y,
                                   int width,
                                   int height,
                                   vte::color::rgb const* color,
                                   double alpha)
{
        ctx.clear(x, y, width, height, color, alpha);
}

static inline void _vte_draw_clear_font_cache(vte::view::DrawingContext& ctx)
{
        ctx.clear_font_cache();
}

static inline void _vte_draw_set_text_font(vte::view::DrawingContext& ctx,
                                           GtkWidget* widget,
                                           PangoFontDescription const* fontdesc,
                                           double cell_width_scale,
                                           double cell_height_scale)
{
        ctx.set_text_font(widget, fontdesc, cell_width_scale, cell_height_scale);
}

static inline void _vte_draw_get_text_metrics(vte::view::DrawingContext& ctx,
                                              int* cell_width,
                                              int* cell_height,
                                              int* char_ascent,
                                              int* char_descent,
                                              GtkBorder* char_spacing)
{
        ctx.get_text_metrics(cell_width, cell_height,
                             char_ascent, char_descent,
                             char_spacing);
}

static inline void _vte_draw_get_char_edges(vte::view::DrawingContext& ctx,
                                            vteunistr c,
                                            int columns,
                                            guint style,
                                            int* left,
                                            int* right)
{
        ctx.get_char_edges(c, columns, style, left, right);
}

static inline gboolean _vte_draw_has_bold(vte::view::DrawingContext& ctx,
                                          guint style)
{
        return ctx.has_bold(style);
}

static inline void _vte_draw_text(vte::view::DrawingContext& ctx,
                                  struct _vte_draw_text_request *requests,
                                  gsize n_requests,
                                  uint32_t attr,
                                  vte::color::rgb const* color,
                                  double alpha,
                                  guint style)
{
        ctx.draw_text(requests, n_requests, attr, color, alpha, style);
}

static inline gboolean _vte_draw_char(vte::view::DrawingContext& ctx,
                                      struct _vte_draw_text_request *request,
                                      uint32_t attr,
                                      vte::color::rgb const* color,
                                      double alpha,
                                      guint style)
{
        return ctx.draw_char(request, attr, color, alpha, style);
}

static inline gboolean _vte_draw_has_char(vte::view::DrawingContext& ctx,
                                          vteunistr c,
                                          guint style)
{
        return ctx.has_char(c, style);
}

static inline void _vte_draw_fill_rectangle(vte::view::DrawingContext& ctx,
                                            int x,
                                            int y,
                                            int width,
                                            int height,
                                            vte::color::rgb const* color,
                                            double alpha)
{
        ctx.fill_rectangle(x, y, width, height, color, alpha);
}

static inline void _vte_draw_draw_rectangle(vte::view::DrawingContext& ctx,
                                            int x,
                                            int y,
                                            int width,
                                            int height,
                                            vte::color::rgb const* color,
                                            double alpha)
{
        ctx.draw_rectangle(x, y, width, height, color, alpha);
}

static inline void _vte_draw_draw_line(vte::view::DrawingContext& ctx,
                                       int x,
                                       int y,
                                       int xp,
                                       int yp,
                                       int line_width,
                                       vte::color::rgb const *color,
                                       double alpha)
{
        ctx.draw_line(x, y, xp, yp, line_width, color, alpha);
}

static inline void _vte_draw_draw_undercurl(vte::view::DrawingContext& ctx,
                                            int x,
                                            double y,
                                            double line_width,
                                            int count,
                                            vte::color::rgb const* color,
                                            double alpha)
{
        ctx.draw_undercurl(x, y, line_width, count, color, alpha);
}

guint _vte_draw_get_style(gboolean bold, gboolean italic);

double
_vte_draw_get_undercurl_height(gint width, double line_width);

class _vte_draw_autoclip_t {
private:
        vte::view::DrawingContext& m_draw;
public:
        _vte_draw_autoclip_t(vte::view::DrawingContext& draw,
                             cairo_rectangle_int_t const* rect)
                : m_draw{draw}
        {
                m_draw.clip(rect);
        }

        ~_vte_draw_autoclip_t()
        {
                m_draw.unclip();
        }
};
