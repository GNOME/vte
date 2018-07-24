/*
 * Copyright © 2018 Christian Persch
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "widget.hh"

#include <new>

namespace vte {

namespace platform {

Widget::Widget(VteTerminal* t) noexcept :
        m_widget{t}
{
        /* Until Terminal init is completely fixed, use zero'd memory */
        auto place = g_malloc0(sizeof(vte::terminal::Terminal));
        m_terminal = new (place) vte::terminal::Terminal(t);
}

Widget::~Widget() noexcept
{
        m_widget = nullptr;

        m_terminal->~Terminal();
        g_free(m_terminal);
}

} // namespace platform

} // namespace vte
