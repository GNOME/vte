#!/bin/bash -e
CFLAGS="${CFLAGS:--g3 -Wall}" ; export CFLAGS
set -x
libtoolize -f
autoheader
aclocal
automake -a
autoconf
if test -f config.cache ; then
	rm -f config.cache
fi
./configure --disable-shared --enable-maintainer-mode $@
