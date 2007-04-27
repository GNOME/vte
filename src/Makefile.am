bin_PROGRAMS = vte
pkginclude_HEADERS = pty.h reaper.h vte.h vteaccess.h
lib_LTLIBRARIES = libvte.la
pkglib_PROGRAMS = interpret utf8echo nativeecho utf8mode iso8859mode slowcat vterdb
pkglib_SCRIPTS = decset osc window
EXTRA_PROGRAMS = buffer dumpkeys iso2022 pty reaper reflect-text-view reflect-vte ring mev ssfe table trie xticker vteconv vtetc
EXTRA_DIST = \
	$(pkglib_SCRIPTS) \
	vteseq-gen.c \
	vteseq-2.gperf \
	vteseq-n.gperf \
	vteseq-2.c \
	vteseq-n.c \
	genkeysyms.py \
	iso2022.txt \
	marshal.list \
	mkunitables.sh \
	uniwidths \
	unitable.CNS11643 \
	unitable.CP437 \
	unitable.GB12345 \
	unitable.GB2312 \
	unitable.JIS0201 \
	unitable.JIS0208 \
	unitable.JIS0212 \
	unitable.KSX1001

# gperf - files need to be modified afterwards
#vteseq-2.c: vteseq-2.gperf
#	gperf vteseq-2.gperf -g -a -S 4 -t -m 100 -C -H vteseq_2_hash -N vteseq_2_lookup > $@
#vteseq-n.c: vteseq-n.gperf
#	gperf vteseq-n.gperf -g -a -S 4 -t -m 100 -C -P -l -H vteseq_n_hash -N vteseq_n_lookup -Q vteseq_n_stringpool > $@


AM_CFLAGS = $(VTE_CFLAGS) $(FT2_CFLAGS) $(X_CFLAGS) $(PCRE_CFLAGS) \
	    -DG_DISABLE_ASSERT -DG_LOG_DOMAIN=\"Vte\"
AM_CPPFLAGS = -DDATADIR='"$(datadir)"' \
	      -DLIBEXECDIR='"$(libexecdir)"' \
	      -DLOCALEDIR='"$(localedir)"'
AM_LDFLAGS = $(LDFLAGS)

EXTRA_libvte_la_SOURCES = keysyms.c

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
	vtefc.c \
	vtefc.h \
	vteft2.c \
	vteft2.h \
	vtegl.c \
	vtegl.h \
	vteglyph.c \
	vteglyph.h \
	vteint.h \
	vtepango.c \
	vtepango.h \
	vterdb.c \
	vterdb.h \
	vteregex.c \
	vteregex.h \
	vtergb.c \
	vtergb.h \
	vteseq.c \
	vteseq.h \
	vteseq-list.h \
	vteseq-table.h \
	vteskel.c \
	vteskel.h \
	vtetc.c \
	vtetc.h \
	vtetree.c \
	vtetree.h
if BUILD_XFT
libvte_la_SOURCES += \
	vtexft.c \
	vtexft.h
endif
if BUILD_PANGOX
libvte_la_SOURCES += \
	vtepangox.c \
	vtepangox.h
endif

# Libtool shared library versioning stuffs.
# REVISION gets incremented whenever the source code changes without adding
# an API or ABI change.
VERSION_REVISION=7
# CURRENT must be incremented when an API or ABI change (addition or removal)
# is made, and REVISION must be reset to 0 when this happens.
VERSION_CURRENT=11
# AGE must be incremented when an API or ABI addition is made, and REVISION
# must be reset to 0 when this happens.
VERSION_AGE=2
libvte_la_LDFLAGS = $(LDFLAGS) \
	-version-info $(VERSION_CURRENT):$(VERSION_REVISION):$(VERSION_AGE) \
	-export-symbols-regex "^vte_terminal_.*|^_vte_pty_.*|^vte_reaper_.*|_vte_debug_.*" \
	@LIBTOOL_EXPORT_OPTIONS@ @LIBTOOL_FLAGS@
libvte_la_LIBADD = $(LIBS) $(VTE_LIBS) $(X_LIBS) $(PCRE_LIBS) $(FT2_LIBS)

CLEANFILES = marshal.c marshal.h

marshal.c marshal.h: marshal.list
	glib-genmarshal --prefix=_vte_marshal --body $^ > marshal.c
	glib-genmarshal --prefix=_vte_marshal --header $^ > marshal.h

vte_SOURCES = \
	vteapp.c
vte_LDADD = libvte.la $(LDFLAGS) $(LIBS) $(VTE_LIBS) $(X_LIBS)

reflect_text_view_CFLAGS = -DUSE_TEXT_VIEW $(VTE_CFLAGS) $(X_CFLAGS)
reflect_text_view_SOURCES = reflect.c
reflect_text_view_LDADD = $(LDFLAGS) $(LIBS) $(VTE_LIBS) $(X_LIBS)

