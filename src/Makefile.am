bin_PROGRAMS =
noinst_PROGRAMS = testvte

CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
EXTRA_DIST =

# The library

headerdir = $(includedir)/vte-$(VTE_API_VERSION)/vte
header_HEADERS = \
	vte/vte.h \
	vte/vtedeprecated.h \
	vte/vteenums.h \
	vte/vteglobals.h \
	vte/vtemacros.h \
	vte/vtepty.h \
	vte/vteregex.h \
	vte/vteterminal.h \
	$(NULL)

nodist_header_HEADERS = \
	vte/vtetypebuiltins.h \
	vte/vteversion.h \
	$(NULL)

lib_LTLIBRARIES = libvte-@VTE_API_MAJOR_VERSION@.@VTE_API_MINOR_VERSION@.la

EXTRA_libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_SOURCES = \
	box_drawing_generate.sh \
	marshal.list \
	vteseq-n.gperf \
	vteseq-n.cc \
	vtetypebuiltins.cc.template \
	vtetypebuiltins.h.template \
	$(NULL)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_SOURCES = \
	vte/vte.h \
	vte/vtedeprecated.h \
	vte/vteenums.h \
	vte/vteglobals.h \
	vte/vtemacros.h \
	vte/vtepty.h \
	vte/vteregex.h \
	vte/vteterminal.h \
	buffer.h \
	caps.cc \
	caps.h \
	debug.cc \
	debug.h \
	iso2022.cc \
	iso2022.h \
	keymap.cc \
	keymap.h \
	matcher.cc \
	matcher.h \
	pty.cc \
	ring.cc \
	ring.h \
	table.cc \
	table.h \
	vte.cc \
	vteaccess.cc \
	vteaccess.h \
	vteconv.cc \
	vteconv.h \
	vtedefines.hh \
	vtedraw.cc \
	vtedraw.hh \
	vtegtk.cc \
	vtegtk.hh \
	vteinternal.hh \
	vtepcre2.h \
	vtepty-private.h \
	vteregex.cc \
	vteregexinternal.hh \
	vterowdata.cc \
	vterowdata.h \
	vteseq.cc \
	vteseq-list.h \
	vtestream.cc \
	vtestream.h \
	vtestream-base.h \
	vtestream-file.h \
	vtetree.cc \
	vtetree.h \
	vtetypes.cc \
	vtetypes.hh \
	vteunistr.cc \
	vteunistr.h \
	vteutils.cc \
	vteutils.h \
	$(NULL)

nodist_libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_SOURCES = \
	box_drawing.h \
	marshal.cc \
	marshal.h \
	vteresources.cc \
	vtetypebuiltins.cc \
	vte/vtetypebuiltins.h \
	vte/vteversion.h \
	$(NULL)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Vte\" \
	-DVTE_API_VERSION=\"$(VTE_API_VERSION)\" \
	-DDATADIR='"$(datadir)"' \
	-DLIBEXECDIR='"$(libexecdir)"' \
	-DLOCALEDIR='"$(localedir)"' \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-DVTE_COMPILATION \
	-I$(srcdir)/vte \
	-I$(builddir)/vte \
	$(AM_CPPFLAGS)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_CXXFLAGS = \
	$(VTE_CFLAGS) \
	$(AM_CXXFLAGS)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_LDFLAGS = \
	$(VTE_CXXLDFLAGS) \
	-version-info $(LT_VERSION_INFO) \
	$(AM_LDFLAGS)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_LIBADD = \
	$(VTE_LIBS)

# Generated sources

VTESEQ_BUILTSOURCES = vteseq-list.h vteseq-n.cc
BUILT_SOURCES = box_drawing.h marshal.cc marshal.h vtetypebuiltins.cc vte/vtetypebuiltins.h vteresources.cc $(VTESEQ_BUILTSOURCES)
MAINTAINERCLEANFILES += $(VTESEQ_BUILTSOURCES)
EXTRA_DIST += $(VTESEQ_BUILTSOURCES) box_drawing.txt box_drawing_generate.sh iso2022.txt
CLEANFILES += box_drawing.h marshal.cc marshal.h vtetypebuiltins.cc vte/vtetypebuiltins.h stamp-vtetypebuiltins.h vteresources.cc

