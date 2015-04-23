#!/usr/bin/env bash

export LC_ALL=C

LDD=ldd
GREP=grep

for cxxlib in 'libstdc++' 'libc++'; do
    if $LDD .libs/libvte-$VTE_API_VERSION.so | $GREP "$cxxlib" &>/dev/null; then
        echo "FAIL: libvte-$VTE_API_VERSION.so is linked to $cxxlib"
        exit 1
    fi
done

exit 0
