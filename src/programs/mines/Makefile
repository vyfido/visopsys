##
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
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

BUILDDIR=../../../build/programs/mines

CC		= gcc
STDDEPS		= Makefile*
INCLUDES	= -I../../include
CFLAGS		= -nostdlib -nodefaultlibs -nostartfiles -nostdinc -O2 \
		-mcpu=pentium -fno-strength-reduce -fno-strict-aliasing \
		-pipe -Wall -Wstrict-prototypes -Wmissing-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror \
		-L../../../build/system/libraries \
		-Wl,../../../build/system/libraries/crt0.o,--warn-common,-X,--oformat,elf32-i386,-Ttext,0x0

OBJS	= ${BUILDDIR}/mines

all: target-dir ${OBJS}
	cp mine.bmp ${BUILDDIR}/

target-dir:
	mkdir -p ${BUILDDIR}

${BUILDDIR}/mines: mines.c ${STDDEPS}
	${CC} ${INCLUDES} ${CFLAGS} mines.c -lwindow -lc -o $@

# Miscellany

clean:
	rm -f *~ *.o core
	rm -Rf ${BUILDDIR}
