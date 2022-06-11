##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = build
OUTPUTDIR = ../../${BUILDDIR}/system/headers

all:
	mkdir -p ${OUTPUTDIR}
	cp *.h ${OUTPUTDIR}
	${MAKE} -C arch/x86 BUILDDIR=${BUILDDIR}
	${MAKE} -C arpa BUILDDIR=${BUILDDIR}
	${MAKE} -C c++ BUILDDIR=${BUILDDIR}
	${MAKE} -C netinet BUILDDIR=${BUILDDIR}
	${MAKE} -C sys BUILDDIR=${BUILDDIR}

clean:
	${MAKE} -C arch/x86 BUILDDIR=${BUILDDIR} clean
	${MAKE} -C arpa BUILDDIR=${BUILDDIR} clean
	${MAKE} -C c++ BUILDDIR=${BUILDDIR} clean
	${MAKE} -C netinet BUILDDIR=${BUILDDIR} clean
	${MAKE} -C sys BUILDDIR=${BUILDDIR} clean
	rm -f *~
	rm -Rf ${OUTPUTDIR}

