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
//  kernelUsbDriver.c
//

#include "kernelUsbDriver.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelInterrupt.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelPic.h"
#include "kernelProcessorX86.h"
#include "kernelSysTimer.h"
#include <string.h>
#include <stdlib.h>

#ifdef DEBUG
static inline void debugDeviceDesc(usbDeviceDesc *deviceDesc)
{
  kernelDebug(debug_usb, "Debug device descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%d\n"
	      "    usbVersion=%d.%d\n"
	      "    deviceClass=%x\n"
	      "    deviceSubClass=%x\n"
	      "    deviceProtocol=%x\n"
	      "    maxPacketSize0=%d\n"
	      "    vendorId=%04x\n"
	      "    productId=%04x\n"
	      "    deviceVersion=%d.%d\n"
	      "    manuStringIdx=%d\n"
	      "    prodStringIdx=%d\n"
	      "    serStringIdx=%d\n"
	      "    numConfigs=%d", deviceDesc->descLength,
	      deviceDesc->descType, ((deviceDesc->usbVersion & 0xFF00) >> 8),
	      (deviceDesc->usbVersion & 0xFF), deviceDesc->deviceClass,
	      deviceDesc->deviceSubClass, deviceDesc->deviceProtocol,
	      deviceDesc->maxPacketSize0, deviceDesc->vendorId,
	      deviceDesc->productId,
	      ((deviceDesc->deviceVersion & 0xFF00) >> 8),
	      (deviceDesc->deviceVersion & 0xFF), deviceDesc->manuStringIdx,
	      deviceDesc->prodStringIdx, deviceDesc->serStringIdx,
	      deviceDesc->numConfigs);
}


static inline void debugDevQualDesc(usbDevQualDesc *devQualDesc)
{
  kernelDebug(debug_usb, "Debug device qualifier descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%d\n"
	      "    usbVersion=%d.%d\n"
	      "    deviceClass=%x\n"
	      "    deviceSubClass=%x\n"
	      "    deviceProtocol=%x\n"
	      "    maxPacketSize0=%d\n"
	      "    numConfigs=%d", devQualDesc->descLength,
	      devQualDesc->descType, ((devQualDesc->usbVersion & 0xFF00) >> 8),
	      (devQualDesc->usbVersion & 0xFF), devQualDesc->deviceClass,
	      devQualDesc->deviceSubClass, devQualDesc->deviceProtocol,
	      devQualDesc->maxPacketSize0, devQualDesc->numConfigs);
}


static inline void debugConfigDesc(usbConfigDesc *configDesc)
{
  kernelDebug(debug_usb, "Debug config descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%d\n"
	      "    totalLength=%d\n"
	      "    numInterfaces=%d\n"
	      "    confValue=%d\n"
	      "    confStringIdx=%d\n"
	      "    attributes=%d\n"
	      "    maxPower=%d", configDesc->descLength, configDesc->descType,
	      configDesc->totalLength, configDesc->numInterfaces,
	      configDesc->confValue, configDesc->confStringIdx,
	      configDesc->attributes, configDesc->maxPower);
}


static inline void debugInterDesc(usbInterDesc *interDesc)
{
  kernelDebug(debug_usb, "Debug inter descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%d\n"
	      "    interNum=%d\n"
	      "    altSetting=%d\n"
	      "    numEndpoints=%d\n"
	      "    interClass=%d\n"
	      "    interSubClass=%d\n"
	      "    interProtocol=%d\n"
	      "    interStringIdx=%d", interDesc->descLength,
	      interDesc->descType, interDesc->interNum, interDesc->altSetting,
	      interDesc->numEndpoints, interDesc->interClass,
	      interDesc->interSubClass, interDesc->interProtocol,
	      interDesc->interStringIdx);
}


