SUBDIRS = src
EXTRA_DIST = HACKING vte.spec vte.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = vte.pc

CVSTAG=$(shell echo $(PACKAGE)-$(VERSION) | sed 's,[\.\-],_,g')

tag:
	cvs tag -cFR $(CVSTAG) .

archive:
	root=`cat CVS/Root` ; \
	repo=`cat CVS/Repository` ; \
	dir=`pwd` ; \
	tmpdir=`mktemp -d /tmp/$(PACKAGE)-cvsXXXXXX` ; \
	cd $$tmpdir ; \
	cvs -d $$root export -r $(CVSTAG) $$repo ; \
	cd $$repo ; \
	./autogen.sh ; \
	make distcheck ; \
	cp $(PACKAGE)-$(VERSION).tar.gz $$dir ; \
	rm -fr $$tmpdir

srpm: $(PACKAGE)-$(VERSION).tar.gz
	rpm -ts $(PACKAGE)-$(VERSION).tar.gz
