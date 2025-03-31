# Copyright © 2025 Christian Persch
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
  'fmt',
  'cpp',
  default_options: [
    'cpp_std=gnu++20'
  ],
  license: ['MIT',],
  version: '11.1.4',
)

fmt_inc = include_directories('include', is_system: true,)

fmt_dep = declare_dependency(
  include_directories: fmt_inc,
  compile_args: [
    '-DFMT_HEADER_ONLY',
  ]
)

meson.override_dependency('fmt', fmt_dep)
