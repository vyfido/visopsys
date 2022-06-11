##
##  Visopsys
##  Copyright (C) 1998-2011 J. Andrew McLaughlin
## 
##  Makefile
##

ROOT=../../..
UTILSDIR=${ROOT}/utils
BUILDDIR=${ROOT}/build/programs
INCDIR=../../include
LIBDIR=${ROOT}/build/system/libraries
MSGDIR=${ROOT}/build/system/locale

include ${ROOT}/Makefile.include

ifneq (${BUILDLANG},)
	DEFLANG = -DBUILDLANG=\"${BUILDLANG}\"
	MAKEMSGS = ${UTILSDIR}/makemsgs.sh de ${MSGDIR}
endif

STDDEPS		= ${ROOT}/Makefile.include Makefile ${INCDIR}/*.h \
		${INCDIR}/sys/*.h *.h ${LIBDIR}/crt0.o
INCLUDE         = -nostdinc -I${INCDIR}
CFLAGS		= ${OPT} ${ARCH} ${CCODEGEN} ${CWARN} ${INCLUDE} ${PARTLOGIC} ${DEFLANG} ${PLUS}
LFLAGS		= -L${LIBDIR} -nostdlib \
		-Wl,${LIBDIR}/crt0.o,--warn-common,-X,--oformat,elf32-i386

NAMES	= fdisk \
	gpt \
	msdos

OBJS = $(addprefix obj/, $(addsuffix .o, ${NAMES}))
FDISK = ${BUILDDIR}/fdisk

# Targets

all: target-dirs ${FDISK} strip messages

target-dirs:
	mkdir -p obj
	mkdir -p ${BUILDDIR}

${FDISK}: ${OBJS}
	${CC} ${CFLAGS} ${LFLAGS} ${OBJS} -lntfs -lintl -lwindow -lvsh -lc -lgcc -o $@

strip: ${FDISK}
	strip -s ${FDISK}

messages: ${OBJS}
	${MAKEMSGS}

obj/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -Rf *~ *.o core obj ${FDISK}
	${shell if [ -d ${MSGDIR} ] ; then find ${MSGDIR} -name fdisk.mo -exec rm {} \; ; fi }
