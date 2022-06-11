//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  kernelUsbHidDriver.c
//

// Driver for USB HIDs (human interface devices) such as keyboards and meeses.

#include "kernelDriver.h" // Contains my prototypes
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelKeyboard.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelMouse.h"
#include "kernelUsbHidDriver.h"
#include <stdlib.h>
#include <string.h>
#include <sys/window.h>

// Flags for keyboard state
#define INSERT_FLAG       0x0800
#define CAPSLOCK_FLAG     0x0400
#define SCROLLLOCK_FLAG   0x0200
#define NUMLOCK_FLAG      0x0100
#define MODIFIER_FLAGS    0x00FF
#define ALTGR_FLAG        0x0040
#define ALT_FLAG          0x0044
#define SHIFT_FLAG        0x0022
#define CONTROL_FLAG      0x0011

// Some particular (USB) scan codes we're interested in
#define TAB_KEY           43
#define CAPSLOCK_KEY      57
#define F1_KEY            58
#define F2_KEY            59
#define PRINTSCREEN_KEY   70
#define SCROLLLOCK_KEY    71
#define DEL_KEY           76
#define NUMLOCK_KEY       83

// A map of USB keyboard codes to PC keyboard scan codes.
static unsigned char usbKeyCodes[232] = {
  0x00, 0x00, 0x00, 0x00, 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21,  // 0-9
  0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19,  // 10-19
  0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,  // 20-29
  0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,  // 30-39
  0x1C, 0x01, 0x0E, 0x0F, 0x39, 0x0C, 0x0D, 0x1A, 0x1B, 0x2B,  // 40-49
  0x2B, 0x27, 0x28, 0x29, 0x33, 0x34, 0x35, 0x3A, 0x3B, 0x3C,  // 50-59
  0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58,  // 60-69
  0x37, 0x46, 0xE1, 0x52, 0x47, 0x49, 0x53, 0x4F, 0x51, 0x4D,  // 70-79
  0x4B, 0x50, 0x48, 0x45, 0x35, 0x37, 0x4A, 0x4E, 0x1C, 0x4F,  // 80-89
  0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49, 0x52, 0x71,  // 90-99
  0x2B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 100-109
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 110-119
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 120,129
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 130-139
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 140-149
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 150-159
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 160-169
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 170-179
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 180-189
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 190-199
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 200-209
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 210-219
  0x00, 0x00, 0x00, 0x00, 0x14, 0x12, 0x00, 0x14, 0x36, 0x11,  // 220-229
  0x11, 0x00                                                   // 230-231
};

#ifdef DEBUG
static void debugHidDesc(usbHidDesc *hidDesc)
{
  kernelDebug(debug_usb, "Debug HID descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%x\n"
	      "    hidVersion=%d.%d\n"
	      "    countryCode=%d\n"
	      "    numDescriptors=%d\n"
	      "    repDescType=%d\n"
	      "    repDescLength=%d", hidDesc->descLength, hidDesc->descType,
	      ((hidDesc->hidVersion & 0xFF00) >> 8),
	      (hidDesc->hidVersion & 0xFF), hidDesc->countryCode,
	      hidDesc->numDescriptors, hidDesc->repDescType,
	      hidDesc->repDescLength);
}

#else
  #define debugHidDesc(hidDesc) do { } while (0)
#endif // DEBUG


static int getHidDescriptor(hidDevice *hidDev)
{
  usbTransaction usbTrans;

  kernelDebug(debug_usb, "Get HID descriptor for target %d, interface %d",
	      hidDev->target, hidDev->interNum);

  // Set up the USB transaction to send the 'get descriptor' command
  kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
  usbTrans.type = usbxfer_control;
  usbTrans.address = hidDev->usbDev->address;
  usbTrans.control.requestType = USB_DEVREQTYPE_INTERFACE;
  usbTrans.control.request = USB_GET_DESCRIPTOR;
  usbTrans.control.value = (USB_DESCTYPE_HID << 8);
  usbTrans.control.index = hidDev->interNum;
  usbTrans.length = 8;
  usbTrans.buffer = &hidDev->hidDesc;

  // Write the command
  return (kernelBusWrite(bus_usb, hidDev->target, sizeof(usbTransaction),
			 (void *) &usbTrans));
}


