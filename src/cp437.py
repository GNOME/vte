#!/usr/bin/python
print '%s' % '(U'
i = 128
while (i < 256):
	print "%c" % i,
	if ((i % 32) == 31):
		print ""
	i = i + 1
print '%s' % '(B)0*B+B'
