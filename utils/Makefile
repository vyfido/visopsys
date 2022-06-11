##
##  Visopsys
##  Copyright (C) 1998-2006 J. Andrew McLaughlin
## 
##  Makefile
##

##  Makefile for the Visopsys utilities.  

ifeq (${ARCH},)
	ARCH	= $(shell ./arch.sh)
endif

CFLAGS		= -O2 ${ARCH} -fno-strength-reduce -fno-strict-aliasing \
		-pipe -Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror

all: copy-boot

copy-boot: ../src/programs/copy-boot.c Makefile
	${CC} ${CFLAGS} ../src/programs/copy-boot.c -lc -o $@

clean:
	rm -f *~ core *.zip *.iso *.img copy-boot mkdosfs.*
