//
//  Visopsys
//  Copyright (C) 1998-2015 J. Andrew McLaughlin
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
//  clock.c
//

// This shows a little clock in the corner of the screen

/* This is the text that appears when a user requests help about this program
<help>

 -- clock --

Show a simple clock in the corner of the screen.

Usage:
  clock

(Only available in graphics mode)

</help>
*/

#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/font.h>
#include <sys/paths.h>
#include <sys/window.h>

#define _(string) gettext(string)
#define gettext_noop(string) (string)

#define WINDOW_TITLE	_("Clock")

static char *weekDay[] = {
	gettext_noop("Mon"),
	gettext_noop("Tue"),
	gettext_noop("Wed"),
	gettext_noop("Thu"),
	gettext_noop("Fri"),
	gettext_noop("Sat"),
	gettext_noop("Sun")
};
static char *month[] = {
	gettext_noop("Jan"),
	gettext_noop("Feb"),
	gettext_noop("Mar"),
	gettext_noop("Apr"),
	gettext_noop("May"),
	gettext_noop("Jun"),
	gettext_noop("Jul"),
	gettext_noop("Aug"),
	gettext_noop("Sep"),
	gettext_noop("Oct"),
	gettext_noop("Nov"),
	gettext_noop("Dec")
};
static char timeString[32];


static void makeTime(void)
{
	struct tm theTime;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("clock");

	memset(&theTime, 0, sizeof(struct tm));

	// Get the current date and time structure
	if (rtcDateTime(&theTime) < 0)
		return;

	// Turn it into a string
	sprintf(timeString, "%s %s %d - %02d:%02d", _(weekDay[theTime.tm_wday]),
		_(month[theTime.tm_mon]), theTime.tm_mday, theTime.tm_hour,
		theTime.tm_min);

	return;
}


int main(int argc __attribute__((unused)), char *argv[])
{
	int status = 0;
	objectKey window = NULL;
	objectKey label = NULL;
	componentParameters params;
	int width, height;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("clock");

	// Only work in graphics mode
	if (!graphicsAreEnabled())
	{
		printf(_("\nThe \"%s\" command only works in graphics mode\n"),
			argv[0]);
		errno = ERR_NOTINITIALIZED;
		return (status = errno);
	}

	// Create a new window, with small, arbitrary size and location
	window = windowNew(multitaskerGetCurrentProcessId(), WINDOW_TITLE);
	if (!window)
		return (status = ERR_NOTINITIALIZED);

	// No title bar
	windowSetHasTitleBar(window, 0);

	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padLeft = 2;
	params.padRight = 2;
	params.padTop = 2;
	params.padBottom = 2;
	params.orientationX = orient_center;
	params.orientationY = orient_middle;
	if (fileFind(PATH_SYSTEM_FONTS "/arial-bold-10.vbf", NULL) >= 0)
		fontLoadSystem("arial-bold-10.vbf", "arial-bold-10",
			&(params.font), 0);

	makeTime();
	label = windowNewTextLabel(window, timeString, &params);

	// Put it in the bottom right corner
	windowGetSize(window, &width, &height);
	windowSetLocation(window, (graphicGetScreenWidth() - width),
		(graphicGetScreenHeight() - height));

	// Make it visible
	windowSetVisible(window, 1);

	while (1)
	{
		makeTime();
		windowComponentSetData(label, timeString, (strlen(timeString) + 1),
			1 /* redraw */);
		sleep(1);
	}
}

