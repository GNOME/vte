# Copyright © 2025, 2026 Christian Persch
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
  'simdutf',
  'cpp',
  default_options: [
    'cpp_std=gnu++23'
  ],
  license: ['MIT',],
  version: '7.7.1',
)

py = import('python')
py_req_modules = [
]
python = py.find_installation(modules: py_req_modules)

simdutf_inc = include_directories('include', is_system: true,)

simdutf_sources = []

simdutf_amalgamate = [
  python,
  files('singleheader' / 'amalgamate.py'),
  '--source-dir', meson.project_source_root() / 'src',
  '--include-dir', meson.project_source_root() / 'include',
  '--output-dir', meson.project_build_root(),
  '--no-zip',
  '--no-readme',
]

if get_option('ascii')
  simdutf_amalgamate += ['--with-ascii']
endif

if get_option('base64')
  simdutf_amalgamate += ['--with-base64']
endif

if get_option('detect-encoding')
  simdutf_amalgamate += ['--with-detect-enc']
endif

if get_option('latin1')
  simdutf_amalgamate += ['--with-latin1']
endif

if get_option('utf8')
  simdutf_amalgamate += ['--with-utf8']
endif

if get_option('utf16')
  simdutf_amalgamate += ['--with-utf16']
endif

if get_option('utf32')
  simdutf_amalgamate += ['--with-utf32']
endif

if get_option('debug')
  simdutf_amalgamate += ['--debug-source']
endif

simdutf_sources += custom_target(
  'amalgamate',
  command: simdutf_amalgamate,
  input: [],
  install: false,
  output: [
    'simdutf.cpp',
    'simdutf.h',
  ],
)

simdutf_lib = static_library(
  'simdutf',
  cpp_args: [
    '-Wno-undef',
    '-Wno-missing-declarations',
  ],
  gnu_symbol_visibility: 'hidden',
  include_directories: simdutf_inc,
  install: false,
  pic: true,
  sources: simdutf_sources,
)

simdutf_dep = declare_dependency(
  include_directories: simdutf_inc,
  link_with: simdutf_lib,
)

meson.override_dependency('simdutf', simdutf_dep)
