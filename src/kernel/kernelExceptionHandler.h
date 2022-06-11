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
//  kernelExceptionHandler.h
//

#if !defined(_KERNELEXCEPTIONHANDLER_H)

// The maximum length of the error message
#define EXMAXMESLEN 79

// Error messages
#define INVALIDEXCEPTION_MESS "The kernel's exception handler was passed an invalid exception number"
#define INVALIDPROCESS_MESS "The kernel's exception handler was unable to determine the current process"
#define CANNOTKILL_MESS "The process that caused this fatal exception could not be killed"

// This type enumerates the possible types of exceptions
typedef enum 
{ 
  faultException, trapException, abortException, unknownException

} kernelExceptionType;

// This structure is used to hold information about exceptions.
typedef struct
{
  char *name;
  kernelExceptionType type;
  int fatal;

} kernelException;


#define NUM_EXCEPTIONS 19

// Functions exported by kernelExceptionHandler.c
int kernelExceptionHandlerInitialize(void);
void kernelExceptionHandler(int);

#define _KERNELEXCEPTIONHANDLER_H
#endif
