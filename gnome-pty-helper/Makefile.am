vtelibdir = $(libdir)/vte
vtelib_PROGRAMS = gnome-pty-helper

gnome_pty_helper_SOURCES = \
	gnome-pty.h \
	gnome-login-support.c \
	gnome-login-support.h \
	gnome-pty-helper.c \
	gnome-utmp.c

install-exec-hook:
	chown root.utmp $(DESTDIR)/$(vtelibdir)/gnome-pty-helper || true
	chmod g+s       $(DESTDIR)/$(vtelibdir)/gnome-pty-helper || true
