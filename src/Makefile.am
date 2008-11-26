bin_PROGRAMS = vte

pkginclude_HEADERS = pty.h reaper.h vte.h vteaccess.h vtetypebuiltins.h vteversion.h

lib_LTLIBRARIES = libvte.la

pkglib_PROGRAMS = interpret utf8echo nativeecho utf8mode iso8859mode slowcat

pkglib_SCRIPTS = decset osc window

EXTRA_PROGRAMS = buffer dumpkeys iso2022 pty reaper reflect-text-view reflect-vte ring mev ssfe table trie xticker vteconv vtetc
EXTRA_DIST = \
	$(pkglib_SCRIPTS) \
	vteseq-2.gperf \
	vteseq-n.gperf \
	vteseq-2.c \
	vteseq-n.c \
	vtetypebuiltins.c.template \
	vtetypebuiltins.h.template \
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
# In particular:
# 1. The lookup needs to return the VteTerminalSequenceHandler, or NULL.
# 2. The functions need to be declared static.
# 3. For vtese-2, the strlen is known so remove the redudant parameter.
gperf: vteseq-2.gperf vteseq-n.gperf
	gperf vteseq-2.gperf -m 100 > vteseq-2.c
	gperf vteseq-n.gperf -m 100 > vteseq-n.c

#vteseq-2.c vteseq-n.c: gperf


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
	vteint.h \
	vtepangocairo.c \
	vtepangocairo.h \
	vteregex.c \
	vteregex.h \
	vteseq.c \
	vteseq.h \
	vteseq-list.h \
	vteseq-table.h \
	vteskel.c \
	vteskel.h \
	vtetc.c \
	vtetc.h \
	vtetree.c \
	vtetree.h \
	vtetypebuiltins.c \
	vtetypebuiltins.h \
	vteversion.h

libvte_la_CPPFLAGS = \
	-DG_DISABLE_ASSERT \
	-DG_LOG_DOMAIN=\"Vte\" \
 	-DDATADIR='"$(datadir)"' \
	-DLIBEXECDIR='"$(libexecdir)"' \
	-DLOCALEDIR='"$(localedir)"' \
	-DVTE_COMPILATION
libvte_la_CFLAGS = $(VTE_CFLAGS) $(PCRE_CFLAGS)
libvte_la_LIBADD = $(VTE_LIBS) $(PCRE_LIBS)
libvte_la_LDFLAGS = \
	-version-info $(LIBVTE_LTVERSION) \
	-export-symbols-regex "^vte_terminal_.*|^_vte_pty_.*|^vte_reaper_.*|_vte_debug_.*" \
	@LIBTOOL_EXPORT_OPTIONS@ @LIBTOOL_FLAGS@

BUILT_SOURCES = marshal.c marshal.h vtetypebuiltins.c vtetypebuiltins.h
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

vte_SOURCES = \
	vteapp.c
vte_CFLAGS = $(VTE_CFLAGS)
vte_LDADD = libvte.la $(VTE_LIBS)

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
utf8echo_CPPFLAGS = -DUTF8ECHO_MAIN
utf8echo_CFLAGS = $(GOBJECT_CFLAGS)
utf8echo_LDADD = $(GOBJECT_LIBS)

nativeecho_SOURCES = \
	nativeecho.c
nativeecho_CPPFLAGS = -DNATIVEECHO_MAIN
nativeecho_CFLAGS = $(GLIB_CFLAGS)
nativeecho_LDADD = $(GLIB_LIBS)

ring_SOURCES = \
	debug.c \
	debug.h \
	ring.c \
	ring.h
ring_CPPFLAGS = -DRING_MAIN
ring_CFLAGS = $(GLIB_CFLAGS)
ring_LDADD = $(GLIB_LIBS)

slowcat_SOURCES = \
	slowcat.c
slowcat_CFLAGS = $(GLIB_CFLAGS)
slowcat_LDADD = $(GLIB_LIBS)

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
table_CFLAGS = $(GOBJECT_CFLAGS)
table_LDADD = $(GOBJECT_LIBS)

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
trie_CPPFLAGS = -DTRIE_MAIN
trie_CFLAGS = $(GOBJECT_CFLAGS)
trie_LDADD = $(GOBJECT_LIBS)

vtetc_SOURCES = \
	debug.c \
	debug.h \
	vtetc.c \
	vtetc.h
vtetc_CPPFLAGS = -DTERMCAP_MAIN
vtetc_CFLAGS = $(GLIB_CFLAGS)
vtetc_LDADD = $(GLIB_LIBS)

pty_SOURCES = \
	debug.c \
	debug.h \
	pty.c \
	pty.h
pty_CPPFLAGS = -DPTY_MAIN -DLIBEXECDIR='"$(libexecdir)"'
pty_CFLAGS = $(GLIB_CFLAGS)
pty_LDADD = $(GLIB_LIBS)

reaper_SOURCES = \
	debug.c \
	debug.h \
	marshal.c \
	marshal.h \
	reaper.c \
	reaper.h
reaper_CPPFLAGS =  -DREAPER_MAIN -DLOCALEDIR='"$(localedir)"'
reaper_CFLAGS = $(GOBJECT_CFLAGS)
reaper_LDADD = $(GOBJECT_LIBS)

vteconv_SOURCES = \
	buffer.c \
	buffer.h \
	debug.c \
	debug.h \
	vteconv.c \
	vteconv.h
vteconv_CPPFLAGS = -DVTECONV_MAIN
vteconv_CFLAGS = $(GLIB_CFLAGS)
vteconv_LDADD = $(GLIB_LIBS)

buffer_SOURCES = buffer.c
buffer_CPPFLAGS = -DBUFFER_MAIN
buffer_CFLAGS = $(GLIB_CFLAGS)
buffer_LDADD = $(GLIB_LIBS)

dumpkeys_SOURCES = dumpkeys.c
dumpkeys_CFLAGS = $(GLIB_CFLAGS)
dumpkeys_LDADD = $(GLIB_LIBS)

mev_SOURCES = mev.c
mev_CFLAGS = $(GLIB_CFLAGS)
mev_LDADD = $(GLIB_LIBS)

ssfe_SOURCES = ssfe.c

xticker_SOURCES = xticker.c
xticker_CFLAGS = $(GLIB_CFLAGS)
xticker_LDADD = $(GLIB_LIBS)
