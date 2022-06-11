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

OBJS	= obj/vshCompleteFilename.o \
	obj/vshCopyFile.o \
	obj/vshCursorMenu.o \
	obj/vshDeleteFile.o \
	obj/vshDumpFile.o \
	obj/vshFileList.o \
	obj/vshMakeAbsolutePath.o \
	obj/vshParseCommand.o \
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

obj/vshCursorMenu.o: vshCursorMenu.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshCursorMenu.c -o $@

obj/vshDeleteFile.o: vshDeleteFile.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshDeleteFile.c -o $@

obj/vshDumpFile.o: vshDumpFile.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshDumpFile.c -o $@

obj/vshFileList.o: vshFileList.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshFileList.c -o $@

obj/vshMakeAbsolutePath.o: vshMakeAbsolutePath.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshMakeAbsolutePath.c -o $@

obj/vshParseCommand.o: vshParseCommand.c ${STDDEP}
	${CC} ${OPTS} ${INCLUDES} -c vshParseCommand.c -o $@

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
