//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kill.c
//

// This is the UNIX-style command for killing processes

/* This is the text that appears when a user requests help about this program
<help>

 -- kill --

Kill (stop) programs or processes.

Usage:
  kill [-f] <process1> [process2] [...]

The 'kill' command can be used to stop and eliminate one or more programs or
processes.  The '-f' option means 'force' (i.e., stop the program even if it
is not responding, or if there are errors of various types).  If no -f option
is specified, the kill operation will tend to quit if errors are encountered
while dismantling a process.  The only mandatory parameter is a process ID
number (and, optionally, any number of additional process ID numbers).  To
see a list of running processes, use the 'ps' command.

Options:
-f  : Force kill -- ignore errors.

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/api.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s [-f] <process1> [process2] [...]\n", name);
  return;
}


int main(int argc, char *argv[])
{
  // This command will prompt the multitasker to kill the process with
  // the supplied process id
  
  int status = 0;
  int processId = 0;
  int force = 0;
  int count = 1;

  if (argc < 2)
    {
      usage(argv[0]);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Check for -f ('force') option
  if (getopt(argc, argv, "f") == 'f')
    {
      if (argc < 3)
	{
	  usage(argv[0]);
	  return (status = ERR_ARGUMENTCOUNT);
	}

      force = 1;
      count++;
    }

  // Loop through all of our process ID arguments
  for ( ; count < argc; count ++)
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
      status = multitaskerKillProcess(processId, force);
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
