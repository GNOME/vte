EXTRA_DIST = vte.defs vte.override

EXTRA_PROGRAMS = vtemodule.so

if BUILD_PYTHON_BINDINGS
pythonsiteexecdir = $(pyexecdir)/gtk-2.0
pythonsiteexec_PROGRAMS = @PYTHONMODULES@
endif

INCLUDES = @VTE_CFLAGS@ @X_CFLAGS@ @PYTHON_INCLUDES@ @PYGTK_CFLAGS@

CLEANFILES = vte.c

vtemodule_so_SOURCES = vtemodule.c vte.c
LDFLAGS = -shared -module -avoid-version -export-symbols-regex initvte
LDADD = $(top_builddir)/src/libvte.la @LIBS@ @PYGTK_LIBS@ @VTE_LIBS@ @X_LIBS@

vte.c: vte.defs vte.override
	pygtk-codegen-2.0 -p py$(PACKAGE) -o vte.override --register @PYGTK_DATADIR@/pygtk/2.0/defs/gtk-types.defs --register @PYGTK_DATADIR@/pygtk/2.0/defs/gdk-types.defs --register @PYGTK_DATADIR@/pygtk/2.0/defs/pango-types.defs $< > $@
