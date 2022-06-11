## 
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
##  
##  Makefile
##

BUILDDIR=../../../build/system/libraries

CC		= gcc
AR		= ar

OPTS		= -nostdlib -nodefaultlibs -nostartfiles -nostdinc -O2 \
		-mcpu=pentium -fno-strength-reduce -fno-strict-aliasing \
		-pipe -Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror

INCLUDES	= -I../../include
STDDEP		= Makefile ../shared/*

OBJS		= obj/windowBannerDialog.o \
		obj/windowCenterDialog.o \
		obj/windowColorDialog.o \
		obj/windowFileDialog.o \
		obj/windowMain.o \
		obj/windowOkDialog.o \
		obj/windowPromptDialog.o \
		obj/windowQueryDialog.o

all: target-dirs ${OBJS} ${STDDEP}
	${AR} -rs ${BUILDDIR}/libwindow.a ${OBJS}

target-dirs:
	mkdir -p obj
	mkdir -p ${BUILDDIR}

obj/windowBannerDialog.o: windowBannerDialog.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c windowBannerDialog.c -o $@

obj/windowCenterDialog.o: windowCenterDialog.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c windowCenterDialog.c -o $@

obj/windowColorDialog.o: windowColorDialog.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c windowColorDialog.c -o $@

obj/windowFileDialog.o: windowFileDialog.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c windowFileDialog.c -o $@

obj/windowMain.o: windowMain.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c windowMain.c -o $@

obj/windowOkDialog.o: windowOkDialog.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c windowOkDialog.c -o $@

obj/windowPromptDialog.o: windowPromptDialog.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c windowPromptDialog.c -o $@

obj/windowQueryDialog.o: windowQueryDialog.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c windowQueryDialog.c -o $@

# Miscellany

clean:
	rm -Rf *~ *.o core obj ${BUILDDIR}/libwindow.a
