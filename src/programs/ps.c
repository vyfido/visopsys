//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  ps.c
//

// This is the UNIX-style command for viewing a list of running processes

/* This is the text that appears when a user requests help about this program
<help>

 -- ps --

Print all of the running processes

Usage:
  ps

This command will print all of the running processes, their process IDs,
privilege level, priority level, CPU utilization and other statistics.

</help>
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>

#define SHOW_MAX_PROCESSES 100

int main(int argc __attribute__((unused)), char *argv[])
{
  // This command will query the kernel for a list of all active processes,
  // and print information about them on the screen.

  unsigned bufferSize = 0;
  process *processes = NULL;
  int numProcesses = 0;
  process *tmpProcess;
  char lineBuffer[160];
  int count;
  
  bufferSize = (SHOW_MAX_PROCESSES * sizeof(process));

  processes = malloc(bufferSize);
  if (processes == NULL)
    {
      perror(argv[0]);
      return (ERR_MEMORY);
    }

  numProcesses = multitaskerGetProcesses(processes, bufferSize);
  if (numProcesses < 0)
    {
      errno = numProcesses;
      perror(argv[0]);
      free(processes);
      return (numProcesses);
    }

  printf("Process list:\n");
  for (count = 0; count < numProcesses; count ++)
    {
      tmpProcess = &processes[count];
	  
      snprintf(lineBuffer, 160, "\"%s\"  PID=%d UID=%d priority=%d "
	       "priv=%d parent=%d\n        %d%% CPU State=",
	       tmpProcess->processName, tmpProcess->processId,
	       tmpProcess->userId, tmpProcess->priority, tmpProcess->privilege,
	       tmpProcess->parentProcessId, tmpProcess->cpuPercent);

      // Get the state
      switch(tmpProcess->state)
	{
	case proc_running:
	  strcat(lineBuffer, "running");
	  break;
	case proc_ready:
	  strcat(lineBuffer, "ready");
	  break;
	case proc_waiting:
	  strcat(lineBuffer, "waiting");
	  break;
	case proc_sleeping:
	  strcat(lineBuffer, "sleeping");
	  break;
	case proc_stopped:
	  strcat(lineBuffer, "stopped");
	  break;
	case proc_finished:
	  strcat(lineBuffer, "finished");
	  break;
	case proc_zombie:
	  strcat(lineBuffer, "zombie");
	  break;
	default:
	  strcat(lineBuffer, "unknown");
	  break;
	}
      printf("%s\n", lineBuffer);
      //textPrintLine(lineBuffer);
    }

  free(processes);

  // Return success
  return (0);
}
