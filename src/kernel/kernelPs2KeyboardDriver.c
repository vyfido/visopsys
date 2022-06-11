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
//  kernelPs2KeyboardDriver.c
//

// Driver for standard PS/2 PC keyboards

#include "kernelDriver.h" // Contains my prototypes
#include "kernelError.h"
#include "kernelFile.h"
#include "kernelInterrupt.h"
#include "kernelKeyboard.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelPic.h"
#include "kernelProcessorX86.h"
#include "kernelShutdown.h"
#include "kernelWindow.h"
#include <stdio.h>
#include <string.h>

// Some special scan values that we care about
#define KEY_RELEASE      128
#define EXTENDED         224
#define LEFTSHIFT_KEY    42
#define RIGHTSHIFT_KEY   54
#define LEFTCTRL_KEY     29
#define ALT_KEY          56
#define ASTERISK_KEY     55
#define F1_KEY           59
#define F2_KEY           60
#define DEL_KEY          83
#define CAPSLOCK_KEY     58
#define NUMLOCK_KEY      69
#define SCROLLLOCK_KEY   70

#define ALTGR_FLAG       0x0100
#define INSERT_FLAG      0x0080
#define CAPSLOCK_FLAG    0x0040
#define NUMLOCK_FLAG     0x0020
#define SCROLLLOCK_FLAG  0x0010
#define ALT_FLAG         0x0008
#define CONTROL_FLAG     0x0004
#define SHIFT_FLAG       0x0003

#define SCROLLLOCK_LIGHT 0
#define NUMLOCK_LIGHT    1
#define CAPSLOCK_LIGHT   2

static unsigned flags;


static void setLight(int whichLight, int onOff)
{
  // Turns the keyboard lights on/off

  unsigned char data = 0;
  static unsigned char lights = 0x00;

  // Wait for port 60h to be ready for a command.  We know it's
  // ready when port 0x64 bit 1 is 0
  data = 0x02;
  while (data & 0x02)
    kernelProcessorInPort8(0x64, data);

  // Tell the keyboard we want to change the light status
  kernelProcessorOutPort8(0x60, 0xED);

  if (onOff)
    lights |= (0x01 << whichLight);
  else
    lights &= (0xFF ^ (0x01 << whichLight));

  // Wait for port 60h to be ready for a command.  We know it's
  // ready when port 0x64 bit 1 is 0
  data = 0x02;
  while (data & 0x02)
    kernelProcessorInPort8(0x64, data);

  // Tell the keyboard to change the lights
  kernelProcessorOutPort8(0x60, lights);

  // Read the ACK
  data = 0;
  while (!(data & 0x01))
    kernelProcessorInPort8(0x64, data);
  kernelProcessorInPort8(0x60, data);
  
  return;
}


__attribute__((noreturn))
static void rebootThread(void)
{
  // This gets called when the user presses CTRL-ALT-DEL.

  // Reboot, force
  kernelShutdown(1, 1);
  while(1);
}


static void screenshotThread(void)
{
  // This gets called when the user presses the 'print screen' key

  char fileName[32];
  file theFile;
  int count = 1;

  // Determine the file name we want to use

  strcpy(fileName, "/screenshot1.bmp");

  // Loop until we get a filename that doesn't already exist
  while (!kernelFileFind(fileName, &theFile))
    {
      count += 1;
      sprintf(fileName, "/screenshot%d.bmp", count);
    }

  kernelMultitaskerTerminate(kernelWindowSaveScreenShot(fileName));
}


