// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  crt0.c
//

// This contains standard startup code which should be linked to all
// programs made for Visopsys using the C language

#include <stdlib.h>
#include <locale.h>

extern int main(void);

// This is the global 'errno' error status variable for this program
int errno = 0;

// This allows us to ensure that kernel API functions are not called from
// within the kernel.
int visopsys_in_kernel = 0;

// Pointer to the current locale
extern struct lconv _c_locale;
struct lconv *_current_locale = &_c_locale;

void _start(void);
void _start(void)
{
  // This is the code that first gets invoked when the program starts
  // to run.  This function will setup an easy termination function
  // that gets called when the program returns.  This file should
  // be compiled into object format and linked with any programs
  // built for use with Visopsys.

  // NO AUTOMATIC (STACK) VARIABLE DECLARATIONS.

  static int _exit_status = 0;
 
  // Our return address should be sitting near the current top of our stack
  // after any stack frame allocated by the compiler.  We don't want the
  // stack frame or return address (we never do a return), since we want to
  // pass our arguments straight to the main() routine.  Basically, we want
  // to simply pop that stuff off the stack.  This assumes that register EBP
  // contains the original stack pointer.
  //
  // WARNING: Anything else you try to do in this function should consider
  //          what is being done here.  For example, allocating automatic
  //          (AKA stack or 'local') variables in this function might be
  //          problematic without changes because the stack frame is about
  //          to be erased.

  // Clear the stack frame
  __asm__ __volatile__ ("movl %%ebp, %%esp \n\t" \
			"popl %%ebp" : : : "%esp");

  // Call the regular program.
  _exit_status = main();

  // Do an exit call to properly terminate the program after main returns
  exit(_exit_status);
}
