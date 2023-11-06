/*
 * Copyright © 2019 Christian Persch
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

namespace vte {

namespace base {

class Pty;

} // namespace base

namespace platform {

class Clipboard;
class EventBase;
class KeyEvent;
class MouseEvent;
class ScrollEvent;
class Widget;

} // namespace platform

namespace sixel {

class Context;
class Parser;
class Sequence;

} // namespace sixel

namespace view {

class FontInfo;
class DrawingContext;

} // namespace view

} // namespace vte
