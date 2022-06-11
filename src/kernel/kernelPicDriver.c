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
//  kernelPicDriver.c
//

// Driver for standard Programmable Interrupt Controllers (PIC)

#include "kernelDriverManagement.h" // Contains my prototypes
#include "kernelProcessorX86.h"


int kernelPicDriverRegisterDevice(void *);
void kernelPicDriverEndOfInterrupt(int);

// Our driver structure.
static kernelPicDriver defaultPicDriver =
{
  kernelPicDriverInitialize,
  kernelPicDriverRegisterDevice,
  kernelPicDriverEndOfInterrupt
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelPicDriverRegisterDevice(void *thePic)
{
  // Initialize the PIC controllers

  // The master controller

  // Initialization word 1
  kernelProcessorOutPort8(0x20, 0x11);
  // Initialization word 2
  kernelProcessorOutPort8(0x21, 0x20);
  // Initialization word 3
  kernelProcessorOutPort8(0x21, 0x04);
  // Initialization word 4
  kernelProcessorOutPort8(0x21, 0x01);
  // Normal operation, normal priorities
  kernelProcessorOutPort8(0x20, 0x27);
  // Mask all ints on
  kernelProcessorOutPort8(0x21, 0x00);

  // The slave controller

  // Initialization word 1
  kernelProcessorOutPort8(0xA0, 0x11);
  // Initialization word 2
  kernelProcessorOutPort8(0xA1, 0x28);
  // Initialization word 3
  kernelProcessorOutPort8(0xA1, 0x02);
  // Initialization word 4
  kernelProcessorOutPort8(0xA1, 0x01);
  // Normal operation, normal priorities
  kernelProcessorOutPort8(0xA0, 0x27);
  // Mask all ints on
  kernelProcessorOutPort8(0xA1, 0x00);

  // Return success
  return (0);
}


int kernelPicDriverInitialize(void)
{
   // Register our driver
  kernelDriverRegister(picDriver, &defaultPicDriver);

  // Return success.
  return (0);
}


void kernelPicDriverEndOfInterrupt(int vector)
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

  return;
}
