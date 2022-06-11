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
//  kernelFilesystem.h
//

#if !defined(_KERNELFILESYSTEM_H)

#include "kernelDiskFunctions.h"
#include <sys/file.h>

// Definitions
#define MAX_FILESYSTEMS 32
#define MAX_FS_NAME_LENGTH 64

// These functions are the common routines for the various types of
// filesystems
#define FATDETECT &kernelFilesystemTypeFatDetect
#define FATCHECK &kernelFilesystemTypeFatCheck
#define FATDEFRAG NULL
#define FATMOUNT &kernelFilesystemTypeFatMount
#define FATSYNC &kernelFilesystemTypeFatSync
#define FATUNMOUNT &kernelFilesystemTypeFatUnmount
#define FATGETFREE &kernelFilesystemTypeFatGetFreeBytes
#define FATNEWENTRY &kernelFilesystemTypeFatNewEntry
#define FATINACTIVE &kernelFilesystemTypeFatInactiveEntry
#define FATREADFILE &kernelFilesystemTypeFatReadFile
#define FATWRITEFILE &kernelFilesystemTypeFatWriteFile
#define FATCREATEFILE &kernelFilesystemTypeFatCreateFile
#define FATDELETEFILE &kernelFilesystemTypeFatDeleteFile
#define FATFILEMOVED &kernelFilesystemTypeFatFileMoved
#define FATREADDIR &kernelFilesystemTypeFatReadDir
#define FATWRITEDIR &kernelFilesystemTypeFatWriteDir
#define FATMAKEDIR &kernelFilesystemTypeFatMakeDir
#define FATREMOVEDIR &kernelFilesystemTypeFatRemoveDir
#define FATTIMESTAMP &kernelFilesystemTypeFatTimestamp

// This structure defines a file or directory entry
typedef volatile struct
{
  unsigned char fileName[MAX_NAME_LENGTH];
  fileType type;
  int flags;
  unsigned creationTime;
  unsigned creationDate;
  unsigned accessedTime;
  unsigned accessedDate;
  unsigned modifiedTime;
  unsigned modifiedDate;
  unsigned size;
  unsigned blocks;

  // Misc
  void *filesystem;     // parent filesystem
  void *fileEntryData;  // private fs-specific data
  int openCount;
  unsigned lockedByProcess;

  // Linked-list stuff.
  void *parentDirectory;
  void *previousEntry;
  void *nextEntry;
  unsigned lastAccess;

  // (The following additional stuff only applies to directory entries)
  void *firstFile;
  int dirty;

} kernelFileEntry;

// This is an enumeration that lists the names of the known types of 
// filesystems.
typedef enum
{
  unknown, Fat
  
} kernelFileSysTypeEnum;

// This is the structure that is used to define file system objects
typedef volatile struct
{
  int filesystemNumber;
  const char *description;
  char mountPoint[MAX_FS_NAME_LENGTH];
  const kernelDiskObject *disk;
  void *filesystemDriver;
  kernelFileEntry *filesystemRoot;
  unsigned blockSize;
  int syncLock;
  int caseInsensitive;
  int readOnly;
  int hasSyncErrors;
  void *filesystemData;

} kernelFilesystem;

// This is the structure that is used to define a file system
// driver
typedef struct
{
  kernelFileSysTypeEnum driverType;
  char *driverTypeName;
  int (*driverDetect) (const kernelDiskObject *);
  int (*driverCheck) (kernelFilesystem *, int, int);
  int (*driverDefragment) (kernelFilesystem *);
  int (*driverMount) (kernelFilesystem *);
  int (*driverSync) (kernelFilesystem *);
  int (*driverUnmount) (kernelFilesystem *);
  unsigned (*driverGetFree) (kernelFilesystem *);
  int (*driverNewEntry) (kernelFileEntry *);
  int (*driverInactiveEntry) (kernelFileEntry *);
  int (*driverReadFile) (kernelFileEntry *, unsigned, unsigned,
			 unsigned char *);
  int (*driverWriteFile) (kernelFileEntry *, unsigned, unsigned,
			  unsigned char *);
  int (*driverCreateFile) (kernelFileEntry *);
  int (*driverDeleteFile) (kernelFileEntry *, int);
  int (*driverFileMoved) (kernelFileEntry *);
  int (*driverReadDir) (kernelFileEntry *);
  int (*driverWriteDir) (kernelFileEntry *);
  int (*driverMakeDir) (kernelFileEntry *);
  int (*driverRemoveDir) (kernelFileEntry *);
  int (*driverTimestamp) (kernelFileEntry *);

} kernelFilesystemDriver;

// Functions exported by kernelFilesystem.c
int kernelFilesystemInitialize(void);
int kernelFilesystemCheck(int, int, int);
int kernelFilesystemDefragment(int);
int kernelFilesystemMount(int, const char *);
int kernelFilesystemSync(const char *);
int kernelFilesystemUnmount(const char *);
int kernelFilesystemUnmountAll(void);
int kernelFilesystemNumberMounted(void);
void kernelFilesystemFirstFilesystem(char *);
void kernelFilesystemNextFilesystem(char *);
unsigned kernelFilesystemGetFree(const char *);
unsigned kernelFilesystemGetBlockSize(const char *);

#define _KERNELFILESYSTEM_H
#endif
