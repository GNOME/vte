/*
 * Copyright © 2026 Christian Hergert
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

#include <algorithm>

#include "render-cache.hh"

namespace vte::view {

RenderCache::Rows::iterator
RenderCache::find(void const* screen,
                  vte::grid::row_t row) noexcept
{
        return m_rows.find(Key{screen, row});
}

RenderCache::Row&
RenderCache::row(void const* screen,
                 vte::grid::row_t row)
{
        auto iter = find(screen, row);

        if (iter == m_rows.end()) {
                auto const key = Key{screen, row};
                m_lru.push_front(key);
                auto const [inserted_iter, inserted] = m_rows.try_emplace(key);
                g_assert(inserted);
                iter = inserted_iter;
                iter->second.row.screen = screen;
                iter->second.row.row = row;
                iter->second.lru = m_lru.begin();
                trim();
        } else if (iter->second.lru != m_lru.begin()) {
                m_lru.splice(m_lru.begin(), m_lru, iter->second.lru);
                iter->second.lru = m_lru.begin();
        }

        return iter->second.row;
}

void
RenderCache::invalidate(void const* screen,
                        vte::grid::row_t first,
                        vte::grid::row_t last,
                        RenderInvalidation parts)
{
        auto matched = false;

        if (last < first || parts == RenderInvalidation::eNONE)
                return;

        for (auto& [key, cached] : m_rows) {
                auto& entry = cached.row;

                if (entry.screen != screen || entry.row < first || entry.row > last)
                        continue;

                matched = true;

                if (parts & RenderInvalidation::eBACKGROUND)
                        entry.background_dirty = true;

                if (parts & RenderInvalidation::eFOREGROUND)
                        entry.foreground_dirty = true;
        }

        if (matched)
                invalidate_viewports();
}

void
RenderCache::clear() noexcept
{
        invalidate_viewports();
        m_rows.clear();
        m_lru.clear();
}

bool
RenderCache::contains(void const* screen,
                      vte::grid::row_t row) const noexcept
{
        return m_rows.contains(Key{screen, row});
}

void
RenderCache::set_row_limit(size_t limit)
{
        limit = std::max(size_t{2}, limit);
        if (limit == m_row_limit)
                return;

        m_row_limit = limit;
        m_rows.reserve(m_row_limit);
        trim();
}

void
RenderCache::trim()
{
        while (m_rows.size() > m_row_limit) {
                m_rows.erase(m_lru.back());
                m_lru.pop_back();
        }
}

GskRenderNode*
RenderCache::viewport(void const* screen,
                      vte::grid::row_t first,
                      vte::grid::row_t last,
                      int start_y,
                      bool blink) const noexcept
{
        auto const& viewport = m_viewports[blink ? 1 : 0];

        if (viewport.screen != screen ||
            viewport.first != first ||
            viewport.last != last ||
            viewport.start_y != start_y)
                return nullptr;

        return viewport.node.get();
}

void
RenderCache::set_viewport(void const* screen,
                          vte::grid::row_t first,
                          vte::grid::row_t last,
                          int start_y,
                          bool blink,
                          RenderNodePtr node)
{
        auto& viewport = m_viewports[blink ? 1 : 0];

        viewport.screen = screen;
        viewport.first = first;
        viewport.last = last;
        viewport.start_y = start_y;
        viewport.node = std::move(node);
}

void
RenderCache::invalidate_viewports() noexcept
{
        for (auto& viewport : m_viewports) {
                viewport.screen = nullptr;
                viewport.node.reset();
        }
}

} // namespace vte::view
