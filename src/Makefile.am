bin_PROGRAMS = vte
pkginclude_HEADERS = caps.h pty.h ring.h termcap.h trie.h vte.h vteaccess.h
lib_LTLIBRARIES = libvte.la
noinst_PROGRAMS = interpret utf8echo utf8mode iso8859mode
EXTRA_PROGRAMS = pty ring termcap trie
EXTRA_DIST = marshal.list

CFLAGS = @CFLAGS@ @X_CFLAGS@ @GTK_CFLAGS@

libvte_la_SOURCES = \
	caps.c \
	caps.h \
	debug.c \
	debug.h \
	marshal.c \
	marshal.h \
	pty.c \
	pty.h \
	ring.c \
	ring.h \
	termcap.c \
	termcap.h \
	trie.c \
	trie.h \
	vte.c \
	vte.h \
	vteaccess.c \
	vteaccess.h

CLEANFILES = marshal.c marshal.h

marshal.c marshal.h: marshal.list
	glib-genmarshal --prefix=_vte_marshal --body $^ > marshal.c
	glib-genmarshal --prefix=_vte_marshal --header $^ > marshal.h

vte_SOURCES = \
	vteapp.c
vte_LDADD = libvte.la @GTK_LIBS@ @X_LIBS@

interpret_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DINTERPRET_MAIN
interpret_SOURCES = \
	caps.c \
	caps.h \
	debug.c \
	debug.h \
	termcap.c \
	termcap.h \
	trie.c \
	trie.h \
	interpret.c
interpret_LDADD = @GLIB_LIBS@

utf8echo_SOURCES = \
	utf8echo.c
utf8echo_LDADD = @GLIB_LIBS@

ring_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DRING_MAIN
ring_SOURCES = \
	debug.c \
	debug.h \
	ring.c \
	ring.h
ring_LDADD = @GLIB_LIBS@

trie_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DTRIE_MAIN
trie_SOURCES = \
	debug.c \
	debug.h \
	termcap.c \
	termcap.h \
	trie.c \
	trie.h
trie_LDADD = @GLIB_LIBS@

termcap_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DTERMCAP_MAIN
termcap_SOURCES = \
	debug.c \
	debug.h \
	termcap.c \
	termcap.h
termcap_LDADD = @GLIB_LIBS@

pty_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DPTY_MAIN
pty_SOURCES = \
	debug.c \
	debug.h \
	pty.c \
	pty.h
pty_LDADD = @GLIB_LIBS@