static int setBootProtocol(hidDevice *hidDev)
{
  usbTransaction usbTrans;

  kernelDebug(debug_usb, "Set HID boot protocol for target %d, interface %d",
	      hidDev->target, hidDev->interNum);

  // Tell the device to use the simple (boot) protocol.
  kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
  usbTrans.type = usbxfer_control;
  usbTrans.address = hidDev->usbDev->address;
  usbTrans.control.requestType =
    (USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_INTERFACE);
  usbTrans.control.request = USB_HID_SET_PROTOCOL;
  usbTrans.control.index = hidDev->interNum;
  usbTrans.pid = USB_PID_OUT;

  // Write the command
  return (kernelBusWrite(bus_usb, hidDev->target, sizeof(usbTransaction),
			 (void *) &usbTrans));
}


static void lockKey(hidDevice *hidDev, unsigned char usbCode)
{
  // This function is called in response to the user pressing one of the
  // keyboard 'lock' keys; i.e. "caps lock", "scroll lock", and "num lock".
  // The flag will be toggled and we'll send a command to the keyboard to
  // set or clear the light.

  usbTransaction usbTrans;
  unsigned char report = 0;

  if (usbCode == CAPSLOCK_KEY)
    hidDev->keyboardFlags ^= CAPSLOCK_FLAG;
  else if (usbCode == SCROLLLOCK_KEY)
    hidDev->keyboardFlags ^= SCROLLLOCK_FLAG;
  else if (usbCode == NUMLOCK_KEY)
    hidDev->keyboardFlags ^= NUMLOCK_FLAG;

  if (hidDev->keyboardFlags & CAPSLOCK_FLAG)
    report |= 2;
  if (hidDev->keyboardFlags & SCROLLLOCK_FLAG)
    report |= 4;
  if (hidDev->keyboardFlags & NUMLOCK_FLAG)
    report |= 1;

  kernelDebug(debug_usb, "Set HID report %02x for target %d, interface %d",
	      report, hidDev->target, hidDev->interNum);

  // Send a "set report" command to the keyboard with the LED status.
  kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
  usbTrans.type = usbxfer_control;
  usbTrans.address = hidDev->usbDev->address;
  usbTrans.control.requestType =
    (USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_INTERFACE);
  usbTrans.control.request = USB_HID_SET_REPORT;
  usbTrans.control.value = 2;
  usbTrans.control.index = hidDev->interNum;
  usbTrans.length = 1;
  usbTrans.buffer = &report;
  usbTrans.pid = USB_PID_OUT;

  // Write the command
  kernelBusWrite(bus_usb, hidDev->target, sizeof(usbTransaction),
		 (void *) &usbTrans);
}


static unsigned char usbCodeToAscii(hidDevice *hidDev, unsigned char usbCode)
{
  // Convert a USB code to an ASCII value.  The value depends on the keyboard
  // flags as well as the particular code.

  unsigned char scanCode = 0;
  unsigned char ascii = 0;

  // If numlock is on, certain scan codes turn into digits
  if (hidDev->keyboardFlags & NUMLOCK_FLAG)
    {
      if ((usbCode >= 89) && (usbCode <= 98))
	usbCode -= 59;
      else if (usbCode == 99)
	usbCode = 55;
    }

  scanCode = usbKeyCodes[usbCode];

  if (hidDev->keyboardFlags & ALTGR_FLAG)
    ascii = kernelKeyMap->altGrMap[scanCode - 1];
  else if (hidDev->keyboardFlags & CONTROL_FLAG)
    ascii = kernelKeyMap->controlMap[scanCode - 1];
  else if (hidDev->keyboardFlags & SHIFT_FLAG)
    ascii = kernelKeyMap->shiftMap[scanCode - 1];
  else
    ascii = kernelKeyMap->regMap[scanCode - 1];

  // If capslock is on, uppercase any alphabetic characters
  if ((hidDev->keyboardFlags & CAPSLOCK_FLAG) &&
      ((ascii >= 'a') && (ascii <= 'z')))
    ascii -= 32;
  
  return (ascii);
}


