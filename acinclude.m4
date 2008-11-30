dnl From msw.
dnl
dnl a macro to check for ability to create python extensions
dnl  AM_CHECK_PYTHON_HEADERS([ACTION-IF-POSSIBLE], [ACTION-IF-NOT-POSSIBLE])
dnl function also defines PYTHON_INCLUDES
AC_DEFUN([AM_CHECK_PYTHON_HEADERS],
[AC_REQUIRE([AM_PATH_PYTHON])
AC_MSG_CHECKING(for headers required to compile python extensions)
dnl deduce PYTHON_INCLUDES
py_prefix=`$PYTHON -c "import sys; print sys.prefix"`
py_exec_prefix=`$PYTHON -c "import sys; print sys.exec_prefix"`
if test -x "$PYTHON-config"; then
    PYTHON_INCLUDES=`$PYTHON-config --includes 2>/dev/null`
else
    PYTHON_INCLUDES="-I${py_prefix}/include/python${PYTHON_VERSION}"
    if test "$py_prefix" != "$py_exec_prefix"; then
      PYTHON_INCLUDES="$PYTHON_INCLUDES -I${py_exec_prefix}/include/python${PYTHON_VERSION}"
    fi
fi
PYTHON_LIBS="-L${py_prefix}/libs -lpython${PYTHON_VERSION}"
AC_SUBST(PYTHON_INCLUDES)
AC_SUBST(PYTHON_LIBS)
dnl check if the headers exist:
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $PYTHON_INCLUDES"
AC_TRY_CPP([#include <Python.h>],dnl
[AC_MSG_RESULT(found)
$1],dnl
[AC_MSG_RESULT(not found)
$2])
CPPFLAGS="$save_CPPFLAGS"
])
