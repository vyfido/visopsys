##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

##  Makefile for the Visopsys software store.

ROOT = ..
SRCDIR = ${ROOT}/src
INCDIR = ${SRCDIR}/include/sys
LIBDIR = .
LIBINSTALLDIR = ${SRCDIR}/lib/libinstall
PROGSDIR = ${SRCDIR}/programs
UTILSDIR = ${ROOT}/utils

include ${ROOT}/Makefile.include

ifdef DEBUG
	override DEBUG = -DDEBUG
endif

STDDEPS = ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h
INCLUDE = \
	-include ${INCDIR}/errors.h \
	-include ${INCDIR}/paths.h \
	-include ${INCDIR}/install.h \
	-include ${INCDIR}/store.h
CFLAGS = ${OPT} -pipe ${CWARN} ${INCLUDE} ${DEBUG}
LFLAGS = -L${LIBDIR}
RELEASE = $(shell ${UTILSDIR}/release.sh)

all: store

debug:
	${MAKE} all DEBUG=1

libinstall.o: ${LIBINSTALLDIR}/libinstall.c ${LIBINSTALLDIR}/libinstall.h \
	${STDDEPS}
	cc ${CFLAGS} -DPORTABLE -c $< -o $@

libinstall.a: libinstall.o
	ar -rs $@ libinstall.o

store: ${PROGSDIR}/store.c libinstall.a ${STDDEPS}
	cc ${CFLAGS} -DPORTABLE -D_VERSION_=\"${RELEASE}\" ${LFLAGS} \
		$< -linstall -lpthread -o $@

clean:
	rm -f *~ core *.o *.a store store.c

