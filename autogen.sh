#!/bin/bash -e
CFLAGS="${CFLAGS:--g3 -O -Wall}" ; export CFLAGS
set -x
libtoolize -f -c
autoheader-2.13 || autoheader
aclocal-1.5 || aclocal
automake-1.5 -a || automake -a
autoconf-2.13 || autoconf
if test -f config.cache ; then
	rm -f config.cache
fi
./configure $@
