##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = build
OUTPUTDIR = ../../${BUILDDIR}/system
FILES = visopsys.jpg install-files.* password.blank
DESTFILES = $(addprefix ${OUTPUTDIR}/, ${FILES})

all:
	mkdir -p ${OUTPUTDIR}
	cp ${FILES} ${OUTPUTDIR}
	${MAKE} -C config BUILDDIR=${BUILDDIR}
	${MAKE} -C fonts BUILDDIR=${BUILDDIR}
	${MAKE} -C icons BUILDDIR=${BUILDDIR}
	${MAKE} -C install BUILDDIR=${BUILDDIR}
	${MAKE} -C keymaps BUILDDIR=${BUILDDIR}
	${MAKE} -C locale BUILDDIR=${BUILDDIR}
	${MAKE} -C mouse BUILDDIR=${BUILDDIR}
	${MAKE} -C wallpaper BUILDDIR=${BUILDDIR}


clean:
	${MAKE} -C config BUILDDIR=${BUILDDIR} clean
	${MAKE} -C fonts BUILDDIR=${BUILDDIR} clean
	${MAKE} -C icons BUILDDIR=${BUILDDIR} clean
	${MAKE} -C install BUILDDIR=${BUILDDIR} clean
	${MAKE} -C keymaps BUILDDIR=${BUILDDIR} clean
	${MAKE} -C locale BUILDDIR=${BUILDDIR} clean
	${MAKE} -C mouse BUILDDIR=${BUILDDIR} clean
	${MAKE} -C wallpaper BUILDDIR=${BUILDDIR} clean
	rm -f *~ core
	rm -f ${DESTFILES}

