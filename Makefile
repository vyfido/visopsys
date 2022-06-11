##
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
## 
##  Makefile
##

# This is the top-level Makefile.

BUILDDIR=build

all:
	mkdir -p ${BUILDDIR}/system
	cp COPYING.txt ${BUILDDIR}/system/
	make -C utils
	make -C dist
	make -C src

clean:
	rm -f *~ core
	make -C src clean
	make -C dist clean
	make -C utils clean
	rm -Rf ${BUILDDIR}
	find . -type f -a ! -name \*.sh -exec chmod -x {} \;
