## Process this file with automake to create Makefile.in.

# The name of the module.
DOC_MODULE=vte

# The top-level SGML file.
DOC_MAIN_SGML_FILE=vte-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../src

# Used for dependencies
HFILE_GLOB= $(top_srcdir)/src/*.h
CFILE_GLOB=$(top_srcdir)/src/*.c

IGNORE_HFILES= \
	buffer.h \
	caps.h \
	debug.h \
	iso2022.h \
	keymap.h \
	marshal.h \
	matcher.h \
	ring.h \
	table.h \
	trie.h \
	vtebg.h \
	vteconv.h \
	vtedraw.h \
	vtefc.h \
	vteft2.h \
	vtegl.h \
	vteglyph.h \
	vteint.h \
	vtepango.h \
	vtepangox.h \
	vte-private.h \
	vterdb.h \
	vteregex.h \
	vtergb.h \
	vteseq.h \
	vteseq-list.h \
	vteseq-table.h \
	vteskel.h \
	vtetc.h \
	vtetree.h \
	vtexft.h


# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = $(VTE_CFLAGS) -I$(top_srcdir)/src
GTKDOC_LIBS = $(top_builddir)/src/libvte.la

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

MAINTAINERCLEANFILES = vte-undocumented.txt

include $(top_srcdir)/gtk-doc.make
