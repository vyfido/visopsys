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

#include "kernelDriverManagement.h" // Contains my prototypes
#include "kernelInterrupt.h"
#include "kernelProcessorX86.h"
#include "kernelError.h"


static int driverRegisterDevice(void *thePic)
{
  // Initialize the PIC master controller

  // We ignore the PIC argument.  This keeps the compiler happy
  if (thePic == NULL)
    return (ERR_NULLPARAMETER);

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
  // Mask all ints offinitially
  kernelProcessorOutPort8(0xA1, 0xFF);

  // Return success
  return (0);
}


static int driverEndOfInterrupt(int vector)
{
  // Sends end of interrupt (EOI) commands to one or both of the PICs.
  // Our parameter should be the number of the interrupt vector.  If
  // The number is greater than 0x27, we will issue EOI to both the
  // slave and master controllers.  Otherwise, just the master.

  if (vector > 0x27)
    // Issue an end-of-interrupt (EOI) to the slave PIC
    kernelProcessorOutPort8(0xA0, 0x20);

  // Issue an end-of-interrupt (EOI) to the master PIC
  kernelProcessorOutPort8(0x20, 0x20);

  return (0);
}


static int driverMask(int vector, int on)
{
  // This masks or unmasks an interrupt.  Our parameters should be the number
  // of the interrupt vector, and an on/off value.

  unsigned char data = 0;

  if (vector < INTERRUPT_VECTOR)
    // Illegal
    return (ERR_INVALID);

  vector -= INTERRUPT_VECTOR;

  if (vector <= 0x07)
    {
      vector = (0x01 << vector);

      // Get the current mask value
      kernelProcessorInPort8(0x21, data);

      // An enabled interrupt has its mask bit off
      if (on)
	data &= ~vector;
      else
	data |= vector;

      kernelProcessorOutPort8(0x21, data);
    }
  else
    {
      vector -= 0x08;
      vector = (0x01 << vector);

      // Get the current mask value
      kernelProcessorInPort8(0xA1, data);

      // An enabled interrupt has its mask bit off
      if (on)
	data &= ~vector;
      else
	data |= vector;

      kernelProcessorOutPort8(0xA1, data);
    }

  return (0);
}


// Our driver structure.
static kernelPicDriver defaultPicDriver =
{
  kernelPicDriverInitialize,
  driverRegisterDevice,
  driverEndOfInterrupt,
  driverMask
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelPicDriverInitialize(void)
{
   // Register our driver
  kernelDriverRegister(picDriver, &defaultPicDriver);

  // Return success.
  return (0);
}
