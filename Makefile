##
##  Visopsys
##  Copyright (C) 1998-2011 J. Andrew McLaughlin
## 
##  Makefile
##

# Top-level Makefile.

BUILDDIR	= build

all:
	mkdir -p ${BUILDDIR}/system
	cp COPYING.txt ${BUILDDIR}/system/
	mkdir -p ${BUILDDIR}/system/locale
	make -C dist
	make -C utils
	make -C src

clean:
	rm -f *~ core
	make -C dist clean
	make -C utils clean
	make -C src clean
	rm -Rf ${BUILDDIR}
	find . -type f -a ! -name \*.sh -exec chmod -x {} \;
