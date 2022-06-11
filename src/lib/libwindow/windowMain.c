// 
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  windowMain.c
//

// This contains functions for user programs to operate GUI components.

#include <string.h>
#include <sys/window.h>
#include <sys/api.h>
#include <sys/errors.h>


static volatile struct {
  objectKey key;
  unsigned eventMask;
  void (*function)(objectKey, windowEvent *);

} callBacks[WINDOW_MAX_COMPONENTS];

static volatile int numCallBacks = 0;
static volatile int run = 0;
static volatile int guiThreadPid = 0;


static void guiRun(int thread)
{
  // This is the thread that runs for each user GUI program polling
  // components' event queues for events.

  objectKey *key = NULL;
  windowEvent event;
  int count;

  run = 1;

  while(run)
    {
      // Loop through all of the registered callbacks looking for components
      // with pending events
      for (count = 0; count < numCallBacks; count ++)
	{
	  key = callBacks[count].key;

	  // Any events pending?
	  while (run && (windowComponentEventGet(key, &event) > 0))
	    callBacks[count].function((objectKey) key, &event);
	}

      // Done
      multitaskerYield();
    }

  if (thread)
    multitaskerTerminate(0);
  else
    return;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


_X_ int windowClearEventHandlers(void)
{
  // Desc: Remove all the callback event handlers registered with the windowRegisterEventHandler() function.

  numCallBacks = 0;
  bzero((void *) callBacks, sizeof(callBacks));
  return (0);
}


_X_ int windowRegisterEventHandler(objectKey key, void (*function)(objectKey, windowEvent *))
{
  // Desc: Register a callback function as an event handler for the GUI object 'key'.  The GUI object can be a window component, or a window for example.  GUI components are typically the target of mouse click or key press events, whereas windows typically receive 'close window' events.  For example, if you create a button component in a window, you should call windowRegisterEventHandler() to receive a callback when the button is pushed by a user.  You can use the same callback function for all your objects if you wish -- the objectKey of the target component can always be found in the windowEvent passed to your callback function.  It is necessary to use one of the 'run' functions, below, such as windowGuiRun() or windowGuiThread() in order to receive the callbacks.

  int status = 0;

  // Check parameters
  if ((key == NULL) || (function == NULL))
    return (status = ERR_NULLPARAMETER);

  callBacks[numCallBacks].key = key;
  callBacks[numCallBacks].function = function;
  numCallBacks++;

  return (status = 0);
}


_X_ void windowGuiRun(void)
{
  // Desc: Run the GUI windowEvent polling as a blocking call.  In other words, use this function when your program has completed its setup code, and simply needs to watch for GUI events such as mouse clicks, key presses, and window closures.  If your program needs to do other processing (independently of windowEvents) you should use the windowGuiThread() function instead.

  guiRun(0 /* no thread */);

  return;
}


_X_ void windowGuiThread(void)
{
// Desc: Run the GUI windowEvent polling as a non-blocking call.  In other words, this function will launch a separate thread to monitor for GUI events and return control to your program.  Your program can then continue execution -- independent of GUI windowEvents.  If your program doesn't need to do any processing after setting up all its window components and event callbacks, use the windowGuiRun() function instead.

  static void *args[] = { (void *) 1 /* thread */ };

  guiThreadPid = multitaskerSpawn(&guiRun, "gui thread", 1, args);

  return;
}


_X_ void windowGuiStop(void)
{
  // Desc: Stop GUI event polling which has been started by a previous call to one of the 'run' functions, such as windowGuiRun() or windowGuiThread().  Note that calling this function clears all callbacks registered with the windowRegisterEventHandler() function, so if you want to resume GUI execution you will need to re-register them.

  run = 0;
  
  if (guiThreadPid && (multitaskerGetCurrentProcessId() != guiThreadPid))
    {
      multitaskerKillProcess(guiThreadPid, 0);
      guiThreadPid = 0;
    }

  return;
}