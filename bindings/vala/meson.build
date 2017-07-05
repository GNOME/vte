deps = [
  'gio-2.0',
  'glib-2.0',
  'pango',
  gtk_dep_name,
]

libvte_vapi_dep = gnome.generate_vapi(
  vte_api_name,
  sources: libvte_gir[0],
  packages: deps,
  install: true,
)

# VALA Test application
sources = files('app.vala')

resource_data = files(
  'app.ui',
  'search-popover.ui',
)

sources += gnome.compile_resources(
  'appresources',
  'app.gresource.xml',
  c_name: 'app',
  dependencies: resource_data,
  export: true,
)

incs = [
  top_inc,
  src_inc,
  vte_inc,
]

deps = [
  config_dep,
  gio_dep,
  glib_dep,
  gtk_dep,
  libvte_vapi_dep,
  posix_dep,
]

c_flags = [
  '-Wno-unused-but-set-variable',
  '-Wno-unused-variable',
]

vala_flags = []

if valac_version.version_compare('>= 0.31.1')
  vala_flags += '--disable-since-check'
endif

if gtk_dep.version().version_compare('>= 3.16')
  vala_flags += '--define=GTK_3_16'
endif

executable(
  vte_api_name,
  sources: sources,
  include_directories: incs,
  dependencies: deps,
  c_args: c_flags,
  vala_args: vala_flags,
)
