SUBDIRS = src python termcaps po doc
EXTRA_DIST = MAINTAINERS HACKING vte.spec vte.pc.in vte-uninstalled.pc.in \
	     intltool-extract.in intltool-merge.in intltool-update.in
DISTCLEANFILES = intltool-extract intltool-merge intltool-update

if BUILD_GNOME_PTY_HELPER
SUBDIRS += gnome-pty-helper
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = vte.pc

CVSTAG=$(shell echo $(PACKAGE)-$(VERSION) | sed 's,[\.\-],_,g')

forcetag:
	cvs tag -cFR $(CVSTAG) .

reallytag:
	cvs tag -cR $(CVSTAG) .

up:
	cvs up -dP

tag:
	$(top_srcdir)/autogen.sh
	$(MAKE) reallytag

archive:
	root=`cat $(top_srcdir)/CVS/Root` ; \
	repo=`cat $(top_srcdir)/CVS/Repository` ; \
	tmpdir=`mktemp -d /tmp/$(PACKAGE)-cvsXXXXXX` ; \
	dir=`pwd` ; \
	cd $$tmpdir ; \
	cvs -d $$root export -r $(CVSTAG) $$repo ; \
	cd $$tmpdir/$$repo ; \
	./autogen.sh ; \
	make dist ; \
	cp $(PACKAGE)-$(VERSION).tar.gz $$dir/ ; \
	rpmbuild -tb $(PACKAGE)-$(VERSION).tar.gz || true ; \
	rm -fr $$tmpdir

srpm: $(top_srcdir)/$(PACKAGE)-$(VERSION).tar.gz
	rpmbuild -ts $(top_srcdir)/$(PACKAGE)-$(VERSION).tar.gz

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

MAINTAINERCLEANFILES = \
		$(srcdir)/INSTALL \
		$(srcdir)/aclocal.m4 \
		$(srcdir)/autoscan.log \
		$(srcdir)/compile \
		$(srcdir)/config.guess \
		$(srcdir)/config.h.in \
		$(srcdir)/config.sub \
		$(srcdir)/configure \
		$(srcdir)/configure.scan \
		$(srcdir)/depcomp \
		$(srcdir)/install-sh \
		$(srcdir)/ltmain.sh \
		$(srcdir)/missing \
		$(srcdir)/mkinstalldirs \
		$(srcdir)/omf.make \
		$(srcdir)/xmldocs.make \
		$(srcdir)/gtk-doc.make \
		$(srcdir)/po/Makefile.in.in \
		`find "$(srcdir)" -type f -name Makefile.in -print`
