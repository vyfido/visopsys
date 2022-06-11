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
//  kernelDevice.c
//

#include "kernelDevice.h"
#include "kernelMalloc.h"
#include "kernelLog.h"
#include "kernelMisc.h"
#include "kernelError.h"
#include "kernelText.h"
#include <stdio.h>
#include <string.h>

// An array of device classes, with names
static kernelDeviceClass allClasses[] = {
  { DEVICECLASS_SYSTEM,   "system"                },
  { DEVICECLASS_CPU,      "CPU"                   },
  { DEVICECLASS_MEMORY,   "memory"                },
  { DEVICECLASS_BUS,      "bus"                   },
  { DEVICECLASS_PIC,      "PIC"                   },
  { DEVICECLASS_SYSTIMER, "system timer"          },
  { DEVICECLASS_RTC,      "real-time clock (RTC)" },
  { DEVICECLASS_DMA,      "DMA controller"        },
  { DEVICECLASS_KEYBOARD, "keyboard"              },
  { DEVICECLASS_MOUSE,    "mouse"                 },
  { DEVICECLASS_DISK,     "disk"                  },
  { DEVICECLASS_GRAPHIC,  "graphic adapter"       },
  { DEVICECLASS_NETWORK,  "network adapter"       },
  { 0, NULL }
};

// An array of device subclasses, with names
static kernelDeviceClass allSubClasses[] = {
  { DEVICESUBCLASS_CPU_X86,             "x86"         },
  { DEVICESUBCLASS_BUS_PCI,             "PCI"         },
  { DEVICESUBCLASS_MOUSE_PS2,           "PS/2"        },
  { DEVICESUBCLASS_MOUSE_SERIAL,        "serial"      },
  { DEVICESUBCLASS_DISK_FLOPPY,         "floppy"      },
  { DEVICESUBCLASS_DISK_IDE,            "IDE"         },
  { DEVICESUBCLASS_DISK_SCSI,           "SCSI"        },
  { DEVICESUBCLASS_GRAPHIC_FRAMEBUFFER, "framebuffer" },
  { DEVICESUBCLASS_NETWORK_ETHERNET,    "ethernet"    },
  { 0, NULL }
};

// Our static list of built-in drivers
static kernelDriver deviceDrivers[] = {
  { DEVICECLASS_CPU, DEVICESUBCLASS_CPU_X86,
    kernelCpuDriverRegister, NULL, NULL                                },
  { DEVICECLASS_MEMORY, 0, kernelMemoryDriverRegister, NULL, NULL      },
  // PIC must be before most drivers so that other ones can unmask
  // interrupts
  { DEVICECLASS_PIC, 0, kernelPicDriverRegister, NULL, NULL            },
  { DEVICECLASS_SYSTIMER, 0, kernelSysTimerDriverRegister, NULL, NULL  },
  { DEVICECLASS_RTC, 0, kernelRtcDriverRegister, NULL, NULL            },
  { DEVICECLASS_DMA, 0, kernelDmaDriverRegister, NULL, NULL            },
  // Do buses before most non-motherboard devices, so that other
  // drivers can find their devices on the buses.
  { DEVICECLASS_BUS, DEVICESUBCLASS_BUS_PCI,
    kernelPciDriverRegister, NULL, NULL                                },
  { DEVICECLASS_KEYBOARD, 0, kernelKeyboardDriverRegister, NULL, NULL  },
  { DEVICECLASS_DISK, DEVICESUBCLASS_DISK_FLOPPY,
    kernelFloppyDriverRegister, NULL, NULL                             },
  { DEVICECLASS_DISK, DEVICESUBCLASS_DISK_IDE,
    kernelIdeDriverRegister, NULL, NULL                                },
  { DEVICECLASS_GRAPHIC, DEVICESUBCLASS_GRAPHIC_FRAMEBUFFER,
    kernelFramebufferGraphicDriverRegister, NULL, NULL                 },
  // Do the mouse device after the graphic device so we can get screen
  // parameters, etc.  Also needs to be after the keyboard driver since
  // PS2 mouses use the keyboard controller.
  { DEVICECLASS_MOUSE, DEVICESUBCLASS_MOUSE_PS2,
    kernelPS2MouseDriverRegister, NULL, NULL                           },
  { DEVICECLASS_NETWORK, DEVICESUBCLASS_NETWORK_ETHERNET,
    kernelLanceDriverRegister, NULL, NULL                              },
  { 0, 0, NULL, NULL, NULL                                             }
};

