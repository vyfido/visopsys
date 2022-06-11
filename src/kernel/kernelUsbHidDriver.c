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
//  kernelUsbHidDriver.c
//

// Driver for USB HIDs (human interface devices) such as keyboards and meeses.

#include "kernelDriver.h" // Contains my prototypes
#include "kernelUsbHidDriver.h"
#include "kernelBus.h"
#include "kernelDevice.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelMouse.h"
#include <string.h>

#ifdef USB_HID_DEBUG

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
#endif

static hidDevice *devices[16];
static int numDevices = 0;


static hidDevice *addHid(hidType type, int target)
{
  hidDevice *hidDev = NULL;

  if (numDevices >= 16)
    {
      kernelError(kernel_error, "Too many USB HID devices registered");
      return (hidDev = NULL);
    }

  hidDev = kernelMalloc(sizeof(hidDevice));
  if (hidDev == NULL)
    return (hidDev);

  hidDev->type = type;
  hidDev->target = target;

  devices[numDevices++] = hidDev;

  return (hidDev);
}


static int removeHid(int target)
{
  int status = 0;
  int position = 0;
  hidDevice *hidDev = NULL;
  int count;

  // Try to find the device
  for (count = 0; count < numDevices; count ++)
    if (devices[count]->target == target)
      {
	position = count;
	hidDev = devices[count];	
	break;
      }

  if (hidDev == NULL)
    {
      kernelError(kernel_error, "No such HID device %d", target);
      return (status = ERR_NOSUCHENTRY);
    }

  if ((numDevices > 1) && (position < (numDevices - 1)))
    devices[position] = devices[numDevices - 1];

  numDevices -= 1;

  kernelFree(hidDev);

  return (status = 0);
}


static hidDevice *findHid(int target)
{
  int count;
  
  // Try to find the device
  for (count = 0; count < numDevices; count ++)
    if (devices[count]->target == target)
      return (devices[count]);

  // Not found
  return (NULL);
}


static int getHidDescriptor(hidDevice *hidDev)
{
  int status = 0;
  usbTransaction usbTrans;
  //usbHidMouseData mouseData;

  kernelDebug(debug_usb, "Get HID descriptor for target %d, interface %d",
	      hidDev->target, hidDev->usbDev.interDesc[0]->interNum);

  // Set up the USB transaction to send the 'get descriptor' command
  kernelMemClear((void *) &usbTrans, sizeof(usbTrans));
  usbTrans.type = usbxfer_control;
  usbTrans.address = hidDev->usbDev.address;
  usbTrans.control.requestType = USB_DEVREQTYPE_INTERFACE;
  usbTrans.control.request = USB_GET_DESCRIPTOR;
  usbTrans.control.value = (USB_DESCTYPE_HID << 8);
  usbTrans.control.index = hidDev->usbDev.interDesc[0]->interNum;
  usbTrans.length = sizeof(usbHidDesc);
  usbTrans.buffer = &(hidDev->hidDesc);

  // Write the command
  status = kernelBusWrite(bus_usb, hidDev->target, (void *) &usbTrans);
  if (status < 0)
    return (status);

  /*
  kernelMemClear(&usbTrans, sizeof(usbTrans));
  usbTrans.type = usbxfer_control;
  usbTrans.address = hidDev->usbDev.address;
  usbTrans.control.requestType =
    (USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_INTERFACE);
  usbTrans.control.request = USB_HID_SET_PROTOCOL;
  usbTrans.control.index = hidDev->usbDev.interDesc[0]->interNum;
  usbTrans.pid = USB_PID_OUT;

  // Write the command
  status = kernelBusWrite(bus_usb, hidDev->target, &usbTrans);
  if (status < 0)
    return (status);

  while(0)
    {
      kernelMemClear(&usbTrans, sizeof(usbTrans));
      kernelMemClear(&mouseData, sizeof(usbHidMouseData));
      usbTrans.type = usbxfer_interrupt;
      usbTrans.address = hidDev->usbDev.address;
      usbTrans.endpoint = hidDev->intInEndpoint;
      usbTrans.length = 8;//sizeof(usbHidMouseData);
      usbTrans.buffer = &(mouseData);
      usbTrans.pid = USB_PID_IN;

      // Write the command
      status = kernelBusWrite(bus_usb, hidDev->target, &usbTrans);
      if (status < 0)
	kernelDebug(debug_usb, "Transfer error");

      if (usbTrans.bytes)
	{
	  kernelDebug(debug_usb, "Mouse buttons=%x xChange=%d yChange=%d",
	              mouseData.buttons, mouseData.xChange, mouseData.yChange);
	}
    }
  */

  return (status = 0);
}


