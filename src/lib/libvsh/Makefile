## 
##  Visopsys
##  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
LFLAGS		= -nostdlib -nodefaultlibs -nostartfiles
OPTS		= ${CFLAGS} ${LFLAGS}

NAMES		= vshCompleteFilename \
		vshCopyFile \
		vshCursorMenu \
		vshDeleteFile \
		vshDumpFile \
		vshFileList \
		vshMakeAbsolutePath \
		vshParseCommand \
		vshPasswordPrompt \
		vshPrintDate \
		vshPrintTime \
		vshProgressBar \
		vshMoveFile \
		vshSearchPath

OBJDIR = obj
OBJS = $(addprefix ${OBJDIR}/, $(addsuffix .o, ${NAMES}))
PICOBJDIR = picobj
PICOBJS = $(addprefix ${PICOBJDIR}/, $(addsuffix .o, ${NAMES}))
LIBRARY = ${BUILDDIR}/libvsh.a
SONAME = libvsh.so
SHAREDLIB = ${BUILDDIR}/${SONAME}

# Targets

all: target-dirs ${LIBRARY} ${SHAREDLIB}

target-dirs:
	mkdir -p ${OBJDIR} ${PICOBJDIR} ${BUILDDIR}

${LIBRARY}: ${OBJS}
	${AR} -rs ${LIBRARY} ${OBJS}

${SHAREDLIB}: ${PICOBJS}
	${LD} -shared -soname=${SONAME} ${PICOBJS} -o $@

${OBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${OPTS} -c $< -o $@

${PICOBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${OPTS} -fpic -c $< -o $@

clean:
	rm -Rf *~ *.o core ${OBJDIR} ${PICOBJDIR} ${LIBRARY} ${SHAREDLIB}
