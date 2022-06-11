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
//  ps.c
//

// This is the UNIX-style command for viewing a list of running processes

#include <stdio.h>
#include <string.h>
#include <sys/api.h>

#define SHOW_MAX_PROCESSES 100

int main(int argc, char *argv[])
{
  // This command will query the kernel for a list of all active processes,
  // and print information about them on the screen.

  process processes[SHOW_MAX_PROCESSES];
  int numberProcesses = 0;
  process *tmpProcess;
  char lineBuffer[160];
  int count;
  
  numberProcesses =
    multitaskerGetProcesses(processes, (SHOW_MAX_PROCESSES * sizeof(process)));

  printf("Process list:\n");
  for (count = 0; count < numberProcesses; count ++)
    {
      tmpProcess = &processes[count];
	  
      sprintf(lineBuffer, "\"%s\"  PID=%d UID=%d priority=%d "
	      "priv=%d parent=%d\n        %d%% CPU State=",
	      (char *) tmpProcess->processName,
	      tmpProcess->processId, tmpProcess->userId,
	      tmpProcess->priority, tmpProcess->privilege,
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
    }

  // Return success
  return (0);
}
