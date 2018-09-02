SUBDIRS = . app

bin_PROGRAMS =
noinst_PROGRAMS =

CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
EXTRA_DIST =

# The library

headerdir = $(includedir)/vte-$(VTE_API_VERSION)/vte
header_HEADERS = \
	vte/vte.h \
	vte/vtedeprecated.h \
	vte/vteenums.h \
	vte/vteglobals.h \
	vte/vtemacros.h \
	vte/vtepty.h \
	vte/vteregex.h \
	vte/vteterminal.h \
	$(NULL)

nodist_header_HEADERS = \
	vte/vtetypebuiltins.h \
	vte/vteversion.h \
	$(NULL)

lib_LTLIBRARIES = libvte-@VTE_API_MAJOR_VERSION@.@VTE_API_MINOR_VERSION@.la

EXTRA_libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_SOURCES = \
	box_drawing_generate.sh \
	marshal.list \
	vtetypebuiltins.cc.template \
	vtetypebuiltins.h.template \
	$(NULL)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_SOURCES = \
	vte/vte.h \
	vte/vtedeprecated.h \
	vte/vteenums.h \
	vte/vteglobals.h \
	vte/vtemacros.h \
	vte/vtepty.h \
	vte/vteregex.h \
	vte/vteterminal.h \
	attr.hh \
	buffer.h \
	caps.hh \
	cell.hh \
	chunk.cc \
	chunk.hh \
	color-triple.hh \
	debug.cc \
	debug.h \
	keymap.cc \
	keymap.h \
	modes.hh \
	modes-ecma.hh \
	modes-private.hh \
	parser.cc \
	parser.hh \
	parser-arg.hh \
	parser-charset.hh \
	parser-charset-tables.hh \
	parser-cmd.hh \
	parser-c01.hh \
	parser-csi.hh \
	parser-dcs.hh \
	parser-esc.hh \
	parser-glue.hh \
	parser-osc.hh \
	parser-reply.hh \
	parser-string.hh \
	pty.cc \
	reaper.cc \
	reaper.hh \
	refptr.hh \
	ring.cc \
	ring.hh \
	utf8.cc \
	utf8.hh \
	vte.cc \
	vteaccess.cc \
	vteaccess.h \
	vtedefines.hh \
	vtedraw.cc \
	vtedraw.hh \
	vtegtk.cc \
	vtegtk.hh \
	vteinternal.hh \
	vtepcre2.h \
	vtepty-private.h \
	vteregex.cc \
	vteregexinternal.hh \
	vterowdata.cc \
	vterowdata.hh \
	vtespawn.cc \
	vtespawn.hh \
	vteseq.cc \
	vtestream.cc \
	vtestream.h \
	vtestream-base.h \
	vtestream-file.h \
	vtetypes.cc \
	vtetypes.hh \
	vteunistr.cc \
	vteunistr.h \
	vteutils.cc \
	vteutils.h \
	widget.cc \
	widget.hh \
	$(NULL)

nodist_libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_SOURCES = \
	box_drawing.h \
	marshal.cc \
	marshal.h \
	vteresources.cc \
	vtetypebuiltins.cc \
	vte/vtetypebuiltins.h \
	vte/vteversion.h \
	$(NULL)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Vte\" \
	-DVTE_API_VERSION=\"$(VTE_API_VERSION)\" \
	-DDATADIR='"$(datadir)"' \
	-DLIBEXECDIR='"$(libexecdir)"' \
	-DLOCALEDIR='"$(localedir)"' \
	-DGLIB_DISABLE_DEPRECATION_WARNINGS \
	-DVTE_COMPILATION \
	-UPARSER_INCLUDE_NOP \
	-I$(builddir)/vte \
	-I$(srcdir)/vte \
	$(AM_CPPFLAGS)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_CXXFLAGS = \
	$(VTE_CFLAGS) \
	$(AM_CXXFLAGS)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_LDFLAGS = \
	$(VTE_CXXLDFLAGS) \
	-version-info $(LT_VERSION_INFO) \
	$(AM_LDFLAGS)

libvte_@VTE_API_MAJOR_VERSION@_@VTE_API_MINOR_VERSION@_la_LIBADD = \
	$(VTE_LIBS)

# Generated sources

BUILT_SOURCES = \
	box_drawing.h \
	marshal.cc \
	marshal.h \
	vtetypebuiltins.cc \
	vte/vtetypebuiltins.h \
	vteresources.cc \
	$(NULL)
EXTRA_DIST += box_drawing.txt box_drawing_generate.sh
CLEANFILES += $(BUILT_SOURCES) stamp-vtetypebuiltins.h

box_drawing.h: box_drawing.txt box_drawing_generate.sh
	$(AM_V_GEN) $(srcdir)/box_drawing_generate.sh < $< > $@

