##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

ROOT = ../../..
BUILDDIR = build
UTILSDIR = ${ROOT}/utils
OUTPUTDIR = ${ROOT}/${BUILDDIR}
MSGDIR = ${OUTPUTDIR}/system/locale/ru

all:
	mkdir -p ${MSGDIR}
	${UTILSDIR}/makemsgs.sh . ${MSGDIR}

clean:
	rm -f *~ core
	${shell if [ -d ${MSGDIR} ] ; then for FILE in *.po ; \
		do find ${MSGDIR} -name `basename $$FILE .po`.mo -exec rm {} \; ; \
			done ; fi }

