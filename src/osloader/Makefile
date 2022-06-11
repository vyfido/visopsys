##
##  Visopsys
##  Copyright (C) 1998-2007 J. Andrew McLaughlin
## 
##  Makefile
##

# This makefile builds all the MBRs, boot sectors, and the OS loader.

ROOT=../..
BOOTBUILDDIR=${ROOT}/build/system/boot
LOADERBUILDDIR=${ROOT}/build

include ${ROOT}/Makefile.include

WARN		= -w+orphan-labels
SECTOPTS	= -f bin ${WARN}
LDROPTS		= -f elf ${WARN}
LINKOPTS	= -Wl,-warn-common,-X,--oformat,binary,-e,loaderMain,-Ttext,0x00000000
STDDEPS		= loader.h Makefile
BOOTDEPS	= bootsect-fatBPB.s bootsect-diskparms.s bootsect-print.s \
		bootsect-error.s bootsect-read.s 
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

${BOOTBUILDDIR}/mbr.simple: mbr-simple.s ${BOOTDEPS} ${STDDEPS}
	${NASM} ${SECTOPTS} $< -o $@

${BOOTBUILDDIR}/mbr.bootmenu: mbr-bootmenu.s ${BOOTDEPS} ${STDDEPS}
	${NASM} ${SECTOPTS} $< -o $@

${BOOTBUILDDIR}/bootmenu: bootmenu.s ${BOOTDEPS} ${STDDEPS}
	${NASM} ${SECTOPTS} $< -o $@

${BOOTBUILDDIR}/bootsect.fat: bootsect-fat.s ${BOOTDEPS} ${STDDEPS}
	${NASM} ${SECTOPTS} $< -o $@

${BOOTBUILDDIR}/bootsect.fat32: bootsect-fat.s ${BOOTDEPS} ${STDDEPS}
	${NASM} ${SECTOPTS} -DFAT32 $< -o $@

${BOOTBUILDDIR}/bootsect.fatnoboot: bootsect-fatnoboot.s ${BOOTDEPS} ${STDDEPS}
	${NASM} ${SECTOPTS} $< -o $@

${BOOTBUILDDIR}/bootsect.fatnoboot32: bootsect-fatnoboot.s ${BOOTDEPS} ${STDDEPS}
	${NASM} ${SECTOPTS} -DFAT32 $< -o $@

#
# Loader files
#

${LOADERBUILDDIR}/vloader: ${OBJS} ${STDDEPS}
	${CC} -nodefaultlibs -nostartfiles ${OPT} ${LINKOPTS} ${OBJS} -o $@

obj/%.o: %.s ${STDDEPS}
	${NASM} ${LDROPTS} $< -o $@

#
# Miscellany
#

clean:
	rm -Rf *~ *.o core obj
	rm -Rf ${BOOTBUILDDIR} ${LOADERBUILDDIR}/vloader
