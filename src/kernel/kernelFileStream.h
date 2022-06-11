//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  kernelFileStream.h
//

// This file describes the kernel's facilities for reading and writing
// files using a 'streams' abstraction.  It's a convenience for dealing
// with files.

#if !defined(_KERNELFILESTREAM_H)

#include <sys/file.h>

// Functions exported by kernelFileStream.c
int kernelFileStreamOpen(const char *, int, fileStream *);
int kernelFileStreamSeek(fileStream *, unsigned);
int kernelFileStreamRead(fileStream *, unsigned, char *);
int kernelFileStreamReadLine(fileStream *, unsigned, char *);
int kernelFileStreamWrite(fileStream *, unsigned, const char *);
int kernelFileStreamWriteStr(fileStream *, const char *);
int kernelFileStreamWriteLine(fileStream *, const char *);
int kernelFileStreamFlush(fileStream *);
int kernelFileStreamClose(fileStream *);
int kernelFileStreamGetTemp(fileStream *);

#define _KERNELFILESTREAM_H
#endif
