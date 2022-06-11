##
##  Visopsys
##  Copyright (C) 1998-2014 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = ../../build/system/libraries

ifdef PLUS
	LIBLIC = ${MAKE} -C liblic
	LICCLEAN = ${LIBLIC} clean
endif

all:
	mkdir -p ${BUILDDIR}
	${MAKE} -C libc DEBUG=${DEBUG}
	${MAKE} -C libdl DEBUG=${DEBUG}
	${MAKE} -C libintl DEBUG=${DEBUG}
	${LIBLIC} DEBUG=${DEBUG}
	${MAKE} -C ../../ports/ntfsprogs -f Makefile.visopsys DEBUG=${DEBUG}
	${MAKE} -C libvsh DEBUG=${DEBUG}
	${MAKE} -C libwindow DEBUG=${DEBUG}

debug:
	${MAKE} all DEBUG=1

clean:
	${MAKE} -C libc clean
	${MAKE} -C libdl clean
	${MAKE} -C libintl clean
	${LICCLEAN}
	${MAKE} -C ../../ports/ntfsprogs -f Makefile.visopsys clean
	${MAKE} -C libvsh clean
	${MAKE} -C libwindow clean
	${MAKE} -C shared clean
	rm -f *~ *.o core
	rm -Rf ${BUILDDIR}

