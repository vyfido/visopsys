##
##  Visopsys
##  Copyright (C) 1998-2003 J. Andrew McLaughlin
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

##  This file is the top-level Makefile for the Visopsys Operating System
##  Kernel.


# Variables

AS		= nasm
CC		= gcc
PLATFORM	= $(shell ../platform.sh)
KVERSION	= -D_KVERSION_=\"$(shell ../../utils/release.sh)\"

STDDEPS		= *.h Makefile

LIB		= -L../lib/${PLATFORM}

INCLUDE		= -I. -I../include

WARN		= -Wall -Wmissing-prototypes -Waggregate-return \
		-Wstrict-prototypes -Wmissing-declarations \
		-Wredundant-decls -Werror

OPT		= -O2

CODEGEN		= -mcpu=pentium -fno-strength-reduce \
		-falign-loops=2 -falign-jumps=2 -falign-functions=2 

CFLAGS		= -nostdlib -nodefaultlibs -nostartfiles -nostdinc \
		${OPT} ${CODEGEN} ${WARN} ${INCLUDE} -DKERNEL

LINKOPTS	= -Wl,-warn-common,-X,--oformat,elf32-i386,-e,kernelMain,-Ttext,0xC0000000,-Map,kernelMap.txt

ASMOPTS		= -f elf -w+orphan-labels


# Object files


GLOBALOBJS	= ${PLATFORM}/kernelMain.o \
		${PLATFORM}/kernelInitialize.o \
		${PLATFORM}/kernelDescriptor.o \
		${PLATFORM}/kernelPageManager.o \
		${PLATFORM}/kernelUser.o \
		${PLATFORM}/kernelLog.o \
		${PLATFORM}/kernelVariableList.o \
		${PLATFORM}/kernelEnvironment.o \
		${PLATFORM}/kernelMemoryManager.o \
		${PLATFORM}/kernelMultitasker.o \
		${PLATFORM}/kernelResourceManager.o \
		${PLATFORM}/kernelEntryPoint.o \
		${PLATFORM}/kernelApi.o \
		${PLATFORM}/kernelLoader.o \
		${PLATFORM}/kernelWindowManager.o \
		${PLATFORM}/kernelWindowTitleBarComponent.o \
		${PLATFORM}/kernelWindowImageComponent.o \
		${PLATFORM}/kernelWindowTextAreaComponent.o \
		${PLATFORM}/kernelWindowTextFieldComponent.o \
		${PLATFORM}/kernelWindowTextLabelComponent.o \
		${PLATFORM}/kernelWindowIconComponent.o \
		${PLATFORM}/kernelWindowButtonComponent.o \
		${PLATFORM}/kernelRandom.o \
		${PLATFORM}/kernelMiscFunctions.o \
		${PLATFORM}/kernelMiscAsmFunctions.o \
		${PLATFORM}/kernelError.o \
		${PLATFORM}/kernelShutdown.o
INTROBJS	= ${PLATFORM}/kernelInterruptsInit.o \
		${PLATFORM}/kernelInterruptHandlers.o
DRIVEROBJS	= ${PLATFORM}/kernelHardwareEnumeration.o \
		${PLATFORM}/kernelDriverManagement.o \
		${PLATFORM}/kernelProcessorDriver.o \
		${PLATFORM}/kernelKeyboardDriver.o \
		${PLATFORM}/kernelPS2MouseDriver.o \
		${PLATFORM}/kernelFloppyDriver.o \
		${PLATFORM}/kernelIdeDriver.o \
		${PLATFORM}/kernelSysTimerDriver.o \
		${PLATFORM}/kernelRtcDriver.o \
		${PLATFORM}/kernelPicDriver.o \
		${PLATFORM}/kernelTextConsoleDriver.o \
		${PLATFORM}/kernelGraphicConsoleDriver.o \
		${PLATFORM}/kernelDmaDriver.o \
		${PLATFORM}/kernelFramebufferGraphicDriver.o
ABSDRIVEROBJS	= ${PLATFORM}/kernelProcessorFunctions.o \
		${PLATFORM}/kernelSysTimerFunctions.o \
		${PLATFORM}/kernelRtcFunctions.o \
		${PLATFORM}/kernelPicFunctions.o \
		${PLATFORM}/kernelSerialFunctions.o \
		${PLATFORM}/kernelDmaFunctions.o \
		${PLATFORM}/kernelKeyboardFunctions.o \
		${PLATFORM}/kernelDiskFunctions.o \
		${PLATFORM}/kernelStream.o \
		${PLATFORM}/kernelText.o \
		${PLATFORM}/kernelGraphicFunctions.o \
		${PLATFORM}/kernelImageFunctions.o \
		${PLATFORM}/kernelFontFunctions.o \
		${PLATFORM}/kernelMouseFunctions.o
FILEOBJS	= ${PLATFORM}/kernelFilesystem.o \
		${PLATFORM}/kernelFile.o \
		${PLATFORM}/kernelFileStream.o \
		${PLATFORM}/kernelFilesystemTypeFat.o

