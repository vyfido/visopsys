//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
#include "kernelDmaFunctions.h"
#include "kernelKeyboardFunctions.h"
#include "kernelDiskFunctions.h"
#include <sys/stream.h>

// Definitions

// Default Processor routines
int kernelProcessorDriverInitialize(void);
unsigned long *kernelProcessorDriverReadTimestamp(void);
#define DEFAULTPROCINIT &kernelProcessorDriverInitialize
#define DEFAULTPROCRDTSC &kernelProcessorDriverReadTimestamp

// Default PIC routines
int kernelPicDriverInitialize(void);
void kernelPicDriverEndOfInterrupt(int);
#define DEFAULTPICINITIALIZE &kernelPicDriverInitialize
#define DEFAULTPICEOI &kernelPicDriverEndOfInterrupt

// Default System Timer routines
int kernelSysTimerDriverInitialize(void);
void kernelSysTimerDriverTick(void);
int kernelSysTimerDriverRead(void);
int kernelSysTimerDriverSetTimer(int, int, int);
int kernelSysTimerDriverReadTimer(int);
#define DEFAULTSYSTIMERINIT &kernelSysTimerDriverInitialize
#define DEFAULTSYSTIMERTICK &kernelSysTimerDriverTick
#define DEFAULTSYSTIMERREADTICKS &kernelSysTimerDriverRead
#define DEFAULTSYSTIMERREADVALUE &kernelSysTimerDriverReadTimer
#define DEFAULTSYSTIMERSETUPTIMER &kernelSysTimerDriverSetTimer

// Default Real-Time clock routines
int kernelRtcDriverInitialize(void);
int kernelRtcDriverReadSeconds(void);
int kernelRtcDriverReadMinutes(void);
int kernelRtcDriverReadHours(void);
int kernelRtcDriverReadDayOfWeek(void);
int kernelRtcDriverReadDayOfMonth(void);
int kernelRtcDriverReadMonth(void);
int kernelRtcDriverReadYear(void);
#define DEFAULTRTCINITIALIZE &kernelRtcDriverInitialize
#define DEFAULTRTCSECONDS &kernelRtcDriverReadSeconds
#define DEFAULTRTCMINUTES &kernelRtcDriverReadMinutes
#define DEFAULTRTCHOURS &kernelRtcDriverReadHours
#define DEFAULTRTCDAYOFWEEK &kernelRtcDriverReadDayOfWeek
#define DEFAULTRTCDAYOFMONTH &kernelRtcDriverReadDayOfMonth
#define DEFAULTRTCMONTH &kernelRtcDriverReadMonth
#define DEFAULTRTCYEAR &kernelRtcDriverReadYear

// Default DMA driver routines
int kernelDmaDriverInitialize(void);
int kernelDmaDriverSetupChannel(int, int, int, int);
int kernelDmaDriverSetMode(int, int);
int kernelDmaDriverEnableChannel(int);
int kernelDmaDriverCloseChannel(int);
#define DEFAULTDMAINIT &kernelDmaDriverInitialize;
#define DEFAULTDMASETUPCHANNEL &kernelDmaDriverSetupChannel;
#define DEFAULTDMASETMODE &kernelDmaDriverSetMode;
#define DEFAULTDMAENABLECHANNEL &kernelDmaDriverEnableChannel;
#define DEFAULTDMACLOSECHANNEL &kernelDmaDriverCloseChannel;

// Default keyboard driver routines
int kernelKeyboardDriverInitialize(stream *, int (*)(stream *, ...));
void kernelKeyboardDriverReadData(void);
#define DEFAULTKBRDINIT &kernelKeyboardDriverInitialize
#define DEFAULTKBRDREADDATA &kernelKeyboardDriverReadData

// Default floppy driver routines
int kernelFloppyDriverInitialize(void);
int kernelFloppyDriverDescribe(int, ...);
int kernelFloppyDriverReset(int);
int kernelFloppyDriverRecalibrate(int);
int kernelFloppyDriverMotorOn(int);
int kernelFloppyDriverMotorOff(int);
int kernelFloppyDriverDiskChanged(int);
int kernelFloppyDriverReadSectors(int, unsigned int, unsigned int,
		   unsigned int, unsigned int, unsigned int, void *);
