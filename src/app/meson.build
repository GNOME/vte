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

app_sources = config_sources + debug_sources + glib_glue_sources + libc_glue_sources + pango_glue_sources + pcre2_glue_sources + refptr_sources + std_glue_sources + vte_glue_sources + files(
  'app.cc',
)

app_common_cppflags = [
  '-DG_LOG_DOMAIN="VTE"',
  '-DLOCALEDIR="@0@"'.format(vte_prefix / vte_localedir),
  '-DVTE_DISABLE_DEPRECATION_WARNINGS',
]

app_common_deps = [
  fmt_dep,
  glib_dep,
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
  app_gtk3_cppflags = app_common_cppflags + gtk3_version_cppflags + ['-DVTE_GTK=3',]
  app_gtk3_deps = app_common_deps + [libvte_gtk3_dep]

  app_gtk3 = executable(
    'vte-' + vte_api_version,
    app_gtk3_sources,
    dependencies: app_gtk3_deps,
    cpp_args: app_gtk3_cppflags,
    include_directories: top_inc,
    install: not meson.is_subproject(),
  )
endif

if get_option('gtk4')

  app_gtk4_resource_data = files(
    'appmenu-gtk4.ui',
    'search-popover-gtk4.ui',
    'window-gtk4.ui',
  )

  app_gtk4_resource_sources = gnome.compile_resources(
    'resources-gtk4.cc',
    'app-gtk4.gresource.xml',
    c_name: 'app',
    dependencies: app_gtk4_resource_data,
  )

  app_gtk4_sources = app_sources + [app_gtk4_resource_sources,]
  app_gtk4_cppflags = app_common_cppflags + gtk4_version_cppflags + ['-DVTE_GTK=4',]
  app_gtk4_deps = app_common_deps + [libvte_gtk4_dep]

  app_gtk4 = executable(
    'vte-' + vte_api_version + '-gtk4',
    app_gtk4_sources,
    dependencies: app_gtk4_deps,
    cpp_args: app_gtk4_cppflags,
    include_directories: top_inc,
    install: not meson.is_subproject(),
  )
endif

# Desktop files

desktopdatadir = vte_datadir / 'applications'

if get_option('gtk3')
  conf = configuration_data()
  conf.set('name', 'VTE Terminal for GTK3')
  conf.set('exec', vte_gtk3_api_name)
  conf.set('gtk', '3')

  app_gtk3_desktop_name = 'org.gnome.Vte.App.Gtk3.desktop'

  app_gtk3_desktop = configure_file(
    configuration: conf,
    input: 'vte.desktop.in',
    install: true,
    install_dir: vte_prefix / desktopdatadir,
    output: app_gtk3_desktop_name,
  )

  meson.add_install_script(
    'meson_desktopfile.py',
    desktopdatadir,
    app_gtk3_desktop_name,
  )
endif

if get_option('gtk4')
  conf = configuration_data()
  conf.set('name', 'VTE Terminal for GTK4')
  conf.set('exec', vte_gtk4_api_name)
  conf.set('gtk', '4')

  app_gtk4_desktop_name = 'org.gnome.Vte.App.Gtk4.desktop'

  app_gtk4_desktop = configure_file(
    configuration: conf,
    input: 'vte.desktop.in',
    install: true,
    install_dir: vte_prefix / desktopdatadir,
    output: app_gtk4_desktop_name,
  )

  meson.add_install_script(
    'meson_desktopfile.py',
    desktopdatadir,
    app_gtk4_desktop_name,
  )
endif

# Install a symlink for xdg-terminal-exec

if get_option('gtk3')
  install_symlink(
    app_gtk3_desktop_name,
    install_dir: vte_datadir / 'xdg-terminals',
    install_tag: 'runtime',
    pointing_to: '..' / 'applications' / app_gtk3_desktop_name,
  )
endif

if get_option('gtk4')
  install_symlink(
    app_gtk4_desktop_name,
    install_dir: vte_datadir / 'xdg-terminals',
    install_tag: 'runtime',
    pointing_to: '..' / 'applications' / app_gtk4_desktop_name,
  )
endif
