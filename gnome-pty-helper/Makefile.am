libexec_PROGRAMS = gnome-pty-helper

gnome_pty_helper_SOURCES = \
	gnome-pty.h \
	gnome-login-support.c \
	gnome-login-support.h \
	gnome-pty-helper.c \
	gnome-utmp.c

gnome_pty_helper_CFLAGS = $(SUID_CFLAGS) $(AM_CFLAGS)
gnome_pty_helper_LDFLAGS = $(SUID_LDFLAGS) $(AM_LDFLAGS)

install-exec-hook:
	chown root.utmp $(DESTDIR)$(libexecdir)/gnome-pty-helper || true
	chmod g+s       $(DESTDIR)$(libexecdir)/gnome-pty-helper || true

MAINTAINERCLEANFILES = \
		$(srcdir)/INSTALL \
		$(srcdir)/aclocal.m4 \
		$(srcdir)/config.guess \
		$(srcdir)/config.h.in \
		$(srcdir)/config.sub \
		$(srcdir)/configure \
		$(srcdir)/depcomp \
		$(srcdir)/install-sh \
		$(srcdir)/mkinstalldirs \
		$(srcdir)/missing \
		$(srcdir)/omf.make \
		$(srcdir)/xmldocs.make \
		`find "$(srcdir)" -type f -name Makefile.in -print`

-include $(top_srcdir)/git.mk
