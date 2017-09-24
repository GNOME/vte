noinst_PROGRAMS = app

CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =

# VAPI

VAPIGEN_VAPIS = vte-$(VTE_API_VERSION).vapi

vte-@VTE_API_VERSION@.vapi: $(top_builddir)/bindings/gir/Vte-$(VTE_API_VERSION).gir
vte_@VTE_API_VERSION_U@_vapi_DEPS = \
	glib-2.0 \
	gio-2.0 \
	pango \
	gdk-$(GTK_API_VERSION) \
	gtk+-$(GTK_API_VERSION) \
	$(NULL)
vte_@VTE_API_VERSION_U@_vapi_METADATADIRS = $(srcdir)
vte_@VTE_API_VERSION_U@_vapi_FILES = $(top_builddir)/bindings/gir/Vte-$(VTE_API_VERSION).gir

vapidir = $(datadir)/vala/vapi
nodist_vapi_DATA = $(VAPIGEN_VAPIS)

CLEANFILES += $(nodist_vapi_DATA)

# VALA Test application

app_SOURCES = \
	app.vala \
	config.vapi \
	$(builddir)/vte-$(VTE_API_VERSION).vapi \
	$(NULL)

nodist_app_SOURCES = \
	appresources.c \
	$(NULL)

app_VALAFLAGS = \
	--target-glib=2.38 \
	--pkg=posix \
	--pkg=gio-2.0 \
	--pkg=gtk+-$(GTK_API_VERSION) \
	--gresources app.gresource.xml \
	$(AM_VALAFLAGS)

app_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/vte \
	-I$(top_builddir)/src/vte \
	$(AM_CPPFLAGS) \
	$(NULL)

app_CFLAGS = \
	-Wno-unused-variable \
	-Wno-unused-but-set-variable \
	$(VTE_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

app_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(NULL)

app_LDADD = \
	$(top_builddir)/src/libvte-$(VTE_API_VERSION).la \
	$(VTE_LIBS) \
	$(NULL)

if HAVE_VALA_0_31
app_VALAFLAGS += --disable-since-check
endif

if HAVE_GTK_3_16
app_VALAFLAGS += -D GTK_3_16
endif

CLEANFILES += \
	app.c \
	appresources.c \
	$(NULL)

EXTRA_DIST += \
        app.gresource.xml \
	app.ui \
	search-popover.ui \
	$(NULL)

appresources.c: app.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/app.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name app $<

.DELETE_ON_ERROR:

include $(VAPIGEN_MAKEFILE)
-include $(top_srcdir)/git.mk
