SUBDIRS = reference openi18n

EXTRA_DIST = ctlseqs.ms readme.txt utmpwtmp.txt boxes.txt

ctlseqs.ps: ctlseqs.ms
	tbl $^ | groff -ms > $@

ctlseqs.txt: ctlseqs.ms
	tbl $^ | nroff -c -ms > $@

-include $(top_srcdir)/git.mk
