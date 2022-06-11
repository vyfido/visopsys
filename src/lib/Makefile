## 
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
##  
##  Makefile
##

BUILDDIR=../../build/system/libraries

all:
	mkdir -p ${BUILDDIR}
	make -C libc
	make -C libvsh
	make -C libwindow

clean:
	rm -f *~ *.o core
	make -C libc clean
	make -C libvsh clean
	make -C libwindow clean
	make -C shared clean
	rm -Rf ${BUILDDIR}
