#!/bin/sh -e

awk=${AWK:-awk}

generate() {
	echo "	/* generated file -- do not edit */"
	${awk} -F'#' '/^0x/ {print $1}' $* |\
	${awk} '{print $(NF-1),$(NF)}' $* |\
	(
	while read A B ; do
		if test `printf '%d' $A` != `printf '%d' $B` ; then
			echo "	{$A, $B},"
		fi
	done
	)
}

maphome=http://www.unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/

encodings="CNS11643 GB12345 GB2312 JIS0201 JIS0208 JIS0212 KSC5601"
if test "$#" != 0 ; then
	encodings="$*"
fi
for encoding in $encodings ; do
	if ! test -f unitable.$encoding ; then
		if ! test -f $encoding.TXT ; then
			echo -n "Retrieving "
			case $encoding in
				CNS*) wget -qc $maphome/OTHER/$encoding.TXT ;;
				GB*) wget -qc $maphome/GB/$encoding.TXT ;;
				JIS*) wget -qc $maphome/JIS/$encoding.TXT ;;
				KSC*) wget -qc $maphome/KSC/$encoding.TXT ;;
			esac
		fi
		echo -n $encoding
		generate < $encoding.TXT > unitable.$encoding
		echo .
	fi
done
