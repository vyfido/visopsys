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
//  kernelDriverManagement.c
//

#include "kernelDriverManagement.h"
#include "kernelHardwareEnumeration.h"
#include "kernelMiscAsmFunctions.h"
#include "kernelError.h"
#include <string.h>
#include <sys/errors.h>


// A bucket sctructure to hold all the drivers
static kernelDriverManager kernelAllDrivers;

// Static driver structures
static kernelProcessorDriver defaultProcessorDriver;
static kernelPicDeviceDriver defaultPicDriver;
static kernelSysTimerDriver defaultSysTimerDriver;
static kernelRtcDeviceDriver defaultRtcDriver;
static kernelDmaDeviceDriver defaultDmaDriver;
static kernelKeyboardDriver defaultKeyboardDriver;
static kernelDiskDeviceDriver defaultFloppyDriver;
static kernelDiskDeviceDriver defaultHardDiskDriver;


int kernelInstallProcessorDriver(void)
{
  // This function knows about all of the default device driver for the
  // indicated device, and will install it.  It is called by the 
  // kernelDriverManagementInitialize(void) routine, and is not exported
  // to the rest of the kernel.  It takes no arguments and returns
  // an integer as a status indicator.  0 means success, negative otherwise.

  int status = 0;


  // Install the default Processor Driver

  // Initialize the space we've reserved for the default kernelProcessorDriver
  // structure we're using
  kernelMemClear(&defaultProcessorDriver, 
			sizeof(kernelProcessorDriver));
  // Put the processor driver into the kernelAllDrivers structure
  kernelAllDrivers.processorDriver = &defaultProcessorDriver;

  kernelAllDrivers.processorDriver->driverInitialize = DEFAULTPROCINIT;
  kernelAllDrivers.processorDriver->driverReadTimestamp = DEFAULTPROCRDTSC;

  kernelProcessorInstallDriver(kernelAllDrivers.processorDriver);

  status = kernelAllDrivers.processorDriver->driverInitialize();
  
  return (status);
}


int kernelInstallPicDriver(void)
{
  // This function knows about all of the default device driver for the
  // indicated device, and will install it.  It is called by the 
  // kernelDriverManagementInitialize(void) routine, and is not exported
  // to the rest of the kernel.  It takes no arguments and returns
  // an integer as a status indicator.  0 means success, negative otherwise.

  int status = 0;


  // Install the default PIC driver

  // Initialize the space we've reserved for the default kernelPicDeviceDriver
  // structure we're using
  kernelMemClear(&defaultPicDriver, sizeof(kernelPicDeviceDriver));
  // Put the PIC driver into the kernelAllDrivers structure
  kernelAllDrivers.picDriver = &defaultPicDriver;

  kernelAllDrivers.picDriver->driverInitialize = DEFAULTPICINITIALIZE;
  kernelAllDrivers.picDriver->driverEndOfInterrupt = DEFAULTPICEOI;

  kernelPicInstallDriver(kernelAllDrivers.picDriver);
  
  status = kernelAllDrivers.picDriver->driverInitialize();

  return (status);
}


int kernelInstallSysTimerDriver(void)
{
  // This function knows about all of the default device driver for the
  // indicated device, and will install it.  It is called by the 
  // kernelDriverManagementInitialize(void) routine, and is not exported
  // to the rest of the kernel.  It takes no arguments and returns
  // an integer as a status indicator.  0 means success, negative otherwise.

  int status = 0;


  // Install the default System Timer Driver

  // Initialize the space we've reserved for the default kernelSysTimerDriver
  // structure we're using
  kernelMemClear(&defaultSysTimerDriver, sizeof(kernelSysTimerDriver));
  // Put the system timer driver into the kernelAllDrivers structure
  kernelAllDrivers.sysTimerDriver = &defaultSysTimerDriver;

  kernelAllDrivers.sysTimerDriver->driverInitialize = DEFAULTSYSTIMERINIT;
  kernelAllDrivers.sysTimerDriver->driverTimerTick = DEFAULTSYSTIMERTICK;
  kernelAllDrivers.sysTimerDriver->driverReadTicks = DEFAULTSYSTIMERREADTICKS;
  kernelAllDrivers.sysTimerDriver->driverReadValue = DEFAULTSYSTIMERREADVALUE;
  kernelAllDrivers.sysTimerDriver->driverSetupTimer = 
    DEFAULTSYSTIMERSETUPTIMER;

  kernelSysTimerInstallDriver(kernelAllDrivers.sysTimerDriver);

  status = kernelAllDrivers.sysTimerDriver->driverInitialize();

  return (status);
}


