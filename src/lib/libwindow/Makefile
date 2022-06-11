## 
##  Visopsys
##  Copyright (C) 1998-2007 J. Andrew McLaughlin
##  
##  Makefile
##

ROOT=../../..
UTILSDIR=${ROOT}/utils
BUILDDIR=${ROOT}/build/system/libraries
INCDIR=../../include

include ${ROOT}/Makefile.include

STDDEPS		= ${ROOT}/Makefile.include Makefile ../shared/* ${INCDIR}/*.h \
		${INCDIR}/sys/*.h
INCLUDE         = -nostdinc -I${INCDIR}
CFLAGS		= ${OPT} ${ARCH} ${CODEGEN} ${WARN} ${INCLUDE}
LFLAGS		= -nostdlib

NAMES		= windowBannerDialog \
		windowCenterDialog \
		windowChoiceDialog \
		windowColorDialog \
		windowFileDialog \
		windowFileList \
		windowMain \
		windowOkDialog \
		windowProgressDialog \
		windowPromptDialog \
		windowRadioDialog \
		windowQueryDialog

OBJDIR = obj
OBJS = $(addprefix ${OBJDIR}/, $(addsuffix .o, ${NAMES}))
PICOBJDIR = picobj
PICOBJS = $(addprefix ${PICOBJDIR}/, $(addsuffix .o, ${NAMES}))
LIBRARY = ${BUILDDIR}/libwindow.a
SONAME = libwindow.so
SHAREDLIB = ${BUILDDIR}/${SONAME}

# Targets

all: target-dirs ${LIBRARY} ${SHAREDLIB}

target-dirs:
	mkdir -p ${OBJDIR} ${PICOBJDIR} ${BUILDDIR}

${LIBRARY}: ${OBJS}
	${AR} -rs $@ ${OBJS}

${SHAREDLIB}: ${PICOBJS}
	${LD} -shared -soname=${SONAME} ${PICOBJS} -o $@

${OBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} -c $< -o $@

${PICOBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} -fpic -c $< -o $@

clean:
	rm -Rf *~ *.o core ${OBJDIR} ${PICOBJDIR} ${LIBRARY} ${SHAREDLIB}
