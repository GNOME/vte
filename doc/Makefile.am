SUBDIRS = reference

ctlseqs.ps: ctlseqs.ms
	tbl $^ | groff -ms > $@

ctlseqs.txt: ctlseqs.ms
	tbl $^ | nroff -c -ms > $@
