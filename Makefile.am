SUBDIRS = src gnome-pty-helper python termcaps po doc
EXTRA_DIST = HACKING vte.spec vte.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = vte.pc

CVSTAG=$(shell echo $(PACKAGE)-$(VERSION) | sed 's,[\.\-],_,g')

forcetag:
	cvs tag -cFR $(CVSTAG) .

reallytag:
	cvs tag -cR $(CVSTAG) .

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
