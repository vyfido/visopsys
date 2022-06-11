//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  kernelFilesystemFat.h
//

#if !defined(_KERNELFILESYSTEMFAT_H)

#include "kernelDisk.h"
#include "kernelFilesystem.h"
#include "kernelLock.h"

// Definitions

// FAT-specific Filesystem things
#define FAT_MAX_SECTORSIZE 4096
#define FAT_BYTES_PER_DIR_ENTRY 32
#define FAT_MAX_DIRTY_FATSECTS 32

// File attributes
#define FAT_ATTRIB_READONLY    0x01
#define FAT_ATTRIB_HIDDEN      0x02
#define FAT_ATTRIB_SYSTEM      0x04
#define FAT_ATTRIB_VOLUMELABEL 0x08
#define FAT_ATTRIB_SUBDIR      0x10
#define FAT_ATTRIB_ARCHIVE     0x20

// Filesystem metadata offsets
#define FAT_BS_OEMNAME       3
#define FAT_BS_BYTESPERSECT  11
#define FAT_BS_SECPERCLUST   13
#define FAT_BS_RESERVEDSECS  14
#define FAT_BS_NUMFATS       16
#define FAT_BS_ROOTENTRIES   17
#define FAT_BS_TOTALSECS16   19
#define FAT_BS_MEDIABYTE     21
#define FAT_BS_FATSIZE16     22
#define FAT_BS_SECSPERCYL    24
#define FAT_BS_NUMHEADS      26
#define FAT_BS_HIDDENSECS    28
#define FAT_BS_TOTALSECS32   32
// These are FAT12/FAT16
#define FAT_BS_DRIVENUM      36
#define FAT_BS_RESERVED1     37
#define FAT_BS_BOOTSIG       38
#define FAT_BS_VOLID         39
#define FAT_BS_VOLLABEL      43
#define FAT_BS_FILESYSTYPE   54
// These are FAT32
#define FAT_BS32_FATSIZE     36
#define FAT_BS32_EXTFLAGS    40
#define FAT_BS32_FSVERSION   42
#define FAT_BS32_ROOTCLUST   44
#define FAT_BS32_FSINFO      48
#define FAT_BS32_BACKUPBOOT  50
#define FAT_BS32_RESERVED2   52
#define FAT_BS32_DRIVENUM    64
#define FAT_BS32_RESERVED1   65
#define FAT_BS32_BOOTSIG     66
#define FAT_BS32_VOLID       67
#define FAT_BS32_VOLLABEL    71
#define FAT_BS32_FILESYSTYPE 82
// These are in the FSInfo structure
#define FAT_FSINFO_LEADSIG   0
#define FAT_FSINFO_RESERVED1 4
#define FAT_FSINFO_STRUCSIG  484
#define FAT_FSINFO_FREECOUNT 488
#define FAT_FSINFO_NEXTFREE  492
#define FAT_FSINFO_RESERVED2 496
#define FAT_FSINFO_TRAILSIG  508

// Structures used internally by the filesystem driver to keep track
// of files and directories

typedef enum
{
  fat12, fat16, fat32, fatUnknown

} fatType;

typedef volatile struct
{
  // These are taken directly from directory entries
  unsigned char shortAlias[12];
  unsigned attributes;
  unsigned res;
  unsigned timeTenth;
  unsigned startCluster;
 
} fatEntryData;

// This structure will contain all of the internal global data
// for a particular filesystem on a particular volume
typedef volatile struct
{
  // Variables for storing information about the current volume.  All
  // of the ones in this list are read directly from the boot block
  // at mount time.
  unsigned char oemName[9];
  unsigned bytesPerSector;
  unsigned sectorsPerCluster;
  unsigned reservedSectors;
  unsigned numberOfFats;
  unsigned rootDirEntries;
  unsigned mediaType;
  unsigned fatSectors;
  unsigned sectorsPerTrack;
  unsigned heads;
  unsigned hiddenSectors;
  unsigned driveNumber;
  unsigned bootSignature;
  unsigned volumeId;
  unsigned char volumeLabel[12];
  unsigned char fsSignature[9];

  // These fields are specific to the FAT32 filesystem type, and
  // are not applicable to FAT12 or FAT16
  unsigned rootDirClusterF32;
  unsigned fsInfoSectorF32;
  unsigned backupBootF32;
  unsigned firstFreeClusterF32;

  // Things that need to be calculated after we have all of the FAT
  // volume data from the boot block (see above)
  fatType fsType;
  unsigned terminalCluster;
  unsigned totalSectors;
  unsigned rootDirSectors;
  unsigned dataSectors;
  unsigned dataClusters;

  // The FAT table itself
  unsigned char *FAT;
  unsigned dirtyFatSectList[FAT_MAX_DIRTY_FATSECTS];
  unsigned numDirtyFatSects;

  // Bitmap of free clusters
  unsigned char *freeClusterBitmap;
  unsigned freeBitmapSize;
  unsigned freeClusters;
  int buildingFreeBitmap;
  kernelLock freeBitmapLock;

  // Miscellany
  const kernelDisk *diskObject;
  
} fatInternalData;

// Functions exported by kernelFileSystemFat.c, not defined elsewhere.

int kernelFilesystemFatDetect(const kernelDisk *);
int kernelFilesystemFatFormat(kernelDisk *, const char *, const char *, int);
int kernelFilesystemFatCheck(kernelFilesystem *, int, int);
int kernelFilesystemFatMount(kernelFilesystem *);
int kernelFilesystemFatSync(kernelFilesystem *);
int kernelFilesystemFatUnmount(kernelFilesystem *);
unsigned kernelFilesystemFatGetFreeBytes(kernelFilesystem *);
int kernelFilesystemFatNewEntry(kernelFileEntry *);
int kernelFilesystemFatInactiveEntry(kernelFileEntry *);
int kernelFilesystemFatReadFile(kernelFileEntry *, unsigned, unsigned,
				unsigned char *);
int kernelFilesystemFatWriteFile(kernelFileEntry *, unsigned, unsigned,
				 unsigned char *);
int kernelFilesystemFatCreateFile(kernelFileEntry *);
int kernelFilesystemFatDeleteFile(kernelFileEntry *, int);
int kernelFilesystemFatFileMoved(kernelFileEntry *);
int kernelFilesystemFatReadDir(kernelFileEntry *);
int kernelFilesystemFatWriteDir(kernelFileEntry *);
int kernelFilesystemFatMakeDir(kernelFileEntry *);
int kernelFilesystemFatRemoveDir(kernelFileEntry *);
int kernelFilesystemFatTimestamp(kernelFileEntry *);

#define _KERNELFILESYSTEMFAT_H
#endif
