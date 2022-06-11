//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  kernelKeyboardDriver.c
//

// Driver for standard PC keyboards

#include "kernelDriver.h" // Contains my prototypes
#include "kernelKeyboard.h"
#include "kernelProcessorX86.h"
#include "kernelMultitasker.h"
#include "kernelMalloc.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelWindow.h"
#include "kernelShutdown.h"
#include "kernelFile.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include <string.h>
#include <stdio.h>
#include <sys/window.h>
#include <sys/stream.h>

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
#define F3_KEY           61
#define PAGEUP_KEY       73
#define PAGEDOWN_KEY     81
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

static void rebootThread(void) __attribute__((noreturn));

static kernelKeyboard *keyboardDevice = NULL;


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


static void driverReadData(void)
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
	  keyboardDevice->flags &= ~SHIFT_FLAG;
	  return;
	case (KEY_RELEASE + LEFTCTRL_KEY):
	  // Left control release.
	  keyboardDevice->flags &= ~CONTROL_FLAG;
	  return;
	case (KEY_RELEASE + ALT_KEY):
	  if (extended)
	    // Right Alt release.
	    keyboardDevice->flags &= ~ALTGR_FLAG;
	  else
	    // Left Alt release.
	    keyboardDevice->flags &= ~ALT_FLAG;
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
	  keyboardDevice->flags |= SHIFT_FLAG;
	  return;
	case LEFTCTRL_KEY:
	  // Left control press.
	  keyboardDevice->flags |= CONTROL_FLAG;
	  return;
	case ALT_KEY:
	  if (extended)
	    // Right alt press.
	    keyboardDevice->flags |= ALTGR_FLAG;
	  else
	    // Left alt press.
	    keyboardDevice->flags |= ALT_FLAG;
	  return;
	case CAPSLOCK_KEY:
	  if (keyboardDevice->flags & CAPSLOCK_FLAG)
	    // Capslock off
	    keyboardDevice->flags ^= CAPSLOCK_FLAG;
	  else
	    // Capslock on
	    keyboardDevice->flags |= CAPSLOCK_FLAG;
	  setLight(CAPSLOCK_LIGHT, (keyboardDevice->flags & CAPSLOCK_FLAG));
	  return;
	case NUMLOCK_KEY:
	  if (keyboardDevice->flags & NUMLOCK_FLAG)
	    // Numlock off
	    keyboardDevice->flags ^= NUMLOCK_FLAG;
	  else
	    // Numlock on
	    keyboardDevice->flags |= NUMLOCK_FLAG;
	  setLight(NUMLOCK_LIGHT, (keyboardDevice->flags & NUMLOCK_FLAG));
	  return;
	case SCROLLLOCK_KEY:
	  if (keyboardDevice->flags & SCROLLLOCK_FLAG)
	    // Scroll lock off
	    keyboardDevice->flags ^= SCROLLLOCK_FLAG;
	  else
	    // Scroll lock on
	    keyboardDevice->flags |= SCROLLLOCK_FLAG;
	  setLight(SCROLLLOCK_LIGHT,
		   (keyboardDevice->flags & SCROLLLOCK_FLAG));
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
  if (!extended && ((keyboardDevice->flags & SHIFT_FLAG) ||
		    ((keyboardDevice->flags & NUMLOCK_FLAG) &&
		     (data >= 0x47) && (data <= 0x53))))
    data = keyboardDevice->keyMap->shiftMap[data - 1];
  
  else if (keyboardDevice->flags & CONTROL_FLAG)
    {
      // CTRL-ALT-DEL?
      if ((keyboardDevice->flags & ALT_FLAG) && (data == DEL_KEY) && release)
	{
	  // CTRL-ALT-DEL means reboot
	  kernelMultitaskerSpawn(rebootThread, "reboot", 0, NULL);
	  return;
	}
      else
	data = keyboardDevice->keyMap->controlMap[data - 1];
    }

  else if (keyboardDevice->flags & ALTGR_FLAG)
    data = keyboardDevice->keyMap->altGrMap[data - 1];
  
  else
    data = keyboardDevice->keyMap->regMap[data - 1];
      
  // If capslock is on, uppercase any alphabetic characters
  if ((keyboardDevice->flags & CAPSLOCK_FLAG) &&
      ((data >= 'a') && (data <= 'z')))
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


static int driverDetect(void *parent, kernelDriver *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.  Also issues the
  // appropriate commands to the keyboard controller to set keyboard settings.

  int status = 0;
  kernelDevice *dev = NULL;
  void *biosData = NULL;
  unsigned char data;

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice) + sizeof(kernelKeyboard));
  if (dev == NULL)
    return (status = 0);

  keyboardDevice = ((void *) dev + sizeof(kernelDevice));

  dev->device.class = kernelDeviceGetClass(DEVICECLASS_KEYBOARD);
  dev->driver = driver;
  dev->data = keyboardDevice;

  // Map the BIOS data area into our memory so we can get hardware information
  // from it.
  status = kernelPageMapToFree(KERNELPROCID, (void *) 0, &biosData, 0x1000);
  if (status < 0)
    {
      kernelError(kernel_error, "Error mapping BIOS data area");
      return (status);
    }

  // Get the flags from the BIOS data area
  keyboardDevice->flags = (unsigned) *((unsigned char *)(biosData + 0x417));

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
  status = kernelKeyboardInitialize(dev);
  if (status < 0)
    {
      kernelFree(dev);
      return (status);
    }

  // Set the default keyboard data stream to be the console input
  status =
    kernelKeyboardSetStream(&(kernelTextGetConsoleInput()->s));
  if (status < 0)
    {
      kernelFree(dev);
      return (status);
    }

  return (status = kernelDeviceAdd(parent, dev));
}


static kernelKeyboardOps keyboardOps = {
  driverReadData
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelKeyboardDriverRegister(kernelDriver *driver)
{
   // Device driver registration.

  driver->driverDetect = driverDetect;
  driver->ops = &keyboardOps;

  return;
}
