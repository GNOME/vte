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

app_sources = config_sources + glib_glue_sources + libc_glue_sources + pango_glue_sources + pcre2_glue_sources + refptr_sources + std_glue_sources + files(
  'app.cc',
)

app_common_cppflags = [
  '-DG_LOG_DOMAIN="VTE"',
  '-DLOCALEDIR="@0@"'.format(vte_prefix / vte_localedir),
  '-DVTE_DISABLE_DEPRECATION_WARNINGS',
]

app_common_deps = [
  pcre2_dep,
]

if get_option('gtk3')

  app_gtk3_resource_data = files(
    'appmenu-gtk3.ui',
    'search-popover-gtk3.ui',
    'window-gtk3.ui',
  )

  app_gtk3_resource_sources = gnome.compile_resources(
    'resources-gtk3.cc',
    'app-gtk3.gresource.xml',
    c_name: 'app',
    dependencies: app_gtk3_resource_data,
  )

  app_gtk3_sources = app_sources + app_gtk3_resource_sources
  app_gtk3_cppflags = app_common_cppflags + gtk3_version_cppflags
  app_gtk3_deps = app_common_deps + [libvte_gtk3_dep]

  app_gtk3 = executable(
    'vte-' + vte_gtk3_api_version,
    app_gtk3_sources,
    dependencies: app_gtk3_deps,
    cpp_args: app_gtk3_cppflags,
    include_directories: top_inc,
    install: true,
  )
endif
