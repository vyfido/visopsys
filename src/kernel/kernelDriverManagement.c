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
//  kernelDriverManagement.c
//

#include "kernelDriverManagement.h"
#include "kernelHardwareEnumeration.h"
#include "kernelError.h"
#include <sys/errors.h>


// Static default driver structures

// Default Processor routines
static kernelProcessorDriver defaultProcessorDriver =
{
  kernelProcessorDriverInitialize,
  kernelProcessorDriverReadTimestamp
};

// Default PIC routines
static kernelPicDeviceDriver defaultPicDriver =
{
  kernelPicDriverInitialize,
  kernelPicDriverEndOfInterrupt
};

// Default System Timer routines
static kernelSysTimerDriver defaultSysTimerDriver =
{
  kernelSysTimerDriverInitialize,
  kernelSysTimerDriverTick,
  kernelSysTimerDriverRead,
  kernelSysTimerDriverReadTimer,
  kernelSysTimerDriverSetTimer
};

// Default Real-Time clock routines
static kernelRtcDeviceDriver defaultRtcDriver =
{
  kernelRtcDriverInitialize,
  kernelRtcDriverReadSeconds,
  kernelRtcDriverReadMinutes,
  kernelRtcDriverReadHours,
  kernelRtcDriverReadDayOfWeek,
  kernelRtcDriverReadDayOfMonth,
  kernelRtcDriverReadMonth,
  kernelRtcDriverReadYear
};

// Default serial port routines
static kernelSerialDriver defaultSerialDriver =
{
  NULL,
  NULL
};

// Default DMA driver routines
static kernelDmaDeviceDriver defaultDmaDriver =
{
  kernelDmaDriverInitialize,
  kernelDmaDriverSetupChannel,
  kernelDmaDriverSetMode,
  kernelDmaDriverEnableChannel,
  kernelDmaDriverCloseChannel
};

// Default keyboard driver routines
static kernelKeyboardDriver defaultKeyboardDriver =
{
  kernelKeyboardDriverInitialize,
  kernelKeyboardDriverSetStream,
  kernelKeyboardDriverReadData
};

// Default mouse driver routines
static kernelMouseDriver defaultMouseDriver =
{
  kernelPS2MouseDriverInitialize,
  kernelPS2MouseDriverReadData
};

// Default floppy driver routines
static kernelDiskDeviceDriver defaultFloppyDriver =
{
  kernelFloppyDriverInitialize,
  kernelFloppyDriverDescribe,
  kernelFloppyDriverReset,
  kernelFloppyDriverRecalibrate,
  kernelFloppyDriverMotorOn,
  kernelFloppyDriverMotorOff,
  kernelFloppyDriverDiskChanged,
  kernelFloppyDriverReadSectors,
  kernelFloppyDriverWriteSectors,
  kernelFloppyDriverLastErrorCode,
  kernelFloppyDriverLastErrorMessage
};

static kernelDiskDeviceDriver defaultHardDiskDriver =
{
  kernelIdeDriverInitialize,
  NULL,
  kernelIdeDriverReset,
  kernelIdeDriverRecalibrate,
  NULL,
  NULL,
  NULL,
  kernelIdeDriverReadSectors,
  kernelIdeDriverWriteSectors,
  kernelIdeDriverLastErrorCode,
  kernelIdeDriverLastErrorMessage
};

// Default framebuffer graphic driver routines
static kernelGraphicDriver defaultGraphicDriver =
{
  kernelLFBGraphicDriverInitialize,
  kernelLFBGraphicDriverClearScreen,
  kernelLFBGraphicDriverDrawPixel,
  kernelLFBGraphicDriverDrawLine,
  kernelLFBGraphicDriverDrawRect,
  kernelLFBGraphicDriverDrawOval,
  kernelLFBGraphicDriverDrawMonoImage,
  kernelLFBGraphicDriverDrawImage,
  kernelLFBGraphicDriverGetImage,
  kernelLFBGraphicDriverCopyArea,
  kernelLFBGraphicDriverRenderBuffer
};

// A bucket sctructure to hold all the drivers
kernelDriverManager kernelAllDrivers = 
{
  &defaultProcessorDriver,
  &defaultPicDriver,
  &defaultSysTimerDriver,
  &defaultRtcDriver,
  &defaultSerialDriver,
  &defaultDmaDriver,
  &defaultKeyboardDriver,
  &defaultMouseDriver,
  &defaultFloppyDriver,
  &defaultHardDiskDriver,
  &defaultGraphicDriver
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelInstallProcessorDriver(void)
{
  // Install the default Processor Driver
  return (kernelProcessorInstallDriver(kernelAllDrivers.processorDriver));
}

int kernelInstallPicDriver(void)
{
  // Install the default PIC driver
  return (kernelPicInstallDriver(kernelAllDrivers.picDriver));
}

int kernelInstallSysTimerDriver(void)
{
  // Install the default System Timer Driver
  return (kernelSysTimerInstallDriver(kernelAllDrivers.sysTimerDriver));
}

int kernelInstallRtcDriver(void)
{
  // Install the default Real-Time clock driver
  return (kernelRtcInstallDriver(kernelAllDrivers.rtcDriver));
}

int kernelInstallDmaDriver(void)
{
  // Install the default DMA driver
  return (kernelDmaFunctionsInstallDriver(kernelAllDrivers.dmaDriver));
}

int kernelInstallKeyboardDriver(void)
{
  // Install the default keyboard driver
  return(kernelKeyboardInstallDriver(kernelAllDrivers.keyboardDriver));
}

int kernelInstallMouseDriver(void)
{
  // Install the default mouse driver
  return(kernelMouseInstallDriver(kernelAllDrivers.mouseDriver));
}

int kernelInstallFloppyDriver(void)
{
  // Install the default Floppy disk driver

  int status = 0;
  kernelDiskObject *theDisk;
  int count;

  if (systemInfo == NULL)
    {
      kernelError(kernel_error,
	  "Attempt to install floppy drivers before device enumeration");
      return (status = ERR_NOTINITIALIZED);
    }

  for (count = 0; count < systemInfo->floppyDisks; count ++)
    {
      theDisk = kernelGetFloppyDiskObject(count);

      // Put the floppy driver into the kernelDiskObject structures
      status = kernelDiskFunctionsInstallDriver(theDisk, 
						kernelAllDrivers
						.floppyDriver);
      if (status < 0)
	return (status);
    }
  
  return (status = 0);
}

int kernelInstallHardDiskDriver(void)
{
  // Install the hard disk driver

  int status = 0;
  int numDisks = 0;
  kernelDiskObject *theDisk;
  int count;

  numDisks = kernelGetNumberLogicalHardDisks();

  for (count = 0; count < numDisks; count ++)
    {
      theDisk = kernelGetHardDiskObject(count);
      
      // Put the hard disk driver into the kernelDiskObject structures
      status = kernelDiskFunctionsInstallDriver(theDisk, 
						kernelAllDrivers
						.hardDiskDriver);
      if (status < 0)
	return (status);
    }
  
  return (status = 0);
}

int kernelInstallGraphicDriver(void)
{
  // Install the default graphic adapter driver
  return (kernelGraphicInstallDriver(kernelAllDrivers.graphicDriver));
}
