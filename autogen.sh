#!/bin/bash -e
#PROTOTYPES="-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations"
WARNINGS="-Wcast-align $PROTOTYPES"
CFLAGS="${CFLAGS:--g3 -Wall $WARNINGS}" ; export CFLAGS
set -x
libtoolize -f
autoheader
aclocal
automake -a
autoconf
./configure --disable-shared --enable-maintainer-mode $@
