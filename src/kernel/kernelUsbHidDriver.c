//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelUsbHidDriver.c
//

// Driver for USB HIDs (human interface devices) such as keyboards and meeses.

#include "kernelDriver.h"	// Contains my prototypes
#include "kernelCpu.h"
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

// Flags for keyboard shift state
#define RALT_FLAG			0x0040
#define RSHIFT_FLAG			0x0020
#define RCONTROL_FLAG		0x0010
#define LALT_FLAG			0x0004
#define LSHIFT_FLAG			0x0002
#define LCONTROL_FLAG		0x0001

// Flags for keyboard toggle state
#define SCROLLLOCK_FLAG		0x04
#define CAPSLOCK_FLAG		0x02
#define NUMLOCK_FLAG		0x01

// Some USB keyboard scan codes we're interested in
#define CAPSLOCK_KEY		57
#define SCROLLLOCK_KEY		71
#define NUMLOCK_KEY			83

// Mapping of USB keyboard scan codes to EFI scan codes
static keyScan usbScan2Scan[] = { 0, 0, 0, 0,
	// A-G
	keyC1, keyB5, keyB3, keyC3, keyD3, keyC4, keyC5,			// 04-0A
	// H-N
	keyC6, keyD8, keyC7, keyC8, keyC9, keyB7, keyB6,			// 0B-11
	// O-U
	keyD9, keyD10, keyD1, keyD4, keyC2, keyD5, keyD7,			// 12-18
	// V-Z, 1-2
	keyB4, keyD2, keyB2, keyD6, keyB1, keyE1, keyE2, 			// 19-1F
	// 3-9
	keyE3, keyE4, keyE5, keyE6,	keyE7, keyE8, keyE9,			// 20-26
	// 0 Enter Esc Bs Tab
	keyE10, keyEnter, keyEsc, keyBackSpace, keyTab,				// 27-2B
	// Spc - = [ ] Bs
	keySpaceBar, keyE11, keyE12, keyD11, keyD12, keyB0,			// 2C-32
	// (INT 2) ; ' ` , .
	keyC12, keyC10, keyC11, keyE0, keyB8, keyB9,				// 32-37
	// / Caps, F1-F4
	keyB10, keyCapsLock, keyF1, keyF2, keyF3, keyF4,			// 38-3D
	// F5-F11
	keyF5, keyF6, keyF7, keyF8, keyF9, keyF10, keyF11,			// 3E-44
	// F12, PrtScn ScrLck PauseBrk Ins Home
	keyF12, keyPrint, keySLck, keyPause, keyIns, keyHome,		// 45-4A
	// PgUp Del End PgDn CurR
	keyPgUp, keyDel, keyEnd, keyPgDn, keyRightArrow,			// 4B-4F
	// CurL CurD CurU NumLck
	keyLeftArrow, keyDownArrow, keyUpArrow, keyNLck,			// 50-53
	// / * - + Enter
	keySlash, keyAsterisk, keyMinus, keyPlus, keyEnter,			// 54-58
	// End CurD PgDn CurL CurR
	keyOne, keyTwo, keyThree, keyFour, keyFive, keySix,			// 59-5E
	// Home CurU PgUp Ins Del (INT 1)
	keySeven, keyEight, keyNine, keyZero, keyDel, keyB0,		// 5F-64
	// Win
	keyA3, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0	// 68-7F
};

