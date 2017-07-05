gir_incs = [
  'Gdk-' + gtk_api_version,
  'Gtk-' + gtk_api_version,
  'Pango-1.0'
]

libvte_gir = gnome.generate_gir(
  libvte,
  sources: headers + [vteversion_header] + vtetype_enums + gir_sources,
  includes: gir_incs,
  dependencies: libvte_dep,
  extra_args: '-DVTE_COMPILATION',
  nsversion: vte_api_version,
  namespace: 'Vte',
  export_packages: vte_api_name,
  header: 'vte' / 'vte.h',
  install: true,
)