int kernelInstallRtcDriver(void)
{
  // This function knows about all of the default device driver for the
  // indicated device, and will install it.  It is called by the 
  // kernelDriverManagementInitialize(void) routine, and is not exported
  // to the rest of the kernel.  It takes no arguments and returns
  // an integer as a status indicator.  0 means success, negative otherwise.

  int status = 0;


  // Install the default Real-Time clock driver

  // Initialize the space we've reserved for the default kernelRtcDeviceDriver
  // structure we're using
  kernelMemClear(&defaultRtcDriver, sizeof(kernelRtcDeviceDriver));
  // Put the RTC driver into the kernelAllDrivers structure
  kernelAllDrivers.rtcDriver = &defaultRtcDriver;

  kernelAllDrivers.rtcDriver->driverInitialize = DEFAULTRTCINITIALIZE;
  kernelAllDrivers.rtcDriver->driverReadSeconds = DEFAULTRTCSECONDS;
  kernelAllDrivers.rtcDriver->driverReadMinutes = DEFAULTRTCMINUTES;
  kernelAllDrivers.rtcDriver->driverReadHours = DEFAULTRTCHOURS;
  kernelAllDrivers.rtcDriver->driverReadDayOfWeek = DEFAULTRTCDAYOFWEEK;
  kernelAllDrivers.rtcDriver->driverReadDayOfMonth = DEFAULTRTCDAYOFMONTH;
  kernelAllDrivers.rtcDriver->driverReadMonth = DEFAULTRTCMONTH;
  kernelAllDrivers.rtcDriver->driverReadYear = DEFAULTRTCYEAR;

  kernelRtcInstallDriver(kernelAllDrivers.rtcDriver);
  
  status = kernelAllDrivers.rtcDriver->driverInitialize();

  return (status);
}


int kernelInstallDmaDriver(void)
{
  // This function knows about all of the default device driver for the
  // indicated device, and will install it.  It is called by the 
  // kernelDriverManagementInitialize(void) routine, and is not exported
  // to the rest of the kernel.  It takes no arguments and returns
  // an integer as a status indicator.  0 means success, negative otherwise.

  int status = 0;

  // Install the default DMA driver

  // Initialize the space we've reserved for the default kernelDmaDriver
  // structure we're using
  kernelMemClear(&defaultDmaDriver, sizeof(kernelDmaDeviceDriver));
  // Put the DMA driver into the kernelAllDrivers structure
  kernelAllDrivers.dmaDriver = &defaultDmaDriver;

  kernelAllDrivers.dmaDriver->driverInitialize = DEFAULTDMAINIT;
  kernelAllDrivers.dmaDriver->driverSetupChannel = DEFAULTDMASETUPCHANNEL;
  kernelAllDrivers.dmaDriver->driverSetMode = DEFAULTDMASETMODE;
  kernelAllDrivers.dmaDriver->driverEnableChannel = DEFAULTDMAENABLECHANNEL;
  kernelAllDrivers.dmaDriver->driverCloseChannel = DEFAULTDMACLOSECHANNEL;

  kernelDmaFunctionsInstallDriver(kernelAllDrivers.dmaDriver);

  status = kernelAllDrivers.dmaDriver->driverInitialize();

  return (status);
}


int kernelInstallKeyboardDriver(void)
{
  // This function knows about all of the default device driver for the
  // indicated device, and will install it.  It is called by the 
  // kernelDriverManagementInitialize(void) routine, and is not exported
  // to the rest of the kernel.  It takes no arguments and returns
  // an integer as a status indicator.  0 means success, negative otherwise.

  int status = 0;

  // Install the default keyboard driver

  // Initialize the space we've reserved for the default kernelKeyboardDriver
  // structure we're using
  kernelMemClear(&defaultKeyboardDriver, sizeof(kernelKeyboardDriver));
  // Put the keyboard driver into the kernelAllDrivers structure
  kernelAllDrivers.keyboardDriver = &defaultKeyboardDriver;

  kernelAllDrivers.keyboardDriver->driverInitialize = DEFAULTKBRDINIT;
  kernelAllDrivers.keyboardDriver->driverReadData = DEFAULTKBRDREADDATA;

  kernelKeyboardInstallDriver(kernelAllDrivers.keyboardDriver);

  return (status);
}


