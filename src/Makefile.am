bin_PROGRAMS = vte
pkginclude_HEADERS = pty.h reaper.h vte.h vteaccess.h
lib_LTLIBRARIES = libvte.la
pkglib_PROGRAMS = interpret utf8echo utf8mode iso8859mode slowcat
noinst_PROGRAMS = iso2022 pty reaper ring table termcap trie
EXTRA_DIST = \
	marshal.list \
	unitable.CNS11643 \
	unitable.GB12345 \
	unitable.GB2312 \
	unitable.JIS0201 \
	unitable.JIS0208 \
	unitable.JIS0212 \
	unitable.KSC5601

CFLAGS = @CFLAGS@ @XFT_CFLAGS@ @GTK_CFLAGS@ @X_CFLAGS@

libvte_la_SOURCES = \
	caps.c \
	caps.h \
	debug.c \
	debug.h \
	iso2022.c \
	iso2022.h \
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

libvte_la_LIBADD = @LIBS@ @XFT_LIBS@ @GTK_LIBS@ @X_LIBS@
libvte_la_LDFLAGS = -version-info 8:0:6

CLEANFILES = marshal.c marshal.h

marshal.c marshal.h: marshal.list
	glib-genmarshal --prefix=_vte_marshal --body $^ > marshal.c
	glib-genmarshal --prefix=_vte_marshal --header $^ > marshal.h

vte_SOURCES = \
	vteapp.c
vte_LDADD = libvte.la @LIBS@ @XFT_LIBS@ @GTK_LIBS@ @X_LIBS@

interpret_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DINTERPRET_MAIN
interpret_SOURCES = \
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
interpret_LDADD = @GLIB_LIBS@

iso2022_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DISO2022_MAIN
iso2022_SOURCES = \
	debug.c \
	debug.h \
	iso2022.c \
	iso2022.h \
	table.c \
	table.h
iso2022_LDADD = @LIBS@ @GLIB_LIBS@

utf8echo_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DUTF8ECHO_MAIN
utf8echo_SOURCES = \
	debug.c \
	debug.h \
	table.c \
	table.h \
	utf8echo.c
utf8echo_LDADD = @LIBS@ @GLIB_LIBS@

ring_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DRING_MAIN
ring_SOURCES = \
	debug.c \
	debug.h \
	ring.c \
	ring.h
ring_LDADD = @LIBS@ @GLIB_LIBS@

slowcat_SOURCES = \
	slowcat.c
slowcat_LDADD = @LIBS@ @GLIB_LIBS@

table_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DTABLE_MAIN
table_SOURCES = \
	debug.c \
	debug.h \
	table.c \
	table.h
table_LDADD = @LIBS@ @GLIB_LIBS@

trie_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DTRIE_MAIN
trie_SOURCES = \
	debug.c \
	debug.h \
	table.c \
	table.h \
	termcap.c \
	termcap.h \
	trie.c \
	trie.h
trie_LDADD = @LIBS@ @GLIB_LIBS@

termcap_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DTERMCAP_MAIN
termcap_SOURCES = \
	debug.c \
	debug.h \
	termcap.c \
	termcap.h
termcap_LDADD = @LIBS@ @GLIB_LIBS@

pty_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DPTY_MAIN
pty_SOURCES = \
	debug.c \
	debug.h \
	pty.c \
	pty.h
pty_LDADD = @LIBS@ @GLIB_LIBS@

reaper_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DREAPER_MAIN
reaper_SOURCES = \
	debug.c \
	debug.h \
	marshal.c \
	marshal.h \
	reaper.c \
	reaper.h
reaper_LDADD = @LIBS@ @GLIB_LIBS@
