##
##  Visopsys
##  Copyright (C) 1998-2011 J. Andrew McLaughlin
## 
##  Makefile
##

BUILDDIR=../build/

all:
	mkdir -p ${BUILDDIR}
	mkdir -p ${BUILDDIR}/temp
	make -C programs
	make -C system

clean:
	rm -f *~ core
	rm -Rf ${BUILDDIR}/temp
	make -C programs clean
	make -C system clean
