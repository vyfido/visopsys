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
//  kill.c
//

// This is the UNIX-style command for killing processes

#include <stdio.h>
#include <stdlib.h>
#include <sys/api.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <process1> [process2] [...]\n", name);
  return;
}


int main(int argc, char *argv[])
{
  // This command will prompt the multitasker to kill the process with
  // the supplied process id
  
  int status = 0;
  int processId = 0;
  int count;


  if (argc < 2)
    {
      usage(argv[0]);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Loop through all of our process ID arguments
  for (count = 1; count < argc; count ++)
    {
      // Make sure our argument isn't NULL
      if (argv[count] == NULL)
	return (status = ERR_NULLPARAMETER);
      
      processId = atoi(argv[count]);

      // OK?
      if (errno)
	{
	  perror(argv[0]);
	  usage(argv[0]);
	  return (status = errno);
	}

      // Kill a process
      status = multitaskerKillProcess(processId);

      if (status < 0)
	{
	  errno = status;
	  perror(argv[0]);
	}
      else
	printf("%d killed\n", processId);
    }
  
  // Return success
  return (status = 0);
}
