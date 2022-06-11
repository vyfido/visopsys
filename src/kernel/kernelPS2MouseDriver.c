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
//  kernelPS2MouseDriver.c
//

// Driver for PS2 mouses.

#include "kernelDriverManagement.h" // Contains my prototypes
#include "kernelProcessorX86.h"


void kernelPS2MouseDriverReadData(void);

static kernelMouseDriver defaultMouseDriver =
{
  kernelPS2MouseDriverInitialize,
  NULL, // driverRegisterDevice
  kernelPS2MouseDriverReadData
};

static int initialized = 0;


static inline unsigned inPort60(void)
{
  // Input a value from the keyboard controller's data port, after checking
  // to make sure that there's some data there for us

  unsigned char data = 0;

  while (!(data & 0x01))
    kernelProcessorInPort8(0x64, data);

  kernelProcessorInPort8(0x60, data);
  return ((unsigned) data);
}


static inline void outPort60(unsigned value)
{
  // Output a value to the keyboard controller's data port, after checking
  // to make sure it's ready for the data

  unsigned char data;
  
  // Wait for the controller to be ready
  data = 0x02;
  while (data & 0x02)
    kernelProcessorInPort8(0x64, data);
  
  data = (unsigned char) value;
  kernelProcessorOutPort8(0x60, data);
  return;
}


static inline void outPort64(unsigned value)
{
  // Output a value to the keyboard controller's command port, after checking
  // to make sure it's ready for the command

  unsigned char data;
  
  // Wait for the controller to be ready
  data = 0x02;
  while (data & 0x02)
    kernelProcessorInPort8(0x64, data);

  data = (unsigned char) value;
  kernelProcessorOutPort8(0x64, data);
  return;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelPS2MouseDriverInitialize(void)
{
  // Talk to the keyboard controller a little bit to initialize the mouse

  int interrupts = 0;
  unsigned char response;
  unsigned char deviceId;
  int count;

  // Initialize the mouse.

  kernelProcessorSuspendInts(interrupts);

  for (count = 0; count < 10; count ++)
    {
      // Disable the mouse line
      outPort64(0xA7);
      
      // Enable the mouse line
      outPort64(0xA8);
      
      // Disable data reporting
      outPort64(0xD4);
      outPort60(0xF5);

      // Read the ack
      response = inPort60();
      if (response != 0xFA)
	continue;

      // Send reset command
      outPort64(0xD4);
      outPort60(0xFF);

      // Read the ack 0xFA
      response = inPort60();
      if (response != 0xFA)
	continue;

      // Should be 'self test passed' 0xAA
      response = inPort60();
      if (response != 0xAA)
	continue;

      // Get the device ID.  0x00 for normal PS/2 mouse
      deviceId = inPort60();

      // Set scaling to 2:1
      outPort64(0xD4);
      outPort60(0xE7);

      // Read the ack
      response = inPort60();
      if (response != 0xFA)
	continue;

      // Tell the controller to issue mouse interrupts
      outPort64(0x20);
      response = inPort60();
      response |= 0x02;
      outPort64(0x60);
      outPort60(response);

      // Enable data reporting (stream mode)
      outPort64(0xD4);
      outPort60(0xF4);

      // Read the ack
      response = inPort60();
      if (response != 0xFA)
	continue;

      // All set
      break;
    }

  kernelProcessorRestoreInts(interrupts);

  initialized = 1;
  return (kernelDriverRegister(mouseDriver, &defaultMouseDriver));
}


void kernelPS2MouseDriverReadData(void)
{
  // This gets called whenever there is a mouse interrupt

  static volatile int button1, button2, button3;
  unsigned char byte1, byte2, byte3;
  int xChange, yChange;

  // The first byte contains button information and sign information
  // for the next two bytes
  byte1 = inPort60();

  // The change in X position
  byte2 = inPort60();

  // The change in Y position
  byte3 = inPort60();

  if ((byte1 & 0x01) != button1)
    kernelMouseButtonChange(1, button1 = (byte1 & 0x01));
  else if ((byte1 & 0x04) != button2)
    kernelMouseButtonChange(2, button2 = (byte1 & 0x04));
  else if ((byte1 & 0x02) != button3)
    kernelMouseButtonChange(3, button3 = (byte1 & 0x02));

  else
    {
      // Sign them
      if (byte1 & 0x10)
	xChange = (int) ((256 - byte2) * -1);
      else
	xChange = (int) byte2;

      if (byte1 & 0x20)
	yChange = (int) (256 - byte3);
      else
	yChange = (int) (byte3 * -1);

      kernelMouseMove(xChange, yChange);
    }

  return;
}
