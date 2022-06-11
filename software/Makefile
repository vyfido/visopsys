##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

# This is the top-level software directory Makefile

BUILDDIR = build

all:
	${MAKE} -C helloworld ARCH=${ARCH} BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}

debug:
	${MAKE} all DEBUG=1

clean:
	${MAKE} -C helloworld clean
	rm -Rf *~ core

