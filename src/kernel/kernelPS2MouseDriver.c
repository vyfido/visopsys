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
//  kernelPs2MouseDriver.c
//

// Driver for PS2 meeses.

#include "kernelDriver.h" // Contains my prototypes
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelInterrupt.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelMouse.h"
#include "kernelPic.h"
#include "kernelProcessorX86.h"
#include "kernelSysTimer.h"
#include <errno.h>
#include <string.h>

#define MOUSETIMEOUT 0xFFFFFF

typedef enum {
  keyboard_input = 0x01,
  mouse_input = 0x21
} inputType;

typedef struct {
  unsigned char byte1;
  unsigned char byte2;
  unsigned char byte3;

} mousePacket;


static inline int isData(inputType type)
{
  // Return 1 if there's data of the requested type waiting
  
  unsigned char status = 0;

  kernelProcessorInPort8(0x64, status);

  if ((status & type) == type)
    return (1);
  else
    return (0);
}


static int inPort60(unsigned char *data, inputType type, int timeOut)
{
  // Input a value from the keyboard controller's data port, after checking
  // to make sure that there's some data there for us (port 0x60).
  // We maybe can't use the same timeout mechanism here as the functions
  // below, since we need to be able to operate without interrupts enabled.

  int interrupts = 0;
  unsigned startTime = 0;
  int count;

  kernelProcessorIntStatus(interrupts);
  if (interrupts)
    startTime = kernelSysTimerRead();

  for (count = 0; count < MOUSETIMEOUT; count ++)
    {
      if (isData(type))
	{
	  kernelProcessorInPort8(0x60, *data);
	  return (0);
	}

      if (interrupts && (kernelSysTimerRead() >= (startTime + timeOut)))
	break;
    }

  kernelError(kernel_error, "Timeout reading port 60");
  return (ERR_TIMEOUT);
}


static int waitControllerReady(int timeOut)
{
  // Wait for the controller to be ready

  unsigned char status = 0x02;
  unsigned startTime = kernelSysTimerRead();

  while (1)
    {
      kernelProcessorInPort8(0x64, status);

      if (!(status & 0x02))
	return (0);

      if (kernelSysTimerRead() >= (startTime + timeOut))
	break;
    }

  kernelError(kernel_error, "Controller not ready timeout");
  return (ERR_TIMEOUT);
}


static int outPort60(unsigned char data, int timeOut)
{
  // Output a value to the keyboard controller's data port, after checking
  // that it's able to receive data (port 0x60).

  int status = 0;

  status = waitControllerReady(timeOut);
  if (status < 0)
    return (status);

  kernelProcessorOutPort8(0x60, data);

  return (status = 0);
}


static int outPort64(unsigned char data, int timeOut)
{
  // Output a value to the keyboard controller's command port, after checking
  // that it's able to receive data (port 0x64).

  int status = 0;

  status = waitControllerReady(timeOut);
  if (status < 0)
    return (status);

  kernelProcessorOutPort8(0x64, data);

  return (status = 0);
}


static inline int buttonEntropy(mousePacket *packet1, mousePacket *packet2)
{
  int entropy = 0;

  if ((packet1->byte1 & 0x04) != (packet2->byte1 & 0x04))
    entropy += 1;
  if ((packet1->byte1 & 0x02) != (packet2->byte1 & 0x02))
    entropy += 1;
  if ((packet1->byte1 & 0x01) != (packet2->byte1 & 0x01))
    entropy += 1;

  return (entropy);
}


static inline void drain(void)
{
  // Just keep reading the data port until it doesn't think there's any more
  // data for us.

  unsigned char data = 0;
  unsigned drained = 0;

  while (isData(mouse_input) && (inPort60(&data, mouse_input, 0) >= 0))
    drained += 1;

  if (drained)
    kernelDebug(debug_io, "PS2MOUSE: Discarded %u bytes ", drained);
}