static void interrupt(usbDevice *usbDev, void *buffer, unsigned length)
{
  hidDevice *hidDev = usbDev->data;
  usbHidKeyboardData *keyboardData = NULL;
  usbHidMouseData *mouseData = NULL;
  unsigned char ascii = 0;
  int count;

  //kernelDebug(debug_usb, "USB HID interrupt %u bytes", length);

  if (hidDev->type == hid_keyboard)
    {
      keyboardData = buffer;

      /*
      kernelDebug(debug_usb, "USB keyboard mod=%02x codes %02x %02x %02x %02x "
		  "%02x %02x", keyboardData->modifier, keyboardData->code[0],
		  keyboardData->code[1], keyboardData->code[2],
		  keyboardData->code[3], keyboardData->code[4],
		  keyboardData->code[5]);
      */

      // Check for ALT presses and releases in the modifier flags
      if (!(hidDev->keyboardFlags & ALT_FLAG) &&
	  (keyboardData->modifier & ALT_FLAG))
	// ALT was pressed
	kernelKeyboardSpecial(keyboardEvent_altPress);
      else if ((hidDev->keyboardFlags & ALT_FLAG) &&
	       !(keyboardData->modifier & ALT_FLAG))
	// ALT was released
	kernelKeyboardSpecial(keyboardEvent_altRelease);

      // Set the new modifier flags
      hidDev->keyboardFlags =
	((hidDev->keyboardFlags & ~MODIFIER_FLAGS) | keyboardData->modifier);

      // Find key releases
      for (count = 0; count < 5; count ++)
	{
	  if ((hidDev->oldKeyboardData.code[count] < 4) ||
	      (hidDev->oldKeyboardData.code[count] > 231))
	    break;

	  if (strchr((const char *) keyboardData->code,
		     hidDev->oldKeyboardData.code[count]))
	    continue;

	  if ((hidDev->oldKeyboardData.code[count] == DEL_KEY) &&
	      (hidDev->keyboardFlags & CONTROL_FLAG) &&
	      (hidDev->keyboardFlags & ALT_FLAG))
	    {
	      // CTRL-ALT-DEL means reboot
	      kernelKeyboardSpecial(keyboardEvent_ctrlAltDel);
	    }
	  else
	    {
	      // Normal key releases
	      ascii =
		usbCodeToAscii(hidDev, hidDev->oldKeyboardData.code[count]);
	      kernelKeyboardInput(ascii, EVENT_KEY_UP);
	    }
	}

      // Find new keypresses
      for (count = 0; count < 5; count ++)
	{
	  if ((keyboardData->code[count] < 4) ||
	      (keyboardData->code[count] > 231))
	    break;

	  if (strchr((const char *) hidDev->oldKeyboardData.code,
		     keyboardData->code[count]))
	    continue;

	  switch (keyboardData->code[count])
	    {
	      // Check for some special cases
	    case F1_KEY:
	      kernelKeyboardSpecial(keyboardEvent_f1);
	      break;

	    case F2_KEY:
	      kernelKeyboardSpecial(keyboardEvent_f2);
	      break;

	    case PRINTSCREEN_KEY:
	      kernelKeyboardSpecial(keyboardEvent_printScreen);
	      break;

	    case TAB_KEY:
	      if (keyboardData->modifier & ALT_FLAG)
		kernelKeyboardSpecial(keyboardEvent_altTab);
	      break;

	    case CAPSLOCK_KEY:
	    case SCROLLLOCK_KEY:
	    case NUMLOCK_KEY:
	      lockKey(hidDev, keyboardData->code[count]);
	      break;

	    default:
	      // Normal key presses
	      ascii = usbCodeToAscii(hidDev, keyboardData->code[count]);
	      kernelKeyboardInput(ascii, EVENT_KEY_DOWN);
	      break;
	    }
	}

      if (length < sizeof(usbHidKeyboardData))
	kernelMemClear(&hidDev->oldKeyboardData, sizeof(usbHidKeyboardData));

      kernelMemCopy(keyboardData, &hidDev->oldKeyboardData,
		    min(length, sizeof(usbHidKeyboardData)));
    }

  else if (hidDev->type == hid_mouse)
    {
      if (length < sizeof(usbHidMouseData))
	return;

      mouseData = buffer;

      kernelDebug(debug_usb, "USB mouse buttons=%02x xChange=%d "
		  "yChange=%d", mouseData->buttons, mouseData->xChange,
		  mouseData->yChange);

      if (mouseData->buttons != hidDev->oldMouseButtons)
	{
	  // Look for changes in mouse button states

	  // Left button; button 1
	  if ((mouseData->buttons & USB_HID_MOUSE_LEFTBUTTON) !=
	      (hidDev->oldMouseButtons & USB_HID_MOUSE_LEFTBUTTON))
	    kernelMouseButtonChange(1, (mouseData->buttons &
					USB_HID_MOUSE_LEFTBUTTON));

	  // Middle button; button 2
	  if ((mouseData->buttons & USB_HID_MOUSE_MIDDLEBUTTON) !=
	      (hidDev->oldMouseButtons & USB_HID_MOUSE_MIDDLEBUTTON))
	    kernelMouseButtonChange(2, (mouseData->buttons &
					USB_HID_MOUSE_MIDDLEBUTTON));

	  // Right button; button 3
	  if ((mouseData->buttons & USB_HID_MOUSE_RIGHTBUTTON) !=
	      (hidDev->oldMouseButtons & USB_HID_MOUSE_RIGHTBUTTON))
	    kernelMouseButtonChange(3, (mouseData->buttons &
					USB_HID_MOUSE_RIGHTBUTTON));

	  // Save the current state
	  hidDev->oldMouseButtons = mouseData->buttons;
	}

      // Mouse movement.
      if (mouseData->xChange || mouseData->yChange)
	kernelMouseMove((int) mouseData->xChange, (int) mouseData->yChange);
    }
}


