##
##  Visopsys
##  Copyright (C) 1998-2004 J. Andrew McLaughlin
## 
##  Makefile
##

##  This file is the Makefile for the Visopsys kernel itself.

BUILDDIR=../../build

CC		= gcc
KVERSION	= -D_KVERSION_=\"$(shell ../../utils/release.sh)\"
STDDEPS		= *.h ../include/sys/*.h Makefile
LIB		= -L../../build/system/libraries/
INCLUDE		= -I. -I../include
WARN		= -Wall -Wmissing-prototypes -Wstrict-prototypes \
		-Wmissing-declarations -Wredundant-decls -Werror
OPT		= -O2 # -DDEBUG
CODEGEN		= -mcpu=pentium -fno-strength-reduce -fno-strict-aliasing
CFLAGS		= -nostdinc -pipe ${OPT} ${CODEGEN} ${WARN} ${INCLUDE} -DKERNEL
LINKOPTS	= -nostdlib -nodefaultlibs -nostartfiles -Wl,-warn-common,-X,--oformat,elf32-i386,-e,kernelMain,-Ttext,0xC0000000

# Object files

# kernelMain needs to go first
OBJS =	obj/kernelMain.o \
	obj/kernelApi.o \
	obj/kernelDescriptor.o \
	obj/kernelDisk.o \
	obj/kernelDma.o \
	obj/kernelDriverManagement.o \
	obj/kernelEncryptMD5.o \
	obj/kernelEnvironment.o \
	obj/kernelError.o \
	obj/kernelFile.o \
	obj/kernelFileStream.o \
	obj/kernelFilesystem.o \
	obj/kernelFilesystemExt.o \
	obj/kernelFilesystemFat.o \
	obj/kernelFilesystemIso.o \
	obj/kernelFont.o \
	obj/kernelGraphic.o \
	obj/kernelHardwareEnumeration.o \
	obj/kernelImage.o \
	obj/kernelInitialize.o \
	obj/kernelInterrupt.o \
	obj/kernelKeyboard.o \
	obj/kernelLoader.o \
	obj/kernelLock.o \
	obj/kernelLog.o \
	obj/kernelMalloc.o \
	obj/kernelMemoryManager.o \
	obj/kernelMouse.o \
	obj/kernelMiscFunctions.o \
	obj/kernelMultitasker.o \
	obj/kernelPageManager.o \
	obj/kernelPic.o \
	obj/kernelRandom.o \
	obj/kernelRtc.o \
	obj/kernelShutdown.o \
	obj/kernelStream.o \
	obj/kernelSysTimer.o \
	obj/kernelText.o \
	obj/kernelUser.o \
	obj/kernelVariableList.o \
	obj/kernelWindow.o \
	obj/kernelWindowBorder.o \
	obj/kernelWindowButton.o \
	obj/kernelWindowCanvas.o \
	obj/kernelWindowCheckbox.o \
	obj/kernelWindowContainer.o \
	obj/kernelWindowEventStream.o \
	obj/kernelWindowIcon.o \
	obj/kernelWindowImage.o \
	obj/kernelWindowList.o \
	obj/kernelWindowListItem.o \
	obj/kernelWindowMenu.o \
	obj/kernelWindowMenuBar.o \
	obj/kernelWindowMenuItem.o \
	obj/kernelWindowPasswordField.o \
	obj/kernelWindowProgressBar.o \
	obj/kernelWindowRadioButton.o \
	obj/kernelWindowScrollBar.o \
	obj/kernelWindowShell.o \
	obj/kernelWindowTextArea.o \
	obj/kernelWindowTextField.o \
	obj/kernelWindowTextLabel.o \
	obj/kernelWindowTitleBar.o

DRIVEROBJS = obj/kernelDmaDriver.o \
	obj/kernelFloppyDriver.o \
	obj/kernelFramebufferGraphicDriver.o \
	obj/kernelGraphicConsoleDriver.o \
	obj/kernelIdeDriver.o \
	obj/kernelKeyboardDriver.o \
	obj/kernelPicDriver.o \
	obj/kernelPS2MouseDriver.o \
	obj/kernelRtcDriver.o \
	obj/kernelSysTimerDriver.o \
	obj/kernelTextConsoleDriver.o

# Targets

all: target-dirs ${OBJS} ${DRIVEROBJS}
	${CC} ${CFLAGS} ${LIB} ${LINKOPTS} ${OBJS} ${DRIVEROBJS} -lc -o ${BUILDDIR}/visopsys
	../../utils/kernel-symbols.sh ${BUILDDIR}/visopsys ${BUILDDIR}/system/kernelSymbols.txt
	strip -s ${BUILDDIR}/visopsys

target-dirs:
	mkdir -p obj
	mkdir -p ${BUILDDIR}

clean:
	rm -Rf *~ *.o core obj ${BUILDDIR}/visopsys ${BUILDDIR}/system/kernelSymbols.txt

#
# Object files
#

obj/kernelApi.o: kernelApi.c kernelApi.h ${STDDEPS}
	${CC} ${CFLAGS} -Wno-strict-prototypes ${INCLUDE} -c kernelApi.c -o $@

obj/kernelDescriptor.o: kernelDescriptor.c kernelDescriptor.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDescriptor.c -o $@

obj/kernelDisk.o: kernelDisk.c kernelDisk.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDisk.c -o $@

obj/kernelDma.o: kernelDma.c kernelDma.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDma.c -o $@

obj/kernelDriverManagement.o: kernelDriverManagement.c kernelDriverManagement.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDriverManagement.c -o $@

obj/kernelEncryptMD5.o: kernelEncryptMD5.c kernelEncrypt.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelEncryptMD5.c -o $@

obj/kernelEnvironment.o: kernelEnvironment.c kernelEnvironment.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelEnvironment.c -o $@

obj/kernelError.o: kernelError.c kernelError.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelError.c -o $@

obj/kernelFile.o: kernelFile.c kernelFile.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFile.c -o $@

obj/kernelFileStream.o: kernelFileStream.c kernelFileStream.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFileStream.c -o $@

obj/kernelFilesystem.o: kernelFilesystem.c kernelFilesystem.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFilesystem.c -o $@

obj/kernelFilesystemExt.o: kernelFilesystemExt.c kernelFilesystemExt.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFilesystemExt.c -o $@

obj/kernelFilesystemFat.o: kernelFilesystemFat.c kernelFilesystemFat.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFilesystemFat.c -o $@

obj/kernelFilesystemIso.o: kernelFilesystemIso.c kernelFilesystemIso.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFilesystemIso.c -o $@

obj/kernelFont.o: kernelFont.c kernelFont.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFont.c -o $@

obj/kernelGraphic.o: kernelGraphic.c kernelGraphic.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelGraphic.c -o $@

obj/kernelHardwareEnumeration.o: kernelHardwareEnumeration.c kernelHardwareEnumeration.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelHardwareEnumeration.c -o $@

obj/kernelImage.o: kernelImage.c kernelImage.h kernelImageFormatBmp.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelImage.c -o $@

obj/kernelInitialize.o: kernelInitialize.c kernelInitialize.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelInitialize.c -o $@

obj/kernelInterrupt.o: kernelInterrupt.c  kernelInterrupt.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelInterrupt.c -o $@

obj/kernelKeyboard.o: kernelKeyboard.c kernelKeyboard.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelKeyboard.c -o $@

obj/kernelLoader.o: kernelLoader.c kernelLoader.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelLoader.c -o $@

obj/kernelLock.o: kernelLock.c kernelLock.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelLock.c -o $@

obj/kernelLog.o: kernelLog.c kernelLog.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelLog.c -o $@

obj/kernelMain.o: kernelMain.c kernelMain.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMain.c -o $@

obj/kernelMalloc.o: kernelMalloc.c kernelMalloc.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMalloc.c -o $@

obj/kernelMemoryManager.o: kernelMemoryManager.c kernelMemoryManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMemoryManager.c -o $@

obj/kernelMiscFunctions.o: kernelMiscFunctions.c kernelMiscFunctions.h ${STDDEPS}
	${CC} ${CFLAGS} $(KVERSION) -c kernelMiscFunctions.c -o $@

obj/kernelMouse.o: kernelMouse.c kernelMouse.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMouse.c -o $@

obj/kernelMultitasker.o: kernelMultitasker.c kernelMultitasker.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelMultitasker.c -o $@

obj/kernelPageManager.o: kernelPageManager.c kernelPageManager.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPageManager.c -o $@

obj/kernelPic.o: kernelPic.c kernelPic.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPic.c -o $@

obj/kernelRandom.o: kernelRandom.c kernelRandom.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelRandom.c -o $@

obj/kernelRtc.o: kernelRtc.c kernelRtc.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelRtc.c -o $@

obj/kernelSerial.o: kernelSerial.c kernelSerial.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelSerial.c -o $@

obj/kernelShutdown.o: kernelShutdown.c kernelShutdown.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelShutdown.c -o $@

obj/kernelStream.o: kernelStream.c kernelStream.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelStream.c -o $@

obj/kernelSysTimer.o: kernelSysTimer.c kernelSysTimer.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelSysTimer.c -o $@

obj/kernelText.o: kernelText.c kernelText.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelText.c -o $@

obj/kernelUser.o: kernelUser.c kernelUser.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelUser.c -o $@

obj/kernelVariableList.o: kernelVariableList.c kernelVariableList.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelVariableList.c -o $@

obj/kernelWindow.o: kernelWindow.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindow.c -o $@

obj/kernelWindowBorder.o: kernelWindowBorder.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowBorder.c -o $@

obj/kernelWindowButton.o: kernelWindowButton.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowButton.c -o $@

obj/kernelWindowCanvas.o: kernelWindowCanvas.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowCanvas.c -o $@

obj/kernelWindowCheckbox.o: kernelWindowCheckbox.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowCheckbox.c -o $@

obj/kernelWindowContainer.o: kernelWindowContainer.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowContainer.c -o $@

obj/kernelWindowEventStream.o: kernelWindowEventStream.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowEventStream.c -o $@

obj/kernelWindowIcon.o: kernelWindowIcon.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowIcon.c -o $@

obj/kernelWindowImage.o: kernelWindowImage.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowImage.c -o $@

obj/kernelWindowList.o: kernelWindowList.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowList.c -o $@

obj/kernelWindowListItem.o: kernelWindowListItem.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowListItem.c -o $@

obj/kernelWindowMenu.o: kernelWindowMenu.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowMenu.c -o $@

obj/kernelWindowMenuBar.o: kernelWindowMenuBar.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowMenuBar.c -o $@

obj/kernelWindowMenuItem.o: kernelWindowMenuItem.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowMenuItem.c -o $@

obj/kernelWindowPasswordField.o: kernelWindowPasswordField.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowPasswordField.c -o $@

obj/kernelWindowProgressBar.o: kernelWindowProgressBar.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowProgressBar.c -o $@

obj/kernelWindowRadioButton.o: kernelWindowRadioButton.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowRadioButton.c -o $@

obj/kernelWindowScrollBar.o: kernelWindowScrollBar.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowScrollBar.c -o $@

obj/kernelWindowShell.o: kernelWindowShell.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowShell.c -o $@

obj/kernelWindowTextArea.o: kernelWindowTextArea.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTextArea.c -o $@

obj/kernelWindowTextField.o: kernelWindowTextField.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTextField.c -o $@

obj/kernelWindowTextLabel.o: kernelWindowTextLabel.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTextLabel.c -o $@

obj/kernelWindowTitleBar.o: kernelWindowTitleBar.c kernelWindow.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelWindowTitleBar.c -o $@

#
# Drivers
#

obj/kernelDmaDriver.o: kernelDmaDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelDmaDriver.c -o $@

obj/kernelFloppyDriver.o: kernelFloppyDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFloppyDriver.c -o $@

obj/kernelFramebufferGraphicDriver.o: kernelFramebufferGraphicDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelFramebufferGraphicDriver.c -o $@

obj/kernelGraphicConsoleDriver.o: kernelGraphicConsoleDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelGraphicConsoleDriver.c -o $@

obj/kernelIdeDriver.o: kernelIdeDriver.c kernelIdeDriver.h ${STDDEPS}
	${CC} ${CFLAGS} -c kernelIdeDriver.c -o $@

obj/kernelKeyboardDriver.o: kernelKeyboardDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelKeyboardDriver.c -o $@

obj/kernelPicDriver.o: kernelPicDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPicDriver.c -o $@

obj/kernelPS2MouseDriver.o: kernelPS2MouseDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelPS2MouseDriver.c -o $@

obj/kernelRtcDriver.o: kernelRtcDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelRtcDriver.c -o $@

obj/kernelSysTimerDriver.o: kernelSysTimerDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelSysTimerDriver.c -o $@

obj/kernelTextConsoleDriver.o: kernelTextConsoleDriver.c ${STDDEPS}
	${CC} ${CFLAGS} -c kernelTextConsoleDriver.c -o $@
