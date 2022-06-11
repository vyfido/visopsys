//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
	
// This file contains utilities for debugging the kernel

#if !defined(_KERNELDEBUG_H)

#include "kernelText.h"

// These are the 'debug level flags' that can be used to construct a custom
// debugging output
#define DEBUG_ENTER_EXIT 0x01

// The current debugging level.
#if !defined(DEBUGLEVEL)
#define DEBUGLEVEL 0
#endif

#if (DEBUGLEVEL & DEBUG_ENTER_EXIT)
#define kernelDebugEnter()                                \
  kernelTextStreamPrintLine(kernelTextGetConsoleOutput(), \
			    "DEBUG: enter function %s", __FUNCTION__)
#else
#define kernelDebugEnter() while(0)
#endif // DEBUG_ENTER_EXIT

#define _KERNELDEBUG_H
#endif
