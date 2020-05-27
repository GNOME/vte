# Copyright © 2018, 2019 Iñigo Martínez
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

# Meson insufficiency! Would be so much easier to just make this
# (vte_gtk3_sources - vte_gtk3_public_headers).filter('.h'), but
# but there is no array subtraction or filtering. Or just allow listing the
# headers that we DO want to get scanned.
# So instead just list all the headers here again... :-(

private_headers = [
  'buffer.h',
  'caps.hh',
  'cell.hh',
  'config.h',
  'debug.h',
  'keymap.h',
  'marshal.h',
  'modes.hh',
  'modes-ecma.hh',
  'modes-private.hh',
  'parser.hh',
  'parser-arg.hh',
  'parser-c01.hh',
  'parser-charset.hh',
  'parser-charset-tables.hh',
  'parser-cmd.hh',
  'parser-csi.hh',
  'parser-dcs.hh',
  'parser-esc.hh',
  'parser-glue.hh',
  'parser-osc.hh',
  'parser-reply.hh',
  'parser-string.hh',
  'ring.hh',
  'tabstops.hh',
  'vteconv.h',
  'vtedraw.h',
  'vteinternal.hh',
  'vterowdata.hh',
  'vtestream-base.h',
  'vtestream-file.h',
  'vtestream.h',
  'vtetypebuiltins.h',
  'vteunistr.h',
]

scan_args = [
  '--deprecated-guards="VTE_DISABLE_DEPRECATED"',
  '--ignore-decorators=_VTE_GNUC_NONNULL\s*\([^)]*\)|_VTE_CXX_NOEXCEPT',
]

glib_prefix = glib_dep.get_pkgconfig_variable('prefix')

version_conf = configuration_data()
version_conf.set('VERSION', vte_version)

content_files = configure_file(
  input: 'version.xml.in',
  output: '@BASENAME@',
  configuration: version_conf
)

if get_option('gtk3')
  gtk3_prefix = gtk3_dep.get_pkgconfig_variable('prefix')

  fixxref_args = [
    '--html-dir=' + (vte_prefix / gnome.gtkdoc_html_dir(vte_gtk3_api_name)),
    '--extra-dir=' + (glib_prefix / gnome.gtkdoc_html_dir('glib')),
    '--extra-dir=' + (glib_prefix / gnome.gtkdoc_html_dir('gio')),
    '--extra-dir=' + (gtk3_prefix / gnome.gtkdoc_html_dir('gdk')),
    '--extra-dir=' + (gtk3_prefix / gnome.gtkdoc_html_dir('gdk-pixbuf')),
    '--extra-dir=' + (gtk3_prefix / gnome.gtkdoc_html_dir('gtk')),
  ]

  gnome.gtkdoc(
    'vte',
    main_xml: 'vte-docs.xml',
    module_version: vte_api_version,
    src_dir: [src_inc, vte_inc],
    ignore_headers: private_headers,
    include_directories: top_inc,
    dependencies: libvte_gtk3_dep,
    c_args: '-DVTE_COMPILATION',
    namespace: 'vte',
    scan_args: scan_args,
    mkdb_args: '--source-suffixes=h,hh,c,cc',
    fixxref_args: fixxref_args,
    gobject_typesfile: 'vte.types',
    content_files: content_files,
    install: true,
  )
endif

if get_option('gtk4')
  assert(false, 'not yet supported')
endif
