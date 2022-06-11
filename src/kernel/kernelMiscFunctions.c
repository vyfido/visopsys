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
//  kernelMiscFunctions.c
//
	
// A file for miscellaneous things

#include "kernelMiscFunctions.h"
#include "kernelLoader.h"
#include "kernelMultitasker.h"
#include "kernelError.h"
#include <stdio.h>


static char *kernelInfo[] =
{
  "Visopsys",
  _KVERSION_
} ;

static char versionString[64];


void kernelNullFunction(void)
{
  return;
}


void kernelConsoleLogin(void)
{
  // This routine will launch a login process on the console.  This should
  // normally be called by the kernel's kernelMain() routine, above, but 
  // also possibly by the keyboard driver when some hot-key is pressed.

  int loginPid = 0;


  // Try to launch the login process
  loginPid = kernelLoaderLoadAndExec("/programs/login", 0 /* no args */,
				     NULL /* no args */, 0 /* don't block */);

  if (loginPid < 0)
    {
      // Don't fail, but make a warning message
      kernelError(kernel_warn, "Couldn't start a login process");
    }

  else
    {
      // Attach the login process to the console text streams
      kernelMultitaskerSetTextInput(loginPid,
				    kernelTextStreamGetConsoleInput());
      kernelMultitaskerSetTextOutput(loginPid,
				     kernelTextStreamGetConsoleOutput());
    }

  // Done
  return;
}


const char *kernelVersion(void)
{
  // This function creates and returns a pointer to the kernel's version
  // string.

  // Construct the version string
  sprintf(versionString, "%s v%s", kernelInfo[0], kernelInfo[1]);

  return (versionString);
}
