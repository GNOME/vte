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

project(
  'vte',
  ['c', 'cpp'],
  version: '0.58.4',
  license: ['LGPL-3.0-or-later', 'GPL-3.0-or-later'],
  default_options: [
    'buildtype=release',
    'c_std=gnu11',
    'cpp_std=gnu++17',
    'warning_level=' + (meson.version().version_compare('>= 0.50.0') ? '0' : '1'),
    'b_ndebug=false',
  ],
  meson_version: '>= 0.49.0',
)

# Requirements

gtk3_req_version          = '3.8.0'
gtk4_req_version          = '4.0.0'

fribidi_req_version       = '1.0.0'
gio_req_version           = '2.40.0'
glib_req_version          = '2.40.0'
gnutls_req_version        = '3.2.7'
pango_req_version         = '1.22.0'
pcre2_req_version         = '10.21'

# API

vte_api_major_version = 2
vte_api_minor_version = 91
vte_api_version = '@0@.@1@'.format(vte_api_major_version, vte_api_minor_version)
vte_api_name = 'vte-@0@.@1@'.format(vte_api_major_version, vte_api_minor_version)

vte_gtk3_api_version = '@0@.@1@'.format(vte_api_major_version, vte_api_minor_version)
vte_gtk4_api_version = '@0@.@1@'.format(vte_api_major_version + 1, vte_api_minor_version)

vte_gtk3_api_name = 'vte-' + vte_gtk3_api_version
vte_gtk4_api_name = 'vte-' + vte_gtk4_api_version

vte_gtk3_api_path = vte_gtk3_api_name / 'vte'
vte_gtk4_api_path = vte_gtk4_api_name / 'vte'

# Library versioning

vte_version = meson.project_version()
version_array = vte_version.split('.')
vte_major_version = version_array[0].to_int()
vte_minor_version = version_array[1].to_int()
vte_micro_version = version_array[2].to_int()

libvte_soversion = 0

lt_revision = (vte_minor_version.is_odd() ? 0 : vte_micro_version)
lt_age = vte_minor_version * 100 + vte_micro_version - lt_revision
lt_current = vte_major_version + lt_age

libvte_gtk3_soversion = '@0@.@1@.@2@'.format(libvte_soversion, lt_current, lt_revision)
libvte_gtk4_soversion = libvte_soversion.to_string()

# i18n

vte_gettext_domain = vte_api_name

# Directories

vte_datadir = get_option('datadir')
vte_includedir = get_option('includedir')
vte_libexecdir = get_option('libexecdir')
vte_localedir = get_option('localedir')
vte_prefix = get_option('prefix')
vte_sysconfdir = get_option('sysconfdir')

# Debug

enable_debug = get_option('debugg') or get_option('debug') or get_option('buildtype').contains('debug')

# Meson modules

gnome = import('gnome')
pkg = import('pkgconfig')

# Compilers

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

top_inc = include_directories('.')

# Start config.h

config_h = configuration_data()

config_h.set_quoted('GETTEXT_PACKAGE', vte_gettext_domain)
config_h.set_quoted('VERSION', vte_version)
config_h.set('VTE_DEBUG', enable_debug)
config_h.set('WITH_FRIBIDI', get_option('fribidi'))
config_h.set('WITH_GNUTLS', get_option('gnutls'))
config_h.set('WITH_ICONV', get_option('iconv'))

# FIXME AC_USE_SYSTEM_EXTENSIONS also supported non-gnu systems
config_h.set10('_GNU_SOURCE', true)

# Check headers

check_headers = [
  'locale.h',
  'pty.h',
  'stropts.h',
  'sys/resource.h',
  'sys/select.h',
  'sys/syslimits.h',
  'sys/termios.h',
  'sys/types.h',
  'sys/wait.h',
  'termios.h',
  'util.h',
  'wchar.h',
]

foreach header: check_headers
  config_h.set('HAVE_' + header.underscorify().to_upper(), cxx.has_header(header))
endforeach

# Check for symbols

check_symbols_required = [
  ['TIOCGWINSZ', 'sys/ioctl.h'],
]

foreach symbol: check_symbols_required
  assert(cxx.has_header_symbol(symbol[1], symbol[0]), symbol[0] + ' not found')
endforeach

# Check for functions

check_functions_required = [
  'fork',
  'grantpt',
  'posix_openpt',
  'ptsname',
  'tcgetattr',
  'unlockpt',
]

foreach func: check_functions_required
  assert(cxx.has_function(func), func + ' not found')
