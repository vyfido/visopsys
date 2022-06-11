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
//  sync.c
//

// This is the UNIX-style command for synchronizing filesystem changes
// with the disk.

#include <stdio.h>
#include <errno.h>
#include <sys/api.h>


/*
static void usage(char *name)
{
  printf("usage:\n");
  printf("%s [filesystem1] [filesystem2] [...]\n", name);
  return;
}
*/


int main(int argc, char *argv[])
{
  // Attempts to synchronize the named filesystem

  int status = 0;
  int count;


  // If there are no arguments, we send a NULL to sync all filesystems
  if (argc == 1)
    {
      // This will sync all filesystems
      status = filesystemSync(NULL /* Means sync all filesystems */);

      if (status < 0)
	{
	  errno = status;
	  perror(argv[0]);
	}
    }
  else
    {
      // Loop for each of our filesystem arguments.
      for (count = 1; count < argc; count ++)
	{
	  // Make sure it isn't NULL
	  if (argv[count] == NULL)
	    return (status = ERR_NULLPARAMETER);

	  status = filesystemSync(argv[count]);

	  if (status < 0)
	    {
	      errno = status;
	      perror(argv[0]);
	    }
	} 
    }

  // Return success
  return (status = 0);
}
