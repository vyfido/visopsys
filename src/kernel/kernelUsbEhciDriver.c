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
//  kernelUsbEhciDriver.c
//

#include "kernelUsbDriver.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelPciDriver.h"
#include "kernelError.h"
#include <string.h>

#include "kernelText.h"


kernelDevice *kernelUsbEhciDetect(kernelDevice *parent,
				  kernelBusTarget *busTarget,
				  kernelDriver *driver)
{
  // This routine is used to detect and initialize a potential EHCI USB
  // device, as well as registering it with the higher-level interfaces.

  int status = 0;
  pciDeviceInfo pciDevInfo;
  kernelDevice *dev = NULL;
  usbRootHub *usb = NULL;

  // Get the PCI device header
  status = kernelBusGetTargetInfo(bus_pci, busTarget->target, &pciDevInfo);
  if (status < 0)
    return (dev = NULL);

  // Make sure it's a non-bridge header
  if (pciDevInfo.device.headerType != PCI_HEADERTYPE_NORMAL)
    return (dev = NULL);

  // Make sure it's an EHCI controller (programming interface is 0x20 in
  // the PCI header)
  if (pciDevInfo.device.progIF != 0x20)
    return (dev = NULL);

  // After this point, we believe we have a supported device.

  // Enable the device on the PCI bus as a bus master
  if ((kernelBusDeviceEnable(bus_pci, busTarget->target, 1) < 0) ||
      (kernelBusSetMaster(bus_pci, busTarget->target, 1) < 0))
    return (dev = NULL);

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    return (dev);

  usb = kernelMalloc(sizeof(usbRootHub));
  if (usb == NULL)
    {
      kernelFree(dev);
      return (dev = NULL);
    }

  // Get the USB version number
  usb->usbVersion = kernelBusReadRegister(bus_pci, busTarget->target, 0x60, 8);
  kernelLog("USB: EHCI bus version %d.%d", ((usb->usbVersion & 0xF0) >> 4),
	    (usb->usbVersion & 0xF));

  // Get the I/O space base address.  For USB, it comes in the 5th
  // PCI base address register
  usb->ioAddress = (void *)
    (kernelBusReadRegister(bus_pci, busTarget->target, 0x04, 32) & 0xFFFFFFE0);

  if (usb->ioAddress == NULL)
    {
      kernelError(kernel_error, "Unknown USB controller I/O address");
      kernelFree(dev);
      kernelFree((void *) usb);
      return (dev = NULL);
    }

  // Get the interrupt line
  usb->interrupt = (int) pciDevInfo.device.nonBridge.interruptLine;

  //kernelTextPrintLine("USB I/O addr %x int %x", usb->ioAddress,
  //	      usb->interrupt);

  // Create the USB kernel device
  dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
  dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB);
  dev->driver = driver;
  dev->data = (void *) usb;

  status = kernelDeviceAdd(parent, dev);
  if (status < 0)
    {
      kernelFree(dev);
      kernelFree((void *) usb);
      return (dev = NULL);
    }

  return (dev);
}