static inline void debugEndpointDesc(usbEndpointDesc *endpointDesc)
{
  kernelDebug(debug_usb, "Debug endpoint descriptor:\n"
	      "    descLength=%d\n"
	      "    descType=%d\n"
	      "    endpntAddress=%d\n"
	      "    attributes=%d\n"
	      "    maxPacketSize=%d\n"
	      "    interval=%d", endpointDesc->descLength,
	      endpointDesc->descType, endpointDesc->endpntAddress,
	      endpointDesc->attributes, endpointDesc->maxPacketSize,
	      endpointDesc->interval);
}

static inline void debugUsbDevice(usbDevice *usbDev)
{
  kernelDebug(debug_usb, "Debug USB device:\n"
	      "    device=%p\n"
	      "    controller=%p (%d)\n"
	      "    port=%d\n"
	      "    lowSpeed=%d\n"
	      "    address=%d\n"
	      "    usbVersion=%04x\n"
	      "    classcode=%02x\n"
	      "    subClassCode=%02x\n"
	      "    protocol=%02x\n"
	      "    vendorId=%04x\n"
	      "    deviceId=%04x\n", usbDev, usbDev->controller,
	      usbDev->controller->num, usbDev->port, usbDev->lowSpeed,
	      usbDev->address, usbDev->usbVersion, usbDev->classCode,
	      usbDev->subClassCode, usbDev->protocol, usbDev->vendorId,
	      usbDev->deviceId);
}
#else
  #define debugDeviceDesc(desc) do { } while (0)
  #define debugDevQualDesc(desc) do { } while (0)
  #define debugConfigDesc(desc) do { } while (0)
  #define debugInterDesc(desc) do { } while (0)
  #define debugEndpointDesc(desc) do { } while (0)
  #define debugUsbDevice(usbDev) do { } while (0)
#endif // DEBUG

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

