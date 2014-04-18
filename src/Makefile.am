bin_PROGRAMS =
noinst_PROGRAMS =

CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
EXTRA_DIST =

# The library

headerdir = $(includedir)/vte-$(VTE_API_VERSION)/vte
header_HEADERS = vte.h vtepty.h vtetypebuiltins.h vteversion.h

lib_LTLIBRARIES = libvte-@VTE_API_MAJOR_VERSION@.@VTE_API_MINOR_VERSION@.la

EXTRA_libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_SOURCES = \
	marshal.list \
	mkunitables.sh \
	unitable.CNS11643 \
	unitable.CP437 \
	unitable.GB12345 \
	unitable.GB2312 \
	unitable.JIS0201 \
	unitable.JIS0208 \
	unitable.JIS0212 \
	unitable.KSX1001 \
	vteseq-2.gperf \
	vteseq-2.c \
	vteseq-n.gperf \
	vteseq-n.c \
	vtetypebuiltins.c.template \
	vtetypebuiltins.h.template \
	$(NULL)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_SOURCES = \
	box_drawing.h \
	buffer.h \
	caps.c \
	caps.h \
	debug.c \
	debug.h \
	iso2022.c \
	iso2022.h \
	keymap.c \
	keymap.h \
	marshal.c \
	marshal.h \
	matcher.c \
	matcher.h \
	pty.c \
	ring.c \
	ring.h \
	table.c \
	table.h \
	trie.c \
	trie.h \
	vte.c \
	vte.h \
	vte-private.h \
	vteaccess.c \
	vteaccess.h \
	vteconv.c \
	vteconv.h \
	vtedraw.c \
	vtedraw.h \
	vteint.h \
	vtepty.h \
	vtepty-private.h \
	vteresources.c \
	vterowdata.c \
	vterowdata.h \
	vteseq.c \
	vteseq-list.h \
	vtestream.c \
	vtestream.h \
	vtestream-base.h \
	vtestream-file.h \
	vteti.c \
	vteti.h \
	vtetivars.h \
	vtetree.c \
	vtetree.h \
	vtetypebuiltins.c \
	vtetypebuiltins.h \
	vteunistr.c \
	vteunistr.h \
	vteutils.c \
	vteutils.h \
	vteversion.h
	$(NULL)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Vte\" \
	-DVTE_API_VERSION=\"$(VTE_API_VERSION)\" \
	-DDATADIR='"$(datadir)"' \
	-DLIBEXECDIR='"$(libexecdir)"' \
	-DLOCALEDIR='"$(localedir)"' \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-DVTE_COMPILATION \
	$(AM_CPPFLAGS)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_CFLAGS = \
	$(VTE_CFLAGS) \
	$(TERMINFO_CFLAGS) \
	$(AM_CFLAGS)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_LDFLAGS = \
	$(VTE_LDFLAGS) \
	-version-info $(LT_VERSION_INFO) \
	-export-symbols-regex "^vte_.*" \
	$(AM_LDFLAGS)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_LIBADD = \
	$(VTE_LIBS) \
	$(TERMINFO_LIBS)

# Generated sources

VTESEQ_BUILTSOURCES = vteseq-list.h vteseq-2.c vteseq-n.c
BUILT_SOURCES = box_drawing.h marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h vteresources.c $(VTESEQ_BUILTSOURCES)
MAINTAINERCLEANFILES+ = $(VTESEQ_BUILTSOURCES)
EXTRA_DIST += $(VTESEQ_BUILTSOURCES) box_drawing.txt iso2022.txt vtetivars.awk
CLEANFILES += box_drawing.h marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h stamp-vtetypebuiltins.h vteresources.c

box_drawing.h: box_drawing.txt
	$(AM_V_GEN) $(srcdir)/box_drawing_generate.sh < $< > $@

marshal.c: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --body --internal $< > $@

marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --internal $< > $@

vtetypebuiltins.h: stamp-vtetypebuiltins.h
	@true
stamp-vtetypebuiltins.h: vtetypebuiltins.h.template vte.h vtepty.h
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter %.h,$^) > xgen-vtbh \
	&& (cmp -s xgen-vtbh vtetypebuiltins.h || cp xgen-vtbh vtetypebuiltins.h ) \
	&& rm -f xgen-vtbh \
	&& echo timestamp > $(@F)

