#!/usr/bin/python
import os, re, string

try:
	unidata = open("EastAsianWidth.txt", "r")
except:
	os.system("wget --passive-ftp -c ftp://ftp.unicode.org/Public/UNIDATA/EastAsianWidth.txt")
	unidata = open("EastAsianWidth.txt", "r")
out = open("uniwidths", "w")
ranges = []
specifics = []
rangere = re.compile("^([0123456789ABCDEF]+)\.\.([0123456789ABCDEF]+);A")
specificre = re.compile("^([0123456789ABCDEF]+);A")
for line in unidata.readlines():
	match = re.match(specificre, line)
	if match:
		if match.groups().__len__() > 0:
			specifics.append(match.groups()[0])
	match = re.match(rangere, line)
	if match:
		if match.groups().__len__() > 1:
			ranges.append((match.groups()[0], match.groups()[1]))

print >> out, "static const struct {"
print >> out, "\tgunichar start, end;"
print >> out, "} _vte_iso2022_ambiguous_ranges[] = {"
for range in ranges:
	print >> out, "\t{0x%x, 0x%x}," % (string.atol(range[0], 16), string.atol(range[1], 16))
print >> out, "};"

print >> out, "static const gunichar _vte_iso2022_ambiguous_chars[] = {"
for specific in specifics:
	print >> out, "\t0x%x," % (string.atol(specific, 16))
print >> out, "};"

