# $Id$
# VAX/VMS "mms" script for VTTEST

THIS = vttest

#### Start of system configuration section. ####

DEFINES = /Define=(STDC_HEADERS,RELEASE=2)
CFLAGS	= /Listing /Include=([]) $(DEFINES)

#### End of system configuration section. ####

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
	vt220.c \
	vt420.c \
	vt52.c \
	unix_io.c \
	vms_io.c \
	xterm.c
H_SRC = \
	vttest.h \
	esc.h \
	ttymodes.h
OBJS = \
	charsets.obj, \
	color.obj, \
	esc.obj, \
	keyboard.obj, \
	main.obj, \
	mouse.obj, \
	nonvt100.obj, \
	printer.obj, \
	reports.obj, \
	reset.obj, \
	setup.obj, \
	sixel.obj, \
	status.obj, \
	vt220.obj, \
	vt420.obj, \
	vt52.obj, \
	vms_io.obj, \
	xterm.obj

SRC =	patchlev.h \
	CHANGES COPYING README BUGS \
	$(THIS).lsm $(THIS).1 \
	$(C_SRC) $(H_SRC) \
	config.hin install.sh mkdirs.sh makefile.in configure.in

all : $(THIS).exe
	@ write sys$output "** produced $?"

$(THIS).exe : $(OBJS)
	$(LINK)/exec=$(THIS) $(OBJS),sys$library:vaxcrtl/lib

$(THIS).com :
	if "''f$search("vttest.com")'" .nes. "" then delete vttest.com;*
	copy nl: vttest.com
	open/append  test_script vttest.com
	write test_script "$ temp = f$environment(""procedure"")"
	write test_script "$ temp = temp -"
	write test_script "		- f$parse(temp,,,""version"",""syntax_only"") -"
	write test_script "		- f$parse(temp,,,""type"",""syntax_only"")"
	write test_script "$ vttest :== $ 'temp'.exe"
	write test_script "$ define/user_mode sys$input  sys$command"
	write test_script "$ define/user_mode sys$output sys$command"
	write test_script "$ vttest 'p1 'p2 'p3 'p4 'p5 'p6 'p7 'p8"
	close test_script
	write sys$output "** made vttest.com"

clean :
	- if f$search("*.obj").nes."" then dele/nolog *.obj;*
	- if f$search("*.lis").nes."" then dele/nolog *.lis;*
	- if f$search("*.log").nes."" then dele/nolog *.log;*
	- if f$search("*.map").nes."" then dele/nolog *.map;*

clobber : clean
	- if f$search("$(THIS).exe").nes."" then dele/nolog $(THIS).exe;*

$(OBJS) : vttest.h
