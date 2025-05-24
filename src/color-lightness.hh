//
//  Copyright © 2025 Tau Gärtli
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 3 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "vtetypes.hh"

namespace vte::color {

// Perceptual lightness (L*) as a value between 0.0 (black) and 1.0 (white)
// where 0.5 is the perceptual middle gray.
auto perceived_lightness(rgb const& color) noexcept -> float;

} // namespace vte::color
