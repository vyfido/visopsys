//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  disprops.c
//

// Sets display properties for the kernel's window manager

/* This is the text that appears when a user requests help about this program
<help>

 -- disprops --

Control the display properties

Usage:
  disprops

The disprops program is interactive, and may only be used in graphics mode.
It can be used to change display settings, such as the screen resolution,
the background wallpaper, and the base colors used by the window manager.

</help>
*/

#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>
#include <sys/image.h>
#include <sys/window.h>

#define _(string) gettext(string)

#define KERNEL_CONFIG			"/system/config/kernel.conf"
#define WINDOW_CONFIG			"/system/config/window.conf"
#define DESKTOP_CONFIG			"/system/config/desktop.conf"
#define CLOCK_VARIABLE			"program.clock"
#define WALLPAPER_VARIABLE		"background.image"
#define CLOCK_PROGRAM			"/programs/clock"
#define WALLPAPER_PROGRAM		"/programs/wallpaper"
#define MAX_IMAGE_DIMENSION		128

typedef struct {
	char description[32];
	videoMode mode; 
} modeInfo;

static int processId = 0;
static int privilege = 0;
static int readOnly = 1;
static int numberModes = 0;
static int showingClock = 0;
static videoMode currentMode;
static videoMode videoModes[MAXVIDEOMODES];
static listItemParameters *listItemParams = NULL;
static objectKey window = NULL;
static objectKey modeList = NULL;
static objectKey bootGraphicsCheckbox = NULL;
static objectKey showClockCheckbox = NULL;
static objectKey wallpaperCheckbox = NULL;
static objectKey wallpaperButton = NULL;
static objectKey wallpaperImage = NULL;
static objectKey colorsRadio = NULL;
static objectKey canvas = NULL;
static objectKey changeColorsButton = NULL;
static objectKey okButton = NULL;
static objectKey cancelButton = NULL;

static color foreground = { 230, 60, 35 };
static color background = { 230, 60, 35 };
static color desktop = { 230, 60, 35 };
static int colorsChanged = 0;


static int getVideoModes(void)
{
	int status = 0;
	int count;

	// Try to get the supported video modes from the kernel
	numberModes = graphicGetModes(videoModes, sizeof(videoModes));
	if (numberModes <= 0)
		return (status = ERR_NODATA);

	if (listItemParams)
		free(listItemParams);

	listItemParams = malloc(numberModes * sizeof(listItemParameters));
	if (listItemParams == NULL)
		return (status = ERR_MEMORY);

	// Construct the mode strings
	for (count = 0; count < numberModes; count ++)
		snprintf(listItemParams[count].text, WINDOW_MAX_LABEL_LENGTH,
			 _(" %d x %d, %d bit "),  videoModes[count].xRes,
			 videoModes[count].yRes, videoModes[count].bitsPerPixel);

	// Get the current mode
	graphicGetMode(&currentMode);

	return (status = 0);
}


static void getColors(void)
{
	// Get the current color scheme from the kernel configuration.

	variableList list;
	char value[128];

	configRead(KERNEL_CONFIG, &list);

	if (list.memory)
	{
		if (variableListGet(&list, "foreground.color.red", value, 128) >= 0)
			foreground.red = atoi(value);
		if (variableListGet(&list, "foreground.color.green", value, 128) >= 0)
			foreground.green = atoi(value);
		if (variableListGet(&list, "foreground.color.blue", value, 128) >= 0)
			foreground.blue = atoi(value);
		if (variableListGet(&list, "background.color.red", value, 128) >= 0)
			background.red = atoi(value);
		if (variableListGet(&list, "background.color.green", value, 128) >= 0)
			background.green = atoi(value);
		if (variableListGet(&list, "background.color.blue", value, 128) >= 0)
			background.blue = atoi(value);
		if (variableListGet(&list, "desktop.color.red", value, 128) >= 0)
			desktop.red = atoi(value);
		if (variableListGet(&list, "desktop.color.green", value, 128) >= 0)
			desktop.green = atoi(value);
		if (variableListGet(&list, "desktop.color.blue", value, 128) >= 0)
			desktop.blue = atoi(value);

		variableListDestroy(&list);
	}
}


static color *getSelectedColor(void)
{
	int selected = 0;

	windowComponentGetSelected(colorsRadio, &selected);
	
	switch (selected)
	{
		case 0:
		default:
			return (&foreground);
		case 1:
			return(&background);
		case 2:
			return(&desktop);
	}
}