int kernelFloppyDriverWriteSectors(int, unsigned int, unsigned int,
		   unsigned int, unsigned int, unsigned int, void *);
int kernelFloppyDriverLastErrorCode(void);
void *kernelFloppyDriverLastErrorMessage(void);
#define DEFAULTFLOPPYINIT &kernelFloppyDriverInitialize
#define DEFAULTFLOPPYDESCRIBE &kernelFloppyDriverDescribe
#define DEFAULTFLOPPYRESET &kernelFloppyDriverReset
#define DEFAULTFLOPPYRECALIBRATE &kernelFloppyDriverRecalibrate
#define DEFAULTFLOPPYMOTORON &kernelFloppyDriverMotorOn
#define DEFAULTFLOPPYMOTOROFF &kernelFloppyDriverMotorOff
#define DEFAULTFLOPPYDISKCHANGED &kernelFloppyDriverDiskChanged
#define DEFAULTFLOPPYREAD &kernelFloppyDriverReadSectors
#define DEFAULTFLOPPYWRITE &kernelFloppyDriverWriteSectors
#define DEFAULTFLOPPYERRCODE &kernelFloppyDriverLastErrorCode
#define DEFAULTFLOPPYERRMESS &kernelFloppyDriverLastErrorMessage


// Default hard disk driver routines
int kernelHardDiskDriverInitialize(void);
int kernelHardDiskDriverReset(int);
int kernelHardDiskDriverRecalibrate(int);
int kernelHardDiskDriverReadSectors(int, unsigned int, unsigned int,
		   unsigned int, unsigned int, unsigned int, void *);
int kernelHardDiskDriverWriteSectors(int, unsigned int, unsigned int,
		   unsigned int, unsigned int, unsigned int, void *);
int kernelHardDiskDriverLastErrorCode(void);
void *kernelHardDiskDriverLastErrorMessage(void);
#define DEFAULTHDDINIT &kernelHardDiskDriverInitialize
#define DEFAULTHDDESCRIBE NULL
#define DEFAULTHDDRESET &kernelHardDiskDriverReset
#define DEFAULTHDDRECALIBRATE &kernelHardDiskDriverRecalibrate
#define DEFAULTHDDMOTORON NULL
#define DEFAULTHDDMOTOROFF NULL
#define DEFAULTHDDDISKCHANGED NULL
#define DEFAULTHDDREAD &kernelHardDiskDriverReadSectors
#define DEFAULTHDDWRITE &kernelHardDiskDriverWriteSectors
#define DEFAULTHDDERRCODE &kernelHardDiskDriverLastErrorCode
#define DEFAULTHDDERRMESS &kernelHardDiskDriverLastErrorMessage


// Structures

typedef struct
{
  // This structure contains a vector for each
  // possible kind of driver
  
  kernelProcessorDriver *processorDriver;
  kernelPicDeviceDriver *picDriver;
  kernelSysTimerDriver *sysTimerDriver;
  kernelRtcDeviceDriver *rtcDriver;
  kernelDmaDeviceDriver *dmaDriver;
  kernelKeyboardDriver *keyboardDriver;
  kernelDiskDeviceDriver *floppyDriver;
  kernelDiskDeviceDriver *hardDiskDriver;

} kernelDriverManager;


// Functions exported by kernelDriverManagement.c
int kernelDriverManagementInitialize(void);
int kernelInstallProcessorDriver(void);
int kernelInstallPicDriver(void);
int kernelInstallSysTimerDriver(void);
int kernelInstallRtcDriver(void);
int kernelInstallDmaDriver(void);
int kernelInstallKeyboardDriver(void);
int kernelInstallFloppyDriver(void);
int kernelInstallHardDiskDriver(void);

#define _KERNELDRIVERMANAGEMENT_H
#endif
