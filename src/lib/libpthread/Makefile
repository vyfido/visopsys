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

include ${ROOT}/Makefile.include

STDDEPS = ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h ${INCDIR}/sys/*.h
INCLUDE = -nostdinc -I${INCDIR}
CFLAGS = ${OPT} ${ARCHFLAGS} ${CCODEGEN} ${CWARN} ${INCLUDE} ${DEBUG}
LFLAGS = -nostdlib

ifdef DEBUG
	override DEBUG = -DDEBUG
else
	STRIP = strip -s ${SHAREDLIB}
endif

NAMES = \
	pthread_attr_destroy \
	pthread_attr_init \
	pthread_cancel \
	pthread_create \
	pthread_exit \
	pthread_join \
	pthread_self

OBJDIR = obj
OBJS = $(addprefix ${OBJDIR}/, $(addsuffix .o, ${NAMES}))
PICOBJDIR = picobj
PICOBJS = $(addprefix ${PICOBJDIR}/, $(addsuffix .o, ${NAMES}))
LIBRARY = ${OUTPUTDIR}/libpthread.a
SONAME = libpthread.so
SHAREDLIB = ${OUTPUTDIR}/${SONAME}

# Targets

all: target-dirs ${LIBRARY} ${SHAREDLIB} strip

target-dirs:
	mkdir -p ${OBJDIR} ${PICOBJDIR} ${OUTPUTDIR}

${LIBRARY}: ${OBJS}
	${AR} -rs $@ ${OBJS}

${SHAREDLIB}: ${PICOBJS}
	${CC} ${ARCHFLAGS} -nostdlib -Wl,-shared -Wl,--soname=${SONAME} \
		-Wl,-z,norelro ${PICOBJS} -o $@

strip: ${SHAREDLIB}
	${STRIP}

${OBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} -c $< -o $@

${PICOBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} -fpic -c $< -o $@

clean:
	rm -Rf *~ *.o core ${OBJDIR} ${PICOBJDIR} ${LIBRARY} ${SHAREDLIB}

