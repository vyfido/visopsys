##
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
## 
##  Makefile
##

##  This is the Makefile for the Visopsys utilities.  

CC		= gcc
CFLAGS		= -O2 -mcpu=pentium -fno-strength-reduce -fno-strict-aliasing \
		-pipe -Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror

all: copy-boot

copy-boot: ../src/programs/copy-boot.c Makefile
	${CC} ${INCLUDES} ${CFLAGS} ../src/programs/copy-boot.c -lc -o $@

clean:
	rm -f *~ core *.zip *.iso *.img copy-boot mkdosfs.*
