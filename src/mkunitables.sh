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

cjkmaphome=http://www.unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/
cpmaphome=http://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/

encodings="CNS11643 CP437 GB12345 GB2312 JIS0201 JIS0208 JIS0212 KSX1001"
if test "$#" != 0 ; then
	encodings="$*"
fi
for encoding in $encodings ; do
	if ! test -f unitable.$encoding ; then
		if ! test -f $encoding.TXT ; then
			echo -n "Retrieving "
			case $encoding in
				CP*) wget -qc $cpmaphome/$encoding.TXT;;
				CNS*) wget -qc $cjkmaphome/OTHER/$encoding.TXT;;
				GB*) wget -qc $cjkmaphome/GB/$encoding.TXT;;
				JIS*) wget -qc $cjkmaphome/JIS/$encoding.TXT;;
				KSX1001)
					wget -qc $cjkmaphome/KSC/$encoding.TXT
					echo 0x2266 0x20AC >> $encoding.TXT
					echo 0x2267 0x00AE >> $encoding.TXT
					;;
			esac
		fi
		echo -n $encoding
		generate < $encoding.TXT > unitable.$encoding
		echo .
	fi
done
