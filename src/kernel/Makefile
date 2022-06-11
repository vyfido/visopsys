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

CC		= gcc
KVERSION	= -D_KVERSION_=\"$(shell ../../utils/release.sh)\"

STDDEPS		= *.h Makefile

LIB		= -L../lib/build
INCLUDE		= -I. -I../include
WARN		= -Wall -Wmissing-prototypes -Wstrict-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror
OPT		= -O2 # -DDEBUG
CODEGEN		= -mcpu=pentium -fno-strength-reduce
CFLAGS		= -nostdinc -pipe ${OPT} ${CODEGEN} ${WARN} ${INCLUDE} -DKERNEL

LINKOPTS	= -nostdlib -nodefaultlibs -nostartfiles -Wl,-warn-common,-X,--oformat,elf32-i386,-e,kernelMain,-Ttext,0xC0000000


# Object files


GLOBALOBJS	= build/kernelMain.o \
		build/kernelInitialize.o \
		build/kernelInterrupt.o \
		build/kernelDescriptor.o \
		build/kernelPageManager.o \
		build/kernelUser.o \
		build/kernelLog.o \
		build/kernelVariableList.o \
		build/kernelEnvironment.o \
		build/kernelMalloc.o \
		build/kernelMemoryManager.o \
		build/kernelMultitasker.o \
		build/kernelLock.o \
		build/kernelApi.o \
		build/kernelLoader.o \
		build/kernelWindowManager.o \
		build/kernelWindowEventStream.o \
		build/kernelWindowTitleBar.o \
		build/kernelWindowImage.o \
		build/kernelWindowTextArea.o \
		build/kernelWindowTextField.o \
		build/kernelWindowTextLabel.o \
		build/kernelWindowIcon.o \
		build/kernelWindowButton.o \
		build/kernelRandom.o \
		build/kernelMiscFunctions.o \
		build/kernelError.o \
		build/kernelShutdown.o
DRIVEROBJS	= build/kernelHardwareEnumeration.o \
		build/kernelDriverManagement.o \
		build/kernelKeyboardDriver.o \
		build/kernelPS2MouseDriver.o \
		build/kernelFloppyDriver.o \
		build/kernelIdeDriver.o \
		build/kernelSysTimerDriver.o \
		build/kernelRtcDriver.o \
		build/kernelPicDriver.o \
		build/kernelTextConsoleDriver.o \
		build/kernelGraphicConsoleDriver.o \
		build/kernelDmaDriver.o \
		build/kernelFramebufferGraphicDriver.o
ABSDRIVEROBJS	= build/kernelSysTimer.o \
		build/kernelRtc.o \
		build/kernelPic.o \
		build/kernelDma.o \
		build/kernelKeyboard.o \
		build/kernelDisk.o \
		build/kernelStream.o \
		build/kernelText.o \
		build/kernelGraphic.o \
		build/kernelImage.o \
		build/kernelFont.o \
		build/kernelMouse.o
		#build/kernelSerial.o
FILEOBJS	= build/kernelFilesystem.o \
		build/kernelFile.o \
		build/kernelFileStream.o \
		build/kernelFilesystemFat.o

OBJS		= ${GLOBALOBJS} ${DRIVEROBJS} ${ABSDRIVEROBJS} \
		${FILEOBJS}


# Targets

all: visopsys

visopsys: message build ${OBJS}
	${CC} ${CFLAGS} ${LIB} ${LINKOPTS} ${OBJS} -lc -o build/visopsys
	../../utils/kernel-symbols.sh build/visopsys build/kernelSymbols.txt

message:
	#
	#  Making kernel
	#

build:
	mkdir -p build

clean:
	#
	#  Making clean
	#
	rm -Rf *~ *.o core build


#
# Object files
#


# Global

build/kernelMain.o: kernelMain.c kernelMain.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMain.c -o $@

build/kernelInitialize.o: kernelInitialize.c kernelInitialize.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelInitialize.c -o $@

build/kernelInterrupt.o: kernelInterrupt.c  kernelInterrupt.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelInterrupt.c -o $@

build/kernelDescriptor.o: kernelDescriptor.c kernelDescriptor.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDescriptor.c -o $@

build/kernelPageManager.o: kernelPageManager.c kernelPageManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPageManager.c -o $@

build/kernelUser.o: kernelUser.c kernelUser.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelUser.c -o $@

build/kernelLog.o: kernelLog.c kernelLog.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelLog.c -o $@

build/kernelVariableList.o: kernelVariableList.c kernelVariableList.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelVariableList.c -o $@

build/kernelEnvironment.o: kernelEnvironment.c kernelEnvironment.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelEnvironment.c -o $@

build/kernelMalloc.o: kernelMalloc.c kernelMalloc.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMalloc.c -o $@

build/kernelMemoryManager.o: kernelMemoryManager.c kernelMemoryManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMemoryManager.c -o $@

