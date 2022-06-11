//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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

// Driver for PS2 meeses.

#include "kernelDriver.h" // Contains my prototypes
#include "kernelMouse.h"
#include "kernelGraphic.h"
#include "kernelInterrupt.h"
#include "kernelMalloc.h"
#include "kernelPic.h"
#include "kernelProcessorX86.h"
#include <string.h>

#define MOUSETIMEOUT 0xFFFF


static unsigned char inPort60(void)
{
  // Input a value from the keyboard controller's data port, after checking
  // to make sure that there's some data there for us

  unsigned char data = 0;

  while (!(data & 0x01))
    kernelProcessorInPort8(0x64, data);

  kernelProcessorInPort8(0x60, data);
  return (data);
}


static void waitControllerReady(void)
{
  // Wait for the controller to be ready

  unsigned char data = 0x02;

  while (data & 0x02)
    kernelProcessorInPort8(0x64, data);
}


static void outPort60(unsigned char value)
{
  // Output a value to the keyboard controller's data port, after checking
  // to make sure it's ready for the data

  waitControllerReady();
  kernelProcessorOutPort8(0x60, value);
  return;
}


static void outPort64(unsigned char value)
{
  // Output a value to the keyboard controller's command port, after checking
  // to make sure it's ready for the command

  waitControllerReady();
  kernelProcessorOutPort8(0x64, value);
  return;
}


static int getMouseData(unsigned char *byte1, unsigned char *byte2,
			unsigned char *byte3)
{
  // Input a value from the keyboard controller's data port, after checking
  // to make sure that there's some mouse data there for us

  int status = 0;
  unsigned char data = 0;
  int count;

  // Qemu can often time out, so we need to check for it.
  for (count = 0; (!(data & 0x01) && (count < MOUSETIMEOUT)); count ++)
    kernelProcessorInPort8(0x64, data);
  if (count >= MOUSETIMEOUT)
    return (status = ERR_NODATA);

  kernelProcessorInPort8(0x60, data);
  *byte1 = data;

  data = 0;
  for (count = 0; (!(data & 0x01) && (count < MOUSETIMEOUT)); count ++)
    kernelProcessorInPort8(0x64, data);
  if (count >= MOUSETIMEOUT)
    return (status = ERR_NODATA);

  kernelProcessorInPort8(0x60, data);
  *byte2 = data;

  data = 0;
  for (count = 0; (!(data & 0x01) && (count < MOUSETIMEOUT)); count ++)
    kernelProcessorInPort8(0x64, data);
  if (count >= MOUSETIMEOUT)
    return (status = ERR_NODATA);

  kernelProcessorInPort8(0x60, data);
  *byte3 = data;

  return (status = 0);
}


static void readData(void)
{
  // This gets called whenever there is a mouse interrupt

  int status = 0;
  static volatile int button1 = 0, button2 = 0, button3 = 0;
  unsigned char byte1 = 0, byte2 = 0, byte3 = 0;
  int xChange, yChange;

  // Disable keyboard output here, because our data reads are not atomic
  outPort64(0xAD);

  // The first byte contains button information and sign information
  // for the next two bytes.  The second byte is the change in X position,
  // and the third is the change in Y position
  status = getMouseData(&byte1, &byte2, &byte3);

  // Re-enable keyboard output
  outPort64(0xAE);

  if (status < 0)
    {
      // Send resend command
      outPort64(0xD4);
      outPort60(0xFE);
      return;
    }

  if ((byte1 & 0x01) != button1)
    {
      button1 = (byte1 & 0x01);
      kernelMouseButtonChange(1, button1);
    }
  else if ((byte1 & 0x04) != button2)
    {
      button2 = (byte1 & 0x04);
      kernelMouseButtonChange(2, button2);
    }
  else if ((byte1 & 0x02) != button3)
    {
      button3 = (byte1 & 0x02);
      kernelMouseButtonChange(3, button3);
    }
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


static void mouseInterrupt(void)
{
  // This is the mouse interrupt handler.  It calls the mouse driver
  // to actually read data from the device.

  void *address = NULL;

  kernelProcessorIsrEnter(address);
  kernelProcessingInterrupt = 1;

  // Call the routine to read the data
  readData();

  kernelPicEndOfInterrupt(INTERRUPT_NUM_MOUSE);
  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit(address);
}


static int driverDetect(void *parent, kernelDriver *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.  Also talks to
  // the keyboard controller a little bit to initialize the mouse

  int status = 0;
  kernelDevice *dev = NULL;
  int interrupts = 0;
  unsigned char response = 0;
  unsigned char deviceId = 0;
  int count;

  // Do the hardware initialization.

  kernelProcessorSuspendInts(interrupts);

  // Disable keyboard output here, because our data reads are not atomic
  outPort64(0xAD);

  for (count = 0; count < 2; count ++)
    {
      // Send reset command
      outPort64(0xD4);
      outPort60(0xFF);

      // Read the ack 0xFA
      response = inPort60();
      if (response != 0xFA)
	goto exit;

      // Should be 'self test passed' 0xAA
      response = inPort60();
      if (response != 0xAA)
	goto exit;

      // Get the device ID.  0x00 for normal PS/2 mouse
      deviceId = inPort60();
      if (deviceId != 0)
	goto exit;
    }

  // Set scaling to 2:1
  outPort64(0xD4);
  outPort60(0xE7);

  // Read the ack 0xFA
  response = inPort60();
  if (response != 0xFA)
    goto exit;

  // Tell the controller to issue mouse interrupts
  outPort64(0x20);
  response = inPort60();
  response |= 0x02;
  outPort64(0x60);
  outPort60(response);

  // Enable data reporting (stream mode)
  outPort64(0xD4);
  outPort60(0xF4);

  // Read the ack 0xFA
  response = inPort60();
  if (response != 0xFA)
    goto exit;

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    goto exit;

  dev->device.class = kernelDeviceGetClass(DEVICECLASS_MOUSE);
  dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_MOUSE_PS2);
  dev->driver = driver;

  // Add the device
  status = kernelDeviceAdd(parent, dev);
  if (status < 0)
    {
      kernelFree(dev);
      goto exit;
    }

  // Register our interrupt handler
  status = kernelInterruptHook(INTERRUPT_NUM_MOUSE, &mouseInterrupt);
  if (status < 0)
    {
      kernelFree(dev);
      goto exit;
    }

  // Turn on the interrupt
  kernelPicMask(INTERRUPT_NUM_MOUSE, 1);

exit:
  // Re-enable keyboard output
  outPort64(0xAE);

  kernelProcessorRestoreInts(interrupts);

  return (status = 0);
}


static kernelMouseOps mouseOps = {
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelPS2MouseDriverRegister(kernelDriver *driver)
{
   // Device driver registration.

  driver->driverDetect = driverDetect;
  driver->ops = &mouseOps;

  return;
}
