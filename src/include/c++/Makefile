##
##  Visopsys
##  Copyright (C) 1998-2020 J. Andrew McLaughlin
##
##  Makefile
##

BUILDDIR = ../../../build/system/headers/c++

all:
	mkdir -p ${BUILDDIR}
	for FILE in * ; do \
		if [ -f "$$FILE" -a "$$FILE" != "Makefile" ] ; then \
			cp $$FILE ${BUILDDIR} ; \
		fi \
	done

clean:
	rm -f *~
	rm -Rf ${BUILDDIR}