marshal.cc: marshal.list
	$(AM_V_GEN) echo '#include "marshal.h"' > $@ \
	&& $(GLIB_GENMARSHAL) --prefix=_vte_marshal --body --internal $< >> $@

marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_vte_marshal --header --internal $< > $@

vte/vtetypebuiltins.h: stamp-vtetypebuiltins.h
	@true
stamp-vtetypebuiltins.h: vtetypebuiltins.h.template vte/vteenums.h vte/vtedeprecated.h
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter %.h,$^) > xgen-vtbh \
	&& (cmp -s xgen-vtbh vte/vtetypebuiltins.h || cp xgen-vtbh vte/vtetypebuiltins.h ) \
	&& rm -f xgen-vtbh \
	&& echo timestamp > $(@F)

vtetypebuiltins.cc: vtetypebuiltins.cc.template vte/vteenums.h vte/vtedeprecated.h
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $< $(filter %.h,$^) > xgen-vtbc \
	&& (cmp -s xgen-vtbc vtetypebuiltins.cc || cp xgen-vtbc vtetypebuiltins.cc ) \
	&& rm -f xgen-vtbc

vteresources.cc: vte.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies vte.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir . --generate-source --c-name _vte $<

# Misc unit tests and utilities

noinst_PROGRAMS += parser-cat slowcat test-modes test-tabstops test-parser test-refptr test-utf8
noinst_SCRIPTS = decset osc window
EXTRA_DIST += $(noinst_SCRIPTS)

check_PROGRAMS = \
	dumpkeys \
	reaper \
	reflect-text-view \
	reflect-vte mev \
	xticker \
	vtestream-file \
	test-vtetypes \
	$(NULL)

dist_check_SCRIPTS = \
	check-doc-syntax.sh \
	test-vte-sh.sh \
	$(NULL)

TESTS = \
	test-modes \
	test-parser \
	test-refptr \
	test-tabstops \
	test-utf8 \
	reaper \
	test-vtetypes \
	vtestream-file \
	$(dist_check_SCRIPTS) \
	$(NULL)

TESTS_ENVIRONMENT = \
	srcdir="$(srcdir)" \
	top_builddir="$(top_builddir)" \
	VTE_API_VERSION="$(VTE_API_VERSION)" \
	$(NULL)

reaper_CPPFLAGS = -DMAIN -I$(builddir) -I$(srcdir) $(AM_CPPFLAGS)
reaper_CXXFLAGS = $(VTE_CFLAGS) $(AM_CXXFLAGS)
reaper_SOURCES = \
	debug.cc \
	debug.h \
	reaper.cc \
	reaper.hh \
	$(NULL)
reaper_LDADD = $(VTE_LIBS)

reflect_text_view_CPPFLAGS = -DUSE_TEXT_VIEW -I$(builddir)/vte -I$(srcdir)/vte $(AM_CPPFLAGS)
reflect_text_view_CFLAGS = $(VTE_CFLAGS) $(AM_CFLAGS)
reflect_text_view_SOURCES = reflect.c
reflect_text_view_LDADD = $(VTE_LIBS)

reflect_vte_CPPFLAGS = -DUSE_VTE -I$(builddir)/vte -I$(srcdir)/vte $(AM_CPPFLAGS)
reflect_vte_CFLAGS = $(VTE_CFLAGS) $(AM_CFLAGS)
reflect_vte_SOURCES = reflect.c
reflect_vte_LDADD = libvte-$(VTE_API_VERSION).la $(VTE_LIBS)

parser_cat_SOURCES = \
	buffer.h \
	caps.hh \
	debug.cc \
	debug.h \
	parser.cc \
	parser.hh \
	parser-arg.hh \
	parser-charset.hh \
	parser-charset-tables.hh \
	parser-cmd.hh \
	parser-c01.hh \
	parser-csi.hh \
	parser-dcs.hh \
	parser-esc.hh \
	parser-glue.hh \
	parser-osc.hh \
	parser-reply.hh \
	parser-string.hh \
	parser-cat.cc \
	utf8.cc \
	utf8.hh \
	$(NULL)
parser_cat_CPPFLAGS = \
	-I$(builddir) \
	-I$(srcdir) \
	-DPARSER_INCLUDE_NOP \
	$(AM_CPPFLAGS)
parser_cat_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(AM_CFLAGS)
parser_cat_CXXFLAGS = \
	$(GLIB_CFLAGS) \
	$(AM_CXXFLAGS)
parser_cat_LDADD = \
	$(GLIB_LIBS)

slowcat_SOURCES = \
	slowcat.c \
	$(NULL)
slowcat_CPPFLAGS = -I$(builddir) -I$(srcdir) $(AM_CPPFLAGS)
slowcat_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
slowcat_LDADD = $(GLIB_LIBS)

