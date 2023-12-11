/*
 * Copyright © 2023 Christian Hergert
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

#include <gtk/gtk.h>

#include "drawing-context.hh"

#define GDK_ARRAY_NAME vte_glyphs
#define GDK_ARRAY_TYPE_NAME VteGlyphs
#define GDK_ARRAY_ELEMENT_TYPE PangoGlyphInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 128
#define GDK_ARRAY_NO_MEMSET
#include "gdkarrayimpl.c"

namespace vte {
namespace view {

class DrawingGsk final : public DrawingContext {
public:
        DrawingGsk() noexcept;
        ~DrawingGsk() override;

        DrawingGsk(DrawingGsk const&) = delete;
        DrawingGsk(DrawingGsk&&) = delete;
        DrawingGsk& operator=(DrawingGsk const&) = delete;
        DrawingGsk& operator=(DrawingGsk&&) = delete;

        void set_snapshot(GtkSnapshot *snapshot) noexcept;

        cairo_t* begin_cairo(int x,
                             int y,
                             int width,
                             int height) const override;
        void end_cairo(cairo_t *cr) const override;

        /* We don't perform any clipping because we render the entire
         * scene graph and let Gsk perform the difference to determine
         * the appropriate damage area.
         */
        inline void clip(Rectangle const* rect) const override { }
        inline void unclip() const override { }

        inline void clip_border(Rectangle const* rect) const override
        {
                gtk_snapshot_push_clip(m_snapshot, rect->graphene());
        }
        inline void unclip_border() const override
        {
                gtk_snapshot_pop(m_snapshot);
        }

        inline void translate(double x,
                              double y) const override
        {
                auto const point = GRAPHENE_POINT_INIT((float)x, (float)y);
                gtk_snapshot_save(m_snapshot);
                gtk_snapshot_translate(m_snapshot, &point);
        }
        inline void untranslate() const override
        {
                gtk_snapshot_restore(m_snapshot);
        }

        void clear(int x,
                   int y,
                   int width,
                   int height,
                   vte::color::rgb const* color,
                   double alpha) const override;
        void fill_rectangle(int x,
                            int y,
                            int width,
                            int height,
                            vte::color::rgb const* color) const override;
        void fill_rectangle(int x,
                            int y,
                            int width,
                            int height,
                            vte::color::rgb const* color,
                            double alpha) const override;
        void draw_rectangle(int x,
                            int y,
                            int width,
                            int height,
                            vte::color::rgb const* color) const override;

        void draw_surface_with_color_mask(GdkTexture *texture,
                                          int x,
                                          int y,
                                          int width,
                                          int height,
                                          vte::color::rgb const* color) const override;

protected:
        void draw_text_internal(TextRequest* requests,
                                gsize n_requests,
                                uint32_t attr,
                                vte::color::rgb const* color) override;

private:
        GtkSnapshot *m_snapshot{nullptr}; // unowned
        VteGlyphs m_glyphs;

        void flush_glyph_string(PangoFont* font,
                                const GdkRGBA* rgba);

};

} // namespace view
} // namespace vte
