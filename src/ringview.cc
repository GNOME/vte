/*
 * Copyright © 2018–2019 Egmont Koblinger
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

#include <config.h>

#include "bidi.hh"
#include "debug.hh"
#include "vtedefines.hh"
#include "vteinternal.hh"

using namespace vte::base;

RingView::RingView()
{
        m_bidirunner = std::make_unique<BidiRunner>(this);
}

RingView::~RingView()
{
        pause();
}

/* Pausing a RingView frees up pretty much all of its memory.
 *
 * This is to be used when the terminal is unlikely to be painted or interacted with
 * in the near future, e.g. the widget is unmapped. Not to be called too frequently,
 * in order to avoid memory fragmentation.
 *
 * The RingView is resumed automatically on demand.
 */
void
RingView::pause()
{
        int i;

        if (m_paused)
                return;

        _vte_debug_print (vte::debug::category::RINGVIEW,
                          "Ringview: pause, freeing {} rows, {} bidirows",
                          m_rows_alloc_len,
                          m_bidirows.size());

        for (i = 0; i < m_rows_alloc_len; i++) {
                _vte_row_data_fini(m_rows[i]);
                g_free (m_rows[i]);
        }
        g_free (m_rows);
        m_rows_alloc_len = 0;

        m_bidirows.clear();
        m_bidi_lru.clear();

        m_invalid = true;
        m_paused = true;
}

/* Allocate (again) the required memory. */
void
RingView::resume()
{
        vte_assert_cmpint (m_len, >=, 1);

        /* +16: A bit of arbitrary heuristics to likely prevent a quickly following
         * realloc for the required context lines. */
        m_rows_alloc_len = m_len + 16;
        m_rows = (VteRowData **) g_malloc (sizeof (VteRowData *) * m_rows_alloc_len);
        for (int i = 0; i < m_rows_alloc_len; i++) {
                m_rows[i] = (VteRowData *) g_malloc (sizeof (VteRowData));
                _vte_row_data_init (m_rows[i]);
        }

        _vte_debug_print (vte::debug::category::RINGVIEW,
                          "Ringview: resume, allocating {} context rows",
                          m_rows_alloc_len);

        m_paused = false;
}

void
RingView::set_ring(Ring *ring)
{
        if (G_LIKELY (ring == m_ring))
                return;

        m_ring = ring;
        invalidate();
}

void
RingView::set_width(vte::grid::column_t width)
{
        if (G_LIKELY (width == m_width))
                return;

        m_width = width;
        invalidate();
}

void
RingView::set_rows(vte::grid::row_t start, vte::grid::row_t len)
{
        /* Force at least 1 row, see bug 134. */
        len = MAX(len, 1);

        if (start == m_start && len == m_len)
                return;

        /* m_rows is expanded on demand in update() */

        m_start = start;
        m_len = len;
        m_bidirows_limit = std::max(size_t{2}, size_t(m_len) * 2);
        m_bidirows.reserve(m_bidirows_limit);

        m_invalid = false;
        for (auto row = m_start; row < m_start + m_len; row++) {
                auto const iter = m_bidirows.find(row);
                if (iter == m_bidirows.end()) {
                        m_invalid = true;
                } else
                        touch_bidirow(iter);
        }

        trim_bidirows();
}

VteRowData const*
RingView::get_row(vte::grid::row_t row) const
{
        vte_assert_cmpint(row, >=, m_top);
        vte_assert_cmpint(row, <, m_top + m_rows_len);

        return m_rows[row - m_top];
}

void
RingView::set_enable_bidi(bool enable_bidi)
{
        if (G_LIKELY (enable_bidi == m_enable_bidi))
                return;

        m_enable_bidi = enable_bidi;
        invalidate();
}

void
RingView::set_enable_shaping(bool enable_shaping)
{
        if (G_LIKELY (enable_shaping == m_enable_shaping))
                return;

        m_enable_shaping = enable_shaping;
        invalidate();
}

void
RingView::invalidate()
{
        m_bidirows.clear();
        m_bidi_lru.clear();
        m_invalid = true;
}

void
RingView::invalidate_rows(vte::grid::row_t first,
                          vte::grid::row_t last)
{
        if (last < first)
                return;

        for (auto iter = m_bidirows.begin(); iter != m_bidirows.end();) {
                if (iter->first >= first && iter->first <= last) {
                        m_bidi_lru.erase(iter->second.lru);
                        iter = m_bidirows.erase(iter);
                } else {
                        ++iter;
                }
        }

        if (first < m_start + m_len && last >= m_start)
                m_invalid = true;
}

void
RingView::touch_bidirow(BidiRows::iterator iter) const
{
        m_bidi_lru.splice(m_bidi_lru.begin(), m_bidi_lru, iter->second.lru);
        iter->second.lru = m_bidi_lru.begin();
}

void
RingView::trim_bidirows()
{
        while (m_bidirows.size() > m_bidirows_limit) {
                auto const row = m_bidi_lru.back();
                m_bidirows.erase(row);
                m_bidi_lru.pop_back();
        }
}

BidiRow const*
RingView::get_bidirow(vte::grid::row_t row) const
{
        vte_assert_cmpint (row, >=, m_start);
        vte_assert_cmpint (row, <, m_start + m_len);
        vte_assert_false (m_invalid);
        vte_assert_false (m_paused);

        auto const iter = m_bidirows.find(row);
        vte_assert_true(iter != m_bidirows.end());

        return iter->second.row.get();
}