test_parser_SOURCES = \
	parser-test.cc \
	parser.cc \
	parser.hh \
	parser-arg.hh \
	parser-charset.hh \
	parser-charset-tables.hh \
	parser-cmd.hh \
	parser-c01.hh \
	parser-csi.hh \
	parser-dcs.hh \
	parser-esc.hh \
	parser-glue.hh \
	parser-osc.hh \
	parser-reply.hh \
	parser-string.hh \
	$(NULL)
test_parser_CPPFLAGS = \
	-I$(builddir) \
	-I$(srcdir) \
	-DPARSER_INCLUDE_NOP \
	$(AM_CPPFLAGS)
test_parser_CXXFLAGS = \
	$(VTE_CFLAGS) \
	$(AM_CXXFLAGS)
test_parser_LDADD = \
	$(VTE_LIBS) \
	$(NULL)

test_modes_SOURCES = \
	modes-test.cc \
	modes.hh \
	modes-ecma.hh \
	modes-private.hh \
	$(NULL)
test_modes_CPPFLAGS = \
	-I$(builddir) \
	-I$(srcdir) \
	$(AM_CPPFLAGS)
test_modes_CXXFLAGS = \
	$(GLIB_CFLAGS) \
	$(AM_CXXFLAGS)
test_modes_LDADD = \
	$(GLIB_LIBS) \
	$(NULL)

test_tabstops_SOURCES = \
	tabstops-test.cc \
	tabstops.hh \
	$(NULL)
test_tabstops_CPPFLAGS = \
	-I$(builddir) \
	-I$(srcdir) \
	$(AM_CPPFLAGS)
test_tabstops_CXXFLAGS = \
	$(GLIB_CFLAGS) \
	$(AM_CXXFLAGS)
test_tabstops_LDADD = \
	$(GLIB_LIBS) \
	$(NULL)

test_refptr_SOURCES = \
	refptr.hh \
	refptr-test.cc \
	$(NULL)
test_refptr_CPPFLAGS = \
	-I$(builddir) \
	-I$(srcdir) \
	$(AM_CPPFLAGS)
test_refptr_CXXFLAGS = \
	$(GOBJECT_CFLAGS) \
	$(AM_CXXFLAGS)
test_refptr_LDADD = \
	$(GOBJECT_LIBS) \
	$(NULL)

test_utf8_SOURCES = \
	utf8-test.cc \
	utf8.cc \
	utf8.hh \
	$(NULL)
test_utf8_CPPFLAGS = \
	-I$(builddir) \
	-I$(srcdir) \
	$(AM_CPPFLAGS)
test_utf8_CXXFLAGS = \
	$(GLIB_CFLAGS) \
	$(AM_CXXFLAGS)
test_utf8_LDADD = \
	$(GLIB_LIBS) \
	$(NULL)

test_vtetypes_SOURCES = \
	vtetypes.cc \
	vtetypes.hh \
	$(NULL)
test_vtetypes_CPPFLAGS = \
	-DMAIN \
	-I$(builddir) \
	-I$(srcdir) \
	$(AM_CPPFLAGS)
test_vtetypes_CXXFLAGS = \
	$(VTE_CFLAGS) \
	$(AM_CXXFLAGS)
test_vtetypes_LDADD = \
	$(VTE_LIBS) \
	$(NULL)

vtestream_file_SOURCES = \
	vtestream-base.h \
	vtestream-file.h \
	vtestream.cc \
	vtestream.h \
	vteutils.cc \
	vteutils.h \
	$(NULL)
vtestream_file_CPPFLAGS = \
	-DVTESTREAM_MAIN \
	-I$(builddir) \
	-I$(srcdir) \
	$(AM_CPPFLAGS)
vtestream_file_CXXFLAGS = \
	$(VTE_CFLAGS) \
	$(AM_CXXFLAGS)
vtestream_file_LDADD = \
	$(VTE_LIBS)

dumpkeys_SOURCES = dumpkeys.c
dumpkeys_CPPFLAGS = -I$(builddir) -I$(srcdir) $(AM_CPPFLAGS)
dumpkeys_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
dumpkeys_LDADD = $(GLIB_LIBS)

mev_SOURCES = mev.c
mev_CPPFLAGS = -I$(builddir) -I$(srcdir) $(AM_CPPFLAGS)
mev_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
mev_LDADD = $(GLIB_LIBS)

xticker_SOURCES = xticker.c
xticker_CPPFLAGS = -I$(builddir) -I$(srcdir) $(AM_CPPFLAGS)
xticker_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
xticker_LDADD = $(GLIB_LIBS)

# Shell

profiledir = $(sysconfdir)/profile.d
dist_profile_DATA = vte.sh

.DELETE_ON_ERROR:

-include $(top_srcdir)/git.mk
