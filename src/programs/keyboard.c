//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  keyboard.c
//

// This is a program for displaying a virtual keyboard.

/* This is the text that appears when a user requests help about this program
<help>

 -- keyboard --

Display a virtual keyboard

Usage:
  keyboard [-i]

The -i flag means run in an 'iconified' mode.

</help>
*/

#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/window.h>

#define _(string) gettext(string)
#define gettext_noop(string) (string)

#define QUIT			gettext_noop("Quit")

#define ICONMENU_QUIT	0
static windowMenuContents iconMenuContents = {
	1,
	{
		{ QUIT, NULL }
	}
};

static int iconify = 0;
static objectKey window = NULL;
static windowKeyboard *keyboard = NULL;
static objectKey keybIcon = NULL;
static objectKey iconMenu = NULL;


static void initMenuContents(void)
{
	strncpy(iconMenuContents.items[ICONMENU_QUIT].text, gettext(QUIT),
		WINDOW_MAX_LABEL_LENGTH);
}


static void refreshWindow(void)
{
	// We got a 'window refresh' event (probably because of a language
	// switch), so we need to update things

	const char *charSet = NULL;

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("keyboard");

	// Re-get the character set
	charSet = getenv(ENV_CHARSET);

	if (charSet)
		windowSetCharSet(window, charSet);

	// Refresh all the menu contents
	initMenuContents();

	// Refresh the icon context menu
	windowMenuUpdate(iconMenu, NULL /* name */, charSet, &iconMenuContents,
		NULL /* params */);

	// Re-layout the window (not necessary if no components have changed)
	//windowLayout(window);
}


static void eventHandler(objectKey key, windowEvent *event)
{
	// Check for window events.
	if (key == window)
	{
		// Check for window refresh
		if (event->type == WINDOW_EVENT_WINDOW_REFRESH)
			refreshWindow();

		// Check for the window being closed
		else if (event->type == WINDOW_EVENT_WINDOW_CLOSE)
			windowGuiStop();
	}

	// Keyboard events
	else if (key == keyboard->canvas)
	{
		keyboard->eventHandler(keyboard, event);
	}

	// Taskbar icon events
	else if (key == keybIcon)
	{
		if (event->type & WINDOW_EVENT_MOUSE_LEFTUP)
		{
			iconify ^= 1;
			windowShellIconify(window, iconify, NULL /* no new icon */);
		}
	}

	// Taskbar icon context menu events
	else if (key == iconMenuContents.items[ICONMENU_QUIT].key)
	{
		if (event->type & WINDOW_EVENT_SELECTION)
			windowGuiStop();
	}
}


static void handleMenuEvents(windowMenuContents *contents)
{
	int count;

	for (count = 0; count < contents->numItems; count ++)
		windowRegisterEventHandler(contents->items[count].key, &eventHandler);
}


static int constructWindow(void)
{
	// If we are in graphics mode, make a window rather than operating on the
	// command line.

	color foreground = { 255, 255, 255 };
	color background = { 230, 60, 35 };
	componentParameters params;
	windowInfo winInfo;
	image img;

	// Create a new window
	window = windowNew(multitaskerGetCurrentProcessId(), "Keyboard");
	if (!window)
		return (ERR_NOCREATE);

	windowSetHasTitleBar(window, 0);
	windowSetBackgroundColor(window, &background);

	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padTop = 5;
	params.padLeft = 5;
	params.padRight = 5;
	params.padBottom = 5;
	params.orientationX = orient_center;
	params.orientationY = orient_middle;
	params.flags = (COMP_PARAMS_FLAG_CUSTOMFOREGROUND |
		COMP_PARAMS_FLAG_CUSTOMBACKGROUND);
	memcpy(&params.foreground, &foreground, sizeof(color));
	memcpy(&params.background, &background, sizeof(color));

	// Create the virtual keyboard.  80% width, and 30% height.  Connect the
	// keyboard's callback function directly to the kernel API function for
	// virtual keyboard input.
	keyboard = windowNewKeyboard(window, ((graphicGetScreenWidth() * 8) / 10),
		((graphicGetScreenHeight() * 3) / 10), &keyboardVirtualInput,
		&params);
	if (!keyboard)
		return (ERR_NOCREATE);

	// Register an event handler to catch keyboard events
	windowRegisterEventHandler(keyboard->canvas, &eventHandler);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);

	// Do window layout, so we can get its size
	windowLayout(window);

	// Put it at the bottom of the screen
	if (windowGetInfo(window, &winInfo) >= 0)
	{
		windowSetLocation(window, ((graphicGetScreenWidth() - winInfo.width) /
			2), (graphicGetScreenHeight() - (winInfo.height + 3)));
	}

	// We don't want the keyboard window to focus
	windowSetFocusable(window, 0);

	// Set up our taskbar icon and iconify, if applicable.
	memset(&img, 0, sizeof(image));
	if (imageLoad(PATH_SYSTEM_ICONS "/keyboard.ico", 24, 24, &img) >= 0)
	{
		keybIcon = windowShellIconify(window, iconify, &img);

		imageFree(&img);

		if (keybIcon)
		{
			// Set up the context menu for the icon
			memset(&params, 0, sizeof(componentParameters));
			initMenuContents();
			iconMenu = windowNewMenu(window, NULL, "icon menu",
				&iconMenuContents, &params);
			if (iconMenu)
			{
				handleMenuEvents(&iconMenuContents);
				windowContextSet(keybIcon, iconMenu);
			}

			windowRegisterEventHandler(keybIcon, &eventHandler);
		}
	}
	else
	{
		windowSetVisible(window, 1);
	}

	return (0);
}


int main(int argc, char *argv[])
{
	int status = 0;
	char opt;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("keyboard");

	// Only work in graphics mode
	if (!graphicsAreEnabled())
	{
		fprintf(stderr, _("\nThe \"%s\" command only works in graphics "
			"mode\n"), (argc? argv[0] : ""));
		return (status = ERR_NOTINITIALIZED);
	}

	// Check options
	while (strchr("i?", (opt = getopt(argc, argv, "i"))))
	{
		switch (opt)
		{
			case 'i':
				// Iconify
				iconify = 1;
				break;

			default:
				fprintf(stderr, _("Unknown option '%c'\n"), optopt);
				return (status = ERR_INVALID);
		}
	}

	// Make our window
	status = constructWindow();
	if (status < 0)
		return (status);

	// Run the GUI
	windowGuiRun();

	// ...and when we come back...
	windowDestroy(window);

	// Also destroy our taskbar icon
	windowShellDestroyTaskbarComp(keybIcon);

	return (status = 0);
}

