src_inc = include_directories('.')

subdir('vte')

gir_sources = files(
  'pty.cc',
  'vtegtk.cc',
)

common_deps = [
  gio_dep,
  glib_dep,
  gobject_dep,
  gtk_dep,
  pango_dep,
]

sources = vtetype_enums + gir_sources + files(
  'chunk.cc',
  'debug.cc',
  'keymap.cc',
  'parser.cc',
  'reaper.cc',
  'ring.cc',
  'utf8.cc',
  'vte.cc',
  'vteaccess.cc',
  'vtedraw.cc',
  'vteregex.cc',
  'vterowdata.cc',
  'vtespawn.cc',
  'vteseq.cc',
  'vtestream.cc',
  'vtetypes.cc',
  'vteunistr.cc',
  'vteutils.cc',
  'widget.cc',
)

box_drawing = 'box_drawing'

sources += custom_target(
  box_drawing,
  input: box_drawing + '.txt',
  output: box_drawing + '.h',
  capture: true,
  command: [find_program('box_drawing_generate.sh'), '@INPUT@'],
)

sources += gnome.compile_resources(
  'vteresources',
  'vte.gresource.xml.in',
  c_name: '_vte',
)

marshal = 'marshal'

sources += gnome.genmarshal(
  marshal,
  sources: marshal + '.list',
  prefix: '_vte_marshal',
  internal: true,
)

incs = [
  top_inc,
  vte_inc,
]

deps = common_deps + [
  libpcre_dep,
  m_dep,
  threads_dep,
  zlib_dep,
]

# FIXME: DATADIR, LIBEXECDIR seem to be unnecessary
cpp_flags = [
  '-DG_LOG_DOMAIN="Vte"',
  '-DVTE_API_VERSION="@0@"'.format(vte_api_version),
  '-DDATADIR="@0@"'.format(vte_prefix / vte_datadir),
  '-DLIBEXECDIR="@0@"'.format(vte_prefix / vte_libexecdir),
  '-DLOCALEDIR="@0@"'.format(vte_prefix / vte_localedir),
  '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
  '-DVTE_COMPILATION',
  '-UPARSER_INCLUDE_NOP',
]

if enable_gnutls
  deps += gnutls_dep
endif

libvte = both_libraries(
  'vte-' + vte_api_version,
  sources: sources,
  version: libversion,
  include_directories: incs,
  dependencies: deps,
  cpp_args: cpp_flags,
  install: true,
)

libvte_dep = declare_dependency(
  sources: [vteversion_header, vtetype_enums[1]],
  include_directories: [src_inc, vte_inc],
  dependencies: common_deps,
  link_with: libvte.get_shared_lib(),
)

pkg.generate(
  libvte,
  version: vte_version,
  name: meson.project_name(),
  description: 'Vte terminal widget.',
  filebase: vte_api_name,
  subdirs: vte_api_name,
  requires: common_deps,
  variables: 'exec_prefix=${prefix}',
)

subdir('app')

sources = files(
  'debug.cc',
  'parser.cc',
  'parser-cat.cc',
  'utf8.cc',
)

executable(
  'parser-cat',
  sources,
  include_directories: top_inc,
  dependencies: glib_dep,
  cpp_args: '-DPARSER_INCLUDE_NOP',
)

# name, sources, dependencies, c_flags
programs = [
  ['dumpkeys', {'sources': 'dumpkeys.c', 'dependencies': glib_dep}],
  ['mev', {'sources': 'mev.c', 'dependencies': glib_dep}],
  ['slowcat', {'sources': 'slowcat.c', 'dependencies': glib_dep}],
  ['reflect-text-view', {'sources': 'reflect.c', 'dependencies': libvte_dep, 'c_args': '-DUSE_TEXT_VIEW'}],
  ['reflect-vte', {'sources': 'reflect.c', 'dependencies': libvte_dep, 'c_args': '-DUSE_VTE'}],
  ['xticker', {'sources': 'xticker.c', 'dependencies': glib_dep}],
]

foreach program: programs
  executable(
    program[0],
    include_directories: top_inc,
    kwargs: program[1],
  )
endforeach

test_env = [
  'srcdir="@0@"'.format(meson.current_source_dir()),
  'top_builddir="@0@"'.format(build_root),
  'VTE_API_VERSION="@0@"'.format(vte_api_version),
]

# name, sources, dependencies, cpp_flags
test_units = [
  ['test-modes', {'sources': 'modes-test.cc', 'dependencies': glib_dep}],
  ['test-refptr', {'sources': 'refptr-test.cc', 'dependencies': gobject_dep}],
  ['test-tabstops', {'sources': 'tabstops-test.cc', 'dependencies': glib_dep}],
]

sources = files(
  'debug.cc',
  'reaper.cc',
)

deps = [
  glib_dep,
  gobject_dep,
]

test_units += [['reaper', {'sources': sources, 'dependencies': deps, 'cpp_args': '-DMAIN'}]]

sources = files(
  'parser-test.cc',
  'parser.cc',
)

test_units += [['test-parser', {'sources': sources, 'dependencies': glib_dep, 'cpp_args': '-DPARSER_INCLUDE_NOP'}]]

sources = files(
  'utf8-test.cc',
  'utf8.cc',
)

test_units += [['test-utf8', {'sources': sources, 'dependencies': glib_dep}]]

deps = [
  glib_dep,
  gtk_dep,
  pango_dep,
]

test_units += [['test-vtetypes', {'sources': 'vtetypes.cc', 'dependencies': deps, 'cpp_args': '-DMAIN'}]]

sources = [
  'vtestream.cc',
  'vteutils.cc',
]

deps = [
  gio_dep,
  glib_dep,
  gobject_dep,
  zlib_dep,
]

if enable_gnutls
  deps += gnutls_dep
endif

test_units += [['vtestream-file', {'sources': sources, 'dependencies': deps, 'cpp_args': '-DVTESTREAM_MAIN'}]]

foreach test_unit: test_units
  exe = executable(
    test_unit[0],
    include_directories: top_inc,
    kwargs: test_unit[1],
  )

  test(
    test_unit[0],
    exe,
    env: test_env,
  )
endforeach

test_scripts = [
  'check-doc-syntax.sh',
  'test-vte-sh.sh',
]

foreach test_script: test_scripts
  test(
    test_script,
    find_program(test_script),
    env: test_env,
  )
endforeach

# Shell
install_data(
  'vte.sh',
  install_dir: vte_sysconfdir / 'profile.d',
)