static int readPacket(mousePacket *packet)
{
  // Read a standard 3-byte PS/2 mouse packet.

  int status = 0;
  static mousePacket lastPacket;
  static int lastPacketValid = 0;
  int syncRetries = 0;

  // Disable keyboard output here, because our data reads are not atomic
  status = outPort64(0xAD, 20);
  if (status < 0)
    goto out;

  while (1)
    {
      status = inPort60(&packet->byte1, mouse_input, 4);
      if (status < 0)
	goto out;

      // Check to see whether we think this is a valid first byte.

      // First, we know that byte 1, bit 3 is always on..
      if (!(packet->byte1 & 0x08) ||
	  (lastPacketValid && (buttonEntropy(packet, &lastPacket) > 1)))
	{
	  if (isData(mouse_input) && (syncRetries < 4))
	    {
	      syncRetries += 1;
	      continue;
	    }
	  else
	    {
	      kernelDebug(debug_io, "PS2MOUSE: Out of sync");
	      drain();
	      status = ERR_BADDATA;
	      goto out;
	    }
	}
      else
	break;
    }

  // Arbitrary checks here.  If the values are too large we're probably out
  // of synch, so ignore them.

  status = inPort60(&packet->byte2, mouse_input, 4);
  if (status < 0)
    goto out;

  if (((packet->byte1 & 0x10) && (packet->byte2 < 128)) ||
      (!(packet->byte1 & 0x10) && (packet->byte2 > 128)))
    {
      status = ERR_BADDATA;
      goto out;
    }

  status = inPort60(&packet->byte3, mouse_input, 4);
  if (status < 0)
    goto out;

  if (((packet->byte1 & 0x20) && (packet->byte3 < 128)) ||
      (!(packet->byte1 & 0x20) && (packet->byte3 > 128)))
    {
      status = ERR_BADDATA;
      goto out;
    }

  kernelDebug(debug_io, "PS2MOUSE: %02x/%02x/%02x: ", packet->byte1,
	      packet->byte2, packet->byte3);

  kernelMemCopy(packet, &lastPacket, sizeof(mousePacket));
  lastPacketValid = 1;

  status = 0;

 out:

  // Re-enable keyboard output
  outPort64(0xAE, 20);

  return (status);
}


