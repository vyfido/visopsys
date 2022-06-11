##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

##  Makefile for the Visopsys utilities

ROOT = ..
BUILDDIR = build
SRCDIR = ${ROOT}/src
INCDIR = ${SRCDIR}/include/sys
LIBDIR = .
LIBINSTALLDIR = ${SRCDIR}/lib/libinstall
PROGSDIR = ${SRCDIR}/programs

include ${ROOT}/Makefile.include

ifdef DEBUG
	override DEBUG = -DDEBUG
endif

STDDEPS = ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h
INCLUDE = \
	-include ${INCDIR}/env.h \
	-include ${INCDIR}/errors.h \
	-include ${INCDIR}/install.h \
	-include ${INCDIR}/paths.h
CFLAGS = ${OPT} -pipe ${CWARN} ${INCLUDE} ${DEBUG}
LFLAGS = -L${LIBDIR}

all: copy-boot vspmake

debug:
	${MAKE} all DEBUG=1

copy-boot: ${PROGSDIR}/copy-boot.c ${STDDEPS}
	cc ${CFLAGS} -DPORTABLE -DBUILDDIR=\"${ROOT}/${BUILDDIR}\" $< -o $@

libinstall.o: ${LIBINSTALLDIR}/libinstall.c ${LIBINSTALLDIR}/libinstall.h \
	${STDDEPS}
	cc ${CFLAGS} -DPORTABLE -c $< -o $@

libinstall.a: libinstall.o
	ar -rs $@ libinstall.o

vspmake: ${PROGSDIR}/vspmake.c libinstall.a ${STDDEPS}
	cc ${CFLAGS} -DPORTABLE ${LFLAGS} $< -linstall -o $@

clean:
	rm -f *~ core *.zip *.iso *.img copy-boot libinstall.* vspmake *.log

