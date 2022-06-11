//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  kernelWindowShell.c
//

// This is the code that manages the window shell in the GUI environment.

#include "kernelWindow.h"
#include "kernelDebug.h"
#include "kernelEnvironment.h"
#include "kernelError.h"
#include "kernelFont.h"
#include "kernelImage.h"
#include "kernelMultitasker.h"
#include "kernelWindowEventStream.h"
#include <string.h>
#include <sys/user.h>
#include <sys/vis.h>

typedef struct {
	int processId;
	userSession *session;
	kernelWindow *rootWindow;

} shellData;

static shellData data;

extern kernelWindowVariables *windowVariables;


static int checkShell(shellData *shell)
{
	// Check that the shell is registered and alive

	int status = 0;

	if (!shell->processId)
		return (status = ERR_NOTINITIALIZED);

	if (!kernelMultitaskerProcessIsAlive(shell->processId))
	{
		memset(shell, 0, sizeof(shellData));
		return (status = ERR_NOSUCHPROCESS);
	}

	return (status = 0);
}


static int writeShellEvent(shellData *shell, windowShellEvent *shellEvent)
{
	// Create and send a window shell event to the shell process

	int status = 0;
	windowEvent event;

	if (!shell->rootWindow)
		return (status = ERR_NOSUCHPROCESS);

	// Send a window event to the root window
	memset(&event, 0, sizeof(windowEvent));
	event.type = WINDOW_EVENT_SHELL;
	memcpy(&event.shell, shellEvent, sizeof(windowShellEvent));

	status = kernelWindowEventStreamWrite(&shell->rootWindow->events, &event);
	if (status < 0)
	{
		kernelDebugError("Error writing window event to shell window");
		return (status);
	}

	return (status = 0);
}


