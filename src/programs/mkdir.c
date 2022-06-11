//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  mkdir.c
//

// This is the UNIX-style command for creating directories

/* This is the text that appears when a user requests help about this program
<help>

 -- mkdir --

Create one or more new directories.

Usage:
  mkdir <directory1> [directory2] [...]

This command can be used to create one or more new directories.  The first
parameter is the name of a new directory to create.  Any number of other
(optional) directories to create can be specified at the same time.

</help>
*/

#include <stdio.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/api.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <directory1> [directory2] [...]\n", name);
  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  int count;

  if (argc < 2)
    {
      usage(argv[0]);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Loop through all of our directory name arguments
  for (count = 1; count < argc; count ++)
    {
      // Make sure the name isn't NULL
      if (argv[count] == NULL)
	return (status = ERR_NULLPARAMETER);

      // Make sure the directory name is complete
      vshMakeAbsolutePath(argv[count], fileName);

      // Attempt to create the directory
      status = fileMakeDir(fileName);

      if (status < 0)
	{
	  errno = status;
	  perror(argv[0]);
	  return (status);
	}
    }

  // Return success
  return (status = 0);
}
