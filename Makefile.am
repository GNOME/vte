SUBDIRS = src python termcaps po doc
EXTRA_DIST = MAINTAINERS HACKING vte.pc.in vte-uninstalled.pc.in
DISTCLEANFILES =

if BUILD_GNOME_PTY_HELPER
SUBDIRS += gnome-pty-helper
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = vte.pc

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
