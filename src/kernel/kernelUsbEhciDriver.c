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

#include "kernelUsbEhciDriver.h"
#include "kernelUsbDriver.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMisc.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelPciDriver.h"
#include "kernelProcessorX86.h"
#include "kernelVariableList.h"
#include <string.h>


#ifdef DEBUG
static inline void debugEhciOpRegs(usbController *controller)
{
  usbEhciData *ehciData = controller->data;

  kernelDebug(debug_usb, "Debug EHCI registers:\n"
	      "    cmd=%08x\n"
	      "    sts=%08x\n"
	      "    intr=%08x\n"
	      "    frindex=%08x\n"
	      "    ctrldsseg=%08x\n"
	      "    perlstbase=%08x\n"
	      "    asynclstaddr=%08x\n"
	      "    configflag=%08x\n"
	      "    portsc1=%08x\n"
	      "    portsc2=%08x\n",
	      ehciData->opRegs->cmd, ehciData->opRegs->sts,
	      ehciData->opRegs->intr, ehciData->opRegs->frindex,
	      ehciData->opRegs->ctrldseg, ehciData->opRegs->perlstbase,
	      ehciData->opRegs->asynclstaddr, ehciData->opRegs->configflag,
	      ehciData->opRegs->portsc[0], ehciData->opRegs->portsc[1]);
}
#else
  #define debugEhciRegs(usb) do { } while (0)
#endif // DEBUG


static void reset(usbController *controller)
{
  // Do complete USB reset

  usbEhciData *ehciData = controller->data;

  // Set host controller reset
  ehciData->opRegs->cmd |= USBEHCI_CMD_HCRESET;
  if (!(ehciData->opRegs->cmd & USBEHCI_CMD_HCRESET))
    kernelDebugError("EHCI reset bit did not set");

  // Wait until the host controller clears it
  while (ehciData->opRegs->cmd & USBEHCI_CMD_HCRESET)
    {}

  // Clear the lock
  kernelMemClear((void *) &controller->lock, sizeof(lock));

  kernelDebug(debug_usb, "EHCI controller reset");
  return;
}


