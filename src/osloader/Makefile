##
##  Visopsys
##  Copyright (C) 1998-2005 J. Andrew McLaughlin
## 
##  Makefile
##

# This makefile builds all the MBRs, boot sectors, and the OS loader.

BOOTBUILDDIR=../../build/system/boot
LOADERBUILDDIR=../../build

AS		= nasm
CC		= gcc
WARNOPTS	= -w+orphan-labels
SECTOPTS	= -f bin ${WARNOPTS}
LDROPTS		= -f aout ${WARNOPTS}
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

bootsectors: ${BOOTBUILDDIR}/mbr.simple ${BOOTBUILDDIR}/mbr.bootmenu ${BOOTBUILDDIR}/bootmenu ${BOOTBUILDDIR}/bootsect.fat ${BOOTBUILDDIR}/bootsect.fat32 ${BOOTBUILDDIR}/bootsect.fatnoboot ${BOOTBUILDDIR}/bootsect.fatnoboot32

#
# MBRs and Boot sectors
#

${BOOTBUILDDIR}/mbr.simple: mbr-simple.s ${STDDEPEND}
	${AS} ${SECTOPTS} $< -o $@

${BOOTBUILDDIR}/mbr.bootmenu: mbr-bootmenu.s ${STDDEPEND}
	${AS} ${SECTOPTS} $< -o $@

${BOOTBUILDDIR}/bootmenu: bootmenu.s ${STDDEPEND}
	${AS} ${SECTOPTS} $< -o $@

${BOOTBUILDDIR}/bootsect.fat: bootsect-fat.s bootsect-fatBPB.s ${STDDEPEND}
	${AS} ${SECTOPTS} $< -o $@

${BOOTBUILDDIR}/bootsect.fat32: bootsect-fat.s bootsect-fatBPB.s ${STDDEPEND}
	${AS} ${SECTOPTS} -DFAT32 $< -o $@

${BOOTBUILDDIR}/bootsect.fatnoboot: bootsect-fatnoboot.s bootsect-fatBPB.s ${STDDEPEND}
	${AS} ${SECTOPTS} $< -o $@

${BOOTBUILDDIR}/bootsect.fatnoboot32: bootsect-fatnoboot.s bootsect-fatBPB.s ${STDDEPEND}
	${AS} ${SECTOPTS} -DFAT32 $< -o $@

#
# Loader files
#

${LOADERBUILDDIR}/vloader: ${OBJS} ${STDDEPEND}
	${CC} -nodefaultlibs -nostartfiles -O2 -Wall ${LINKOPTS} ${OBJS} -o $@

obj/%.o: %.s ${STDDEPS}
	${AS} ${LDROPTS} $< -o $@

#
# Miscellany
#

clean:
	rm -Rf *~ *.o core obj
	rm -Rf ${BOOTBUILDDIR} ${LOADERBUILDDIR}/vloader
