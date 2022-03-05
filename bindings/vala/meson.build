# Copyright © 2018, 2019 Iñigo Martínez
# Copyright © 2019 Christian Persch
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

assert(get_option('gir'), 'gir is required for vala support')

add_languages('vala', required: true)

valac = meson.get_compiler('vala')
assert(valac.version().version_compare('>= 0.24.0'), 'vala >= 0.24 required')

posix_dep = valac.find_library('posix')

# Vala test application

vapp_resource_data = files(
  'app.ui',
  'search-popover.ui',
)

vapp_resource_sources = gnome.compile_resources(
  'appresources',
  'app.gresource.xml',
  c_name: 'app',
  dependencies: vapp_resource_data,
  export: true,
)

vapp_sources = vapp_resource_sources + files(
  'config.vapi',
  'app.vala'
)

vapp_cflags = [
  '-Wno-unused-but-set-variable',
  '-Wno-unused-variable',
]

vapp_valaflags = [
  '--enable-deprecated'
]

if valac.version().version_compare('>= 0.31.1')
  vapp_valaflags += '--disable-since-check'
endif

vapp_incs = [
  top_inc,
  src_inc,
  vte_inc,
]

if get_option('gtk3')
  libvte_gtk3_vapi_deps = [
    'gio-2.0',
    'glib-2.0',
    'pango',
    'gtk+-3.0',
  ]

  libvte_gtk3_vapi_dep = gnome.generate_vapi(
    vte_gtk3_api_name,
    sources: libvte_gtk3_gir[0],
    packages: libvte_gtk3_vapi_deps,
    install: true,
  )

  if gtk3_dep.version().version_compare('>= 3.16')
    vapp_valaflags += '--define=GTK_3_16'
  endif

  vapp_deps = [
    gio_dep,
    glib_dep,
    gtk3_dep,
    libvte_gtk3_vapi_dep,
    posix_dep,
  ]

  vapp = executable(
    'vala-test',
    sources: vapp_sources,
    include_directories: vapp_incs,
    dependencies: vapp_deps,
    c_args: vapp_cflags,
    vala_args: vapp_valaflags,
    install: false,
  )
endif

if get_option('gtk4')
  libvte_gtk4_vapi_deps = [
    'gio-2.0',
    'glib-2.0',
    'graphene-1.0',
    'pango',
    'gtk4',
  ]

  libvte_gtk4_vapi_dep = gnome.generate_vapi(
    vte_gtk4_api_name,
    sources: libvte_gtk4_gir[0],
    packages: libvte_gtk4_vapi_deps,
    install: true,
  )
endif
