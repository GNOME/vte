EXTRA_DIST = xterm xterm.baseline
termcapdir = $(pkgdatadir)/termcap
termcap_DATA = xterm $(VTE_DEFAULT_EMULATION)
$(VTE_DEFAULT_EMULATION): xterm
	sed -e s,^xterm:,$(VTE_DEFAULT_EMULATION):,g \
	    -e s,^xterm\|,$(VTE_DEFAULT_EMULATION)\|,g $< > $@