static int detectTarget(void *parent, int target, void *driver)
{
  int status = 0;
  hidDevice *hidDev = NULL;
  int count;

  // Get an HID device structure
  hidDev = kernelMalloc(sizeof(hidDevice));
  if (hidDev == NULL)
    return (status = ERR_MEMORY);

  hidDev->target = target;

  hidDev->usbDev = kernelUsbGetDevice(target);
  if (hidDev->usbDev == NULL)
    {
      status = ERR_NODATA;
      goto out;
    }

  hidDev->usbDev->data = hidDev;

  // If the USB class is 0x03, then we believe we have a USB HID device.
  // However, at the moment we're only supporting keyboards (protocol 0x01)
  // and mice (protocol 0x02) using the boot protocol (subclass 0x01).
  // then we believe we have a USB keyboard or mouse device

  if (hidDev->usbDev->classCode != 0x03)
    {
      status = ERR_INVALID;
      goto out;
    }

  if (hidDev->usbDev->protocol == 0x01)
    {
      hidDev->type = hid_keyboard;
      hidDev->dev.device.class = kernelDeviceGetClass(DEVICECLASS_KEYBOARD);
      hidDev->dev.device.subClass =
	kernelDeviceGetClass(DEVICESUBCLASS_KEYBOARD_USB);
    }
  else if (hidDev->usbDev->protocol == 0x02)
    {
      hidDev->type = hid_mouse;
      hidDev->dev.device.class = kernelDeviceGetClass(DEVICECLASS_MOUSE);
      hidDev->dev.device.subClass =
	kernelDeviceGetClass(DEVICESUBCLASS_MOUSE_USB);
    }
  else
    {
      status = ERR_INVALID;
      goto out;
    }

  // Record the interface that uses the boot protocol
  for (count = 0; count < hidDev->usbDev->configDesc->numInterfaces; count ++)
    if (hidDev->usbDev->interDesc[count]->interSubClass == 1)
      {
	hidDev->interNum = hidDev->usbDev->interDesc[count]->interNum;
	break;
      }

  // Record the interrupt-in endpoint
  for (count = 0; count < hidDev->usbDev->numEndpoints; count ++)
    if ((hidDev->usbDev->endpointDesc[count]->attributes == 0x03) &&
	(hidDev->usbDev->endpointDesc[count]->endpntAddress & 0x80) &&
	!hidDev->intrInDesc)
      {
	hidDev->intrInDesc = hidDev->usbDev->endpointDesc[count];
	hidDev->intrInEndpoint = (hidDev->intrInDesc->endpntAddress & 0xF);
	kernelDebug(debug_usb, "Got interrupt endpoint %d",
		    hidDev->intrInEndpoint);
	break;
      }

  // We *must* have an interrupt in endpoint.
  if (!hidDev->intrInDesc)
    {
      kernelError(kernel_error, "HID device %d has no interrupt endpoint",
		  target);
      status = ERR_NODATA;
      goto out;
    }

  // Try to get the HID descriptor
  status = getHidDescriptor(hidDev);
  if (status < 0)
    goto out;

  debugHidDesc(&hidDev->hidDesc);

  if (hidDev->usbDev->subClassCode == 0x01)
    {
      // Set to simple (boot) protocol
      status = setBootProtocol(hidDev);
      if (status < 0)
	goto out;
    }
  else
    {
      status = ERR_INVALID;
      goto out;
    }

  hidDev->dev.driver = driver;

  // Schedule the regular interrupt.
  kernelUsbScheduleInterrupt(hidDev->usbDev, hidDev->intrInEndpoint,
			     hidDev->intrInDesc->interval,
			     hidDev->intrInDesc->maxPacketSize, &interrupt);

  // Add the device
  status = kernelDeviceAdd(parent, &hidDev->dev);

 out:
  if (status < 0)
    {
      if (hidDev)
	kernelFree(hidDev);
    }
  else
    kernelDebug(debug_usb, "Detected USB HID device");

  return (status);
}


