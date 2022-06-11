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

# This makefile builds all the necessary parts of the boot sector and OS
# loader.

AS		= nasm
CC		= gcc
OPTS		= -f aout -w+orphan-labels
LINKOPTS	= -mcpu=pentium -Wl,-warn-common,-X,--oformat,binary,-e,loaderMain,-Ttext,0x00000000
STDDEPEND	= loader.h Makefile
OBJS		= build/loaderMain.o \
		build/loaderVideo.o \
		build/loaderDetectHardware.o \
		build/loaderLoadFile.o \
		build/loaderLoadKernel.o \
		build/loaderDiskError.o \
		build/loaderPrintRoutines.o


all: target-dir bootsectors build/vloader

bootsectors: build/bootsect.fatnoboot build/bootsect.fat12

target-dir:
	#
	#  Making OS-loader
	#
	mkdir -p build

#
# Boot sector files
#

build/bootsect.fat12: bootsect-fat12.s ${STDDEPEND}
	${AS} -f bin -w+orphan-labels bootsect-fat12.s -o $@

build/bootsect.fatnoboot: bootsect-fatnoboot.s ${STDDEPEND}
	${AS} -f bin -w+orphan-labels bootsect-fatnoboot.s -o $@

#
# Loader files
#

build/vloader: ${OBJS} ${STDDEPEND}
	${CC} -nodefaultlibs -nostartfiles -O2 -Wall ${LINKOPTS} ${OBJS} -o $@

build/loaderMain.o: loaderMain.s ${STDDEPEND}
	${AS} ${OPTS} loaderMain.s -o $@

build/loaderVideo.o: loaderVideo.s ${STDDEPEND}
	${AS} ${OPTS} loaderVideo.s -o $@

build/loaderDetectHardware.o: loaderDetectHardware.s ${STDDEPEND}
	${AS} ${OPTS} loaderDetectHardware.s -o $@

build/loaderDiskError.o: loaderDiskError.s ${STDDEPEND}
	${AS} ${OPTS} loaderDiskError.s -o $@

build/loaderLoadFile.o: loaderLoadFile.s ${STDDEPEND}
	${AS} ${OPTS} loaderLoadFile.s -o $@

build/loaderLoadKernel.o: loaderLoadKernel.s ${STDDEPEND}
	${AS} ${OPTS} loaderLoadKernel.s -o $@

build/loaderPrintRoutines.o: loaderPrintRoutines.s ${STDDEPEND}
	${AS} ${OPTS} loaderPrintRoutines.s -o $@

#
# Miscellany
#

clean:
	rm -Rf *~ *.o core build