static usbSubClass subclass_hub[] = {
  { 0x00, "hub", DEVICECLASS_HUB, DEVICESUBCLASS_HUB_USB },
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
  { 0x09, "hub", subclass_hub },
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

static kernelLinkedList controllers;
static kernelLinkedList hubs;
static kernelLinkedList devices;
static int usbThreadId = 0;


static void usbInterrupt(void)
{
  // This is the USB interrupt handler.
 
  usbController *controller = NULL;
  void *address = NULL;
  int interruptNum = 0;

  kernelProcessorIsrEnter(address);
  kernelProcessingInterrupt = 1;

  // Which interrupt number is active?
  interruptNum = kernelPicGetActive();
  if (interruptNum < 0)
    goto out;

  //kernelDebug(debug_usb, "USB interrupt %d", interruptNum);

  // Search for the controller that's registered this interrupt number
  controller = kernelLinkedListIterStart(&controllers);
  while (controller)
    {
      if (controller->interruptNum == interruptNum)
	{
	  if (controller->interrupt)
	    controller->interrupt(controller);
	  
	  break;
	}

      controller = kernelLinkedListIterNext(&controllers);
    }
  kernelLinkedListIterEnd(&controllers);

  kernelPicEndOfInterrupt(interruptNum);

 out:
  kernelProcessingInterrupt = 0;
  kernelProcessorIsrExit(address);
}


static void deviceInfo2BusTarget(usbDevice *usbDev, kernelBusTarget *target)
{
  // Translate a device to a bus target listing

  usbClass *class = NULL;
  usbSubClass *subClass = NULL;

  class = kernelUsbGetClass(usbDev->classCode);
  if (class == NULL)
    return;

  subClass =
    kernelUsbGetSubClass(class, usbDev->subClassCode, usbDev->protocol);
  if (subClass == NULL)
    return;

  target->target =
    usbMakeTargetCode(usbDev->controller->num, usbDev->address, 0);

  target->class = kernelDeviceGetClass(subClass->systemClassCode);
  target->subClass = kernelDeviceGetClass(subClass->systemSubClassCode);
}


__attribute__((noreturn))
static void usbThread(void)
{
  usbHub *hub = NULL;

  while (1)
    {
      kernelMultitaskerYield();

      // Call applicable thread calls for all the hubs
      hub = kernelLinkedListIterStart(&hubs);
      while (hub)
	{
	  if (hub->threadCall)
	    hub->threadCall(hub);

	  hub = kernelLinkedListIterNext(&hubs);
	}
      kernelLinkedListIterEnd(&hubs);
    }
}


static int driverGetTargets(kernelBusTarget **pointer)
{
  // Generate our list of targets and return it

  static kernelBusTarget *busTargets = NULL;
  usbDevice *usbDev = NULL;
  int targetCount = 0;

  if (busTargets)
    {
      kernelFree(busTargets);
      busTargets = NULL;
    }

  if (devices.numItems)
    {
      // Allocate memory for the targets list
      busTargets = kernelMalloc(devices.numItems * sizeof(kernelBusTarget));
      if (busTargets == NULL)
	return (0);

      // Now fill up our targets list
      usbDev = kernelLinkedListIterStart(&devices);
      while (usbDev)
	{
	  deviceInfo2BusTarget(usbDev, &busTargets[targetCount++]);

	  usbDev = kernelLinkedListIterNext(&devices);
	}
      kernelLinkedListIterEnd(&devices);
    }

  *pointer = busTargets;
  return (targetCount);
}


static int driverGetTargetInfo(int target, void *pointer)
{
  // Given a target number, copy the device's USB device info into the
  // supplied memory pointer

  int status = ERR_NOSUCHENTRY;
  usbDevice *usbDev = NULL;

  usbDev = kernelLinkedListIterStart(&devices);
  while (usbDev)
    {
      if (usbMakeTargetCode(usbDev->controller->num, usbDev->address, 0) ==
	  target)
	{
	  kernelMemCopy((void *) usbDev, pointer, sizeof(usbDevice));
	  status = 0;
	  break;
	}
  
      usbDev = kernelLinkedListIterNext(&devices);
    }
  kernelLinkedListIterEnd(&devices);

  return (status);
}


static int transaction(int target, usbTransaction *trans, int numTrans)
{
  int status = 0;
  usbDevice *usbDev = NULL;

  usbDev = kernelUsbGetDevice(target);
  if (usbDev == NULL)
    return (status = ERR_NOSUCHENTRY);

  if (!usbDev->controller)
    {
      kernelError(kernel_error, "Device hub is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  if (!usbDev->controller->queue)
    {
      kernelError(kernel_error, "Hub driver cannot queue transactions");
      return (status = ERR_NOTIMPLEMENTED);
    }

  status =
    usbDev->controller->queue(usbDev->controller, usbDev, trans, numTrans);

  return (status);
}


static int driverDetect(void *parent __attribute__((unused)),
			kernelDriver *driver)
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
  usbController *controller = NULL;

  kernelMemClear(&controllers, sizeof(kernelLinkedList));
  kernelMemClear(&hubs, sizeof(kernelLinkedList));
  kernelMemClear(&devices, sizeof(kernelLinkedList));

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
  for (deviceCount = 0; deviceCount < numPciTargets; deviceCount ++)
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

      // See if it's an OHCI controller
      else if ((dev = kernelUsbOhciDetect(pciDevice, &pciTargets[deviceCount],
					  driver)))
	; // empty

      // See if it's an EHCI controller
      else if ((dev = kernelUsbEhciDetect(pciDevice, &pciTargets[deviceCount],
					  driver)))
	; // empty

      else
	// Not a supported USB controller
	continue;

      controller = dev->data;
      controller->device = dev;
      controller->num = controllers.numItems;

      // Register the interrupt handler
      status = kernelInterruptHook(controller->interruptNum, &usbInterrupt);
      if (status < 0)
	break;

      // Turn on the interrupt
      kernelPicMask(controller->interruptNum, 1);

      status = kernelBusRegister(bus_usb, dev);
      if (status < 0)
	break;

      // Add it to our list of controllers
      status = kernelLinkedListAdd(&controllers, (void *) controller);
      if (status < 0)
	break;

      // Also add the controller's hub to our list of hubs
      status = kernelLinkedListAdd(&hubs, (void *) &controller->hub);
      if (status < 0)
	break;
    }

  kernelFree(pciTargets);
  return (status);
}


static int driverWrite(int target, unsigned size, void *params)
{
  // A wrapper for the read/write function
  return (transaction(target, (usbTransaction *) params,
		      (size / sizeof(usbTransaction))));
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


void kernelUsbDriverRegister(kernelDriver *driver)
{
   // Device driver registration.

  driver->driverDetect = driverDetect;
  driver->ops = &usbOps;

  return;
}


int kernelUsbInitialize(void)
{
  // This gets called after multitasking is enabled.

  if (controllers.numItems)
    usbThreadId =
      kernelMultitaskerSpawnKernelThread(usbThread, "usb thread", 0, NULL);

  return (0);
}


int kernelUsbShutdown(void)
{
  // Called at shutdown.  We do a reset of all registered controllers so there
  // won't be remnants of transactions on the buses messing things up (for
  // example if we're doing a soft reboot)

  usbController *controller = NULL;

  controller = kernelLinkedListIterStart(&controllers);
  while (controller)
    {
      if (controller->reset)
	controller->reset(controller);

      controller = kernelLinkedListIterNext(&controllers);
    }
  kernelLinkedListIterEnd(&controllers);

  return (0);
}


usbClass *kernelUsbGetClass(int classCode)
{
  // Return the USB class, given the class code

  int count;

  for (count = 0; count < 256; count++)
    {	
      // If no more classcodes are in the list
      if (usbClasses[count].classCode == USB_INVALID_CLASSCODE)
	return (NULL);
		
      // If valid classcode is found
      if (usbClasses[count].classCode == classCode)
	return (&usbClasses[count]);
    }

  return (NULL);
}


usbSubClass *kernelUsbGetSubClass(usbClass *class, int subClassCode,
				  int protocol)
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
	    return (NULL);

	  if (class->subClasses[count].subClassCode == subClassCode)
	    return (&class->subClasses[count]);
	}
    }

  return (NULL);
}


