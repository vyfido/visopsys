##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = build
OUTPUTDIR = ../../../${BUILDDIR}/system/config

all:
	mkdir -p ${OUTPUTDIR}
	cp *.conf ${OUTPUTDIR}
	mkdir -p ${OUTPUTDIR}/de
	cp de/*.conf ${OUTPUTDIR}/de
	mkdir -p ${OUTPUTDIR}/es
	cp es/*.conf ${OUTPUTDIR}/es
	mkdir -p ${OUTPUTDIR}/ru
	cp ru/*.conf ${OUTPUTDIR}/ru
	mkdir -p ${OUTPUTDIR}/tr
	cp tr/*.conf ${OUTPUTDIR}/tr

clean:
	rm -f *~ core
	rm -Rf ${OUTPUTDIR}

