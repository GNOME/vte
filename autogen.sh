#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir
PROJECT=vte
TEST_TYPE=-f

DIE=0

have_libtool=false
if libtool --version < /dev/null > /dev/null 2>&1 ; then
	libtool_version=`libtoolize --version |  libtoolize --version | sed 's/^[^0-9]*\([0-9.][0-9.]*\).*/\1/'`
	case $libtool_version in
	    1.4*|1.5*)
		have_libtool=true
		;;
	esac
fi

if $have_libtool ; then : ; else
	echo
	echo "You must have libtool 1.4 or newer installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
fi

for autoconf in autoconf autoconf-2.57 autoconf-2.56 autoconf-2.55 autoconf-2.54 autoconf-2.53 autoconf-2.52 autoconf-2.51 autoconf-2.50 autoconf-2.5 ; do
	if "$autoconf" --version < /dev/null > /dev/null 2>&1 ; then
		version=`"$autoconf" --version | head -1 | awk '{print $NF}'`
		acmajor=`echo "$version" | cut -f1 -d.`
		acminor=`echo "$version" | cut -f2 -d.`
		if test "$acmajor" -gt 3 ; then
			break
		fi
		if test "$acmajor" -ge 2 ; then
			if test "$acminor" -ge 50 ; then
				break
			fi
		fi
	fi
done
if ! "$autoconf" --version < /dev/null > /dev/null 2>&1 ; then
	echo
	echo "You must have autoconf 2.52 installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
fi
autoheader=`echo "$autoconf" | sed s,autoconf,autoheader,g`

(freetype-config --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have freetype 2 installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution, or get the"
	echo "source tarball at ftp://ftp.freetype.org/freetype/freetype2"
	DIE=1
}

have_automake=false
for automakev in 1.7 1.6 ; do
	if automake-$automakev --version < /dev/null > /dev/null 2>&1 ; then
		have_automake=true
		break;
	fi
done
if $have_automake ; then : ; else
	echo
	echo "You must have automake 1.6 installed to compile $PROJECT."
	echo "Get ftp://ftp.gnu.org/pub/gnu/automake/automake-1.6.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
fi

if test "$DIE" -eq 1; then
	exit 1
fi

if test -z "$AUTOGEN_SUBDIR_MODE"; then
        if test -z "$*"; then
                echo "I am going to run ./configure with no arguments - if you wish "
                echo "to pass any to it, please specify them on the $0 command line."
        fi
fi

case $CC in
*xlc | *xlc\ * | *lcc | *lcc\ *) am_opt=--include-deps;;
esac

libtoolize -f -c
glib-gettextize -f -c
touch config.h.in
aclocal-$automakev $ACLOCAL_FLAGS

# optionally feature autoheader
$autoheader
automake-$automakev -a -c $am_opt
$autoconf

cd gnome-pty-helper
touch config.h.in
aclocal-$automakev $ACLOCAL_FLAGS
$autoheader
automake-$automakev -a -c $am_opt
$autoconf

cd $ORIGDIR

if test -z "$AUTOGEN_SUBDIR_MODE"; then
        $srcdir/configure --enable-maintainer-mode --enable-gtk-doc "$@"
	chmod -Rf u+w $srcdir
        echo 
        echo "Now type 'make' to compile $PROJECT."
fi