int kernelUsbGetClassName(int classCode, int subClassCode, int protocol,
			  char **className, char **subClassName)
{
  // Returns name of the class and the subclass in human readable format.
  // Buffers classname and subclassname have to provide

  int status = 0;
  usbClass *class = NULL;
  usbSubClass *subClass = NULL;

  class = kernelUsbGetClass(classCode);
  if (class == NULL)
    {
      *className = "unknown device";
      *subClassName = "";
      return (status = USB_INVALID_CLASSCODE);
    }

  *className = (char *) class->name;

  subClass = kernelUsbGetSubClass(class, subClassCode, protocol);
  if (subClass == NULL)
    {
      *subClassName = "";
      return (status = USB_INVALID_SUBCLASSCODE);
    }

  *subClassName = (char *) subClass->name;
  return (status = 0);
}


int kernelUsbDevConnect(usbController *controller, usbHub *hub, int port,
			int lowSpeed)
{
  // Enumerate a new device in respose to a port connection by sending out
  // a 'set address' transaction and getting device information

  int status = 0;
  usbDevice *usbDev = NULL;
  unsigned bytes = 0;
  usbConfigDesc *tmpConfigDesc = NULL;
  char *className = NULL;
  char *subClassName = NULL;
  usbClass *class = NULL;
  usbSubClass *subClass = NULL;
  void *ptr = NULL;
  int count1, count2;

  kernelDebug(debug_usb, "Device connection on controller %d hub %p port %d",
	      controller->num, hub, port);

  usbDev = kernelMalloc(sizeof(usbDevice));
  if (usbDev == NULL)
    return (status = ERR_MEMORY);

  usbDev->controller = controller;
  usbDev->port = port;
  usbDev->lowSpeed = lowSpeed;

  // Try to set a device address.  Make a couple of attempts.
  for (count1 = 0; count1 < 3; count1 ++)
    {
      kernelDebug(debug_usb, "Set address %d for new device %p",
		  (controller->addressCounter + 1), usbDev);
      status =
	kernelUsbControlTransfer(usbDev, USB_SET_ADDRESS,
				 (controller->addressCounter + 1), 0, 0, NULL,
				 NULL);

      // We're supposed to allow a 2ms delay for the device after the set
      // address command.
      kernelDebug(debug_usb, "Delay after set_address");
      kernelSysTimerWaitTicks(1);

      if (status >= 0)
	break;
    }

  if (status < 0)
    // No device waiting for an address, we guess
    goto err_out;

  usbDev->address = (controller->addressCounter + 1);

  // Try getting a device descriptor of only 8 bytes.  Thereafter we will
  // *know* the supported packet size.
  kernelDebug(debug_usb, "Get short device descriptor for new device %d",
	      usbDev->address);
  status = kernelUsbControlTransfer(usbDev, USB_GET_DESCRIPTOR,
				    (USB_DESCTYPE_DEVICE << 8), 0, 8,
				    (void *) &(usbDev->deviceDesc), NULL);
  if (status < 0)
    {
      kernelError(kernel_error, "Error getting device descriptor");
      goto err_out;
    }

  debugDeviceDesc((usbDeviceDesc *) &(usbDev->deviceDesc));

  // Now get the whole descriptor
  kernelDebug(debug_usb, "Get full device descriptor for new device %d",
	      usbDev->address);
  status = kernelUsbControlTransfer(usbDev, USB_GET_DESCRIPTOR,
				    (USB_DESCTYPE_DEVICE << 8),
				    0, sizeof(usbDeviceDesc),
				    (void *) &(usbDev->deviceDesc), NULL);
  if (status < 0)
    {
      kernelError(kernel_error, "Error getting device descriptor");
      goto err_out;
    }

  debugDeviceDesc((usbDeviceDesc *) &(usbDev->deviceDesc));

  // If the device is a USB 2.0+ device, see if it's got a 'device qualifier'
  // descriptor
  if (usbDev->deviceDesc.usbVersion >= 0x0200)
    {
      kernelDebug(debug_usb, "Get device qualifier for new device %d",
		  usbDev->address);
      bytes = 0;
      kernelUsbControlTransfer(usbDev, USB_GET_DESCRIPTOR,
			       (USB_DESCTYPE_DEVICEQUAL << 8), 0,
			       sizeof(usbDevQualDesc),
			       (void *) &(usbDev->devQualDesc), &bytes);
      if (bytes)
	debugDevQualDesc((usbDevQualDesc *) &(usbDev->devQualDesc));
    }

  usbDev->usbVersion = usbDev->deviceDesc.usbVersion;
  usbDev->classCode = usbDev->deviceDesc.deviceClass;
  usbDev->subClassCode = usbDev->deviceDesc.deviceSubClass;
  usbDev->protocol = usbDev->deviceDesc.deviceProtocol;
  usbDev->deviceId = usbDev->deviceDesc.productId;
  usbDev->vendorId = usbDev->deviceDesc.vendorId;

  // Get the first configuration, which includes interface and endpoint
  // descriptors.  The initial attempt must be limited to the max packet
  // size for endpoint zero.

  tmpConfigDesc = kernelMalloc(usbDev->deviceDesc.maxPacketSize0);
  if (tmpConfigDesc == NULL)
    {
      status = ERR_MEMORY;
      goto err_out;
    }

  kernelDebug(debug_usb, "Get short first configuration for new device %d",
	      usbDev->address);
  bytes = 0;
  status = kernelUsbControlTransfer(usbDev, USB_GET_DESCRIPTOR,
				    (USB_DESCTYPE_CONFIG << 8), 0,
				    min(usbDev->deviceDesc.maxPacketSize0,
					sizeof(usbConfigDesc)),
				    tmpConfigDesc, &bytes);
  if ((status < 0) &&
      (bytes < min(usbDev->deviceDesc.maxPacketSize0, sizeof(usbConfigDesc))))
    goto err_out;

  // Now that we know the total size of the configuration information, if
  // it is bigger than the max packet size, do a second request that gets
  // all the data.

  if (tmpConfigDesc->totalLength >
      min(usbDev->deviceDesc.maxPacketSize0, sizeof(usbConfigDesc)))
    {
      usbDev->configDesc = kernelMalloc(tmpConfigDesc->totalLength);
      if (usbDev->configDesc == NULL)
	{
	  status = ERR_MEMORY;
	  goto err_out;
	}

      kernelDebug(debug_usb, "Get full first configuration for new device %d",
		  usbDev->address);
      bytes = 0;
      status = kernelUsbControlTransfer(usbDev, USB_GET_DESCRIPTOR,
					(USB_DESCTYPE_CONFIG << 8), 0,
					tmpConfigDesc->totalLength,
					usbDev->configDesc, &bytes);
      if ((status < 0) && (bytes < tmpConfigDesc->totalLength))
	goto err_out;

      kernelFree(tmpConfigDesc);
      tmpConfigDesc = NULL;
    }
  else
    {
      usbDev->configDesc = tmpConfigDesc;
      tmpConfigDesc = NULL;
    }

  debugConfigDesc(usbDev->configDesc);

  // Set the configuration
  kernelDebug(debug_usb, "Set configuration for new device %d",
	      usbDev->address);
  status =
    kernelUsbControlTransfer(usbDev, USB_SET_CONFIGURATION,
			     usbDev->configDesc->confValue, 0, 0, NULL, NULL);
  if (status < 0)
    goto err_out;

  ptr = ((void *) usbDev->configDesc + usbDev->configDesc->descLength);
  for (count1 = 0; ((count1 < usbDev->configDesc->numInterfaces) &&
		    (count1 < USB_MAX_INTERFACES)); )
    {
      if (ptr >= ((void *) usbDev->configDesc +
		  usbDev->configDesc->totalLength))
	break;

      usbDev->interDesc[count1] = ptr;
      if (usbDev->interDesc[count1]->descType != USB_DESCTYPE_INTERFACE)
	{
	  ptr += usbDev->interDesc[count1]->descLength;
	  continue;
	}

      if (!count1 && !usbDev->classCode)
	{
	  usbDev->classCode = usbDev->interDesc[count1]->interClass;
	  usbDev->subClassCode = usbDev->interDesc[count1]->interSubClass;
	  usbDev->protocol = usbDev->interDesc[count1]->interProtocol;
	}

      debugInterDesc(usbDev->interDesc[count1]);

      ptr += usbDev->interDesc[count1]->descLength;

      for (count2 = 0; ((count2 < usbDev->interDesc[count1]->numEndpoints) &&
			(usbDev->numEndpoints < USB_MAX_ENDPOINTS)); )
	{
	  if (ptr >= ((void *) usbDev->configDesc +
		      usbDev->configDesc->totalLength))
	    break;

	  usbDev->endpointDesc[usbDev->numEndpoints] = ptr;
	  if (usbDev->endpointDesc[usbDev->numEndpoints]->descType !=
	      USB_DESCTYPE_ENDPOINT)
	    {
	      ptr += usbDev->endpointDesc[usbDev->numEndpoints]->descLength;
	      continue;
	    }

	  debugEndpointDesc(usbDev->endpointDesc[usbDev->numEndpoints]);

	  ptr += sizeof(usbEndpointDesc);
	  usbDev->numEndpoints += 1;
	  count2 += 1;
	}

      count1 += 1;
    }

  // Ok, we will add this device.

  status = kernelLinkedListAdd(&devices, (void *) usbDev);
  if (status < 0)
    return (status);

  status =
    kernelLinkedListAdd((kernelLinkedList *) &hub->devices, (void *) usbDev);
  if (status < 0)
    return (status);

  controller->addressCounter += 1;

  kernelUsbGetClassName(usbDev->classCode, usbDev->subClassCode,
			usbDev->protocol, &className, &subClassName);

  kernelLog("USB: %s %s %u:%u dev:%04x, vend:%04x, class:%02x, "
	    "sub:%02x proto:%02x usb:%x.%x", subClassName, className,
	    usbDev->controller->num, usbDev->address, usbDev->deviceId,
	    usbDev->vendorId, usbDev->classCode, usbDev->subClassCode,
	    usbDev->protocol, ((usbDev->usbVersion & 0xFF00) >> 8),
	    (usbDev->usbVersion & 0xFF));

  // See about calling the appropriate hotplug detection functions of the
  // appropriate drivers
  class = kernelUsbGetClass(usbDev->classCode);
  subClass =
    kernelUsbGetSubClass(class, usbDev->subClassCode, usbDev->protocol);

  if (subClass)
    {
      status =
	kernelDeviceHotplug(controller->device, subClass->systemSubClassCode,
			    bus_usb, usbMakeTargetCode(usbDev->controller->num,
						       usbDev->address, 0), 1);
      if (status < 0)
	return (status);

      // If the device is a hub, add it to our list of hubs
      if ((usbDev->classCode == 0x09) && (usbDev->subClassCode == 0x00))
	{
	  status = kernelLinkedListAdd(&hubs, usbDev->data);
	  if (status < 0)
	    return (status);
	}
    }

  debugUsbDevice(usbDev);
  kernelDebug(debug_usb, "%d controllers, %d hubs, %d devices",
	      controllers.numItems, hubs.numItems, devices.numItems);
  return (status = 0);

 err_out:

  if (tmpConfigDesc)
    kernelFree(tmpConfigDesc);

  if (usbDev->configDesc)
    kernelFree(usbDev->configDesc);

  if (usbDev)
    kernelFree((void *) usbDev);

  return (status);
}


