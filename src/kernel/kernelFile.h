//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  kernelFile.h
//

// This file contains the routines designed for managing the file
// system tree.

#if !defined(_KERNELFILE_H)

#include "kernelLock.h"
#include <sys/disk.h>

// Definitions
#define MAX_BUFFERED_FILES 1024
// MicrosoftTM's filesystems can't handle too many directory entries
#define MAX_DIRECTORY_ENTRIES 0xFFFE
// The 'flags' values for the field in kernelFileEntry
#define FLAG_SECUREDELETE 0x01

// Can't include kernelDisk.h, it's circular.
struct _kernelDisk;

// This structure defines a file or directory entry
typedef volatile struct _kernelFileEntry {
  char name[MAX_NAME_LENGTH];
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
  volatile struct _kernelDisk *disk;     // parent filesystem
  void *driverData;                      // private fs-driver-specific data
  int openCount;
  lock lock;

  // Linked-list stuff.
  volatile struct _kernelFileEntry *parentDirectory;
  volatile struct _kernelFileEntry *previousEntry;
  volatile struct _kernelFileEntry *nextEntry;
  unsigned lastAccess;

  // (The following additional stuff only applies to directories and links)
  volatile struct _kernelFileEntry *contents;

} kernelFileEntry;

// Functions exported by kernelFile.c
int kernelFileInitialize(void);
int kernelFileSetRoot(kernelFileEntry *);
kernelFileEntry *kernelFileNewEntry(volatile struct _kernelDisk *);
void kernelFileReleaseEntry(kernelFileEntry *);
int kernelFileInsertEntry(kernelFileEntry *, kernelFileEntry *);
int kernelFileRemoveEntry(kernelFileEntry *);
int kernelFileGetFullName(kernelFileEntry *, char *);
kernelFileEntry *kernelFileLookup(const char *);
kernelFileEntry *kernelFileResolveLink(kernelFileEntry *);
int kernelFileCountDirEntries(kernelFileEntry *);
int kernelFileMakeDotDirs(kernelFileEntry *, kernelFileEntry *);
int kernelFileUnbufferRecursive(kernelFileEntry *);
int kernelFileSetSize(kernelFileEntry *, unsigned);
// More functions, but also exported to user space
int kernelFileFixupPath(const char *, char *);
int kernelFileSeparateLast(const char *, char *, char *);
int kernelFileGetDisk(const char *, disk *);
int kernelFileCount(const char *);
int kernelFileFirst(const char *, file *);
int kernelFileNext(const char *, file *);
int kernelFileFind(const char *, file *);
int kernelFileOpen(const char *, int, file *);
int kernelFileClose(file *);
int kernelFileRead(file *, unsigned, unsigned, void *);
int kernelFileWrite(file *, unsigned, unsigned, void *);
int kernelFileDelete(const char *);
int kernelFileDeleteRecursive(const char *);
int kernelFileDeleteSecure(const char *);
int kernelFileMakeDir(const char *);
int kernelFileRemoveDir(const char *);
int kernelFileCopy(const char *, const char *);
int kernelFileCopyRecursive(const char *, const char *);
int kernelFileMove(const char *, const char *);
int kernelFileTimestamp(const char *);
int kernelFileGetTemp(file *);

#define _KERNELFILE_H
#endif
