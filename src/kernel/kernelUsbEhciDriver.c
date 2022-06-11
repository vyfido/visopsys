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
//  kernelUsbEhciDriver.c
//

#include "kernelUsbDriver.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelPciDriver.h"
#include "kernelVariableList.h"
#include <string.h>


kernelDevice *kernelUsbEhciDetect(kernelDevice *parent,
				  kernelBusTarget *busTarget,
				  kernelDriver *driver)
{
  // This routine is used to detect and initialize a potential EHCI USB
  // device, as well as registering it with the higher-level interfaces.

  int status = 0;
  pciDeviceInfo pciDevInfo;
  kernelDevice *dev = NULL;
  usbController *controller = NULL;
  const char *headerType = NULL;

  // Get the PCI device header
  status = kernelBusGetTargetInfo(bus_pci, busTarget->target, &pciDevInfo);
  if (status < 0)
    goto err_out;

  // Make sure it's an EHCI controller (programming interface is 0x20 in
  // the PCI header)
  if (pciDevInfo.device.progIF != 0x20)
    goto err_out;

  // After this point, we believe we have a supported device.

  // Enable the device on the PCI bus as a bus master
  if ((kernelBusDeviceEnable(bus_pci, busTarget->target,
			     PCI_COMMAND_IOENABLE) < 0) ||
      (kernelBusSetMaster(bus_pci, busTarget->target, 1) < 0))
    goto err_out;

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    goto err_out;

  controller = kernelMalloc(sizeof(usbController));
  if (controller == NULL)
    goto err_out;

  // Get the USB version number
  controller->usbVersion =
    kernelBusReadRegister(bus_pci, busTarget->target, 0x60, 8);

  // Don't care about the 'multi-function' bit in the header type
  if (pciDevInfo.device.headerType & PCI_HEADERTYPE_MULTIFUNC)
    pciDevInfo.device.headerType &= ~PCI_HEADERTYPE_MULTIFUNC;

  // Get the interrupt line
  if (pciDevInfo.device.headerType == PCI_HEADERTYPE_NORMAL)
    {
      controller->interruptNum =
	(int) pciDevInfo.device.nonBridge.interruptLine;
      headerType = "normal";
    }
  else if (pciDevInfo.device.headerType == PCI_HEADERTYPE_BRIDGE)
    {
      controller->interruptNum = (int) pciDevInfo.device.bridge.interruptLine;
      headerType = "bridge";
    }
  else if (pciDevInfo.device.headerType == PCI_HEADERTYPE_CARDBUS)
    {
      controller->interruptNum = (int) pciDevInfo.device.cardBus.interruptLine;
      headerType = "cardbus";
    }
  else
    {
      kernelDebugError("EHCI: Unsupported USB controller header type %d",
		       pciDevInfo.device.headerType);
      goto err_out;
    }

  kernelLog("USB: EHCI controller USB %d.%d interrupt %d PCI type: %s",
	    ((controller->usbVersion & 0xF0) >> 4),
	    (controller->usbVersion & 0xF), controller->interruptNum,
	    headerType);

  // Get the I/O space base address.  For USB, it comes in the 5th
  // PCI base address register
  controller->ioAddress = (void *)
    (kernelBusReadRegister(bus_pci, busTarget->target, 0x04, 32) & 0xFFFFFFE0);

  if (controller->ioAddress == NULL)
    {
      kernelDebugError("EHCI: Unknown USB controller I/O address");
      goto err_out;
    }

  // Create the USB kernel device
  dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
  dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB);
  dev->driver = driver;
  dev->data = (void *) controller;

  // Initialize the variable list for attributes of the controller
  status = kernelVariableListCreate(&dev->device.attrs);
  if (status >= 0)
    kernelVariableListSet(&dev->device.attrs, "controller.type", "EHCI");

  status = kernelDeviceAdd(parent, dev);
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