void kernelUsbDevDisconnect(usbController *controller, usbHub *hub, int port)
{
  // If the port status(es) indicate that a device has disconnected, figure
  // out which one it is and remove it from the root hub's list

  usbDevice *usbDev = NULL;
  usbClass *class = NULL;
  usbSubClass *subClass = NULL;
  
  kernelDebug(debug_usb, "Device disconnection on controller %d hub %p port "
	      "%d", controller->num, hub, port);
  kernelDebug(debug_usb, "Hub %p has %d devices", hub, hub->devices.numItems);

  // Try to find the device
  usbDev = kernelLinkedListIterStart((kernelLinkedList *) &hub->devices);
  while (usbDev)
    {
      if (usbDev->port == port)
	{
	  kernelDebug(debug_usb, "Device %d disconnected %p", usbDev->address,
		      usbDev);
	  debugUsbDevice(usbDev);

	  class = kernelUsbGetClass(usbDev->classCode);
	  subClass = kernelUsbGetSubClass(class, usbDev->subClassCode,
					  usbDev->protocol);
	
	  if (subClass)
	    // Tell the device hotplug function that the device has
	    // disconnected
	    kernelDeviceHotplug(controller->device,
				subClass->systemSubClassCode, bus_usb,
				usbMakeTargetCode(controller->num,
						  usbDev->address, 0), 0);

	  // Remove it from the device list.
	  kernelLinkedListRemove(&devices, (void *) usbDev);

	  // Remove it from the hub's list
	  kernelLinkedListRemove((kernelLinkedList *) &hub->devices,
				 (void *) usbDev);

	  // If the device was a hub, remove it from our list of hubs
	  if ((usbDev->classCode == 0x09) && (usbDev->subClassCode == 0x00))
	    kernelLinkedListRemove(&hubs, (void *) usbDev->data);

	  // Free the device memory
	  if (usbDev->configDesc)
	    kernelFree(usbDev->configDesc);
	  kernelFree((void *) usbDev);

	  break;
	}

      usbDev = kernelLinkedListIterNext((kernelLinkedList *) &hub->devices);
    }
  kernelLinkedListIterEnd((kernelLinkedList *) &hub->devices);

  kernelDebug(debug_usb, "%d controllers, %d hubs, %d devices",
	      controllers.numItems, hubs.numItems, devices.numItems);
  return;
}