box_drawing.h: box_drawing.txt box_drawing_generate.sh
	$(AM_V_GEN) $(srcdir)/box_drawing_generate.sh < $< > $@

marshal.cc: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --body --internal $< > $@

marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --internal $< > $@

vte/vtetypebuiltins.h: stamp-vtetypebuiltins.h
	@true
stamp-vtetypebuiltins.h: vtetypebuiltins.h.template vte/vteenums.h vte/vtedeprecated.h
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter %.h,$^) > xgen-vtbh \
	&& (cmp -s xgen-vtbh vte/vtetypebuiltins.h || cp xgen-vtbh vte/vtetypebuiltins.h ) \
	&& rm -f xgen-vtbh \
	&& echo timestamp > $(@F)

vtetypebuiltins.cc: vtetypebuiltins.cc.template vte/vteenums.h vte/vtedeprecated.h
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter %.h,$^) > xgen-vtbc \
	&& (cmp -s xgen-vtbc vtetypebuiltins.cc || cp xgen-vtbc vtetypebuiltins.cc ) \
	&& rm -f xgen-vtbc

vteseq-n.cc: vteseq-n.gperf
	$(AM_V_GEN) $(top_srcdir)/missing --run gperf -m 100 $< > $@.tmp && \
	mv -f $@.tmp $@

vteseq-list.h: vteseq-n.gperf
	$(AM_V_GEN) echo '/* Generated file.  Do not edit */' > $@.tmp && \
	cat $^ | grep -v '^#' | grep '\<VTE_SEQUENCE_HANDLER\>'| sed 's/.*, //' | LANG=C sort -u >> $@.tmp && \
	mv -f $@.tmp $@

vteresources.cc: vte.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies vte.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir . --generate-source --c-name _vte $<

# introspection

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
include $(INTROSPECTION_MAKEFILE)

Vte-@VTE_API_VERSION@.gir: libvte-$(VTE_API_VERSION).la
Vte_@VTE_API_VERSION_U@_gir_INCLUDES = Pango-1.0 Gdk-$(GTK_API_VERSION) Gtk-$(GTK_API_VERSION)
Vte_@VTE_API_VERSION_U@_gir_CFLAGS = $(VTE_CFLAGS) -DVTE_COMPILATION -I$(builddir)/vte -I$(srcdir)/vte
Vte_@VTE_API_VERSION_U@_gir_LIBS = libvte-$(VTE_API_VERSION).la
Vte_@VTE_API_VERSION_U@_gir_EXPORT_PACKAGES = vte-$(VTE_API_VERSION)
Vte_@VTE_API_VERSION_U@_gir_SCANNERFLAGS = --c-include "vte/vte.h"
Vte_@VTE_API_VERSION_U@_gir_FILES = \
	$(header_HEADERS) \
	$(nodist_header_HEADERS) \
	vtegtk.cc \
	vtetypebuiltins.cc \
	pty.cc \
	$(NULL)

INTROSPECTION_GIRS += Vte-$(VTE_API_VERSION).gir

girdir = $(datadir)/gir-1.0
nodist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
nodist_typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(nodist_gir_DATA) $(nodist_typelib_DATA)

if ENABLE_VAPIGEN
include $(VAPIGEN_MAKEFILE)

VAPIGEN_VAPIS = vte-$(VTE_API_VERSION).vapi

vte-@VTE_API_VERSION@.vapi: Vte-$(VTE_API_VERSION).gir
vte_@VTE_API_VERSION_U@_vapi_DEPS = \
	glib-2.0 \
	gio-2.0 \
	pango \
	gdk-$(GTK_API_VERSION) \
	gtk+-$(GTK_API_VERSION) \
	$(NULL)
vte_@VTE_API_VERSION_U@_vapi_METADATADIRS = $(srcdir)
vte_@VTE_API_VERSION_U@_vapi_FILES = Vte-$(VTE_API_VERSION).gir

vapidir = $(datadir)/vala/vapi
nodist_vapi_DATA = $(VAPIGEN_VAPIS)

CLEANFILES += $(nodist_vapi_DATA)

