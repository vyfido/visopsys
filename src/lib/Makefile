## 
##  Visopsys
##  Copyright (C) 1998-2013 J. Andrew McLaughlin
##  
##  Makefile
##

BUILDDIR=../../build/system/libraries

ifneq (${PLUS},)
	LIBLIC = make -C liblic
	LICCLEAN = make -C liblic clean
endif

all:
	mkdir -p ${BUILDDIR}
	make -C libc
	make -C libdl
	make -C libintl
	${LIBLIC}
	make -C ../../ports/ntfsprogs -f Makefile.visopsys
	make -C libvsh
	make -C libwindow

clean:
	rm -f *~ *.o core
	make -C libc clean
	make -C libdl clean
	make -C libintl clean
	${LICCLEAN}
	make -C ../../ports/ntfsprogs -f Makefile.visopsys clean
	make -C libvsh clean
	make -C libwindow clean
	make -C shared clean
	rm -Rf ${BUILDDIR}
