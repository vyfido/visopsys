//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
// 
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the Free
//  Software Foundation; either version 2 of the License, or (at your option)
//  any later version.
// 
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
//  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
//  for more details.
//  
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  kernelDriverManagement.h
//

// Prototypes for the assembler routines associated with the 
// default device drivers.  The names are pretty much self-explanatory.

#if !defined(_KERNELDRIVERMANAGEMENT_H)

#include "kernelProcessorFunctions.h"
#include "kernelSysTimerFunctions.h"
#include "kernelRtcFunctions.h"
#include "kernelPicFunctions.h"
#include "kernelSerialFunctions.h"
#include "kernelDmaFunctions.h"
#include "kernelKeyboardFunctions.h"
#include "kernelMouseFunctions.h"
#include "kernelDiskFunctions.h"
#include "kernelGraphicFunctions.h"

// These driver functions are not declared in other header files

// Default Processor routines
int kernelProcessorDriverInitialize(void);
unsigned long *kernelProcessorDriverReadTimestamp(void);

// Default PIC routines
int kernelPicDriverInitialize(void);
void kernelPicDriverEndOfInterrupt(int);

// Default System Timer routines
int kernelSysTimerDriverInitialize(void);
void kernelSysTimerDriverTick(void);
int kernelSysTimerDriverRead(void);
int kernelSysTimerDriverSetTimer(int, int, int);
int kernelSysTimerDriverReadTimer(int);

// Default Real-Time clock routines
int kernelRtcDriverInitialize(void);
int kernelRtcDriverReadSeconds(void);
int kernelRtcDriverReadMinutes(void);
int kernelRtcDriverReadHours(void);
int kernelRtcDriverReadDayOfWeek(void);
int kernelRtcDriverReadDayOfMonth(void);
int kernelRtcDriverReadMonth(void);
int kernelRtcDriverReadYear(void);

// Default DMA driver routines
int kernelDmaDriverInitialize(void);
int kernelDmaDriverSetupChannel(int, int, int, int);
int kernelDmaDriverSetMode(int, int);
int kernelDmaDriverEnableChannel(int);
int kernelDmaDriverCloseChannel(int);

// Default keyboard driver routines
int kernelKeyboardDriverInitialize(void);
int kernelKeyboardDriverSetStream(stream *, int (*)(stream *, ...));
void kernelKeyboardDriverReadData(void);

// Default PS2 mouse driver routines
int kernelPS2MouseDriverInitialize(void);
void kernelPS2MouseDriverReadData(void);

// Default floppy driver routines
int kernelFloppyDriverInitialize(void);
int kernelFloppyDriverDescribe(int, ...);
int kernelFloppyDriverReset(int);
int kernelFloppyDriverRecalibrate(int);
int kernelFloppyDriverMotorOn(int);
int kernelFloppyDriverMotorOff(int);
int kernelFloppyDriverDiskChanged(int);
int kernelFloppyDriverReadSectors(int, unsigned, unsigned, unsigned,
				  unsigned, unsigned, void *);
int kernelFloppyDriverWriteSectors(int, unsigned, unsigned, unsigned,
				   unsigned, unsigned, void *);
int kernelFloppyDriverLastErrorCode(void);
void *kernelFloppyDriverLastErrorMessage(void);

// Default hard disk driver routines
int kernelIdeDriverInitialize(void);
int kernelIdeDriverReset(int);
int kernelIdeDriverRecalibrate(int);
int kernelIdeDriverReadSectors(int, unsigned, unsigned, unsigned, unsigned,
			       unsigned, void *);
int kernelIdeDriverWriteSectors(int, unsigned, unsigned, unsigned, unsigned,
				unsigned, void *);
int kernelIdeDriverLastErrorCode(void);
void *kernelIdeDriverLastErrorMessage(void);

// Default framebuffer graphic driver routines
int kernelLFBGraphicDriverInitialize(void *);
int kernelLFBGraphicDriverClearScreen(color *);
int kernelLFBGraphicDriverDrawPixel(kernelGraphicBuffer *, color *, drawMode,
				    int, int);
int kernelLFBGraphicDriverDrawLine(kernelGraphicBuffer *, color *, drawMode,
				   int, int, int, int);
int kernelLFBGraphicDriverDrawRect(kernelGraphicBuffer *, color *, drawMode,
				   int, int, unsigned, unsigned, unsigned,
				   int);
int kernelLFBGraphicDriverDrawOval(kernelGraphicBuffer *, color *, drawMode,
				   int, int, unsigned, unsigned, unsigned,
				   int);
int kernelLFBGraphicDriverDrawMonoImage(kernelGraphicBuffer *, image *,
					color *, color *, int, int);
int kernelLFBGraphicDriverDrawImage(kernelGraphicBuffer *, image *, int, int);
int kernelLFBGraphicDriverGetImage(kernelGraphicBuffer *, image *, int, int,
				   unsigned, unsigned);
int kernelLFBGraphicDriverCopyArea(kernelGraphicBuffer *, int, int, unsigned,
				   unsigned, int, int);
int kernelLFBGraphicDriverRenderBuffer(kernelGraphicBuffer *, int, int, int,
				       int, unsigned, unsigned);

// Structures

typedef struct
{
  // This structure contains a vector for each
  // possible kind of driver
  
  kernelProcessorDriver *processorDriver;
  kernelPicDeviceDriver *picDriver;
  kernelSysTimerDriver *sysTimerDriver;
  kernelRtcDeviceDriver *rtcDriver;
  kernelSerialDriver *serialDriver;
  kernelDmaDeviceDriver *dmaDriver;
  kernelKeyboardDriver *keyboardDriver;
  kernelMouseDriver *mouseDriver;
  kernelDiskDeviceDriver *floppyDriver;
  kernelDiskDeviceDriver *hardDiskDriver;
  kernelGraphicDriver *graphicDriver;

} kernelDriverManager;

// Functions exported by kernelDriverManagement.c
int kernelInstallProcessorDriver(void);
int kernelInstallPicDriver(void);
int kernelInstallSysTimerDriver(void);
int kernelInstallRtcDriver(void);
int kernelInstallDmaDriver(void);
int kernelInstallKeyboardDriver(void);
int kernelInstallMouseDriver(void);
int kernelInstallFloppyDriver(void);
int kernelInstallHardDiskDriver(void);
int kernelInstallGraphicDriver(void);

#define _KERNELDRIVERMANAGEMENT_H
#endif
