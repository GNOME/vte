libexec_PROGRAMS = gnome-pty-helper

CFLAGS = @CFLAGS@ @GLIB_CFLAGS@
LDFLAGS = @LDFLAGS@ @GLIB_LIBS@

gnome_pty_helper_SOURCES = \
	gnome-pty.h \
	gnome-login-support.c \
	gnome-login-support.h \
	gnome-pty-helper.c \
	gnome-utmp.c

install-exec-hook:
	chown root.utmp $(DESTDIR)$(vtelibdir)/gnome-pty-helper || true
	chmod g+s       $(DESTDIR)$(vtelibdir)/gnome-pty-helper || true
