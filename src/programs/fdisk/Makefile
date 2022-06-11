##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

ROOT = ../../..
BUILDDIR = build
OUTPUTDIR = ${ROOT}/${BUILDDIR}/programs
INCDIR = ../../include
LIBDIR = ${ROOT}/${BUILDDIR}/system/libraries

include ${ROOT}/Makefile.include

STDDEPS = ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h ${INCDIR}/sys/*.h \
	*.h ${LIBDIR}/crt0.o
INCLUDE = -nostdinc -I${INCDIR}
CFLAGS = ${OPT} ${ARCHFLAGS} ${CCODEGEN} ${CWARN} ${INCLUDE} ${DEBUG} \
	${PARTLOGIC}
LFLAGS = -L${LIBDIR} -nostdlib \
	-Wl,${LIBDIR}/crt0.o,--warn-common,-X,--oformat,elf32-i386

ifdef DEBUG
	override DEBUG = -DDEBUG
else
	STRIP = strip -s ${FDISK}
endif

NAMES = \
	fdisk \
	gpt \
	msdos

OBJS = $(addprefix obj/, $(addsuffix .o, ${NAMES}))
FDISK = ${OUTPUTDIR}/fdisk

# Targets

all: target-dirs ${FDISK} strip
	${MAKE} -C de BUILDDIR=${BUILDDIR}

target-dirs:
	mkdir -p obj
	mkdir -p ${OUTPUTDIR}

${FDISK}: ${OBJS}
	${CC} ${CFLAGS} ${LFLAGS} ${OBJS} -lintl -lwindow -lvis -lvsh -ldl -lc -lgcc -o $@

strip: ${FDISK}
	${STRIP}

obj/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} -c $< -o $@

clean:
	${MAKE} -C de BUILDDIR=${BUILDDIR} clean
	rm -Rf *~ *.o core obj ${FDISK}

