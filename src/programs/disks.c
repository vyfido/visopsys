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
//  disks.c
//

// This command just lists all the disks registered in the system

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>


int main(int argc, char *argv[])
{
  int status = 0;
  int availableDisks = 0;
  disk diskInfo;
  int count;

  // Call the kernel to give us the number of available disks
  availableDisks = diskFunctionsGetCount();

  printf("\n");
      
  for (count = 0; count < availableDisks; count ++)
    {
      status = diskFunctionsGetInfo(count, &diskInfo);
      
      if (status < 0)
	{
	  // Eek.  Problem getting disk info
	  errno = status;
	  perror(argv[0]);
	  return (status);
	}

      // Print disk info
      printf("%d: %s\n", count, diskInfo.description);
    }

  errno = 0;
  return (status = errno);
}