void
RingView::update()
{
        if (!m_invalid)
                return;
        if (m_paused)
                resume();

        /* Find the beginning of the topmost paragraph.
         *
         * Extract at most VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX context rows.
         * If this safety limit is reached then together with the first
         * non-context row this paragraph fragment is already longer
         * than VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX lines, and thus the
         * BiDi code will skip it. */
        vte::grid::row_t row = m_start;
        const VteRowData *row_data;

        _vte_debug_print (vte::debug::category::RINGVIEW,
                          "Ringview: updating for [{}..{}] ({} rows)",
                          m_start,
                          m_start + m_len - 1,
                          m_len);

        int i = VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX;
        while (i--) {
                if (!m_ring->is_soft_wrapped(row - 1))
                        break;
                row--;
        }

        /* Extract the data beginning at the found row.
         *
         * Extract at most VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX rows
         * beyond the end of the specified area. Again, if this safety
         * limit is reached then together with the last non-context row
         * this paragraph fragment is already longer than
         * VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX lines, and thus the
         * BiDi code will skip it. */
        m_top = row;
        m_rows_len = 0;
        while (row < m_start + m_len + VTE_RINGVIEW_PARAGRAPH_LENGTH_MAX) {
                if (G_UNLIKELY (m_rows_len == m_rows_alloc_len)) {
                        /* Don't realloc too aggressively. */
                        m_rows_alloc_len = std::max(m_rows_alloc_len + 1, m_rows_alloc_len * 5 / 4 /* whatever */);
                        _vte_debug_print (vte::debug::category::RINGVIEW,
                                          "Ringview: reallocate to {} rows",
                                          m_rows_alloc_len);
                        m_rows = (VteRowData **) g_realloc (m_rows, sizeof (VteRowData *) * m_rows_alloc_len);
                        for (int j = m_rows_len; j < m_rows_alloc_len; j++) {
                                m_rows[j] = (VteRowData *) g_malloc (sizeof (VteRowData));
                                _vte_row_data_init (m_rows[j]);
                        }
                }

                row_data = m_ring->contains(row) ? m_ring->index(row) : nullptr;
                if (G_LIKELY (row_data != nullptr)) {
                        _vte_row_data_copy (row_data, m_rows[m_rows_len]);
                        /* Make sure that the extracted data is not wider than the screen,
                         * something that can happen if the window was narrowed with rewrapping disabled.
                         * Also make sure that we won't end up with unfinished characters.
                         * FIXME remove this once bug 135 is addressed. */
                        if (G_UNLIKELY (_vte_row_data_length(m_rows[m_rows_len]) > m_width)) {
                                int j = m_width;
                                while (j > 0) {
                                        VteCell const* cell = _vte_row_data_get(m_rows[m_rows_len], j);
                                        if (!cell->attr.fragment())
                                                break;
                                        j--;
                                }
                                _vte_row_data_shrink(m_rows[m_rows_len], j);
                        }
                } else {
                        _vte_row_data_clear (m_rows[m_rows_len]);
                }
                m_rows_len++;
                row++;

                /* Once the bottom of the specified area is reached, stop at a hard newline. */
                if (row >= m_start + m_len && (!row_data || !row_data->attr.soft_wrapped))
                        break;
        }

        _vte_debug_print (vte::debug::category::RINGVIEW,
                          "Ringview: extracted {}+{} context lines: [{}..{}] ({} rows)",
                          m_start - m_top, (m_top + m_rows_len) - (m_start + m_len),
                          m_top, m_top + m_rows_len - 1, m_rows_len);

        /* Loop through paragraphs of the extracted text, and do whatever we need to do on each paragraph. */
        auto top = m_top;
        row = top;
        while (row < m_top + m_rows_len) {
                row_data = m_rows[row - m_top];
                if (!row_data->attr.soft_wrapped || row == m_top + m_rows_len - 1) {
                        /* Found a paragraph from @top to @row, inclusive. */

                        auto const visible_top = std::max(top, m_start);
                        auto const visible_bottom = std::min(row + 1, m_start + m_len);
                        auto needs_update = false;

                        for (auto visible_row = visible_top;
                             visible_row < visible_bottom;
                             visible_row++) {
                                if (m_bidirows.find(visible_row) == m_bidirows.end()) {
                                        needs_update = true;
                                        break;
                                }
                        }

                        if (needs_update)
                                m_bidirunner->paragraph(top, row + 1,
                                                        m_enable_bidi, m_enable_shaping);

                        /* Doing syntax highlighting etc. come here in the future. */

                        top = row + 1;
                }
                row++;
        }

        for (auto visible_row = m_start; visible_row < m_start + m_len; visible_row++) {
                auto const iter = m_bidirows.find(visible_row);
                vte_assert_true(iter != m_bidirows.end());
                touch_bidirow(iter);
        }

        trim_bidirows();

        m_invalid = false;
}

/* For internal use by BidiRunner. Get where the BiDi mapping for the given row
 * needs to be stored, of nullptr if it's a context row. */
BidiRow* RingView::get_bidirow_writable(vte::grid::row_t row) const
{
        if (row < m_start || row >= m_start + m_len)
                return nullptr;

        auto iter = m_bidirows.find(row);
        if (iter == m_bidirows.end()) {
                m_bidi_lru.push_front(row);
                auto const [inserted_iter, inserted] = m_bidirows.try_emplace(
                        row,
                        BidiRowEntry{std::make_unique<BidiRow>(), m_bidi_lru.begin()});
                vte_assert_true(inserted);
                iter = inserted_iter;
        } else {
                touch_bidirow(iter);
        }

        return iter->second.row.get();
}
