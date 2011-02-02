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
    echo "You need to install the gnome-common package"
    exit 1
}

. gnome-autogen.sh
