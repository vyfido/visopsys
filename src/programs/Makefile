##
##  Visopsys
##  Copyright (C) 1998-2001 J. Andrew McLaughlin
## 
##  This program is free software; you can redistribute it and/or modify it
##  under the terms of the GNU General Public License as published by the Free
##  Software Foundation; either version 2 of the License, or (at your option)
##  any later version.
## 
##  This program is distributed in the hope that it will be useful, but
##  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
##  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
##  for more details.
##  
##  You should have received a copy of the GNU General Public License along
##  with this program; if not, write to the Free Software Foundation, Inc.,
##  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
##
##  Makefile
##

##  This file is the top-level Makefile for the Visopsys Operating System's
##  built-in user applications.


AS		= nasm
CC		= gcc
PLATFORM	= $(shell ../platform.sh)
OPTS		= -nodefaultlibs -nostartfiles -nostdinc -O2 -Wall -m486 \
		-malign-functions=2 -fno-strength-reduce -malign-loops=2 \
		-malign-jumps=2 -Waggregate-return -Wstrict-prototypes \
		-Wmissing-prototypes -Wmissing-declarations \
		-Wredundant-decls -Werror

LIBDIR		= -L../lib/${PLATFORM}
LINKOPTS	= -Wl,-warn-common,-X,-oformat,binary,-Ttext,0x0
ASMOPTS		= -f elf -w+orphan-labels
INCLUDES	= -I. -I../include
RELEASE		= $(shell ../../utils/release.sh)

PROGRAMS	= ${PLATFORM}/login \
		${PLATFORM}/sish \
		${PLATFORM}/hello \
		${PLATFORM}/looper

COMMANDS	= ${PLATFORM}/cat \
		${PLATFORM}/cp \
		${PLATFORM}/date \
		${PLATFORM}/kill \
		${PLATFORM}/ls \
		${PLATFORM}/mem \
		${PLATFORM}/mkdir \
		${PLATFORM}/more \
		${PLATFORM}/mount \
		${PLATFORM}/move \
		${PLATFORM}/mv \
		${PLATFORM}/ps \
		${PLATFORM}/reboot \
		${PLATFORM}/renice \
		${PLATFORM}/rm \
		${PLATFORM}/rmdir \
		${PLATFORM}/shutdown \
		${PLATFORM}/sync \
		${PLATFORM}/touch \
		${PLATFORM}/umount \
		${PLATFORM}/uname \
		${PLATFORM}/uptime

all: target-dir ${PROGRAMS} ${COMMANDS}

target-dir:
	#
	#  Making executables on ${PLATFORM}
	#
	mkdir -p ${PLATFORM}

${PLATFORM}/login: login.c Makefile
	${CC} ${INCLUDES} ${OPTS} -DRELEASE=\"${RELEASE}\" -c login.c -o ${PLATFORM}/login.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/login.o -lc -o $@

${PLATFORM}/sish: sish.c Makefile
	${CC} ${INCLUDES} ${OPTS} -DRELEASE=\"${RELEASE}\" -c sish.c -o ${PLATFORM}/sish.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/sish.o -lc -o $@

${PLATFORM}/looper: looper.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c looper.c -o ${PLATFORM}/looper.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/looper.o -lc -o $@

${PLATFORM}/hello: hello.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c hello.c -o ${PLATFORM}/hello.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/hello.o -lc -o $@


#
# Commands
#

${PLATFORM}/cat: cat.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c cat.c -o ${PLATFORM}/cat.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/cat.o -lc -o $@

${PLATFORM}/cp: cp.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c cp.c -o ${PLATFORM}/cp.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/cp.o -lc -o $@

${PLATFORM}/date: date.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c date.c -o ${PLATFORM}/date.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/date.o -lc -o $@

${PLATFORM}/kill: kill.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c kill.c -o ${PLATFORM}/kill.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/kill.o -lc -o $@

${PLATFORM}/ls: ls.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c ls.c -o ${PLATFORM}/ls.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/ls.o -lc -o $@

${PLATFORM}/mem: mem.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c mem.c -o ${PLATFORM}/mem.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/mem.o -lc -o $@

${PLATFORM}/mkdir: mkdir.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c mkdir.c -o ${PLATFORM}/mkdir.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/mkdir.o -lc -o $@

${PLATFORM}/more: more.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c more.c -o ${PLATFORM}/more.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/more.o -lc -o $@

${PLATFORM}/mount: mount.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c mount.c -o ${PLATFORM}/mount.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/mount.o -lc -o $@

${PLATFORM}/move: move.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c move.c -o ${PLATFORM}/move.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/move.o -lc -o $@

${PLATFORM}/mv: mv.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c mv.c -o ${PLATFORM}/mv.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/mv.o -lc -o $@

${PLATFORM}/ps: ps.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c ps.c -o ${PLATFORM}/ps.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/ps.o -lc -o $@

${PLATFORM}/reboot: reboot.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c reboot.c -o ${PLATFORM}/reboot.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/reboot.o -lc -o $@

${PLATFORM}/renice: renice.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c renice.c -o ${PLATFORM}/renice.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/renice.o -lc -o $@

${PLATFORM}/rm: rm.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c rm.c -o ${PLATFORM}/rm.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/rm.o -lc -o $@

${PLATFORM}/rmdir: rmdir.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c rmdir.c -o ${PLATFORM}/rmdir.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/rmdir.o -lc -o $@

${PLATFORM}/shutdown: shutdown.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c shutdown.c -o ${PLATFORM}/shutdown.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/shutdown.o -lc -o $@

${PLATFORM}/sync: sync.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c sync.c -o ${PLATFORM}/sync.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/sync.o -lc -o $@

${PLATFORM}/touch: touch.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c touch.c -o ${PLATFORM}/touch.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/touch.o -lc -o $@

${PLATFORM}/umount: umount.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c umount.c -o ${PLATFORM}/umount.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/umount.o -lc -o $@

${PLATFORM}/uname: uname.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c uname.c -o ${PLATFORM}/uname.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/uname.o -lc -o $@

${PLATFORM}/uptime: uptime.c Makefile
	${CC} ${INCLUDES} ${OPTS} -c uptime.c -o ${PLATFORM}/uptime.o
	${CC} ${OPTS} ${LINKOPTS} ${LIBDIR} ../lib/${PLATFORM}/start.o ${PLATFORM}/uptime.o -lc -o $@


# Miscellany

clean:
	rm -Rf *~ *.o core ${PLATFORM}
