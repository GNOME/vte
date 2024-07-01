#!/usr/bin/env python
# Copyright © 2022 Christian Persch
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

import sys

quadrants = [u'\u00a0', u'\u2598', u'\u259d', u'\u2580',
             u'\u2596', u'\u258c', u'\u259e', u'\u259b',
             u'\u2597', u'\u259a', u'\u2590', u'\u259c',
             u'\u2584', u'\u2599', u'\u259f', u'\u2588']

separated_quadrants = [u'\u00a0',     u'\U0001cc21', u'\U0001cc22', u'\U0001cc23',
                       u'\U0001cc24', u'\U0001cc25', u'\U0001cc26', u'\U0001cc27',
                       u'\U0001cc28', u'\U0001cc29', u'\U0001cc2a', u'\U0001cc2b',
                       u'\U0001cc2c', u'\U0001cc2d', u'\U0001cc2e', u'\U0001cc2f',]

sys.stdout.write('Quadrants:\n')

sys.stdout.write('  contiguous:')
for q in quadrants:
    sys.stdout.write(f' {q}')

sys.stdout.write('\n\n')

sys.stdout.write('  separated: ')
for q in separated_quadrants:
    sys.stdout.write(f' {q}')

sys.stdout.write('\n\n')
