# $Id$
# Makefile-template for 'vttest'

THIS = vttest
RELEASE = 2

#### Start of system configuration section. ####

srcdir = .

CC		= gcc
LINK		= $(CC)
INSTALL 	= /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA	= ${INSTALL} -m 644

LIBS		= 
CFLAGS		= -g -O2
LDFLAGS 	= 

prefix		= /usr/local
exec_prefix	= ${prefix}

bindir		= ${exec_prefix}/bin
mandir		= ${prefix}/man/man$(manext)
manext		= 1

#### End of system configuration section. ####

SHELL		= /bin/sh

DEFINES 	= -DHAVE_CONFIG_H -DRELEASE=$(RELEASE)
INCLUDES	= -I. -I$(srcdir)
CPPFLAGS	= $(DEFINES) $(INCLUDES) 

.c.o:
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $<

C_SRC = \
	charsets.c \
	color.c \
	esc.c \
	keyboard.c \
	main.c \
	mouse.c \
	nonvt100.c \
	printer.c \
	reports.c \
	reset.c \
	setup.c \
	sixel.c \
	status.c \
	ttymodes.c \
	unix_io.c \
	vt220.c \
	vt420.c \
	vt52.c \
	xterm.c
H_SRC = \
	vttest.h \
	esc.h \
	ttymodes.h
OBJS  = \
	charsets.o \
	color.o \
	esc.o \
	keyboard.o \
	main.o \
	mouse.o \
	nonvt100.o \
	printer.o \
	reports.o \
	reset.o \
	setup.o \
	sixel.o \
	status.o \
	ttymodes.o \
	unix_io.o \
	vt220.o \
	vt420.o \
	vt52.o \
	xterm.o

SRC =	patchlev.h \
	CHANGES COPYING README BUGS \
	$(THIS).lsm $(THIS).1 \
	$(C_SRC) $(H_SRC) \
	tech.set \
	config.hin install.sh mkdirs.sh makefile.in configure.in

DISTFILES = MANIFEST configure $(SRC)

all:	$(THIS)

$(THIS): $(OBJS)
	-mv $(THIS) o$(THIS)
	$(LINK) $(CFLAGS) $(LDFLAGS) -o $(THIS) $(OBJS) $(LIBS)

$(OBJS): config.h $(THIS).h patchlev.h

install: all installdirs
	$(INSTALL_PROGRAM) $(THIS) $(bindir)/$(THIS)
	$(INSTALL_DATA) $(srcdir)/$(THIS).1 $(mandir)/$(THIS).$(manext)

installdirs:
	$(SHELL) ${srcdir}/mkdirs.sh $(bindir) $(mandir)

uninstall:
	rm -f $(bindir)/$(THIS)
	rm -f $(mandir)/$(THIS).$(manext)

makefile: makefile.in config.status
	./config.status

config.status: configure
	./config.status --recheck

mostlyclean:
	rm -f *.o o$(THIS) core *~ *.BAK *.trace *.atac

clean: mostlyclean
	rm -f $(THIS)

distclean: clean
	rm -f makefile config.log config.cache config.status config.h

realclean: distclean
	rm -f tags TAGS # don't remove configure!

tags:	$(C_SRC) $(HDRS)
	ctags $(C_SRC) $(HDRS)

TAGS:	$(C_SRC) $(HDRS)
	etags $(C_SRC) $(HDRS)

# I keep my sources in RCS, and assign a symbolic release to the current patch
# level.  The 'manifest' script knows how to build a list of files for a given
# revision.
MANIFEST: patchlev.h
#	manifest -rv$(RELEASE)`fgrep PATCHLEVEL patchlev.h | sed -e s'/^[^0-9]*/_/'`
	manifest -rd`fgrep PATCH_DATE patchlev.h | sed -e s'/^[^0-9]*//'`

dist: MANIFEST
	- rm -f .fname .files
	fgrep PATCH_DATE patchlev.h | sed -e 's/[^0-9.]//g' -e 's/^/$(THIS)-/' >.fname
	cat MANIFEST | grep ' ' | egrep -v ' subdirectory$$' | sed -e 's/\ .*//' | uniq >.files
	rm -rf `cat .fname`
	TOP=`cat .fname`; mkdir $$TOP `cat .files | grep / | sed -e 's@/.*@@' | sed -e s@\^@$$TOP/@ | uniq`
	for file in `cat .files`; do \
	  ln $(srcdir)/$$file `cat .fname`/$$file \
	    || { echo copying $$file instead; cp $$file `cat .fname`/$$file; }; \
	done
	tar -cf - `cat .fname` | gzip >`cat .fname`.tgz
	rm -rf `cat .fname` .fname .files

# Some of the output will be uuencoded because the test scripts include
# <CR><LF> terminated ".bat" files for MS-DOS.
dist-shar: MANIFEST
	- rm -f .fname .files
	fgrep PATCH_DATE patchlev.h | sed -e 's/[^0-9.]//g' -e 's/^/$(THIS)-/' >.fname
	cat MANIFEST | grep ' ' | egrep -v ' subdirectory$$' | sed -e 's/\ .*//' | uniq >.files
	shar -M -n`cat .fname` -opart -l50 `cat .files`
	- rm -f .fname .files

$(THIS).o:	config.h

lint:
	$(LINT) $(CPPFLAGS) $(C_SRC)

check: $(THIS)
	@echo 'Sorry -- You must run this interactively.'
