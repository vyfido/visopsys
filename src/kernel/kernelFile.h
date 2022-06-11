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
#define FLAG_IMMUTABLE    0x02

// Functions exported by kernelFile.c
int kernelFileInitialize(void);
int kernelFileSetRoot(kernelFileEntry *);
int kernelFileFixupPath(const char *, char *);
kernelFileEntry *kernelFileNewEntry(kernelFilesystem *);
void kernelFileReleaseEntry(kernelFileEntry *);
int kernelFileInsertEntry(kernelFileEntry *, kernelFileEntry *);
int kernelFileRemoveEntry(kernelFileEntry *);
int kernelFileCountDirEntries(kernelFileEntry *);
kernelFileEntry *kernelFileLookup(const char *);
int kernelFileSeparateLast(const char *, char *, char *);
int kernelFileFirst(const char *, file *);
int kernelFileNext(const char *, file *);
int kernelFileFind(const char *, file *);
int kernelFileCreate(const char *);
int kernelFileOpen(const char *, int, file *);
int kernelFileClose(file *);
int kernelFileRead(file *, unsigned int, unsigned int, unsigned char *);
int kernelFileWrite(file *, unsigned int, unsigned int, unsigned char *);
int kernelFileDelete(const char *);
int kernelFileDeleteSecure(const char *);
int kernelFileMakeDir(const char *);
int kernelFileRemoveDir(const char *);
int kernelFileCopy(const char *, const char *);
int kernelFileMove(const char *, const char *);
int kernelFileSetSize(file *, unsigned int);
int kernelFileTimestamp(const char *);
int kernelFileWriteDirtyDirs(const char *);
int kernelFileUnbufferRecursive(kernelFileEntry *);

#define _KERNELFILE_H
#endif
