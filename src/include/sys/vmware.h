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
//  vmware.h
//

// This file contains definitions and structures for communicating with the
// host, when Visopsys is running under VMware.

#ifndef _VMWARE_H
#define _VMWARE_H

// Backdoor access
#define VMWARE_BACKDOOR_MAGIC				0x564D5868
#define VMWARE_BACKDOOR_PORT				0x5658
#define VMWARE_BACKDOOR_HBPORT				0x5659

// Backdoor command codes - partial listing
#define VMWARE_BACKDOORCMD_PROCSPEED		1
#define VMWARE_BACKDOORCMD_APM				2
#define VMWARE_BACKDOORCMD_DISKGEOM			3
#define VMWARE_BACKDOORCMD_GETMOUSEPTR		4
#define VMWARE_BACKDOORCMD_SETMOUSEPTR		5
#define VMWARE_BACKDOORCMD_GETCLIPBLEN		6
#define VMWARE_BACKDOORCMD_GETCLIPBDATA		7
#define VMWARE_BACKDOORCMD_SETCLIPBLEN		8
#define VMWARE_BACKDOORCMD_SETCLIPBDATA		9
#define VMWARE_BACKDOORCMD_VERSION			10
#define VMWARE_BACKDOORCMD_DEVINFO			11
#define VMWARE_BACKDOORCMD_DEVCONNECT		12
#define VMWARE_BACKDOORCMD_GETGUIOPT		13
#define VMWARE_BACKDOORCMD_SETGUIOPT		14
#define VMWARE_BACKDOORCMD_SCREENSIZE		15
#define VMWARE_BACKDOORCMD_HWVERSION		17
#define VMWARE_BACKDOORCMD_OSNOTFOUND		18
#define VMWARE_BACKDOORCMD_FIRMWAREUUID		19
#define VMWARE_BACKDOORCMD_MEMSIZE			20
#define VMWARE_BACKDOORCMD_RPC				30

typedef struct {
	unsigned long regA;
	unsigned long regB;
	unsigned long regC;
	unsigned long regD;
	unsigned long regE;
	unsigned long regF;

} vmwareBackdoorProto;

typedef struct {
	unsigned long regA;
	unsigned long regB;
	unsigned long regC;
	unsigned long regD;
	unsigned long regE;
	unsigned long regF;
	unsigned long regG;

} vmwareBackdoorHbProto;

#endif