OBJS		= ${GLOBALOBJS} ${INTROBJS} ${DRIVEROBJS} ${ABSDRIVEROBJS} \
		${FILEOBJS}


# Targets

all: visopsys

visopsys: message ${PLATFORM} ${OBJS}
	${CC} ${CFLAGS} ${LIB} ${LINKOPTS} ${OBJS} -lc -o ${PLATFORM}/visopsys

message:
	#
	#  Making kernel on ${PLATFORM}
	#

${PLATFORM}:
	mkdir -p ${PLATFORM}

clean:
	#
	#  Making clean on ${PLATFORM}
	#
	rm -Rf *~ *.o core kernelMap.txt \
	test-${PLATFORM} ${PLATFORM}


#
# Object files
#


# Global

${PLATFORM}/kernelMain.o: kernelMain.c kernelMain.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMain.c -o $@

${PLATFORM}/kernelInitialize.o: kernelInitialize.c kernelInitialize.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelInitialize.c -o $@

${PLATFORM}/kernelShutdown.o: kernelShutdown.c kernelShutdown.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelShutdown.c -o $@

${PLATFORM}/kernelDescriptor.o: kernelDescriptor.c kernelDescriptor.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDescriptor.c -o $@

${PLATFORM}/kernelPageManager.o: kernelPageManager.c kernelPageManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPageManager.c -o $@

${PLATFORM}/kernelUser.o: kernelUser.c kernelUser.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelUser.c -o $@

${PLATFORM}/kernelLog.o: kernelLog.c kernelLog.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelLog.c -o $@

${PLATFORM}/kernelVariableList.o: kernelVariableList.c kernelVariableList.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelVariableList.c -o $@

${PLATFORM}/kernelEnvironment.o: kernelEnvironment.c kernelEnvironment.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelEnvironment.c -o $@

${PLATFORM}/kernelMemoryManager.o: kernelMemoryManager.c kernelMemoryManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMemoryManager.c -o $@

${PLATFORM}/kernelMultitasker.o: kernelMultitasker.c kernelMultitasker.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMultitasker.c -o $@

${PLATFORM}/kernelResourceManager.o: kernelResourceManager.c kernelResourceManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelResourceManager.c -o $@

${PLATFORM}/kernelApi.o: kernelApi.c kernelApi.h ${STDDEPS}
	${CC} ${CFLAGS} -Wno-strict-prototypes ${INCLUDE} -c kernelApi.c -o $@

${PLATFORM}/kernelEntryPoint.o: kernelEntryPoint.s ${STDDEPS}
	${AS} ${ASMOPTS} kernelEntryPoint.s -o $@

${PLATFORM}/kernelLoader.o: kernelLoader.c kernelLoader.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelLoader.c -o $@

${PLATFORM}/kernelWindowManager.o: kernelWindowManager.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowManager.c -o $@

${PLATFORM}/kernelWindowTitleBarComponent.o: kernelWindowTitleBarComponent.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTitleBarComponent.c -o $@

${PLATFORM}/kernelWindowImageComponent.o: kernelWindowImageComponent.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowImageComponent.c -o $@

${PLATFORM}/kernelWindowTextAreaComponent.o: kernelWindowTextAreaComponent.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTextAreaComponent.c -o $@

${PLATFORM}/kernelWindowTextFieldComponent.o: kernelWindowTextFieldComponent.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTextFieldComponent.c -o $@

${PLATFORM}/kernelWindowTextLabelComponent.o: kernelWindowTextLabelComponent.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTextLabelComponent.c -o $@

${PLATFORM}/kernelWindowIconComponent.o: kernelWindowIconComponent.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowIconComponent.c -o $@

${PLATFORM}/kernelWindowButtonComponent.o: kernelWindowButtonComponent.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowButtonComponent.c -o $@

${PLATFORM}/kernelRandom.o: kernelRandom.c kernelRandom.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelRandom.c -o $@

${PLATFORM}/kernelMiscFunctions.o: kernelMiscFunctions.c kernelMiscFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} $(KVERSION) -c kernelMiscFunctions.c -o $@

${PLATFORM}/kernelMiscAsmFunctions.o: kernelMiscAsmFunctions.s kernelAssemblerHeader.h ${STDDEPS}
	${AS} ${ASMOPTS} kernelMiscAsmFunctions.s -o $@

${PLATFORM}/kernelError.o: kernelError.c kernelError.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelError.c -o $@


# Interrupts

${PLATFORM}/kernelInterruptsInit.o: kernelInterruptsInit.c kernelInterruptsInit.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelInterruptsInit.c -o $@

${PLATFORM}/kernelInterruptHandlers.o: kernelInterruptHandlers.s kernelAssemblerHeader.h ${STDDEPS}
	${AS} ${ASMOPTS} kernelInterruptHandlers.s -o $@


# Drivers

${PLATFORM}/kernelHardwareEnumeration.o: kernelHardwareEnumeration.c kernelHardwareEnumeration.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelHardwareEnumeration.c -o $@