static void readData(void)
{
  // This routine reads the keyboard data and returns it to the keyboard
  // console text input stream

  unsigned char data = 0;
  int release = 0;
  static int extended = 0;

  // Wait for keyboard data to be available
  while (!(data & 0x01))
    kernelProcessorInPort8(0x64, data);

  // Read the data from port 60h
  kernelProcessorInPort8(0x60, data);

  // If an extended scan code is coming next...
  if (data == EXTENDED)
    {
      // The next thing coming is an extended scan code.  Set the flag
      // so it can be collected next time
      extended = 1;
      return;
    }

  // Key press or key release?
  if (data >= KEY_RELEASE)
    {
      // This is a key release.  We only care about a couple of cases if
      // it's a key release.

      switch (data)
	{
	case (KEY_RELEASE + LEFTSHIFT_KEY):
	case (KEY_RELEASE + RIGHTSHIFT_KEY):
	  // Left or right shift release.
	  flags &= ~SHIFT_FLAG;
	  return;
	case (KEY_RELEASE + LEFTCTRL_KEY):
	  // Left control release.
	  flags &= ~CONTROL_FLAG;
	  return;
	case (KEY_RELEASE + ALT_KEY):
	  if (extended)
	    // Right Alt release.
	    flags &= ~ALTGR_FLAG;
	  else
	    // Left Alt release.
	    flags &= ~ALT_FLAG;
	  return;
	default:
	  data -= KEY_RELEASE;
	  release = 1;
	  break;
	}
    }

  else
    {
      // Regular key.

      switch (data)
	{
	case LEFTSHIFT_KEY:
	case RIGHTSHIFT_KEY:
	  // Left shift or right shift press.
	  flags |= SHIFT_FLAG;
	  return;
	case LEFTCTRL_KEY:
	  // Left control press.
	  flags |= CONTROL_FLAG;
	  return;
	case ALT_KEY:
	  if (extended)
	    // Right alt press.
	    flags |= ALTGR_FLAG;
	  else
	    // Left alt press.
	    flags |= ALT_FLAG;
	  return;
	case CAPSLOCK_KEY:
	  if (flags & CAPSLOCK_FLAG)
	    // Capslock off
	    flags ^= CAPSLOCK_FLAG;
	  else
	    // Capslock on
	    flags |= CAPSLOCK_FLAG;
	  setLight(CAPSLOCK_LIGHT, (flags & CAPSLOCK_FLAG));
	  return;
	case NUMLOCK_KEY:
	  if (flags & NUMLOCK_FLAG)
	    // Numlock off
	    flags ^= NUMLOCK_FLAG;
	  else
	    // Numlock on
	    flags |= NUMLOCK_FLAG;
	  setLight(NUMLOCK_LIGHT, (flags & NUMLOCK_FLAG));
	  return;
	case SCROLLLOCK_KEY:
	  if (flags & SCROLLLOCK_FLAG)
	    // Scroll lock off
	    flags ^= SCROLLLOCK_FLAG;
	  else
	    // Scroll lock on
	    flags |= SCROLLLOCK_FLAG;
	  setLight(SCROLLLOCK_LIGHT, (flags & SCROLLLOCK_FLAG));
	  return;
	case F1_KEY:
	  kernelConsoleLogin();
	  return;
	case F2_KEY:
	  kernelMultitaskerDumpProcessList();
	  return;
	default:
	  break;
	}
    }

  // If this is an 'extended' asterisk (*), we probably have a 'print screen'
  // or 'sys req'.
  if (extended && (data == ASTERISK_KEY) && !release)
    {
      kernelMultitaskerSpawn(screenshotThread, "screenshot", 0, NULL);
      // Clear the extended flag
      extended = 0;
      return;
    }

  // Check whether the control or shift keys are pressed.  Shift
  // overrides control.
  if (!extended && ((flags & SHIFT_FLAG) ||
		    ((flags & NUMLOCK_FLAG) &&
		     (data >= 0x47) && (data <= 0x53))))
    data = kernelKeyMap->shiftMap[data - 1];
  
  else if (flags & CONTROL_FLAG)
    {
      // CTRL-ALT-DEL?
      if ((flags & ALT_FLAG) && (data == DEL_KEY) && release)
	{
	  // CTRL-ALT-DEL means reboot
	  kernelMultitaskerSpawn(rebootThread, "reboot", 0, NULL);
	  return;
	}
      else
	data = kernelKeyMap->controlMap[data - 1];
    }

  else if (flags & ALTGR_FLAG)
    data = kernelKeyMap->altGrMap[data - 1];
  
  else
    data = kernelKeyMap->regMap[data - 1];
      
  // If capslock is on, uppercase any alphabetic characters
  if ((flags & CAPSLOCK_FLAG) && ((data >= 'a') && (data <= 'z')))
    data -= 32;
  
  // Notify the keyboard function of the event
  if (release)
    kernelKeyboardInput((int) data, EVENT_KEY_UP);
  else
    kernelKeyboardInput((int) data, EVENT_KEY_DOWN);
  
  // Clear the extended flag
  extended = 0;
  return;
}


static void interrupt(void)
{
  // This is the PS/2 interrupt handler.

  void *address = NULL;

  kernelProcessorIsrEnter(address);
  kernelProcessingInterrupt = 1;

  readData();

  kernelPicEndOfInterrupt(INTERRUPT_NUM_KEYBOARD);
  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit(address);
}


static int driverDetect(void *parent, kernelDriver *driver)
{
  // This routine is used to detect a PS/2 keyboard and initialize it, as
  // well as registering it with the higher-level device functions.

  int status = 0;
  kernelDevice *dev = NULL;
  void *biosData = NULL;
  unsigned char data;

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    return (status = ERR_MEMORY);

  dev->device.class = kernelDeviceGetClass(DEVICECLASS_KEYBOARD);
  dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_KEYBOARD_PS2);
  dev->driver = driver;

  // Map the BIOS data area into our memory so we can get hardware information
  // from it.
  status = kernelPageMapToFree(KERNELPROCID, (void *) 0, &biosData, 0x1000);
  if (status < 0)
    goto out;

  // Get the flags from the BIOS data area
  flags = (unsigned) *((unsigned char *)(biosData + 0x417));

  // Unmap BIOS data
  kernelPageUnmap(KERNELPROCID, biosData, 0x1000);

  // Wait for port 64h to be ready for a command.  We know it's ready when
  // port 64 bit 1 is 0
  data = 0x02;
  while (data & 0x02)
    kernelProcessorInPort8(0x64, data);

  // Tell the keyboard to enable
  kernelProcessorOutPort8(0x64, 0xAE);

  // Initialize keyboard operations
  status = kernelKeyboardInitialize();
  if (status < 0)
    goto out;

  // Register our interrupt handler
  status = kernelInterruptHook(INTERRUPT_NUM_KEYBOARD, &interrupt);
  if (status < 0)
    goto out;

  // Turn on the interrupt
  kernelPicMask(INTERRUPT_NUM_KEYBOARD, 1);

  status = kernelDeviceAdd(parent, dev);
  if (status < 0)
    goto out;

  status = 0;

 out:
  if ((status < 0) && dev)
    kernelFree(dev);

  return (status);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelPs2KeyboardDriverRegister(kernelDriver *driver)
{
  // Device driver registration.

  driver->driverDetect = driverDetect;

  return;
}
