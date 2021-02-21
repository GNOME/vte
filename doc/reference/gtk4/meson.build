# Copyright © 2020, 2021 Christian Persch
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

make_args_gtk4 = [
  '-f', meson.current_source_dir() / '..' / 'Makefile.docs',
  '--directory', meson.current_build_dir(),
  'srcdir=' + meson.current_source_dir(),
  'builddir=' + meson.current_build_dir(),
  'abs_top_srcdir=' + meson.source_root(),
  'abs_top_builddir=' + meson.build_root(),
  'datadir=' + vte_prefix / vte_datadir,
  'CAIRO_PREFIX=' + cairo_dep.get_pkgconfig_variable('prefix'),
  'GLIB_PREFIX=' + glib_dep.get_pkgconfig_variable('prefix'),
  'GTK_PREFIX=' + gtk4_dep.get_pkgconfig_variable('prefix'),
  'PANGO_PREFIX=' + pango_dep.get_pkgconfig_variable('prefix'),
  'CC=' + ' '.join(cc.cmd_array()),
  'VERSION=' + meson.project_version(),
  'VTE_API_VERSION=' + vte_api_version,
  'VTE_GTK=4',
  'VTE_LIB=' + libvte_gtk4.full_path(),
]

stamp = custom_target(
  'meson.stamp',
  build_by_default: true,
  capture: false,
  command: [make] + make_args_gtk4 + [
    'meson.stamp',
  ],
  depends: [
    libvte_gtk4,
  ],
  install: false,
  output: 'meson.stamp',
)

meson.add_install_script(
  make,
  make_args_gtk4,
  'install-data-local',
)

# Unfortunately, there's no way to hook up the 'clean-local' target
