
EXTRA_DIST = iso2022.txt


# The library

pkginclude_HEADERS = pty.h reaper.h vte.h vteaccess.h vtetypebuiltins.h vteversion.h

lib_LTLIBRARIES = libvte.la

EXTRA_libvte_la_SOURCES = \
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

libvte_la_SOURCES = \
	buffer.c \
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
	vte-private.h \
	vteaccess.c \
	vteaccess.h \
	vtebg.c \
	vtebg.h \
	vteconv.c \
	vteconv.h \
	vtedraw.c \
	vtedraw.h \
	vteint.h \
	vtepangocairo.c \
	vtepangocairo.h \
	vteregex.c \
	vteregex.h \
	vteseq.c \
	vteseq-list.h \
	vteskel.c \
	vteskel.h \
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

libvte_la_CPPFLAGS = \
	-DG_DISABLE_ASSERT \
	-DG_LOG_DOMAIN=\"Vte\" \
 	-DDATADIR='"$(datadir)"' \
	-DLIBEXECDIR='"$(libexecdir)"' \
	-DLOCALEDIR='"$(localedir)"' \
	-DVTE_COMPILATION
libvte_la_CFLAGS = $(VTE_CFLAGS)
libvte_la_LIBADD = $(VTE_LIBS)
libvte_la_LDFLAGS = \
	-version-info $(LIBVTE_LTVERSION) \
	-export-symbols-regex "^vte_terminal_.*|^_vte_pty_.*|^vte_reaper_.*|_vte_debug_.*" \
	@LIBTOOL_EXPORT_OPTIONS@ @LIBTOOL_FLAGS@


# Generated sources

VTESEQ_BUILTSOURCES = vteseq-list.h vteseq-2.c vteseq-n.c
BUILT_SOURCES = marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h $(VTESEQ_BUILTSOURCES)
MAINTAINERCLEANFILES = $(VTESEQ_BUILTSOURCES)
EXTRA_DIST += $(VTESEQ_BUILTSOURCES)
CLEANFILES = marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h stamp-vtetypebuiltins.h

marshal.c marshal.h: marshal.list
	$(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --body --internal $^ > marshal.c
	$(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --internal $^ > marshal.h

vtetypebuiltins.h: stamp-vtetypebuiltins.h
	@true
stamp-vtetypebuiltins.h: vtetypebuiltins.h.template vte.h
	$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-vtbh \
	&& (cmp -s xgen-vtbh vtetypebuiltins.h || cp xgen-vtbh vtetypebuiltins.h ) \
	&& rm -f xgen-vtbh \
	&& echo timestamp > $(@F)

vtetypebuiltins.c: vtetypebuiltins.c.template vte.h
	$(GLIB_MKENUMS) --template $< $(filter-out $<,$^) > xgen-vtbc \
	&& (cmp -s xgen-vtbc vtetypebuiltins.c || cp xgen-vtbc vtetypebuiltins.c ) \
	&& rm -f xgen-vtbc

vteseq-2.c: vteseq-2.gperf
	@echo creating $@
	$(top_srcdir)/missing --run gperf -m 100 --no-strlen $< > $@.tmp && \
	mv -f $@.tmp $@

vteseq-n.c: vteseq-n.gperf
	@echo creating $@
	$(top_srcdir)/missing --run gperf -m 100 $< > $@.tmp && \
	mv -f $@.tmp $@

vteseq-list.h: vteseq-2.gperf vteseq-n.gperf
	@echo creating $@
	echo '/* Generated file.  Do not edit */' > $@.tmp && \
	cat $^ | grep -v '^#' | grep '\<VTE_SEQUENCE_HANDLER\>'| sed 's/.*, //' | LANG=C sort -u >> $@.tmp && \
	mv -f $@.tmp $@


# vte the application

bin_PROGRAMS = vte

vte_SOURCES = vteapp.c
vte_CFLAGS = $(VTE_CFLAGS)
vte_LDADD = libvte.la $(VTE_LIBS)


# Misc unit tests and utilities

pkglib_PROGRAMS = interpret slowcat
pkglib_SCRIPTS = decset osc window
EXTRA_DIST += $(pkglib_SCRIPTS)

TEST_SH = check-doc-syntax.sh
EXTRA_DIST += $(TEST_SH)

check_PROGRAMS = buffer dumpkeys iso2022 pty reaper reflect-text-view reflect-vte ring mev ssfe table trie xticker vteconv vtetc
TESTS = buffer ring table trie $(TEST_SH)

AM_CFLAGS = $(GLIB_CFLAGS) $(GOBJECT_CFLAGS)
LDADD = $(GLIB_LIBS) $(GOBJECT_LIBS)

reflect_text_view_CPPFLAGS = -DUSE_TEXT_VIEW
reflect_text_view_CFLAGS = $(VTE_CFLAGS)
reflect_text_view_SOURCES = reflect.c
reflect_text_view_LDADD = $(VTE_LIBS)

reflect_vte_CPPFLAGS = -DUSE_VTE
reflect_vte_CFLAGS = $(VTE_CFLAGS)
reflect_vte_SOURCES = reflect.c
reflect_vte_LDADD = libvte.la $(VTE_LIBS)

interpret_SOURCES = \
	buffer.c \
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
 	-DDATADIR='"$(datadir)"'
interpret_CFLAGS = $(GTK_CFLAGS)
interpret_LDADD = $(GTK_LIBS)

iso2022_SOURCES = \
	buffer.c \
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

ring_SOURCES = ring.c ring.h debug.c debug.h
ring_CPPFLAGS = -DRING_MAIN

slowcat_SOURCES = \
	slowcat.c

table_SOURCES = \
	buffer.c \
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
	buffer.c \
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

pty_SOURCES = pty.c pty.h debug.c debug.h
pty_CPPFLAGS = -DPTY_MAIN -DLIBEXECDIR='"$(libexecdir)"'

reaper_SOURCES = reaper.c reaper.h debug.c debug.h marshal.c marshal.h
reaper_CPPFLAGS =  -DREAPER_MAIN -DLOCALEDIR='"$(localedir)"'

vteconv_SOURCES = buffer.c buffer.h debug.c debug.h vteconv.c vteconv.h
vteconv_CPPFLAGS = -DVTECONV_MAIN
vteconv_CFLAGS = $(VTE_CFLAGS)
vteconv_LDADD = $(VTE_LIBS)

buffer_SOURCES = buffer.c
buffer_CPPFLAGS = -DBUFFER_MAIN

dumpkeys_SOURCES = dumpkeys.c
mev_SOURCES = mev.c
ssfe_SOURCES = ssfe.c
xticker_SOURCES = xticker.c
