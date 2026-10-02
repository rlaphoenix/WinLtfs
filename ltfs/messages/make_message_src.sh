#!/bin/sh

set -e

GENRB=genrb
PKGDATA=pkgdata

if [ "$#" -ne "1" ]; then
	echo "Usage: $0 object_file"
	exit 1
fi

BASENAME=`echo $1 | sed -e 's/_dat\.o$//'`

cd ${BASENAME}

make_obj() {
	# Create a fresh work directory
	if [ -d work ]; then
		rm -rf work
	fi
	mkdir work

	# Generate files
	${GENRB} -d work -q *.txt
	cd work
	ls *.res >packagelist.txt

	# Modern ICU pkgdata requires an options file (-O) for static/dll modes
	PKGDATA_INC=${PKGDATA_INC:-/mingw64/lib/icu/current/pkgdata.inc}
	if [ -f "$PKGDATA_INC" ]; then
		sed '/^GENLIB=/s/ -Wl,--out-implib=/ -s&/' "$PKGDATA_INC" > pkgdata.inc
		PKGDATA_OPTS="-O pkgdata.inc"
	else
		PKGDATA_OPTS=
	fi

	# We use dynamic libraries for the package data (-m dll). Modern ICU
	# pkgdata emits a lib-prefixed DLL plus a real import library; keep the
	# DLL name pkgdata embedded in the import lib (lib${BASENAME}.dll) so the
	# loader finds it.
	${PKGDATA} -p ${BASENAME} -m dll -q ${PKGDATA_OPTS} packagelist.txt >/dev/null
	cp lib${BASENAME}.dll.a ../../lib${BASENAME}.a
	cp lib${BASENAME}.dll ../../lib${BASENAME}.dll
	cp ${BASENAME}_dat.o ../../${BASENAME}_dat.o

	# Clean up
	cd ..
	rm -rf work
}

# Check whether we need to do anything
if [ -f "../$1" ]; then
	for file in *; do
		if [ "$file" -nt "../$1" ]; then
			make_obj
			exit 0
		fi
	done
else
	make_obj
fi