static int mouseDetectTarget(void *parent, int target, void *driver)
{
  int status = 0;
  hidDevice *hidDev = NULL;
  int count;

  // Get an HID device structure
  hidDev = addHid(hid_mouse, target);
  if (hidDev == NULL)
    return (status = ERR_MEMORY);
 
  hidDev->dev.device.class = kernelDeviceGetClass(DEVICECLASS_MOUSE);
  hidDev->dev.device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_MOUSE_USB);
  hidDev->dev.driver = driver;

  // Try to get the USB information about the target
  status = kernelBusGetTargetInfo(bus_usb, target, (void *) &(hidDev->usbDev));
  if (status < 0)
    {
      removeHid(target);
      return (status);
    }

  // If the USB class is 0x03, the subclass is 0x06, and the protocol is 0x01
  // then we believe we have a USB mouse device
  if ((hidDev->usbDev.classCode != 0x03) ||
      (hidDev->usbDev.subClassCode != 0x01) ||
      (hidDev->usbDev.protocol != 0x02))
    {
      removeHid(target);
      return (status = ERR_INVALID);
    }

  // Try to get the HID descriptor
  status = getHidDescriptor(hidDev);
  if (status < 0)
    {
      removeHid(target);
      return (status);
    }

  debugHidDesc(&(hidDev->hidDesc));

  // Record the interrupt-in endpoint
  for (count = 0; count < hidDev->usbDev.interDesc[0]->numEndpoints; count ++)
    {
      if (hidDev->usbDev.endpointDesc[count]->attributes != 0x03)
	continue;

      if (!hidDev->intInEndpoint &&
	  (hidDev->usbDev.endpointDesc[count]->endpntAddress & 0x80))
	{
	  hidDev->intIn = hidDev->usbDev.endpointDesc[count];
	  hidDev->intInEndpoint = (hidDev->intIn->endpntAddress & 0xF);
	  kernelDebug(debug_usb, "Got interrupt in endpoint %d",
		      hidDev->intInEndpoint);
	}
    }

  // Add the device
  status = kernelDeviceAdd(parent, &(hidDev->dev));
  if (status < 0)
    {
      removeHid(target);
      return (status);
    }

  kernelDebug(debug_usb, "Detected mouse");

  return (status = 0);
}


static int mouseDetect(void *parent __attribute__((unused)),
		       kernelDriver *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.

  int status = 0;
  kernelDevice *usbDev = NULL;
  kernelBusTarget *busTargets = NULL;
  int numBusTargets = 0;
  int deviceCount = 0;

  // Look for USB Mice
  status = kernelDeviceFindType(kernelDeviceGetClass(DEVICECLASS_BUS),
				kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB),
				&usbDev, 1);
  if (status > 0)
    {
      // Search the USB bus(es) for devices
      numBusTargets = kernelBusGetTargets(bus_usb, &busTargets);
      if (numBusTargets <= 0)
	return (status = 0);
      
      // Search the bus targets for USB mouse devices
      for (deviceCount = 0; deviceCount < numBusTargets; deviceCount ++)
	{
	  // If it's not a USB mouse device, skip it
	  if ((busTargets[deviceCount].class == NULL) ||
	      (busTargets[deviceCount].class->class != DEVICECLASS_MOUSE) ||
	      (busTargets[deviceCount].subClass == NULL) ||
	      (busTargets[deviceCount].subClass->class !=
	       DEVICESUBCLASS_MOUSE_USB))
	    continue;

	  mouseDetectTarget(usbDev, busTargets[deviceCount].target, driver);
	}

      kernelFree(busTargets);
    }

  return (status = 0);
}


static int mouseHotplug(void *parent, int busType __attribute__((unused)),
			int target, int connected, kernelDriver *driver)
{
  // This routine is used to detect whether a newly-connected, hotplugged
  // device is supported by this driver during runtime, and if so to do the
  // appropriate device setup and registration.  Alternatively if the device
  // is disconnected a call to this function lets us know to stop trying
  // to communicate with it.

  int status = 0;
  hidDevice *hidDev = NULL;

  if (connected)
    {
      status = mouseDetectTarget(parent, target, driver);
      if (status < 0)
	return (status);
    }
  else
    {
      hidDev = findHid(target);
      if (hidDev == NULL)
	{
	  kernelError(kernel_error, "No such HID device %d", target);
	  return (status = ERR_NOSUCHENTRY);
	}

      // Remove it from the device tree
      kernelDeviceRemove(&(hidDev->dev));

      // Delete.
      status = removeHid(target);
      if (status < 0)
	return (status);
    }

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


void kernelUsbMouseDriverRegister(kernelDriver *driver)
{
   // Device driver registration.

  driver->driverDetect = mouseDetect;
  driver->driverHotplug = mouseHotplug;
  driver->ops = &mouseOps;

  return;
}
