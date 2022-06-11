##
##  Visopsys
##  Copyright (C) 1998-2020 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = ../../build/system/libraries

all:
	mkdir -p ${BUILDDIR}
	${MAKE} -C libc DEBUG=${DEBUG}
	${MAKE} -C libcompress DEBUG=${DEBUG}
	${MAKE} -C libdl DEBUG=${DEBUG}
	${MAKE} -C libhtml DEBUG=${DEBUG}
	${MAKE} -C libhttp DEBUG=${DEBUG}
	${MAKE} -C libinstall DEBUG=${DEBUG}
	${MAKE} -C libintl DEBUG=${DEBUG}
	${MAKE} -C ../../ports/ntfsprogs -f Makefile.visopsys DEBUG=${DEBUG}
	${MAKE} -C libpthread DEBUG=${DEBUG}
	${MAKE} -C libstdc++ DEBUG=${DEBUG}
	${MAKE} -C libtelnet DEBUG=${DEBUG}
	${MAKE} -C libtimer DEBUG=${DEBUG}
	${MAKE} -C libvis DEBUG=${DEBUG}
	${MAKE} -C libvsh DEBUG=${DEBUG}
	${MAKE} -C libwindow DEBUG=${DEBUG}
	${MAKE} -C libxml DEBUG=${DEBUG}

debug:
	${MAKE} all DEBUG=1

clean:
	${MAKE} -C libc clean
	${MAKE} -C libcompress clean
	${MAKE} -C libdl clean
	${MAKE} -C libhtml clean
	${MAKE} -C libhttp clean
	${MAKE} -C libinstall clean
	${MAKE} -C libintl clean
	${MAKE} -C ../../ports/ntfsprogs -f Makefile.visopsys clean
	${MAKE} -C libpthread clean
	${MAKE} -C libstdc++ clean
	${MAKE} -C libtelnet clean
	${MAKE} -C libtimer clean
	${MAKE} -C libvis clean
	${MAKE} -C libvsh clean
	${MAKE} -C libwindow clean
	${MAKE} -C libxml clean
	rm -f *~ *.o core
	rm -Rf ${BUILDDIR}

