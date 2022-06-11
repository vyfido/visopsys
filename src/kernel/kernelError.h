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
//  kernelError.h
//

// This header file defines things used to communicate errors within
// the kernel, and routines for communicating these errors to the user.

#if !defined(_KERNELERROR_H)

// Definitions
#define MAX_ERRORTEXT_LENGTH 1024

// Items concerning severity
typedef enum 
{
  kernel_panic, 
  kernel_error, 
  kernel_warn 

} kernelErrorKind;

int kernelErrorInitialize(void);
int kernelErrorSetForeground(int);
void kernelErrorOutput(const char *, const char *, int, kernelErrorKind, 
		       const char *, ...);

// This macro should be used to invoke all kernel errors
#define kernelError(kind, message, arg...) \
  kernelErrorOutput(__FILE__, __FUNCTION__, __LINE__, kind, message, ##arg)

#define _KERNELERROR_H
#endif
