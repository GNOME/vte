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

#include "bidi.hh"
#include "ring.hh"
#include "vterowdata.hh"
#include "vtetypes.hh"
#include "vteunistr.h"

namespace vte {

namespace base {  // FIXME ???

class BidiRow;
class BidiRunner;

/*
 * RingView provides a "view" to a continuous segment of the Ring (or stream),
 * typically the user visible area.
 *
 * It computes additional data that are needed to display the contents (or handle
 * user events such as mouse click), but not needed for the terminal emulation logic.
 * In order to save tons of resources, these data are computed when the Ring's
 * contents are about to be displayed, rather than whenever they change.
 *
 * For computing these data, context lines (outside of the specified region of the
 * Ring) are also taken into account up to the next hard newline or a safety limit.
 *
 * Currently RingView is used for BiDi: to figure out which logical character is
 * mapped to which visual position.
 *
 * Future possible uses include "highlight all" for the search match, and
 * syntax highlighting. URL autodetection might also be ported to this
 * infrastructure one day.
 */
class RingView {
        friend class BidiRunner;

public:
        RingView();
        ~RingView();

        // prevent accidents
        RingView(RingView& o) = delete;
        RingView(RingView const& o) = delete;
        RingView(RingView&& o) = delete;
        RingView& operator= (RingView& o) = delete;
        RingView& operator= (RingView const& o) = delete;
        RingView& operator= (RingView&& o) = delete;

        void set_ring(Ring *ring);
        void set_rows(vte::grid::row_t start, vte::grid::row_t len);
        void set_width(vte::grid::column_t width);
        vte::grid::column_t get_width() { return m_width; }
        void set_enable_bidi(bool enable_bidi);
        void set_enable_shaping(bool enable_shaping);

        inline void invalidate() { m_invalid = true; }
        void update();
        void pause();

        VteRowData const* get_row(vte::grid::row_t row) const;

        BidiRow const* get_bidirow(vte::grid::row_t row) const;

private:
        Ring *m_ring;

        VteRowData **m_rows;
        int m_rows_len;
        int m_rows_alloc_len;

        bool m_enable_bidi;
        bool m_enable_shaping;
        BidiRow **m_bidirows;
        int m_bidirows_alloc_len;

        BidiRunner *m_bidirunner;

        vte::grid::row_t m_top;  /* the row of the Ring corresponding to m_rows[0] */

        vte::grid::row_t m_start;
        vte::grid::row_t m_len;
        vte::grid::column_t m_width;

        bool m_invalid;
        bool m_paused;

        void resume();

        BidiRow* get_bidirow_writable(vte::grid::row_t row) const;
};

}; /* namespace base */

}; /* namespace vte */
