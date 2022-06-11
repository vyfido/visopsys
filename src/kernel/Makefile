##
##  Visopsys
##  Copyright (C) 1998-2021 J. Andrew McLaughlin
##
##  Makefile
##

##  This file is the Makefile for the Visopsys kernel itself.

ROOT = ../..
BUILDDIR = build
UTILSDIR = ${ROOT}/utils
OUTPUTDIR = ${ROOT}/${BUILDDIR}
INCDIR = ../include
LIBDIR = ${OUTPUTDIR}/system/libraries

include ${ROOT}/Makefile.include

STDDEPS = ${ROOT}/Makefile.include Makefile kernel-*.ld *.h \
	${INCDIR}/*.h ${INCDIR}/sys/*.h
STATLIBS = ${LIBDIR}/libvis.a ${LIBDIR}/libc.a
INCLUDE = -nostdinc -I. -I${INCDIR}
CFLAGS = ${OPT} ${ARCHFLAGS} ${CCODEGEN} ${CWARN} ${INCLUDE} -DKERNEL ${DEBUG}
LFLAGS = -L${LIBDIR} -nostdlib -Wl,-static -Wl,-warn-common -Wl,-X \
	-Wl,-T,kernel-${ARCH}.ld
RELEASE = $(shell ${UTILSDIR}/release.sh)

ifdef DEBUG
	override DEBUG = -DDEBUG
else
	STRIP = strip -s ${KERNEL}
endif

# kernelMain needs to go first
NAMES = \
	kernelMain \
	kernelApi \
	kernelBus \
	kernelCharset \
	kernelCpu \
	kernelDebug \
	kernelDescriptor \
	kernelDevice \
	kernelDisk \
	kernelDma \
	kernelDriver \
	kernelCryptHashMd5 \
	kernelCryptHashSha1 \
	kernelCryptHashSha256 \
	kernelEnvironment \
	kernelError \
	kernelFile \
	kernelFileStream \
	kernelFilesystem \
	kernelFilesystemExt \
	kernelFilesystemFat \
	kernelFilesystemIso \
	kernelFilesystemLinuxSwap \
	kernelFilesystemNtfs \
	kernelFilesystemUdf \
	kernelFont \
	kernelFontTtf \
	kernelFontVbf \
	kernelGraphic \
	kernelImage \
	kernelImageBmp \
	kernelImageIco \
	kernelImageJpg \
	kernelImagePpm \
	kernelInitialize \
	kernelInterrupt \
	kernelKeyboard \
	kernelLoader \
	kernelLoaderClass \
	kernelLoaderElf \
	kernelLocale \
	kernelLock \
	kernelLog \
	kernelMalloc \
	kernelMemory \
	kernelMisc \
	kernelMouse \
	kernelMultitasker \
	kernelNetwork \
	kernelNetworkArp \
	kernelNetworkDevice \
	kernelNetworkDhcp \
	kernelNetworkDns \
	kernelNetworkEthernet \
	kernelNetworkIcmp \
	kernelNetworkIp4 \
	kernelNetworkStream \
	kernelNetworkTcp \
	kernelNetworkUdp \
	kernelPage \
	kernelPic \
	kernelPipe \
	kernelPower \
	kernelRandom \
	kernelRtc \
	kernelShutdown \
	kernelStream \
	kernelSysTimer \
	kernelText \
	kernelTouch \
	kernelUser \
	kernelVmware \
	kernelWindow \
	kernelWindowBorder \
	kernelWindowButton \
	kernelWindowCanvas \
	kernelWindowCheckbox \
	kernelWindowComponent \
	kernelWindowContainer \
	kernelWindowDivider \
	kernelWindowEventStream \
	kernelWindowIcon \
	kernelWindowImage \
	kernelWindowList \
	kernelWindowListItem \
	kernelWindowMenu \
	kernelWindowMenuBar \
	kernelWindowMenuBarIcon \
	kernelWindowMenuItem \
	kernelWindowPasswordField \
	kernelWindowProgressBar \
	kernelWindowRadioButton \
	kernelWindowScrollBar \
	kernelWindowShell \
	kernelWindowSlider \
	kernelWindowSysContainer \
	kernelWindowTextArea \
	kernelWindowTextField \
	kernelWindowTextLabel \
	kernelWindowTitleBar \
	kernelWindowTree

DRIVERNAMES = \
	kernelAcpiDriver \
	kernelApicDriver \
	kernelAtaDriver \
	kernelDmaDriver \
	kernelFloppyDriver \
	kernelFramebufferGraphicDriver \
	kernelGraphicConsoleDriver \
	kernelIdeDriver \
	kernelNetworkLoopDriver \
	kernelNetworkPcNetDriver \
	kernelPciDriver \
	kernelPicDriver \
	kernelPs2KeyboardDriver \
	kernelPs2MouseDriver \
	kernelRamDiskDriver \
	kernelRtcDriver \
	kernelSataAhciDriver \
	kernelScsiDiskDriver \
	kernelSysTimerDriver \
	kernelSystemDriver \
	kernelTextConsoleDriver \
	kernelUsbAtapiDriver \
	kernelUsbDriver \
	kernelUsbEhciDriver \
	kernelUsbGenericDriver \
	kernelUsbKeyboardDriver \
	kernelUsbMouseDriver \
	kernelUsbHubDriver \
	kernelUsbOhciDriver \
	kernelUsbTouchscreenDriver \
	kernelUsbUhciDriver \
	kernelUsbXhciDriver

ALLNAMES = ${NAMES} ${DRIVERNAMES}
OBJS = $(addprefix obj/, $(addsuffix .o, ${ALLNAMES}))
KERNEL = ${OUTPUTDIR}/visopsys

# Targets

all: target-dirs ${KERNEL} strip
	${MAKE} -C de BUILDDIR=${BUILDDIR}
	${MAKE} -C es BUILDDIR=${BUILDDIR}
	${MAKE} -C ru BUILDDIR=${BUILDDIR}
	${MAKE} -C tr BUILDDIR=${BUILDDIR}

target-dirs:
	mkdir -p obj
	mkdir -p ${OUTPUTDIR}

${KERNEL}: ${OBJS} ${STATLIBS}
	${CC} ${CFLAGS} ${LFLAGS} ${OBJS} -lvis -lc -lgcc -o $@

strip: ${KERNEL}
	${STRIP}

obj/%.o: %.c ${STDDEPS}
	${CC} ${CFLAGS} -c $< -o $@

# Object files with explicit rules

obj/kernelApi.o: kernelApi.c ${STDDEPS}
	${CC} ${CFLAGS} -O0 -Wno-strict-prototypes -c $< -o $@

obj/kernelMisc.o: kernelMisc.c ${STDDEPS}
	${CC} ${CFLAGS} -D_KVERSION_=\"${RELEASE}\" -c $< -o $@

clean:
	${MAKE} -C de BUILDDIR=${BUILDDIR} clean
	${MAKE} -C es BUILDDIR=${BUILDDIR} clean
	${MAKE} -C ru BUILDDIR=${BUILDDIR} clean
	${MAKE} -C tr BUILDDIR=${BUILDDIR} clean
	rm -Rf *~ *.o core obj ${KERNEL}

