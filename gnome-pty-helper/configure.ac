AC_INIT([gnome-pty-helper],[1.95.0],[http://bugzilla.gnome.org/enter_bug.cgi?product=vte])

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-bzip2])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([gnome-pty.h])

AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS

AM_MAINTAINER_MODE([enable])

AC_CHECK_HEADERS(sys/syslimits.h sys/time.h sys/types.h sys/un.h alloca.h lastlog.h libutil.h paths.h pty.h stropts.h termios.h ttyent.h util.h utmp.h utmpx.h pty.h util.h libutil.h ttyent.h)
have_openpty=0
AC_CHECK_LIB(c,grantpt,true,[AC_CHECK_LIB(pt,grantpt)])
AC_CHECK_LIB(c,openpty,true,[AC_CHECK_LIB(util,openpty)])
AC_CHECK_LIB(c,sendmsg,true,[AC_CHECK_LIB(socket,sendmsg,LIBS="$LIBS -lsocket -lnsl",,-lnsl)])
AC_CHECK_FUNCS(endutent fcntl forkpty getttyent getutent getutmpx grantpt flock login_tty openpty revoke sendmsg seteuid setreuid setutent strrchr updwtmp updwtmpx utmpname utmpxname)
GPH_CHECK_UTMP

AC_CHECK_HEADERS(stropts.h)

AC_ARG_VAR([SUID_CFLAGS],
	   [CFLAGS used for binaries which are usually with the suid bit])
AC_ARG_VAR([SUID_LDFLAGS],
	   [LDFLAGS used for binaries which are usually with the suid bit])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