#ifdef DEBUG
static void debugHidDesc(usbHidDesc *hidDesc)
{
	kernelDebug(debug_usb, "Debug HID descriptor:\n"
		"  descLength=%d\n"
		"  descType=%x\n"
		"  hidVersion=%d.%d\n"
		"  countryCode=%d\n"
		"  numDescriptors=%d\n"
		"  repDescType=%d\n"
		"  repDescLength=%d", hidDesc->descLength, hidDesc->descType,
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

	kernelDebug(debug_usb, "Get HID descriptor for target 0x%08x, "
		"interface %d", hidDev->busTarget->id, hidDev->interNum);

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
	return (kernelBusWrite(hidDev->busTarget, sizeof(usbTransaction),
		(void *) &usbTrans));
}


static int setBootProtocol(hidDevice *hidDev)
{
	usbTransaction usbTrans;

	kernelDebug(debug_usb, "Set HID boot protocol for target 0x%08x, "
		"interface %d", hidDev->busTarget->id, hidDev->interNum);

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
	return (kernelBusWrite(hidDev->busTarget, sizeof(usbTransaction),
		(void *) &usbTrans));
}


static void setLights(hidDevice *hidDev)
{
	// This function is called to update the state of the keyboard lights

	usbTransaction usbTrans;
	unsigned char report = hidDev->keyboard.lights;

	kernelDebug(debug_usb, "Set HID report %02x for target 0x%08x, "
		"interface %d", report, hidDev->busTarget->id, hidDev->interNum);

	// Send a "set report" command to the keyboard with the LED status.
	kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
	usbTrans.type = usbxfer_control;
	usbTrans.address = hidDev->usbDev->address;
	usbTrans.control.requestType = (USB_DEVREQTYPE_HOST2DEV |
		USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_INTERFACE);
	usbTrans.control.request = USB_HID_SET_REPORT;
	usbTrans.control.value = (2 << 8);
	usbTrans.control.index = hidDev->interNum;
	usbTrans.length = 1;
	usbTrans.buffer = &report;
	usbTrans.pid = USB_PID_OUT;

	// Write the command
	kernelBusWrite(hidDev->busTarget, sizeof(usbTransaction),
		(void *) &usbTrans);
}


static void keyboardThreadCall(kernelKeyboard *keyboard)
{
	// Update keyboard lights to reflect the keyboard state

	hidDevice *hidDev = keyboard->data;
	unsigned lights = 0;

	if (hidDev->keyboard.state.toggleState & KEYBOARD_SCROLL_LOCK_ACTIVE)
		lights |= SCROLLLOCK_FLAG;
	if (hidDev->keyboard.state.toggleState & KEYBOARD_CAPS_LOCK_ACTIVE)
		lights |= CAPSLOCK_FLAG;
	if (hidDev->keyboard.state.toggleState & KEYBOARD_NUM_LOCK_ACTIVE)
		lights |= NUMLOCK_FLAG;

	if (lights != keyboard->lights)
	{
		keyboard->lights = lights;
		setLights(hidDev);
	}

	if (keyboard->repeatKey && (kernelCpuTimestamp() >= keyboard->repeatTime))
	{
		kernelKeyboardInput(&hidDev->keyboard, EVENT_KEY_DOWN,
			keyboard->repeatKey);
		keyboard->repeatTime = (kernelCpuTimestamp() +
			(kernelCpuTimestampFreq() >> 5));
	}
}


static void interrupt(usbDevice *usbDev, void *buffer, unsigned length)
{
	hidDevice *hidDev = usbDev->data;
	usbHidKeyboardData *keyboardData = NULL;
	usbHidMouseData *mouseData = NULL;
	keyScan scan = 0;
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

		// If the modifier flags have changed, we will need to send fake
		// key presses or releases for CTRL, ALT, or shift
		if (keyboardData->modifier != hidDev->oldKeyboardData.modifier)
		{
			if ((keyboardData->modifier & RALT_FLAG) !=
				(hidDev->oldKeyboardData.modifier & RALT_FLAG))
			{
				kernelKeyboardInput(&hidDev->keyboard,
					((keyboardData->modifier & RALT_FLAG)?
						EVENT_KEY_DOWN : EVENT_KEY_UP), keyA2);
			}

			if ((keyboardData->modifier & RSHIFT_FLAG) !=
				(hidDev->oldKeyboardData.modifier & RSHIFT_FLAG))
			{
				kernelKeyboardInput(&hidDev->keyboard,
					((keyboardData->modifier & RSHIFT_FLAG)?
						EVENT_KEY_DOWN : EVENT_KEY_UP), keyRShift);
			}

			if ((keyboardData->modifier & RCONTROL_FLAG) !=
				(hidDev->oldKeyboardData.modifier & RCONTROL_FLAG))
			{
				kernelKeyboardInput(&hidDev->keyboard,
					((keyboardData->modifier & RCONTROL_FLAG)?
						EVENT_KEY_DOWN : EVENT_KEY_UP), keyRCtrl);
			}

			if ((keyboardData->modifier & LALT_FLAG) !=
				(hidDev->oldKeyboardData.modifier & LALT_FLAG))
			{
				kernelKeyboardInput(&hidDev->keyboard,
					((keyboardData->modifier & LALT_FLAG)?
						EVENT_KEY_DOWN : EVENT_KEY_UP), keyLAlt);
			}

			if ((keyboardData->modifier & LSHIFT_FLAG) !=
				(hidDev->oldKeyboardData.modifier & LSHIFT_FLAG))
			{
				kernelKeyboardInput(&hidDev->keyboard,
					((keyboardData->modifier & LSHIFT_FLAG)?
						EVENT_KEY_DOWN : EVENT_KEY_UP), keyLShift);
			}

			if ((keyboardData->modifier & LCONTROL_FLAG) !=
				(hidDev->oldKeyboardData.modifier & LCONTROL_FLAG))
			{
				kernelKeyboardInput(&hidDev->keyboard,
					((keyboardData->modifier & LCONTROL_FLAG)?
						EVENT_KEY_DOWN : EVENT_KEY_UP), keyLCtrl);
			}
		}

		// Find key releases
		for (count = 0; count < 6; count ++)
		{
			if ((hidDev->oldKeyboardData.code[count] < 4) ||
				(hidDev->oldKeyboardData.code[count] > 231))
			{
				break;
			}

			if (strchr((const char *) keyboardData->code,
				hidDev->oldKeyboardData.code[count]))
			{
				continue;
			}

			scan = usbScan2Scan[hidDev->oldKeyboardData.code[count]];

			kernelKeyboardInput(&hidDev->keyboard, EVENT_KEY_UP, scan);

			if (hidDev->keyboard.repeatKey == scan)
				hidDev->keyboard.repeatKey = 0;
		}

		// Find new keypresses
		for (count = 0; count < 6; count ++)
		{
			if ((keyboardData->code[count] < 4) ||
				(keyboardData->code[count] > 231))
			{
				break;
			}

			if (strchr((const char *) hidDev->oldKeyboardData.code,
				keyboardData->code[count]))
			{
				continue;
			}

			scan = usbScan2Scan[keyboardData->code[count]];

			kernelKeyboardInput(&hidDev->keyboard, EVENT_KEY_DOWN, scan);

			hidDev->keyboard.repeatKey = scan;
			hidDev->keyboard.repeatTime = (kernelCpuTimestamp() +
				(kernelCpuTimestampFreq() >> 1));
		}

		if (length < sizeof(usbHidKeyboardData))
			kernelMemClear(&hidDev->oldKeyboardData,
				sizeof(usbHidKeyboardData));

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
			{
				kernelMouseButtonChange(1, (mouseData->buttons &
					USB_HID_MOUSE_LEFTBUTTON));
			}

			// Middle button; button 2
			if ((mouseData->buttons & USB_HID_MOUSE_MIDDLEBUTTON) !=
				(hidDev->oldMouseButtons & USB_HID_MOUSE_MIDDLEBUTTON))
			{
				kernelMouseButtonChange(2, (mouseData->buttons &
					USB_HID_MOUSE_MIDDLEBUTTON));
			}

			// Right button; button 3
			if ((mouseData->buttons & USB_HID_MOUSE_RIGHTBUTTON) !=
				(hidDev->oldMouseButtons & USB_HID_MOUSE_RIGHTBUTTON))
			{
				kernelMouseButtonChange(3, (mouseData->buttons &
					USB_HID_MOUSE_RIGHTBUTTON));
			}

			// Save the current state
			hidDev->oldMouseButtons = mouseData->buttons;
		}

		// Mouse movement.
		if (mouseData->xChange || mouseData->yChange)
			kernelMouseMove((int) mouseData->xChange, (int) mouseData->yChange);
	}
}


