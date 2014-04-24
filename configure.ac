m4_define([version_major],0)
m4_define([version_minor],37)
m4_define([version_micro],0)
m4_define([version_triplet],version_major.version_minor.version_micro)

AC_PREREQ([2.63])
AC_INIT([vte],
        [version_triplet],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=vte],
        [vte])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/vte.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-bzip2 -Wno-portability])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

if test -z "$enable_maintainer_mode"; then
  enable_maintainer_mode=yes
fi
AM_MAINTAINER_MODE([enable])

################################################################################
# Libtool versioning
################################################################################
m4_define([lt_revision], m4_if(m4_eval(version_minor%2),1,0,version_micro))
m4_define([lt_age], m4_eval(version_minor*100+version_micro-lt_revision))
m4_define([lt_current], m4_eval(version_major+lt_age))
m4_define([lt_triplet],lt_current:lt_revision:lt_age)

LT_VERSION_INFO="lt_triplet()"
AC_SUBST([LT_VERSION_INFO])

################################################################################
# GTK+
################################################################################

VTE_API_MAJOR_VERSION=2
VTE_API_MINOR_VERSION=91
VTE_API_VERSION=$VTE_API_MAJOR_VERSION.$VTE_API_MINOR_VERSION
GTK_API_VERSION=3.0
GTK_REQUIRED=3.8.0
GLADE_API_VERSION=2.0

AC_SUBST([VTE_API_VERSION])
AC_SUBST([VTE_API_VERSION_U],[AS_TR_SH([$VTE_API_VERSION])])
AC_SUBST([VTE_API_MAJOR_VERSION])
AC_SUBST([VTE_API_MINOR_VERSION])
AC_SUBST([GTK_API_VERSION])

################################################################################

# Check for programs
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
AC_HEADER_STDC
AM_PROG_CC_C_O
AC_PROG_SED

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

################################################################################
# Enable debugging messages and additional run-time checks.
################################################################################

AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug],
                              [enable extra debugging checks and logging messages])],
              [],[enable_debug=no])
if test "x$enable_debug" = "xyes" ; then
        AC_MSG_RESULT([enabling debug checks and messages])
        AC_DEFINE(VTE_DEBUG,1,[Enable debugging messages.])
fi

################################################################################
# i18n
################################################################################

IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=vte-$VTE_API_VERSION
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [Package translatable messages domain name.])
AM_GLIB_GNU_GETTEXT
AC_CHECK_DECLS(bind_textdomain_codeset,,,[#include "libintl.h"])

################################################################################
# Compilation
################################################################################

CC_CHECK_FLAGS_APPEND([AM_CFLAGS],[CFLAGS],[ \
  -pipe \
  -Waggregate-return \
  -Wall \
  -Wcast-align \
  -Wendif-labels \
  -Werror=declaration-after-statement \
  -Werror=format=2 \
  -Werror=format-nonliteral \
  -Werror=format-security \
  -Werror=implicit-function-declaration \
  -Werror=init-self \
  -Werror=missing-include-dirs \
  -Werror=missing-prototypes \
  -Werror=pointer-arith \
  -Wextra \
  -Wfloat-equal \
  -Wformat-nonliteral \
  -Wimplicit \
  -Wlogical-op \
  -Wmissing-declarations \
  -Wmissing-include-dirs \
  -Wmissing-format-attribute \
  -Wmissing-noreturn \
  -Wnested-externs \
  -Wno-missing-field-initializers \
  -Wno-switch-enum \
  -Wno-unused-parameter \
  -Wold-style-definition \
  -Wpacked \
  -Wshadow \
  -Wsign-compare \
  -Wstrict-aliasing=2 \
  -Wstrict-prototypes \
  -Wundef \
  -Wuninitialized \
  -Wunsafe-loop-optimizations \
  -Wwrite-strings \
  -fno-common \
  -fdiagnostics-show-option \
  -fno-strict-aliasing \
  -fstack-protector \
  -fstack-protector-strong \
  -Wno-deprecated-declarations \
])

##########################################################
# Check for -Bsymbolic-functions linker flag used to avoid
# intra-library PLT jumps, if available.
##########################################################

AC_ARG_ENABLE([Bsymbolic],
  [AS_HELP_STRING([--disable-Bsymbolic],
                  [disable linking with -Bsymbolic])],
  [],[enable_Bsymbolic=yes])

VTE_LDFLAGS=
if test "$enable_Bsymbolic" != "no"; then
  CC_CHECK_LDFLAGS(["-Wl,-Bsymbolic-functions"],
    [VTE_LDFLAGS="$VTE_LDFLAGS -Wl,-Bsymbolic-functions"],
    [if test "$enable_Bsymbolic" = "auto"; then
       AC_MSG_WARN([-Bsymbolic not supported by ld; disabling])
       enable_Bsymbolic=no
     else
       AC_MSG_ERROR([-Bsymbolic requested but not supported by ld. Use --disable-Bsymbolic to disable])
     fi])
fi

AC_SUBST([VTE_LDFLAGS])

################################################################################
# Core
################################################################################

GLIB_REQUIRED=2.40.0
GIO_REQUIRED=2.40.0
PANGO_REQUIRED=1.22.0
AC_DEFINE(GDK_MULTIHEAD_SAFE,1,[Force use of GDK multihead-safe APIs.])

AC_PATH_PROG([GLIB_GENMARSHAL],[glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

VTE_DEFAULT_EMULATION=xterm
AC_DEFINE_UNQUOTED(VTE_DEFAULT_EMULATION,"$VTE_DEFAULT_EMULATION",[The default terminal type to be emulated.])

# Check for headers.
AC_CHECK_HEADERS(sys/select.h sys/syslimits.h sys/termios.h sys/un.h sys/wait.h stropts.h termios.h util.h wchar.h)
AC_HEADER_TIOCGWINSZ

# Check for PTY handling functions.
AC_CHECK_FUNCS([cfmakeraw fork setsid setpgid getpgid getpt grantpt unlockpt posix_openpt ptsname ptsname_r tcgetattr tcsetattr])

# Misc I/O routines.
AC_CHECK_FUNCS([pread pwrite fwrite_unlocked])

# Pull in the right libraries for various functions which might not be
# bundled into an exploded libc.
AC_CHECK_FUNC(socket,[have_socket=1],AC_CHECK_LIB(socket,socket,[have_socket=1; LIBS="$LIBS -lsocket"]))
AC_CHECK_FUNC(socketpair,[have_socketpair=1],AC_CHECK_LIB(socket,socketpair,[have_socketpair=1; LIBS="$LIBS -lsocket"]))
AC_CHECK_FUNC(recvmsg,[have_recvmsg=1],AC_CHECK_LIB(socket,recvmsg,[have_recvmsg=1; LIBS="$LIBS -lsocket -lnsl"]))
AC_CHECK_FUNC(openpty,[have_openpty=1],AC_CHECK_LIB(util,openpty,[have_openpty=1; LIBS="$LIBS -lutil"]))
if test x$have_socket = x1 ; then
	AC_DEFINE(HAVE_SOCKET,1,[Define if you have the socket function.])
fi
if test x$have_socketpair = x1 ; then
	AC_DEFINE(HAVE_SOCKETPAIR,1,[Define if you have the socketpair function.])
fi
if test x$have_recvmsg = x1 ; then
	AC_DEFINE(HAVE_RECVMSG,1,[Define if you have the recvmsg function.])
fi
if test x$have_openpty = x1 ; then
	AC_DEFINE(HAVE_OPENPTY,1,[Define if you have the openpty function.])
fi
AC_CHECK_FUNC(floor,,AC_CHECK_LIB(m,floor,LIBS=["$LIBS -lm"]))
AC_CHECK_FUNCS([ceil floor round])

# Search for the required modules.

VTE_PKGS="glib-2.0 >= $GLIB_REQUIRED gobject-2.0 pango >= $PANGO_REQUIRED gtk+-$GTK_API_VERSION >= $GTK_REQUIRED gobject-2.0 gio-2.0 gio-unix-2.0"
PKG_CHECK_MODULES([VTE],[$VTE_PKGS])
AC_SUBST([VTE_PKGS])

PKG_CHECK_MODULES([TERMINFO],[tinfo])
PKG_CHECK_MODULES([GLIB],[glib-2.0 >= $GLIB_REQUIRED])
PKG_CHECK_MODULES([GOBJECT],[gobject-2.0])
PKG_CHECK_MODULES([GTK],[gtk+-$GTK_API_VERSION >= $GTK_REQUIRED])

# terminfo

save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $TERMINFO_CFLAGS"
AC_CHECK_HEADERS([ncurses.h term.h tic.h],[],[AC_MSG_ERROR([ncurses terminfo headers not found])])
CFLAGS="$save_CFLAGS"

wcs_funcs_includes="
#ifdef HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif
#ifdef HAVE_WCHAR_H
# include <wchar.h>
#endif
"

AC_CHECK_TYPES(wint_t, AC_DEFINE(HAVE_WINT_T, , [Defined when the wint_t type is supported]), ,$wcs_funcs_includes)

################################################################################
# GNOME PTY Helper
################################################################################

AC_ARG_ENABLE(gnome-pty-helper, [AS_HELP_STRING(--enable-gnome-pty-helper,Build a setuid helper for opening ptys [default=yes])], enable_gnome_pty_helper="$enableval", enable_gnome_pty_helper=yes)
if test "$enable_gnome_pty_helper" != no; then
	AC_DEFINE(VTE_USE_GNOME_PTY_HELPER,1,[Define if you intend to use gnome-pty-helper.])
	AC_CONFIG_SUBDIRS(gnome-pty-helper)
fi
AM_CONDITIONAL(BUILD_GNOME_PTY_HELPER,[test "$enable_gnome_pty_helper" != no])

################################################################################
# Glade catalogue
################################################################################

AC_MSG_CHECKING([whether to install the glade catalogue])
AC_ARG_ENABLE([glade-catalogue],
  [AS_HELP_STRING([--disable-glade],[Disable installing the glade catalogue])],
  [],[enable_glade_catalogue=no])
AC_MSG_RESULT([$enable_glade_catalogue])

if test "$enable_glade_catalogue" = "yes"; then
  GLADE_CATALOGUE_DIR="$($PKG_CONFIG --variable=catalogdir gladeui-$GLADE_API_VERSION)"
  GLADE_ICON_DIR="$($PKG_CONFIG --variable=pixmapdir gladeui-$GLADE_API_VERSION)"
  GLADE_MODULE_DIR="$($PKG_CONFIG --variable=moduledir gladeui-$GLADE_API_VERSION)"

  if test -z "$GLADE_CATALOGUE_DIR" -o -z "$GLADE_MODULE_DIR"; then
    AC_MSG_ERROR([gladeui-$GLADE_API_VERSION not found])
  fi

  AC_SUBST([GLADE_CATALOGUE_DIR])
  AC_SUBST([GLADE_ICON_DIR])
  AC_SUBST([GLADE_MODULE_DIR])
fi
  
AM_CONDITIONAL([ENABLE_GLADE_CATALOGUE],[test "$enable_glade_catalogue" = "yes"])

# ********
# Bindings
# ********

# No automagic please!
if test -z "$enable_introspection"; then
  enable_introspection=yes
fi

GOBJECT_INTROSPECTION_CHECK([0.9.0])

# No automagic please!
if test -z "$enable_vala"; then
  enable_vala=yes
fi

VAPIGEN_CHECK([0.18])

# ****************
# Test Application
# ****************

AC_MSG_CHECKING([whether to build the test application])
AC_ARG_ENABLE([test-application],
        [AS_HELP_STRING([--disable-test-application],[Disable test application])],
        [],[enable_test_application=$enable_vala])
AC_MSG_RESULT([$enable_test_application])

AM_PROG_VALAC([0.18.0],[],[
  if test "$enable_test_application" != "no"; then
    AC_MSG_ERROR([vala support enabled by vala compiler not found])
  fi])

AM_CONDITIONAL([ENABLE_TEST_APPLICATION],[test "$enable_test_application" = "yes"])

# ***************
# Auxiliary tools
# ***************

AC_ARG_VAR([GLIB_COMPILE_RESOURCES],[the glib-compile-resources programme])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources],[])
if test -z "$GLIB_COMPILE_RESOURCES"; then
  AC_MSG_ERROR([glib-compile-resources not found])
fi

AC_ARG_VAR([XMLLINT],[the xmllint programme])
AC_PATH_PROG([XMLLINT],[xmllint],[])
if test -z "$XMLLINT"; then
  AC_MSG_ERROR([xmllint not found])
fi

################################################################################

GTK_DOC_CHECK([1.13],[--flavour no-tmpl])

AC_SUBST([GLIB_PREFIX],[$($PKG_CONFIG --variable=prefix glib-2.0)])
AC_SUBST([GTK_PREFIX],[$($PKG_CONFIG --variable=prefix gtk+-$GTK_API_VERSION)])

################################################################################

AC_SUBST([VTE_MAJOR_VERSION],[version_major])
AC_SUBST([VTE_MINOR_VERSION],[version_minor])
AC_SUBST([VTE_MICRO_VERSION],[version_micro])

AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_LDFLAGS])

################################################################################

AC_CONFIG_FILES([
Makefile
src/Makefile
src/vteversion.h
src/vte.gresource.xml
perf/Makefile
po/Makefile.in
doc/Makefile
doc/openi18n/Makefile
doc/reference/Makefile
doc/reference/version.xml
glade/Makefile
glade/vte.xml.in
])

AC_CONFIG_FILES([vte-${VTE_API_VERSION}.pc:vte.pc.in],[VTE_API_VERSION=${VTE_API_VERSION}])

AC_OUTPUT

#####################
# Print configuration
#####################
cat <<EOF | tee -a config.log

## -------------- ##
## Configuration. ##
## -------------- ##

Configuration for libvte $VERSION for gtk+-$GTK_API_VERSION
	Installing Glade catalogue: $enable_glade_catalogue
	Debugging: $enable_debug
	Introspection: $enable_introspection
        Vala bindings: $enable_vala
        Test application: $enable_test_application
EOF
