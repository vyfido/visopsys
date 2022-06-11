##
##  Visopsys
##  Copyright (C) 1998-2020 J. Andrew McLaughlin
##
##  Makefile
##

##  Makefile for the Visopsys utilities

ROOT = ..
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
	${CC} ${CFLAGS} -DPORTABLE $< -o $@

libinstall.o: ${LIBINSTALLDIR}/libinstall.c ${LIBINSTALLDIR}/libinstall.h \
	${STDDEPS}
	${CC} ${CFLAGS} -DPORTABLE -c $< -o $@

libinstall.a: libinstall.o
	${AR} -rs $@ libinstall.o

vspmake: ${PROGSDIR}/vspmake.c libinstall.a ${STDDEPS}
	${CC} ${CFLAGS} -DPORTABLE ${LFLAGS} $< -linstall -o $@

clean:
	rm -f *~ core *.zip *.iso *.img copy-boot libinstall.* vspmake *.log

