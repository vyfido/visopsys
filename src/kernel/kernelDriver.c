//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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

#include "kernelDriver.h"
#include "kernelText.h"
#include "kernelFilesystemExt.h"
#include "kernelError.h"
#include <string.h>

// Arrays of the kernel's built-in (non-device) drivers.  In no particular
// order, except that the initializations are done in sequence

static void *textDriverInits[] = {
  kernelTextConsoleInitialize,
  kernelGraphicConsoleInitialize,
  (void *) -1
};

static void *filesystemDriverInits[] = {
  kernelFilesystemExtInitialize,
  kernelFilesystemFatInitialize,
  kernelFilesystemIsoInitialize,
  (void *) -1
};

// A structure to hold all the kernel's built-in (non-device) drivers.
static struct {
  kernelFilesystemDriver *extDriver;
  kernelFilesystemDriver *fatDriver;
  kernelFilesystemDriver *isoDriver;
  kernelTextOutputDriver *textConsoleDriver;
  kernelTextOutputDriver *graphicConsoleDriver;

} allDrivers = {
  NULL, // EXT filesystem driver
  NULL, // FAT filesystem driver
  NULL, // ISO filesystem driver
  NULL, // Text-mode console driver
  NULL  // Graphic-mode console driver
};


static int driversInitialize(void *initArray[])
{
  // This function calls the driver initialize() functions of the supplied

  int status = 0;
  int errors = 0;
  int (*driverInit)(void) = NULL;
  int count;

  // Loop through all of the initialization functions we have
  for (count = 0; ; count ++)
    {
      driverInit = initArray[count];

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
    return (status = ERR_NOTINITIALIZED);
  else
    return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelTextDriversInitialize(void)
{
  // This function is called during startup so we can call the initialize()
  // functions of the text drivers
  return (driversInitialize(textDriverInits));
}


int kernelFilesystemDriversInitialize(void)
{
  // This function is called during startup so we can call the initialize()
  // functions of the filesystem drivers
  return (driversInitialize(filesystemDriverInits));
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
    case extDriver:
      allDrivers.extDriver = (kernelFilesystemDriver *) driver;
      break;
    case fatDriver:
      allDrivers.fatDriver = (kernelFilesystemDriver *) driver;
      break;
    case isoDriver:
      allDrivers.isoDriver = (kernelFilesystemDriver *) driver;
      break;
    case textConsoleDriver:
      allDrivers.textConsoleDriver = (kernelTextOutputDriver *) driver;
      break;
    case graphicConsoleDriver:
      allDrivers.graphicConsoleDriver = (kernelTextOutputDriver *) driver;
      break;
    default:
      kernelError(kernel_error, "Unknown driver type %d", type);
      return (status = ERR_NOSUCHENTRY);
    }

  return (status = 0);
}


void *kernelDriverGetExt(void)
{
  // Return the EXT filesystem driver
  return (allDrivers.extDriver);
}


void *kernelDriverGetFat(void)
{
  // Return the FAT filesystem driver
  return (allDrivers.fatDriver);
}


void *kernelDriverGetIso(void)
{
  // Return the ISO filesystem driver
  return (allDrivers.isoDriver);
}


void *kernelDriverGetTextConsole(void)
{
  // Return the text mode console driver
  return (allDrivers.textConsoleDriver);
}


void *kernelDriverGetGraphicConsole(void)
{
  // Return the graphic mode console driver
  return (allDrivers.graphicConsoleDriver);
}