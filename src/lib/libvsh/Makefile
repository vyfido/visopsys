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

OBJS	= obj/vshCompleteFilename.o \
	obj/vshCopyFile.o \
	obj/vshDeleteFile.o \
	obj/vshDumpFile.o \
	obj/vshFileList.o \
	obj/vshMakeAbsolutePath.o \
	obj/vshPasswordPrompt.o \
	obj/vshPrintDate.o \
	obj/vshPrintTime.o \
	obj/vshRenameFile.o \
	obj/vshSearchPath.o

all: target-dirs ${OBJS} ${STDDEP}
	${AR} -rs ${BUILDDIR}/libvsh.a ${OBJS}

target-dirs:
	mkdir -p obj
	mkdir -p ${BUILDDIR}

# The Visopsys library files

obj/vshCompleteFilename.o: vshCompleteFilename.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshCompleteFilename.c -o $@

obj/vshCopyFile.o: vshCopyFile.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshCopyFile.c -o $@

obj/vshDeleteFile.o: vshDeleteFile.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshDeleteFile.c -o $@

obj/vshDumpFile.o: vshDumpFile.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshDumpFile.c -o $@

obj/vshFileList.o: vshFileList.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshFileList.c -o $@

obj/vshMakeAbsolutePath.o: vshMakeAbsolutePath.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshMakeAbsolutePath.c -o $@

obj/vshPasswordPrompt.o: vshPasswordPrompt.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshPasswordPrompt.c -o $@

obj/vshPrintDate.o: vshPrintDate.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshPrintDate.c -o $@

obj/vshPrintTime.o: vshPrintTime.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshPrintTime.c -o $@

obj/vshRenameFile.o: vshRenameFile.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshRenameFile.c -o $@

obj/vshSearchPath.o: vshSearchPath.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshSearchPath.c -o $@

# Miscellany

clean:
	rm -Rf *~ *.o core obj ${BUILDDIR}/libvsh.a
