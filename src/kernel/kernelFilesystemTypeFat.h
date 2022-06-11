//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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

// Driver things
#define MAX_FATSECTORS 100

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

// Error messages
#define FAT_NO_BOOTSECTOR "Unable to gather information about the FAT filesystem from the boot block"
#define FAT_NO_FSINFOSECTOR "Unable to read or write the FAT32 FSInfo structure after the boot block"
#define FAT_NULL_DISK_OBJECT "The disk object passed or referenced is NULL"
#define FAT_NULL_DRIVER "The disk object passed or referenced has a NULL driver"
#define FAT_NULL_DRIVER_ROUTINE "The disk object passed or referenced has a NULL driver routine"
#define FAT_BAD_VOLUME "The FAT volume is corrupt"
#define FAT_MEDIA_REMOVED "Mounted media has been changed or removed.  No operations until replaced"
#define FAT_NOT_ENOUGH_FREE "There is not enough free space on the volume to complete the operation"
#define FAT_CANT_OVERWRITE "The target file exists and cannot be overwritten"
#define FAT_MEMORY "Could not allocate the memory needed to complete the operation"
#define FAT_ALIGN_CLUSTERS "Number of pages does not correspond to an even number of clusters"

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
  unsigned int attributes;
  unsigned int res;
  unsigned int timeTenth;
  unsigned int startCluster;
 
} fatEntryData;


typedef volatile struct
{
  // This little structure is used to contain an individual sector
  // of the FAT

  unsigned int index;
  unsigned char *data;
  int dirty;
  unsigned int lastAccess;

} fatSector;


// This structure will contain all of the internal global data
// for a particular filesystem on a particular volume
typedef volatile struct
{
  // Buffer for keeping track of FAT sectors in a single filesystem.
  fatSector FAT[MAX_FATSECTORS];
  unsigned char *fatBuffer;
  unsigned int fatBufferSize;
  unsigned int fatSectorsBuffered;
  
  // Bitmap of free clusters
  unsigned char *freeClusterBitmap;
  unsigned int freeBitmapSize;
  unsigned int freeClusters;
  int buildingFreeBitmap;
  int freeBitmapLock;

  // Variables for storing information about the current volume.  All
  // of the ones in this list are read directly from the boot block
  // at mount time.
  unsigned int bytesPerSector;
  unsigned int sectorsPerCluster;
  unsigned int reservedSectors;
  unsigned int numberOfFats;
  unsigned int rootDirEntries;
  unsigned int totalSectors16;
  unsigned int mediaType;
  unsigned int fatSectors;
  unsigned int sectorsPerTrack;
  unsigned int heads;
  unsigned int hiddenSectors;
  unsigned int totalSectors32;
  unsigned int driveNumber;
  unsigned int bootSignature;
  unsigned int volumeId;
  unsigned char volumeLabel[12];
  unsigned char fsSignature[9];

  // These fields are specific to the FAT32 filesystem type, and
  // are not applicable to FAT12 or FAT16
  unsigned int rootDirClusterF32;
  unsigned int fsInfoSectorF32;
  unsigned int freeClusterCountF32;
  unsigned int firstFreeClusterF32;

  // Things that need to be calculated after we have all of the FAT
  // volume data from the boot block (see above)
  fatType fsType;
  unsigned int terminalCluster;
  unsigned int totalSectors;
  unsigned int rootDirSectors;
  unsigned int dataSectors;
  unsigned int dataClusters;

  // Miscellany
  int changedLock;
  const kernelDiskObject *diskObject;
  
} fatInternalData;


// Functions exported by kernelFileSystemTypeFat.c
int kernelFilesystemTypeFatDetect(const kernelDiskObject *);
int kernelFilesystemTypeFatCheck(const kernelDiskObject *);
int kernelFilesystemTypeFatMount(kernelFilesystem *);
int kernelFilesystemTypeFatSync(kernelFilesystem *);
int kernelFilesystemTypeFatUnmount(kernelFilesystem *);
unsigned int kernelFilesystemTypeFatGetFreeBytes(kernelFilesystem *);
int kernelFilesystemTypeFatNewEntry(kernelFileEntry *);
int kernelFilesystemTypeFatInactiveEntry(kernelFileEntry *);
int kernelFilesystemTypeFatReadFile(kernelFileEntry *, unsigned int,
				    unsigned int, unsigned char *);
int kernelFilesystemTypeFatWriteFile(kernelFileEntry *, unsigned int,
				     unsigned int, unsigned char *);
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
