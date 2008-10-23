NULL =

cataloguedir = $(GLADE_CATALOGUE_DIR)
catalogue_in_files = vte.xml.in
catalogue_DATA = $(catalogue_in_files:.xml.in=.xml)

icons = \
	hicolor_actions_16x16_widget-vte-terminal.png \
	hicolor_actions_22x22_widget-vte-terminal.png \
	$(NULL)

EXTRA_DIST = \
	$(catalogue_DATA) \
	$(icons) \
	$(NULL)

install-icons:
	for icon in $(icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		mkdir -p $(DESTDIR)$(GLADE_ICON_DIR)/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(GLADE_ICON_DIR)/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

uninstall-icons:
	-for icon in $(icons); do \
		THEME=`echo $$icon | cut -d_ -f1`; \
		CONTEXT=`echo $$icon | cut -d_ -f2`; \
		SIZE=`echo $$icon | cut -d_ -f3`; \
		ICONFILE=`echo $$icon | cut -d_ -f4`; \
		rm -f $(DESTDIR)$(GLADE_ICON_DIR)/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

validate: $(catalogue_DATA)
	xmllint --dtdvalid $(GLADE_CATALOGUE_DIR)/glade-catalog.dtd --noout $<

install-data-local: install-icons

uninstall-local: uninstall-icons

@INTLTOOL_XML_NOMERGE_RULE@
