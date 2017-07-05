project(
  'vte', ['c', 'cpp'],
  version: '0.57.0',
  license: 'GPL3',
  default_options: [
    'buildtype=debugoptimized',
    'cpp_std=gnu++17',
  ],
  meson_version: '>= 0.49.0',
)

vte_version = meson.project_version()
version_array = vte_version.split('.')
vte_major_version = version_array[0].to_int()
vte_minor_version = version_array[1].to_int()
vte_micro_version = version_array[2].to_int()

vte_prefix = get_option('prefix')
vte_datadir = get_option('datadir')
vte_libexecdir = get_option('libexecdir')
vte_localedir = get_option('localedir')
vte_includedir = get_option('includedir')
vte_sysconfdir = get_option('sysconfdir')

enable_gir = get_option('introspection')
enable_vala = get_option('vapi')
enable_glade_catalogue = get_option('glade_catalogue')

if get_option('gtk') == '3.0'
  gtk_api_version = '3.0'
  gtk_req_version = '>= 3.8.0'
  vte_api_version = '2.91'
  glade_dep_name = 'gladeui-2.0'
else
  gtk_api_version = '4.0'
  gtk_req_version = '>= 3.89.0'
  vte_api_version = '3.91'
  # glade_dep_name = 'gladeui-4.0' ?
  # remove the following when they're ready
  enable_gir = false
  enable_vala = false
  enable_glade_catalogue = false
endif

gtk_dep_name = 'gtk+-' + gtk_api_version

vte_api_name = '@0@-@1@'.format(meson.project_name(), vte_api_version)
vte_api_path = vte_api_name / meson.project_name()

vte_pkgincludedir = vte_includedir / vte_api_name / meson.project_name()

soversion = 0

revision = (vte_minor_version.is_odd() ? 0 : vte_micro_version)
age = vte_minor_version * 100 + vte_micro_version - revision
current = vte_major_version + age

libversion = '@0@.@1@.@2@'.format(soversion, current, revision)

vte_debug = get_option('buildtype').contains('debug')

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

source_root = meson.current_source_dir()
build_root = meson.current_build_dir()

po_dir = source_root / 'po'

top_inc = include_directories('.')

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

config_h = configuration_data()

config_h.set_quoted('VERSION', vte_version)

# i18n
config_h.set_quoted('GETTEXT_PACKAGE', vte_api_name)

# GLIB tools
config_h.set_quoted('VTE_DEFAULT_TERM', 'xterm-256color')

config_h.set('VTE_DEBUG', vte_debug)

# Globally define_GNU_SOURCE and therefore enable the GNU extensions
config_h.set10('_GNU_SOURCE', true)

# headers
check_headers = [
  'pty.h',
  'stropts.h',
  'sys/select.h',
  'sys/syslimits.h',
  'sys/termios.h',
  'sys/wait.h',
  'termios.h',
  'util.h',
  'wchar.h',
  # i18n
  'locale.h',
  # for vtespawn
  'sys/resource.h',
]

foreach header: check_headers
  config_h.set('HAVE_' + header.underscorify().to_upper(), cc.has_header(header))
endforeach

# symbols
assert(cc.has_header_symbol('sys/ioctl.h', 'TIOCGWINSZ'), 'no support for TIOCGWINSZ symbol')

# i18n
config_h.set('HAVE_DECL_BIND_TEXTDOMAIN_CODESET', cc.has_header_symbol('libintl.h', 'bind_textdomain_codeset'))

# functions
# Check for how to open a new PTY. We support posix_openpt and BSDs openpty
foreach func: ['grantpt', 'posix_openpt', 'ptsname', 'unlockpt']
  assert(cc.has_function(func), 'no support for Unix98 PTY found')
endforeach

assert(cc.has_function('fork'), 'no support for fork function')
assert(cc.has_function('tcgetattr'), 'no support for tcgetattr function')

check_functions = [
  # Misc PTY handling functions
  'cfmakeraw',
  'getpgid',
  'setpgid',
  'setsid',
  'tcsetattr',
  # Misc I/O routines.
  'pread',
  'pwrite',
  'explicit_bzero',
  # Misc string routines.
  'strchrnul',
  # for vtespawn
  'fdwalk',
]

foreach func: check_functions
  config_h.set('HAVE_' + func.underscorify().to_upper(), cc.has_function(func))
endforeach

# Math functions
m_dep = cc.find_library('m')

assert(cc.has_function('ceil', dependencies: m_dep), 'no support for ceil function')
assert(cc.has_function('floor', dependencies: m_dep), 'no support for floor function')
config_h.set('HAVE_ROUND', cc.has_function(func, dependencies: m_dep))

# compiler flags
common_flags = ['-DHAVE_CONFIG_H']
common_c_flags = []
common_cpp_flags = []