static void drawColor(color *draw)
{
	// Draw the current color on the canvas

	windowDrawParameters drawParams;

	bzero(&drawParams, sizeof(windowDrawParameters));
	drawParams.operation = draw_rect;
	drawParams.mode = draw_normal;
	drawParams.foreground.red = draw->red;
	drawParams.foreground.green = draw->green;
	drawParams.foreground.blue = draw->blue;
	drawParams.xCoord1 = 0;
	drawParams.yCoord1 = 0;
	drawParams.width = windowComponentGetWidth(canvas);
	drawParams.height = windowComponentGetHeight(canvas);
	drawParams.thickness = 1;
	drawParams.fill = 1;
	windowComponentSetData(canvas, &drawParams, sizeof(windowDrawParameters));
}


static void eventHandler(objectKey key, windowEvent *event)
{
	color *selectedColor = getSelectedColor();
	int mode = 0;
	int clockSelected = 0;
	char string[160];
	int selected = 0;
	file tmp;

	if (key == window)
	{
		// Check for the window being closed by a GUI event.
		if (event->type == EVENT_WINDOW_CLOSE)
			windowGuiStop();

		// Check for window resize
		if (event->type == EVENT_WINDOW_RESIZE)
			// Redraw the canvas
			drawColor(selectedColor);
	}

	else if ((key == wallpaperCheckbox) && (event->type == EVENT_MOUSE_LEFTUP))
	{
		windowComponentGetSelected(wallpaperCheckbox, &selected);
		windowComponentSetEnabled(wallpaperButton, selected);
		if (!selected)
			windowThumbImageUpdate(wallpaperImage, NULL, MAX_IMAGE_DIMENSION,
				 MAX_IMAGE_DIMENSION);
		windowComponentSetEnabled(wallpaperImage, selected);
	}

	else if ((key == wallpaperButton) && (event->type == EVENT_MOUSE_LEFTUP))
	{
		loaderLoadAndExec(WALLPAPER_PROGRAM, privilege, 1);
		if (configGet(DESKTOP_CONFIG, WALLPAPER_VARIABLE, string, 160) >= 0)
			windowThumbImageUpdate(wallpaperImage, string, MAX_IMAGE_DIMENSION,
				 MAX_IMAGE_DIMENSION);
	}

	else if ((key == colorsRadio) || (key == changeColorsButton))
	{
		if ((key == changeColorsButton) && (event->type == EVENT_MOUSE_LEFTUP))
		{
			windowNewColorDialog(window, selectedColor);
			colorsChanged = 1;
		}

		if (((key == changeColorsButton) &&
			(event->type == EVENT_MOUSE_LEFTUP)) ||
			((key == colorsRadio) && (event->type & EVENT_SELECTION)))
		{
			drawColor(selectedColor);
		}
	}

	else if ((key == okButton) && (event->type == EVENT_MOUSE_LEFTUP))
	{
		// Does the user not want to boot in graphics mode?
		windowComponentGetSelected(bootGraphicsCheckbox, &selected);
		if (!selected)
		{
			// Try to create the /nograph file
			bzero(&tmp, sizeof(file));
			fileOpen("/nograph", (OPENMODE_WRITE | OPENMODE_CREATE |
				OPENMODE_TRUNCATE), &tmp);
			fileClose(&tmp);
		}

		// Does the user want to show a clock on the desktop?
		windowComponentGetSelected(showClockCheckbox, &clockSelected);
		if ((!showingClock && clockSelected) ||
			(showingClock && !clockSelected))
		{
			if (!showingClock && clockSelected)
			{
				// Run the clock program now.  No block.
				loaderLoadAndExec(CLOCK_PROGRAM, privilege, 0);
	
				if (!readOnly)
					// Set the variable for the clock
					configSet(DESKTOP_CONFIG, CLOCK_VARIABLE, CLOCK_PROGRAM);
			}
			else
			{
				// Try to kill any clock program(s) currently running
				multitaskerKillByName("clock", 0);

				if (!readOnly)
					// Remove any clock variable
					configUnset(DESKTOP_CONFIG, CLOCK_VARIABLE);
			}
		}

		windowComponentGetSelected(modeList, &mode);
		if ((mode >= 0) && (videoModes[mode].mode != currentMode.mode))
		{
			if (!graphicSetMode(&(videoModes[mode])))
			{
				sprintf(string, _("The resolution has been changed to %dx%d, "
					"%dbpp\nThis will take effect after you reboot."),
					videoModes[mode].xRes, videoModes[mode].yRes,
					videoModes[mode].bitsPerPixel);
				windowNewInfoDialog(window, _("Changed"), string);
			}
			else
			{
				sprintf(string, _("Error %d setting mode"), mode);
				windowNewErrorDialog(window, _("Error"), string);
			}
		}

		windowComponentGetSelected(wallpaperCheckbox, &selected);
		if (!selected && (fileFind(WALLPAPER_PROGRAM, NULL) >= 0))
			system(WALLPAPER_PROGRAM " none");

		if (colorsChanged)
		{
			// Set the colors
			windowSetColor("foreground", &foreground);
			windowSetColor("background", &background);
			windowSetColor("desktop", &desktop);
			windowResetColors();
			colorsChanged = 0;
		}

		windowGuiStop();
	}

	else if ((key == cancelButton) && (event->type == EVENT_MOUSE_LEFTUP))
		windowGuiStop();

	return;
}


