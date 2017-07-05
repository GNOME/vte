vte_xml = 'vte.xml'
vte_glade_xml = vte_api_name + '.xml'

vte_conf = configuration_data()
vte_conf.set('VTE_API_VERSION', vte_api_version)
vte_conf.set('VERSION', vte_version)

vte_glade_in = configure_file(
  input: vte_xml + '.in.in',
  output: vte_glade_xml + '.in',
  configuration: vte_conf,
)

intltool_cache = po_dir / '.intltool-merge-cache'

vte_glade = custom_target(
  vte_glade_xml,
  input: vte_glade_in,
  output: '@BASENAME@',
  command: [find_program('intltool-merge'), '-x', '-u', '-c', intltool_cache, po_dir, '@INPUT@', '@OUTPUT@'],
  install: true,
  install_dir: glade_catalogdir,
)

run_target(
  'validate',
  command: [find_program('xmllint'), '--dtdvalid', glade_catalog_dtd, '--noout', vte_glade]
)

icon_sizes = [
  '16x16',
  '22x22',
]

icon = 'widget-vte-terminal.png'

sources = []
renames = []
foreach icon_size: icon_sizes
  sources += ['_'.join(['hicolor', 'actions', icon_size, icon])]
  renames += [icon_size / 'actions' / icon]
endforeach

install_data(
  sources,
  rename: renames,
  install_dir: glade_pixmapdir / 'hicolor',
)