static void readData(void)
{
  // This gets called whenever there is a mouse interrupt

  int status = 0;
  static unsigned char button1 = 0, button2 = 0, button3 = 0;
  mousePacket packet;
  int xChange = 0, yChange = 0;

  while (isData(mouse_input))
    {
      // The first byte contains button information and sign information
      // for the next two bytes.  The second byte is the change in X position,
      // and the third is the change in Y position
      status = readPacket(&packet);
      if (status < 0)
	return;

      if ((packet.byte1 & 0x01) != button1)
	{
	  button1 = (packet.byte1 & 0x01);
	  kernelDebug(debug_io, "PS2MOUSE: Button1 ");
	  kernelMouseButtonChange(1, button1);
	}

      if ((packet.byte1 & 0x04) != button2)
	{
	  button2 = (packet.byte1 & 0x04);
	  kernelDebug(debug_io, "PS2MOUSE: Button2 ");
	  kernelMouseButtonChange(2, button2);
	}

      if ((packet.byte1 & 0x02) != button3)
	{
	  button3 = (packet.byte1 & 0x02);
	  kernelDebug(debug_io, "PS2MOUSE: Button3 ");
	  kernelMouseButtonChange(3, button3);
	}

      if (packet.byte2 ||  packet.byte3)
	{
	  // Sign them
	  if (packet.byte1 & 0x10)
	    xChange = (int) ((256 - packet.byte2) * -1);
	  else
	    xChange = (int) packet.byte2;

	  if (packet.byte1 & 0x20)
	    yChange = (int) (256 - packet.byte3);
	  else
	    yChange = (int) (packet.byte3 * -1);

	  kernelDebug(debug_io, "PS2MOUSE: Move (%d,%d) ", xChange, yChange);
	  kernelMouseMove(xChange, yChange);
	}
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


static int command(unsigned char cmd, int numParams, unsigned char *inParams,
		   unsigned char *outParams, int timeOut)
{
  // Send a mouse command to the keyboard controller

  int status = 0;
  unsigned char data = 0;
  int count;

  kernelDebug(debug_io, "PS2MOUSE: Mouse command %02x... ", cmd);

  // Mouse command...
  status = outPort64(0xD4, timeOut);
  if (status < 0)
    {
      kernelError(kernel_error, "Error writing command");
      return (status);
    }
  kernelDebug(debug_io, "PS2MOUSE: MC, ");

  // Send command
  status = outPort60(cmd, timeOut);
  if (status < 0)
    {
      kernelError(kernel_error, "Error writing command");
      return (status);
    }
  kernelDebug(debug_io, "PS2MOUSE: cmd, ");

  // Read the ack 0xFA
  status = inPort60(&data, mouse_input, timeOut);
  if (status < 0)
    {
      kernelError(kernel_error, "Error reading ack");
      return (status);
    }
  if (data != 0xFA)
    {
      kernelDebug(debug_io, "PS2MOUSE: No command ack, response=%02x", data);
      return (status = ERR_IO);
    }
  kernelDebug(debug_io, "PS2MOUSE: ack, ");

  // Now, if there are parameters to this command...
  for (count = 0; count < numParams; count ++)
    {
      if (inParams)
	{
	  status = inPort60(&data, mouse_input, timeOut);
	  if (status < 0)
	    {
	      kernelError(kernel_error, "Error reading command parameter %d",
			  count);
	      return (status);
	    }
	  
	  inParams[count] = data;
	  kernelDebug(debug_io, "PS2MOUSE: p%d=%02x, ", count, data);
	}

      else if (outParams)
	{
	  status = outPort60(outParams[count], timeOut);
	  if (status < 0)
	    {
	      kernelError(kernel_error, "Error writing parameter %d", count);
	      return (status);
	    }
	  kernelDebug(debug_io, "PS2MOUSE: p%d=%02x, ", count,
		      outParams[count]);

	  /*
	  // Read the ack 0xFA
	  status = inPort60(&data, mouse_input, timeOut);
	  if (status < 0)
	    {
	      kernelError(kernel_error, "Error reading ack");
	      return (status);
	    }
	  if (data != 0xFA)
	    {
	      kernelDebug(debug_io, "PS2MOUSE: no ack data=%02x", data);
	      return (status = ERR_IO);
	    }
	  kernelDebug(debug_io, "PS2MOUSE: ack, ");
	  */
	}
    }

  kernelDebug(debug_io, "PS2MOUSE: done");
  return (status = 0);
}


static int initialize(void)
{
  int status = 0;
  unsigned char data[2];
  int retries = 0;

  kernelDebug(debug_io, "PS2MOUSE: Mouse intialize");

  for (retries = 0; retries < 5; retries ++)
    { 
      // Send the reset command
      if (command(0xFF, 2, data, NULL, 20) < 0)
	continue;

      // Should be 'self test passed' 0xAA and device ID 0 for normal
      // PS/2 mouse
      if ((data[0] != 0xAA) || (data[1] != 0))
	continue;

      /*
      // Set sample rate (decimal 10);
      data[0] = 10;
      if (command(0xF3, 1, NULL, data, 20) < 0)
	continue;

      // Read device type.
      if (command(0xF2, 1, data, NULL, 20) < 0)
	continue;
      
      // Should be type 0.
      if (data[0] != 0)
	continue;

      // Set resolution.  100 dpi, 4 counts/mm, decimal 2.
      data[0] = 2;
      if (command(0xE8, 1, NULL, data, 20) < 0)
	continue;
      */

      // Set scaling to 2:1
      if (command(0xE7, 0, NULL, NULL, 20) < 0)
	continue;

      /*
      // Set sample rate (decimal 40);
      data[0] = 40;
      if (command(0xF3, 1, NULL, data, 20) < 0)
	continue;
      */

      // Set stream mode.
      if (command(0xEA, 0, NULL, NULL, 20) < 0)
	continue;

      // Enable data reporting.
      if (command(0xF4, 0, NULL, NULL, 20) < 0)
	continue;

      // Success
      break;
    }

  if (retries < 5)
    return (status = 0);
  else
    return (status = ERR_IO);
}


static int driverDetect(void *parent, kernelDriver *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.  Also talks to
  // the keyboard controller a little bit to initialize the mouse

  int status = 0;
  unsigned char data = 0;
  kernelDevice *dev = NULL;
  int interrupts = 0;

  // Do the hardware initialization.

  // Disable keyboard output here, because our data reads are not atomic
  status = outPort64(0xAD, 20);
  if (status < 0)
    goto exit;

  // Enable the mouse port
  status = outPort64(0xA8, 20);
  if (status < 0)
    goto exit;

  status = initialize();
  if (status < 0)
    {
      // Perhaps there is no mouse
      status = 0;
      goto exit;
    }

  kernelProcessorSuspendInts(interrupts);

  // Don't worry about the timeout values for these 'inPort' and 'outPort'
  // commands whilst interrupts are disabled -- as long as the value is non-
  // zero it will not time out (no sys timer interrupt).

  // Tell the controller to issue mouse interrupts
  kernelDebug(debug_io, "PS2MOUSE: Turn on mouse interrupts...");
  outPort64(0x20, 1);
  inPort60(&data, keyboard_input, 1);
  data |= 0x02;
  outPort64(0x60, 1);
  outPort60(data, 1);
  kernelDebug(debug_io, "PS2MOUSE: done");

  kernelProcessorRestoreInts(interrupts);

  // Re-enable keyboard output
  outPort64(0xAE, 20);

  // Register our interrupt handler
  status = kernelInterruptHook(INTERRUPT_NUM_MOUSE, &mouseInterrupt);
  if (status < 0)
    goto exit;

  // Turn on the interrupt
  kernelPicMask(INTERRUPT_NUM_MOUSE, 1);

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    {
      status = ERR_MEMORY;
      goto exit;
    }

  dev->device.class = kernelDeviceGetClass(DEVICECLASS_MOUSE);
  dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_MOUSE_PS2);
  dev->driver = driver;

  // Add the device
  status = kernelDeviceAdd(parent, dev);
  if (status < 0)
    goto exit;

  kernelDebug(debug_io, "PS2MOUSE: Successfully detected mouse");
  status = 0;

exit:

  // Re-enable keyboard output
  outPort64(0xAE, 20);

  if (status < 0)
    {
      kernelDebug(debug_io, "PS2MOUSE: Error %d detecting mouse", status);
      if (dev)
	kernelFree(dev);
    }

  return (status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelPs2MouseDriverRegister(kernelDriver *driver)
{
   // Device driver registration.

  driver->driverDetect = driverDetect;

  return;
}
