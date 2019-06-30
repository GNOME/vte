# Copyright © 2018, 2019 Iñigo Martínez
# Copyright © 2019 Christian Persch
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library.  If not, see <https://www.gnu.org/licenses/>.

subdir('vte')

src_inc = include_directories('.')

debug_sources = files(
  'debug.cc',
  'debug.h',
)

modes_sources = files(
  'modes-ecma.hh',
  'modes-private.hh',
  'modes.hh',
)

parser_sources = files(
  'parser-arg.hh',
  'parser-c01.hh',
  'parser-charset-tables.hh',
  'parser-charset.hh',
  'parser-cmd.hh',
  'parser-csi.hh',
  'parser-dcs.hh',
  'parser-decsgr.hh',
  'parser-esc.hh',
  'parser-glue.hh',
  'parser-osc.hh',
  'parser-reply.hh',
  'parser-sci.hh',
  'parser-sgr.hh',
  'parser-string.hh',
  'parser.cc',
  'parser.hh',
)

utf8_sources = files(
  'utf8.cc',
  'utf8.hh',
)

libvte_common_sources = debug_sources + modes_sources + parser_sources + utf8_sources + files(
  'attr.hh',
  'bidi.cc',
  'bidi.hh',
  'buffer.h',
  'caps.hh',
  'cell.hh',
  'chunk.cc',
  'chunk.hh',
  'color-triple.hh',
  'keymap.cc',
  'keymap.h',
  'pty.cc',
  'reaper.cc',
  'reaper.hh',
  'refptr.hh',
  'ring.cc',
  'ring.hh',
  'ringview.cc',
  'ringview.hh',
  'utf8.cc',
  'utf8.hh',
  'vte.cc',
  'vteaccess.cc',
  'vteaccess.h',
  'vtedefines.hh',
  'vtedraw.cc',
  'vtedraw.hh',
  'vtegtk.cc',
  'vtegtk.hh',
  'vteinternal.hh',
  'vtepcre2.h',
  'vtepty-private.h',
  'vteregex.cc',
  'vteregexinternal.hh',
  'vterowdata.cc',
  'vterowdata.hh',
  'vteseq.cc',
  'vtespawn.cc',
  'vtespawn.hh',
  'vtestream-base.h',
  'vtestream-file.h',
  'vtestream.cc',
  'vtestream.h',
  'vtetypes.cc',
  'vtetypes.hh',
  'vteunistr.cc',
  'vteunistr.h',
  'vteutils.cc',
  'vteutils.h',
  'widget.cc',
  'widget.hh',
)

libvte_common_doc_sources = files(
  # These file contain gtk-doc comments to be extracted for docs and gir
  'pty.cc',
  'vtegtk.cc',
)

# FIXMEchpe this should use files('...') (allowed as per docs, afaict), but that crashes meson
generate_box_drawing = find_program('box_drawing_generate.sh')

libvte_common_sources += custom_target(
  'box-drawing',
  input: 'box_drawing.txt',
  output: 'box_drawing.h',
  capture: true,
  command: [generate_box_drawing, '@INPUT@'],
  install: false,
)

libvte_common_sources += gnome.compile_resources(
  'vteresources',
  'vte.gresource.xml',
  c_name: '_vte',
)

libvte_common_sources += gnome.genmarshal(
  'marshal',
  sources: 'marshal.list',
  prefix: '_vte_marshal',
  internal: true,
  valist_marshallers: true,
)

libvte_common_public_deps = [
  gio_dep,
  glib_dep,
  gobject_dep,
  pango_dep,
]

libvte_common_deps = libvte_common_public_deps + [
  fribidi_dep,
  gnutls_dep,
  pcre2_dep,
  libm_dep,
  pthreads_dep,
  zlib_dep,
]

incs = [
  top_inc,
  vte_inc,
]

libvte_common_cppflags = [
  '-DG_LOG_DOMAIN="VTE"',
  '-DLOCALEDIR="@0@"'.format(vte_prefix / vte_localedir),
  '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
  '-DVTE_COMPILATION',
  '-UPARSER_INCLUDE_NOP',
]

if get_option('gtk3')
  libvte_gtk3_sources = libvte_common_sources + libvte_gtk3_public_headers + libvte_gtk3_enum_sources
  libvte_gtk3_deps = libvte_common_deps + [gtk3_dep]
  libvte_gtk3_public_deps = libvte_common_public_deps + [gtk3_dep]

  libvte_gtk3 = shared_library(
    vte_gtk3_api_name,
    sources: libvte_gtk3_sources,
    version: libvte_gtk3_soversion,
    include_directories: incs,
    dependencies: libvte_gtk3_deps,
    cpp_args: libvte_common_cppflags,
    install: true,
  )

  libvte_gtk3_dep = declare_dependency(
    sources: libvte_gtk3_public_headers,
    include_directories: [src_inc, vte_inc],
    dependencies: libvte_gtk3_deps,
    link_with: libvte_gtk3
  )

  pkg.generate(
    libvte_gtk3,
    version: vte_version,
    name: 'vte',
    description: 'VTE widget for GTK+ 3.0',
    filebase: vte_gtk3_api_name,
    subdirs: vte_gtk3_api_name,
    requires: libvte_gtk3_public_deps,
    variables: 'exec_prefix=${prefix}',
  )
endif

