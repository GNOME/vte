#!/usr/bin/python

import os, string, sys

def scanheader(filename):
	definitions = {}
	file = open(filename, "r")
	if file:
		for line in file.readlines():
			if line[:12] == '#define GDK_':
				fields = string.split(line)
				fields[2] = string.lower(fields[2])
				if definitions.has_key(fields[2]):
					definitions[fields[2]] = definitions[fields[2]] + ", " + fields[1]
				else:
					definitions[fields[2]] = fields[1]
		file.close()
	for key in definitions.keys():
		print '\tcase %s: return \"%s\";' % (key, definitions[key])

print '\t/* this file is auto-generated -- do not edit */'

if len(sys.argv) > 1:
	for header in sys.argv[1:]:
		scanheader(header)
else:
	scanheader("/usr/include/gtk-2.0/gdk/gdkkeysyms.h")
