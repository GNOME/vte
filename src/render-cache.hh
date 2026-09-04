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

#pragma once

#include <array>
#include <list>
#include <memory>
#include <unordered_map>

#include <gtk/gtk.h>

#include "refptr.hh"
#include "vtetypes.hh"

namespace vte::view {

struct RenderNodeUnref {
        void operator()(GskRenderNode* node) const noexcept
        {
                if (node)
                        gsk_render_node_unref(node);
        }
};

struct BytesUnref {
        void operator()(GBytes* bytes) const noexcept
        {
                if (bytes)
                        g_bytes_unref(bytes);
        }
};

using RenderNodePtr = std::unique_ptr<GskRenderNode, RenderNodeUnref>;
using BytesPtr = std::unique_ptr<GBytes, BytesUnref>;

enum class RenderInvalidation : unsigned {
        eNONE       = 0,
        eBACKGROUND = 1u << 0,
        eFOREGROUND = 1u << 1,
        eBOTH       = 3,
};

constexpr RenderInvalidation
operator|(RenderInvalidation left,
          RenderInvalidation right) noexcept
{
        return RenderInvalidation(unsigned(left) | unsigned(right));
}

constexpr bool
operator&(RenderInvalidation left,
          RenderInvalidation right) noexcept
{
        return (unsigned(left) & unsigned(right)) != 0;
}

class RenderCache final {
public:
        struct Row {
                void const* screen{};
                vte::grid::row_t row{};

                RenderNodePtr background{};
                std::array<RenderNodePtr, 2> foreground{};
                vte::glib::RefPtr<GdkTexture> background_texture{};
                BytesPtr background_bytes{};

                bool background_dirty{true};
                bool foreground_dirty{true};
                std::array<bool, 2> foreground_built{};
                bool has_blink{false};
        };

        RenderCache() = default;
        ~RenderCache() = default;

        RenderCache(RenderCache const&) = delete;
        RenderCache(RenderCache&&) = delete;
        RenderCache& operator=(RenderCache const&) = delete;
        RenderCache& operator=(RenderCache&&) = delete;

        Row& row(void const* screen,
                 vte::grid::row_t row);

        void invalidate(void const* screen,
                        vte::grid::row_t first,
                        vte::grid::row_t last,
                        RenderInvalidation parts);
        void clear() noexcept;
        void set_row_limit(size_t limit);
        size_t size() const noexcept { return m_rows.size(); }
        bool contains(void const* screen,
                      vte::grid::row_t row) const noexcept;

        GskRenderNode* viewport(void const* screen,
                                vte::grid::row_t first,
                                vte::grid::row_t last,
                                int start_y,
                                bool blink) const noexcept;
        void set_viewport(void const* screen,
                          vte::grid::row_t first,
                          vte::grid::row_t last,
                          int start_y,
                          bool blink,
                          RenderNodePtr node);
        void invalidate_viewports() noexcept;

private:
        struct Key {
                void const* screen{};
                vte::grid::row_t row{};

                bool operator==(Key const&) const noexcept = default;
        };

        struct KeyHash {
                size_t operator()(Key const& key) const noexcept
                {
                        auto const screen = std::hash<void const*>{}(key.screen);
                        auto const row = std::hash<vte::grid::row_t>{}(key.row);
                        return screen ^ (row + 0x9e3779b9 + (screen << 6) + (screen >> 2));
                }
        };

        using Lru = std::list<Key>;

        struct Entry {
                Row row{};
                Lru::iterator lru{};
        };

        using Rows = std::unordered_map<Key, Entry, KeyHash>;

        struct Viewport {
                void const* screen{};
                vte::grid::row_t first{};
                vte::grid::row_t last{};
                int start_y{};
                RenderNodePtr node{};
        };

        Lru m_lru{};
        Rows m_rows{};
        std::array<Viewport, 2> m_viewports{};
        size_t m_row_limit{2};

        Rows::iterator find(void const* screen,
                            vte::grid::row_t row) noexcept;
        void trim();
};

} // namespace vte::view
