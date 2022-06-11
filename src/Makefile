##
##  Visopsys
##  Copyright (C) 1998-2014 J. Andrew McLaughlin
## 
##  Makefile
##

# This is the top-level source directories Makefile.

BUILDDIR	= ../build

all:
	mkdir -p ${BUILDDIR}
	make -C osloader
	make -C include
	make -C lib
	make -C kernel
	make -C programs

clean:
	rm -Rf *~ *.o core
	make -C osloader clean
	make -C include clean
	make -C lib clean
	make -C kernel clean
	make -C programs clean
