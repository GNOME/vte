#!/usr/bin/env bash

export LC_ALL=C

LDD=ldd
GREP=grep

if $LDD .libs/libvte-$VTE_API_VERSION.so | $GREP 'libstdc++' &>/dev/null; then
    echo "FAIL: libvte-$VTE_API_VERSION.so is linked to libstdc++"
    exit 1
fi

exit 0