// Our device tree
static kernelDevice *deviceTree = NULL;
static int numTreeDevices = 0;


static int isDevInTree(kernelDevice *root, kernelDevice *dev)
{
  // This is for checking device pointers passed in from user space to make
  // sure that they point to devices in our tree.

  while (root)
    {
      if (root == dev)
	return (1);
      
      if (root->device.firstChild)
	if (isDevInTree(root->device.firstChild, dev) == 1)
	  return (1);

      root = root->device.next;
    }

  return (0);
}


static int findDeviceType(kernelDevice *dev, kernelDeviceClass *class,
			  kernelDeviceClass *subClass,
			  kernelDevice *devPointers[], int maxDevices,
			  int numDevices)
{
  // Recurses through the device tree rooted at the supplied device and
  // returns the all instances of devices of the requested type

  while (dev)
    {
      if (numDevices >= maxDevices)
	return (numDevices);

      if ((dev->device.class == class) && (dev->device.subClass == subClass))
	devPointers[numDevices++] = dev;

      if (dev->device.firstChild)
	numDevices += findDeviceType(dev->device.firstChild, class, subClass,
				     devPointers, maxDevices, numDevices);

      dev = dev->device.next;
    }

  return (numDevices);
}


static void device2user(kernelDevice *kernel, device *user)
{
  // Convert a kernelDevice structure to the user version
  
  kernelMemClear(user, sizeof(device));

  if (kernel->device.class)
    {
      user->class.class = kernel->device.class->class;
      strncpy(user->class.name, kernel->device.class->name, DEV_CLASSNAME_MAX);
    }

  if (kernel->device.subClass)
    {
      user->subClass.class = kernel->device.subClass->class;
      strncpy(user->subClass.name, kernel->device.subClass->name,
	      DEV_CLASSNAME_MAX);
    }

  if (kernel->device.model)
    strncpy(user->model, kernel->device.model, DEV_MODELNAME_MAX);

  user->parent = kernel->device.parent;
  user->firstChild = kernel->device.firstChild;
  user->next = kernel->device.next;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelDeviceInitialize(void)
{
  // This function is called during startup so we can call the
  // driverRegister() functions of all our drivers

  int status = 0;
  kernelDeviceClass *class = NULL;
  kernelDeviceClass *subClass = NULL;
  char driverString[128];
  int driverCount = 0;

  // Allocate a NULL 'system' device to build our device tree from
  deviceTree = kernelMalloc(sizeof(kernelDevice));
  if (deviceTree == NULL)
    return (status = ERR_MEMORY);

  deviceTree->device.class = kernelDeviceGetClass(DEVICECLASS_SYSTEM);
  numTreeDevices = 1;

  // Loop through our static structure of built-in device drivers and
  // initialize them.
  for (driverCount = 0; (deviceDrivers[driverCount].class != 0);
       driverCount ++)
    {
      if (deviceDrivers[driverCount].driverRegister)
	deviceDrivers[driverCount].driverRegister(&deviceDrivers[driverCount]);
    }

  // Now loop for each hardware driver, and see if it has any devices for us
  for (driverCount = 0; (deviceDrivers[driverCount].class != 0);
       driverCount ++)
    {
      class = NULL;
      class = kernelDeviceGetClass(deviceDrivers[driverCount].class);

      subClass = NULL;
      if (deviceDrivers[driverCount].subClass != 0)
	subClass = kernelDeviceGetClass(deviceDrivers[driverCount].subClass);

      driverString[0] = '\0';
      if (subClass)
	sprintf(driverString, "%s ", subClass->name);
      if (class)
	strcat(driverString, class->name);

      if (deviceDrivers[driverCount].driverDetect == NULL)
	{
	  kernelError(kernel_error, "Device driver for \"%s\" has no 'detect' "
		      "function", driverString);
	  continue;
	}

      status = deviceDrivers[driverCount]
	.driverDetect(&deviceDrivers[driverCount]);
      if (status < 0)
	kernelError(kernel_error, "Error %d detecting \"%s\" devices",
		    status, driverString);
    }

  return (status = 0);
}


kernelDeviceClass *kernelDeviceGetClass(int classNum)
{
  // Given a device (sub)class number, return a pointer to the static class
  // description

  kernelDeviceClass *classList = allClasses;
  int count;

  // Looking for a subclass?
  if ((classNum & DEVICESUBCLASS_MASK) != 0)
    classList = allSubClasses;

  // Loop through the list
  for (count = 0; (classList[count].class != 0) ; count ++)
    if (classList[count].class == classNum)
      return (&classList[count]);

  // Not found
  return (NULL);
}


int kernelDeviceFindType(kernelDeviceClass *class, kernelDeviceClass *subClass,
			 kernelDevice *devPointers[], int maxDevices)
{
  // Calls findDevice to return the first device it finds, with the
  // requested device class and subclass
  return (findDeviceType(deviceTree, class, subClass, devPointers, maxDevices,
			 0));
}


int kernelDeviceAdd(kernelDevice *parent, kernelDevice *new)
{
  // Given a parent device, add the new device as a child

  int status = 0;
  kernelDevice *listPointer = NULL;
  char driverString[128];

  // Check params
  if (new == NULL)
    {
      kernelError(kernel_error, "Device to add is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // NULL parent means use the root system device.
  if (parent == NULL)
    parent = deviceTree;

  new->device.parent = parent;

  driverString[0] = '\0';
  if (new->device.model)
    sprintf(driverString, "\"%s\" ", new->device.model);
  if (new->device.subClass)
    sprintf((driverString + strlen(driverString)), "%s ",
	    new->device.subClass->name);
  if (new->device.class)
    strcat(driverString, new->device.class->name);

  // If the parent has no children, make this the first one.
  if (parent->device.firstChild == NULL)
    parent->device.firstChild = new;

  else
    {
      // The parent has at least one child.  Follow the linked list to the
      // last child.
      listPointer = parent->device.firstChild;
      while (listPointer->device.next != NULL)
	listPointer = listPointer->device.next;

      // listPointer points to the last child.
      listPointer->device.next = new;
    }

  kernelLog("%s device detected", driverString);

  numTreeDevices += 1;
  return (status = 0);
}


int kernelDeviceTreeGetCount(void)
{
  // Returns the number of devices in the kernel's device tree.
  return (numTreeDevices);
}


int kernelDeviceTreeGetRoot(device *rootDev)
{
  // Returns the user-space portion of the device tree root device

  int status = 0;

  // Are we initialized?
  if (deviceTree == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (rootDev == NULL)
    {
      kernelError(kernel_error, "Device pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  device2user(&deviceTree[0], rootDev);
  return (status = 0);
}


int kernelDeviceTreeGetChild(device *parentDev, device *childDev)
{
  // Returns the user-space portion of the supplied device's first child
  // device

  int status = 0;

  // Are we initialized?
  if (deviceTree == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if ((parentDev == NULL) || (childDev == NULL))
    {
      kernelError(kernel_error, "Device pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }
  
  if ((parentDev->firstChild == NULL) ||
      !isDevInTree(deviceTree, parentDev->firstChild))
    return (status = ERR_NOSUCHENTRY);

  device2user(parentDev->firstChild, childDev);
  return (status = 0);
}


int kernelDeviceTreeGetNext(device *siblingDev)
{
  // Returns the user-space portion of the supplied device's 'next' (sibling)
  // device

  int status = 0;

  // Are we initialized?
  if (deviceTree == NULL)
    return (status = ERR_NOTINITIALIZED);

  // Check params
  if (siblingDev == NULL)
    {
      kernelError(kernel_error, "Device pointer is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  if ((siblingDev->next == NULL) || !isDevInTree(deviceTree, siblingDev->next))
    return (status = ERR_NOSUCHENTRY);

  device2user(siblingDev->next, siblingDev);
  return (status = 0);
}
