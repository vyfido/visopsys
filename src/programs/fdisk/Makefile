##
##  Visopsys
##  Copyright (C) 1998-2007 J. Andrew McLaughlin
## 
##  Makefile
##

ROOT=../../..
UTILSDIR=${ROOT}/utils
BUILDDIR=${ROOT}/build/programs
INCDIR=../../include
LIBDIR=${ROOT}/build/system/libraries

include ${ROOT}/Makefile.include

STDDEPS		= ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h \
		${INCDIR}/sys/*.h *.h ${LIBDIR}/crt0.o
INCLUDE         = -nostdinc -I${INCDIR}
CFLAGS		= ${OPT} ${ARCH} ${CODEGEN} ${WARN} ${INCLUDE} ${PARTLOGIC}
LFLAGS		= -L${LIBDIR} -nostdlib \
		-Wl,${LIBDIR}/crt0.o,--warn-common,-X,--oformat,elf32-i386

NAMES	= fdisk \
	msdos

OBJS = $(addprefix obj/, $(addsuffix .o, ${NAMES}))
FDISK = ${BUILDDIR}/fdisk

# Targets

all: target-dirs ${FDISK} strip

target-dirs:
	mkdir -p obj
	mkdir -p ${BUILDDIR}

${FDISK}: ${OBJS}
	${CC} ${CFLAGS} ${LFLAGS} ${OBJS} -lntfs -lwindow -lvsh -lc -o $@

strip: ${FDISK}
	strip -s ${FDISK}

obj/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -Rf *~ *.o core obj ${FDISK}
