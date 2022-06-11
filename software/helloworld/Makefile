##
##  Visopsys
##  Copyright (C) 1998-2020 J. Andrew McLaughlin
##
##  Makefile
##

ROOT = ../..
BUILDDIR = build
INCDIR = ${ROOT}/src/include
LIBDIR = ${ROOT}/build/system/libraries
UTILSDIR = ${ROOT}/utils

include ${ROOT}/Makefile.include

STDDEPS = ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h \
	${INCDIR}/sys/*.h ${LIBDIR}/crt0.o
INCLUDE = -nostdinc -I${INCDIR}
CFLAGS = ${OPT} ${ARCH} ${CCODEGEN} ${CWARN} ${INCLUDE} ${DEBUG}
LFLAGS = -L${LIBDIR} -nostdlib \
	-Wl,${LIBDIR}/crt0.o,--warn-common,-X,--oformat,elf32-i386

ifdef DEBUG
	override DEBUG = -DDEBUG
else
	STRIP = strip -s ${OBJS}
endif

NAMES = helloworld

OBJS = $(addprefix ${BUILDDIR}/, ${NAMES})

# Targets

all: target-dirs strip package

target-dirs:
	mkdir -p ${BUILDDIR}

${BUILDDIR}/%: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lwindow -lvis -lintl -lc -o $@

strip: ${OBJS}
	${STRIP}

package: ${OBJS} helloworld-manifest.txt
	${UTILSDIR}/vspmake helloworld-manifest.txt

clean:
	rm -Rf *~ *.o core ${OBJS} ${BUILDDIR} installtmp-* *.vsp

