sources = files('app.cc')

resource_data = files(
  'appmenu.ui',
  'search-popover.ui',
  'window.ui',
)

sources += gnome.compile_resources(
  'resources.cc',
  'app.gresource.xml',
  c_name: 'app',
  dependencies: resource_data,
)

deps = [
  libpcre_dep,
  libvte_dep,
]

cpp_flags = [
  '-DG_LOG_DOMAIN="Vte"',
  '-DLOCALEDIR="@0@"'.format(vte_prefix / vte_localedir),
]

executable(
  'vte-' + vte_api_version,
  sources,
  include_directories: top_inc,
  dependencies: libvte_dep,
  cpp_args: cpp_flags,
  install: true,
)
