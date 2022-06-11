##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

# Top-level Makefile

ARCH = x86
BUILDROOT = build
BUILDDIR = ${BUILDROOT}/${ARCH}
OUTPUTDIR = ${BUILDDIR}

all:
	mkdir -p ${OUTPUTDIR}/system
	cp COPYING.txt ${OUTPUTDIR}/system/
	${MAKE} -C dist BUILDDIR=${BUILDDIR}
	${MAKE} -C src ARCH=${ARCH} BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C store DEBUG=${DEBUG}
	${MAKE} -C utils BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	# Needs utils
	${MAKE} -C software ARCH=${ARCH} BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}

debug:
	${MAKE} all DEBUG=1

clean:
	${MAKE} -C dist BUILDDIR=${BUILDDIR} clean
	${MAKE} -C src BUILDDIR=${BUILDDIR} clean
	${MAKE} -C store clean
	${MAKE} -C utils clean
	${MAKE} -C software clean
	rm -f *~ core
	rm -Rf ${BUILDROOT}
	find -name '*.rej' -exec rm {} \;
	find -name '*.orig' -exec rm {} \;
	-find contrib dist docs patches software src store utils work \
		-type f -a ! -name '*.sh' -exec chmod -x {} \; 2> /dev/null