vtetypebuiltins.c: vtetypebuiltins.c.template vte.h vtepty.h
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter %.h,$^) > xgen-vtbc \
	&& (cmp -s xgen-vtbc vtetypebuiltins.c || cp xgen-vtbc vtetypebuiltins.c ) \
	&& rm -f xgen-vtbc

vteseq-2.c: vteseq-2.gperf
	$(AM_V_GEN) $(top_srcdir)/missing --run gperf -m 100 --no-strlen $< > $@.tmp && \
	mv -f $@.tmp $@

vteseq-n.c: vteseq-n.gperf
	$(AM_V_GEN) $(top_srcdir)/missing --run gperf -m 100 $< > $@.tmp && \
	mv -f $@.tmp $@

vteseq-list.h: vteseq-2.gperf vteseq-n.gperf
	$(AM_V_GEN) echo '/* Generated file.  Do not edit */' > $@.tmp && \
	cat $^ | grep -v '^#' | grep '\<VTE_SEQUENCE_HANDLER\>'| sed 's/.*, //' | LANG=C sort -u >> $@.tmp && \
	mv -f $@.tmp $@

vteresources.c: vte.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies vte.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir . --generate --c-name vte $<

vtetivars.h: vtetivars.awk
	$(AM_V_GEN) $(top_srcdir)/missing --run gawk -f $< Caps > $@.tmp && \
	mv -f $@.tmp $@

# introspection

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
include $(INTROSPECTION_MAKEFILE)

Vte-@VTE_API_VERSION@.gir: libvte-$(VTE_API_VERSION).la
Vte_@VTE_API_VERSION_U@_gir_INCLUDES = Pango-1.0 Gdk-$(GTK_API_VERSION) Gtk-$(GTK_API_VERSION)
Vte_@VTE_API_VERSION_U@_gir_CFLAGS = $(VTE_CFLAGS) -DVTE_COMPILATION -I$(top_srcdir)
Vte_@VTE_API_VERSION_U@_gir_LIBS = libvte-$(VTE_API_VERSION).la
Vte_@VTE_API_VERSION_U@_gir_EXPORT_PACKAGES = vte-$(VTE_API_VERSION)
Vte_@VTE_API_VERSION_U@_gir_SCANNERFLAGS = --c-include "vte/vte.h"
Vte_@VTE_API_VERSION_U@_gir_FILES = \
	vte.c \
	vte.h \
	vtepty.h \
	pty.c \
	vtetypebuiltins.c \
	vtetypebuiltins.h \
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

# Test application

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
	--vapidir=. \
	--girdir=. \
	--pkg=posix \
	--pkg=gio-2.0 \
	--pkg=gtk+-$(GTK_API_VERSION) \
	--gresources app.gresource.xml \
	$(AM_VALAFLAGS)

vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_CPPFLAGS = \
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

CLEANFILES += \
	app.c \
	appresources.c \
	$(NULL)

EXTRA_DIST += \
        app.gresource.xml \
	app.ui \
	$(NULL)

appresources.c: app.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/app.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name app $<

endif # ENABLE_TEST_APPLICATION

# Misc unit tests and utilities

noinst_PROGRAMS += interpret slowcat
noinst_SCRIPTS = decset osc window
EXTRA_DIST += $(noinst_SCRIPTS)

TEST_SH = \
	check-doc-syntax.sh \
	test-vte-sh.sh \
	$(NULL)
EXTRA_DIST += $(TEST_SH)

check_PROGRAMS = dumpkeys iso2022 reflect-text-view reflect-vte mev table trie xticker vteconv vteti
TESTS = table trie $(TEST_SH)

reflect_text_view_CPPFLAGS = -DUSE_TEXT_VIEW $(AM_CPPFLAGS)
reflect_text_view_CFLAGS = $(VTE_CFLAGS) $(AM_CFLAGS)
reflect_text_view_SOURCES = reflect.c
reflect_text_view_LDADD = $(VTE_LIBS)

reflect_vte_CPPFLAGS = -DUSE_VTE $(AM_CPPFLAGS)
reflect_vte_CFLAGS = $(VTE_CFLAGS) $(AM_CFLAGS)
reflect_vte_SOURCES = reflect.c
reflect_vte_LDADD = libvte-$(VTE_API_VERSION).la $(VTE_LIBS)

