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
//  kernelFilesystem.h
//

#if !defined(_KERNELFILESYSTEM_H)

#include "kernelDiskFunctions.h"
#include <sys/file.h>

// Definitions
#define MAX_FILESYSTEMS 32
#define MAX_FS_NAME_LENGTH 64

// Error messages
#define FS_NOT_INITIALIZED "The filesystem manager hasn't been initialized.  Filesystem cannot be mounted"
#define FS_ALREADY_MOUNTED "The requested disk or mount point is already in use by the system"
#define INVALID_FS_NUMBER "The Id passed or referenced is not a valid filesystem number"
#define INVALID_FS_NAME "The mount point passed or referenced is not a valid filesystem name"
#define INCONSISTENT_FS_NUMBER "The Id passed or referenced doesn't refer to an existing filesystem"
#define NULL_FS_DISK_OBJECT "The filesystem object passed or referenced has a NULL disk object"
#define UNKNOWN_FS_TYPE "The system was not able to determine the type of this filesystem"
#define NULL_FS_OBJECT "The filesystem object passed or referenced is NULL"
#define NULL_FS_DRIVER "The filesystem driver attached to the filesystem object is NULL"
#define NULL_FS_DRIVER_FUNCTION "The requested filesystem driver function is NULL"
#define MAX_FS_EXCEEDED "The maximum number of mounted filesystems will not be exceeded"
#define NO_SYNCHY_NO_UMOUNT "The driver was unable to sync the filesystem.  Unmount aborted"
#define MISSING_FS_OBJECT "The filesystem to be unmounted cannot be found in the filesystem list"
#define NOSYNCD_MESG "The filesystem manager could not spawn the synchronizer"
#define BADSYNCDPRIORITY_MESG "The filesystem manager could not assign a low priority to the synchronizer"
#define FS_DRIVER_INIT_FAILED "The filesystem driver's initialize routine returned this error code"
#define FS_ROOT_ORPHANS "Cannot unmount / when child filesystems are still mounted"


// These functions are the common routines for the various types of
// filesystems
#define FATDETECT &kernelFilesystemTypeFatDetect
#define FATCHECK &kernelFilesystemTypeFatCheck
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
  unsigned int creationTime;
  unsigned int creationDate;
  unsigned int accessedTime;
  unsigned int accessedDate;
  unsigned int modifiedTime;
  unsigned int modifiedDate;
  unsigned int size;
  unsigned int blocks;

  // Misc
  void *filesystem;     // parent filesystem
  void *fileEntryData;  // private fs-specific data
  int openCount;
  unsigned int lockedByProcess;

  // Linked-list stuff.
  void *parentDirectory;
  void *previousEntry;
  void *nextEntry;
  unsigned int lastAccess;

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
  unsigned int blockSize;
  int syncLock;
  int caseInsensitive;
  void *filesystemData;

} kernelFilesystem;


// This is the structure that is used to define a file system
// driver
typedef struct
{
  kernelFileSysTypeEnum driverType;
  char *driverTypeName;
  int (*driverDetect) (const kernelDiskObject *);
  int (*driverCheck) (const kernelDiskObject *);
  int (*driverDefragment) (kernelFilesystem *);
  int (*driverMount) (kernelFilesystem *);
  int (*driverSync) (kernelFilesystem *);
  int (*driverUnmount) (kernelFilesystem *);
  unsigned int (*driverGetFree) (kernelFilesystem *);
  int (*driverNewEntry) (kernelFileEntry *);
  int (*driverInactiveEntry) (kernelFileEntry *);
  int (*driverReadFile) (kernelFileEntry *, unsigned int, unsigned int,
			 unsigned char *);
  int (*driverWriteFile) (kernelFileEntry *, unsigned int, unsigned int,
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
int kernelFilesystemMount(int, const char *);
int kernelFilesystemSync(const char *);
int kernelFilesystemUnmount(const char *);
int kernelFilesystemUnmountAll(void);
int kernelFilesystemNumberMounted(void);
void kernelFilesystemFirstFilesystem(char *);
void kernelFilesystemNextFilesystem(char *);
unsigned int kernelFilesystemGetFree(const char *);
unsigned int kernelFilesystemGetBlockSize(const char *);

#define _KERNELFILESYSTEM_H
#endif
