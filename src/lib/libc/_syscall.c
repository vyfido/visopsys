// 
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  _syscall.c
//

// This contains code for calling the Visopsys kernel

#include <sys/api.h>

// Utility macros for stack manipulation
#define stackPush(value) \
  __asm__ __volatile__ ("pushl %0 \n\t" : : "r" (value))
#define stackSub(bytes)                       \
  __asm__ __volatile__ ("addl %0, %%esp \n\t" \
                        : : "r" (bytes) : "%esp")

// This is generic method for invoking the kernel API
#define farCall(retCode)                                 \
  __asm__ __volatile__ ("lcall $0x003B,$0x00000000 \n\t" \
			"movl %%eax, %0 \n\t"            \
			: "=r" (retCode) : : "%eax", "memory");


int _syscall(int fnum, int numArgs, ...)
{
  // This function sets up the stack and arguments, invokes the kernel API,
  // cleans up the stack, and returns the return code.

  int status = 0;
  int *args = &numArgs;
  int count;

  // Push the arguments (reverse order)
  for (count = numArgs; count > 0; count --)
    stackPush(args[count]);

  // Push the function number and the (incremented) argument count
  stackPush(fnum);
  stackPush(numArgs + 1);

  if (!visopsys_in_kernel)
    // Call the kernel
    farCall(status);

  // Clean up the stack
  stackSub((numArgs + 2) * sizeof(int));

  return (status);
}

