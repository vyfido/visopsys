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
//  kernelDebug.h
//

// This header file defines things used to print debug messages from within
// the kernel

#if !defined(_KERNELDEBUG_H)
#if defined(DEBUG)

// Definitions
#define MAX_DEBUGTEXT_LENGTH  1024
#define MAX_DEBUG_CATEGORIES  16
#define MAX_DEBUG_FILENAMES   16

#define DEBUG_SHOWPROCESS     0x08
#define DEBUG_SHOWFILE        0x04
#define DEBUG_SHOWFUNCTION    0x02

typedef enum {
  debug_all, debug_api, debug_fs, debug_gui, debug_io, debug_misc,
  debug_multitasker, debug_scsi, debug_usb
} kernelDebugCategory;

void kernelDebugInitialize(void);
void kernelDebugFlags(int);
void kernelDebugAddCategory(kernelDebugCategory);
void kernelDebugAddFile(const char *);
void kernelDebugOutput(const char *, const char *, int, kernelDebugCategory,
		       const char *, ...)
     __attribute__((format(printf, 5, 6)));

// These macro should be used for actual debug calls
#define kernelDebug(category, message, arg...) \
  kernelDebugOutput(__FILE__, __FUNCTION__, __LINE__, category, message, ##arg)

#else // !defined(DEBUG)

#define kernelDebugInitialize(...) do {} while (0)
#define kernelDebugFlags(...) do {} while (0)
#define kernelDebugAddCategory(...) do {} while (0)
#define kernelDebugAddFile(...) do {} while (0)
#define kernelDebug(...) do {} while (0)

#endif // defined(DEBUG)
#define _KERNELDEBUG_H
#endif
