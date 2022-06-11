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
//  disk.h
//

// This file contains definitions and structures for using and manipulating
// disks in Visopsys.

#ifndef _DISK_H
#define _DISK_H

#include <sys/file.h>
#include <sys/guid.h>
#include <sys/paths.h>
#include <sys/types.h>

#define DISK_MOUNT_CONFIG			PATH_SYSTEM_CONFIG "/mount.conf"
#define DISK_MAXDEVICES				32
#define DISK_MAX_NAMELENGTH			15
#define DISK_MAX_MODELLENGTH		40
#define DISK_MAX_PARTITIONS			16
#define DISK_MAX_PRIMARY_PARTITIONS	4
#define DISK_MAX_CACHE				1048576 // 1 Meg
#define DISK_NAME_PREFIX_FLOPPY		"fd"
#define DISK_NAME_PREFIX_CDROM		"cd"
#define DISK_NAME_PREFIX_SCSIDISK	"sd"
#define DISK_NAME_PREFIX_HARDDISK	"hd"
#define DISK_NAME_PREFIX_RAMDISK	"rd"
#define FSTYPE_MAX_NAMELENGTH		31

// Flags for supported filesystem operations on a partition
#define FS_OP_FORMAT				0x01
#define FS_OP_CLOBBER				0x02
#define FS_OP_CHECK					0x04
#define FS_OP_DEFRAG				0x08
#define FS_OP_STAT					0x10
#define FS_OP_RESIZECONST			0x20
#define FS_OP_RESIZE				0x40

// Flags to describe what type of disk is described by a disk structure
#define DISKTYPE_PHYSLOG_MASK		0xF0000000
#define DISKTYPE_LOGICAL			0x20000000
#define DISKTYPE_PHYSICAL			0x10000000
#define DISKTYPE_PRIMLOG_MASK		0x0F000000
#define DISKTYPE_PRIMARY			0x01000000
#define DISKTYPE_FIXEDREM_MASK		0x00F00000
#define DISKTYPE_FIXED				0x00200000
#define DISKTYPE_REMOVABLE			0x00100000
#define DISKTYPE_HARDWARE_MASK		0x0000FFFF
#define DISKTYPE_RAMDISK			0x00000200
#define DISKTYPE_FLOPPY				0x00000100
#define DISKTYPE_USBCDROM			0x00000080
#define DISKTYPE_SCSICDROM			0x00000040
#define DISKTYPE_SATACDROM			0x00000020
#define DISKTYPE_IDECDROM			0x00000010
#define DISKTYPE_CDROM \
	(DISKTYPE_USBCDROM | DISKTYPE_SCSICDROM | DISKTYPE_SATACDROM | \
	DISKTYPE_IDECDROM)
#define DISKTYPE_FLASHDISK			0x00000008
#define DISKTYPE_SCSIDISK			0x00000004
#define DISKTYPE_SATADISK			0x00000002
#define DISKTYPE_IDEDISK			0x00000001
#define DISKTYPE_HARDDISK \
	(DISKTYPE_FLASHDISK | DISKTYPE_SCSIDISK | DISKTYPE_SATADISK | \
	DISKTYPE_IDEDISK)

// Flags to describe the current state of the disk
#define DISKFLAG_NOCACHE			0x10
#define DISKFLAG_READONLY			0x08
#define DISKFLAG_MOTORON			0x04
#define DISKFLAG_DOORLOCKED			0x02
#define DISKFLAG_DOOROPEN			0x01
#define DISKFLAG_USERSETTABLE		(DISKFLAG_NOCACHE | DISKFLAG_READONLY)

// This structure is used to describe an MS-DOS partition tag
typedef struct {
	unsigned char tag;
	const char description[FSTYPE_MAX_NAMELENGTH + 1];

} msdosPartType;

// This structure is used to describe a GPT partition type GUID
typedef struct {
	guid typeGuid;
	const char description[FSTYPE_MAX_NAMELENGTH + 1];

} gptPartType;

typedef struct {
	char name[DISK_MAX_NAMELENGTH + 1];
	int deviceNumber;
	unsigned type;
	char model[DISK_MAX_MODELLENGTH];
	unsigned flags;
	char partType[FSTYPE_MAX_NAMELENGTH + 1];
	char fsType[FSTYPE_MAX_NAMELENGTH + 1];
	unsigned opFlags;

	unsigned heads;
	unsigned cylinders;
	unsigned sectorsPerCylinder;
	unsigned sectorSize;

	uquad_t startSector;
	uquad_t numSectors;

	// Filesystem related
	char label[MAX_NAME_LENGTH + 1];
	unsigned blockSize;
	uquad_t freeBytes;
	uquad_t minSectors;  // for
	uquad_t maxSectors;  // resize
	int mounted;
	char mountPoint[MAX_PATH_LENGTH + 1];
	int readOnly;

} disk;

typedef struct {
	// Throughput measurement.
	unsigned readTimeMs;
	unsigned readKbytes;
	unsigned writeTimeMs;
	unsigned writeKbytes;

} diskStats;

#define CYLSECTS(d) ((d)->heads * (d)->sectorsPerCylinder)

#endif

