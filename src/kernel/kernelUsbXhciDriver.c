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
//  kernelUsbXhciDriver.c
//

#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelPciDriver.h"
#include "kernelUsbDriver.h"
#include "kernelVariableList.h"
#include <stdio.h>


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelDevice *kernelUsbXhciDetect(kernelBusTarget *busTarget,
	kernelDriver *driver)
{
	// This routine is used to detect and initialize a potential XHCI USB
	// device, as well as registering it with the higher-level interfaces.

	// This version is just a stub, to show that the device has been detected.
	// The real driver is in progress, but it's not ready for release.

	int status = 0;
	pciDeviceInfo pciDevInfo;
	usbController *controller = NULL;
	kernelDevice *dev = NULL;

	// Get the PCI device header
	status = kernelBusGetTargetInfo(busTarget, &pciDevInfo);
	if (status < 0)
		goto err_out;

	// Don't care about the 'multi-function' bit in the header type
	if (pciDevInfo.device.headerType & PCI_HEADERTYPE_MULTIFUNC)
		pciDevInfo.device.headerType &= ~PCI_HEADERTYPE_MULTIFUNC;

	// Make sure it's a non-bridge header
	if ((pciDevInfo.device.headerType & ~PCI_HEADERTYPE_MULTIFUNC) !=
		PCI_HEADERTYPE_NORMAL)
	{
		kernelDebugError("XHCI headertype not 'normal' (%02x)",
			(pciDevInfo.device.headerType &
				~PCI_HEADERTYPE_MULTIFUNC));
		goto err_out;
	}

	// Make sure it's an XHCI controller (programming interface is 0x30 in
	// the PCI header)
	if (pciDevInfo.device.progIF != 0x30)
		goto err_out;

	// After this point, we believe we have a supported device.

	kernelDebug(debug_usb, "XHCI controller found");

	// Allocate memory for the controller
	controller = kernelMalloc(sizeof(usbController));
	if (controller == NULL)
		goto err_out;

	// Get the USB version number
	controller->usbVersion = kernelBusReadRegister(busTarget, 0x60, 8);

	// Get the interrupt line
	controller->interruptNum = (int) pciDevInfo.device.nonBridge.interruptLine;

	kernelLog("USB: XHCI controller USB %d.%d interrupt %d",
		((controller->usbVersion & 0xF0) >> 4),
		(controller->usbVersion & 0xF), controller->interruptNum);

	// Allocate memory for the kernel device
	dev = kernelMalloc(sizeof(kernelDevice));
	if (dev == NULL)
		goto err_out;

	// Set up the kernel device
	dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
	dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB);
	dev->driver = driver;
	dev->data = (void *) controller;

	// Initialize the variable list for attributes of the controller
	status = kernelVariableListCreate(&dev->device.attrs);
	if (status >= 0)
		kernelVariableListSet(&dev->device.attrs, "controller.type", "XHCI");

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