static int detectTarget(void *parent, int target, void *driver, hidType type)
{
	int status = 0;
	hidDevice *hidDev = NULL;
	int count;

	// Get an HID device structure
	hidDev = kernelMalloc(sizeof(hidDevice));
	if (!hidDev)
		return (status = ERR_MEMORY);

	hidDev->busTarget = kernelBusGetTarget(bus_usb, target);
	if (!hidDev->busTarget)
	{
		status = ERR_NODATA;
		goto out;
	}

	hidDev->usbDev = kernelUsbGetDevice(target);
	if (!hidDev->usbDev)
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
		if ((type != hid_keyboard) && (type != hid_any))
		{
			status = ERR_INVALID;
			goto out;
		}

		hidDev->type = hid_keyboard;
		hidDev->usbDev->dev.device.class =
			kernelDeviceGetClass(DEVICECLASS_KEYBOARD);
		hidDev->usbDev->dev.device.subClass =
			kernelDeviceGetClass(DEVICESUBCLASS_KEYBOARD_USB);

		// Set up the keyboard structure
		hidDev->keyboard.type = keyboard_usb;
		hidDev->keyboard.data = hidDev;
		hidDev->keyboard.threadCall = &keyboardThreadCall;

		// Add this keyboard to the keyboard subsystem
		status = kernelKeyboardAdd(&hidDev->keyboard);
		if (status < 0)
			goto out;
	}
	else if (hidDev->usbDev->protocol == 0x02)
	{
		if ((type != hid_mouse) && (type != hid_any))
		{
			status = ERR_INVALID;
			goto out;
		}

		hidDev->type = hid_mouse;
		hidDev->usbDev->dev.device.class =
			kernelDeviceGetClass(DEVICECLASS_MOUSE);
		hidDev->usbDev->dev.device.subClass =
			kernelDeviceGetClass(DEVICESUBCLASS_MOUSE_USB);
	}
	else
	{
		status = ERR_INVALID;
		goto out;
	}

	// Record the interface that uses the boot protocol
	for (count = 0; count < hidDev->usbDev->configDesc->numInterfaces;
		count ++)
	{
		if (hidDev->usbDev->interDesc[count]->interSubClass == 1)
		{
			hidDev->interNum = hidDev->usbDev->interDesc[count]->interNum;
			break;
		}
	}

	// Record the interrupt-in endpoint
	for (count = 0; count < hidDev->usbDev->numEndpoints; count ++)
	{
		if (((hidDev->usbDev->endpointDesc[count]->attributes &
				USB_ENDP_ATTR_MASK) == USB_ENDP_ATTR_INTERRUPT) &&
			(hidDev->usbDev->endpointDesc[count]->endpntAddress & 0x80) &&
				!hidDev->intrInDesc)
		{
			hidDev->intrInDesc = hidDev->usbDev->endpointDesc[count];
			hidDev->intrInEndpoint = hidDev->intrInDesc->endpntAddress;
			kernelDebug(debug_usb, "Got interrupt endpoint %02x",
				hidDev->intrInEndpoint);
			break;
		}
	}

	// We *must* have an interrupt in endpoint.
	if (!hidDev->intrInDesc)
	{
		kernelError(kernel_error, "HID device 0x%08x has no interrupt "
			"endpoint",	target);
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

	hidDev->usbDev->dev.driver = driver;

	// Tell USB that we're claiming this device.
	kernelBusDeviceClaim(hidDev->busTarget, driver);

	// Schedule the regular interrupt.
	kernelUsbScheduleInterrupt(hidDev->usbDev, hidDev->intrInEndpoint,
		hidDev->intrInDesc->interval, hidDev->intrInDesc->maxPacketSize,
		&interrupt);

	// Add the device
	status = kernelDeviceAdd(parent, (kernelDevice *) &hidDev->usbDev->dev);

out:
	if (status < 0)
	{
		if (hidDev)
		{
			if (hidDev->busTarget)
				kernelFree(hidDev->busTarget);
			kernelFree(hidDev);
		}
	}
	else
		kernelDebug(debug_usb, "Detected USB HID device");

 	return (status);
}