reflect_vte_CFLAGS = -DUSE_VTE $(VTE_CFLAGS) $(X_CFLAGS)
reflect_vte_SOURCES = reflect.c
reflect_vte_LDADD = libvte.la $(LDFLAGS) $(LIBS) $(VTE_LIBS) $(X_LIBS)

interpret_CFLAGS = $(GTK_CFLAGS) -DINTERPRET_MAIN
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
interpret_LDADD = $(LDFLAGS) $(GOBJECT_LIBS)

iso2022_CFLAGS = $(GTK_CFLAGS) -DISO2022_MAIN
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
iso2022_LDADD = $(LDFLAGS) $(LIBS) $(GOBJECT_LIBS)

utf8echo_CFLAGS = $(GOBJECT_CFLAGS) -DUTF8ECHO_MAIN
utf8echo_SOURCES = \
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
	utf8echo.c \
	vteconv.c \
	vteconv.h \
	vtetc.c \
	vtetc.h
utf8echo_LDADD = $(LDFLAGS) $(LIBS) $(GOBJECT_LIBS)

nativeecho_CFLAGS = $(GLIB_CFLAGS) -DNATIVEECHO_MAIN
nativeecho_SOURCES = \
	nativeecho.c
nativeecho_LDADD = $(LDFLAGS) $(LIBS) $(GLIB_LIBS)

ring_CFLAGS = $(GLIB_CFLAGS) -DRING_MAIN
ring_SOURCES = \
	debug.c \
	debug.h \
	ring.c \
	ring.h
ring_LDADD = $(LDFLAGS) $(LIBS) $(GLIB_LIBS)

slowcat_SOURCES = \
	slowcat.c
slowcat_LDADD = $(LDFLAGS) $(LIBS) $(GLIB_LIBS)

table_CFLAGS = $(GOBJECT_CFLAGS) -DTABLE_MAIN
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
table_LDADD = $(LDFLAGS) $(LIBS) $(GOBJECT_LIBS)

trie_CFLAGS = $(GOBJECT_CFLAGS) -DTRIE_MAIN
trie_SOURCES = \
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
trie_LDADD = $(LDFLAGS) $(LIBS) $(GOBJECT_LIBS)

vtetc_CFLAGS = $(GLIB_CFLAGS) -DTERMCAP_MAIN
vtetc_SOURCES = \
	debug.c \
	debug.h \
	vtetc.c \
	vtetc.h
vtetc_LDADD = $(LDFLAGS) $(LIBS) $(GLIB_LIBS)

pty_CFLAGS = $(GLIB_CFLAGS) -DPTY_MAIN
pty_SOURCES = \
	debug.c \
	debug.h \
	pty.c \
	pty.h
pty_LDADD = $(LDFLAGS) $(LIBS) $(GLIB_LIBS)

reaper_CFLAGS = $(GTK_CFLAGS) -DREAPER_MAIN
reaper_SOURCES = \
	debug.c \
	debug.h \
	marshal.c \
	marshal.h \
	reaper.c \
	reaper.h
reaper_LDADD = $(LDFLAGS) $(LIBS) $(GOBJECT_LIBS)

vterdb_CFLAGS = $(GTK_CFLAGS) $(X_CFLAGS) -DVTERDB_MAIN
vterdb_SOURCES = vterdb.c
vterdb_LDADD = $(LDFLAGS) $(LIBS) $(GTK_LIBS)

vteconv_CFLAGS = $(GLIB_CFLAGS) -DVTECONV_MAIN
vteconv_SOURCES = \
	buffer.c \
	buffer.h \
	debug.c \
	debug.h \
	vteconv.c \
	vteconv.h
vteconv_LDADD = $(LDFLAGS) $(GLIB_LIBS)

buffer_CFLAGS = $(GLIB_CFLAGS) -DBUFFER_MAIN
buffer_SOURCES = buffer.c
buffer_LDADD = $(LDFLAGS) $(GLIB_LIBS)

dumpkeys_SOURCES = dumpkeys.c
dumpkeys_LDADD = $(LDFLAGS) $(LIBS) $(GLIB_LIBS)

mev_SOURCES = mev.c
mev_LDADD = $(LDFLAGS) $(LIBS) $(GLIB_LIBS)

ssfe_SOURCES = ssfe.c
ssfe_LDADD = $(LDFLAGS) $(OTHERLIBS)

xticker_CFLAGS = $(GLIB_CFLAGS)
xticker_SOURCES = xticker.c
xticker_LDADD = $(LDFLAGS) $(LIBS) $(GLIB_LIBS)

vteseq-table.h: vteseq-gen.c vteseq-list.h
	$(MAKE) vteseq-gen
	./vteseq-gen > $@
