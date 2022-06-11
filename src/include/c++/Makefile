##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = build
OUTPUTDIR = ../../../${BUILDDIR}/system/headers/c++

all:
	mkdir -p ${OUTPUTDIR}
	for FILE in * ; do \
		if [ -f "$$FILE" -a "$$FILE" != "Makefile" ] ; then \
			cp $$FILE ${OUTPUTDIR} ; \
		fi \
	done

clean:
	rm -f *~
	rm -Rf ${OUTPUTDIR}

