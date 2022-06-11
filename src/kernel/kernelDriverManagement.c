//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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


// Initialization functions for all the kernel's built-in driver.  In no
// particular order, except that the initializations are done in sequence
static void *builtinDriverInits[] = {
  kernelPicDriverInitialize,
  kernelSysTimerDriverInitialize,
  kernelRtcDriverInitialize,
  NULL, // Serial driver
  kernelDmaDriverInitialize,
  kernelKeyboardDriverInitialize,
  kernelPS2MouseDriverInitialize,
  kernelFloppyDriverInitialize,
  kernelIdeDriverInitialize,
  kernelFramebufferGraphicDriverInitialize,
  kernelFilesystemExtInitialize,
  kernelFilesystemFatInitialize,
  kernelFilesystemIsoInitialize,
  kernelTextConsoleInitialize,
  kernelGraphicConsoleInitialize,
  (void *) -1
};

// A structure to hold all the kernel's built-in drivers.
kernelDriverManager kernelAllDrivers = 
{
  NULL, // PIC driver
  NULL, // System timer driver
  NULL, // RTC driver
  NULL, // Serial driver
  NULL, // DMA driver
  NULL, // Keyboard driver
  NULL, // Mouse driver
  NULL, // Floppy driver
  NULL, // IDE driver
  NULL, // Graphics driver
  NULL, // EXT filesystem driver
  NULL, // FAT filesystem driver
  NULL, // ISO filesystem driver
  NULL, // Text-mode console driver
  NULL  // Graphic-mode console driver
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelDriversInitialize(void)
{
  // This function is called during startup so we can call the initialize()
  // functions of all our built-in drivers

  int status = 0;
  int errors = 0;
  int (*driverInit)(void) = NULL;
  int count;

  // Loop through all of the initialization functions we have
  for (count = 0; ; count ++)
    {
      driverInit = builtinDriverInits[count];

      if (driverInit == (void *) -1)
	break;

      if (driverInit == NULL)
	continue;

      // Call the initialization.  The driver should then call 
      // kernelDriverRegister() when it has finished initializing
      status = driverInit();
      if (status < 0)
	errors++;
    }

  if (errors)
    {
      kernelError(kernel_error, "%d errors initializing built-in drivers",
		  errors);
      return (status = ERR_NOTINITIALIZED);
    }
  else
    return (status = 0);
}


int kernelDriverRegister(kernelDriverType type, void *driver)
{
  // This function is called by the drivers during their initialize() call,
  // so that we can add them to the table of known drivers.

  int status = 0;

  if (driver == NULL)
    {
      kernelError(kernel_error, "Driver to register is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  switch (type)
    {
    case picDriver:
      kernelAllDrivers.picDriver = (kernelPicDriver *) driver;
      break;
    case sysTimerDriver:
      kernelAllDrivers.sysTimerDriver = (kernelSysTimerDriver *) driver;
      break;
    case rtcDriver:
      kernelAllDrivers.rtcDriver = (kernelRtcDriver *) driver;
      break;
    case dmaDriver:
      kernelAllDrivers.dmaDriver = (kernelDmaDriver *) driver;
      break;
    case keyboardDriver:
      kernelAllDrivers.keyboardDriver = (kernelKeyboardDriver *) driver;
      break;
    case mouseDriver:
      kernelAllDrivers.mouseDriver = (kernelMouseDriver *) driver;
      break;
    case floppyDriver:
      kernelAllDrivers.floppyDriver = (kernelDiskDriver *) driver;
      break;
    case ideDriver:
      kernelAllDrivers.ideDriver = (kernelDiskDriver *) driver;
      break;
    case graphicDriver:
      kernelAllDrivers.graphicDriver = (kernelGraphicDriver *) driver;
      break;
    case extDriver:
      kernelAllDrivers.extDriver = (kernelFilesystemDriver *) driver;
      break;
    case fatDriver:
      kernelAllDrivers.fatDriver = (kernelFilesystemDriver *) driver;
      break;
    case isoDriver:
      kernelAllDrivers.isoDriver = (kernelFilesystemDriver *) driver;
      break;
    case textConsoleDriver:
      kernelAllDrivers.textConsoleDriver = (kernelTextOutputDriver *) driver;
      break;
    case graphicConsoleDriver:
      kernelAllDrivers.graphicConsoleDriver =
	(kernelTextOutputDriver *) driver;
      break;
    default:
      kernelError(kernel_error, "Unknown driver type %d", type);
      return (status = ERR_NOSUCHENTRY);
    }

  return (status = 0);
}


void kernelInstallPicDriver(kernelPic *pic)
{
  // Install the default PIC driver
  pic->driver = kernelAllDrivers.picDriver;
}


void kernelInstallSysTimerDriver(kernelSysTimer *timer)
{
  // Install the default System Timer Driver
  timer->driver = kernelAllDrivers.sysTimerDriver;
}


void kernelInstallRtcDriver(kernelRtc *rtc)
{
  // Install the default Real-Time clock driver
  rtc->driver = kernelAllDrivers.rtcDriver;
}


void kernelInstallDmaDriver(kernelDma *dma)
{
  // Install the default DMA driver
  dma->driver = kernelAllDrivers.dmaDriver;
}


void kernelInstallKeyboardDriver(kernelKeyboard *keyboard)
{
  // Install the default keyboard driver
  keyboard->driver = kernelAllDrivers.keyboardDriver;
}


void kernelInstallMouseDriver(kernelMouse *mouse)
{
  // Install the default mouse driver
  mouse->driver = kernelAllDrivers.mouseDriver;
}


void kernelInstallFloppyDriver(kernelPhysicalDisk *theDisk)
{
  // Install the default Floppy disk driver
  theDisk->driver = kernelAllDrivers.floppyDriver;
}


void kernelInstallIdeDriver(kernelPhysicalDisk *theDisk)
{
  // Install the hard disk driver
  theDisk->driver = kernelAllDrivers.ideDriver;
}


void kernelInstallGraphicDriver(kernelGraphicAdapter *adapter)
{
  // Install the default graphic adapter driver
  adapter->driver = kernelAllDrivers.graphicDriver;
}


kernelFilesystemDriver *kernelDriverGetExt(void)
{
  // Return the default EXT filesystem driver
  return (kernelAllDrivers.extDriver);
}


kernelFilesystemDriver *kernelDriverGetFat(void)
{
  // Return the default FAT filesystem driver
  return (kernelAllDrivers.fatDriver);
}


kernelFilesystemDriver *kernelDriverGetIso(void)
{
  // Return the default ISO filesystem driver
  return (kernelAllDrivers.isoDriver);
}


kernelTextOutputDriver *kernelDriverGetTextConsole(void)
{
  // Return the default text mode console driver
  return (kernelAllDrivers.textConsoleDriver);
}


kernelTextOutputDriver *kernelDriverGetGraphicConsole(void)
{
  // Return the default graphic mode console driver
  return (kernelAllDrivers.graphicConsoleDriver);
}
