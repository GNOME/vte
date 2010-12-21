
EXTRA_DIST = iso2022.txt


# The library

headerdir = $(includedir)/vte-$(VTE_API_VERSION)/vte
header_HEADERS = pty.h reaper.h vte.h vteaccess.h vtepty.h vtetypebuiltins.h vteversion.h vtedeprecated.h

lib_LTLIBRARIES = libvte@VTE_LIBRARY_SUFFIX_U@.la

EXTRA_libvte@VTE_LIBRARY_SUFFIX_U@_la_SOURCES = \
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

libvte@VTE_LIBRARY_SUFFIX_U@_la_SOURCES = \
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
	pty.h \
	reaper.c \
	reaper.h \
	ring.c \
	ring.h \
	table.c \
	table.h \
	trie.c \
	trie.h \
	vte.c \
	vte.h \
	vte-gtk-compat.h \
	vte-private.h \
	vteaccess.c \
	vteaccess.h \
	vtebg.c \
	vtebg.h \
	vteconv.c \
	vteconv.h \
	vtedraw.c \
	vtedraw.h \
	vtedeprecated.h \
	vteint.h \
	vtepty.h \
	vtepty-private.h \
	vteregex.c \
	vteregex.h \
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

libvte@VTE_LIBRARY_SUFFIX_U@_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Vte-$(VTE_API_VERSION)\" \
 	-DDATADIR='"$(datadir)"' \
	-DLIBEXECDIR='"$(libexecdir)"' \
	-DLOCALEDIR='"$(localedir)"' \
	-DTERMCAPDIR='"$(pkgdatadir)/termcap-$(VTE_API_VERSION)"' \
	-DVTE_COMPILATION \
	$(AM_CPPFLAGS)

libvte@VTE_LIBRARY_SUFFIX_U@_la_CFLAGS = $(VTE_CFLAGS) $(AM_CFLAGS)
libvte@VTE_LIBRARY_SUFFIX_U@_la_LIBADD = $(VTE_LIBS)
libvte@VTE_LIBRARY_SUFFIX_U@_la_LDFLAGS = \
  $(VTE_LDFLAGS) \
	-version-info $(LT_VERSION_INFO) \
	-export-symbols-regex "^vte_terminal_.*|^vte_pty_.*|^_vte_pty_.*|^vte_reaper_.*|_vte_debug_.*" \
	@LIBTOOL_EXPORT_OPTIONS@ @LIBTOOL_FLAGS@ \
	$(AM_LDFLAGS)


# Generated sources

VTESEQ_BUILTSOURCES = vteseq-list.h vteseq-2.c vteseq-n.c
BUILT_SOURCES = marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h $(VTESEQ_BUILTSOURCES)
MAINTAINERCLEANFILES = $(VTESEQ_BUILTSOURCES)
EXTRA_DIST += $(VTESEQ_BUILTSOURCES)
CLEANFILES = marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h stamp-vtetypebuiltins.h

marshal.c: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --body --internal $< > $@
	
marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --internal $< > $@

vtetypebuiltins.h: stamp-vtetypebuiltins.h
	@true
stamp-vtetypebuiltins.h: vtetypebuiltins.h.template vte.h vtepty.h vtedeprecated.h
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter %.h,$^) > xgen-vtbh \
	&& (cmp -s xgen-vtbh vtetypebuiltins.h || cp xgen-vtbh vtetypebuiltins.h ) \
	&& rm -f xgen-vtbh \
	&& echo timestamp > $(@F)

vtetypebuiltins.c: vtetypebuiltins.c.template vte.h vtepty.h vtedeprecated.h
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


# vte the application

bin_PROGRAMS = vte@VTE_LIBRARY_SUFFIX_U@

vte@VTE_LIBRARY_SUFFIX_U@_SOURCES = vteapp.c
vte@VTE_LIBRARY_SUFFIX_U@_CFLAGS = $(VTE_CFLAGS)
vte@VTE_LIBRARY_SUFFIX_U@_LDADD = libvte@VTE_LIBRARY_SUFFIX_U@.la $(VTE_LIBS)

# introspection

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION

Vte-@VTE_API_VERSION@.gir: libvte@VTE_LIBRARY_SUFFIX_U@.la
Vte_@VTE_API_VERSION_U@_gir_INCLUDES = Pango-1.0 Gdk-$(GTK_API_VERSION) Gtk-$(GTK_API_VERSION)
Vte_@VTE_API_VERSION_U@_gir_CFLAGS = $(VTE_CFLAGS) -DVTE_COMPILATION -I$(top_srcdir)
Vte_@VTE_API_VERSION_U@_gir_LIBS = libvte@VTE_LIBRARY_SUFFIX_U@.la
Vte_@VTE_API_VERSION_U@_gir_FILES = \
	vte.c \
	vte.h \
	vtepty.h \
	pty.c \
	vtetypebuiltins.c \
	vtetypebuiltins.h \
	$(NULL)

INTROSPECTION_GIRS += Vte-$(VTE_API_VERSION).gir

girdir = $(INTROSPECTION_GIRDIR)
nodist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(INTROSPECTION_TYPELIBDIR)
nodist_typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(nodist_gir_DATA) $(nodist_typelib_DATA)
endif


# Misc unit tests and utilities

noinst_PROGRAMS = interpret slowcat
noinst_SCRIPTS = decset osc window
EXTRA_DIST += $(noinst_SCRIPTS)

TEST_SH = check-doc-syntax.sh
EXTRA_DIST += $(TEST_SH)

check_PROGRAMS = dumpkeys iso2022 reaper reflect-text-view reflect-vte mev ssfe table trie xticker vteconv vtetc
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
reflect_vte_LDADD = libvte@VTE_LIBRARY_SUFFIX_U@.la $(VTE_LIBS)

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
	vtetc.c \
	vtetc.h \
	vtetree.c \
	vtetree.h \
	interpret.c
interpret_CPPFLAGS = \
 	-DINTERPRET_MAIN \
 	-DTERMCAPDIR='"$(pkgdatadir)/termcap-$(VTE_API_VERSION)"'
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

reaper_SOURCES = reaper.c reaper.h debug.c debug.h marshal.c marshal.h
reaper_CPPFLAGS =  -DREAPER_MAIN -DLOCALEDIR='"$(localedir)"'

vteconv_SOURCES = buffer.h debug.c debug.h vteconv.c vteconv.h
vteconv_CPPFLAGS = -DVTECONV_MAIN
vteconv_CFLAGS = $(VTE_CFLAGS)
vteconv_LDADD = $(VTE_LIBS)

dumpkeys_SOURCES = dumpkeys.c
mev_SOURCES = mev.c
ssfe_SOURCES = ssfe.c
xticker_SOURCES = xticker.c

# GSettings

# Don't use the automatic rules since the generated .enums.xml isn't very nice
# gsettings_ENUM_NAMESPACE = org.gnome.vte.v0
# gsettings_ENUM_FILES = vte.h

dist_gsettingsschema_DATA = org.gnome.vte.v0.enums.xml

#

-include $(top_srcdir)/git.mk
-include $(INTROSPECTION_MAKEFILE)
