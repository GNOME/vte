
EXTRA_DIST = iso2022.txt palette.c


# The library

headerdir = $(includedir)/vte-$(VTE_API_VERSION)/vte
header_HEADERS = vte.h vtebuffer.h vteenums.h vtepty.h vtetypebuiltins.h vteversion.h vteview.h

lib_LTLIBRARIES = libvte-@VTE_API_MAJOR_VERSION@.@VTE_API_MINOR_VERSION@.la

EXTRA_libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_SOURCES = \
	genkeysyms.py \
	keysyms.c \
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
	vteenums.h \
	vte-private.h \
	vteaccess.c \
	vteaccess.h \
	vteconv.c \
	vteconv.h \
	vtedraw.c \
	vtedraw.h \
	vteint.h \
	vtepalettedefs.h \
	vtepalettecss.h \
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
	vtetc.c \
	vtetc.h \
	vtetree.c \
	vtetree.h \
	vtetypebuiltins.c \
	vtetypebuiltins.h \
	vteunistr.c \
	vteunistr.h \
	vteversion.h
	$(NULL)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Vte-$(VTE_API_VERSION)\" \
	-DVTE_API_VERSION=\"$(VTE_API_VERSION)\" \
 	-DDATADIR='"$(datadir)"' \
	-DLIBEXECDIR='"$(libexecdir)"' \
	-DLOCALEDIR='"$(localedir)"' \
	-DVTE_COMPILATION \
	$(AM_CPPFLAGS)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_CFLAGS = $(VTE_CFLAGS) $(AM_CFLAGS)
libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_LIBADD = $(VTE_LIBS)
libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_LDFLAGS = \
  $(VTE_LDFLAGS) \
	-version-info $(LT_VERSION_INFO) \
	-export-symbols-regex "^vte_.*" \
	@LIBTOOL_EXPORT_OPTIONS@ @LIBTOOL_FLAGS@ \
	$(AM_LDFLAGS)


# Generated sources

VTESEQ_BUILTSOURCES = vteseq-list.h vteseq-2.c vteseq-n.c
BUILT_SOURCES = marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h vteresources.c $(VTESEQ_BUILTSOURCES)
MAINTAINERCLEANFILES = $(VTESEQ_BUILTSOURCES)
EXTRA_DIST += $(VTESEQ_BUILTSOURCES)
CLEANFILES = marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h stamp-vtetypebuiltins.h vteresources.c

marshal.c: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --body --internal $< > $@
	
marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --internal $< > $@

vtetypebuiltins.h: stamp-vtetypebuiltins.h
	@true
stamp-vtetypebuiltins.h: vtetypebuiltins.h.template vteenums.h vtepty.h
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter %.h,$^) > xgen-vtbh \
	&& (cmp -s xgen-vtbh vtetypebuiltins.h || cp xgen-vtbh vtetypebuiltins.h ) \
	&& rm -f xgen-vtbh \
	&& echo timestamp > $(@F)

vtetypebuiltins.c: vtetypebuiltins.c.template vteenums.h vtepty.h
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

# vte the application

bin_PROGRAMS = vte-@VTE_API_MAJOR_VERSION@.@VTE_API_MINOR_VERSION@

vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_SOURCES = \
	vteapp.c \
	debug.c \
	debug.h \
	$(NULL)
vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_CFLAGS = \
	$(VTE_CFLAGS) \
	$(AM_CFLAGS)
vte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_LDADD = \
	libvte-@VTE_API_MAJOR_VERSION@.@VTE_API_MINOR_VERSION@.la \
	$(VTE_LIBS)

# introspection

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
include $(INTROSPECTION_MAKEFILE)

Vte-@VTE_GIR_VERSION@.gir: libvte-@VTE_API_MAJOR_VERSION@.@VTE_API_MINOR_VERSION@.la

Vte_@VTE_GIR_VERSION_U@_gir_EXPORT_PACKAGES = vte-$(VTE_API_VERSION)
Vte_@VTE_GIR_VERSION_U@_gir_SCANNERFLAGS = --c-include "vte/vte.h"
Vte_@VTE_GIR_VERSION_U@_gir_INCLUDES = Pango-1.0 Gdk-$(GTK_API_VERSION) Gtk-$(GTK_API_VERSION)
Vte_@VTE_GIR_VERSION_U@_gir_CFLAGS = $(VTE_CFLAGS) -DVTE_COMPILATION -I$(top_srcdir) -I$(top_builddir)
Vte_@VTE_GIR_VERSION_U@_gir_LIBS = libvte-@VTE_API_MAJOR_VERSION@.@VTE_API_MINOR_VERSION@.la
Vte_@VTE_GIR_VERSION_U@_gir_FILES = \
	vte.c \
	vte.h \
	vtepty.h \
	pty.c \
	vtetypebuiltins.c \
	vtetypebuiltins.h \
	$(NULL)