## Tests

# cat

parser_cat_sources = parser_sources + utf8_sources + debug_sources + files(
  'parser-cat.cc'
)

parser_cat = executable(
  'parser-cat',
  parser_cat_sources,
  dependencies: [glib_dep],
  cpp_args: ['-DPARSER_INCLUDE_NOP'],
  include_directories: top_inc,
  install: false,
)

# dumpkeys

dumpkeys_sources = files(
  'dumpkeys.c'
)

dumpkeys = executable(
  'dumpkeys',
  sources: dumpkeys_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

# mev

mev_sources = files(
  'mev.c'
)

mev = executable(
  'mev',
  sources: mev_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

# slowcat

slowcat_sources = files(
  'slowcat.c'
)

slowcat = executable(
  'slowcat',
  sources: slowcat_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

# reflect

reflect_sources = files(
  'reflect.c'
)

reflect_textview = executable(
  'reflect-textview',
  sources: reflect_sources,
  dependencies: [gtk3_dep],
  c_args: ['-DUSE_TEXT_VIEW'],
  include_directories: top_inc,
  install: false,
)

reflect_vte = executable(
  'reflect-vte',
  sources: reflect_sources,
  dependencies: [gtk3_dep, libvte_gtk3_dep],
  c_args: ['-DUSE_VTE'],
  include_directories: top_inc,
  install: false,
)

# xticker

xticker_sources = files(
  'xticker.c'
)

xticker = executable(
  'xticker',
  sources: xticker_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

# Unit tests

test_modes_sources = modes_sources + files(
  'modes-test.cc',
)

test_modes = executable(
  'test-modes',
  sources: test_modes_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

test_parser_sources = parser_sources + files(
  'parser-test.cc',
)

test_parser = executable(
  'test-parser',
  sources: test_parser_sources,
  dependencies: [glib_dep],
  cpp_args: ['-DPARSER_INCLUDE_NOP'],
  include_directories: top_inc,
  install: false,
)

test_reaper_sources = debug_sources + files(
  'reaper.cc',
  'reaper.hh'
)

test_reaper = executable(
  'test-reaper',
  sources: test_reaper_sources,
  dependencies: [gobject_dep],
  cpp_args: ['-DMAIN'],
  include_directories: top_inc,
  install: false,
)

test_refptr_sources = files(
  'refptr-test.cc',
  'refptr.hh'
)

test_refptr = executable(
  'test-refptr',
  sources: test_refptr_sources,
  dependencies: [glib_dep, gobject_dep],
  include_directories: top_inc,
  install: false,
)

test_tabstops_sources = files(
  'tabstops-test.cc',
  'tabstops.hh'
)

test_stream_sources = files(
  'vtestream-base.h',
  'vtestream-file.h',
  'vtestream.cc',
  'vtestream.h',
  'vteutils.cc',
  'vteutils.h',
)

test_stream = executable(
  'test-stream',
  sources: test_stream_sources,
  dependencies: [gio_dep, gnutls_dep, zlib_dep],
  cpp_args: ['-DVTESTREAM_MAIN'],
  include_directories: top_inc,
  install: false,
)

test_tabstops = executable(
  'test-tabstops',
  sources: test_tabstops_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

test_utf8_sources = utf8_sources + files(
  'utf8-test.cc',
)

test_utf8 = executable(
  'test-utf8',
  sources: test_utf8_sources,
  dependencies: [glib_dep],
  include_directories: top_inc,
  install: false,
)

test_vtetypes_sources = files(
   'vtetypes.cc',
   'vtetypes.hh',
)

test_vtetypes = executable(
  'test-vtetypes',
  sources: test_vtetypes_sources,
  dependencies: [glib_dep, pango_dep, gtk3_dep],
  cpp_args: ['-DMAIN'],
  include_directories: top_inc,
  install: false,
)

test_env = [
  'VTE_DEBUG=0'
]

# apparently there is no way to get a name back from an executable(), so it this ugly way
test_units = [
  ['modes', test_modes],
  ['parser', test_parser],
  ['reaper', test_reaper],
  ['refptr', test_refptr],
  ['stream', test_stream],
  ['tabstops', test_tabstops],
  ['utf8', test_utf8],
  ['vtetypes', test_vtetypes],
]

foreach test: test_units
  test(
    test[0],
    test[1],
    env: test_env,
    timeout: 600, # See issue vte#120
  )
endforeach

test_script_env = test_env + [
  # srcdir is needed for the vte.sh test
  'srcdir=@0@'.format(meson.current_source_dir()),
]

test_scripts = [
  ['doc syntax', 'check-doc-syntax.sh'],
]

foreach test: test_scripts
  test(
    test[0],
    files(test[1]),
    env: test_script_env,
  )
endforeach

test_scripts_interactive = [
  ['vte.sh', 'test-vte-sh.sh', files('vte.sh')],
]

# vte.sh needs to be run in an interactive shell, but there is way to
# specify that in test(), except by running bash itself explicitly with -i.
bash = find_program('bash')

foreach test: test_scripts_interactive
  test(
    test[0],
    bash,
    args: ['-i', files(test[1]), test[2]],
    env: test_script_env,
  )
endforeach

# Shell integration

install_data(
  'vte.sh',
  install_dir: vte_sysconfdir / 'profile.d',
)

# Demo application

subdir('app')
