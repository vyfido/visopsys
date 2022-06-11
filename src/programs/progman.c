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
//  progman.c
//

// This is a program for managing programs and processes.

/* This is the text that appears when a user requests help about this program
<help>

 -- progman --

Also known as the "Program Manager", progman is used to manage processes.

Usage:
  progman

(Only available in graphics mode)

The progman program is interactive, and shows a constantly-updated list of
running processes and threads, including statistics such as CPU and memory
usage.  It is a graphical utility combining the same functionalities as the
'ps', 'mem', 'kill', and 'renice' command-line programs.

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/vsh.h>

#define SHOW_MAX_PROCESSES     100
#define PROCESS_STRING_LENGTH  64
#define USEDBLOCKS_STRING      "Memory blocks: "
#define USEDMEM_STRING         "Used Memory: "
#define FREEMEM_STRING         "Free Memory: "

static int processId = 0;
static int privilege = 0;
static listItemParameters *processListParams = NULL;
static process *processes = NULL;
static int numProcesses = 0;
static objectKey window = NULL;
static objectKey memoryBlocksLabel = NULL;
static objectKey memoryUsedLabel = NULL;
static objectKey memoryFreeLabel = NULL;
static objectKey processList = NULL;
static objectKey showThreadsCheckbox = NULL;
static objectKey runProgramButton = NULL;
static objectKey setPriorityButton = NULL;
static objectKey killProcessButton = NULL;
static int showThreads = 1;
static int stop = 0;


__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
  // Generic error message code for either text or graphics modes
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  vsnprintf(output, MAXSTRINGLENGTH, format, list);
  va_end(list);

  windowNewErrorDialog(window, "Error", output);
}


static void sortChildren(process *tmpProcessArray, int tmpNumProcesses)
{
  // (Recursively) sort any children of the last process in the process list
  // from the temporary list into our regular list.

  process *parent = NULL;
  int count;

  if (numProcesses == 0)
    // No parent to sort children for.
    return;

  parent = &processes[numProcesses - 1];

  for (count = 0; count < tmpNumProcesses; count ++)
    {
      if ((tmpProcessArray[count].processName[0] == '\0') ||
	  (tmpProcessArray[count].type != proc_thread) ||
	  (tmpProcessArray[count].parentProcessId != parent->processId))
	continue;
      
      // Copy this thread into the regular array
      memcpy(&processes[numProcesses++], &tmpProcessArray[count],
	     sizeof(process));
      tmpProcessArray[count].processName[0] = '\0';
	    
      // Now sort any children behind it.
      sortChildren(tmpProcessArray, tmpNumProcesses);
    }
}


static int getUpdate(void)
{
  // Get the list of processes from the kernel

  int status = 0;
  memoryStats memStats;
  char labelChar[32];
  unsigned totalFree = 0;
  int percentUsed = 0;
  char *bufferPointer = NULL;
  process *tmpProcessArray = NULL;
  process *tmpProcess = NULL;
  int tmpNumProcesses = 0;
  int count;

  // Try to get the memory stats
  bzero(&memStats, sizeof(memoryStats));
  status = memoryGetStats(&memStats, 0);
  if (status >= 0)
    {
      // Switch raw bytes numbers to kilobytes.  This will also prevent
      // overflow when we calculate percentage, below.
      memStats.totalMemory >>= 10;
      memStats.usedMemory >>= 10;

      totalFree = (memStats.totalMemory - memStats.usedMemory);
      percentUsed = ((memStats.usedMemory * 100) / memStats.totalMemory);

      // Assign the strings to our labels
      if (memoryBlocksLabel)
	{
	  sprintf(labelChar, USEDBLOCKS_STRING "%d", memStats.usedBlocks);
	  windowComponentSetData(memoryBlocksLabel, labelChar,
				 strlen(labelChar));
	}
      if (memoryUsedLabel)
	{
	  sprintf(labelChar, USEDMEM_STRING "%u Kb - %d%%",
		  memStats.usedMemory, percentUsed);
	  windowComponentSetData(memoryUsedLabel, labelChar,
				 strlen(labelChar));
	}
      if (memoryFreeLabel)
	{
	  sprintf(labelChar, FREEMEM_STRING "%u Kb - %d%%", totalFree,
		  (100 - percentUsed));
	  windowComponentSetData(memoryFreeLabel, labelChar,
				 strlen(labelChar));
	}
    }

  tmpProcessArray = malloc(SHOW_MAX_PROCESSES * sizeof(process));
  if (tmpProcessArray == NULL)
    {
      error("Can't get temporary memory for processes");
      return (status = ERR_MEMORY);
    }

  tmpNumProcesses =
    multitaskerGetProcesses(tmpProcessArray,
			    (SHOW_MAX_PROCESSES * sizeof(process)));
  if (tmpNumProcesses < 0)
    {
      free(tmpProcessArray);
      return (tmpNumProcesses);
    }

  numProcesses = 0;

  // Sort the processes from our temporary array into our regular array
  // so that we are skipping threads, if applicable, and so that all children
  // follow their parents

  for (count = 0; count < tmpNumProcesses; count ++)
    {
      if ((tmpProcessArray[count].processName[0] == '\0') ||
	  (tmpProcessArray[count].type == proc_thread))
	continue;

      // Copy this process into the regular array
      memcpy(&processes[numProcesses++], &tmpProcessArray[count],
	     sizeof(process));
      tmpProcessArray[count].processName[0] = '\0';

      if (showThreads)
	// Now sort any children, grandchildren, etc., behind it.
	sortChildren(tmpProcessArray, tmpNumProcesses);
    }

  free(tmpProcessArray);

  for (count = 0; count < numProcesses; count ++)
    {
      memset(processListParams[count].text, ' ', WINDOW_MAX_LABEL_LENGTH);
      bufferPointer = processListParams[count].text;
      tmpProcess = &processes[count];

      sprintf(bufferPointer, "%s%s",
	      ((tmpProcess->type == proc_thread)? " - " : ""),
	      tmpProcess->processName);
      bufferPointer[strlen(bufferPointer)] = ' ';
      sprintf((bufferPointer + 26), "%d", tmpProcess->processId);
       bufferPointer[strlen(bufferPointer)] = ' ';
      sprintf((bufferPointer + 30), "%d", tmpProcess->parentProcessId);
       bufferPointer[strlen(bufferPointer)] = ' ';
      sprintf((bufferPointer + 35), "%d", tmpProcess->userId);
      bufferPointer[strlen(bufferPointer)] = ' ';
      sprintf((bufferPointer + 39), "%d", tmpProcess->priority);
      bufferPointer[strlen(bufferPointer)] = ' ';
      sprintf((bufferPointer + 43), "%d", tmpProcess->privilege);
      bufferPointer[strlen(bufferPointer)] = ' ';
      sprintf((bufferPointer + 48), "%d", tmpProcess->cpuPercent);
      bufferPointer[strlen(bufferPointer)] = ' ';

      // Get the state
      switch(tmpProcess->state)
	{
	case proc_running:
	  strcpy((bufferPointer + 53), "running ");
	  break;
	case proc_ready:
	  strcpy((bufferPointer + 53), "ready ");
	  break;
	case proc_waiting:
	  strcpy((bufferPointer + 53), "waiting ");
	  break;
	case proc_sleeping:
	  strcpy((bufferPointer + 53), "sleeping ");
	  break;
	case proc_stopped:
	  strcpy((bufferPointer + 53), "stopped ");
	  break;
	case proc_finished:
	  strcpy((bufferPointer + 53), "finished ");
	  break;
	case proc_zombie:
	  strcpy((bufferPointer + 53), "zombie ");
	  break;
	default:
	  strcpy((bufferPointer + 53), "unknown ");
	  break;
	}
    }

  return (status = 0);
}


static void runProgram(void)
{
  // Prompts the user for a program to run.

  int status = 0;
  char commandLine[MAX_PATH_NAME_LENGTH];
  char command[MAX_PATH_NAME_LENGTH];
  char fullCommand[MAX_PATH_NAME_LENGTH];
  int argc = 0;
  char *argv[64];
  int count;

  status =
    windowNewFileDialog(NULL, "Enter command", "Please enter a command to "
			"run:", "/programs", commandLine,
			MAX_PATH_NAME_LENGTH);
  if (status != 1)
    goto out;

  // Turn the command line into a program and args
  status = vshParseCommand(commandLine, command, &argc, argv);
  if (status < 0)
    goto out;
  if (command[0] == '\0')
    {
      status = ERR_NOSUCHFILE;
      goto out;
    }

  // Got an executable command.  Execute it.

  strcpy(fullCommand, command);
  strcat(fullCommand, " ");
  for (count = 1; count < argc; count ++)
    {
      strcat(fullCommand, argv[count]);
      strcat(fullCommand, " ");
    }

  status = loaderLoadAndExec(fullCommand, privilege, 0);

 out:
  if (status < 0)
    error("Unable to execute program");
  multitaskerTerminate(status);
}


static int getNumberDialog(const char *title, const char *prompt)
{
  // Creates a dialog that prompts for a number value

  int status = 0;
  char buffer[11];

  status = windowNewPromptDialog(window, title, prompt, 1, 10, buffer);
  if (status < 0)
    return (status);

  if (buffer[0] == '\0')
    return (status = ERR_NODATA);

  // Try to turn it into a number
  buffer[10] = '\0';
  status = atoi(buffer);
  return (status);
}


static int setPriority(int whichProcess)
{
  // Set a new priority on a process

  int status = 0;
  int newPriority = 0;
  process *changeProcess = NULL;

  // Get the process to change
  changeProcess = &processes[whichProcess];
  
  newPriority = getNumberDialog("Set priority", "Please enter the desired "
				"priority");
  if (newPriority < 0)
    return (newPriority);

  status =
    multitaskerSetProcessPriority(changeProcess->processId, newPriority);

  // Refresh our list of processes
  getUpdate();
  windowComponentSetData(processList, processListParams, numProcesses);

  return (status);
}


static int killProcess(int whichProcess)
{
  // Tells the kernel to kill the requested process

  int status = 0;
  process *theProcess = NULL;

  // Get the process to kill
  theProcess = &processes[whichProcess];
  
  status = multitaskerKillProcess(theProcess->processId, 0);

  // Refresh our list of processes
  getUpdate();
  windowComponentSetData(processList, processListParams, numProcesses);

  return (status);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  int processNumber = 0;

  // Check for the window being closed by a GUI event.
  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    {
      stop = 1;
      windowGuiStop();
      windowDestroy(window);
    }
  
  else if ((key == showThreadsCheckbox) && (event->type & EVENT_SELECTION))
    {
      windowComponentGetSelected(showThreadsCheckbox, &showThreads);  
      getUpdate();
      windowComponentSetData(processList, processListParams, numProcesses);
    }
  
  else
    {
      windowComponentGetSelected(processList, &processNumber);
      if (processNumber < 0)
	return;

      if ((key == runProgramButton) && (event->type == EVENT_MOUSE_LEFTUP))
	{
	  if (multitaskerSpawn(&runProgram, "run program", 0, NULL) < 0)
	    error("Unable to launch file dialog");
	}

      else if ((key == setPriorityButton) &&
	       (event->type == EVENT_MOUSE_LEFTUP))
	setPriority(processNumber);

      else if ((key == killProcessButton) &&
	       (event->type == EVENT_MOUSE_LEFTUP))
	killProcess(processNumber);
    }
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;
  objectKey container = NULL;

  // Create a new window
  window = windowNew(processId, "Program Manager");
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padTop = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_top;

  memoryBlocksLabel = windowNewTextLabel(window, USEDBLOCKS_STRING, &params);

  params.gridY = 1;
  memoryUsedLabel = windowNewTextLabel(window, USEDMEM_STRING, &params);

  params.gridY = 2;
  params.padBottom = 10;
  memoryFreeLabel = windowNewTextLabel(window, FREEMEM_STRING, &params);

  params.gridY = 3;
  params.padBottom = 0;
  fontGetDefault(&(params.font));
  // Create the label of column headers for the list below
  windowNewTextLabel(window, "Process                   PID PPID UID Pri Priv "
		     "CPU% STATE   ", &params);

  // Create the list of processes
  params.gridY = 4;
  processList = windowNewList(window, windowlist_textonly, 20, 1, 0,
			      processListParams, numProcesses, &params);

  // Create a 'show sub-processes' checkbox
  params.gridY = 5;
  params.padBottom = 5;
  params.font = NULL;
  showThreadsCheckbox =
    windowNewCheckbox(window, "Show all sub-processes", &params);
  windowComponentSetSelected(showThreadsCheckbox, 1);
  windowRegisterEventHandler(showThreadsCheckbox, &eventHandler);

  // Make a container for the right hand side components
  params.gridX = 1;
  params.gridY = 4;
  params.padRight = 5;
  params.flags |= WINDOW_COMPFLAG_FIXEDHEIGHT;
  container = windowNewContainer(window, "rightContainer", &params);
  
  // Create a 'run program' button
  params.gridX = 0;
  params.gridY = 0;
  params.padLeft = 0;
  params.padRight = 0;
  params.padTop = 0;
  params.padBottom = 0;
  runProgramButton = windowNewButton(container, "Run program", NULL, &params);
  windowRegisterEventHandler(runProgramButton, &eventHandler);

  // Create a 'set priority' button
  params.gridY = 1;
  params.padTop = 5;
  setPriorityButton =
    windowNewButton(container, "Set priority", NULL, &params);
  windowRegisterEventHandler(setPriorityButton, &eventHandler);

  // Create a 'kill process' button
  params.gridY = 2;
  killProcessButton =
    windowNewButton(container, "Kill process", NULL, &params);
  windowRegisterEventHandler(killProcessButton, &eventHandler);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  int guiThreadPid = 0;

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n",
	     (argc? argv[0] : ""));
      return (errno = ERR_NOTINITIALIZED);
    }

  processId = multitaskerGetCurrentProcessId();
  privilege = multitaskerGetProcessPrivilege(processId);

  // Get a buffer for process structures
  processes = malloc(SHOW_MAX_PROCESSES * sizeof(process));
  // Get an array of list parameters structures for process strings
  processListParams = malloc(SHOW_MAX_PROCESSES * sizeof(listItemParameters));

  if ((processes == NULL) || (processListParams == NULL))
    {
      if (processes)
	free(processes);
      if (processListParams)
	free(processListParams);
      error("Error getting memory");
      return (errno = ERR_MEMORY);
    }


  // Get the list of process strings
  status = getUpdate();
  if (status < 0)
    {
      free(processes);
      free(processListParams);
      errno = status;
      if (argc)
	perror(argv[0]);
      return (status);
    }

  // Make our window
  constructWindow();

  // Run the GUI
  guiThreadPid = windowGuiThread();

  while (!stop && multitaskerProcessIsAlive(guiThreadPid))
    {
      windowComponentSetData(processList, processListParams, numProcesses);
      multitaskerWait(20);
      getUpdate();
    }

  free(processes);
  free(processListParams);
  return (status = errno = 0);
}