if vte_debug
  common_test_flags = [
    '-fno-common',
    '-fdiagnostics-show-option',
    '-fno-semantic-interposition',
    '-fno-strict-aliasing',
    '-fstack-protector',
    '-fstack-protector-strong',
    '-Wcast-align',
    '-Werror=declaration-after-statement',
    '-Werror=format=2',
    '-Werror=format-nonliteral',
    '-Werror=format-security',
    '-Werror=implicit-function-declaration',
    '-Werror=init-self',
    '-Werror=missing-include-dirs',
    '-Werror=missing-prototypes',
    '-Werror=pointer-arith',
    '-Wendif-labels',
    '-Wfloat-equal',
    '-Wlogical-op',
    '-Wmisleading-indentation',
    '-Wmissing-declarations',
    '-Wmissing-include-dirs',
    '-Wmissing-format-attribute',
    '-Wmissing-noreturn',
    '-Wno-deprecated-declarations',
    '-Wno-missing-field-initializers',
    '-Wno-switch-enum',
    '-Wno-unused-parameter',
    '-Wno-packed',
    '-Wshadow',
    '-Wstrict-aliasing=2',
    '-Wundef',
    '-Wunsafe-loop-optimizations',
    '-Wvla',
    '-Wwrite-strings',
  ]

  common_c_flags += cc.get_supported_arguments(common_test_flags + [
    '-Waggregate-return',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wsign-compare',
    '-Wstrict-prototypes',
  ])

  common_cpp_flags += cpp.get_supported_arguments(common_test_flags + [
    '-fno-rtti',
    '-fno-exceptions',
    '-fvisibility-inlines-hidden',
    '-Wimplicit-fallthrough=5',
    '-Wstrict-null-sentinel',
    # FIXME: otherwise it doesn't compile
    '-fpermissive',
  ])

  flag = '-fvisibility=hidden'
  assert(cpp.has_argument(flag), cpp.get_id() + ' doesn\'t support required flag -fvisibility=hidden')
  common_cpp_flags += [flag]
endif

add_project_arguments(common_flags + common_c_flags, language: 'c')
add_project_arguments(common_flags + common_cpp_flags, language: 'cpp')

enable_b_symbolic = get_option('bsymbolic_functions')
if not enable_b_symbolic.disabled()
  ldflag = '-Wl,-Bsymbolic-functions'

  if cc.has_link_argument(ldflag)
    add_project_link_arguments(ldflag, language: 'c')
  else
    if enable_b_symbolic.enabled()
      error('-Bsymbolic_functions requested but not supported by ld. Use -DBsymbolic_functions=false to disable')
    endif
    message('-Bsymbolic_functions not supported by ld; disabling')
  endif

  if cpp.has_link_argument(ldflag)
    add_project_link_arguments(ldflag, language: 'cpp')
  else
    if enable_b_symbolic.enabled()
      error('-Bsymbolic_functions requested but not supported by cxx ld. Use -DBsymbolic_functions=false to disable')
    endif
    message('-Bsymbolic_functions not supported by ld; disabling')
  endif
endif

gio_dep = dependency('gio-2.0')
glib_dep = dependency('glib-2.0', version: '>= 2.40.0')
gobject_dep = dependency('gobject-2.0')
pango_dep = dependency('pango', version: '>= 1.22.0')
libpcre_dep = dependency('libpcre2-8', version: '>= 10.21')
zlib_dep = dependency('zlib')

threads_dep = dependency('threads')

gtk_dep = dependency(gtk_dep_name, version: gtk_req_version)

enable_gnutls = get_option('gnutls')
if enable_gnutls
  gnutls_dep = dependency('gnutls', version: '>= 3.2.7')
endif
config_h.set('WITH_GNUTLS', enable_gnutls)

# Legacy Charset Support
enable_iconv = get_option('iconv')
config_h.set('WITH_ICONV', enable_iconv)

subdir('src')

# introspection support
if enable_gir
  gir_dep = dependency('gobject-introspection-1.0', version: '>= 0.9.0', required: false)
  assert(gir_dep.found(), 'Introspection requested but gobject-introspection-1.0 not installed. Try -Dintrospection=false')

  subdir('bindings/gir')
endif

# vala support
if enable_vala
  assert(enable_gir, 'you need introspection support for the vala support')

  vala_req_version = '>= 0.24.0'
  error_str = 'Vala requested but vala compiler ' + vala_req_version + ' not found. Try -Dvapi=false'

  assert(add_languages('vala', required: false), error_str)
  valac = meson.get_compiler('vala')
  valac_version = valac.version()
  assert(valac_version.version_compare(vala_req_version), error_str)

  vapi_dir = source_root / 'bindings' / 'vala'
  config_dep = valac.find_library('config', dirs: vapi_dir)
  posix_dep = valac.find_library('posix')

  subdir('bindings/vala')
endif

subdir('po')

if get_option('gtk_doc')
  subdir('doc/reference')
endif

if enable_glade_catalogue
  glade_dep = dependency(glade_dep_name)

  glade_catalogdir = glade_dep.get_pkgconfig_variable('catalogdir', define_variable: ['datarootdir', vte_prefix / vte_datadir])
  assert(glade_catalogdir != '', glade_dep_name + ' not found')
  glade_catalog_dtd = glade_dep.get_pkgconfig_variable('catalogdir') / 'glade-catalog.dtd'

  # FIXME: glade_moduledir is not used at all
  # FIXME: this should redefine libdir. however, moduledir definition is wrong in the pc file
  #glade_moduledir = glade_dep.get_pkgconfig_variable('moduledir', define_variable: ['prefix', vte_prefix])
  #assert(glade_moduledir != '', glade_dep_name + ' not found')

  glade_pixmapdir = glade_dep.get_pkgconfig_variable('pixmapdir', define_variable: ['datarootdir', vte_prefix / vte_datadir])

  subdir('glade')
endif

configure_file(
  output: 'config.h',
  configuration: config_h,
)

output = 'Configuration for libvte ' + vte_version + ' for ' + gtk_dep_name + '\n'
output += '        GNUTLS: ' + enable_gnutls.to_string() + '\n'
output += '        IConv: ' + enable_iconv.to_string() + '\n'
output += '        Installing Glade catalogue: ' + enable_glade_catalogue.to_string() + '\n'
output += '        Debugging: ' + vte_debug.to_string() + '\n'
output += '        Introspection: ' + enable_gir.to_string() + '\n'
output += '        Vala binginds: ' + enable_vala.to_string()
message(output)
