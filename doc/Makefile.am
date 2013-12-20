NULL =

SUBDIRS = reference openi18n

EXTRA_DIST = \
        boxes.txt \
	ctlseqs.ms \
	readme.txt \
        rewrap.txt \
	utmpwtmp.txt \
	$(NULL)

ctlseqs.ps: ctlseqs.ms
	$(AM_V_GEN) tbl $^ | groff -ms > $@

ctlseqs.txt: ctlseqs.ms
	$(AM_V_GEN) tbl $^ | nroff -c -ms > $@

-include $(top_srcdir)/git.mk