static int detect(kernelDriver *driver, hidType type)
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
		status = kernelBusGetTargetInfo(&busTargets[deviceCount],
			(void *) &usbDev);
		if (status < 0)
			continue;

		if (usbDev.classCode != 0x03)
			continue;

		if ((type == hid_keyboard) && (usbDev.protocol != 0x01))
			continue;

		if ((type == hid_mouse) && (usbDev.protocol != 0x02))
			continue;

		detectTarget(usbDev.controller->dev, busTargets[deviceCount].id,
			driver, type);
	}

	kernelFree(busTargets);
	return (status = 0);
}


static int detectKeyboard(void *parent __attribute__((unused)),
	kernelDriver *driver)
{
	return (detect(driver, hid_keyboard));
}


static int detectMouse(void *parent __attribute__((unused)),
	kernelDriver *driver)
{
	return (detect(driver, hid_mouse));
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
		status = detectTarget(parent, target, driver, hid_any);
		if (status < 0)
			return (status);
	}
	else
	{
		usbDev = kernelUsbGetDevice(target);
		if (!usbDev)
		{
			kernelError(kernel_error, "No such USB device 0x%08x", target);
			return (status = ERR_NOSUCHENTRY);
		}

		hidDev = usbDev->data;
		if (!hidDev)
		{
			kernelError(kernel_error, "No such HID device 0x%08x", target);
			return (status = ERR_NOSUCHENTRY);
		}

		// Found it.
		kernelDebug(debug_usb, "HID device removed");

		// Unschedule any interrupts
		if (hidDev->intrInDesc)
			kernelUsbUnscheduleInterrupt(usbDev);

		// Remove it from the device tree
		kernelDeviceRemove((kernelDevice *) &hidDev->usbDev->dev);

		// Free the memory.
		if (hidDev->busTarget)
			kernelFree(hidDev->busTarget);
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

	driver->driverDetect = detectKeyboard;
	driver->driverHotplug = hotplug;

	return;
}


void kernelUsbMouseDriverRegister(kernelDriver *driver)
{
	// Device driver registration.

	driver->driverDetect = detectMouse;
	driver->driverHotplug = hotplug;

	return;
}

