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

# This makefile builds all the necessary parts of the boot sector and OS
# loader.

AS		= nasm
CC		= gcc
PLATFORM	= $(shell ../platform.sh)
OPTS		= -f aout -w+orphan-labels
LINKOPTS	= -m486 -Wl,-warn-common,-X,-oformat,binary,-e,loaderMain,-Ttext,0x00000000
STDDEPEND	= loader.h ../kernel/kernelAssemblerHeader.h Makefile
OBJS		= ${PLATFORM}/loaderMain.o \
		${PLATFORM}/loaderVideo.o \
		${PLATFORM}/loaderSplash.o \
		${PLATFORM}/loaderDetectHardware.o \
		${PLATFORM}/loaderLoadFile.o \
		${PLATFORM}/loaderLoadKernel.o \
		${PLATFORM}/loaderDiskError.o \
		${PLATFORM}/loaderPrintRoutines.o


all: target-dir ${PLATFORM}/bootsect.f12 ${PLATFORM}/vloader

target-dir:
	#
	#  Making OS-loader on ${PLATFORM}
	#
	mkdir -p ${PLATFORM}

#
# Assembly files
#

${PLATFORM}/bootsect.f12: bootsect.s ${STDDEPEND}
	${AS} -f bin -w+orphan-labels bootsect.s -o $@

${PLATFORM}/vloader: ${OBJS} ${STDDEPEND}
	${CC} -nodefaultlibs -nostartfiles -O2 -Wall ${LINKOPTS} ${OBJS} -o $@

${PLATFORM}/loaderMain.o: loaderMain.s ${STDDEPEND}
	${AS} ${OPTS} loaderMain.s -o $@

${PLATFORM}/loaderVideo.o: loaderVideo.s ${STDDEPEND}
	${AS} ${OPTS} loaderVideo.s -o $@

${PLATFORM}/loaderSplash.o: loaderSplash.s ${STDDEPEND}
	${AS} ${OPTS} loaderSplash.s -o $@

${PLATFORM}/loaderDetectHardware.o: loaderDetectHardware.s ${STDDEPEND}
	${AS} ${OPTS} loaderDetectHardware.s -o $@

${PLATFORM}/loaderDiskError.o: loaderDiskError.s ${STDDEPEND}
	${AS} ${OPTS} loaderDiskError.s -o $@

${PLATFORM}/loaderLoadFile.o: loaderLoadFile.s ${STDDEPEND}
	${AS} ${OPTS} loaderLoadFile.s -o $@

${PLATFORM}/loaderLoadKernel.o: loaderLoadKernel.s ${STDDEPEND}
	${AS} ${OPTS} loaderLoadKernel.s -o $@

${PLATFORM}/loaderPrintRoutines.o: loaderPrintRoutines.s ${STDDEPEND}
	${AS} ${OPTS} loaderPrintRoutines.s -o $@

#
# Miscellany
#

clean:
	rm -Rf *~ *.o core ${PLATFORM}
