##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = build
OUTPUTDIR = ../${BUILDDIR}

all:
	mkdir -p ${OUTPUTDIR}/temp
	mkdir -p ${OUTPUTDIR}/users
	${MAKE} -C programs BUILDDIR=${BUILDDIR}
	${MAKE} -C system BUILDDIR=${BUILDDIR}

clean:
	${MAKE} -C programs BUILDDIR=${BUILDDIR} clean 
	${MAKE} -C system BUILDDIR=${BUILDDIR} clean
	rm -f *~ core
	rm -Rf ${OUTPUTDIR}/temp
	rm -Rf ${OUTPUTDIR}/users

