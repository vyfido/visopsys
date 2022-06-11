##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

ROOT = ../../..
BUILDDIR = build
OUTPUTDIR = ${ROOT}/${BUILDDIR}/system/locale/es
UTILSDIR = ${ROOT}/utils

all:
	mkdir -p ${OUTPUTDIR}
	${UTILSDIR}/makemsgs.sh . ${OUTPUTDIR}

clean:
	rm -f *~ core
	${shell if [ -d ${OUTPUTDIR} ] ; then for FILE in *.po ; \
		do find ${OUTPUTDIR} -name `basename $$FILE .po`.mo -exec rm {} \; ; \
			done ; fi }