INTROSPECTION_GIRS += Vte-$(VTE_GIR_VERSION).gir

girdir = $(datadir)/gir-1.0
nodist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
nodist_typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(nodist_gir_DATA) $(nodist_typelib_DATA)

if ENABLE_VAPIGEN
include $(VAPIGEN_MAKEFILE)

VAPIGEN_VAPIS = Vte-$(VTE_VAPI_VERSION).vapi

Vte-@VTE_VAPI_VERSION@.vapi: Vte-$(VTE_GIR_VERSION).gir
Vte_@VTE_VAPI_VERSION_U@_vapi_DEPS = \
	glib-2.0 \
	gio-2.0 \
	pango \
	gdk-$(GTK_API_VERSION) \
	gtk+-$(GTK_API_VERSION) \
	$(NULL)
Vte_@VTE_VAPI_VERSION_U@_vapi_METADATADIRS = $(srcdir)
Vte_@VTE_VAPI_VERSION_U@_vapi_FILES = Vte-$(VTE_GIR_VERSION).gir

#vapidir = $(datadir)/vala/vapi
vapidir = $(VAPIGEN_VAPIDIR)
nodist_vapi_DATA = $(VAPIGEN_VAPIS)

CLEANFILES += $(nodist_vapi_DATA)

endif # ENABLE_VAPIGEN

endif # HAVE_INTROSPECTION

# Misc unit tests and utilities

noinst_PROGRAMS = interpret slowcat
noinst_SCRIPTS = decset osc window
EXTRA_DIST += $(noinst_SCRIPTS)

TEST_SH = check-doc-syntax.sh
EXTRA_DIST += $(TEST_SH)

check_PROGRAMS = dumpkeys iso2022 reflect-text-view reflect-vte mev ssfe table trie xticker vteconv vtetc
TESTS = table trie $(TEST_SH)

AM_CFLAGS = $(GLIB_CFLAGS)
LDADD = $(GLIB_LIBS)

reflect_text_view_CPPFLAGS = -DUSE_TEXT_VIEW
reflect_text_view_CFLAGS = $(VTE_CFLAGS)
reflect_text_view_SOURCES = reflect.c
reflect_text_view_LDADD = $(VTE_LIBS)

reflect_vte_CPPFLAGS = -DUSE_VTE
reflect_vte_CFLAGS = $(VTE_CFLAGS)
reflect_vte_SOURCES = reflect.c
reflect_vte_LDADD = libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@.la $(VTE_LIBS)

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
	vteresources.c \
	vtetc.c \
	vtetc.h \
	vtetree.c \
	vtetree.h \
	interpret.c
interpret_CPPFLAGS = \
	-DINTERPRET_MAIN \
	-DVTE_API_VERSION=\"$(VTE_API_VERSION)\" \
	$(AM_CPPFLAGS)
interpret_CFLAGS = $(GTK_CFLAGS)
interpret_LDADD = $(GTK_LIBS)

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
	vtetc.c \
	vtetc.h \
	vtetree.c \
	vtetree.h
iso2022_CPPFLAGS = -DISO2022_MAIN
iso2022_CFLAGS = $(GTK_CFLAGS)
iso2022_LDADD = $(GTK_LIBS)

slowcat_SOURCES = \
	slowcat.c

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
	vtetc.c \
	vtetc.h
table_CPPFLAGS = -DTABLE_MAIN
table_CFLAGS = $(VTE_CFLAGS)
table_LDADD = $(VTE_LIBS)

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
	vtetc.c \
	vtetc.h 
trie_CPPFLAGS = -DTRIE_MAIN
trie_CFLAGS = $(VTE_CFLAGS)
trie_LDADD = $(VTE_LIBS)

vtetc_SOURCES = vtetc.c vtetc.h debug.c debug.h
vtetc_CPPFLAGS = -DTERMCAP_MAIN

vteconv_SOURCES = buffer.h debug.c debug.h vteconv.c vteconv.h
vteconv_CPPFLAGS = -DVTECONV_MAIN
vteconv_CFLAGS = $(VTE_CFLAGS)
vteconv_LDADD = $(VTE_LIBS)

dumpkeys_SOURCES = dumpkeys.c
mev_SOURCES = mev.c
ssfe_SOURCES = ssfe.c
xticker_SOURCES = xticker.c

#

-include $(top_srcdir)/git.mk
