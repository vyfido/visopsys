## 
##  Visopsys
##  Copyright (C) 1998-2011 J. Andrew McLaughlin
##  
##  Makefile
##

ROOT=../../..
UTILSDIR=${ROOT}/utils
BUILDDIR=${ROOT}/build/system/libraries
INCDIR=../../include

include ${ROOT}/Makefile.include

ifneq (${BUILDLANG},)
	DEFLANG = -DBUILDLANG=\"${BUILDLANG}\"
	MAKELANG = make -C ${BUILDLANG}
	MAKELANGCLEAN = make -C ${BUILDLANG} clean
endif

STDDEPS		= ${ROOT}/Makefile.include Makefile ../shared/* ${INCDIR}/*.h \
		${INCDIR}/sys/*.h
INCLUDE         = -nostdinc -I${INCDIR}
CFLAGS		= ${OPT} ${ARCH} ${CCODEGEN} ${CWARN} ${INCLUDE} ${DEFLANG}
LFLAGS		= -nostdlib

ifeq (${DEBUG},)
	STRIP = strip -s ${SHAREDLIB}
endif 

NAMES		= windowBannerDialog \
		windowCenterDialog \
		windowChoiceDialog \
		windowColorDialog \
		windowFileDialog \
		windowFileList \
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
	${MAKELANG}

target-dirs:
	mkdir -p ${OBJDIR} ${PICOBJDIR} ${BUILDDIR}

${LIBRARY}: ${OBJS}
	${AR} -rs $@ ${OBJS}

${SHAREDLIB}: ${PICOBJS}
	${LD} -shared -soname=${SONAME} ${PICOBJS} -o $@

strip: ${SHAREDLIB}
	${STRIP}

${OBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} -c $< -o $@

${PICOBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} ${LFLAGS} -fpic -c $< -o $@

clean:
	rm -Rf *~ *.o core ${OBJDIR} ${PICOBJDIR} ${LIBRARY} ${SHAREDLIB}
	${MAKELANGCLEAN}