endif # ENABLE_VAPIGEN

endif # HAVE_INTROSPECTION

# C Test application

testvte_SOURCES = \
	vteapp.c \
	vtepcre2.h \
	debug.c \
	debug.h \
	$(NULL)

testvte_CPPFLAGS = \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-DGDK_DISABLE_DEPRECATION_WARNINGS \
	-I$(srcdir)/vte \
	-I$(builddir)/vte \
	$(AM_CPPFLAGS)

testvte_CFLAGS = \
	$(VTE_CFLAGS) \
	$(AM_CFLAGS)

testvte_LDADD = \
	libvte-$(VTE_API_VERSION).la \
	$(VTE_LIBS)

# VALA Test application

if ENABLE_TEST_APPLICATION

bin_PROGRAMS += vte-@VTE_API_MAJOR_VERSION@.@VTE_API_MINOR_VERSION@

vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_SOURCES = \
	app.vala \
	config.vapi \
	vte-$(VTE_API_VERSION).vapi \
	$(NULL)

nodist_vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_SOURCES = \
	appresources.c \
	$(NULL)

vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_VALAFLAGS = \
	--target-glib=2.38 \
	--vapidir=. \
	--girdir=. \
	--pkg=posix \
	--pkg=gio-2.0 \
	--pkg=gtk+-$(GTK_API_VERSION) \
	--gresources app.gresource.xml \
	$(AM_VALAFLAGS)

vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_CPPFLAGS = \
	-I$(srcdir)/vte \
	-I$(builddir)/vte \
	$(AM_CPPFLAGS) \
	$(NULL)

vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_CFLAGS = \
	-Wno-unused-variable \
	-Wno-unused-but-set-variable \
	$(VTE_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(NULL)

vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_LDADD = \
	libvte-$(VTE_API_VERSION).la \
	$(VTE_LIBS) \
	$(NULL)

if HAVE_VALA_0_31
vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_VALAFLAGS += --disable-since-check
endif

if HAVE_GTK_3_16
vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_VALAFLAGS += -D GTK_3_16
endif

CLEANFILES += \
	app.c \
	appresources.c \
	$(NULL)

EXTRA_DIST += \
        app.gresource.xml \
	app.ui \
	search-popover.ui \
	$(NULL)

appresources.c: app.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/app.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name app $<

endif # ENABLE_TEST_APPLICATION

# Misc unit tests and utilities

noinst_PROGRAMS += interpret slowcat
noinst_SCRIPTS = decset osc window
EXTRA_DIST += $(noinst_SCRIPTS)

check_PROGRAMS = \
	dumpkeys \
	reflect-text-view \
	reflect-vte mev \
	table \
	xticker \
	vteconv \
	vtestream-file \
	test-vtetypes \
	$(NULL)

dist_check_SCRIPTS = \
	check-doc-syntax.sh \
	test-vte-sh.sh \
	$(NULL)

TESTS = \
	table \
	test-vtetypes \
	vteconv \
	vtestream-file \
	$(dist_check_SCRIPTS) \
	$(NULL)

TESTS_ENVIRONMENT = \
	srcdir="$(srcdir)" \
	top_builddir="$(top_builddir)" \
	VTE_API_VERSION="$(VTE_API_VERSION)" \
	$(NULL)

reflect_text_view_CPPFLAGS = -DUSE_TEXT_VIEW -I$(srcdir) -I $(builddir) $(AM_CPPFLAGS)
reflect_text_view_CFLAGS = $(VTE_CFLAGS) $(AM_CFLAGS)
reflect_text_view_SOURCES = reflect.c
reflect_text_view_LDADD = $(VTE_LIBS)

reflect_vte_CPPFLAGS = -DUSE_VTE  -I$(srcdir) -I $(builddir) $(AM_CPPFLAGS)
reflect_vte_CFLAGS = $(VTE_CFLAGS) $(AM_CFLAGS)
reflect_vte_SOURCES = reflect.c
reflect_vte_LDADD = libvte-$(VTE_API_VERSION).la $(VTE_LIBS)

interpret_SOURCES = \
	buffer.h \
	caps.cc \
	caps.h \
	debug.cc \
	debug.h \
	iso2022.cc \
	iso2022.h \
	matcher.cc \
	matcher.h \
	table.cc \
	table.h \
	vteconv.cc \
	vteconv.h \
	vtetree.cc \
	vtetree.h \
	interpret.c
interpret_CPPFLAGS = \
	-DINTERPRET_MAIN \
	-DVTE_API_VERSION=\"$(VTE_API_VERSION)\" \
	-I$(srcdir) \
	-I$(builddir) \
	$(AM_CPPFLAGS)
interpret_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GTK_CFLAGS) \
	$(AM_CFLAGS)
