bin_PROGRAMS = vte
pkginclude_HEADERS = pty.h reaper.h vte.h vteaccess.h
lib_LTLIBRARIES = libvte.la
pkglib_PROGRAMS = interpret utf8echo nativeecho utf8mode iso8859mode slowcat vterdb
pkglib_SCRIPTS = decset osc window
noinst_PROGRAMS = buffer dumpkeys iso2022 pty reaper reflect-text-view reflect-vte $(REFLECT_ZVT) ring mev ssfe table trie xticker vteconv vtetc
EXTRA_DIST = \
	$(pkglib_SCRIPTS) \
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

AM_CFLAGS = @VTE_CFLAGS@ @CFLAGS@ @X_CFLAGS@ -DG_DISABLE_ASSERT
AM_LDFLAGS = @LDFLAGS@

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
	vtepangox.c \
	vtepangox.h \
	vterdb.c \
	vterdb.h \
	vteregex.c \
	vteregex.h \
	vtergb.c \
	vtergb.h \
	vteskel.c \
	vteskel.h \
	vtetc.c \
	vtetc.h \
	vtetree.c \
	vtetree.h \
	vtexft.c \
	vtexft.h

# Libtool shared library versioning stuffs.
# REVISION gets incremented whenever the source code changes without adding
# an API or ABI change.
VERSION_REVISION=0
# CURRENT must be incremented when an API or ABI change (addition or removal)
# is made, and REVISION must be reset to 0 when this happens.
VERSION_CURRENT=8
# AGE must be incremented when an API or ABI addition is made, and REVISION
# must be reset to 0 when this happens.
VERSION_AGE=4
libvte_la_LDFLAGS = @LDFLAGS@ -version-info $(VERSION_CURRENT):$(VERSION_REVISION):$(VERSION_AGE)
libvte_la_LIBADD = @LIBS@ @VTE_LIBS@ @X_LIBS@

CLEANFILES = marshal.c marshal.h

marshal.c marshal.h: marshal.list
	glib-genmarshal --prefix=_vte_marshal --body $^ > marshal.c
	glib-genmarshal --prefix=_vte_marshal --header $^ > marshal.h

vte_SOURCES = \
	vteapp.c
vte_LDADD = libvte.la @LDFLAGS@ @LIBS@ @VTE_LIBS@ @X_LIBS@

reflect_text_view_CFLAGS = @CFLAGS@ -DUSE_TEXT_VIEW @VTE_CFLAGS@ @X_CFLAGS@
reflect_text_view_SOURCES = reflect.c
reflect_text_view_LDADD = @LDFLAGS@ @LIBS@ @VTE_LIBS@ @X_LIBS@

reflect_vte_CFLAGS = @CFLAGS@ -DUSE_VTE @VTE_CFLAGS@ @X_CFLAGS@
reflect_vte_SOURCES = reflect.c
reflect_vte_LDADD = libvte.la @LDFLAGS@ @LIBS@ @VTE_LIBS@ @X_LIBS@

if REFLECT_USE_ZVT
REFLECT_ZVT = reflect-zvt
reflect_zvt_CFLAGS = @CFLAGS@ -DUSE_ZVT @ZVT_DEFS@ @ZVT_CFLAGS@ @X_CFLAGS@
reflect_zvt_SOURCES = reflect.c
reflect_zvt_LDADD = @LDFLAGS@ @LIBS@ @ZVT_LIBS@ @X_LIBS@
else
REFLECT_ZVT = 
endif

interpret_CFLAGS = @CFLAGS@ @GTK_CFLAGS@ -DINTERPRET_MAIN
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
	interpret.c
interpret_LDADD = @LDFLAGS@ @GOBJECT_LIBS@

iso2022_CFLAGS = @CFLAGS@ @GTK_CFLAGS@ -DISO2022_MAIN
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
	vtetc.h
iso2022_LDADD = @LDFLAGS@ @LIBS@ @GOBJECT_LIBS@

utf8echo_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DUTF8ECHO_MAIN
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
utf8echo_LDADD = @LDFLAGS@ @LIBS@ @GOBJECT_LIBS@

nativeecho_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DNATIVEECHO_MAIN
nativeecho_SOURCES = \
	nativeecho.c
nativeecho_LDADD = @LDFLAGS@ @LIBS@

ring_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DRING_MAIN
ring_SOURCES = \
	debug.c \
	debug.h \
	ring.c \
	ring.h
ring_LDADD = @LDFLAGS@ @LIBS@ @GLIB_LIBS@

slowcat_SOURCES = \
	slowcat.c
slowcat_LDADD = @LDFLAGS@ @LIBS@ @GLIB_LIBS@

table_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DTABLE_MAIN
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
table_LDADD = @LDFLAGS@ @LIBS@ @GOBJECT_LIBS@

trie_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DTRIE_MAIN
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
trie_LDADD = @LDFLAGS@ @LIBS@ @GOBJECT_LIBS@

vtetc_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DTERMCAP_MAIN
vtetc_SOURCES = \
	debug.c \
	debug.h \
	vtetc.c \
	vtetc.h
vtetc_LDADD = @LDFLAGS@ @LIBS@ @GLIB_LIBS@

pty_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DPTY_MAIN
pty_SOURCES = \
	debug.c \
	debug.h \
	pty.c \
	pty.h
pty_LDADD = @LDFLAGS@ @LIBS@ @GLIB_LIBS@

reaper_CFLAGS = @CFLAGS@ @GTK_CFLAGS@ -DREAPER_MAIN
reaper_SOURCES = \
	debug.c \
	debug.h \
	marshal.c \
	marshal.h \
	reaper.c \
	reaper.h
reaper_LDADD = @LDFLAGS@ @LIBS@ @GOBJECT_LIBS@

vterdb_CFLAGS = @CFLAGS@ @GTK_CFLAGS@ -DVTERDB_MAIN
vterdb_SOURCES = vterdb.c
vterdb_LDADD = @LDFLAGS@ @LIBS@ @GTK_LIBS@

vteconv_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DVTECONV_MAIN
vteconv_SOURCES = \
	buffer.c \
	buffer.h \
	debug.c \
	debug.h \
	vteconv.c \
	vteconv.h
vteconv_LDADD = @LDFLAGS@ @GLIB_LIBS@

buffer_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DBUFFER_MAIN
buffer_SOURCES = buffer.c
buffer_LDADD = @LDFLAGS@ @GLIB_LIBS@

dumpkeys_SOURCES = dumpkeys.c
dumpkeys_LDADD = @LDFLAGS@ @LIBS@ @GLIB_LIBS@

mev_SOURCES = mev.c
mev_LDADD = @LDFLAGS@ @LIBS@ @GLIB_LIBS@

ssfe_SOURCES = ssfe.c
ssfe_LDADD = @LDFLAGS@ @OTHERLIBS@

xticker_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@
xticker_SOURCES = xticker.c
xticker_LDADD = @LIBS@ @GLIB_LIBS@
