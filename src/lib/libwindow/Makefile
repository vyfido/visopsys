## 
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
##  
##  This library is free software; you can redistribute it and/or modify it
##  under the terms of the GNU Lesser General Public License as published by
##  the Free Software Foundation; either version 2.1 of the License, or (at
##  your option) any later version.
##
##  This library is distributed in the hope that it will be useful, but
##  WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
##  General Public License for more details.
##
##  You should have received a copy of the GNU Lesser General Public License
##  along with this library; if not, write to the Free Software Foundation,
##  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

OBJS		= obj/windowCenterDialog.o \
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
