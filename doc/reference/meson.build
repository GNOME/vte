private_headers = [
  'buffer.h',
  'caps.hh',
  'cell.hh',
  'config.h',
  'debug.h',
  'iso2022.h',
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
  '--ignore-decorators=_VTE_GNUC_NONNULL\s*\([^)]*\)',
]

glib_prefix = glib_dep.get_pkgconfig_variable('prefix')
gtk_prefix = gtk_dep.get_pkgconfig_variable('prefix')

fixxref_args = [
  '--html-dir=' + (vte_prefix / gnome.gtkdoc_html_dir(vte_api_name)),
  '--extra-dir=' + (glib_prefix / gnome.gtkdoc_html_dir('glib')),
  '--extra-dir=' + (glib_prefix / gnome.gtkdoc_html_dir('gio')),
  '--extra-dir=' + (gtk_prefix / gnome.gtkdoc_html_dir('gdk')),
  '--extra-dir=' + (gtk_prefix / gnome.gtkdoc_html_dir('gdk-pixbuf')),
  '--extra-dir=' + (gtk_prefix / gnome.gtkdoc_html_dir('gtk')),
]

version_conf = configuration_data()
version_conf.set('VERSION', vte_version)

content_files = configure_file(
  input: 'version.xml.in',
  output: '@BASENAME@',
  configuration: version_conf
)

gnome.gtkdoc(
  meson.project_name(),
  main_xml: meson.project_name() + '-docs.xml',
  module_version: vte_api_version,
  src_dir: [src_inc, vte_inc],
  ignore_headers: private_headers,
  include_directories: top_inc,
  dependencies: libvte_dep,
  c_args: '-DVTE_COMPILATION',
  namespace: meson.project_name(),
  scan_args: scan_args,
  mkdb_args: '--source-suffixes=h,hh,c,cc',
  fixxref_args: fixxref_args,
  gobject_typesfile: meson.project_name() + '.types',
  content_files: content_files,
  install: true,
)
