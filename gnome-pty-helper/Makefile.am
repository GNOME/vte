libexec_PROGRAMS = gnome-pty-helper

AM_CFLAGS = @CFLAGS@ @GLIB_CFLAGS@
AM_LDFLAGS = @LDFLAGS@ @GLIB_LIBS@

gnome_pty_helper_SOURCES = \
	gnome-pty.h \
	gnome-login-support.c \
	gnome-login-support.h \
	gnome-pty-helper.c \
	gnome-utmp.c

install-exec-hook:
	chown root.utmp $(DESTDIR)$(libexecdir)/gnome-pty-helper || true
	chmod g+s       $(DESTDIR)$(libexecdir)/gnome-pty-helper || true

MAINTAINERCLEANFILES = \
		$(srcdir)/INSTALL \
		$(srcdir)/aclocal.m4 \
		$(srcdir)/config.h.in \
		$(srcdir)/configure \
		$(srcdir)/omf.make \
		$(srcdir)/xmldocs.make \
		`find "$(srcdir)" -type f -name Makefile.in -print`
