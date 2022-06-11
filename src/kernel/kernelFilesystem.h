//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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

#include "kernelDisk.h"
#include "kernelLock.h"
#include <sys/file.h>

// Definitions
#define MAX_FILESYSTEMS 32
#define MAX_FS_NAME_LENGTH 64

// This structure defines a file or directory entry
typedef volatile struct
{
  unsigned char name[MAX_NAME_LENGTH];
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
  void *driverData;     // private fs-driver-specific data
  int openCount;
  lock lock;

  // Linked-list stuff.
  void *parentDirectory;
  void *previousEntry;
  void *nextEntry;
  unsigned lastAccess;

  // (The following additional stuff only applies to directories and links)
  void *contents;

} kernelFileEntry;

// This is an enumeration that lists the names of the known types of 
// filesystems.
typedef enum {
  unknown, Ext, Fat, Iso
} kernelFileSysTypeEnum;

// This is the structure that is used to define file systems
typedef volatile struct
{
  const char *description;
  char mountPoint[MAX_FS_NAME_LENGTH];
  const kernelDisk *disk;
  void *driver;
  kernelFileEntry *filesystemRoot;
  unsigned blockSize;
  int caseInsensitive;
  int readOnly;
  void *filesystemData;

} kernelFilesystem;

// This is the structure that is used to define a file system
// driver
typedef struct
{
  kernelFileSysTypeEnum driverType;
  char *driverTypeName;
  int (*driverInitialize) (void);
  int (*driverDetect) (const kernelDisk *);
  int (*driverFormat) (kernelDisk *, const char *, const char *, int);
  int (*driverCheck) (kernelFilesystem *, int, int);
  int (*driverDefragment) (kernelFilesystem *);
  int (*driverMount) (kernelFilesystem *);
  int (*driverUnmount) (kernelFilesystem *);
  unsigned (*driverGetFree) (kernelFilesystem *);
  int (*driverNewEntry) (kernelFileEntry *);
  int (*driverInactiveEntry) (kernelFileEntry *);
  int (*driverResolveLink) (kernelFileEntry *);
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

// The default driver initializations
int kernelFilesystemExtInitialize(void);
int kernelFilesystemFatInitialize(void);
int kernelFilesystemIsoInitialize(void);

// Functions exported by kernelFilesystem.c
int kernelFilesystemScan(kernelDisk *);
int kernelFilesystemCheck(const char *, int, int);
int kernelFilesystemFormat(const char *, const char *, const char *, int);
int kernelFilesystemDefragment(const char *);
int kernelFilesystemMount(const char *, const char *);
int kernelFilesystemUnmount(const char *);
int kernelFilesystemUnmountAll(void);
kernelFilesystem *kernelFilesystemGet(char *);
unsigned kernelFilesystemGetFree(const char *);
unsigned kernelFilesystemGetBlockSize(const char *);

#define _KERNELFILESYSTEM_H
#endif