${PLATFORM}/kernelDriverManagement.o: kernelDriverManagement.c kernelDriverManagement.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDriverManagement.c -o $@

${PLATFORM}/kernelTextConsoleDriver.o: kernelTextConsoleDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelTextConsoleDriver.c -o $@

${PLATFORM}/kernelGraphicConsoleDriver.o: kernelGraphicConsoleDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelGraphicConsoleDriver.c -o $@

${PLATFORM}/kernelKeyboardDriver.o: kernelKeyboardDriver.s kernelAssemblerHeader.h ${STDDEPS}
	${AS} ${ASMOPTS} kernelKeyboardDriver.s -o $@

${PLATFORM}/kernelPS2MouseDriver.o: kernelPS2MouseDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPS2MouseDriver.c -o $@

${PLATFORM}/kernelProcessorDriver.o: kernelProcessorDriver.s kernelAssemblerHeader.h ${STDDEPS}
	${AS} ${ASMOPTS} kernelProcessorDriver.s -o $@

${PLATFORM}/kernelFloppyDriver.o: kernelFloppyDriver.s kernelAssemblerHeader.h ${STDDEPS}
	${AS} ${ASMOPTS} kernelFloppyDriver.s -o $@

${PLATFORM}/kernelIdeDriver.o: kernelIdeDriver.s kernelAssemblerHeader.h ${STDDEPS}
	${AS} ${ASMOPTS} kernelIdeDriver.s -o $@

${PLATFORM}/kernelRtcDriver.o: kernelRtcDriver.s kernelAssemblerHeader.h ${STDDEPS}
	${AS} ${ASMOPTS} kernelRtcDriver.s -o $@

${PLATFORM}/kernelPicDriver.o: kernelPicDriver.s kernelAssemblerHeader.h ${STDDEPS}
	${AS} ${ASMOPTS} kernelPicDriver.s -o $@

${PLATFORM}/kernelSysTimerDriver.o: kernelSysTimerDriver.s kernelAssemblerHeader.h ${STDDEPS}
	${AS} ${ASMOPTS} kernelSysTimerDriver.s -o $@

${PLATFORM}/kernelDmaDriver.o: kernelDmaDriver.s kernelAssemblerHeader.h ${STDDEPS}
	${AS} ${ASMOPTS} kernelDmaDriver.s -o $@

${PLATFORM}/kernelFramebufferGraphicDriver.o: kernelFramebufferGraphicDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFramebufferGraphicDriver.c -o $@


# Driver abstractions

${PLATFORM}/kernelProcessorFunctions.o: kernelProcessorFunctions.c kernelProcessorFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelProcessorFunctions.c -o $@

${PLATFORM}/kernelSysTimerFunctions.o: kernelSysTimerFunctions.c kernelSysTimerFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelSysTimerFunctions.c -o $@

${PLATFORM}/kernelRtcFunctions.o: kernelRtcFunctions.c kernelRtcFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelRtcFunctions.c -o $@

${PLATFORM}/kernelPicFunctions.o: kernelPicFunctions.c kernelPicFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPicFunctions.c -o $@

${PLATFORM}/kernelSerialFunctions.o: kernelSerialFunctions.c kernelSerialFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelSerialFunctions.c -o $@

${PLATFORM}/kernelDmaFunctions.o: kernelDmaFunctions.c kernelDmaFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDmaFunctions.c -o $@

${PLATFORM}/kernelGraphicFunctions.o: kernelGraphicFunctions.c kernelGraphicFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelGraphicFunctions.c -o $@

${PLATFORM}/kernelImageFunctions.o: kernelImageFunctions.c kernelImageFunctions.h kernelImageFormatBmp.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelImageFunctions.c -o $@

${PLATFORM}/kernelFontFunctions.o: kernelFontFunctions.c kernelFontFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFontFunctions.c -o $@

${PLATFORM}/kernelMouseFunctions.o: kernelMouseFunctions.c kernelMouseFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMouseFunctions.c -o $@

${PLATFORM}/kernelKeyboardFunctions.o: kernelKeyboardFunctions.c kernelKeyboardFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelKeyboardFunctions.c -o $@

${PLATFORM}/kernelDiskFunctions.o: kernelDiskFunctions.c kernelDiskFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDiskFunctions.c -o $@

${PLATFORM}/kernelStream.o: kernelStream.c kernelStream.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelStream.c -o $@

${PLATFORM}/kernelText.o: kernelText.c kernelText.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelText.c -o $@


# Filesystems

${PLATFORM}/kernelFilesystem.o: kernelFilesystem.c kernelFilesystem.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFilesystem.c -o $@

${PLATFORM}/kernelFile.o: kernelFile.c kernelFile.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFile.c -o $@

${PLATFORM}/kernelFileStream.o: kernelFileStream.c kernelFileStream.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFileStream.c -o $@

${PLATFORM}/kernelFilesystemTypeFat.o: kernelFilesystemTypeFat.c kernelFilesystemTypeFat.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFilesystemTypeFat.c -o $@
