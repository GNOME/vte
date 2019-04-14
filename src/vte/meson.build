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

vte_inc = include_directories('.')

libvte_common_enum_headers = files(
  # These files contain enums to be extracted by glib-mkenums
  'vtedeprecated.h',
  'vteenums.h',
)

libvte_gtk3_enum_sources = gnome.mkenums(
  'vtetypebuiltins.h',
  sources: libvte_common_enum_headers,
  c_template: '../vtetypebuiltins.cc.template',
  h_template: '../vtetypebuiltins.h.template',
  install_header: true,
  install_dir: vte_includedir / vte_gtk3_api_path
)

libvte_common_public_headers = files(
  'vte.h',
  'vtedeprecated.h',
  'vteenums.h',
  'vteglobals.h',
  'vtemacros.h',
  'vtepty.h',
  'vteregex.h',
  'vteterminal.h',
)

vteversion_conf = configuration_data()
vteversion_conf.set('VTE_MAJOR_VERSION', vte_major_version)
vteversion_conf.set('VTE_MINOR_VERSION', vte_minor_version)
vteversion_conf.set('VTE_MICRO_VERSION', vte_micro_version)

libvte_version_headers = configure_file(
  input: 'vteversion.h.in',
  output: '@BASENAME@',
  configuration: vteversion_conf,
  install: false,
)

if get_option('gtk3')
  libvte_gtk3_public_headers = libvte_common_public_headers + [libvte_version_headers]

  install_headers(
    libvte_gtk3_public_headers,
    subdir: vte_gtk3_api_path
  )

  # BUG! Due to meson bug, this header cannot be installed with the rule above. Instead,
  # use the install_header attribute in the mkenums call, and add the header afterwards
  # to the list.
  libvte_gtk3_public_headers += libvte_gtk3_enum_sources[1]

endif
