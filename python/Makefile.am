EXTRA_PROGRAMS = vtemodule.so
EXTRA_DIST = vte.defs vte.override
pythonsiteexecdir = $(pyexecdir)
pythonsiteexec_PROGRAMS = @PYTHONMODULES@

CFLAGS = @CFLAGS@ @XFT_CFLAGS@ @GTK_CFLAGS@ @X_CFLAGS@ @PYTHON_INCLUDES@

CLEANFILES = vte.c

vtemodule_so_SOURCES = vtemodule.c vte.c
vtemodule_so_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ @PYGTK_CFLAGS@ -I$(includedir)/python@PYTHONREV@
vtemodule_so_LDFLAGS = -shared
vtemodule_so_LDADD = $(top_builddir)/src/libvte.la @LIBS@ @PYGTK_LIBS@ @XFT_LIBS@ @GTK_LIBS@ @X_LIBS@

vte.c: vte.defs vte.override
	pygtk-codegen-2.0 -p py$(PACKAGE) -o vte.override --register @PYGTK_DATADIR@/pygtk/2.0/defs/gtk-types.defs --register @PYGTK_DATADIR@/pygtk/2.0/defs/gdk-types.defs --register @PYGTK_DATADIR@/pygtk/2.0/defs/pango-types.defs $< > $@
