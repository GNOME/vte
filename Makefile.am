SUBDIRS = src
EXTRA_DIST = HACKING vte.spec vte.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = vte.pc

CVSTAG=$(shell echo $(PACKAGE)-$(VERSION) | sed 's,[\.\-],_,g')

tag:
	$(top_srcdir)/autogen.sh
	cvs tag -cFR $(CVSTAG) .

archive:
	root=`cat $(top_srcdir)/CVS/Root` ; \
	repo=`cat $(top_srcdir)/CVS/Repository` ; \
	tmpdir=`mktemp -d /tmp/$(PACKAGE)-cvsXXXXXX` ; \
	cd $$tmpdir ; \
	cvs -d $$root export -r $(CVSTAG) $$repo ; \
	cd $$repo ; \
	./autogen.sh ; \
	make distcheck ; \
	cp $(PACKAGE)-$(VERSION).tar.gz $(top_srcdir) ; \
	rm -fr $$tmpdir

srpm: $(top_srcdir)/$(PACKAGE)-$(VERSION).tar.gz
	rpm -ts $(top_srcdir)/$(PACKAGE)-$(VERSION).tar.gz
