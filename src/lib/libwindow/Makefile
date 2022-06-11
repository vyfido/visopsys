##
##  Visopsys
##  Copyright (C) 1998-2015 J. Andrew McLaughlin
##
##  Makefile
##

ROOT = ../../..
BUILDDIR = ${ROOT}/build/system/libraries
INCDIR = ../../include

include ${ROOT}/Makefile.include

STDDEPS = ${ROOT}/Makefile.include Makefile ../shared/* ${INCDIR}/*.h \
	${INCDIR}/sys/*.h
INCLUDE = -nostdinc -I${INCDIR}
CFLAGS = ${OPT} ${ARCH} ${CCODEGEN} ${CWARN} ${INCLUDE} ${DEBUG}
LFLAGS = -nostdlib

ifdef DEBUG
	override DEBUG = -DDEBUG
else
	STRIP = strip -s ${SHAREDLIB}
endif

NAMES = windowBannerDialog \
	windowCenterDialog \
	windowChoiceDialog \
	windowColorDialog \
	windowFileDialog \
	windowFileList \
	windowLanguageDialog \
	windowMain \
	windowNumberDialog \
	windowOkDialog \
	windowProgressDialog \
	windowPromptDialog \
	windowQueryDialog \
	windowRadioDialog \
	windowThumbImage

OBJDIR = obj
OBJS = $(addprefix ${OBJDIR}/, $(addsuffix .o, ${NAMES}))
PICOBJDIR = picobj
PICOBJS = $(addprefix ${PICOBJDIR}/, $(addsuffix .o, ${NAMES}))
LIBRARY = ${BUILDDIR}/libwindow.a
SONAME = libwindow.so
SHAREDLIB = ${BUILDDIR}/${SONAME}

# Targets

all: target-dirs ${LIBRARY} ${SHAREDLIB} strip
	${MAKE} -C de
	${MAKE} -C es

target-dirs:
	mkdir -p ${OBJDIR} ${PICOBJDIR} ${BUILDDIR}

${LIBRARY}: ${OBJS}
	${AR} -rs $@ ${OBJS}

${SHAREDLIB}: ${PICOBJS}
	${LD} -shared -nostdlib -z norelro -soname=${SONAME} ${PICOBJS} -o $@

strip: ${SHAREDLIB}
	${STRIP}

${OBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} -c $< -o $@

${PICOBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} -fpic -c $< -o $@

clean:
	${MAKE} -C de clean
	${MAKE} -C es clean
	rm -Rf *~ *.o core ${OBJDIR} ${PICOBJDIR} ${LIBRARY} ${SHAREDLIB}

