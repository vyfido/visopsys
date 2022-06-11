##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

ROOT = ../../..
BUILDDIR = build
OUTPUTDIR = ${ROOT}/${BUILDDIR}/system/libraries
INCDIR = ../../include
UTILSDIR = ${ROOT}/utils

include ${ROOT}/Makefile.include

ifdef DEBUG
	override DEBUG = -DDEBUG
else
	STRIP = strip -s ${SHAREDLIB}
endif

STDDEPS = ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h ${INCDIR}/sys/*.h \
	${INCDIR}/c++/*
INCLUDE = -nostdinc -I${INCDIR} -I${INCDIR}/c++
CFLAGS = ${OPT} ${ARCHFLAGS} ${C++CODEGEN} ${C++WARN} ${INCLUDE} ${DEBUG}

NAMES = \
	_cxa \
	ios \
	iostream \
	istream \
	memory \
	new \
	ostream \
	string

OBJDIR = obj
PICOBJDIR = picobj
OBJS = $(addprefix ${OBJDIR}/, $(addsuffix .o, ${NAMES}))
PICOBJS = $(addprefix ${PICOBJDIR}/, $(addsuffix .o, ${NAMES}))
LIBRARY = ${OUTPUTDIR}/libstdc++.a
SONAME = libstdc++.so
SHAREDLIB = ${OUTPUTDIR}/${SONAME}

# Targets

all: target-dirs ${LIBRARY} ${SHAREDLIB} strip

target-dirs:
	mkdir -p ${OBJDIR} ${PICOBJDIR} ${OUTPUTDIR}

${LIBRARY}: ${OBJS}
	${AR} -rs $@ ${OBJS}

${SHAREDLIB}: ${PICOBJS}
	${CC++} ${ARCHFLAGS} -nostdlib -Wl,-shared -Wl,--soname=${SONAME} \
		-Wl,-z,norelro ${PICOBJS} -static -lgcc -o $@

strip: ${SHAREDLIB}
	${STRIP}

${OBJDIR}/%.o: %.cpp ${STDDEPS}
	${CC++} ${CFLAGS} -c $< -o $@

${PICOBJDIR}/%.o: %.cpp ${STDDEPS}
	${CC++} ${CFLAGS} -fpic -c $< -o $@

clean:
	rm -Rf *~ *.o core ${OBJDIR} ${PICOBJDIR} ${LIBRARY} ${SHAREDLIB}

