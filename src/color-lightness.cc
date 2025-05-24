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

// Implementation of determining the perceived lightness
// follows this excellent answer: https://stackoverflow.com/a/56678483

#include "config.h"
#include "color-lightness.hh"
#include <math.h>

using namespace vte::color;

// Converts a non-linear sRGB value to a linear one via gamma correction.
static auto gamma_function(float value) noexcept -> float
{
        if (value <= .04045f) {
                return value / 12.92f;
        } else {
                return powf((value + .055f) / 1.055f, 2.4f);
        }
}

// Luminance (`Y`) calculated using the [CIE XYZ formula](https://en.wikipedia.org/wiki/Relative_luminance).
static auto luminance(rgb const& color) noexcept -> float
{
        auto const r = gamma_function(static_cast<float>(color.red) / static_cast<float>(G_MAXUINT16));
        auto const g = gamma_function(static_cast<float>(color.green) / static_cast<float>(G_MAXUINT16));
        auto const b = gamma_function(static_cast<float>(color.blue) / static_cast<float>(G_MAXUINT16));
        return .2126f * r + .7152f * g + .0722f * b;
}

static auto luminance_to_perceived_lightness(float luminance) noexcept -> float
{
        if (luminance <= (216.f / 24389.f)) {
                return luminance * (24389.f / 27.f);
        } else {
                return cbrtf(luminance) * 116.f - 16.f;
        }
}

// Perceptual lightness (L*) as a value between 0.0 (black) and 1.0 (white)
// where 0.5 is the perceptual middle gray.
auto vte::color::perceived_lightness(rgb const& color) noexcept -> float
{
        return luminance_to_perceived_lightness(luminance(color)) / 100.f;
}
