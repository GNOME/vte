dnl $Id$
dnl ---------------------------------------------------------------------------
dnl Test if we have a usable ioctl with FIONREAD, or if fcntl.h is preferred.
AC_DEFUN([CF_FCNTL_VS_IOCTL],
[
AC_MSG_CHECKING(if we may use FIONREAD)
AC_CACHE_VAL(cf_cv_use_fionread,[
	AC_TRY_COMPILE([
#if HAVE_SYS_FILIO_H
#  include <sys/filio.h>	/* FIONREAD */
#endif
	],[
long l1;
ioctl (0, FIONREAD, &l1);
	],
	[cf_cv_use_fionread=yes],
	[cf_cv_use_fionread=no])
])
AC_MSG_RESULT($cf_cv_use_fionread)
test $cf_cv_use_fionread = yes && AC_DEFINE(USE_FIONREAD)
])dnl
dnl ---------------------------------------------------------------------------
dnl Special test to workaround gcc 2.6.2, which cannot parse C-preprocessor
dnl conditionals.
AC_DEFUN([CF_POSIX_VDISABLE],
[
AC_MSG_CHECKING(if POSIX VDISABLE symbol should be used)
AC_CACHE_VAL(cf_cv_posix_vdisable,[
	AC_TRY_RUN([
#if HAVE_TERMIOS_H && HAVE_TCGETATTR
#include <termios.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(_POSIX_VDISABLE)
int main() { exit(_POSIX_VDISABLE == -1); }
#endif],
	[cf_cv_posix_vdisable=yes],
	[cf_cv_posix_vdisable=no],
	[AC_TRY_COMPILE([
#if HAVE_TERMIOS_H && HAVE_TCGETATTR
#include <termios.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif],[
#if defined(_POSIX_VDISABLE) && (_POSIX_VDISABLE != -1)
int temp = _POSIX_VDISABLE;
#else
this did not work
#endif],
	[cf_cv_posix_vdisable=yes],
	[cf_cv_posix_vdisable=no],
	)])
])
AC_MSG_RESULT($cf_cv_posix_vdisable)
test $cf_cv_posix_vdisable = yes && AC_DEFINE(HAVE_POSIX_VDISABLE)
])dnl
