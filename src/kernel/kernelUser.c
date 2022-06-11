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
//  kernelUser.c
//

// This file contains the routines designed for managing user access

#include "kernelUser.h"
#include "kernelMultitasker.h"


int kernelUserGetUid(void)
{
  // This routine will return the user id of the current task.

  int currentPID = 0;
  int currentUID = 0;

  // We have to call the multitasker function to identify the PID
  // of the current process
  currentPID = kernelMultitaskerGetCurrentProcessId();

  if (currentPID < 0)
    // We couldn't determine the current process
    return (currentPID);

  // We have to call the multitasker function to identify the user
  // owner of the current thread/process.

  currentUID = kernelMultitaskerGetProcessOwner(currentPID);

  // Return whatever was returned from the previous call
  return (currentUID);
}

