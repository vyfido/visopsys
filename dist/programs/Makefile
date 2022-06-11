##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = build
OUTPUTDIR = ../../${BUILDDIR}/programs

all:
	${MAKE} -C fontutil.dir BUILDDIR=${BUILDDIR}
	${MAKE} -C helpfiles BUILDDIR=${BUILDDIR}
	${MAKE} -C mines.dir BUILDDIR=${BUILDDIR}
	${MAKE} -C snake.dir BUILDDIR=${BUILDDIR}

clean:
	${MAKE} -C fontutil.dir BUILDDIR=${BUILDDIR} clean
	${MAKE} -C helpfiles BUILDDIR=${BUILDDIR} clean
	${MAKE} -C mines.dir BUILDDIR=${BUILDDIR} clean
	${MAKE} -C snake.dir BUILDDIR=${BUILDDIR} clean
	rm -f *~ core

