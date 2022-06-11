//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  kernelUsbDriver.c
//

#include "kernelUsbDriver.h"
#include "kernelCpu.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelInterrupt.h"
#include "kernelLinkedList.h"
#include "kernelLocale.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelPic.h"
#include "kernelProcessorX86.h"
#include "kernelUsbHubDriver.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define _(string) kernelGetText(string)

#ifdef DEBUG
static inline void debugDeviceDesc(usbDeviceDesc *deviceDesc)
{
	kernelDebug(debug_usb, "USB debug device descriptor:\n"
		"    descLength=%d\n"
		"    descType=%d\n"
		"    usbVersion=%d.%d\n"
		"    deviceClass=0x%02x\n"
		"    deviceSubClass=0x%02x\n"
		"    deviceProtocol=0x%02x\n"
		"    maxPacketSize0=%d\n"
		"    vendorId=0x%04x\n"
		"    productId=0x%04x\n"
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
	kernelDebug(debug_usb, "USB debug device qualifier descriptor:\n"
		"    descLength=%d\n"
		"    descType=%d\n"
		"    usbVersion=%d.%d\n"
		"    deviceClass=0x%02x\n"
		"    deviceSubClass=0x%02x\n"
		"    deviceProtocol=0x%02x\n"
		"    maxPacketSize0=%d\n"
		"    numConfigs=%d", devQualDesc->descLength,
		devQualDesc->descType, ((devQualDesc->usbVersion & 0xFF00) >> 8),
		(devQualDesc->usbVersion & 0xFF), devQualDesc->deviceClass,
		devQualDesc->deviceSubClass, devQualDesc->deviceProtocol,
		devQualDesc->maxPacketSize0, devQualDesc->numConfigs);
}


static inline void debugConfigDesc(usbConfigDesc *configDesc)
{
	kernelDebug(debug_usb, "USB debug config descriptor:\n"
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
	kernelDebug(debug_usb, "USB debug inter descriptor:\n"
		"    descLength=%d\n"
		"    descType=%d\n"
		"    interNum=%d\n"
		"    altSetting=%d\n"
		"    numEndpoints=%d\n"
		"    interClass=0x%02x\n"
		"    interSubClass=0x%02x\n"
		"    interProtocol=0x%02x\n"
		"    interStringIdx=%d", interDesc->descLength,
		interDesc->descType, interDesc->interNum, interDesc->altSetting,
		interDesc->numEndpoints, interDesc->interClass,
		interDesc->interSubClass, interDesc->interProtocol,
		interDesc->interStringIdx);
}


static inline void debugEndpointDesc(usbEndpointDesc *endpointDesc)
{
	kernelDebug(debug_usb, "USB debug endpoint descriptor:\n"
		"    descLength=%d\n"
		"    descType=%d\n"
		"    endpntAddress=0x%02x\n"
		"    attributes=%d\n"
		"    maxPacketSize=%d\n"
		"    interval=%d", endpointDesc->descLength,
		endpointDesc->descType, endpointDesc->endpntAddress,
		endpointDesc->attributes, endpointDesc->maxPacketSize,
		endpointDesc->interval);
}

static inline void debugUsbDevice(usbDevice *usbDev)
{
	kernelDebug(debug_usb, "USB debug device:\n"
		"    device=%p\n"
		"    controller=%p (%d)\n"
		"    port=%d\n"
		"    speed=%s\n"
		"    address=%d\n"
		"    usbVersion=%d.%d\n"
		"    classcode=0x%02x\n"
		"    subClassCode=0x%02x\n"
		"    protocol=0x%02x\n"
		"    vendorId=0x%04x\n"
		"    deviceId=0x%04x", usbDev, usbDev->controller,
		usbDev->controller->num, usbDev->port,
		usbDevSpeed2String(usbDev->speed), usbDev->address,
		((usbDev->usbVersion & 0xFF00) >> 8),
		(usbDev->usbVersion & 0xFF), usbDev->classCode,
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
	{ 0x00, "USB", DEVICECLASS_HUB, DEVICESUBCLASS_HUB_USB },
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
	{ 0x0F, "personal healthcare", NULL },
	{ 0xDC, "diagnostic", NULL },
	{ 0xE0, "wireless controller", NULL },
	{ 0xEF, "miscellaneous", NULL },
	{ 0xFE, "application-specific", NULL },
	{ 0xFF, "vendor-specific", subclass_vendor },
	{ USB_INVALID_CLASSCODE, "", NULL }
};

static kernelLinkedList controllerList;
static kernelLinkedList hubList;
static kernelLinkedList deviceList;
static int usbThreadId = 0;
static void *oldIntHandlers[INTERRUPT_VECTORS];


static void usbInterrupt(void)
{
	// This is the USB interrupt handler.

	void *address = NULL;
	int interruptNum = 0;
	usbController *controller = NULL;
	kernelLinkedListItem *iter = NULL;
	int serviced = 0;

	kernelProcessorIsrEnter(address);

	// Which interrupt number is active?
	interruptNum = kernelPicGetActive();
	if (interruptNum < 0)
		goto out;

	kernelInterruptSetCurrent(interruptNum);

	//kernelDebug(debug_usb, "USB interrupt %d", interruptNum);

	// Search for the controller that's registered with this interrupt number.
	controller = kernelLinkedListIterStart(&controllerList, &iter);
	while (controller && !serviced)
	{
		if (controller->interruptNum == interruptNum)
		{
			//kernelDebug(debug_usb, "USB try controller %d", controller->num);
			if (controller->interrupt)
			{
				// See whether this controller is interrupting.  If not, it
				// must return the 'no data' error code.
				if (controller->interrupt(controller) != ERR_NODATA)
				{
					//kernelDebug(debug_usb, "USB interrupt serviced");
					serviced = 1;
				}
			}
		}

		controller = kernelLinkedListIterNext(&controllerList, &iter);
	}

	if (serviced)
		kernelPicEndOfInterrupt(interruptNum);

	kernelInterruptClearCurrent();

	if (!serviced)
	{
		if (oldIntHandlers[interruptNum])
		{
			// We didn't service this interrupt, and we're sharing this PCI
			// interrupt with another device whose handler we saved.  Call it.
			kernelDebug(debug_usb, "USB interrupt not serviced - chaining");
			kernelProcessorIsrCall(oldIntHandlers[interruptNum]);
		}
		else
			kernelDebugError("Interrupt not serviced and no saved ISR");
	}

out:

	kernelProcessorIsrExit(address);
}


__attribute__((noreturn))
static void usbThread(void)
{
	kernelLinkedListItem *iter = NULL;
	usbHub *hub = NULL;

	while (1)
	{
		kernelMultitaskerYield();

		// Call applicable thread calls for all the hubs
		hub = kernelLinkedListIterStart(&hubList, &iter);
		while (hub)
		{
			if (hub->threadCall)
				hub->threadCall(hub);

			hub = kernelLinkedListIterNext(&hubList, &iter);
		}
	}
}


static void deviceInfo2BusTarget(usbDevice *usbDev, kernelBusTarget *target)
{
	// Translate a device to a bus target listing

	usbClass *class = NULL;
	usbSubClass *subClass = NULL;

	class = kernelUsbGetClass(usbDev->classCode);
	if (class == NULL)
	{
		kernelDebugError("Target %p - no device class", target);
		return;
	}

	subClass =
		kernelUsbGetSubClass(class, usbDev->subClassCode, usbDev->protocol);

	target->bus = usbDev->controller->bus;
	target->id = usbMakeTargetCode(usbDev->controller->num, usbDev->address, 0);
	if (subClass)
	{
		target->class = kernelDeviceGetClass(subClass->systemClassCode);
		target->subClass = kernelDeviceGetClass(subClass->systemSubClassCode);
	}

	target->claimed = usbDev->claimed;
}


static int driverGetTargets(kernelBus *bus, kernelBusTarget **pointer)
{
	// Generate the list of targets that reside on the given bus (controller).

	kernelLinkedListItem *iter = NULL;
	usbDevice *usbDev = NULL;
	int targetCount = 0;
	kernelBusTarget *busTargets = NULL;

	// Count the number of USB devices attached to the controller that owns
	// this bus
	usbDev = kernelLinkedListIterStart(&deviceList, &iter);
	while (usbDev)
	{
		if (usbDev->controller && (usbDev->controller->bus == bus))
		{
			// (Re-)allocate memory for the targets list
			busTargets = kernelRealloc(busTargets, ((targetCount + 1) *
				sizeof(kernelBusTarget)));
			if (busTargets == NULL)
				return (targetCount = ERR_MEMORY);

			deviceInfo2BusTarget(usbDev, &busTargets[targetCount++]);
		}

		usbDev = kernelLinkedListIterNext(&deviceList, &iter);
	}

	*pointer = busTargets;
	return (targetCount);
}


static int driverGetTargetInfo(kernelBusTarget *target, void *pointer)
{
	// Given a target number, copy the device's USB device info into the
	// supplied memory pointer

	int status = ERR_NOSUCHENTRY;
	kernelLinkedListItem *iter = NULL;
	usbDevice *usbDev = NULL;

	usbDev = kernelLinkedListIterStart(&deviceList, &iter);
	while (usbDev)
	{
		if (usbMakeTargetCode(usbDev->controller->num, usbDev->address, 0) ==
			target->id)
		{
			kernelMemCopy((void *) usbDev, pointer, sizeof(usbDevice));
			status = 0;
			break;
		}
  
		usbDev = kernelLinkedListIterNext(&deviceList, &iter);
	}

	return (status);
}


static int transaction(int target, usbTransaction *trans, int numTrans)
{
	int status = 0;
	usbDevice *usbDev = NULL;

	kernelDebug(debug_usb, "USB do transaction for target 0x%08x", target);

	usbDev = kernelUsbGetDevice(target);
	if (usbDev == NULL)
	{
		kernelError(kernel_error, "No such device");
		return (status = ERR_NOSUCHENTRY);
	}

#ifdef DEBUG
	char *className = NULL, *subClassName = NULL;
	if (kernelUsbGetClassName(usbDev->classCode, usbDev->subClassCode,
			usbDev->protocol, &className, &subClassName) >= 0)
	{
		kernelDebug(debug_usb, "USB (%s %s)", subClassName, className);
	}
#endif // DEBUG

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


static int addController(kernelDevice *dev, int numControllers,
	kernelDriver *driver)
{
	int status = 0;
	usbController *controller = NULL;
	char value[32];

	controller = dev->data;
	controller->dev = dev;
	controller->num = numControllers;

	// Add any values we want in the attributes list
	sprintf(value, "%d", controller->interruptNum);
	kernelVariableListSet(&dev->device.attrs, "controller.interrupt", value);
	snprintf(value, 32, "%d.%d", ((controller->usbVersion & 0xF0) >> 4),
		(controller->usbVersion & 0xF));
	kernelVariableListSet(&dev->device.attrs, "controller.usbVersion", value);

	// Add it to our list of controllers
	status = kernelLinkedListAdd(&controllerList, (void *) controller);
	if (status < 0)
		return (status);

	kernelDebug(debug_usb, "USB %d controllers, %d hubs, %d devices",
		controllerList.numItems, hubList.numItems, deviceList.numItems);

	// Get memory for the bus service
	controller->bus = kernelMalloc(sizeof(kernelBus));
	if (controller->bus == NULL)
		return (status = ERR_MEMORY);

	controller->bus->type = bus_usb;
	controller->bus->dev = dev;
	controller->bus->ops = driver->ops;

	// Register the bus service
	status = kernelBusRegister(controller->bus);
	if (status < 0)
		return (status);

	// Save any existing handler for the interrupt we're hooking
	if (!oldIntHandlers[controller->interruptNum] &&
		(kernelInterruptGetHandler(controller->interruptNum) !=
			usbInterrupt))
		oldIntHandlers[controller->interruptNum] =
			kernelInterruptGetHandler(controller->interruptNum);

	// Register the interrupt handler
	status = kernelInterruptHook(controller->interruptNum, &usbInterrupt);
	if (status < 0)
	{
		if ((controller->interruptNum < 0) ||
			(controller->interruptNum >= INTERRUPT_VECTORS))
		{
			kernelTextPrintLine("%s", _("\n *** Unable to initialize "
				"interrupt handling for your USB controller.\n *** You may "
				"be able to temporarily disable the 'plug and play OS' "
				"setting\n *** in your BIOS setup.\n"));
		}

		return (status);
	}

	// Turn on the interrupt
	kernelPicMask(controller->interruptNum, 1);

	// Add the controller's root hub to our list of hubs.  This is the
	// last step, and will trigger cold-plugged device detection
	kernelUsbAddHub(&(controller->hub), 0 /* no hotplug */);

	return (status = 0);
}


static int driverDetect(void *parent __attribute__((unused)),
	kernelDriver *driver)
{
	// This routine is called to detect USB buses.  There are a couple of
	// different types so we call further detection routines to do the
	// actual hardware interaction.

	int status = 0;
	kernelBusTarget *pciTargets = NULL;
	int numPciTargets = 0;
	int numControllers = 0;
	int deviceCount = 0;
	kernelDevice *dev = NULL;

	kernelMemClear(&controllerList, sizeof(kernelLinkedList));
	kernelMemClear(&hubList, sizeof(kernelLinkedList));
	kernelMemClear(&deviceList, sizeof(kernelLinkedList));

	// Clear old interrupt handlers list
	kernelMemClear(&oldIntHandlers, (INTERRUPT_VECTORS * sizeof(void *)));

	// See if there are any USB controllers on the PCI bus.  This obviously
	// depends upon PCI hardware detection occurring before USB detection.

	// Search the PCI bus(es) for devices
	numPciTargets = kernelBusGetTargets(bus_pci, &pciTargets);
	if (numPciTargets <= 0)
		return (status = numPciTargets);

	// Search the PCI bus targets for USB controllers

	// We must do EHCI controllers before UHCI controllers, as they need to
	// detect non-high-speed devices and release port ownership, so that the
	// UHCI companion controllers will detect them afterwards.
	for (deviceCount = 0; deviceCount < numPciTargets; deviceCount ++)
	{
		// If it's not a USB device, skip it
		if ((pciTargets[deviceCount].class == NULL) ||
			(pciTargets[deviceCount].class->class != DEVICECLASS_BUS) ||
			(pciTargets[deviceCount].subClass == NULL) ||
			(pciTargets[deviceCount].subClass->class != DEVICESUBCLASS_BUS_USB))
			continue;

		// See if it's an EHCI controller
		if ((dev = kernelUsbEhciDetect(&pciTargets[deviceCount], driver)))
		{
			// Add the controller
			status = addController(dev, numControllers++, driver);
			if (status < 0)
				kernelError(kernel_warn, "Couldn't add USB controller");		
		}
	}

	// Now do the rest
	for (deviceCount = 0; deviceCount < numPciTargets; deviceCount ++)
	{
		// If it's not a USB device, skip it
		if ((pciTargets[deviceCount].class == NULL) ||
			(pciTargets[deviceCount].class->class != DEVICECLASS_BUS) ||
			(pciTargets[deviceCount].subClass == NULL) ||
			(pciTargets[deviceCount].subClass->class != DEVICESUBCLASS_BUS_USB))
			continue;

		// See if it's an XHCI controller
		if ((dev = kernelUsbXhciDetect(&pciTargets[deviceCount], driver)))
			; // empty

		// See if it's a UHCI controller
		else if ((dev = kernelUsbUhciDetect(&pciTargets[deviceCount], driver)))
			; // empty

		// See if it's an OHCI controller
		else if ((dev = kernelUsbOhciDetect(&pciTargets[deviceCount], driver)))
			; // empty

		else
			// Not a supported USB controller
			continue;

		// Add the controller
		status = addController(dev, numControllers++, driver);
		if (status < 0)
			kernelError(kernel_warn, "Couldn't add USB controller");		
	}

	kernelFree(pciTargets);

	return (status);
}


static void driverDeviceClaim(kernelBusTarget *target, kernelDriver *driver)
{
	// Allows a driver to claim a USB bus device

	usbDevice *usbDev = NULL;

	if ((target == NULL) || (driver == NULL))
	{
		kernelError(kernel_error, "Target or driver is NULL");
		return;
	}

	// Find the USB device using the ID
	usbDev = kernelUsbGetDevice(target->id);
	if (usbDev == NULL)
		return;

	kernelDebug(debug_usb, "USB target 0x%08x claimed", target->id);
	usbDev->claimed = driver;

	return;
}


static int driverWrite(kernelBusTarget *target, unsigned size, void *params)
{
	// A wrapper for the 'transaction' function

	kernelDebug(debug_usb, "USB driver write");

	if ((target == NULL) || (params == NULL))
	{
		kernelError(kernel_error, "Target or params are NULL");
		return (ERR_NULLPARAMETER);
	}

	return (transaction(target->id, (usbTransaction *) params,
		(size / sizeof(usbTransaction))));
}


// Our driver operations structure.
static kernelBusOps usbOps = {
	driverGetTargets,
	driverGetTargetInfo,
	NULL, // driverReadRegister
	NULL, // driverWriteRegister
	driverDeviceClaim,
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

	kernelLinkedListItem *iter = NULL;
	usbDevice *usbDev = NULL;

	// Loop through the devices that were detected at boot time, and see
	// whether we have any that weren't claimed by a driver
	usbDev = kernelLinkedListIterStart(&deviceList, &iter);
	while (usbDev)
	{
		if (usbDev->claimed)
			kernelDebug(debug_usb, "USB device %p claimed", usbDev);
		else
			kernelDebug(debug_usb, "USB device %p not claimed", usbDev);

		usbDev = kernelLinkedListIterNext(&deviceList, &iter);
	}

	// Spawn the USB thread
	if (controllerList.numItems)
		usbThreadId =
			kernelMultitaskerSpawnKernelThread(usbThread, "usb thread", 0,
				NULL);

	return (0);
}


int kernelUsbShutdown(void)
{
	// Called at shutdown.  We do a reset of all registered controllers so there
	// won't be remnants of transactions on the buses messing things up (for
	// example if we're doing a soft reboot)

	kernelLinkedListItem *iter = NULL;
	usbController *controller = NULL;

	controller = kernelLinkedListIterStart(&controllerList, &iter);
	while (controller)
	{
		if (controller->reset)
			controller->reset(controller);

		controller = kernelLinkedListIterNext(&controllerList, &iter);
	}

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

	if (!class)
		return (NULL);

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
				return (NULL);
			}

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

	// Check params
	if (!className || !subClassName)
	{
		kernelError(kernel_error, "className or subClassName is NULL");
		return (status = ERR_NULLPARAMETER);
	}

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
		*subClassName = "USB";
		return (status = USB_INVALID_SUBCLASSCODE);
	}

	*subClassName = (char *) subClass->name;
	return (status = 0);
}


void kernelUsbAddHub(usbHub *hub, int hotplug)
{
	if (kernelLinkedListAdd(&hubList, (void *) hub) < 0)
	{
		kernelDebugError("Couldn't add hub to list");
		return;
	}

	kernelDebug(debug_usb, "USB %d controllers, %d hubs, %d devices",
		controllerList.numItems, hubList.numItems, deviceList.numItems);

	// Do an initial device detection.  We can't assume it's OK for
	// USB devices to simply be added later when the first thread
	// call comes (for example, if we're booting from a USB stick,
	// it needs to be registered immediately)
	if (hub->detectDevices)
		hub->detectDevices(hub, hotplug);
}


int kernelUsbDevConnect(usbController *controller, usbHub *hub, int port,
	usbDevSpeed speed, int hotplug)
{
	// Enumerate a new device in respose to a port connection by assigning the
	// address, various descriptors, and setting the configuration.

	int status = 0;
	usbDevice *usbDev = NULL;
	unsigned bytes = 0;
	usbConfigDesc *tmpConfigDesc = NULL;
	char *className = NULL;
	char *subClassName = NULL;
	usbClass *class = NULL;
	usbSubClass *subClass = NULL;
	void *ptr = NULL;
	char value[80];
	int count1, count2;

	// Check params
	if ((controller == NULL) || (hub == NULL))
	{
		kernelError(kernel_error, "Controller or hub is NULL");
		return (status = ERR_NULLPARAMETER);
	}

	kernelDebug(debug_usb, "USB device connection on controller %d hub %p "
		"port %d", controller->num, hub, port);

	usbDev = kernelMalloc(sizeof(usbDevice));
	if (usbDev == NULL)
		return (status = ERR_MEMORY);

	usbDev->controller = controller;
	usbDev->hub = hub;
	usbDev->port = port;
	usbDev->speed = speed;

	// Try getting a device descriptor of only 8 bytes.  Thereafter we will
	// *know* the supported packet size.
	kernelDebug(debug_usb, "USB get short device descriptor for new device %d",
		usbDev->address);
	status = kernelUsbControlTransfer(usbDev, USB_GET_DESCRIPTOR,
		(USB_DESCTYPE_DEVICE << 8), 0, 8, (void *) &(usbDev->deviceDesc),
		NULL);
	if (status < 0)
	{
		kernelError(kernel_error, "Error getting device descriptor");
		goto err_out;
	}

	debugDeviceDesc((usbDeviceDesc *) &(usbDev->deviceDesc));

	// Try to set a device address.
	kernelDebug(debug_usb, "USB set address %d for new device %p",
		(controller->addressCounter + 1), usbDev);
	status =
		kernelUsbControlTransfer(usbDev, USB_SET_ADDRESS,
			(controller->addressCounter + 1), 0, 0, NULL, NULL);
	if (status < 0)
	{
		// No device waiting for an address, we guess
		kernelError(kernel_error, "Error setting device address");
		goto err_out;
	}

	// We're supposed to allow a 2ms delay for the device after the set
	// address command.
	kernelDebug(debug_usb, "USB delay after set_address");
	kernelCpuSpinMs(2);

	// The device is now in the 'addressed' state
	usbDev->address = (controller->addressCounter + 1);

	// Now get the whole descriptor
	kernelDebug(debug_usb, "USB get full device descriptor for new device %d",
		usbDev->address);
	status = kernelUsbControlTransfer(usbDev, USB_GET_DESCRIPTOR,
		(USB_DESCTYPE_DEVICE << 8), 0, sizeof(usbDeviceDesc),
		(void *) &(usbDev->deviceDesc), NULL);
	if (status < 0)
	{
		kernelError(kernel_error, "Error getting device descriptor");
		goto err_out;
	}

	debugDeviceDesc((usbDeviceDesc *) &(usbDev->deviceDesc));

	usbDev->usbVersion = usbDev->deviceDesc.usbVersion;
	usbDev->classCode = usbDev->deviceDesc.deviceClass;
	usbDev->subClassCode = usbDev->deviceDesc.deviceSubClass;
	usbDev->protocol = usbDev->deviceDesc.deviceProtocol;
	usbDev->deviceId = usbDev->deviceDesc.productId;
	usbDev->vendorId = usbDev->deviceDesc.vendorId;

	// Get the first configuration, which includes interface and endpoint
	// descriptors.  The initial attempt must be limited to the max packet
	// size for endpoint zero.

	if (!usbDev->deviceDesc.maxPacketSize0)
	{
		kernelError(kernel_error, "New device %d max packet size is 0",
			usbDev->address);
		status = ERR_INVALID;
		goto err_out;
	}

	tmpConfigDesc = kernelMalloc(usbDev->deviceDesc.maxPacketSize0);
	if (tmpConfigDesc == NULL)
	{
		status = ERR_MEMORY;
		goto err_out;
	}

	kernelDebug(debug_usb, "USB get short first configuration for new device %d",
		usbDev->address);
	bytes = 0;
	status = kernelUsbControlTransfer(usbDev, USB_GET_DESCRIPTOR,
		(USB_DESCTYPE_CONFIG << 8), 0,
		min(usbDev->deviceDesc.maxPacketSize0, sizeof(usbConfigDesc)),
		tmpConfigDesc, &bytes);
	if ((status < 0) || (bytes < min(usbDev->deviceDesc.maxPacketSize0,
		sizeof(usbConfigDesc))))
	{
		goto err_out;
	}

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

		kernelDebug(debug_usb, "USB get full first configuration for new "
			"device %d", usbDev->address);
		bytes = 0;
		status = kernelUsbControlTransfer(usbDev, USB_GET_DESCRIPTOR,
			(USB_DESCTYPE_CONFIG << 8), 0, tmpConfigDesc->totalLength,
			usbDev->configDesc, &bytes);
		if ((status < 0) || (bytes < tmpConfigDesc->totalLength))
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

	// There's always a control endpoint 0
	usbDev->endpoint0Desc.maxPacketSize = usbDev->deviceDesc.maxPacketSize0;
	usbDev->endpointDesc[usbDev->numEndpoints] =
		(usbEndpointDesc *) &(usbDev->endpoint0Desc);
	usbDev->numEndpoints += 1;

	ptr = ((void *) usbDev->configDesc + usbDev->configDesc->descLength);
	for (count1 = 0; ((count1 < usbDev->configDesc->numInterfaces) &&
		(count1 < USB_MAX_INTERFACES)); )
	{
		if (ptr >= ((void *) usbDev->configDesc +
			usbDev->configDesc->totalLength))
		{
			break;
		}

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
			{
				break;
			}

			usbDev->endpointDesc[usbDev->numEndpoints] = ptr;
			if (usbDev->endpointDesc[usbDev->numEndpoints]->descType !=
				USB_DESCTYPE_ENDPOINT)
			{
				ptr += usbDev->endpointDesc[usbDev->numEndpoints]->descLength;
				continue;
			}

			debugEndpointDesc(usbDev->endpointDesc[usbDev->numEndpoints]);

			// Register the endpoint address in our list of data toggles.
			usbDev->dataToggle[usbDev->numEndpoints].endpntAddress =
				usbDev->endpointDesc[usbDev->numEndpoints]->endpntAddress;

			ptr += sizeof(usbEndpointDesc);
			usbDev->numEndpoints += 1;
			count2 += 1;
		}

		count1 += 1;
	}

	// Set the configuration
	kernelDebug(debug_usb, "USB set configuration %d for new device %d",
		usbDev->configDesc->confValue, usbDev->address);
	status = kernelUsbControlTransfer(usbDev, USB_SET_CONFIGURATION,
		usbDev->configDesc->confValue, 0, 0, NULL, NULL);
	if (status < 0)
		goto err_out;

	debugUsbDevice(usbDev);

	// Ok, we will add this device.

	kernelDebug(debug_usb, "USB add device, target=0x%08x",
		usbMakeTargetCode(usbDev->controller->num, usbDev->address, 0));
	status = kernelLinkedListAdd(&deviceList, (void *) usbDev);
	if (status < 0)
		return (status);

	kernelDebug(debug_usb, "USB %d controllers, %d hubs, %d devices",
		controllerList.numItems, hubList.numItems, deviceList.numItems);

	status = kernelLinkedListAdd((kernelLinkedList *) &hub->devices,
		(void *) usbDev);
	if (status < 0)
		return (status);

	controller->addressCounter += 1;

	kernelUsbGetClassName(usbDev->classCode, usbDev->subClassCode,
		usbDev->protocol, &className, &subClassName);

	// Initialize the variable list for device attributes, and add a few
	// generic things.
	status = kernelVariableListCreate((variableList *)
		&usbDev->dev.device.attrs);
	if (status >= 0)
	{
		snprintf(value, 80, "0x%02x (%s)", usbDev->classCode, className);
		kernelVariableListSet((variableList *) &usbDev->dev.device.attrs,
			"usb.class", value);

		snprintf(value, 80, "0x%02x (%s)", usbDev->subClassCode, subClassName);
		kernelVariableListSet((variableList *) &usbDev->dev.device.attrs,
			"usb.subclass", value);

		snprintf(value, 80, "%d", (usbDev->port + 1));
		kernelVariableListSet((variableList *) &usbDev->dev.device.attrs,
			"usb.port", value);

		snprintf(value, 80, "%d", usbDev->address);
		kernelVariableListSet((variableList *) &usbDev->dev.device.attrs,
			"usb.address", value);

		kernelVariableListSet((variableList *) &usbDev->dev.device.attrs,
			"usb.speed", usbDevSpeed2String(usbDev->speed));

		snprintf(value, 80, "%d.%d", ((usbDev->usbVersion & 0xFF00) >> 8),
			(usbDev->usbVersion & 0xFF));
		kernelVariableListSet((variableList *) &usbDev->dev.device.attrs,
			"usb.version", value);
	}

	kernelLog("USB: %s %s %u:%u dev:%04x, vend:%04x, class:%02x, "
		"sub:%02x proto:%02x usb:%d.%d", subClassName, className,
		usbDev->controller->num, usbDev->address, usbDev->deviceId,
		usbDev->vendorId, usbDev->classCode, usbDev->subClassCode,
		usbDev->protocol, ((usbDev->usbVersion & 0xFF00) >> 8),
		(usbDev->usbVersion & 0xFF));

	if (hotplug)
	{
		// See about calling the appropriate hotplug detection functions of the
		// appropriate drivers
		class = kernelUsbGetClass(usbDev->classCode);
		subClass =
			kernelUsbGetSubClass(class, usbDev->subClassCode, usbDev->protocol);
  
		if (subClass)
		{
			status =
				kernelDeviceHotplug(controller->dev,
					subClass->systemSubClassCode, bus_usb,
					usbMakeTargetCode(usbDev->controller->num,
						usbDev->address, 0), 1);
			if (status < 0)
				return (status);
		}
	}

	return (status = 0);

err_out:
	if (tmpConfigDesc)
		kernelFree(tmpConfigDesc);

	if (usbDev)
	{
		if (usbDev->configDesc)
			kernelFree(usbDev->configDesc);

		kernelFree((void *) usbDev);
	}

	return (status);
}


void kernelUsbDevDisconnect(usbController *controller, usbHub *hub, int port)
{
	// If the port status(es) indicate that a device has disconnected, figure
	// out which one it is and remove it from the root hub's list

	kernelLinkedListItem *iter = NULL;
	usbDevice *usbDev = NULL;
	usbClass *class = NULL;
	usbSubClass *subClass = NULL;
  
	kernelDebug(debug_usb, "USB device disconnection on controller %d hub %p "
		"port %d", controller->num, hub, port);
	kernelDebug(debug_usb, "USB hub %p has %d devices", hub,
		hub->devices.numItems);

	// Try to find the device
	usbDev =
		kernelLinkedListIterStart((kernelLinkedList *) &hub->devices, &iter);
	while (usbDev)
	{
		if (usbDev->port == port)
		{
			kernelDebug(debug_usb, "USB device %d disconnected %p",
				usbDev->address, usbDev);
			debugUsbDevice(usbDev);

			class = kernelUsbGetClass(usbDev->classCode);
			subClass = kernelUsbGetSubClass(class, usbDev->subClassCode,
				usbDev->protocol);
	
			if (subClass)
			{
				// Tell the device hotplug function that the device has
				// disconnected
				kernelDeviceHotplug(controller->dev,
					subClass->systemSubClassCode, bus_usb,
					usbMakeTargetCode(controller->num,
					usbDev->address, 0), 0);

				// Remove it from the device list.
				kernelLinkedListRemove(&deviceList, (void *) usbDev);

				// Remove it from the hub's list
				kernelLinkedListRemove((kernelLinkedList *) &hub->devices,
					(void *) usbDev);

				// If the device was a hub, remove it from our list of hubs
				if ((usbDev->classCode == 0x09) &&
					(usbDev->subClassCode == 0x00))
				{
					kernelLinkedListRemove(&hubList, (void *) usbDev->data);
				}

				// Free the attributes list
				kernelVariableListDestroy((variableList *)
					&usbDev->dev.device.attrs);

				// Free the device memory
				if (usbDev->configDesc)
					kernelFree(usbDev->configDesc);
				kernelFree((void *) usbDev);

				break;
			}

			usbDev = kernelLinkedListIterNext((kernelLinkedList *)
				&hub->devices, &iter);
		}
	}

	kernelDebug(debug_usb, "USB %d controllers, %d hubs, %d devices",
		controllerList.numItems, hubList.numItems, deviceList.numItems);

	return;
}


usbDevice *kernelUsbGetDevice(int target)
{
	int controllerNum = 0;
	int address = 0;
	int endpoint __attribute__((unused)) = 0;
	kernelLinkedListItem *iter = NULL;
	usbDevice *tmpUsbDev = NULL;
	usbDevice *usbDev = NULL;

	// Break out the target information
	usbMakeContAddrEndp(target, controllerNum, address, endpoint);

	// Try to find the device
	tmpUsbDev = kernelLinkedListIterStart(&deviceList, &iter);
	while(tmpUsbDev)
	{
		if ((tmpUsbDev->controller->num == controllerNum) &&
			(tmpUsbDev->address == address))
		{
			usbDev = tmpUsbDev;
			break;
		}
      
		tmpUsbDev = kernelLinkedListIterNext(&deviceList, &iter);
	}

	if (usbDev == NULL)
		kernelError(kernel_error, "No such device %d", address);

	return (usbDev);  
}


usbEndpointDesc *kernelUsbGetEndpointDesc(usbDevice *dev,
	unsigned char endpntAddress)
{
	// Try to find the endpoint descriptor for the given address

	usbEndpointDesc *endpoint = NULL;
	int count;

	if (endpntAddress == 0)
		endpoint = dev->endpointDesc[0];
	else
	{
		for (count = 1; count < dev->numEndpoints; count ++)
		{
			if (dev->endpointDesc[count]->endpntAddress == endpntAddress)
			{
				endpoint = dev->endpointDesc[count];
				break;
			}
		}
	}

	return (endpoint);
}


volatile unsigned char *
kernelUsbGetEndpointDataToggle(usbDevice *dev, unsigned char endpntAddress)
{
	// Try to find the endpoint data toggle for the given address

	volatile unsigned char *toggle = NULL;
	int count;

	if (dev == NULL)
	{
		kernelError(kernel_error, "NULL device");
		return (toggle = NULL);
	}

	if (endpntAddress == 0)
		toggle = &(dev->dataToggle[0].toggle);
	else
	{
		for (count = 1; count < dev->numEndpoints; count ++)
		{
			if (dev->dataToggle[count].endpntAddress == endpntAddress)
			{
				toggle = &(dev->dataToggle[count].toggle);
				break;
			}
		}
	}

	return (toggle);
}


int kernelUsbSetupDeviceRequest(usbTransaction *trans, usbDeviceRequest *req)
{
	// Create a USB device request from the supplied USB transaction structure.

	int status = 0;
	const char *opString __attribute__((unused)) = NULL;

	if ((trans == NULL) || (req == NULL))
	{
		kernelError(kernel_error, "NULL transaction or request");
		return (status = ERR_NULLPARAMETER);
	}

	kernelDebug(debug_usb, "USB set up device request");

	kernelMemClear(req, sizeof(usbDeviceRequest));

	req->requestType = trans->control.requestType;

	// Does the request go to an endpoint?
	if (trans->endpoint)
	{
		req->requestType |= USB_DEVREQTYPE_ENDPOINT;
		// trans->endpoint = 0;
	}

	req->request = trans->control.request;
	req->value = trans->control.value;
	req->index = trans->control.index;
	req->length = trans->length;

	if (req->requestType & (USB_DEVREQTYPE_CLASS | USB_DEVREQTYPE_VENDOR))
	{
		// The request is class- or vendor-specific
		opString = "class/vendor-specific control transfer";
	}
	else
	{
		// What request are we doing?  Determine the correct requestType and
		// whether there will be a data phase, etc.
		switch (trans->control.request)
		{
			case USB_GET_STATUS:
				opString = "USB_GET_STATUS";
				req->requestType |= USB_DEVREQTYPE_DEV2HOST;
				break;
			case USB_CLEAR_FEATURE:
				opString = "USB_CLEAR_FEATURE";
				req->requestType |= USB_DEVREQTYPE_HOST2DEV;
				break;
			case USB_SET_FEATURE:
				opString = "USB_SET_FEATURE";
				req->requestType |= USB_DEVREQTYPE_HOST2DEV;
				break;
			case USB_SET_ADDRESS:
				opString = "USB_SET_ADDRESS";
				req->requestType |= USB_DEVREQTYPE_HOST2DEV;
				break;
			case USB_GET_DESCRIPTOR:
				opString = "USB_GET_DESCRIPTOR";
				req->requestType |= USB_DEVREQTYPE_DEV2HOST;
				break;
			case USB_SET_DESCRIPTOR:
				opString = "USB_SET_DESCRIPTOR";
				req->requestType |= USB_DEVREQTYPE_HOST2DEV;
				break;
			case USB_GET_CONFIGURATION:
				opString = "USB_GET_CONFIGURATION";
				req->requestType |= USB_DEVREQTYPE_DEV2HOST;
				break;
			case USB_SET_CONFIGURATION:
				opString = "USB_SET_CONFIGURATION";
				req->requestType |= USB_DEVREQTYPE_HOST2DEV;
				break;
			case USB_GET_INTERFACE:
				opString = "USB_GET_INTERFACE";
				req->requestType |= USB_DEVREQTYPE_DEV2HOST;
				break;
			case USB_SET_INTERFACE:
				opString = "USB_SET_INTERFACE";
				req->requestType |= USB_DEVREQTYPE_HOST2DEV;
				break;
			case USB_SYNCH_FRAME:
				opString = "USB_SYNCH_FRAME";
				req->requestType |= USB_DEVREQTYPE_DEV2HOST;
				break;
			// Device-class-specific ones
			case USB_MASSSTORAGE_RESET:
				opString = "USB_MASSSTORAGE_RESET";
				req->requestType |= (USB_DEVREQTYPE_HOST2DEV |
					USB_DEVREQTYPE_CLASS |
					USB_DEVREQTYPE_INTERFACE);
			break;
			default:
				// Perhaps some thing we don't know about.  Try to proceed
				// anyway.
				opString = "unknown control transfer";
				break;
		}
	}

	if (req->requestType & USB_DEVREQTYPE_DEV2HOST)
		trans->pid = USB_PID_IN;
	else
		trans->pid = USB_PID_OUT;

	kernelDebug(debug_usb, "USB do %s for address %d:%02x", opString,
		trans->address, trans->endpoint);
	kernelDebug(debug_usb, "USB type=%02x, req=%02x, value=%02x, "
		"index=%02x, length=%02x", req->requestType, req->request,
		req->value, req->index, req->length);

	return (status = 0);
}


int kernelUsbControlTransfer(usbDevice *usbDev, unsigned char request,
	unsigned short value, unsigned short index, unsigned short length,
	void *buffer, unsigned *bytes)
{
	// This is a convenience function for doing a control transfer, so that
	// callers (i.e. device drivers) don't have to construct a usbTransaction
	// structure manually.
  
	int status = 0;
	usbTransaction trans;

	// Check params
	if ((usbDev == NULL) || (usbDev->controller == NULL))
	{
		kernelError(kernel_error, "Device or controller is NULL");
		return (status = ERR_NULLPARAMETER);
	}

	if (!usbDev->controller->queue)
	{
		kernelError(kernel_error, "Controller cannot queue transactions");
		return (status = ERR_NOTIMPLEMENTED);
	}

	kernelDebug(debug_usb, "USB control transfer of %d bytes for address %d",
		length, usbDev->address);

	kernelMemClear((void *) &trans, sizeof(usbTransaction));
	trans.type = usbxfer_control;
	trans.address = usbDev->address;
	// trans.endpoint = 0; <- no need to re-clear it
	trans.control.request = request;
	trans.control.value = value;
	trans.control.index = index;
	trans.length = length;
	trans.buffer = buffer;

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
