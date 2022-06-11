##
##  Visopsys
##  Copyright (C) 1998-2020 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = ../../build/system/headers

all:
	mkdir -p ${BUILDDIR}
	cp *.h ${BUILDDIR}
	${MAKE} -C arpa
	${MAKE} -C c++
	${MAKE} -C netinet
	${MAKE} -C sys

clean:
	${MAKE} -C arpa clean
	${MAKE} -C c++ clean
	${MAKE} -C netinet clean
	${MAKE} -C sys clean
	rm -f *~
	rm -Rf ${BUILDDIR}

