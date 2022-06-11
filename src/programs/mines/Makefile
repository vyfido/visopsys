##
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
## 
##  Makefile
##

BUILDDIR=../../../build/programs/mines

CC		= gcc
STDDEPS		= Makefile*
INCLUDES	= -I../../include
CFLAGS		= -nostdlib -nodefaultlibs -nostartfiles -nostdinc -O2 \
		-mcpu=pentium -fno-strength-reduce -fno-strict-aliasing \
		-pipe -Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror \
		-L../../../build/system/libraries \
		-Wl,../../../build/system/libraries/crt0.o,--warn-common,-X,--oformat,elf32-i386,-Ttext,0x0

OBJS	= ${BUILDDIR}/mines

all: target-dir ${OBJS}
	cp mine.bmp ${BUILDDIR}/

target-dir:
	mkdir -p ${BUILDDIR}

${BUILDDIR}/mines: mines.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} mines.c -lwindow -lc -o $@

# Miscellany

clean:
	rm -f *~ *.o core
	rm -Rf ${BUILDDIR}
