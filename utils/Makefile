##
##  Visopsys
##  Copyright (C) 1998-2007 J. Andrew McLaughlin
## 
##  Makefile
##

##  Makefile for the Visopsys utilities.  

ROOT=..
SRCDIR=${ROOT}/src/programs

include ${ROOT}/Makefile.include

CFLAGS		= ${OPT} -pipe ${WARN}

all: copy-boot

copy-boot: ${SRCDIR}/copy-boot.c Makefile
	${CC} ${CFLAGS} ${SRCDIR}/copy-boot.c -o $@

clean:
	rm -f *~ core *.zip *.iso *.img copy-boot *.log
