## 
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
##  
##  Makefile
##

BUILDDIR=../../build/system/headers

all:
	mkdir -p ${BUILDDIR}
	cp *.h ${BUILDDIR}
	make -C sys

clean:
	rm -f *~
	make -C sys clean
	rm -Rf ${BUILDDIR}
