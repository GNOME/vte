#!/bin/bash -e
CFLAGS="${CFLAGS:--g3 -O -Wall}" ; export CFLAGS
set -x
libtoolize -f -c
autoheader
aclocal-1.5 || aclocal
automake-1.5 -a || automake -a
autoconf
if test -f config.cache ; then
	rm -f config.cache
fi
./configure $@