endforeach

check_functions = [
  # Misc PTY handling functions
  'cfmakeraw',
  'getpgid',
  'setpgid',
  'setsid',
  'tcsetattr',
  # Misc I/O routines.
  'explicit_bzero',
  'pread',
  'pwrite',
  # Misc string routines.
  'strchrnul',
  # for vtespawn
  'fdwalk',
]

foreach func: check_functions
  config_h.set('HAVE_' + func.underscorify().to_upper(), cxx.has_function(func))
endforeach

# Math functions

libm_dep = cxx.find_library('m')

check_math_functions_required = [
  'ceil',
  'floor',
]

foreach func: check_math_functions_required
  assert(cxx.has_function(func, dependencies: libm_dep), func + ' not found')
endforeach

check_math_functions = [
  'round',
]

foreach func: check_math_functions
  config_h.set('HAVE_' + func.underscorify().to_upper(), cxx.has_function(func, dependencies: libm_dep))
endforeach

# Compiler

# Meson has a misfeature where it allows the user to override the -std option
# for the C/C++ compiler. Disallow that.

assert(get_option('c_std') == 'gnu11', 'cannot override C std version')
assert(get_option('cpp_std') == 'gnu++17', 'cannot override C++ std version')

# Meson only checks that -std supports the given string, but *not* that
# the compiler really supports that C++ standard version. Do a simple version
# check based on https://gcc.gnu.org/projects/cxx-status.html#cxx17

if cxx.get_id() == 'gcc'
  assert(cxx.version().version_compare('>= 7.0'), 'needs G++ >= 7 for C++17 support')
endif

# Asserts must not be disabled

assert(get_option('b_ndebug') == 'false', 'assertions may not be disabled')

# Compiler flags

compiler_flags_common = [
  '-Wall',
  '-Wextra',
  '-Wcast-align',
  '-Wcast-function-type',
  '-Wclobbered',
  '-Wempty-body',
  '-Wendif-labels',
  '-Werror=init-self',
  '-Werror=missing-include-dirs',
  '-Werror=pointer-arith',
  '-Wfloat-equal',
  '-Wignored-qualifiers',
  '-Winvalid-pch',
  '-Wlogical-op',
  '-Wmisleading-indentation',
  '-Wmissing-declarations',
  '-Wmissing-field-initializers',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wno-address-of-packed-member',
  '-Wno-deprecated-declarations',
  '-Wno-missing-field-initializers',
  '-Wno-packed',
  '-Wno-switch-enum',
  '-Wno-unused-parameter',
  '-Wshadow',
  '-Wshift-negative-value',
  '-Wsign-compare',
  '-Wstrict-aliasing=2',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wuninitialized',
  '-Wunsafe-loop-optimizations',
  '-Wunused',
  '-Wunused-but-set-parameter',
  '-Wunused-but-set-variable',
  '-Wunused-function',
  '-Wunused-label',
  '-Wunused-local-typedefs',
  '-Wunused-value',
  '-Wunused-variable',
  '-Wvla',
  '-Wwrite-strings',
  '-fdiagnostics-show-option',
  '-fno-common',
  '-fno-semantic-interposition',
  '-fstack-protector',
  '-fstack-protector-strong',
]

if enable_debug
  compiler_flags_common += [
    '-ggdb3',
  ]
endif

# These are currently needed but the code should be fixed instead
compiler_flags_common_undesirable = [
  '-fno-strict-aliasing'
]

compiler_flags_c_only = [
  '-Waggregate-return',
  '-Werror=implicit-function-declaration',
  '-Werror=missing-prototypes',
  '-Wimplicit',
  '-Wimplicit-fallthrough=3',
  '-Wmissing-parameter-type',
  '-Wnested-externs',
  '-Wold-style-declaration',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wsign-compare',
  '-Wstrict-prototypes',
]

compiler_flags_cxx_only = [
  '-Wimplicit-fallthrough=5',
  '-Wnon-virtual-dtor',
  '-Wstrict-null-sentinel',
]

compiler_flags_cxx_required = [
  '-fno-exceptions',
  '-fno-rtti',
  '-fvisibility-inlines-hidden',
  '-fvisibility=hidden',
]

global_cflags = cc.get_supported_arguments(compiler_flags_common +
                                           compiler_flags_common_undesirable +
                                           compiler_flags_c_only)
global_cxxflags = cxx.get_supported_arguments(compiler_flags_common +
                                              compiler_flags_common_undesirable +
                                              compiler_flags_cxx_only +
                                              compiler_flags_cxx_required)

