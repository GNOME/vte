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

class DrawingGsk : public DrawingContext {
public:
        DrawingGsk() noexcept;
        virtual ~DrawingGsk();

        DrawingGsk(DrawingGsk const&) = delete;
        DrawingGsk(DrawingGsk&&) = delete;
        DrawingGsk& operator=(DrawingGsk const&) = delete;
        DrawingGsk& operator=(DrawingGsk&&) = delete;

        void set_snapshot(GtkSnapshot *snapshot) noexcept;

        virtual cairo_t* begin_cairo(int x, int y, int width, int height) const;
        virtual void end_cairo(cairo_t *cr) const;

        /* We don't perform any clipping because we render the entire
         * scene graph and let Gsk perform the difference to determine
         * the appropriate damage area.
         */
        inline virtual void clip(Rectangle const* rect) const {}
        inline virtual void unclip() const {}

        inline virtual void clip_border(Rectangle const* rect) const {
                gtk_snapshot_push_clip(m_snapshot, rect->graphene());
        }
        inline virtual void unclip_border() const {
                gtk_snapshot_pop(m_snapshot);
        }

        inline virtual void translate(double x, double y) const {
                auto const point = GRAPHENE_POINT_INIT((float)x, (float)y);
                gtk_snapshot_save(m_snapshot);
                gtk_snapshot_translate(m_snapshot, &point);
        }
        inline virtual void untranslate() const {
                gtk_snapshot_restore(m_snapshot);
        }

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

        virtual void draw_surface_with_color_mask(GdkTexture *texture,
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
        GtkSnapshot *m_snapshot{nullptr}; // unowned
        VteGlyphs m_glyphs;

        void flush_glyph_string(PangoFont* font,
                                const GdkRGBA* rgba);

};

} // namespace view
} // namespace vte
