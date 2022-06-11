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
//  kernelSataAhciDriver.c
//

// Driver for standard AHCI SATA disks

#include "kernelSataAhciDriver.h"
#include "kernelBus.h"
#include "kernelDebug.h"
#include "kernelDevice.h"
#include "kernelDisk.h"
#include "kernelError.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelPciDriver.h"
#include "kernelSysTimer.h"
#include <stdio.h>
#include <stdlib.h>

static ahciController *controllers = NULL;
static int numControllers = 0;


/*
static int reset(int ctrlNum)
{
  kernelDebug(debug_io, "PCI AHCI: reset controller %d", ctrlNum);
  controllers[ctrlNum].regs->ghc |= 1;

  // Wait for the reset to finish
  while (controllers[ctrlNum].regs->ghc & 1);

  return 0;
}
*/


static kernelDevice *detectPciControllers(kernelDriver *driver)
{
  // Try to detect AHCI controllers on the PCI bus

  int status = 0;
  kernelDevice *controllerDevices = NULL;
  kernelDevice *busDevice = NULL;
  kernelBusTarget *pciTargets = NULL;
  int numPciTargets = 0;
  int deviceCount = 0;
  pciDeviceInfo pciDevInfo;
  unsigned modPageSize = 0;
  void *virtualAddress = NULL;
  int legacySupport = 0;
  ahciPort *port = NULL;
  int isAtapi = 0;
  kernelPhysicalDisk *physicalDisk = NULL;
  kernelDevice *diskDevice = NULL;
  int count;

  // See if there are any AHCI controllers on the PCI bus.  This obviously
  // depends upon PCI hardware detection occurring before AHCI detection.

  // Get the PCI bus device
  status = kernelDeviceFindType(kernelDeviceGetClass(DEVICECLASS_BUS),
				kernelDeviceGetClass(DEVICESUBCLASS_BUS_PCI),
				&busDevice, 1);
  if (status <= 0)
    {
      kernelDebug(debug_io, "PCI AHCI: no PCI bus");
      return (controllerDevices = NULL);
    }

  // Search the PCI bus(es) for devices
  numPciTargets = kernelBusGetTargets(bus_pci, &pciTargets);
  if (numPciTargets <= 0)
    {
      kernelDebug(debug_io, "PCI AHCI: no PCI targets");
      return (controllerDevices = NULL);
    }

  // Search the PCI bus targets for AHCI controllers
  for (deviceCount = 0; deviceCount < numPciTargets; deviceCount ++)
    {
      // If it's not an AHCI controller, skip it
      if ((pciTargets[deviceCount].class == NULL) ||
	  (pciTargets[deviceCount].class->class != DEVICECLASS_DISKCTRL) ||
	  (pciTargets[deviceCount].subClass == NULL) ||
	  (pciTargets[deviceCount].subClass->class !=
	   DEVICESUBCLASS_DISKCTRL_SATA))
	continue;

      // Get the PCI device header
      status = kernelBusGetTargetInfo(bus_pci, pciTargets[deviceCount].target,
				      &pciDevInfo);
      if (status < 0)
	{
	  kernelDebug(debug_io, "PCI AHCI: error getting target info");
	  continue;
	}

      kernelDebug(debug_io, "PCI AHCI: check device %x %x progif=%02x",
		  (pciTargets[deviceCount].class?
		   pciTargets[deviceCount].class->class : 0),
		  (pciTargets[deviceCount].subClass?
		   pciTargets[deviceCount].subClass->class : 0),
		  pciDevInfo.device.progIF);

      // Make sure it's a non-bridge header
      if (pciDevInfo.device.headerType != PCI_HEADERTYPE_NORMAL)
	{
	  kernelDebug(debug_io, "PCI AHCI: Headertype not 'normal' (%d)",
		      pciDevInfo.device.headerType);
	  continue;
	}

      // Make sure it's an AHCI controller (programming interface is 0x01 in
      // the PCI header)
      if (pciDevInfo.device.progIF != 0x01)
	{
	  kernelDebug(debug_io, "PCI AHCI: SATA controller not AHCI");
	  continue;
	}

      kernelDebug(debug_io, "PCI AHCI: Found");

      // Enable bus mastering and disable the memory decoder
      if (pciDevInfo.device.commandReg & PCI_COMMAND_MASTERENABLE)
	kernelDebug(debug_io, "PCI AHCI: Bus mastering already enabled");
      else
	kernelBusSetMaster(bus_pci, pciTargets[deviceCount].target, 1);
      kernelBusDeviceEnable(bus_pci, pciTargets[deviceCount].target, 0);

      // Re-read target info
      kernelBusGetTargetInfo(bus_pci, pciTargets[deviceCount].target,
			     &pciDevInfo);

      if (!(pciDevInfo.device.commandReg & PCI_COMMAND_MASTERENABLE))
	{
	  kernelDebugError("PCI AHCI: Couldn't enable bus mastering");
	  continue;
	}
      kernelDebug(debug_io, "PCI AHCI: Bus mastering enabled in PCI");

      // Make sure the ABAR refers to a memory decoder
      if (pciDevInfo.device.nonBridge.baseAddress[5] & 0x00000001)
	{
	  kernelDebugError("PCI AHCI: ABAR is not a memory decoder");
	  continue;
	}

      // (Re)allocate memory for the controllers
      controllers =
	kernelRealloc((void *) controllers, ((numControllers + 1) *
					     sizeof(ahciController)));
      if (controllers == NULL)
	return (controllerDevices = NULL);

      // Print registers
      kernelDebug(debug_io, "PCI AHCI: Interrupt line=%d",
		  pciDevInfo.device.nonBridge.interruptLine);
      kernelDebug(debug_io, "PCI AHCI: ABAR base address reg=%08x",
		  pciDevInfo.device.nonBridge.baseAddress[5]);

      // Get the interrupt line
      if (pciDevInfo.device.nonBridge.interruptLine &&
	  (pciDevInfo.device.nonBridge.interruptLine != 0xFF))
	{
	  kernelDebug(debug_io, "PCI AHCI: Using PCI interrupt=%d",
		      pciDevInfo.device.nonBridge.interruptLine);
	  controllers[numControllers].interrupt =
	    pciDevInfo.device.nonBridge.interruptLine;
	}
      else
	kernelDebug(debug_io, "PCI AHCI: Unknown PCI interrupt=%d",
		    pciDevInfo.device.nonBridge.interruptLine);

      // Get the memory range address
      controllers[numControllers].physMemSpace =
	(pciDevInfo.device.nonBridge.baseAddress[5] & 0xFFFFFFF0);

      kernelDebug(debug_io, "PCI AHCI: Registers address %08x",
		  controllers[numControllers].physMemSpace);

      // Determine the memory space size.  Write all 1s to the register.
      kernelBusWriteRegister(bus_pci, pciTargets[deviceCount].target,
			     PCI_CONFREG_BASEADDRESS5_32, 32, 0xFFFFFFFF);

      controllers[numControllers].memSpaceSize =
	(~(kernelBusReadRegister(bus_pci, pciTargets[deviceCount].target,
				 PCI_CONFREG_BASEADDRESS5_32, 32) & ~0x7) + 1);

      kernelDebug(debug_io, "PCI AHCI: address size %08x (%d)",
		  controllers[numControllers].memSpaceSize,
		  controllers[numControllers].memSpaceSize);

      // Restore the register we clobbered.
      kernelBusWriteRegister(bus_pci, pciTargets[deviceCount].target,
			     PCI_CONFREG_BASEADDRESS5_32, 32,
			     pciDevInfo.device.nonBridge.baseAddress[5]);

      kernelDebug(debug_io, "PCI AHCI: ABAR now %08x",
		  kernelBusReadRegister(bus_pci,
					pciTargets[deviceCount].target,
					PCI_CONFREG_BASEADDRESS5_32, 32));

      // Map the physical memory address of the controller's registers into
      // our virtual address space.

      // We round the physical address so that we can map whole pages.
      modPageSize =
	(controllers[numControllers].physMemSpace % MEMORY_PAGE_SIZE);

      // Map the physical memory space pointed to by the decoder.
      status = kernelPageMapToFree(KERNELPROCID, (void *)
				   (controllers[numControllers].physMemSpace -
				    modPageSize), &virtualAddress,
				   (controllers[numControllers].memSpaceSize +
				    modPageSize));
      if (status < 0)
	{
	  kernelDebugError("PCI AHCI: Error mapping memory");
	  continue;
	}

      // Adjust our registers pointer forward from the start of the page to
      // the beginning of the actual data
      controllers[numControllers].regs = (virtualAddress + modPageSize);

      // Enable memory mapping access
      if (pciDevInfo.device.commandReg & PCI_COMMAND_MEMORYENABLE)
	kernelDebug(debug_io, "PCI AHCI: Memory access already enabled");
      else
	kernelBusDeviceEnable(bus_pci, pciTargets[deviceCount].target,
			      PCI_COMMAND_MEMORYENABLE);

      // Re-read target info
      kernelBusGetTargetInfo(bus_pci, pciTargets[deviceCount].target,
			     &pciDevInfo);

      if (!(pciDevInfo.device.commandReg & PCI_COMMAND_MEMORYENABLE))
	{
	  kernelDebug(debug_io, "PCI AHCI: Couldn't enable memory access");
	  continue;
	}
      kernelDebug(debug_io, "PCI AHCI: Memory access enabled in PCI");

      // Enable AHCI .  Later we can enable interrupts here as well.
      if (controllers[numControllers].regs->ghc & (1 << 31))
	kernelDebug(debug_io, "PCI AHCI: SATA mode already enabled");
      else
	controllers[numControllers].regs->ghc |= (1 << 31);

      kernelDebug(debug_io, "PCI AHCI: caps=%08x",
		  controllers[numControllers].regs->caps);
      kernelDebug(debug_io, "PCI AHCI: ghc=%08x",
		  controllers[numControllers].regs->ghc);
      kernelDebug(debug_io, "PCI AHCI: intStat=%08x",
		  controllers[numControllers].regs->intStat);
      kernelDebug(debug_io, "PCI AHCI: portsImpl=%08x",
		  controllers[numControllers].regs->portsImpl);
      kernelDebug(debug_io, "PCI AHCI: version=%08x",
		  controllers[numControllers].regs->version);

      if (controllers[numControllers].regs->caps & (1 << 18))
	kernelDebug(debug_io, "PCI AHCI: only works in native mode");
      else
	{
	  legacySupport = 1;
	  kernelDebug(debug_io, "PCI AHCI: supports legacy mode");
	}

      kernelDebug(debug_io, "PCI AHCI: %d ports supported",
		  ((controllers[numControllers].regs->caps & 0x1F) + 1));

      // Create a device for it in the kernel.

      // Allocate memory for the device.
      controllerDevices =
	kernelRealloc(controllerDevices,
		      ((numControllers + 1) * sizeof(kernelDevice)));
      if (controllerDevices == NULL)
	continue;

      controllerDevices[numControllers].device.class =
	kernelDeviceGetClass(DEVICECLASS_DISKCTRL);
      controllerDevices[numControllers].device.subClass =
	kernelDeviceGetClass(DEVICESUBCLASS_DISKCTRL_SATA);

      // Register the controller
      kernelDeviceAdd(busDevice, &controllerDevices[numControllers]);

      // For each implemented port, loop through and see whether we think
      // there's a device attached.  The number of implemented ports is
      // a zero-based number.
      for (count = 0; count <= // <- zero based
	     (int)(controllers[numControllers].regs->caps & 0x1F); count ++)
	{
	  // Port implemented?
	  if (controllers[numControllers].regs->portsImpl & (1 << count))
	    {
	      port = &controllers[numControllers].regs->ports[count];

	      // Stop the port
	      port->CMD &= ~0x1;

	      // Perform device detection
	      port->SCTL &= ~0xF;
	      port->SCTL |= 0x1;
	      kernelSysTimerWaitTicks(1);
	      port->SCTL &= ~0xF;
	      kernelSysTimerWaitTicks(1);

	      kernelDebug(debug_io, "PCI AHCI: port %d SIG=%08x", count,
			  port->SIG);

	      kernelDebug(debug_io, "PCI AHCI: port %d SSTS=%03x", count,
			  port->SSTS);

	      // Is there a device here?
	      if (port->SSTS & 0x3)
		{
		  isAtapi = (port->CMD & (1 << 24));

		  kernelDebug(debug_io, "PCI AHCI: port %d %sdevice detected",
			      count, (isAtapi? "ATAPI " : ""));

		  // For the moment, we haven't implemented a proper driver,
		  // so only register disks if the controller doesn't support
		  // legacy mode.
		  if (!legacySupport)
		    {
		      // Allocate memory for the disk structure
		      physicalDisk = kernelMalloc(sizeof(kernelPhysicalDisk));
		      if (physicalDisk == NULL)
			continue;

		      physicalDisk->deviceNumber = deviceCount;
		      physicalDisk->driver = driver;

		      if (isAtapi)
			{
			  physicalDisk->description = "SATA CD-ROM";
			  physicalDisk->type =
			    (DISKTYPE_PHYSICAL | DISKTYPE_REMOVABLE |
			     DISKTYPE_SATACDROM);
			}
		      else
			{
			  physicalDisk->description = "SATA hard disk";
			  physicalDisk->type =
			    (DISKTYPE_PHYSICAL | DISKTYPE_FIXED |
			     DISKTYPE_SATADISK);
			  physicalDisk->flags = DISKFLAG_MOTORON;
			}

		      // Lots of things divide by this
		      physicalDisk->sectorSize = 512;

		      // Allocate memory for the device
		      diskDevice = kernelMalloc(sizeof(kernelDevice));
		      if (diskDevice == NULL)
			continue;

		      diskDevice->device.class =
			kernelDeviceGetClass(DEVICECLASS_DISK);
		      diskDevice->device.subClass =
			kernelDeviceGetClass(DEVICESUBCLASS_DISK_SATA);
		      diskDevice->driver = driver;
		      diskDevice->data = (void *) physicalDisk;

		      // Register the disk
		      status = kernelDiskRegisterDevice(diskDevice);
		      if (status < 0)
			continue;

		      // Add the device
		      status =
			kernelDeviceAdd(&controllerDevices[numControllers],
					diskDevice);
		      if (status < 0)
			continue;
		    }
		}
	      else
		kernelDebug(debug_io, "PCI AHCI: port %d no device", count);
	    }
	  else
	    kernelDebug(debug_io, "PCI AHCI: port %d not implemented", count);
	}

      // Now, since we haven't implemented this driver yet, if the controller
      // supports legacy mode, deactivate AHCI so we can use our IDE driver.
      if (legacySupport)
	{
	  kernelDebug(debug_io, "PCI AHCI: disabling AHCI for legacy mode");
	  controllers[numControllers].regs->ghc &= ~(1 << 31);
	}

      numControllers += 1;
    }

  kernelFree(pciTargets);
  return (controllerDevices);
}


static int driverDetect(void *parent __attribute__((unused)),
			kernelDriver *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.  Also does
  // general driver initialization.
  
  int status = 0;
  kernelDevice *controllerDevices = NULL;

  kernelLog("AHCI: Examining disks...");

  // Reset controller count
  numControllers = 0;

  // First see whether we have PCI controller(s)
  controllerDevices = detectPciControllers(driver);
  if (controllerDevices == NULL)
    {
      // Nothing on PCI.
      kernelDebug(debug_io, "PCI AHCI controller not detected.");
      return (status = 0);
    }

  kernelDebug(debug_io, "PCI AHCI: %d controllers detected", numControllers);

  return (status = 0);
}


static kernelDiskOps ahciOps = {
  NULL, // driverReset
  NULL, // driverRecalibrate
  NULL, // driverSetMotorState
  NULL, // driverSetLockState
  NULL, // driverSetDoorState
  NULL, // driverDiskChanged
  NULL, // driverReadSectors
  NULL, // driverWriteSectors
  NULL  // driverFlush
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelSataAhciDriverRegister(kernelDriver *driver)
{
  // Device driver registration.

  driver->driverDetect = driverDetect;
  driver->ops = &ahciOps;

  return;
}
