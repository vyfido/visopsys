##
##  Visopsys
##  Copyright (C) 1998-2014 J. Andrew McLaughlin
## 
##  Makefile
##

BUILDDIR	= ../../build/system

all:
	mkdir -p ${BUILDDIR}
	cp *.jpg install-files.* password.blank ${BUILDDIR}
	mkdir -p ${BUILDDIR}/config
	cp config/*.conf ${BUILDDIR}/config
	mkdir -p ${BUILDDIR}/icons 
	cp icons/*.bmp icons/*.ico ${BUILDDIR}/icons
	mkdir -p ${BUILDDIR}/fonts
	cp fonts/*.vbf ${BUILDDIR}/fonts
	mkdir -p ${BUILDDIR}/keymaps
	cp keymaps/*.map ${BUILDDIR}/keymaps
	mkdir -p ${BUILDDIR}/mouse
	cp mouse/*.bmp ${BUILDDIR}/mouse
	mkdir -p ${BUILDDIR}/wallpaper
	cp wallpaper/*.jpg ${BUILDDIR}/wallpaper

clean:
	make -C config clean
	rm -f *~ core
	rm -Rf ${BUILDDIR}/config ${BUILDDIR}/icons ${BUILDDIR}/fonts ${BUILDDIR}/keymaps ${BUILDDIR}/mouse ${BUILDDIR}/wallpaper ${BUILDDIR}/*.jpg ${BUILDDIR}/install-files.* ${BUILDDIR}/password.blank
