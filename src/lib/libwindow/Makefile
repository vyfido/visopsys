## 
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
##  
##  Makefile
##

BUILDDIR=../../../build/system/libraries

CC		= gcc
AR		= ar
LD		= ld
CPU		= $(shell ../../../utils/cpu.sh)
STDDEPS		= Makefile ../shared/*
CODEGEN		= -O2 ${CPU} -fno-strength-reduce -fno-strict-aliasing \
		-ffreestanding
WARN		= -Wall -W -Wshadow -Wmissing-prototypes -Wstrict-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror
INCLUDE         = -nostdinc -I../../include
CFLAGS		= -pipe ${CODEGEN} ${WARN} ${INCLUDE}
LFLAGS		= -nostdlib -nodefaultlibs -nostartfiles
OPTS		= ${CFLAGS} ${LFLAGS}

NAMES		= windowBannerDialog \
		windowCenterDialog \
		windowChoiceDialog \
		windowColorDialog \
		windowFileDialog \
		windowMain \
		windowOkDialog \
		windowPromptDialog \
		windowQueryDialog

OBJDIR = obj
OBJS = $(addprefix ${OBJDIR}/, $(addsuffix .o, ${NAMES}))
LIBRARY = ${BUILDDIR}/libwindow.a

# Targets

all: target-dirs ${LIBRARY}

target-dirs:
	mkdir -p ${OBJDIR} ${BUILDDIR}

${LIBRARY}: ${OBJS}
	${AR} -rs $@ ${OBJS}

${OBJDIR}/%.o: %.c ${STDDEPS}
	${CC} ${OPTS} -c $< -o $@

clean:
	rm -Rf *~ *.o core ${OBJDIR} ${LIBRARY}
