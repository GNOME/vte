SUBDIRS = src python termcaps po doc perf

EXTRA_DIST = \
	MAINTAINERS \
	HACKING \
	autogen.sh \
	ChangeLog.pre-git

DISTCLEANFILES =

if BUILD_GNOME_PTY_HELPER
SUBDIRS += gnome-pty-helper
endif

if ENABLE_GLADE_CATALOGUE
SUBDIRS += glade
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = vte.pc

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

MAINTAINERCLEANFILES = \
		ChangeLog \
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

# Build ChangeLog from GIT  history
ChangeLog:
	@if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
