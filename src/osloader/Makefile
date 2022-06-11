##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = build

all:
	${MAKE} -C bios ARCH=${ARCH} BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}

debug:
	${MAKE} all DEBUG=1

clean:
	${MAKE} -C bios BUILDDIR=${BUILDDIR} clean
	rm -f *~ *.o core

