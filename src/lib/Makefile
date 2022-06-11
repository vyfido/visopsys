##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = build
OUTPUTDIR = ../../${BUILDDIR}/system/libraries

all:
	mkdir -p ${OUTPUTDIR}
	${MAKE} -C libc BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libcompress BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libdl BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libhtml BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libhttp BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libinstall BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libintl BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C ../../ports/ntfsprogs -f Makefile.visopsys \
		BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libpthread BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libstdc++ BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libtelnet BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libtimer BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libvis BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libvsh BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libwindow BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}
	${MAKE} -C libxml BUILDDIR=${BUILDDIR} DEBUG=${DEBUG}

debug:
	${MAKE} all DEBUG=1

clean:
	${MAKE} -C libc BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libcompress BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libdl BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libhtml BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libhttp BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libinstall BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libintl BUILDDIR=${BUILDDIR} clean
	${MAKE} -C ../../ports/ntfsprogs -f Makefile.visopsys \
		BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libpthread BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libstdc++ BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libtelnet BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libtimer BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libvis BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libvsh BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libwindow BUILDDIR=${BUILDDIR} clean
	${MAKE} -C libxml BUILDDIR=${BUILDDIR} clean
	rm -f *~ *.o core
	rm -Rf ${OUTPUTDIR}

