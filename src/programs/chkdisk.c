//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
//  chkdisk.c
//

// This is a program for performing filesystem scans

/* This is the text that appears when a user requests help about this program
<help>

 -- chkdisk --

This command can be used to perform a filesystem integrity check on a
logical disk.

Usage:
  chkdisk <disk_name>

The first parameter is the name of a disk (use the 'disks' command to list
the disks).  A check will be performed if the disk's filesystem is of a
recognized type, and the applicable filesystem driver supports a checking
function.

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <disk>\n", name);
  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char *diskName = NULL;
  int force = 0;
  int repair = 0;
  char yesNo = '\0';

  // Our argument is the disk number
  if (argc < 2)
    {
      usage(argv[0]);

      // Try to list the disks in the system
      loaderLoadAndExec("/programs/disks", 3 /* user privilege */, 0, NULL,
			1 /* block */);
      printf("\n");

      errno = ERR_ARGUMENTCOUNT;
      return (status = errno);
    }

  // Make sure none of our arguments are NULL
  if ((argv[0] == NULL) || (argv[1] == NULL))
    {
      errno = ERR_NULLPARAMETER;
      perror(argv[0]);
      return (status = errno);
    }

  diskName = argv[1];

  // Print a message
  printf("\nVisopsys CHKDISK Utility\nCopyright (C) 1998-2005 J. Andrew "
	 "McLaughlin\n\n");

  status = filesystemCheck(diskName, force, repair);
  
  if ((status < 0) && !repair)
    {
      // It's possible that the filesystem driver has no 'check' function.
      if (status != ERR_NOSUCHFUNCTION)
	{
	  // The filesystem may contain errors.  Before we fail the whole
	  // operation, ask whether the user wants to try and repair it.
	  printf("\nThe filesystem may contain errors.\nDo you want to try to "
		 "repair it? (y/n): ");
	  yesNo = getchar();
	  printf("\n");

	  if ((yesNo == 'y') || (yesNo == 'Y'))
	    // Force, repair
	    status = filesystemCheck(diskName, force, 1 /*repair*/);
	}

      if (status < 0)
	{
	  // Make the error
	  printf("Filesystem consistency check failed.\n");
	  errno = status;
	  return (status);
	}
    }

  errno = 0;
  return (status = errno);
}
