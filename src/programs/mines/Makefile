##
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
## 
##  Makefile
##

BUILDDIR=../../../build/programs/mines

CC		= gcc
CPU		= $(shell ../../utils/cpu.sh)
STDDEPS		= Makefile*
CODEGEN		= -O2 ${CPU} -fno-strength-reduce -fno-strict-aliasing
WARN		= -Wall -Wmissing-prototypes -Wstrict-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror
INCLUDE         = -nostdinc -I../../include
CFLAGS		= -pipe ${CODEGEN} ${WARN} ${INCLUDE}
LIB		= -L../../../build/system/libraries
LFLAGS		= ${LIB} -nostdlib -nodefaultlibs -nostartfiles -Wl,../../../build/system/libraries/crt0.o,--warn-common,-X,--oformat,elf32-i386,-Ttext,0x0
OPTS		= ${CFLAGS} ${LFLAGS}

NAMES	= mines

OBJS = $(addprefix ${BUILDDIR}/, ${NAMES})

all: target-dir ${OBJS}
	cp mine.bmp ${BUILDDIR}/

target-dir:
	mkdir -p ${BUILDDIR}

${BUILDDIR}/%: %.c ${STDDEPS}
	${CC} ${OPTS} $< -lwindow -lc -o $@

clean:
	rm -Rf *~ *.o core ${BUILDDIR}