interpret_SOURCES = \
	buffer.h \
	caps.c \
	caps.h \
	debug.c \
	debug.h \
	iso2022.c \
	iso2022.h \
	matcher.c \
	matcher.h \
	table.c \
	table.h \
	trie.c \
	trie.h \
	vteconv.c \
	vteconv.h \
	vteti.c \
	vteti.h \
	vtetivars.h \
	vtetree.c \
	vtetree.h \
	interpret.c
interpret_CPPFLAGS = \
	-DINTERPRET_MAIN \
	-DVTE_API_VERSION=\"$(VTE_API_VERSION)\" \
	$(AM_CPPFLAGS)
interpret_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GTK_CFLAGS) \
	$(TERMINFO_CFLAGS) \
	$(AM_CFLAGS)
interpret_LDADD = \
	$(GLIB_LIBS) \
	$(GOBJECT_LIBS) \
	$(TERMINFO_LIBS)

iso2022_SOURCES = \
	buffer.h \
	caps.c \
	caps.h \
	debug.c \
	debug.h \
	iso2022.c \
	iso2022.h \
	matcher.c \
	matcher.h \
	table.c \
	table.h \
	trie.c \
	trie.h \
	vteconv.c \
	vteconv.h \
	vteti.c \
	vteti.h \
	vtetivars.h \
	vtetree.c \
	vtetree.h
iso2022_CPPFLAGS = \
	-DISO2022_MAIN \
	$(AM_CPPFLAGS)
iso2022_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(TERMINFO_CFLAGS) \
	$(AM_CFLAGS)
iso2022_LDADD = \
	$(GLIB_LIBS) \
	$(GOBJECT_LIBS) \
	$(TERMINFO_LIBS)

slowcat_SOURCES = \
	slowcat.c \
	$(NULL)
slowcat_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
slowcat_LDADD = $(GLIB_LIBS)

table_SOURCES = \
	buffer.h \
	caps.c \
	caps.h \
	debug.c \
	debug.h \
	matcher.c \
	matcher.h \
	table.c \
	table.h \
	trie.c \
	trie.h \
	vteconv.c \
	vteconv.h \
	vteti.c \
	vteti.h \
	vtetivars.h \
	$(NULL)
table_CPPFLAGS = \
	-DTABLE_MAIN \
	$(AM_CPPFLAGS)
table_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(TERMINFO_CFLAGS) \
	$(AM_CFLAGS)
table_LDADD = \
	$(GLIB_LIBS) \
	$(GOBJECT_LIBS) \
	$(TERMINFO_LIBS)

trie_SOURCES = \
	trie.c \
	trie.h \
	buffer.h \
	caps.c \
	caps.h \
	debug.c \
	debug.h \
	matcher.c \
	matcher.h \
	table.c \
	table.h \
	vteconv.c \
	vteconv.h \
	vteti.c \
	vteti.h \
	vtetivars.h \
	$(NULL)
trie_CPPFLAGS = \
	-DTRIE_MAIN \
	$(AM_CPPFLAGS)
trie_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(TERMINFO_CFLAGS) \
	$(AM_CFLAGS)
trie_LDADD = \
	$(GLIB_LIBS) \
	$(GOBJECT_LIBS) \
	$(TERMINFO_LIBS)

vteti_SOURCES = \
	debug.c \
	debug.h \
	vteti.c \
	vteti.h \
	vtetivars.h \
	$(NULL)
vteti_CPPFLAGS = \
	-DTERMINFO_MAIN \
	$(AM_CPPFLAGS)
vteti_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(TERMINFO_CFLAGS) \
	$(AM_CFLAGS)
vteti_LDADD = \
	$(GLIB_LIBS) \
	$(TERMINFO_LIBS)

vteconv_SOURCES = buffer.h debug.c debug.h vteconv.c vteconv.h
vteconv_CPPFLAGS = -DVTECONV_MAIN $(AM_CPPFLAGS)
vteconv_CFLAGS = $(VTE_CFLAGS) $(AM_CFLAGS)
vteconv_LDADD = $(VTE_LIBS)

dumpkeys_SOURCES = dumpkeys.c
dumpkeys_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
dumpkeys_LDADD = $(GLIB_LIBS)

mev_SOURCES = mev.c
mev_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
mev_LDADD = $(GLIB_LIBS)

xticker_SOURCES = xticker.c
xticker_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
xticker_LDADD = $(GLIB_LIBS)

# Shell

profiledir = $(sysconfdir)/profile.d
dist_profile_DATA = vte.sh

.DELETE_ON_ERROR:

-include $(top_srcdir)/git.mk