int kernelInstallFloppyDriver(void)
{
  // This function knows about all of the default device driver for the
  // indicated device, and will install it.  It is called by the 
  // kernelDriverManagementInitialize(void) routine, and is not exported
  // to the rest of the kernel.  It takes no arguments and returns
  // an integer as a status indicator.  0 means success, negative otherwise.

  int status = 0;
  int numDisks = 0;
  int count;

  // Temp variable for installing drivers
  kernelDiskObject *theDisk;


  // Install the default Floppy disk driver

  // Initialize the space we've reserved for the default
  // kernelDiskDeviceDriver structure we're using
  kernelMemClear(&defaultFloppyDriver, sizeof(kernelDiskDeviceDriver));

  kernelAllDrivers.floppyDriver = &defaultFloppyDriver;

  // Add the required driver routines to the kernelDiskDeviceDriver
  // structure.  These are already pointers (see header)
  kernelAllDrivers.floppyDriver->driverInitialize = DEFAULTFLOPPYINIT;
  kernelAllDrivers.floppyDriver->driverDescribe = DEFAULTFLOPPYDESCRIBE;
  kernelAllDrivers.floppyDriver->driverReset = DEFAULTFLOPPYRESET;
  kernelAllDrivers.floppyDriver->driverRecalibrate = DEFAULTFLOPPYRECALIBRATE;
  kernelAllDrivers.floppyDriver->driverMotorOn = DEFAULTFLOPPYMOTORON;
  kernelAllDrivers.floppyDriver->driverMotorOff = DEFAULTFLOPPYMOTOROFF;
  kernelAllDrivers.floppyDriver->driverDiskChanged = DEFAULTFLOPPYDISKCHANGED;
  kernelAllDrivers.floppyDriver->driverReadSectors = DEFAULTFLOPPYREAD;
  kernelAllDrivers.floppyDriver->driverWriteSectors = DEFAULTFLOPPYWRITE;
  kernelAllDrivers.floppyDriver->driverLastErrorCode = DEFAULTFLOPPYERRCODE;
  kernelAllDrivers.floppyDriver->driverLastErrorMessage =DEFAULTFLOPPYERRMESS;
  
  // Install the floppy disk driver objects

  if (systemInfo == NULL)
    {
      kernelError(kernel_error,
	  "Attempt to install floppy drivers before device enumeration");
      return (status = ERR_NOTINITIALIZED);
    }

  numDisks = systemInfo->floppyDisks;

  for (count = 0; count < numDisks; count ++)
    {
      theDisk = kernelGetFloppyDiskObject(count);

      // Put the floppy driver into the kernelDiskObject structures
      kernelDiskFunctionsInstallDriver(theDisk, 
				       kernelAllDrivers.floppyDriver);
    }
  
  status = kernelAllDrivers.floppyDriver->driverInitialize();

  return (status);
}


int kernelInstallHardDiskDriver(void)
{
  // This function knows about all of the default device driver for the
  // indicated device, and will install it.  It is called by the 
  // kernelDriverManagementInitialize(void) routine, and is not exported
  // to the rest of the kernel.  It takes no arguments and returns
  // an integer as a status indicator.  0 means success, negative otherwise.

  int status = 0;
  int numDisks = 0;
  int count;

  // Temp variable for installing drivers
  kernelDiskObject *theDisk;


  // Install the hard disk driver

  // Initialize the space we've reserved for the default
  // kernelDiskDeviceDriver structure we're using
  kernelMemClear(&defaultHardDiskDriver, 
			sizeof(kernelDiskDeviceDriver));

  kernelAllDrivers.hardDiskDriver = &defaultHardDiskDriver;

  // Add the required driver routines to the kernelDiskDeviceDriver
  // structure.  These are already pointers (see header)
  kernelAllDrivers.hardDiskDriver->driverInitialize = DEFAULTHDDINIT;
  kernelAllDrivers.hardDiskDriver->driverDescribe = DEFAULTHDDESCRIBE;
  kernelAllDrivers.hardDiskDriver->driverReset = DEFAULTHDDRESET;
  kernelAllDrivers.hardDiskDriver->driverRecalibrate = DEFAULTHDDRECALIBRATE;
  kernelAllDrivers.hardDiskDriver->driverMotorOn = DEFAULTHDDMOTORON;
  kernelAllDrivers.hardDiskDriver->driverMotorOff = DEFAULTHDDMOTOROFF;
  kernelAllDrivers.hardDiskDriver->driverDiskChanged = DEFAULTHDDDISKCHANGED;
  kernelAllDrivers.hardDiskDriver->driverReadSectors = DEFAULTHDDREAD;
  kernelAllDrivers.hardDiskDriver->driverWriteSectors = DEFAULTHDDWRITE;
  kernelAllDrivers.hardDiskDriver->driverLastErrorCode = DEFAULTHDDERRCODE;
  kernelAllDrivers.hardDiskDriver->driverLastErrorMessage = DEFAULTHDDERRMESS;
  
  // Install the hard disk drivers

  numDisks = kernelGetNumberLogicalHardDisks();
  for (count = 0; count < numDisks; count ++)
    {
      theDisk = kernelGetHardDiskObject(count);
      
      // Put the hard disk driver into the kernelDiskObject structures
      kernelDiskFunctionsInstallDriver(theDisk, 
				       kernelAllDrivers.hardDiskDriver);
    }
  
  status = kernelAllDrivers.hardDiskDriver->driverInitialize();

  return (status);
}


int kernelDriverManagementInitialize(void)
{
  // This routine is used to initialize the driver management functions,
  // install the default drivers, etc.  It takes no arguments and returns
  // an integer as a status indicator.  0 means success, negative otherwise.

  int status = 0;

  // Put some meat into the structure (initialize it)
  kernelMemClear(&kernelAllDrivers, sizeof(kernelDriverManager));

  // That's all we do.  The hardware enumeration routines will call 
  // each of the functions above to initialize the drivers of the 
  // devices in order.
  
  return (status = 0);
}
