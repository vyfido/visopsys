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
//  kernelPicDriver.c
//

// Driver for standard Programmable Interrupt Controllers (PIC)

#include "kernelDriver.h" // Contains my prototypes
#include "kernelPic.h"
#include "kernelInterrupt.h"
#include "kernelMalloc.h"
#include "kernelProcessorX86.h"
#include "kernelError.h"
#include <string.h>


static int driverEndOfInterrupt(int intNumber)
{
  // Sends end of interrupt (EOI) commands to one or both of the PICs.
  // Our parameter should be the number of the interrupt.  If the number
  // is greater than 7, we will issue EOI to both the slave and master
  // controllers.  Otherwise, just the master.

  if (intNumber > 0x07)
    // Issue an end-of-interrupt (EOI) to the slave PIC
    kernelProcessorOutPort8(0xA0, 0x20);

  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  return (0);
}


static int driverMask(int intNumber, int on)
{
  // This masks or unmasks an interrupt.  Our parameters should be the number
  // of the interrupt vector, and an on/off value.

  unsigned char data = 0;

  if (intNumber <= 0x07)
    {
      intNumber = (0x01 << intNumber);

      // Get the current mask value
      kernelProcessorInPort8(0x21, data);

      // An enabled interrupt has its mask bit off
      if (on)
	data &= ~intNumber;
      else
	data |= intNumber;

      kernelProcessorOutPort8(0x21, data);
    }
  else
    {
      intNumber = (0x01 << (intNumber - 0x08));

      // Get the current mask value
      kernelProcessorInPort8(0xA1, data);

      // An enabled interrupt has its mask bit off
      if (on)
	data &= ~intNumber;
      else
	data |= intNumber;

      kernelProcessorOutPort8(0xA1, data);
    }

  return (0);
}


static int driverGetActive(void)
{
  // Returns the number of the active interrupt

  unsigned char data = 0;
  int intNumber = 0;
  
  // First ask the master pic
  kernelProcessorOutPort8(0x20, 0x0B);
  kernelProcessorInPort8(0x20, data);
  
  while (!((data >> intNumber) & 1))
    intNumber += 1;
  
  // Is it actually the slave PIC?
  if (intNumber == 2)
    {
      // Ask the slave PIC which interrupt
      kernelProcessorOutPort8(0xA0, 0x0B);
      kernelProcessorInPort8(0xA0, data);

      intNumber = 8;
      while (!((data >> (intNumber - 8)) & 1))
	intNumber += 1;
    }

  return (intNumber);
}


static int driverDetect(void *driver)
{
  // Normally, this routine is used to detect and initialize each device,
  // as well as registering each one with any higher-level interfaces.  Since
  // we can assume that there's a PIC, just initialize it.

  int status = 0;
  kernelDevice *device = NULL;

  // Allocate memory for the device
  device = kernelMalloc(sizeof(kernelDevice));
  if (device == NULL)
    return (status = 0);

  device->class = kernelDeviceGetClass(DEVICECLASS_PIC);
  device->driver = driver;

  // Initialization byte 1
  kernelProcessorOutPort8(0x20, 0x11);
  // Initialization byte 2
  kernelProcessorOutPort8(0x21, 0x20);
  // Initialization byte 3
  kernelProcessorOutPort8(0x21, 0x04);
  // Initialization byte 4
  kernelProcessorOutPort8(0x21, 0x01);
  // Normal operation, normal priorities
  kernelProcessorOutPort8(0x20, 0x27);
  // Mask all ints off initially, except for 2 (the slave controller)
  kernelProcessorOutPort8(0x21, 0xFB);

  // The slave controller

  // Initialization byte 1
  kernelProcessorOutPort8(0xA0, 0x11);
  // Initialization byte 2
  kernelProcessorOutPort8(0xA1, 0x28);
  // Initialization byte 3
  kernelProcessorOutPort8(0xA1, 0x02);
  // Initialization byte 4
  kernelProcessorOutPort8(0xA1, 0x01);
  // Normal operation, normal priorities
  kernelProcessorOutPort8(0xA0, 0x27);
  // Mask all ints off initially
  kernelProcessorOutPort8(0xA1, 0xFF);

  // Initialize PIC operations
  status = kernelPicInitialize(device);
  if (status < 0)
    {
      kernelFree(device);
      return (status);
    }

  return (status = kernelDeviceAdd(NULL, device));
}


static kernelPicOps picOps = {
  driverEndOfInterrupt,
  driverMask,
  driverGetActive
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelPicDriverRegister(void *driverData)
{
   // Device driver registration.

  kernelDriver *driver = (kernelDriver *) driverData;

  driver->driverDetect = driverDetect;
  driver->ops = &picOps;

  return;
}
