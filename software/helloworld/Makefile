##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

ROOT = ../..
BUILDROOT = build
OUTPUTDIR = ${BUILDROOT}/${ARCH}
INCDIR = ${ROOT}/src/include
LIBDIR = ${ROOT}/${BUILDDIR}/system/libraries
UTILSDIR = ${ROOT}/utils

include ${ROOT}/Makefile.include

STDDEPS = ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h \
	${INCDIR}/sys/*.h ${LIBDIR}/crt0.o
INCLUDE = -nostdinc -I${INCDIR}
CFLAGS = ${OPT} ${ARCHFLAGS} ${CCODEGEN} ${CWARN} ${INCLUDE} ${DEBUG}
LFLAGS = -L${LIBDIR} -nostdlib \
	-Wl,${LIBDIR}/crt0.o,--warn-common,-X,--oformat,elf32-i386

ifdef DEBUG
	override DEBUG = -DDEBUG
else
	STRIP = strip -s ${OBJS}
endif

NAMES = helloworld

OBJS = $(addprefix ${OUTPUTDIR}/, ${NAMES})

# Targets

all: target-dirs strip package

target-dirs:
	mkdir -p ${OUTPUTDIR}

${OUTPUTDIR}/%: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} $< -lwindow -lvis -lintl -lc -o $@

strip: ${OBJS}
	${STRIP}

package: ${OBJS} helloworld-manifest.txt
	sed "s/#ARCH#/${ARCH}/" helloworld-manifest.txt > \
		helloworld-${ARCH}-manifest.txt
	${UTILSDIR}/vspmake helloworld-${ARCH}-manifest.txt

clean:
	rm -Rf *~ *.o core ${OBJS} ${BUILDROOT} helloworld-*-manifest.txt\
		installtmp-* *.vsp