static void constructWindow(void)
{
	// If we are in graphics mode, make a window rather than operating on the
	// command line.

	componentParameters params;
	objectKey container = NULL;
	process tmpProc;
	char value[128];
	int count;

	// Create a new window, with small, arbitrary size and location
	window = windowNew(processId, _("Display Settings"));
	if (window == NULL)
		return;

	bzero(&params, sizeof(componentParameters));

	// Make a container for the left hand side components
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.orientationX = orient_left;
	params.orientationY = orient_top;
	container = windowNewContainer(window, "leftContainer", &params);

	// Make a label for the graphics modes
	params.gridWidth = 2;
	params.padTop = 5;
	params.padLeft = 5;
	params.padRight = 5;
	params.flags |= WINDOW_COMPFLAG_FIXEDHEIGHT;
	windowNewTextLabel(container, _("Screen resolution:"), &params);

	// Make a list with all the available graphics modes
	params.gridY++;
	params.flags &= ~WINDOW_COMPFLAG_FIXEDHEIGHT;
	modeList = windowNewList(container, windowlist_textonly, 5, 1, 0,
		listItemParams, numberModes, &params);

	// Select the current mode
	for (count = 0; count < numberModes; count ++)
	{
		if (videoModes[count].mode == currentMode.mode)
		{
			windowComponentSetSelected(modeList, count);
			break;
		}
	}
	if (readOnly || privilege)
		windowComponentSetEnabled(modeList, 0);

	// A label for the colors
	params.gridY++;
	params.padTop = 10;
	params.flags |= WINDOW_COMPFLAG_FIXEDHEIGHT;
	windowNewTextLabel(container, _("Colors:"), &params);

	// Create the colors radio button
	params.gridY++;
	params.gridWidth = 1;
	params.gridHeight = 2;
	params.padTop = 5;
	params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
	colorsRadio = windowNewRadioButton(container, 3, 1, (char *[])
		{ _("Foreground"), _("Background"), _("Desktop") }, 3 , &params);
	windowRegisterEventHandler(colorsRadio, &eventHandler);

	// The canvas to show the current color
	params.gridX++;
	params.gridHeight = 1;
	params.flags &= ~(WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
	params.flags |= WINDOW_COMPFLAG_HASBORDER;
	canvas = windowNewCanvas(container, 50, 50, &params);

	// Create the change color button
	params.gridY++;
	params.flags &= ~WINDOW_COMPFLAG_HASBORDER;
	changeColorsButton = windowNewButton(container, _("Change"), NULL, &params);
	windowRegisterEventHandler(changeColorsButton, &eventHandler);

	// Adjust the canvas width so that it matches the width of the button.
	windowComponentSetWidth(canvas, windowComponentGetWidth(changeColorsButton));

	// A little divider
	params.gridX = 1;
	params.gridY = 0;
	params.orientationX = orient_center;
	params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
	windowNewDivider(window, divider_vertical, &params);

	// Make a container for the right hand side components
	params.gridX = 2;
	params.padTop = 0;
	params.padLeft = 0;
	params.padRight = 0;
	params.orientationX = orient_left;
	params.flags &= ~WINDOW_COMPFLAG_FIXEDWIDTH;
	container = windowNewContainer(window, "rightContainer", &params);

	// A label for the background wallpaper
	params.gridX = 0;
	params.padTop = 5;
	params.padLeft = 5;
	params.padRight = 5;
	params.flags |= WINDOW_COMPFLAG_FIXEDHEIGHT;
	windowNewTextLabel(container, _("Background wallpaper:"), &params);

	// Create the thumbnail image for the background wallpaper.  Start with a
	// blank one and update it in a minute.
	params.gridY++;
	params.flags |= WINDOW_COMPFLAG_HASBORDER;
	wallpaperImage = windowNewThumbImage(container, NULL, MAX_IMAGE_DIMENSION,
		MAX_IMAGE_DIMENSION, &params);

	// Create the background wallpaper button
	params.gridY++;
	params.flags &= ~WINDOW_COMPFLAG_HASBORDER;
	params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
	wallpaperButton = windowNewButton(container, _("Choose"), NULL, &params);
	windowRegisterEventHandler(wallpaperButton, &eventHandler);

	// Create the checkbox for whether to use background wallpaper
	params.gridY++;
	wallpaperCheckbox =
		windowNewCheckbox(container, _("Use background wallpaper"), &params);
	windowComponentSetSelected(wallpaperCheckbox, 1);
	windowRegisterEventHandler(wallpaperCheckbox, &eventHandler);

	// Try to get the wallpaper image name
	if (configGet(DESKTOP_CONFIG, WALLPAPER_VARIABLE, value, 128) >= 0)
		windowThumbImageUpdate(wallpaperImage, value, MAX_IMAGE_DIMENSION,
			MAX_IMAGE_DIMENSION);
	else
	{
		windowComponentSetSelected(wallpaperCheckbox, 0);
		windowComponentSetEnabled(wallpaperButton, 0);
	}

	if (fileFind(WALLPAPER_PROGRAM, NULL) < 0)
	{
		windowComponentSetEnabled(wallpaperButton, 0);
		windowComponentSetEnabled(wallpaperCheckbox, 0);
	}

	// A little divider
	params.gridY++;
	params.flags &= ~WINDOW_COMPFLAG_FIXEDWIDTH;
	windowNewDivider(container, divider_horizontal, &params);

	// A label for the miscellaneous stuff
	params.gridY++;
	params.flags |= WINDOW_COMPFLAG_FIXEDWIDTH;
	windowNewTextLabel(container, _("Miscellaneous:"), &params);

	// Make a checkbox for whether to boot in graphics mode
	params.gridY++;
	bootGraphicsCheckbox =
		windowNewCheckbox(container, _("Boot in graphics mode"), &params);
	windowComponentSetSelected(bootGraphicsCheckbox, 1);
	if (readOnly)
		windowComponentSetEnabled(bootGraphicsCheckbox, 0);

	// Make a checkbox for whether to show the clock on the desktop
	params.gridY++;
	showClockCheckbox =
		windowNewCheckbox(container, _("Show a clock on the desktop"), &params);

	// Are we currently set to show one?
	bzero(&tmpProc, sizeof(process));
	if (multitaskerGetProcessByName("clock", &tmpProc) == 0)
	{
		showingClock = 1;
		windowComponentSetSelected(showClockCheckbox, showingClock);
	}

	if (fileFind(CLOCK_PROGRAM, NULL) < 0)
		windowComponentSetEnabled(showClockCheckbox, 0);

	// Make a container for the bottom buttons
	params.gridX = 0;
	params.gridY = 1;
	params.gridWidth = 3;
	params.padTop = 0;
	params.padLeft = 0;
	params.padRight = 0;
	params.orientationX = orient_center;
	params.flags |= (WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
	container = windowNewContainer(window, "bottomContainer", &params);

	// Create the OK button
	params.gridY = 0;
	params.gridWidth = 1;
	params.padTop = 5;
	params.padLeft = 5;
	params.padRight = 5;
	params.padBottom = 5;
	params.orientationX = orient_right;
	okButton = windowNewButton(container, _("OK"), NULL, &params);
	windowRegisterEventHandler(okButton, &eventHandler);

	// Create the Cancel button
	params.gridX++;
	params.orientationX = orient_left;
	cancelButton = windowNewButton(container, _("Cancel"), NULL, &params);
	windowRegisterEventHandler(cancelButton, &eventHandler);
	windowComponentFocus(cancelButton);

	// Register an event handler to catch window close events
	windowRegisterEventHandler(window, &eventHandler);
	
	windowSetVisible(window, 1);

	// Get the current colors we're interested in
	windowGetColor("foreground", &foreground);
	windowGetColor("background", &background);
	windowGetColor("desktop", &desktop);

	drawColor(&foreground);

	return;
}


int main(int argc __attribute__((unused)), char *argv[])
{
	int status = 0;
	char *language = "";
	disk sysDisk;

#ifdef BUILDLANG
	language=BUILDLANG;
#endif
	setlocale(LC_ALL, language);
	textdomain("disprops");

	// Only work in graphics mode
	if (!graphicsAreEnabled())
	{
		printf(_("\nThe \"%s\" command only works in graphics mode\n"), argv[0]);
		return (status = ERR_NOTINITIALIZED);
	}

	// Find out whether we are currently running on a read-only filesystem
	if (!fileGetDisk("/system", &sysDisk))
		readOnly = sysDisk.readOnly;

	// We need our process ID and privilege to create the windows
	processId = multitaskerGetCurrentProcessId();
	privilege = multitaskerGetProcessPrivilege(processId);

	// Get the list of supported video modes
	status = getVideoModes();
	if (status < 0)
		return (status);

	// Get the current color scheme
	getColors();

	// Make the window
	constructWindow();

	// Run the GUI
	windowGuiRun();
	windowDestroy(window);

	if (listItemParams)
		free(listItemParams);

	return (status);
}
