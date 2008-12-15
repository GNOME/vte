#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="vte"

(test -f $srcdir/configure.in \
  && test -f $srcdir/README \
  && test -d $srcdir/src) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

echo "checking for GNU gperf"
which gperf || {
    echo "You need to install GNU gperf"
    exit 1
}

echo "checking for gnome-autogen.sh"
which gnome-autogen.sh || {
    echo "You need to install gnome-common from the GNOME SVN"
    exit 1
}

USE_COMMON_DOC_BUILD=yes
REQUIRED_AUTOMAKE_VERSION=1.9
REQUIRED_INTLTOOL_VERSION=0.40.0

. gnome-autogen.sh
