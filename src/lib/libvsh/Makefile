## 
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
##  
##  Makefile
##

BUILDDIR=../../../build/system/libraries

CC		= gcc
AR		= ar
CPU		= $(shell ../../../utils/cpu.sh)
STDDEPS		= Makefile ../shared/*
CODEGEN		= -O2 ${CPU} -fno-strength-reduce -fno-strict-aliasing
WARN		= -Wall -Wmissing-prototypes -Wstrict-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror
INCLUDE         = -nostdinc -I../../include
CFLAGS		= -pipe ${CODEGEN} ${WARN} ${INCLUDE}
LFLAGS		= -nostdlib -nodefaultlibs -nostartfiles
OPTS		= ${CFLAGS} ${LFLAGS}

NAMES	= vshCompleteFilename \
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
	vshRenameFile \
	vshSearchPath

OBJS = $(addprefix obj/, $(addsuffix .o, ${NAMES}))
LIBRARY = ${BUILDDIR}/libvsh.a

# Targets

all: target-dirs ${LIBRARY}

target-dirs:
	mkdir -p obj
	mkdir -p ${BUILDDIR}

${LIBRARY}: ${OBJS}
	${AR} -rs ${LIBRARY} ${OBJS}

obj/%.o: %.c ${STDDEPS}
	${CC} ${OPTS} -c $< -o $@

clean:
	rm -Rf *~ *.o core obj ${LIBRARY}