usbDevice *kernelUsbGetDevice(int target)
{
  int controllerNum = 0;
  int address = 0;
  int endpoint = 0;
  usbDevice *tmpUsbDev = NULL;
  usbDevice *usbDev = NULL;

  // Break out the target information
  usbMakeContAddrEndp(target, controllerNum, address, endpoint);

  // Try to find the device
  tmpUsbDev = kernelLinkedListIterStart(&devices);
  while(tmpUsbDev)
    {
      if ((tmpUsbDev->controller->num == controllerNum) &&
	  (tmpUsbDev->address == address))
	{
	  usbDev = tmpUsbDev;
	  break;
	}
      
      tmpUsbDev = kernelLinkedListIterNext(&devices);
    }
  kernelLinkedListIterEnd(&devices);

  if (usbDev == NULL)
    kernelError(kernel_error, "No such device %d", address);

  return (usbDev);  
}


int kernelUsbControlTransfer(usbDevice *usbDev, unsigned char request,
			     unsigned short value, unsigned short index,
			     unsigned short length, void *buffer,
			     unsigned *bytes)
{
  // This is a convenience function for doing a control transfer, so that
  // callers (i.e. device drivers) don't have to construct a usbTransaction
  // structure manually.
  
  int status = 0;
  usbTransaction trans;

  kernelDebug(debug_usb, "Control transfer of %d bytes for address %d",
	      length, usbDev->address);

  kernelMemClear((void *) &trans, sizeof(usbTransaction));
  trans.type = usbxfer_control;
  trans.address = usbDev->address;
  trans.endpoint = 0;
  trans.control.request = request;
  trans.control.value = value;
  trans.control.index = index;
  trans.length = length;
  trans.buffer = buffer;

  if (!usbDev->controller)
    {
      kernelError(kernel_error, "Device controller is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  if (!usbDev->controller->queue)
    {
      kernelError(kernel_error, "Controller cannot queue transactions");
      return (status = ERR_NOTIMPLEMENTED);
    }

  status = usbDev->controller->queue(usbDev->controller, usbDev, &trans, 1);

  if (bytes)
    *bytes = trans.bytes;

  return (status);
}


int kernelUsbScheduleInterrupt(usbDevice *usbDev, unsigned char endpoint,
			       int interval, unsigned maxLen,
			       void (*callback)(usbDevice *, void *, unsigned))
{
  // This is a function for scheduling a periodic interrupt transfer for a
  // device, with a callback to the caller (i.e. a device driver).

  int status = 0;

  if (!usbDev->controller)
    {
      kernelError(kernel_error, "Device hub is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  if (!usbDev->controller->schedInterrupt)
    {
      kernelError(kernel_error, "Controller cannot schedule interrupts");
      return (status = ERR_NOTIMPLEMENTED);
    }

  if (interval < 1)
    {
      kernelError(kernel_error, "Interrupt intervals must be 1 or greater");
      return (status = ERR_RANGE);
    }

  status =
    usbDev->controller->schedInterrupt(usbDev->controller, usbDev, endpoint,
				       interval, maxLen, callback);
  return (status);
}


int kernelUsbUnscheduleInterrupt(usbDevice *usbDev)
{
  // Remove any previously scheduled interrupts.

  int status = 0;

  if (!usbDev->controller)
    {
      kernelError(kernel_error, "Device controller is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  if (!usbDev->controller->schedInterrupt)
    {
      kernelError(kernel_error, "Controller cannot schedule interrupts");
      return (status = ERR_NOTIMPLEMENTED);
    }

  if (!usbDev->controller->unschedInterrupt)
    {
      kernelError(kernel_error, "Controller cannot schedule interrupts");
      return (status = ERR_NOTIMPLEMENTED);
    }

  status = usbDev->controller->unschedInterrupt(usbDev->controller, usbDev);
  return (status);
}
