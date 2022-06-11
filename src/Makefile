##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

# This is the top-level source directories Makefile.

BUILDDIR = build

all:
	${MAKE} -C osloader ARCH=${ARCH} BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C include BUILDDIR=${BUILDDIR}
	${MAKE} -C lib ARCH=${ARCH} BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C kernel ARCH=${ARCH} BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C programs ARCH=${ARCH} BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}

debug:
	${MAKE} all DEBUG=1

clean:
	${MAKE} -C osloader BUILDDIR=${BUILDDIR} clean
	${MAKE} -C include BUILDDIR=${BUILDDIR} clean
	${MAKE} -C lib BUILDDIR=${BUILDDIR} clean
	${MAKE} -C kernel BUILDDIR=${BUILDDIR} clean
	${MAKE} -C programs BUILDDIR=${BUILDDIR} clean
	rm -Rf *~ core

