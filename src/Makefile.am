bin_PROGRAMS = vte
lib_LTLIBRARIES = libvte.la
noinst_PROGRAMS = interpret utf8echo
EXTRA_PROGRAMS = pty termcap trie
EXTRA_DIST = marshal.list

CFLAGS = @CFLAGS@ @X_CFLAGS@ @GTK_CFLAGS@

libvte_la_SOURCES = \
	caps.c \
	caps.h \
	marshal.c \
	marshal.h \
	pty.c \
	pty.h \
	termcap.c \
	termcap.h \
	trie.c \
	trie.h \
	vte.c \
	vte.h

CLEANFILES = marshal.c marshal.h

marshal.c marshal.h: marshal.list
	glib-genmarshal --prefix=_vte_marshal --body $^ > marshal.c
	glib-genmarshal --prefix=_vte_marshal --header $^ > marshal.h

vte_SOURCES = \
	vteapp.c
vte_LDADD = libvte.la @GTK_LIBS@ @X_LIBS@

interpret_SOURCES = \
	caps.c \
	caps.h \
	termcap.c \
	termcap.h \
	trie.c \
	trie.h \
	interpret.c
interpret_LDADD = @GLIB_LIBS@

utf8echo_SOURCES = \
	utf8echo.c

trie_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DTRIE_MAIN
trie_SOURCES = \
	termcap.c \
	termcap.h \
	trie.c \
	trie.h
trie_LDADD = @GLIB_LIBS@

termcap_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DTERMCAP_MAIN
termcap_SOURCES = \
	termcap.c \
	termcap.h
termcap_LDADD = @GLIB_LIBS@

pty_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@ -DPTY_MAIN
pty_SOURCES = \
	pty.c \
	pty.h
pty_LDADD = @GLIB_LIBS@
