bin_PROGRAMS = vte
pkginclude_HEADERS = pty.h reaper.h vte.h vteaccess.h
lib_LTLIBRARIES = libvte.la
pkglib_PROGRAMS = interpret utf8echo nativeecho utf8mode iso8859mode slowcat
pkglib_SCRIPTS = decset osc window
noinst_PROGRAMS = dumpkeys iso2022 pty reaper ring table termcap trie
EXTRA_DIST = \
	$(pkglib_SCRIPTS) \
	genkeysyms.py \
	marshal.list \
	unitable.CNS11643 \
	unitable.GB12345 \
	unitable.GB2312 \
	unitable.JIS0201 \
	unitable.JIS0208 \
	unitable.JIS0212 \
	unitable.KSC5601

CFLAGS = @CFLAGS@ @VTE_CFLAGS@ @X_CFLAGS@

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
	pty.c \
	pty.h \
	reaper.c \
	reaper.h \
	ring.c \
	ring.h \
	table.c \
	table.h \
	termcap.c \
	termcap.h \
	trie.c \
	trie.h \
	vte.c \
	vte.h \
	vteaccess.c \
	vteaccess.h

libvte_la_LIBADD = @LIBS@ @VTE_LIBS@ @X_LIBS@
libvte_la_LDFLAGS = -version-info 4:0:0

CLEANFILES = marshal.c marshal.h

marshal.c marshal.h: marshal.list
	glib-genmarshal --prefix=_vte_marshal --body $^ > marshal.c
	glib-genmarshal --prefix=_vte_marshal --header $^ > marshal.h

vte_SOURCES = \
	vteapp.c
vte_LDADD = libvte.la @LIBS@ @VTE_LIBS@ @X_LIBS@

interpret_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DINTERPRET_MAIN
interpret_SOURCES = \
	buffer.c \
	buffer.h \
	caps.c \
	caps.h \
	debug.c \
	debug.h \
	iso2022.c \
	iso2022.h \
	table.c \
	table.h \
	termcap.c \
	termcap.h \
	interpret.c
interpret_LDADD = @GOBJECT_LIBS@

iso2022_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DISO2022_MAIN
iso2022_SOURCES = \
	debug.c \
	debug.h \
	iso2022.c \
	iso2022.h \
	table.c \
	table.h
iso2022_LDADD = @LIBS@ @GOBJECT_LIBS@

utf8echo_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DUTF8ECHO_MAIN
utf8echo_SOURCES = \
	debug.c \
	debug.h \
	table.c \
	table.h \
	utf8echo.c
utf8echo_LDADD = @LIBS@ @GOBJECT_LIBS@

nativeecho_CFLAGS = @CFLAGS@ -DNATIVEECHO_MAIN
nativeecho_SOURCES = \
	nativeecho.c
nativeecho_LDADD = @LIBS@

ring_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DRING_MAIN
ring_SOURCES = \
	debug.c \
	debug.h \
	ring.c \
	ring.h
ring_LDADD = @LIBS@ @GOBJECT_LIBS@

slowcat_SOURCES = \
	slowcat.c
slowcat_LDADD = @LIBS@ @GOBJECT_LIBS@

table_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DTABLE_MAIN
table_SOURCES = \
	debug.c \
	debug.h \
	table.c \
	table.h
table_LDADD = @LIBS@ @GOBJECT_LIBS@

trie_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DTRIE_MAIN
trie_SOURCES = \
	debug.c \
	debug.h \
	table.c \
	table.h \
	termcap.c \
	termcap.h \
	trie.c \
	trie.h
trie_LDADD = @LIBS@ @GOBJECT_LIBS@

termcap_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DTERMCAP_MAIN
termcap_SOURCES = \
	debug.c \
	debug.h \
	termcap.c \
	termcap.h
termcap_LDADD = @LIBS@ @GOBJECT_LIBS@

pty_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DPTY_MAIN
pty_SOURCES = \
	debug.c \
	debug.h \
	pty.c \
	pty.h
pty_LDADD = @LIBS@ @GOBJECT_LIBS@

reaper_CFLAGS = @CFLAGS@ @GOBJECT_CFLAGS@ -DREAPER_MAIN
reaper_SOURCES = \
	debug.c \
	debug.h \
	marshal.c \
	marshal.h \
	reaper.c \
	reaper.h
reaper_LDADD = @LIBS@ @GOBJECT_LIBS@

dumpkeys_CFLAGS = @CFLAGS@
dumpkeys_SOURCES = dumpkeys.c
dumpkeys_LDADD = @LIBS@ @GOBJECT_LIBS@
