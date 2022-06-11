##
##  Visopsys
##  Copyright (C) 1998-2015 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = ../../../build/system/config

all:
	mkdir -p ${BUILDDIR}
	cp *.conf ${BUILDDIR}
	mkdir -p ${BUILDDIR}/de
	cp de/*.conf ${BUILDDIR}/de
	mkdir -p ${BUILDDIR}/es
	cp es/*.conf ${BUILDDIR}/es

clean:
	rm -f *~ core
	rm -Rf ${BUILDDIR}

