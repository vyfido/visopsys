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
//  kernelUsbDriver.c
//

#include "kernelUsbDriver.h"
#include "kernelInterrupt.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelPic.h"
#include "kernelProcessorX86.h"
#include "kernelError.h"
#include <string.h>

//#define DEBUG

#ifdef DEBUG
  #include "kernelText.h"

  #define debug(message, arg...) kernelTextPrintLine(message, ##arg)
#else
  #define debug(message, arg...) do { } while (0)
#endif

static usbSubClass subclass_hid[] = {
  { 0x01, "keyboard", DEVICECLASS_KEYBOARD, DEVICESUBCLASS_KEYBOARD_USB },
  { 0x02, "mouse", DEVICECLASS_MOUSE, DEVICESUBCLASS_MOUSE_USB },
  { USB_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static usbSubClass subclass_massstorage[] = {
  { 0x01, "flash", DEVICECLASS_STORAGE, DEVICESUBCLASS_STORAGE_FLASH },
  { 0x02, "CD/DVD", DEVICECLASS_DISK, DEVICESUBCLASS_DISK_CDDVD },
  { 0x03, "tape", DEVICECLASS_STORAGE, DEVICESUBCLASS_STORAGE_TAPE },
  { 0x04, "floppy", DEVICECLASS_DISK, DEVICESUBCLASS_DISK_FLOPPY },
  { 0x05, "floppy", DEVICECLASS_DISK, DEVICESUBCLASS_DISK_FLOPPY },
  { 0x06, "SCSI", DEVICECLASS_DISK, DEVICESUBCLASS_DISK_SCSI },
  { USB_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static usbSubClass subclass_vendor[] = {
  { 0xFF, "unknown", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { USB_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static usbClass usbClasses[] = {
  { 0x01, "audio", NULL },
  { 0x02, "CDC-control", NULL },
  { 0x03, "human interface device", subclass_hid },
  { 0x05, "physical", NULL },
  { 0x06, "image", NULL },
  { 0x07, "printer", NULL },
  { 0x08, "mass storage", subclass_massstorage },
  { 0x09, "hub", NULL },
  { 0x0A, "CDC-data", NULL },
  { 0x0B, "chip/smart card", NULL },
  { 0x0D, "content-security", NULL },
  { 0x0E, "video", NULL },
  { 0xDC, "diagnostic", NULL },
  { 0xE0, "wireless controller", NULL },
  { 0xFE, "application-specific", NULL },
  { 0xFF, "vendor-specific", subclass_vendor },
  { USB_INVALID_CLASSCODE, "", NULL }
};

static usbRootHub *usbControllers[USB_MAX_CONTROLLERS];
static int numUsbControllers = 0;
static int usbProcId = 0;


/*
static void usbInterrupt(void)
{
  // This is the USB interrupt handler.
 
  void *address = NULL;
  int interruptNum = 0;

  kernelProcessorIsrEnter(address);
  kernelProcessingInterrupt = 1;

  // Which interrupt number is active?
  interruptNum = kernelPicGetActive();

  //debug("USB: interrupt %d", interruptNum);

  kernelPicEndOfInterrupt(interruptNum);

  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit(address);
}
*/


static void getClass(int classCode, usbClass **class)
{
  // Return the USB class, given the class code

  int count;

  for (count = 0; count < 256; count++)
    {	
      // If no more classcodes are in the list
      if (usbClasses[count].classCode == USB_INVALID_CLASSCODE)
	{
	  *class = NULL;
	  return;
	}
		
      // If valid classcode is found
      if (usbClasses[count].classCode == classCode)
	{
	  *class = &(usbClasses[count]);
	  return;
	}
    }
}


static void getSubClass(usbClass *class, int subClassCode, int protocol,
			usbSubClass **subClass)
{
  // Return the USB subclass, given the class and subclass code

  int count;

  // Some things are classified by protocol rather than subclass code
  if (class->classCode == 3)
    subClassCode = protocol;

  if (class->subClasses)
    {
      for (count = 0; count < 256; count++)
	{	
	  // If no more subclass codes are in the list
	  if (class->subClasses[count].subClassCode ==
	      USB_INVALID_SUBCLASSCODE)
	    {
	      *subClass = NULL;
	      return;
	    }

	  if (class->subClasses[count].subClassCode == subClassCode)
	    {
	      *subClass = &(class->subClasses[count]);
	      return;
	    }
	}
    }
}


static int getClassName(int classCode, int subClassCode, int protocol,
			char **className, char **subClassName)
{
  // Returns name of the class and the subclass in human readable format.
  // Buffers classname and subclassname have to provide

  int status = 0;
  usbClass *class = NULL;
  usbSubClass *subClass = NULL;

  getClass(classCode, &class);
  if (class == NULL)
    {
      *className = "unknown device";
      *subClassName = "";
      return (status = USB_INVALID_CLASSCODE);
    }

  *className = (char *) class->name;

  getSubClass(class, subClassCode, protocol, &subClass);
  if (subClass == NULL)
    {
      *subClassName = "";
      return (status = USB_INVALID_SUBCLASSCODE);
    }

  *subClassName = (char *) subClass->name;
  return (status = 0);
}


static void deviceInfo2BusTarget(usbDevice *dev, kernelBusTarget *target)
{
  // Translate a device to a bus target listing

  usbClass *class = NULL;
  usbSubClass *subClass = NULL;

  getClass(dev->classCode, &class);
  if (class == NULL)
    return;

  getSubClass(class, dev->subClassCode, dev->protocol, &subClass);
  if (subClass == NULL)
    return;

  target->target = usbMakeTargetCode(dev->controller, dev->address, 0);
  target->class = kernelDeviceGetClass(subClass->systemClassCode);
  target->subClass = kernelDeviceGetClass(subClass->systemSubClassCode);
}


static void usbThread(void)
{
  usbRootHub *usb = NULL;
  int count;

  while(1)
    {
      kernelMultitaskerWait(10);

      for (count = 0; count < numUsbControllers; count ++)
	{
	  usb = usbControllers[count];

	  if (usb->threadCall)
	    usb->threadCall(usb);
	}
    }
}


static int driverGetTargets(kernelBusTarget **pointer)
{
  // Generate our list of targets and return it

  static kernelBusTarget *busTargets = NULL;
  int numBusTargets = 0;
  usbRootHub *usb = NULL;
  int targetCount = 0;
  int controllerCount, deviceCount;
  
  for (controllerCount = 0; controllerCount < numUsbControllers;
       controllerCount ++)
    numBusTargets += usbControllers[controllerCount]->numDevices;
  
  if (busTargets)
    {
      kernelFree(busTargets);
      busTargets = NULL;
    }

  if (numBusTargets)
    {
      // Allocate memory for the targets list
      busTargets = kernelMalloc(numBusTargets * sizeof(kernelBusTarget));
      if (busTargets == NULL)
	return (numBusTargets = 0);

      targetCount = 0;

      // Now fill up our targets list
      for (controllerCount = 0; controllerCount < numUsbControllers;
	   controllerCount ++)
	{
	  usb = usbControllers[controllerCount];

	  for (deviceCount = 0; deviceCount < usb->numDevices; deviceCount ++)
	    deviceInfo2BusTarget(usb->devices[deviceCount],
				 &busTargets[targetCount]);
	}
    }

  *pointer = busTargets;
  return (numBusTargets);
}


static int driverGetTargetInfo(int target, void *pointer)
{
  // Given a target number, copy the device's USB device info into the
  // supplied memory pointer

  int status = 0;
  usbRootHub *usb = NULL;
  usbDevice *dev = NULL;
  int controllerCount, deviceCount;

  for (controllerCount = 0; controllerCount < numUsbControllers;
       controllerCount ++)
    {
      usb = usbControllers[controllerCount];

      for (deviceCount = 0; deviceCount < usb->numDevices; deviceCount ++)
	{
	  dev = usb->devices[deviceCount];

	  if (usbMakeTargetCode(dev->controller, dev->address, 0) == target)
	    {
	      kernelMemCopy(usb->devices[deviceCount], pointer,
			    sizeof(usbDevice));
	      break;
	    }
	}
    }

  return (status = 0);
}


static void addFuncPointers(usbRootHub *usb)
{
  usb->getClass = &getClass;
  usb->getSubClass = &getSubClass;
  usb->getClassName = &getClassName;
}


static int transaction(int target, usbTransaction *trans)
{
  int status = 0;
  unsigned char controller = 0;
  unsigned char address = 0;
  unsigned char endPoint = 0; 
  usbRootHub *usb = NULL;
  usbDevice *dev = NULL;
  int count;

  // Break out the target information
  usbMakeContAddrEndp(target, controller, address, endPoint);

  // Try to find the controller
  for (count = 0; count < numUsbControllers; count ++)
    if (usbControllers[count]->controller == controller)
      {
	usb = usbControllers[count];
	break;
      }

  if (usb == NULL)
    {
      kernelError(kernel_error, "No such controller %d", controller);
      return (status = ERR_NOSUCHENTRY);
    }

  // Try to find the device
  for (count = 0; count < usb->numDevices; count ++)
    if (usb->devices[count]->address == address)
      {
	dev = usb->devices[count];
	break;
      }

  if (dev == NULL)
    {
      kernelError(kernel_error, "No such device %d", address);
      return (status = ERR_NOSUCHENTRY);
    }

  if (usb->transaction)
    status = usb->transaction(usb, dev, trans);

  return (status);
}


static int driverDetect(void *parent __attribute__((unused)), void *driver)
{
  // This routine is called to detect USB buses.  There are a couple of
  // different types so we call further detection routines to do the
  // actual hardware interaction.

  int status = 0;
  kernelDevice *pciDevice = NULL;
  kernelBusTarget *pciTargets = NULL;
  int numPciTargets = 0;
  int deviceCount = 0;
  kernelDevice *dev = NULL;
  usbRootHub *usb = NULL;

  numUsbControllers = 0;

  // See if there are any USB controllers on the PCI bus.  This obviously
  // depends upon PCI hardware detection occurring before USB detection.

  // Get the PCI bus device
  status = kernelDeviceFindType(kernelDeviceGetClass(DEVICECLASS_BUS),
				kernelDeviceGetClass(DEVICESUBCLASS_BUS_PCI),
				&pciDevice, 1);
  if (status <= 0)
    return (status);

  // Search the PCI bus(es) for devices
  numPciTargets = kernelBusGetTargets(bus_pci, &pciTargets);
  if (numPciTargets <= 0)
    return (status = numPciTargets);

  // Search the PCI bus targets for USB controllers
  for (deviceCount = 0; ((deviceCount < numPciTargets) &&
			 (numUsbControllers < USB_MAX_CONTROLLERS));
       deviceCount ++)
    {
      // If it's not a USB device, skip it
      if ((pciTargets[deviceCount].class == NULL) ||
	  (pciTargets[deviceCount].class->class != DEVICECLASS_BUS) ||
	  (pciTargets[deviceCount].subClass == NULL) ||
	  (pciTargets[deviceCount].subClass->class != DEVICESUBCLASS_BUS_USB))
	continue;

      // See if it's a UHCI controller
      if ((dev = kernelUsbUhciDetect(pciDevice, &pciTargets[deviceCount],
				     driver)))
	; // empty

      // See if it's an EHCI controller
      else if ((dev = kernelUsbEhciDetect(pciDevice, &pciTargets[deviceCount],
					  driver)))
	; // empty

      else
	// Not a supported USB controller
	continue;

      usb = dev->data;
      usb->device = dev;
      usb->controller = numUsbControllers;
      addFuncPointers(usb);

      /*
      // Register our interrupt handler
      status = kernelInterruptHook(usb->interrupt, &usbInterrupt);
      if (status < 0)
	{
	  kernelFree(pciTargets);
	  return (status);
	}

      // Turn on the interrupt
      kernelPicMask(usb->interrupt, 1);
      */

      // Call the thread function once now, so that it can do its device
      // enumeration
      if (usb->threadCall)
	usb->threadCall(usb);
      usb->didEnum = 1;

      status = kernelBusRegister(bus_usb, dev);
      if (status < 0)
	{
	  kernelFree(pciTargets);
	  return (status);
	}

      usbControllers[numUsbControllers++] = usb;
    }

  kernelFree(pciTargets);
  return (status = 0);
}


static int driverWrite(int target, void *params)
{
  // A wrapper for the read/write function
  return (transaction(target, (usbTransaction *) params));
}


// Our driver operations structure.
static kernelBusOps usbOps = {
  driverGetTargets,
  driverGetTargetInfo,
  NULL, // driverReadRegister
  NULL, // driverWriteRegister
  NULL, // driverDeviceEnable
  NULL, // driverSetMaster
  NULL, // driverRead  (All USB transactions are 'write' transactions)
  driverWrite
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelUsbDriverRegister(void *driverData)
{
   // Device driver registration.

  kernelDriver *driver = (kernelDriver *) driverData;

  driver->driverDetect = driverDetect;
  driver->ops = &usbOps;

  return;
}


int kernelUsbInitialize(void)
{
  // This gets called after multitasking is enabled.

  if (numUsbControllers)
    usbProcId =
      kernelMultitaskerSpawnKernelThread(usbThread, "usb thread", 0, NULL);

  return (0);
}
