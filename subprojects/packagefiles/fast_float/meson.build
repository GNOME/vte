# Copyright © 2024 Christian Persch
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

project(
  'fast_float',
  'cpp',
  default_options: [
    'cpp_std=gnu++20'
  ],
  license: ['MIT',],
  version: '6.1.6',
)

fast_float_inc = include_directories('include', is_system: true,)
fast_float_dep = declare_dependency(include_directories: fast_float_inc,)

meson.override_dependency('fast_float', fast_float_dep)
