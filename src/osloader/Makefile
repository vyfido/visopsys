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

BOOTBUILDDIR=../../build/system/boot
LOADERBUILDDIR=../../build

AS		= nasm
CC		= gcc
OPTS		= -f aout -w+orphan-labels
LINKOPTS	= -mcpu=pentium -Wl,-warn-common,-X,--oformat,binary,-e,loaderMain,-Ttext,0x00000000
STDDEPEND	= loader.h Makefile
OBJS		= obj/loaderMain.o \
		obj/loaderVideo.o \
		obj/loaderDetectHardware.o \
		obj/loaderLoadFile.o \
		obj/loaderLoadKernel.o \
		obj/loaderDiskError.o \
		obj/loaderPrintRoutines.o

all: target-dirs bootsectors ${LOADERBUILDDIR}/vloader

target-dirs:
	mkdir -p obj ${BOOTBUILDDIR} ${LOADERBUILDDIR}

bootsectors: ${BOOTBUILDDIR}/bootsect.fatnoboot ${BOOTBUILDDIR}/bootsect.fat12

#
# Boot sector files
#

${BOOTBUILDDIR}/bootsect.fatnoboot: bootsect-fatnoboot.s ${STDDEPEND}
	${AS} -f bin -w+orphan-labels bootsect-fatnoboot.s -o $@

${BOOTBUILDDIR}/bootsect.fat12: bootsect-fat12.s ${STDDEPEND}
	${AS} -f bin -w+orphan-labels bootsect-fat12.s -o $@

#
# Loader files
#

${LOADERBUILDDIR}/vloader: ${OBJS} ${STDDEPEND}
	${CC} -nodefaultlibs -nostartfiles -O2 -Wall ${LINKOPTS} ${OBJS} -o $@

obj/loaderMain.o: loaderMain.s ${STDDEPEND}
	${AS} ${OPTS} loaderMain.s -o $@

obj/loaderVideo.o: loaderVideo.s ${STDDEPEND}
	${AS} ${OPTS} loaderVideo.s -o $@

obj/loaderDetectHardware.o: loaderDetectHardware.s ${STDDEPEND}
	${AS} ${OPTS} loaderDetectHardware.s -o $@

obj/loaderDiskError.o: loaderDiskError.s ${STDDEPEND}
	${AS} ${OPTS} loaderDiskError.s -o $@

obj/loaderLoadFile.o: loaderLoadFile.s ${STDDEPEND}
	${AS} ${OPTS} loaderLoadFile.s -o $@

obj/loaderLoadKernel.o: loaderLoadKernel.s ${STDDEPEND}
	${AS} ${OPTS} loaderLoadKernel.s -o $@

obj/loaderPrintRoutines.o: loaderPrintRoutines.s ${STDDEPEND}
	${AS} ${OPTS} loaderPrintRoutines.s -o $@

#
# Miscellany
#

clean:
	rm -Rf *~ *.o core obj
	rm -Rf ${BOOTBUILDDIR} ${LOADERBUILDDIR}/vloader
