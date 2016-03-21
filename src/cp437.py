#!/usr/bin/python
from __future__ import print_function
import sys
if sys.version_info > (3,):
    PY3 = True
    two_bytes = bytearray(b'\x00\x20')
else:
    PY3 = False
print('%s' % '(U')
i = 128
while (i < 256):
    if PY3:
        # avoid encoding of print function
        two_bytes[0] = i
        sys.stdout.flush()
        sys.stdout.buffer.write(two_bytes)
    else:
        print("%c" % i, end=' ')
    if ((i % 32) == 31):
        print("")
    i = i + 1
print('%s' % '(B)0*B+B')