build/kernelMultitasker.o: kernelMultitasker.c kernelMultitasker.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMultitasker.c -o $@

build/kernelLock.o: kernelLock.c kernelLock.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelLock.c -o $@

build/kernelApi.o: kernelApi.c kernelApi.h ${STDDEPS}
	${CC} ${CFLAGS} -Wno-strict-prototypes ${INCLUDE} -c kernelApi.c -o $@

build/kernelLoader.o: kernelLoader.c kernelLoader.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelLoader.c -o $@

build/kernelWindowManager.o: kernelWindowManager.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowManager.c -o $@

build/kernelWindowEventStream.o: kernelWindowEventStream.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowEventStream.c -o $@

build/kernelWindowTitleBar.o: kernelWindowTitleBar.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTitleBar.c -o $@

build/kernelWindowImage.o: kernelWindowImage.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowImage.c -o $@

build/kernelWindowTextArea.o: kernelWindowTextArea.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTextArea.c -o $@

build/kernelWindowTextField.o: kernelWindowTextField.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTextField.c -o $@

build/kernelWindowTextLabel.o: kernelWindowTextLabel.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTextLabel.c -o $@

build/kernelWindowIcon.o: kernelWindowIcon.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowIcon.c -o $@

build/kernelWindowButton.o: kernelWindowButton.c kernelWindowManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowButton.c -o $@

build/kernelRandom.o: kernelRandom.c kernelRandom.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelRandom.c -o $@

build/kernelMiscFunctions.o: kernelMiscFunctions.c kernelMiscFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} $(KVERSION) -c kernelMiscFunctions.c -o $@

build/kernelError.o: kernelError.c kernelError.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelError.c -o $@

build/kernelShutdown.o: kernelShutdown.c kernelShutdown.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelShutdown.c -o $@


# Drivers

build/kernelHardwareEnumeration.o: kernelHardwareEnumeration.c kernelHardwareEnumeration.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelHardwareEnumeration.c -o $@

build/kernelDriverManagement.o: kernelDriverManagement.c kernelDriverManagement.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDriverManagement.c -o $@

build/kernelTextConsoleDriver.o: kernelTextConsoleDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelTextConsoleDriver.c -o $@

build/kernelGraphicConsoleDriver.o: kernelGraphicConsoleDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelGraphicConsoleDriver.c -o $@

build/kernelKeyboardDriver.o: kernelKeyboardDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelKeyboardDriver.c -o $@

build/kernelPS2MouseDriver.o: kernelPS2MouseDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPS2MouseDriver.c -o $@

build/kernelFloppyDriver.o: kernelFloppyDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFloppyDriver.c -o $@

build/kernelIdeDriver.o: kernelIdeDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelIdeDriver.c -o $@

build/kernelRtcDriver.o: kernelRtcDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelRtcDriver.c -o $@

build/kernelPicDriver.o: kernelPicDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPicDriver.c -o $@

build/kernelSysTimerDriver.o: kernelSysTimerDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelSysTimerDriver.c -o $@

build/kernelDmaDriver.o: kernelDmaDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDmaDriver.c -o $@

build/kernelFramebufferGraphicDriver.o: kernelFramebufferGraphicDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFramebufferGraphicDriver.c -o $@


# Driver abstractions

build/kernelSysTimer.o: kernelSysTimer.c kernelSysTimer.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelSysTimer.c -o $@

build/kernelRtc.o: kernelRtc.c kernelRtc.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelRtc.c -o $@

build/kernelPic.o: kernelPic.c kernelPic.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPic.c -o $@

build/kernelSerial.o: kernelSerial.c kernelSerial.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelSerial.c -o $@

build/kernelDma.o: kernelDma.c kernelDma.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDma.c -o $@

build/kernelGraphic.o: kernelGraphic.c kernelGraphic.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelGraphic.c -o $@

build/kernelImage.o: kernelImage.c kernelImage.h kernelImageFormatBmp.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelImage.c -o $@

build/kernelFont.o: kernelFont.c kernelFont.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFont.c -o $@

build/kernelMouse.o: kernelMouse.c kernelMouse.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMouse.c -o $@

build/kernelKeyboard.o: kernelKeyboard.c kernelKeyboard.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelKeyboard.c -o $@

build/kernelDisk.o: kernelDisk.c kernelDisk.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDisk.c -o $@

build/kernelStream.o: kernelStream.c kernelStream.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelStream.c -o $@

build/kernelText.o: kernelText.c kernelText.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelText.c -o $@


# Filesystems

build/kernelFilesystem.o: kernelFilesystem.c kernelFilesystem.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFilesystem.c -o $@

build/kernelFile.o: kernelFile.c kernelFile.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFile.c -o $@

build/kernelFileStream.o: kernelFileStream.c kernelFileStream.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFileStream.c -o $@

build/kernelFilesystemFat.o: kernelFilesystemFat.c kernelFilesystemFat.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFilesystemFat.c -o $@
