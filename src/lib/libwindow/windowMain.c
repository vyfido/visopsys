//
//  Visopsys
//  Copyright (C) 1998-2019 J. Andrew McLaughlin
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
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  windowMain.c
//

// This contains functions for user programs to operate GUI components.

#include <errno.h>
#include <libintl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/api.h>
#include <sys/vis.h>
#include <sys/window.h>


typedef struct {
	objectKey key;
	void (*function)(objectKey, windowEvent *);

} callBack;

int libwindow_initialized = 0;
void libwindowInitialize(void);

static linkedList callBackList = { NULL, NULL, 0, { 0 } };
static volatile int run = 0;
static volatile int guiThreadPid = 0;


static void guiRun(void)
{
	// This is the thread that runs for each user GUI program polling
	// components' event queues for events.

	callBack *cb = NULL;
	linkedListItem *iter = NULL;
	windowEvent event;

	run = 1;

	while (run)
	{
		// Loop through all of the registered callbacks looking for components
		// with pending events

		cb = linkedListIterStart(&callBackList, &iter);

		while (cb)
		{
			if (cb->key && (windowComponentEventGet(cb->key, &event) > 0))
			{
				if (cb->function)
					cb->function(cb->key, &event);
			}

			cb = linkedListIterNext(&callBackList, &iter);
		}

		// Done
		multitaskerYield();
	}
}


static void guiRunThread(void)
{
	guiRun();
	multitaskerTerminate(0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

void libwindowInitialize(void)
{
	bindtextdomain("libwindow", GETTEXT_LOCALEDIR_PREFIX);
	libwindow_initialized = 1;
}


_X_ int windowClearEventHandlers(void)
{
	// Desc: Remove all the callback event handlers registered with the windowRegisterEventHandler() function.

	callBack *cb = NULL;
	linkedListItem *iter = NULL;

	cb = linkedListIterStart(&callBackList, &iter);

	while (cb)
	{
		if (linkedListRemove(&callBackList, cb) >= 0)
			free(cb);

		cb = linkedListIterNext(&callBackList, &iter);
	}

	// Probably unnecessary
	linkedListClear(&callBackList);

	return (0);
}


_X_ int windowRegisterEventHandler(objectKey key, void (*function)(objectKey, windowEvent *))
{
	// Desc: Register a callback function as an event handler for the GUI object 'key'.  The GUI object can be a window component, or a window for example.  GUI components are typically the target of mouse click or key press events, whereas windows typically receive 'close window' events.  For example, if you create a button component in a window, you should call windowRegisterEventHandler() to receive a callback when the button is pushed by a user.  You can use the same callback function for all your objects if you wish -- the objectKey of the target component can always be found in the windowEvent passed to your callback function.  It is necessary to use one of the 'run' functions, below, such as windowGuiRun() or windowGuiThread() in order to receive the callbacks.

	int status = 0;
	callBack *cb = NULL;

	// Check params
	if (!key || !function)
		return (status = ERR_NULLPARAMETER);

	cb = calloc(1, sizeof(callBack));
	if (!cb)
		return (status = ERR_MEMORY);

	cb->key = key;
	cb->function = function;

	status = linkedListAdd(&callBackList, cb);
	if (status < 0)
	{
		free(cb);
		return (status);
	}

	return (status = 0);
}


_X_ int windowClearEventHandler(objectKey key)
{
	// Desc: Remove a callback event handler registered with the windowRegisterEventHandler() function.

	int status = 0;
	callBack *cb = NULL;
	linkedListItem *iter = NULL;

	cb = linkedListIterStart(&callBackList, &iter);

	while (cb)
	{
		if (cb->key == key)
		{
			status = linkedListRemove(&callBackList, cb);
			if (status < 0)
				return (status);

			free(cb);

			return (status = 0);
		}

		cb = linkedListIterNext(&callBackList, &iter);
	}

	// Not found
	return (status = ERR_NOSUCHENTRY);
}


_X_ void windowGuiRun(void)
{
	// Desc: Run the GUI windowEvent polling as a blocking call.  In other words, use this function when your program has completed its setup code, and simply needs to watch for GUI events such as mouse clicks, key presses, and window closures.  If your program needs to do other processing (independently of windowEvents) you should use the windowGuiThread() function instead.

	guiRun();
}


_X_ int windowGuiThread(void)
{
	// Desc: Run the GUI windowEvent polling as a non-blocking call.  In other words, this function will launch a separate thread to monitor for GUI events and return control to your program.  Your program can then continue execution -- independent of GUI windowEvents.  If your program doesn't need to do any processing after setting up all its window components and event callbacks, use the windowGuiRun() function instead.

	if (!guiThreadPid || !multitaskerProcessIsAlive(guiThreadPid))
	{
		guiThreadPid = multitaskerSpawn(&guiRunThread, "gui thread",
			0 /* no args */, NULL /* no args */, 1 /* run */);
	}

	return (guiThreadPid);
}


_X_ int windowGuiThreadPid(void)
{
	// Desc: Returns the current GUI thread PID, if applicable, or else 0.
	return (guiThreadPid);
}


_X_ void windowGuiStop(void)
{
	// Desc: Stop GUI event polling which has been started by a previous call to one of the 'run' functions, such as windowGuiRun() or windowGuiThread().

	run = 0;

	if (guiThreadPid && (multitaskerGetCurrentProcessId() != guiThreadPid))
		multitaskerKillProcess(guiThreadPid, 0);

	multitaskerYield();

	guiThreadPid = 0;
}

