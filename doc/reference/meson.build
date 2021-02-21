# Copyright © 2021 Christian Persch
#
# This library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library.  If not, see <https://www.gnu.org/licenses/>.

# It turned out to be impossible to build gtk3 and gtk4 versions of the
# docs from the same source using meson's gnome.gtkdoc(). Instead, build
# using make with a gtk-doc.make-derived Makefile.

make = find_program('gmake', 'make')

cairo_dep = dependency('cairo')

if get_option('gtk3')
  subdir('gtk3')
endif

if get_option('gtk4')
  subdir('gtk4')
endif
