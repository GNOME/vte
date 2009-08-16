SUBDIRS = reference openi18n

EXTRA_DIST = ctlseqs.ms readme.txt utmpwtmp.txt boxes.txt

ctlseqs.ps: ctlseqs.ms
	$(AM_V_GEN) tbl $^ | groff -ms > $@

ctlseqs.txt: ctlseqs.ms
	$(AM_V_GEN) tbl $^ | nroff -c -ms > $@

-include $(top_srcdir)/git.mk
