vte_inc = include_directories('.')

enum_headers = files(
  'vtedeprecated.h',
  'vteenums.h',
)

headers = enum_headers + files(
  'vteglobals.h',
  'vte.h',
  'vtemacros.h',
  'vtepty.h',
  'vteregex.h',
  'vteterminal.h',
)

install_headers(
  headers,
  subdir: vte_api_path
)

vteversion_conf = configuration_data()
vteversion_conf.set('VTE_MAJOR_VERSION', vte_major_version)
vteversion_conf.set('VTE_MINOR_VERSION', vte_minor_version)
vteversion_conf.set('VTE_MICRO_VERSION', vte_micro_version)

vteversion_header = configure_file(
  input: 'vteversion.h.in',
  output: '@BASENAME@',
  configuration: vteversion_conf,
  install: true,
  install_dir: vte_pkgincludedir,
)

enum_types = 'vtetypebuiltins'

vtetype_enums = gnome.mkenums(
  enum_types + '.h',
  sources: enum_headers,
  c_template: '../' + enum_types + '.cc.template',
  h_template: '../' + enum_types + '.h.template',
  install_header: true,
  install_dir: vte_pkgincludedir,
)
