EXTRA_DIST = xterm xterm.baseline
termcapdir = $(pkgdatadir)/termcap-$(VTE_API_VERSION)
termcap_DATA = xterm

if VTE_DEFAULT_EMULATION
termcap_DATA += $(VTE_DEFAULT_EMULATION)
$(VTE_DEFAULT_EMULATION): xterm
	$(AM_V_GEN) $(SED) -e s,^xterm:,$(VTE_DEFAULT_EMULATION):,g \
	    -e s,^xterm\|,$(VTE_DEFAULT_EMULATION)\|,g $< > $@
endif

-include $(top_srcdir)/git.mk