static void updateMenuBarComponents(shellData *shell)
{
	// Re-layout the menu bar
	if (shell->rootWindow->menuBar->layout)
		shell->rootWindow->menuBar->layout(shell->rootWindow->menuBar);

	// Re-draw the menu bar
	if (shell->rootWindow->menuBar->draw)
		shell->rootWindow->menuBar->draw(shell->rootWindow->menuBar);

	// Re-render the menu bar on screen
	shell->rootWindow->update(shell->rootWindow,
		shell->rootWindow->menuBar->xCoord,
		shell->rootWindow->menuBar->yCoord, shell->rootWindow->menuBar->width,
		shell->rootWindow->menuBar->height);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

int kernelWindowShell(int processId)
{
	// Register a new window shell

	int status = 0;

	kernelDebug(debug_gui, "WindowShell register processId=%d", processId);

	memset(&data, 0, sizeof(shellData));

	data.processId = processId;

	data.session = kernelMultitaskerGetProcessUserSession(data.processId);
	if (!data.session)
	{
		kernelError(kernel_error, "Unable to get the user session");
		return (status = ERR_NOSUCHUSER);
	}

	return (status = 0);
}


int kernelWindowShellSetRoot(kernelWindow *window)
{
	// Set the root window for a shell

	int status = 0;

	kernelDebug(debug_gui, "WindowShell set root window");

	// Check params
	if (!window)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check the state of the shell
	status = checkShell(&data);
	if (status < 0)
		return (status);

	// Check permissions.  Only the shell itself can set the root window.
	if (!kernelCurrentProcess || (kernelCurrentProcess->processId !=
		data.processId))
	{
		return (status = ERR_PERMISSION);
	}

	data.rootWindow = window;

	return (status = 0);
}


int kernelWindowShellUpdateList(void)
{
	// Tell the window shell to update its list of windows

	int status = 0;
	windowShellEvent shellEvent;

	kernelDebug(debug_gui, "WindowShell update window list");

	// Check the state of the shell
	status = checkShell(&data);
	if (status < 0)
		return (status = 0);

	// Send a shell event
	memset(&shellEvent, 0, sizeof(windowShellEvent));
	shellEvent.type = WINDOW_SHELL_EVENT_WINDOWLIST;

	status = writeShellEvent(&data, &shellEvent);

	return (status);
}


int kernelWindowShellRefresh(linkedList *windowList)
{
	// Refresh the whole shell environment

	int status = 0;
	kernelWindow *listWindow = NULL;
	linkedListItem *iter = NULL;
	windowEvent event;
	windowShellEvent shellEvent;

	kernelDebug(debug_gui, "WindowShell refresh");

	// Check params
	if (!windowList)
		return (status = ERR_NULLPARAMETER);

	// Check the state of the shell
	status = checkShell(&data);
	if (status < 0)
		return (status = 0);

	// Reload the user environment in the shell process
	if (kernelEnvironmentLoad(data.session->name, data.processId) >= 0)
	{
		// Propagate it to all the shell's child processes
		kernelMultitaskerPropagateEnvironment(data.processId, NULL);
	}

	// Send a 'window refresh' event to every window from the same user
	// session

	kernelDebug(debug_gui, "WindowShell refresh windows");

	memset(&event, 0, sizeof(windowEvent));
	event.type = WINDOW_EVENT_WINDOW_REFRESH;

	listWindow = linkedListIterStart(windowList, &iter);
	while (listWindow)
	{
		if (kernelMultitaskerGetProcessUserSession(listWindow->processId) ==
			data.session)
		{
			kernelWindowEventStreamWrite(&listWindow->events, &event);

			// Yield after sending each one; hopefully this will allow them
			// all a chance to update before we send our shell event.
			kernelMultitaskerYield();
			kernelMultitaskerYield();
		}

		listWindow = linkedListIterNext(windowList, &iter);
	}

	// Send a shell event

	kernelDebug(debug_gui, "WindowShell refresh shell");

	memset(&shellEvent, 0, sizeof(windowShellEvent));
	shellEvent.type = WINDOW_SHELL_EVENT_REFRESH;

	status = writeShellEvent(&data, &shellEvent);
	kernelMultitaskerYield();

	return (status);
}


int kernelWindowShellChangeBackground(void)
{
	// Notifies the shell that the desktop background image has changed.
	// It should be changed in the desktop config file before calling this
	// function.

	int status = 0;
	windowShellEvent shellEvent;

	kernelDebug(debug_gui, "WindowShell change background image");

	// Check the state of the shell
	status = checkShell(&data);
	if (status < 0)
		return (status);

	// Send a shell event

	memset(&shellEvent, 0, sizeof(windowShellEvent));
	shellEvent.type = WINDOW_SHELL_EVENT_CHANGEBACKGRND;

	status = writeShellEvent(&data, &shellEvent);

	return (status = 0);
}


int kernelWindowShellRaiseWindowMenu(void)
{
	// Focus the root window and raise the window menu.  This would typically
	// be done in response to the user pressing ALT-Tab.

	int status = 0;
	windowShellEvent shellEvent;

	kernelDebug(debug_gui, "WindowShell toggle root window menu bar");

	// Check the state of the shell
	status = checkShell(&data);
	if (status < 0)
		return (status = 0);

	// Send a shell event
	memset(&shellEvent, 0, sizeof(windowShellEvent));
	shellEvent.type = WINDOW_SHELL_EVENT_RAISEWINMENU;

	status = writeShellEvent(&data, &shellEvent);

	return (status);
}


kernelWindowComponent *kernelWindowShellNewTaskbarIcon(image *img)
{
	// Create an icon in the shell's taskbar

	kernelWindowComponent *iconComponent = NULL;
	componentParameters params;
	windowShellEvent shellEvent;

	kernelDebug(debug_gui, "WindowShell add taskbar icon");

	// Check params
	if (!img)
	{
		kernelError(kernel_error, "NULL parameter");
		return (iconComponent = NULL);
	}

	// Check the state of the shell
	if (checkShell(&data) < 0)
		return (iconComponent = NULL);

	// Make sure we have a root window and menu bar
	if (!data.rootWindow || !data.rootWindow->menuBar)
		return (iconComponent = NULL);

	memset(&params, 0, sizeof(componentParameters));
	params.flags = COMP_PARAMS_FLAG_CANFOCUS;

	// Create the menu bar icon
	iconComponent = kernelWindowNewMenuBarIcon(data.rootWindow->menuBar, img,
		&params);
	if (!iconComponent)
		return (iconComponent);

	// Re-draw the menu bar
	updateMenuBarComponents(&data);

	// Send a shell event
	memset(&shellEvent, 0, sizeof(windowShellEvent));
	shellEvent.type = WINDOW_SHELL_EVENT_NEWBARCOMP;
	shellEvent.component = iconComponent;
	shellEvent.processId = kernelMultitaskerGetCurrentProcessId();

	if (writeShellEvent(&data, &shellEvent) < 0)
	{
		kernelWindowComponentDestroy(iconComponent);
		return (iconComponent = NULL);
	}

	return (iconComponent);
}


kernelWindowComponent *kernelWindowShellNewTaskbarTextLabel(const char *text)
{
	// Create a label in the shell's taskbar

	kernelWindowComponent *labelComponent = NULL;
	componentParameters params;
	windowShellEvent shellEvent;

	kernelDebug(debug_gui, "WindowShell add taskbar text label");

	// Check params
	if (!text)
	{
		kernelError(kernel_error, "NULL parameter");
		return (labelComponent = NULL);
	}

	// Check the state of the shell
	if (checkShell(&data) < 0)
		return (labelComponent = NULL);

	// Make sure we have a root window and menu bar
	if (!data.rootWindow || !data.rootWindow->menuBar)
		return (labelComponent = NULL);

	memset(&params, 0, sizeof(componentParameters));
	params.foreground = data.rootWindow->menuBar->params.foreground;
	params.background = data.rootWindow->menuBar->params.background;
	params.flags |= (COMP_PARAMS_FLAG_CUSTOMFOREGROUND |
		COMP_PARAMS_FLAG_CUSTOMBACKGROUND);
	params.font = windowVariables->font.varWidth.small.font;

	// Create the menu bar label
	labelComponent = kernelWindowNewTextLabel(data.rootWindow->menuBar, text,
		&params);
	if (!labelComponent)
		return (labelComponent);

	// Re-draw the menu bar
	updateMenuBarComponents(&data);

	// Send a shell event
	memset(&shellEvent, 0, sizeof(windowShellEvent));
	shellEvent.type = WINDOW_SHELL_EVENT_NEWBARCOMP;
	shellEvent.component = labelComponent;
	shellEvent.processId = kernelMultitaskerGetCurrentProcessId();

	if (writeShellEvent(&data, &shellEvent) < 0)
	{
		kernelWindowComponentDestroy(labelComponent);
		return (labelComponent = NULL);
	}

	return (labelComponent);
}


int kernelWindowShellDestroyTaskbarComp(kernelWindowComponent *component)
{
	// Destroy a component in the shell's taskbar

	int status = 0;
	windowShellEvent shellEvent;

	kernelDebug(debug_gui, "WindowShell destroy taskbar component");

	// Check params
	if (!component)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check the state of the shell
	status = checkShell(&data);
	if (status < 0)
		return (status = 0);

	// Send a shell event
	memset(&shellEvent, 0, sizeof(windowShellEvent));
	shellEvent.type = WINDOW_SHELL_EVENT_DESTROYBARCOMP;
	shellEvent.component = component;

	status = writeShellEvent(&data, &shellEvent);

	// Destroy it
	kernelWindowComponentDestroy(component);

	// Re-draw the menu bar
	updateMenuBarComponents(&data);

	return (status);
}


kernelWindowComponent *kernelWindowShellIconify(kernelWindow *window,
	int iconify, image *img)
{
	// 'Iconify' (or de-iconify) a window, with an optional image to be used
	// as a taskbar icon.

	kernelWindowComponent *iconComponent = NULL;

	// Check params.  img is allowed to be NULL.
	if (!window)
	{
		kernelError(kernel_error, "NULL parameter");
		return (iconComponent = NULL);
	}

	kernelDebug(debug_gui, "WindowShell %siconify window \"%s\"", (iconify?
		"" : "de-"), window->title);

	// Check the state of the shell
	if (checkShell(&data) < 0)
		return (iconComponent = NULL);

	if (iconify)
		window->flags |= WINDOW_FLAG_ICONIFIED;
	else
		window->flags &= ~WINDOW_FLAG_ICONIFIED;

	if (img)
	{
		iconComponent = kernelWindowShellNewTaskbarIcon(img);
		if (!iconComponent)
			return (iconComponent);
	}

	kernelWindowSetVisible(window, !iconify);

	// Tell the shell to update its window list
	kernelWindowShellUpdateList();

	return (iconComponent);
}

