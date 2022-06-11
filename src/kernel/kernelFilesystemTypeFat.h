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
//  kernelFilesystemTypeFat.h
//

#if !defined(_KERNELFILESYSTEMTYPEFAT_H)

#include "kernelDiskFunctions.h"
#include "kernelFilesystem.h"

// Definitions
#define MAX_FATSECTORS 300

// FAT-specific Filesystem things
#define FAT_MAX_SECTORSIZE 4096
#define FAT_BYTES_PER_DIR_ENTRY 32

// File attributes
#define FAT_ATTRIB_READONLY    0x01
#define FAT_ATTRIB_HIDDEN      0x02
#define FAT_ATTRIB_SYSTEM      0x04
#define FAT_ATTRIB_VOLUMELABEL 0x08
#define FAT_ATTRIB_SUBDIR      0x10
#define FAT_ATTRIB_ARCHIVE     0x20

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

typedef volatile struct
{
  // This little structure is used to contain an individual sector
  // of the FAT
  unsigned index;
  unsigned char *data;
  int dirty;
  unsigned lastAccess;

} fatSector;

// This structure will contain all of the internal global data
// for a particular filesystem on a particular volume
typedef volatile struct
{
  // Buffer for keeping track of FAT sectors in a single filesystem.
  fatSector FAT[MAX_FATSECTORS];  // This looks like a problem.  Or something.
  unsigned char *fatBuffer;
  unsigned fatBufferSize;
  unsigned fatSectorsBuffered;
  
  // Bitmap of free clusters
  unsigned char *freeClusterBitmap;
  unsigned freeBitmapSize;
  unsigned freeClusters;
  int buildingFreeBitmap;
  int freeBitmapLock;

  // Variables for storing information about the current volume.  All
  // of the ones in this list are read directly from the boot block
  // at mount time.
  unsigned bytesPerSector;
  unsigned sectorsPerCluster;
  unsigned reservedSectors;
  unsigned numberOfFats;
  unsigned rootDirEntries;
  unsigned totalSectors16;
  unsigned mediaType;
  unsigned fatSectors;
  unsigned sectorsPerTrack;
  unsigned heads;
  unsigned hiddenSectors;
  unsigned totalSectors32;
  unsigned driveNumber;
  unsigned bootSignature;
  unsigned volumeId;
  unsigned char volumeLabel[12];
  unsigned char fsSignature[9];

  // These fields are specific to the FAT32 filesystem type, and
  // are not applicable to FAT12 or FAT16
  unsigned rootDirClusterF32;
  unsigned fsInfoSectorF32;
  unsigned freeClusterCountF32;
  unsigned firstFreeClusterF32;

  // Things that need to be calculated after we have all of the FAT
  // volume data from the boot block (see above)
  fatType fsType;
  unsigned terminalCluster;
  unsigned totalSectors;
  unsigned rootDirSectors;
  unsigned dataSectors;
  unsigned dataClusters;

  // Miscellany
  int changedLock;
  const kernelDiskObject *diskObject;
  
} fatInternalData;

// Functions exported by kernelFileSystemTypeFat.c
int kernelFilesystemTypeFatDetect(const kernelDiskObject *);
int kernelFilesystemTypeFatCheck(kernelFilesystem *, int, int);
int kernelFilesystemTypeFatMount(kernelFilesystem *);
int kernelFilesystemTypeFatSync(kernelFilesystem *);
int kernelFilesystemTypeFatUnmount(kernelFilesystem *);
unsigned kernelFilesystemTypeFatGetFreeBytes(kernelFilesystem *);
int kernelFilesystemTypeFatNewEntry(kernelFileEntry *);
int kernelFilesystemTypeFatInactiveEntry(kernelFileEntry *);
int kernelFilesystemTypeFatReadFile(kernelFileEntry *, unsigned,
				    unsigned, unsigned char *);
int kernelFilesystemTypeFatWriteFile(kernelFileEntry *, unsigned,
				     unsigned, unsigned char *);
int kernelFilesystemTypeFatCreateFile(kernelFileEntry *);
int kernelFilesystemTypeFatDeleteFile(kernelFileEntry *, int);
int kernelFilesystemTypeFatFileMoved(kernelFileEntry *);
int kernelFilesystemTypeFatReadDir(kernelFileEntry *);
int kernelFilesystemTypeFatWriteDir(kernelFileEntry *);
int kernelFilesystemTypeFatMakeDir(kernelFileEntry *);
int kernelFilesystemTypeFatRemoveDir(kernelFileEntry *);
int kernelFilesystemTypeFatTimestamp(kernelFileEntry *);

#define _KERNELFILESYSTEMTYPEFAT_H
#endif
