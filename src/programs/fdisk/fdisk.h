//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  fdisk.h
//

// This is the header for the "Disk Manager" program, fdisk.c

#if !defined(_FDISK_H)

#include <sys/disk.h>
#include <sys/image.h>
#include <sys/progress.h>

#define TEMP_DIR                       "/temp"
#define BOOT_DIR                       "/system/boot"
#define BACKUP_MBR                     BOOT_DIR"/backup-%s.mbr"
#define SIMPLE_MBR_FILE                BOOT_DIR"/mbr.simple"

#define ENTRYOFFSET_DRV_ACTIVE         0
#define ENTRYOFFSET_START_HEAD         1
#define ENTRYOFFSET_START_CYLSECT      2
#define ENTRYOFFSET_START_CYL          3
#define ENTRYOFFSET_TYPE               4
#define ENTRYOFFSET_END_HEAD           5
#define ENTRYOFFSET_END_CYLSECT        6
#define ENTRYOFFSET_END_CYL            7
#define ENTRYOFFSET_START_LBA          8
#define ENTRYOFFSET_SIZE_LBA           12
#define MAX_SLICES                     ((DISK_MAX_PARTITIONS * 2) + 1)
#define MAX_DESCSTRING_LENGTH          128

#ifdef PARTLOGIC
#define SLICESTRING_DISKFIELD_WIDTH    3
#else
#define SLICESTRING_DISKFIELD_WIDTH    5
#endif
#define SLICESTRING_LABELFIELD_WIDTH   22
#define SLICESTRING_FSTYPEFIELD_WIDTH  11
#define SLICESTRING_CYLSFIELD_WIDTH    14
#define SLICESTRING_SIZEFIELD_WIDTH    9
#define SLICESTRING_ATTRIBFIELD_WIDTH  15
#define SLICESTRING_LENGTH             (SLICESTRING_DISKFIELD_WIDTH +   \
					SLICESTRING_LABELFIELD_WIDTH +  \
					SLICESTRING_FSTYPEFIELD_WIDTH + \
					SLICESTRING_CYLSFIELD_WIDTH +   \
					SLICESTRING_SIZEFIELD_WIDTH +   \
					SLICESTRING_ATTRIBFIELD_WIDTH)

#define ISLOGICAL(slc) ((slc)->raw.type == partition_logical)

// Types of slices
typedef enum {
  partition_none = 0,
  partition_primary,
  partition_logical,
  partition_any
} sliceType;

// Slice flags
#define SLICEFLAG_BOOTABLE  0x01

// This structure represents the disk geometry of a raw slice
typedef struct {
  unsigned startCylinder;
  unsigned startHead;
  unsigned startSector;
  unsigned endCylinder;
  unsigned endHead;
  unsigned endSector;

} rawGeom;  

// This stucture represents a "raw slice", containing just the information
// passed between the main program and the disk-label-specific code in,
// for example, msdos.c or gpt.c, to represent either a partition or empty
// space.
typedef struct {
  int order;
  sliceType type;
  unsigned flags;
  unsigned tag;
  unsigned startLogical;
  unsigned sizeLogical;
  rawGeom geom;

} rawSlice;

// This stucture represents both used partitions and empty spaces.
typedef struct {
  // This comes directly from the disk label
  rawSlice raw;
  // Below here, the fields are generated internally
  char diskName[6];
  char showSliceName[6];
  unsigned opFlags;
  char fsType[FSTYPE_MAX_NAMELENGTH];
  char string[MAX_DESCSTRING_LENGTH];
  int pixelX;
  int pixelWidth;
  color *color;

} slice;

// Types of disk labels
typedef enum {
  label_none,
  label_msdos,
  label_gpt
} labelType;

// Label flags
#define LABELFLAG_PRIMARYPARTS  0x01
#define LABELFLAG_LOGICALPARTS  0x02

struct _partitionTable;

// This structure represents a generic disk label.
typedef struct {
  labelType type;
  unsigned flags;

  // Disk label operations
  int (*detect) (const disk *);
  int (*read) (const disk *, rawSlice *, int *);
  int (*write) (const disk *, rawSlice *);
  sliceType (*canCreate) (slice *, int, int);

} diskLabel;

typedef struct {
  disk *disk;
  int diskNumber;
  diskLabel *label;
  rawSlice rawSlices[DISK_MAX_PARTITIONS];
  int numRawSlices;
  slice slices[MAX_SLICES];
  int numSlices;
  int selectedSlice;
  int changesPending;
  int backupAvailable;

} partitionTable;

// A struct for managing concurrent read/write IO during things like
// disk-to-disk copies
typedef struct {
  struct {
    unsigned char *data;
    int full;
  } buffer[2];
  unsigned bufferSize;

} ioBuffer;

// Arguments for the reader/writer threads during things like disk-to-disk
// copies
typedef struct {
  disk *theDisk;
  unsigned startSector;
  unsigned numSectors;
  ioBuffer *buffer;
  progress *prog;

} ioThreadArgs;

// Functions exported from fdisk.c
void pause(void);
void error(const char *, ...) __attribute__((format(printf, 1, 2)));
void warning(const char *, ...) __attribute__((format(printf, 1, 2)));

#define _FDISK_H
#endif