foreach flag: compiler_flags_cxx_required
  assert(cxx.has_argument(flag), flag + ' is required but not supported')
endforeach


# Meson problem: GCC only accepts the latter 2 options of the 3 below
# if the first is *also* passed, which doesn't work with get_supported_arguments()
# above. So just add these unconditionally, since all compilers we support
# accept these flags.

compiler_flags_format_warnings = [
  '-Werror=format=2',
  '-Werror=format-nonliteral',
  '-Werror=format-security',
]

global_cflags += compiler_flags_format_warnings
global_cxxflags += compiler_flags_format_warnings

# ... and now make these flags the default

add_project_arguments(global_cflags, language: 'c')
add_project_arguments(global_cxxflags, language: 'cpp')

# Linker flags

linker_flags = [
  '-Wl,-Bsymbolic-functions'
]

foreach flag: linker_flags
  assert(cc.has_link_argument(flag), flag + ' is required but not supported')
  add_project_link_arguments(flag, language: 'c')

  assert(cxx.has_link_argument(flag), flag + ' is required but not supported')
  add_project_link_arguments(flag, language: 'cpp')
endforeach

# Dependencies

gio_dep      = dependency('gio-2.0',    version: '>=' + gio_req_version)
glib_dep     = dependency('glib-2.0',   version: '>=' + glib_req_version)
gobject_dep  = dependency('gobject-2.0')
pango_dep    = dependency('pango',      version: '>=' + pango_req_version)
pcre2_dep    = dependency('libpcre2-8', version: '>=' + pcre2_req_version)
pthreads_dep = dependency('threads')
zlib_dep     = dependency('zlib')

if get_option('fribidi')
  fribidi_dep = dependency('fribidi', version: '>=' + fribidi_req_version)
else
  fribidi_dep = dependency('', required: false)
endif

if get_option('gnutls')
  gnutls_dep = dependency('gnutls', version: '>=' + gnutls_req_version)
else
  gnutls_dep = dependency('', required: false)
endif

if get_option('gtk3')
  gtk3_dep = dependency('gtk+-3.0', version: '>=' + gtk3_req_version)
else
  gtk3_dep = dependency('', required: false)
endif

if get_option('gtk4')
  gtk4_dep = dependency('gtk+-4.0', version: '>=' + gtk4_req_version)
else
  gtk4_dep = dependency('', required: false)
endif

# Write config.h

configure_file(
  output: 'config.h',
  configuration: config_h,
)

# Subdirs

subdir('src')
subdir('bindings')
subdir('po')

if get_option('docs')
  subdir('doc/reference')
endif

# Simple compat Makefile

makefile_conf = configuration_data()
makefile_conf.set('srcdir', meson.current_source_dir())
makefile_conf.set('builddir', meson.current_build_dir())
makefile_conf.set('vte_gtk3_api_version', vte_gtk3_api_version)
makefile_conf.set('vte_gtk4_api_version', vte_gtk4_api_version)

configure_file(
  input: 'Makefile.meson',
  output: '@BASENAME@',
  configuration: makefile_conf,
)

# .gitignore everything in the build directory

configure_file(
  output: '.gitignore',
  command: ['echo', '**/**',],
  capture: true,
  install: false,
)

# Summary

output = '\n'
output += 'Configuration for VTE:\n\n'
output += '  Version:      ' + vte_version + '\n'
output += '\n'
output += '  C compiler:   ' + cc.get_id() + '\n'
output += '  C++ compiler: ' + cxx.get_id() + '\n'
output += '\n'
output += '  Coverage:     ' + get_option('b_coverage').to_string() + '\n'
output += '  Debug:        ' + enable_debug.to_string() + '\n'
output += '  Docs:         ' + get_option('docs').to_string() + '\n'
output += '  FRIBIDI:      ' + get_option('fribidi').to_string() + '\n'
output += '  GNUTLS:       ' + get_option('gnutls').to_string() + '\n'
output += '  GTK+ 3.0:     ' + get_option('gtk3').to_string() + '\n'
output += '  GTK+ 4.0:     ' + get_option('gtk4').to_string() + '\n'
output += '  IConv:        ' + get_option('iconv').to_string() + '\n'
output += '  GIR:          ' + get_option('gir').to_string() + '\n'
output += '  Vala:         ' + get_option('vapi').to_string() + '\n'
output += '\n'
output += '  Prefix:       ' + get_option('prefix') + '\n'
message(output)

# Done
