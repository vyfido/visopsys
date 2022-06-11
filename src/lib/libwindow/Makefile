## 
##  Visopsys
##  Copyright (C) 1998-2006 J. Andrew McLaughlin
##  
##  Makefile
##

UTILSDIR=../../../utils
BUILDDIR=../../../build/system/libraries
INCDIR=../../include

ifeq (${ARCH},)
	ARCH	= $(shell ${UTILSDIR}/arch.sh)
endif

STDDEPS		= Makefile ../shared/* ${INCDIR}/*.h ${INCDIR}/sys/*.h
CODEGEN		= -O2 ${ARCH} -fno-strength-reduce -fno-strict-aliasing \
		-ffreestanding
WARN		= -Wall -W -Wshadow -Wmissing-prototypes -Wstrict-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror
INCLUDE         = -nostdinc -I${INCDIR}
CFLAGS		= -pipe ${CODEGEN} ${WARN} ${INCLUDE}
LFLAGS		= -nostdlib -nodefaultlibs -nostartfiles
OPTS		= ${CFLAGS} ${LFLAGS}

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
	${CC} ${OPTS} -c $< -o $@

${PICOBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${OPTS} -fpic -c $< -o $@

clean:
	rm -Rf *~ *.o core ${OBJDIR} ${PICOBJDIR} ${LIBRARY} ${SHAREDLIB}