interpret_CXXFLAGS = \
	$(GLIB_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GTK_CFLAGS) \
	$(AM_CXXFLAGS)
interpret_LDADD = \
	$(GLIB_LIBS) \
	$(GOBJECT_LIBS)

slowcat_SOURCES = \
	slowcat.c \
	$(NULL)
slowcat_CPPFLAGS = -I$(srcdir) -I$(builddir) $(AM_CPPFLAGS)
slowcat_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
slowcat_LDADD = $(GLIB_LIBS)

table_SOURCES = \
	buffer.h \
	caps.cc \
	caps.h \
	debug.cc \
	debug.h \
	matcher.cc \
	matcher.h \
	table.cc \
	table.h \
	vteconv.cc \
	vteconv.h \
	$(NULL)
table_CPPFLAGS = \
	-DTABLE_MAIN \
	-I$(srcdir) \
	-I$(builddir) \
	$(AM_CPPFLAGS)
table_CXXFLAGS = \
	$(GLIB_CFLAGS) \
	$(AM_CXXFLAGS)
table_LDADD = \
	$(GLIB_LIBS) \
	$(GOBJECT_LIBS)

test_vtetypes_SOURCES = \
	vtetypes.cc \
	vtetypes.hh \
	$(NULL)
test_vtetypes_CPPFLAGS = \
	-DMAIN \
	-I$(srcdir) \
	-I$(builddir) \
	$(AM_CPPFLAGS)
test_vtetypes_CXXFLAGS = \
	$(VTE_CFLAGS) \
	$(AM_CXXFLAGS)
test_vtetypes_LDADD = \
	$(VTE_LIBS) \
	$(NULL)

vtestream_file_SOURCES = \
	vtestream-base.h \
	vtestream-file.h \
	vtestream.cc \
	vtestream.h \
	vteutils.cc \
	vteutils.h \
	$(NULL)
vtestream_file_CPPFLAGS = \
	-DVTESTREAM_MAIN \
	-I$(srcdir) \
	-I$(builddir) \
	$(AM_CPPFLAGS)
vtestream_file_CXXFLAGS = \
	$(VTE_CFLAGS) \
	$(AM_CXXFLAGS)
vtestream_file_LDADD = \
	$(VTE_LIBS)

vteconv_SOURCES = buffer.h debug.cc debug.h vteconv.cc vteconv.h
vteconv_CPPFLAGS = -DVTECONV_MAIN -I$(srcdir) -I$(builddir) $(AM_CPPFLAGS)
vteconv_CXXFLAGS = $(VTE_CFLAGS) $(AM_CXXFLAGS)
vteconv_LDADD = $(VTE_LIBS)

dumpkeys_SOURCES = dumpkeys.c
dumpkeys_CPPFLAGS = -I$(srcdir) -I$(builddir) $(AM_CPPFLAGS)
dumpkeys_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
dumpkeys_LDADD = $(GLIB_LIBS)

mev_SOURCES = mev.c
mev_CPPFLAGS = -I$(srcdir) -I$(builddir) $(AM_CPPFLAGS)
mev_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
mev_LDADD = $(GLIB_LIBS)

xticker_SOURCES = xticker.c
xticker_CPPFLAGS = -I$(srcdir) -I$(builddir) $(AM_CPPFLAGS)
xticker_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
xticker_LDADD = $(GLIB_LIBS)

# Shell

profiledir = $(sysconfdir)/profile.d
dist_profile_DATA = vte.sh

.DELETE_ON_ERROR:

-include $(top_srcdir)/git.mk
