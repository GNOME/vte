#!/usr/bin/env python3

# Image viewer for terminals that support true colors.
# Copyright (C) 2014, 2026  Egmont Koblinger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.


import argparse
import os
import subprocess
import sys
from PIL import Image


if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument("-w", "--width",
                        type=int,
                        help="output width (default: terminal's width)")
    parser.add_argument("--colon", "--official", "--dejure",
                        dest='semicolon',
                        action='store_false',
                        default=False,
                        help="official format \\e[38:2::R:G:Bm (default)")
    parser.add_argument("--semicolon", "--common", "--defacto",
                        dest='semicolon',
                        action='store_true',
                        help="commonly used format \\e[38;2;R;G;Bm")
    parser.add_argument("filename")

    args = parser.parse_args()

    if args.semicolon:
        sep1, sep2 = ';', ';'
    else:
        sep1, sep2 = ':', '::'

    if args.width:
        width = args.width
    else:
        width, _ = os.get_terminal_size(0)  # allow stdout/stderr redirect

    img = Image.open(args.filename)

    if img.mode not in ['RGB', 'RGBA']:
        img = img.convert('RGBA')

    height = round(width * img.height / img.width)
    img = img.resize((width, height))

    for y in range(0, height, 2):
        for x in (range(width)):
            rgb = img.getpixel((x, y))
            print(f'\033[38{sep1}2{sep2}{rgb[0]}{sep1}{rgb[1]}{sep1}{rgb[2]}', end='')
            if (y + 1 < height):
                # not last half row
                rgb = img.getpixel((x, y + 1))
                print(f';48{sep1}2{sep2}{rgb[0]}{sep1}{rgb[1]}{sep1}{rgb[2]}', end='')
            print('m\u2580', end='')
        print('\033[m')