static int detect(void *parent, kernelDriver *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.

  int status = 0;
  kernelBusTarget *busTargets = NULL;
  int numBusTargets = 0;
  int deviceCount = 0;
  usbDevice usbDev;

  // Search the USB bus(es) for devices
  numBusTargets = kernelBusGetTargets(bus_usb, &busTargets);
  if (numBusTargets <= 0)
    return (status = 0);
      
  // Search the bus targets for USB HID devices
  for (deviceCount = 0; deviceCount < numBusTargets; deviceCount ++)
    {
      // Try to get the USB information about the target
      status = kernelBusGetTargetInfo(bus_usb, busTargets[deviceCount].target,
				      (void *) &usbDev);
      if (status < 0)
	continue;

      if (usbDev.classCode != 0x03)
	continue;
  
      detectTarget(parent, busTargets[deviceCount].target, driver);
    }

  kernelFree(busTargets);
  return (status = 0);
}


static int hotplug(void *parent, int busType __attribute__((unused)),
		   int target, int connected, kernelDriver *driver)
{
  // This routine is used to detect whether a newly-connected, hotplugged
  // device is supported by this driver during runtime, and if so to do the
  // appropriate device setup and registration.  Alternatively if the device
  // is disconnected a call to this function lets us know to stop trying
  // to communicate with it.

  int status = 0;
  usbDevice *usbDev = NULL;
  hidDevice *hidDev = NULL;

  if (connected)
    {
      status = detectTarget(parent, target, driver);
      if (status < 0)
	return (status);
    }
  else
    {
      usbDev = kernelUsbGetDevice(target);
      if (usbDev == NULL)
	{
	  kernelError(kernel_error, "No such USB device %d", target);
	  return (status = ERR_NOSUCHENTRY);
	}

      hidDev = usbDev->data;
      if (hidDev == NULL)
	{
	  kernelError(kernel_error, "No such HID device %d", target);
	  return (status = ERR_NOSUCHENTRY);
	}

      // Found it.
      kernelDebug(debug_scsi, "HID device removed");

      // Unschedule any interrupts
      if (hidDev->intrInDesc)
	kernelUsbUnscheduleInterrupt(usbDev);

      // Remove it from the device tree
      kernelDeviceRemove(&hidDev->dev);

      // Free the memory.
      kernelFree(hidDev);
    }

  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelUsbKeyboardDriverRegister(kernelDriver *driver)
{
   // Device driver registration.

  driver->driverDetect = detect;
  driver->driverHotplug = hotplug;

  return;
}


void kernelUsbMouseDriverRegister(kernelDriver *driver)
{
   // Device driver registration.

  driver->driverDetect = detect;
  driver->driverHotplug = hotplug;

  return;
}
