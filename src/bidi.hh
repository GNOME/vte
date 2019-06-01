/*
 * Copyright © 2018–2019 Egmont Koblinger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include <glib.h>

#include "ring.hh"
#include "ringview.hh"
#include "vterowdata.hh"
#include "vtetypes.hh"
#include "vteunistr.h"

namespace vte {

namespace base {  // FIXME ???

class RingView;

/* BidiRow contains the BiDi transformation of a single row. */
class BidiRow {
        friend class BidiRunner;

public:
        BidiRow();
        ~BidiRow();

        // prevent accidents
        BidiRow(BidiRow& o) = delete;
        BidiRow(BidiRow const& o) = delete;
        BidiRow(BidiRow&& o) = delete;
        BidiRow& operator= (BidiRow& o) = delete;
        BidiRow& operator= (BidiRow const& o) = delete;
        BidiRow& operator= (BidiRow&& o) = delete;

        vte::grid::column_t log2vis(vte::grid::column_t col) const;
        vte::grid::column_t vis2log(vte::grid::column_t col) const;
        bool log_is_rtl(vte::grid::column_t col) const;
        bool vis_is_rtl(vte::grid::column_t col) const;
        vteunistr vis_get_shaped_char(vte::grid::column_t col, vteunistr s) const;
        bool base_is_rtl() const;
        bool has_foreign() const;

private:
        void set_width(vte::grid::column_t width);

        vte::grid::column_t m_width;
        vte::grid::column_t m_width_alloc;

        vte::grid::column_t *m_log2vis;
        vte::grid::column_t *m_vis2log;
        guint8 *m_vis_rtl;
        gunichar *m_vis_shaped_char;

        guint8 m_base_rtl: 1;
        guint8 m_has_foreign: 1;
};


/* BidiRunner is not a "real" class, rather the collection of methods that run the BiDi algorithm. */
class BidiRunner {
public:
        BidiRunner(RingView *ringview);
        ~BidiRunner();

        // prevent accidents
        BidiRunner(BidiRunner& o) = delete;
        BidiRunner(BidiRunner const& o) = delete;
        BidiRunner(BidiRunner&& o) = delete;
        BidiRunner& operator= (BidiRunner& o) = delete;
        BidiRunner& operator= (BidiRunner const& o) = delete;
        BidiRunner& operator= (BidiRunner&& o) = delete;

        void paragraph(vte::grid::row_t start, vte::grid::row_t end,
                       bool do_bidi, bool do_shaping);

private:
        RingView *m_ringview;

        bool needs_shaping(vte::grid::row_t row);
#ifdef WITH_FRIBIDI
        static bool is_arabic(gunichar c);
        void explicit_line_shape(vte::grid::row_t row);
#endif

        void explicit_line(vte::grid::row_t row, bool rtl, bool do_shaping);
        void explicit_paragraph(vte::grid::row_t start, vte::grid::row_t end, bool rtl, bool do_shaping);
#ifdef WITH_FRIBIDI
        bool implicit_paragraph(vte::grid::row_t start, vte::grid::row_t end, bool do_shaping);
#endif
};

}; /* namespace base */

}; /* namespace vte */

G_BEGIN_DECLS

gboolean vte_bidi_get_mirror_char (vteunistr unistr, gboolean mirror_box_drawing, vteunistr *unistr_mirrored);

G_END_DECLS
