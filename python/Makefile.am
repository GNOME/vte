EXTRA_DIST = vte.defs vte.override

PYTHON_MODULES = vtemodule.la

if BUILD_PYTHON_MODULES
pythonsiteexecdir = $(pyexecdir)/gtk-2.0
pythonsiteexec_LTLIBRARIES = $(PYTHON_MODULES)

CLEANFILES = vte.c

vtemodule_la_CFLAGS = @VTE_CFLAGS@ @X_CFLAGS@ @PYTHON_INCLUDES@ @PYGTK_CFLAGS@
vtemodule_la_SOURCES = vtemodule.c vte.c
vtemodule_la_LIBADD = $(top_builddir)/src/libvte.la @LIBS@ @PYGTK_LIBS@ @VTE_LIBS@ @X_LIBS@
vtemodule_la_LDFLAGS = -module -avoid-version -export-symbols-regex=initvte

vte.c: vte.defs vte.override
	pygtk-codegen-2.0 -p py$(PACKAGE) -o vte.override --register @PYGTK_DATADIR@/pygtk/2.0/defs/gtk-types.defs --register @PYGTK_DATADIR@/pygtk/2.0/defs/gdk-types.defs --register @PYGTK_DATADIR@/pygtk/2.0/defs/pango-types.defs $< > $@

endif
