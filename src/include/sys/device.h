//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  device.h
//

// This file contains definitions and structures for using Visopsys hardware
// devices

#ifndef _DEVICE_H
#define _DEVICE_H

#include <sys/vis.h>

#define DEV_CLASSNAME_MAX					31

// Hardware device classes and subclasses
#define DEVICECLASS_NONE					0
#define DEVICECLASS_CPU						0x0100
#define DEVICECLASS_MEMORY					0x0200
#define DEVICECLASS_SYSTEM					0x0300
#define DEVICECLASS_POWER					0x0400
#define DEVICECLASS_BUS						0x0500
#define DEVICECLASS_BRIDGE					0x0600
#define DEVICECLASS_INTCTRL					0x0700
#define DEVICECLASS_SYSTIMER				0x0800
#define DEVICECLASS_RTC						0x0900
#define DEVICECLASS_DMA						0x0A00
#define DEVICECLASS_DISKCTRL				0x0B00
#define DEVICECLASS_KEYBOARD				0x0C00
#define DEVICECLASS_MOUSE					0x0D00
#define DEVICECLASS_TOUCHSCR				0x0E00
#define DEVICECLASS_DISK					0x0F00
#define DEVICECLASS_GRAPHIC					0x1000
#define DEVICECLASS_NETWORK					0x1100
#define DEVICECLASS_HUB						0x1200
#define DEVICECLASS_STORAGE					0x1300
#define DEVICECLASS_UNKNOWN					0xFF00

// Device sub-classes
#define DEVICESUBCLASS_NONE					0

// Sub-classes of CPUs
#define DEVICESUBCLASS_CPU_X86				(DEVICECLASS_CPU | 0x01)
#define DEVICESUBCLASS_CPU_X86_64			(DEVICECLASS_CPU | 0x02)

// System device subclasses
#define DEVICESUBCLASS_SYSTEM_BIOS			(DEVICECLASS_SYSTEM | 0x01)
#define DEVICESUBCLASS_SYSTEM_BIOS32		(DEVICECLASS_SYSTEM | 0x02)
#define DEVICESUBCLASS_SYSTEM_BIOSPNP		(DEVICECLASS_SYSTEM | 0x03)
#define DEVICESUBCLASS_SYSTEM_MULTIPROC		(DEVICECLASS_SYSTEM | 0x04)

// Sub-classes of power management
#define DEVICESUBCLASS_POWER_ACPI			(DEVICECLASS_POWER | 0x01)

// Sub-classes of buses
#define DEVICESUBCLASS_BUS_PCI				(DEVICECLASS_BUS | 0x01)
#define DEVICESUBCLASS_BUS_USB				(DEVICECLASS_BUS | 0x02)

// Sub-classes of bridges
#define DEVICESUBCLASS_BRIDGE_PCI			(DEVICECLASS_BRIDGE | 0x01)
#define DEVICESUBCLASS_BRIDGE_ISA			(DEVICECLASS_BRIDGE | 0x02)

// Sub-classes of PICs
#define DEVICESUBCLASS_INTCTRL_PIC			(DEVICECLASS_INTCTRL | 0x01)
#define DEVICESUBCLASS_INTCTRL_APIC			(DEVICECLASS_INTCTRL | 0x02)

// Sub-classes of disk controllers
#define DEVICESUBCLASS_DISKCTRL_IDE			(DEVICECLASS_DISKCTRL | 0x01)
#define DEVICESUBCLASS_DISKCTRL_SATA		(DEVICECLASS_DISKCTRL | 0x02)
#define DEVICESUBCLASS_DISKCTRL_SCSI		(DEVICECLASS_DISKCTRL | 0x03)

// Sub-classes of keyboards
#define DEVICESUBCLASS_KEYBOARD_PS2			(DEVICECLASS_KEYBOARD | 0x01)
#define DEVICESUBCLASS_KEYBOARD_USB			(DEVICECLASS_KEYBOARD | 0x02)

// Sub-classes of mice
#define DEVICESUBCLASS_MOUSE_PS2			(DEVICECLASS_MOUSE | 0x01)
#define DEVICESUBCLASS_MOUSE_SERIAL			(DEVICECLASS_MOUSE | 0x02)
#define DEVICESUBCLASS_MOUSE_USB			(DEVICECLASS_MOUSE | 0x03)

// Sub-classes of touchscreens
#define DEVICESUBCLASS_TOUCHSCR_USB			(DEVICECLASS_TOUCHSCR | 0x01)

// Sub-classes of disks
#define DEVICESUBCLASS_DISK_RAMDISK			(DEVICECLASS_DISK | 0x01)
#define DEVICESUBCLASS_DISK_FLOPPY			(DEVICECLASS_DISK | 0x02)
#define DEVICESUBCLASS_DISK_IDE				(DEVICECLASS_DISK | 0x03)
#define DEVICESUBCLASS_DISK_SATA			(DEVICECLASS_DISK | 0x04)
#define DEVICESUBCLASS_DISK_SCSI			(DEVICECLASS_DISK | 0x05)
#define DEVICESUBCLASS_DISK_CDDVD			(DEVICECLASS_DISK | 0x06)

// Sub-classes of graphics adapters
#define DEVICESUBCLASS_GRAPHIC_FRAMEBUFFER	(DEVICECLASS_GRAPHIC | 0x01)

// Sub-classes of network adapters
#define DEVICESUBCLASS_NETWORK_ETHERNET		(DEVICECLASS_NETWORK | 0x01)
#define DEVICESUBCLASS_NETWORK_WIRELESS		(DEVICECLASS_NETWORK | 0x02)

// Sub-classes of hubs
#define DEVICESUBCLASS_HUB_USB				(DEVICECLASS_HUB | 0x01)

// Sub-classes of storage
#define DEVICESUBCLASS_STORAGE_FLASH		(DEVICECLASS_STORAGE | 0x01)
#define DEVICESUBCLASS_STORAGE_TAPE			(DEVICECLASS_STORAGE | 0x02)

// Sub-classes of unknown things
#define DEVICESUBCLASS_UNKNOWN_USB			(DEVICECLASS_UNKNOWN | 0x01)

// For masking off class/subclass
#define DEVICECLASS_MASK					0xFF00
#define DEVICESUBCLASS_MASK					0x00FF

// A list of standard device attribute names
#define DEVICEATTRNAME_VENDOR				"vendor.name"
#define DEVICEATTRNAME_MODEL				"model.name"

// A structure for device classes and subclasses, which just allows us to
// associate the different types with string names.
typedef struct {
	int classNum;
	char name[DEV_CLASSNAME_MAX + 1];

} deviceClass;

// The generic hardware device structure
typedef struct {
	// Device class and subclass.  Subclass optional.
	deviceClass devClass;
	deviceClass subClass;

	// Optional list of text attributes
	variableList attrs;

	// Used for maintaining the list of devices as a tree
	void *parent;
	void *firstChild;
	void *previous;
	void *next;

} device;

#endif

