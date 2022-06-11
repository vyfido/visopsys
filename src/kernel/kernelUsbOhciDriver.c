//
//  Visopsys
//  Copyright (C) 1998-2015 J. Andrew McLaughlin
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
//  kernelUsbOhciDriver.c
//

#include "kernelUsbDriver.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelPciDriver.h"
#include "kernelVariableList.h"
#include <string.h>

#define USBOHCI_PCI_PROGIF	0x10


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelDevice *kernelUsbOhciDetect(kernelBusTarget *busTarget,
	kernelDriver *driver)
{
	// This routine is used to detect OHCI USB controllers, as well as
	// registering it with the higher-level interfaces.

	// This version is just a stub, to show that the device has been detected.
	// The real driver will probably never be written, since these are fairly
	// obsolete.

	int status = 0;
	pciDeviceInfo pciDevInfo;
	kernelDevice *dev = NULL;
	usbController *controller = NULL;

	// Get the PCI device header
	status = kernelBusGetTargetInfo(busTarget, &pciDevInfo);
	if (status < 0)
		goto err_out;

	// Don't care about the 'multi-function' bit in the header type
	if (pciDevInfo.device.headerType & PCI_HEADERTYPE_MULTIFUNC)
		pciDevInfo.device.headerType &= ~PCI_HEADERTYPE_MULTIFUNC;

	// Make sure it's a non-bridge header
	if (pciDevInfo.device.headerType != PCI_HEADERTYPE_NORMAL)
	{
		kernelDebug(debug_usb, "OHCI headertype not 'normal' (%d)",
			pciDevInfo.device.headerType);
		goto err_out;
	}

	// Make sure it's an OHCI controller (programming interface is 0x10 in
	// the PCI header)
	if (pciDevInfo.device.progIF != USBOHCI_PCI_PROGIF)
		goto err_out;

	// After this point, we believe we have a supported device.

	// Allocate memory for the device
	dev = kernelMalloc(sizeof(kernelDevice));
	if (!dev)
		goto err_out;

	// Allocate memory for the controller
	controller = kernelMalloc(sizeof(usbController));
	if (!controller)
		goto err_out;

	// Set the controller type
	controller->type = usb_ohci;

	// The USB version number.  Fake this.
	controller->usbVersion = 0x10;

	// Get the interrupt number.
	controller->interruptNum = pciDevInfo.device.nonBridge.interruptLine;

	kernelLog("USB: OHCI controller interrupt %d", controller->interruptNum);

	// Create the USB kernel device
	dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
	dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB);
	dev->driver = driver;
	dev->data = (void *) controller;

	// Initialize the variable list for attributes of the controller
	status = kernelVariableListCreate(&dev->device.attrs);
	if (status >= 0)
		kernelVariableListSet(&dev->device.attrs, "controller.type", "OHCI");

	// Claim the controller device in the list of PCI targets.
	kernelBusDeviceClaim(busTarget, driver);

	// Add the kernel device
	status = kernelDeviceAdd(busTarget->bus->dev, dev);
	if (status < 0)
		goto err_out;
	else
		return (dev);

err_out:
	if (dev)
		kernelFree(dev);
	if (controller)
		kernelFree((void *) controller);

  return (dev = NULL);
}

