EXTRA_DIST = vte.defs vte.override cat.py unrealize.py vte-demo.py

PYTHON_MODULES = vtemodule.la

if BUILD_PYTHON_MODULES
pythonsiteexecdir = $(pyexecdir)/gtk-2.0
pythonsiteexec_LTLIBRARIES = $(PYTHON_MODULES)

CLEANFILES = vte.c

vtemodule_la_CFLAGS = $(VTE_CFLAGS) $(X_CFLAGS) $(PYTHON_INCLUDES) $(PYGTK_CFLAGS)
vtemodule_la_SOURCES = vtemodule.c
vtemodule_la_LIBADD = $(top_builddir)/src/libvte.la $(LIBS) $(PYGTK_LIBS) $(VTE_LIBS) $(X_LIBS)
vtemodule_la_LDFLAGS = -module -avoid-version -export-symbols-regex initvte
nodist_vtemodule_la_SOURCES = vte.c

vte.c: vte.defs vte.override

.defs.c:
	(cd $(srcdir) \
	 && $(PYGTK_CODEGEN) \
            --register $(PYGTK_DEFSDIR)/pango-types.defs \
            --register $(PYGTK_DEFSDIR)/gdk-types.defs \
            --register $(PYGTK_DEFSDIR)/gtk-types.defs \
            --override $*.override \
            --prefix py$* $*.defs) > gen-$*.c \
        && cp gen-$*.c $*.c \
        && rm -f gen-$*.c

endif
