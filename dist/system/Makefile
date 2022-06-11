##
##  Visopsys
##  Copyright (C) 1998-2006 J. Andrew McLaughlin
## 
##  Makefile
##

BUILDDIR=../../build/system

all:
	mkdir -p ${BUILDDIR}
	cp *.bmp install-files.* password.blank ${BUILDDIR}
	mkdir -p ${BUILDDIR}/config
	cp config/*.conf ${BUILDDIR}/config
	mkdir -p ${BUILDDIR}/icons 
	cp icons/*.bmp icons/*.ico ${BUILDDIR}/icons
	mkdir -p ${BUILDDIR}/fonts
	cp fonts/*.bmp ${BUILDDIR}/fonts
	mkdir -p ${BUILDDIR}/mount
	mkdir -p ${BUILDDIR}/mouse
	cp mouse/*.bmp ${BUILDDIR}/mouse
	mkdir -p ${BUILDDIR}/wallpaper
	cp wallpaper/*.bmp ${BUILDDIR}/wallpaper

clean:
	make -C config clean
	rm -f *~ core
	rm -Rf ${BUILDDIR}/config ${BUILDDIR}/icons ${BUILDDIR}/fonts ${BUILDDIR}/mount ${BUILDDIR}/mouse ${BUILDDIR}/*.bmp ${BUILDDIR}/install-files.* ${BUILDDIR}/kernel.conf ${BUILDDIR}/password.blank ${BUILDDIR}/windowmanager.conf
