##
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
## 
##  Makefile
##

BUILDDIR=../../build/system

all:
	mkdir -p ${BUILDDIR}
	cp *.bmp install-files.* kernel.conf password.blank windowmanager.conf ${BUILDDIR}
	mkdir -p ${BUILDDIR}/icons 
	cp icons/*.bmp ${BUILDDIR}/icons
	mkdir -p ${BUILDDIR}/fonts
	cp fonts/*.bmp ${BUILDDIR}/fonts

clean:
	rm -f *~ core
	rm -Rf ${BUILDDIR}/icons ${BUILDDIR}/fonts ${BUILDDIR}/*.bmp ${BUILDDIR}/install-files.* ${BUILDDIR}/kernel.conf ${BUILDDIR}/password.blank ${BUILDDIR}/windowmanager.conf
