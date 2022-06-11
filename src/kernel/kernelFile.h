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
//  kernelFile.h
//

// This file contains the routines designed for managing the file
// system tree.

#if !defined(_KERNELFILE_H)

#include "kernelFilesystem.h"

// Definitions
#define MAX_BUFFERED_FILES 1024
// MicrosoftTM's filesystems can't handle too many directory entries
#define MAX_DIRECTORY_ENTRIES 0xFFFE
// The 'flags' values for the field in kernelFileEntry
#define FLAG_SECUREDELETE 0x01

// Functions exported by kernelFile.c
int kernelFileInitialize(void);
int kernelFileSetRoot(kernelFileEntry *);
kernelFileEntry *kernelFileNewEntry(kernelFilesystem *);
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
int kernelFileFirst(const char *, file *);
int kernelFileNext(const char *, file *);
int kernelFileFind(const char *, file *);
int kernelFileOpen(const char *, int, file *);
int kernelFileClose(file *);
int kernelFileRead(file *, unsigned, unsigned, unsigned char *);
int kernelFileWrite(file *, unsigned, unsigned, unsigned char *);
int kernelFileDelete(const char *);
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
