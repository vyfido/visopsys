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
//  kernelPciDriver.c
//

// These routines allow access to PCI configuration space.  Based on an
// original version contributed by Jonas Zaddach: See the file
// contrib/jonas-pci/src/kernel/kernelBusPCI.c

#include "kernelPciDriver.h"
#include "kernelBus.h"
#include "kernelDevice.h"
#include "kernelProcessorX86.h"
#include "kernelMalloc.h"
#include "kernelLog.h"
#include "kernelError.h"
#include <string.h>

static pciSubClass subclass_old[] = {
  { 0x00, "other", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x01, "VGA", DEVICECLASS_GRAPHIC, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_disk[] = {
  { 0x00, "SCSI", DEVICECLASS_DISK, DEVICESUBCLASS_DISK_SCSI },
  { 0x01, "IDE", DEVICECLASS_DISK, DEVICESUBCLASS_DISK_IDE },
  { 0x02, "floppy", DEVICECLASS_DISK, DEVICESUBCLASS_DISK_FLOPPY },
  { 0x03, "IPI", DEVICECLASS_DISK, DEVICECLASS_NONE },
  { 0x04, "RAID", DEVICECLASS_DISK, DEVICECLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_net[] = {
  { 0x00, "ethernet", DEVICECLASS_NETWORK, DEVICESUBCLASS_NETWORK_ETHERNET },
  { 0x01, "token ring", DEVICECLASS_NETWORK, DEVICESUBCLASS_NONE },
  { 0x02, "FDDI", DEVICECLASS_NETWORK, DEVICESUBCLASS_NONE },
  { 0x03, "ATM", DEVICECLASS_NETWORK, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_graphics[] = {
  { 0x00, "VGA", DEVICECLASS_GRAPHIC, DEVICESUBCLASS_NONE },
  { 0x01, "super VGA", DEVICECLASS_GRAPHIC, DEVICESUBCLASS_NONE },
  { 0x02, "XGA", DEVICECLASS_GRAPHIC, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_mma[] = {
  { 0x00, "video", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x01, "audio", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_mem[] = {
  { 0x00, "RAM", DEVICECLASS_MEMORY, DEVICESUBCLASS_NONE },
  { 0x01, "flash", DEVICECLASS_MEMORY, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_bridge[] = {
  { 0x00, "CPU/PCI", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x01, "PCI/ISA", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x02, "PCI/EISA", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x03, "PCI/MCA", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x04, "PCI/PCI", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x05, "PCI/PCMCIA", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x06, "PCI/NuBus", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x07, "PCI/cardbus", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_comm[] = {
  { 0x00, "serial", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x01, "parallel", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_sys[] = {
  { 0x00, "PIC", DEVICECLASS_PIC, DEVICESUBCLASS_NONE },
  { 0x01, "DMAC", DEVICECLASS_DMA, DEVICESUBCLASS_NONE },
  { 0x02, "timer", DEVICECLASS_SYSTIMER, DEVICESUBCLASS_NONE },
  { 0x03, "RTC", DEVICECLASS_RTC, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_hid[] = {
  { 0x00, "keyboard", DEVICECLASS_KEYBOARD, DEVICESUBCLASS_NONE },
  { 0x01, "digitizer", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { 0x02, "mouse", DEVICECLASS_MOUSE, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_dock[] = {
  { 0x00, "generic", DEVICECLASS_NONE, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_cpu[] = {
  { 0x00, "386", DEVICECLASS_CPU, DEVICESUBCLASS_CPU_X86 },
  { 0x01, "486", DEVICECLASS_CPU, DEVICESUBCLASS_CPU_X86 },
  { 0x02, "pentium", DEVICECLASS_CPU, DEVICESUBCLASS_CPU_X86 },
  { 0x03, "P6", DEVICECLASS_CPU, DEVICESUBCLASS_CPU_X86 },
  { 0x10, "alpha", DEVICECLASS_CPU, DEVICESUBCLASS_NONE },
  { 0x40, "coprocessor", DEVICECLASS_CPU, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciSubClass subclass_serial[] = {
  { 0x00, "firewire", DEVICECLASS_BUS, DEVICESUBCLASS_NONE },
  { 0x01, "ACCESS.bus", DEVICECLASS_BUS, DEVICESUBCLASS_NONE },
  { 0x02, "SSA", DEVICECLASS_BUS, DEVICESUBCLASS_NONE },
  { 0x03, "USB", DEVICECLASS_BUS, DEVICESUBCLASS_BUS_USB },
  { 0x04, "fiber channel", DEVICECLASS_BUS, DEVICESUBCLASS_NONE },
  { PCI_INVALID_SUBCLASSCODE, "", DEVICECLASS_NONE, DEVICESUBCLASS_NONE }
};

static pciClass pciClassNames[] = {
  { 0x00, "before PCI 2.0", subclass_old },
  { 0x01, "disk controller", subclass_disk },
  { 0x02, "network interface", subclass_net },
  { 0x03, "graphics adapter", subclass_graphics },
  { 0x04, "multimedia adapter", subclass_mma },
  { 0x05, "memory", subclass_mem },
  { 0x06, "bridge", subclass_bridge },
  { 0x07, "communication", subclass_comm },
  { 0x08, "system peripheral", subclass_sys },
  { 0x09, "HID", subclass_hid },
  { 0x0a, "docking station", subclass_dock },
  { 0x0b, "CPU", subclass_cpu },
  { 0x0c, "serial bus", subclass_serial },
  { PCI_INVALID_CLASSCODE, "", NULL }
};

static const char *invalidDevice = "invalid device";
static const char *otherDevice = "other";

static kernelBusTarget *targets = NULL;
static int numTargets = 0;

#define headerAddress(bus, device, function, reg)                       \
  (0x80000000L |  (((unsigned) ((bus) & 0xFF) << 16) |                  \
   (((device) & 0x1F)  << 11) | (((function) & 0x07) << 8) |            \
   (((reg) & 0x3F) << 2)))

// Make our proprietary PCI target code
#define makeTargetCode(bus, device, function)                           \
  ((((bus) & 0xFF) << 16) | (((device) & 0xFF) << 8) | ((function) & 0xFF))

// Translate a target code back to bus, device, function
#define makeBusDevFunc(targetCode, bus, device, function)               \
  {  (bus) = (((targetCode) >> 16) & 0xFF);                             \
     (device) = (((targetCode) >> 8) & 0xFF);                           \
     (function) = ((targetCode) & 0xFF);  }


static void readConfig8(int bus, int dev, int function, int reg,
			unsigned char *data)
{
  // Reads configuration byte
  unsigned address = headerAddress(bus, dev, function, (reg / 4));
  kernelProcessorOutPort32(PCI_CONFIG_PORT, address);
  kernelProcessorInPort8((PCI_DATA_PORT + (reg % 4)), *data);
  return;
}


static void writeConfig8(int bus, int dev, int function, int reg,
			 unsigned char data)
{
  // Writes configuration byte
  unsigned address = headerAddress(bus, dev, function, (reg / 4));
  kernelProcessorOutPort32(PCI_CONFIG_PORT, address);
  kernelProcessorOutPort8((PCI_DATA_PORT + (reg % 4)), data);
  return;
}


static void readConfig16(int bus, int dev, int function, int reg,
			 unsigned short *data)
{
  // Reads configuration word
  unsigned address = headerAddress(bus, dev, function, (reg / 2));
  kernelProcessorOutPort32(PCI_CONFIG_PORT, address);
  kernelProcessorInPort16((PCI_DATA_PORT + (reg % 2)), *data);
  return;
}


static void writeConfig16(int bus, int dev, int function, int reg,
			  unsigned short data)
{
  // Writes configuration word
  unsigned address = headerAddress(bus, dev, function, (reg / 2));
  kernelProcessorOutPort32(PCI_CONFIG_PORT, address);
  kernelProcessorOutPort16((PCI_DATA_PORT + (reg % 2)), data);
  return;
}


static void readConfig32(int bus, int dev, int function, int reg,
			 unsigned *data)
{
  // Reads configuration dword
  unsigned address = headerAddress(bus, dev, function, reg);
  kernelProcessorOutPort32(PCI_CONFIG_PORT, address);
  kernelProcessorInPort32(PCI_DATA_PORT, *data);
  return;
}


static void writeConfig32(int bus, int dev, int function, int reg,
			  unsigned data)
{
  // Writes configuration dword
  unsigned address = headerAddress(bus, dev, function, reg);
  kernelProcessorOutPort32(PCI_CONFIG_PORT, address);
  kernelProcessorOutPort32(PCI_DATA_PORT, data);
  return;
}


static void readConfigHeader(int bus, int dev, int function,
			     pciDeviceInfo *devInfo)
{
  // Fill up the supplied device info header

  unsigned address = 0;
  int count;

  for (count = 0; count < PCI_CONFIGHEADER_SIZE; count ++)
    {
      address =	headerAddress(bus, dev, function, count);
      kernelProcessorOutPort32(PCI_CONFIG_PORT, address);
      kernelProcessorInPort32(PCI_DATA_PORT, devInfo->header[count]);
    }
}


static void getClass(int classCode, pciClass **class)
{
  // Return the PCI class, given the class code

  int count;

  for (count = 0; count < 256; count++)
    {	
      // If no more classcodes are in the list
      if (pciClassNames[count].classCode == PCI_INVALID_CLASSCODE)
	{
	  *class = NULL;
	  return;
	}
		
      // If valid classcode is found
      if (pciClassNames[count].classCode == classCode)
	{
	  *class = &(pciClassNames[count]);
	  return;
	}
    }
}


static void getSubClass(pciClass *class, int subClassCode,
			pciSubClass **subClass)
{
  // Return the PCI subclass, given the class and subclass code

  int count;

  for (count = 0; count < 256; count++)
    {	
      // If no more subclass codes are in the list
      if (class->subClasses[count].subClassCode == PCI_INVALID_SUBCLASSCODE)
	{
	  *subClass = NULL;
	  return;
	}
		
      // If valid subclass code is found
      if (class->subClasses[count].subClassCode == subClassCode)
	{
	  *subClass = &(class->subClasses[count]);
	  return;
	}
    }
}


static int getClassName(int classCode, int subClassCode, char **className,
			char **subClassName)
{
  // Returns name of the class and the subclass in human readable format.
  // Buffers classname and subclassname have to provide

  int status = 0;
  pciClass *class = NULL;
  pciSubClass *subClass = NULL;

  getClass(classCode, &class);
  if (class == NULL)
    {
      *className = (char *) invalidDevice;
      return (status = PCI_INVALID_CLASSCODE);
    }

  *className = (char *) class->name;

  // Subclasscode 0x80 is always other
  if (subClassCode == 0x80)
    {
      *subClassName = (char *) otherDevice;
      return (status = 0);
    }

  getSubClass(class, subClassCode, &subClass);
  if (subClass == NULL)
    {
      *subClassName = (char *) invalidDevice;
      return (status = PCI_INVALID_SUBCLASSCODE);
    }

  *subClassName = (char *) subClass->name;
  return (status = 0);
}


static void deviceInfo2BusTarget(int bus, int dev, int function,
				 pciDeviceInfo *info, kernelBusTarget *target)
{
  // Translate a device info header to a bus target listing

  pciClass *class = NULL;
  pciSubClass *subClass = NULL;

  getClass(info->device.classCode, &class);
  if (class == NULL)
    return;

  getSubClass(class, info->device.subClassCode, &subClass);
  if (subClass == NULL)
    return;

  target->target = makeTargetCode(bus, dev, function);
  target->class = kernelDeviceGetClass(subClass->systemClassCode);
  target->subClass = kernelDeviceGetClass(subClass->systemSubClassCode);
}


static int driverGetTargets(kernelBusTarget **pointer)
{
  // Return the pointer to our list of targets
  *pointer = targets;
  return (numTargets);
}


static int driverGetTargetInfo(int target, void *pointer)
{
  // Read the device's PCI header and copy it to the supplied memory
  // pointer

  int status = 0;
  int bus, dev, function;

  makeBusDevFunc(target, bus, dev, function);
  readConfigHeader(bus, dev, function, pointer);

  return (status = 0);
}


static unsigned driverReadRegister(int target, int reg, int bitWidth)
{
  // Returns the contents of a PCI configuration register

  unsigned contents = 0;
  int bus, dev, function;
  
  makeBusDevFunc(target, bus, dev, function);

  switch (bitWidth)
    {
    case 8:
      readConfig8(bus, dev, function, reg, (unsigned char *) &contents);
      break;
    case 16:
      readConfig16(bus, dev, function, reg, (unsigned short *) &contents);
      break;
    case 32:
      readConfig32(bus, dev, function, reg, &contents);
      break;
    default:
      kernelError(kernel_error, "Register width %d not supported", bitWidth);
    }

  return (contents);
}


static void driverWriteRegister(int target, int reg, int bitWidth,
				unsigned contents)
{
  // Write the contents of a PCI configuration register

  int bus, dev, function;
  
  makeBusDevFunc(target, bus, dev, function);

  switch (bitWidth)
    {
    case 8:
      writeConfig8(bus, dev, function, reg, contents);
      break;
    case 16:
      writeConfig16(bus, dev, function, reg, contents);
      break;
    case 32:
      writeConfig32(bus, dev, function, reg, contents);
      break;
    default:
      kernelError(kernel_error, "Register width %d not supported", bitWidth);
    }

  return;
}


static int driverDeviceEnable(int target, int enable)
{
  // Enables or disables a PCI bus device

  int bus, dev, function;
  unsigned short commandReg = 0;

  makeBusDevFunc(target, bus, dev, function);

  // Read the command register
  readConfig16(bus, dev, function, PCI_CONFREG_COMMAND, &commandReg);
  
  if (enable)
    // Turn on I/O access, memory access, and bus master enable.
    commandReg |= (PCI_COMMAND_IOENABLE | PCI_COMMAND_MEMORYENABLE |
		   PCI_COMMAND_MASTERENABLE);
  else
    // Turn off I/O access and memory access
    commandReg &= ~(PCI_COMMAND_IOENABLE | PCI_COMMAND_MEMORYENABLE);

  // Write back command register
  writeConfig16(bus, dev, function, PCI_CONFREG_COMMAND, commandReg);

  return (0);
}


static int driverSetMaster(int target, int master)
{
  // Sets the target device as a bus master

  int bus, dev, function;
  unsigned short commandReg = 0;
  unsigned char latency = 0;

  makeBusDevFunc(target, bus, dev, function);

  // Read the command register
  readConfig16(bus, dev, function, PCI_CONFREG_COMMAND, &commandReg);

  // Toggle busmaster bit
  if (master)
    commandReg |= PCI_COMMAND_MASTERENABLE;
  else
    commandReg &= ~PCI_COMMAND_MASTERENABLE;

  // Write back command register
  writeConfig16(bus, dev, function, PCI_CONFREG_COMMAND, commandReg);

  // Check latency timer
  readConfig8(bus, dev, function, PCI_CONFREG_LATENCY, &latency);

  if (latency < 0x10)
    {
      latency = 0x40;
      writeConfig8(bus, dev, function, PCI_CONFREG_LATENCY, latency);
    }

  return (0);
}


static int driverDetect(void *parent, void *driver)
{
  // This routine is used to detect and initialize each device, as well as
  // registering each one with any higher-level interfaces.

  int status = 0;
  unsigned reply = 0;
  int busCount = 0;
  int deviceCount = 0;
  int functionCount = 0;
  char *className = NULL;
  char *subclassName = NULL;
  pciDeviceInfo pciDevice;
  kernelDevice *dev = NULL;

  // Check for a configuration mechanism #1 able PCI controller.
  kernelProcessorOutPort32(PCI_CONFIG_PORT, 0x80000000L);
  kernelProcessorInPort32(PCI_CONFIG_PORT, reply);

  if (reply != 0x80000000)
    // No device that uses configuration mechanism #1.  Fine enough: No PCI
    // functionality for you.
    return (status = 0);

  // First count all the devices on the bus
  for (busCount = 0; busCount < PCI_MAX_BUSES; busCount ++) 
    for (deviceCount = 0; deviceCount < PCI_MAX_DEVICES; deviceCount ++)
      for (functionCount = 0; functionCount < PCI_MAX_FUNCTIONS;
	   functionCount ++) 
	{
	  // Just read the first dword of the header to get the device and
	  // vendor IDs
	  readConfig32(busCount, deviceCount, functionCount, 0,
		       &(pciDevice.header[0]));

	  // See if this is really a device, or if this device header is
	  // unoccupied.
	  if ((pciDevice.device.vendorID == 0x0000) ||
	      (pciDevice.device.vendorID == 0xffff) ||
	      (pciDevice.device.deviceID == 0xffff))
	    // No device here, so try next one
	    continue;

	  // If here, we found a PCI device
	  numTargets += 1;
	}

  // Allocate memory for the targets list
  targets = kernelMalloc(numTargets * sizeof(kernelBusTarget));
  if (targets == NULL)
    return (status = ERR_MEMORY);

  // Now fill up our targets list
  numTargets = 0;
  for (busCount = 0; busCount < PCI_MAX_BUSES; busCount ++) 
    for (deviceCount = 0; deviceCount < PCI_MAX_DEVICES; deviceCount ++)
      for (functionCount = 0; functionCount < PCI_MAX_FUNCTIONS;
	   functionCount ++) 
	{
	  // Just read the first dword of the header to get the device and
	  // vendor IDs
	  readConfig32(busCount, deviceCount, functionCount, 0,
		       &(pciDevice.header[0]));

	  if ((pciDevice.device.vendorID == 0x0000) ||
	      (pciDevice.device.vendorID == 0xffff) ||
	      (pciDevice.device.deviceID == 0xffff))
	    // No device here, so try next one
	    continue;

	  // There's a device.  Get the full device header.
	  readConfigHeader(busCount, deviceCount, functionCount, &pciDevice);

	  getClassName(pciDevice.device.classCode,
		       pciDevice.device.subClassCode, &className,
		       &subclassName);

	  kernelLog("PCI: %s %s %u:%u:%u dev:%04x, vend:%04x, class:%02x, "
		    "sub:%02x", subclassName, className, busCount,
		    deviceCount, functionCount, pciDevice.device.deviceID,
		    pciDevice.device.vendorID, pciDevice.device.classCode,
		    pciDevice.device.subClassCode); 

	  deviceInfo2BusTarget(busCount, deviceCount, functionCount,
			       &pciDevice, &targets[numTargets]);
	  numTargets += 1;
	}

  // Allocate memory for the device
  dev = kernelMalloc(sizeof(kernelDevice));
  if (dev == NULL)
    return (status = ERR_MEMORY);

  dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
  dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_PCI);
  dev->driver = driver;

  status = kernelDeviceAdd(parent, dev);
  if (status < 0)
    return (status);

  status = kernelBusRegister(bus_pci, dev);
  if (status < 0)
    return (status);

  return (status = 0);
}


// Our driver operations structure.
static kernelBusOps pciOps = {
  driverGetTargets,
  driverGetTargetInfo,
  driverReadRegister,
  driverWriteRegister,
  driverDeviceEnable,
  driverSetMaster,
  NULL, // driverRead
  NULL  // driverWrite
};


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


void kernelPciDriverRegister(void *driverData)
{
   // Device driver registration.

  kernelDriver *driver = (kernelDriver *) driverData;

  driver->driverDetect = driverDetect;
  driver->ops = &pciOps;

  return;
}