kernelDevice *kernelUsbEhciDetect(kernelBusTarget *busTarget,
				  kernelDriver *driver)
{
  // This routine is used to detect and initialize a potential EHCI USB
  // device, as well as registering it with the higher-level interfaces.

  int status = 0;
  pciDeviceInfo pciDevInfo;
  kernelDevice *dev = NULL;
  usbController *controller = NULL;
  usbEhciData *ehciData = NULL;

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
      kernelDebug(debug_usb, "EHCI headertype not 'normal' (%d)",
		  pciDevInfo.device.headerType);
      goto err_out;
    }

  // Make sure it's an EHCI controller (programming interface is 0x20 in
  // the PCI header)
  if (pciDevInfo.device.progIF != 0x20)
    goto err_out;

  // After this point, we believe we have a supported device.

  kernelDebug(debug_usb, "EHCI found");

  // Enable bus mastering and disable the memory decoder
  if (pciDevInfo.device.commandReg & PCI_COMMAND_MASTERENABLE)
    kernelDebug(debug_usb, "EHCI bus mastering already enabled");
  else
    kernelBusSetMaster(busTarget, 1);
  kernelBusDeviceEnable(busTarget, 0);

  // Re-read target info
  status = kernelBusGetTargetInfo(busTarget, &pciDevInfo);
  if (status < 0)
    goto err_out;

  if (!(pciDevInfo.device.commandReg & PCI_COMMAND_MASTERENABLE))
    {
      kernelDebugError("EHCI: Couldn't enable bus mastering");
      goto err_out;
    }
  kernelDebug(debug_usb, "EHCI bus mastering enabled in PCI");

  // Make sure the BAR refers to a memory decoder
  if (pciDevInfo.device.nonBridge.baseAddress[0] & 0x1)
    {
      kernelDebugError("EHCI: ABAR is not a memory decoder");
      goto err_out;
    }

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    goto err_out;

  // Allocate memory for the controller
  controller = kernelMalloc(sizeof(usbController));
  if (controller == NULL)
    goto err_out;

  // Get the USB version number
  controller->usbVersion = kernelBusReadRegister(busTarget, 0x60, 8);

  // Get the interrupt line
  controller->interruptNum = (int) pciDevInfo.device.nonBridge.interruptLine;

  kernelLog("USB: EHCI controller USB %d.%d interrupt %d",
	    ((controller->usbVersion & 0xF0) >> 4),
	    (controller->usbVersion & 0xF), controller->interruptNum);

  // Allocate memory for the EHCI data
  controller->data = kernelMalloc(sizeof(usbEhciData));
  if (controller->data == NULL)
    goto err_out;

  ehciData = controller->data;

  // Get the memory range address
  ehciData->physMemSpace =
    (pciDevInfo.device.nonBridge.baseAddress[0] & 0xFFFFFFF0);

  if (pciDevInfo.device.nonBridge.baseAddress[0] & 0x6)
    {
      kernelError(kernel_error, "EHCI: Register memory must be mappable in "
		  "32-bit address space");
      goto err_out;
    }

  // Determine the memory space size.  Write all 1s to the register.
  kernelBusWriteRegister(busTarget,
			 PCI_CONFREG_BASEADDRESS0_32, 32, 0xFFFFFFFF);

  ehciData->memSpaceSize =
    (~(kernelBusReadRegister(busTarget,
			     PCI_CONFREG_BASEADDRESS0_32, 32) & ~0xF) + 1);

  // Restore the register we clobbered.
  kernelBusWriteRegister(busTarget, PCI_CONFREG_BASEADDRESS0_32, 32,
			 pciDevInfo.device.nonBridge.baseAddress[0]);

  // Map the physical memory address of the controller's registers into
  // our virtual address space.

  // Map the physical memory space pointed to by the decoder.
  status = kernelPageMapToFree(KERNELPROCID, (void *) ehciData->physMemSpace,
			       (void **) &(ehciData->capRegs),
			       ehciData->memSpaceSize);
  if (status < 0)
    {
      kernelDebugError("EHCI: Error mapping memory");
      goto err_out;
    }

  // Make it non-cacheable, since this memory represents memory-mapped
  // hardware registers.
  status =
    kernelPageSetAttrs(KERNELPROCID, 1 /* set */, PAGEFLAG_CACHEDISABLE,
		       (void *) ehciData->capRegs,ehciData->memSpaceSize);
  if (status < 0)
    {
      kernelDebugError("EHCI: Error setting page attrs");
      goto err_out;
    }

  // Enable memory mapping access
  if (pciDevInfo.device.commandReg & PCI_COMMAND_MEMORYENABLE)
    kernelDebug(debug_usb, "EHCI memory access already enabled");
  else
    kernelBusDeviceEnable(busTarget, PCI_COMMAND_MEMORYENABLE);

  // Re-read target info
  kernelBusGetTargetInfo(busTarget, &pciDevInfo);

  if (!(pciDevInfo.device.commandReg & PCI_COMMAND_MEMORYENABLE))
    {
      kernelDebug(debug_usb, "EHCI couldn't enable memory access");
      goto err_out;
    }
  kernelDebug(debug_usb, "EHCI memory access enabled in PCI");

  ehciData->opRegs = ((void *) ehciData->capRegs + ehciData->capRegs->capslen);

  // Reset the controller
  reset(controller);

  // That's all we do for now.  We don't yet support EHCI, but some systems
  // leave EHCI in a configured state, meaning that we subsequently don't get
  // access to any USB 2.0 devices via the legacy controllers.  Resetting the
  // EHCI controller should solve that.

  // Create the USB kernel device
  dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
  dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB);
  dev->driver = driver;
  dev->data = (void *) controller;

  // Initialize the variable list for attributes of the controller
  status = kernelVariableListCreate(&dev->device.attrs);
  if (status >= 0)
    kernelVariableListSet(&dev->device.attrs, "controller.type", "EHCI");

  status = kernelDeviceAdd(busTarget->bus->dev, dev);
  if (status < 0)
    goto err_out;
  else 
    return (dev);

 err_out:

  if (dev)
    kernelFree(dev);
  if (controller)
    {
      if (controller->data)
	kernelFree(controller->data);
      kernelFree((void *) controller);
    }

  return (dev = NULL);
}
