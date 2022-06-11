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
//  crt0.c
//

// This contains standard startup code which should be linked to all
// programs made for Visopsys using the C language


#include <sys/api.h>

extern int main(void);
extern void exit(int);

// This is the global 'errno' error status variable for this program
int errno = 0;

// This allows us to ensure that kernel API functions are not called from
// within the kernel.
int visopsys_in_kernel = 0;

void _start(void)
{
  // This is the code that first gets invoked when the program starts
  // to run.  This function will setup an easy termination function
  // that gets called when the program returns.  This file should
  // be compiled into object format and linked with any programs
  // built for use with Visopsys.

  static int status = 0;
 
  // NO AUTOMATIC (STACK) VARIABLE DECLARATIONS.

  // Our return address should be sitting near the current top of our stack
  // after any stack frame allocated by the compiler.  We don't want the
  // stack frame or return address (we never do a return), since we want to
  // pass our arguments straight to the main() routine.  Basically, we want
  // to simply pop that stuff off the stack.  This assumes that register EBP
  // contains the original stack pointer.
  //
  // WARNING: Anything else you try to do in this function should consider
  //          what is being done here.  For example, allocating variables
  //          in this function is impossible without changes because the
  //          stack frame is about to be erased.

  // Clear the stack frame
  __asm__ __volatile__ ("movl %%ebp, %%esp" : : : "%esp");
  // Clear the return address
  __asm__ __volatile__ ("addl $4, %%esp" : : : "%esp");

  // Call the regular program.
  status = main();

  // Now we do an exit call to properly terminate the program after
  // main returns
  exit(status);
}


void _exit(int status)
{
  // Tell the multitasker to shut us down

  // Shut down
  multitaskerTerminate(status);

  // Now, there's nothing else we can do except wait to be killed
  while(1);
}
