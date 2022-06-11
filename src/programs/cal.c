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
//  cal.c
//

// This is a Calendar program, the original version of which was submitted
// by Bauer Vladislav <bauer@ccfit.nsu.ru>

/* This is the text that appears when a user requests help about this program
<help>

 -- cal --

Display the days of the current calendar month.

Usage:
  cal [-T]

In graphics mode, the program is interactive and allows the user to change
the month and year to display.

Options:
-T  : Force text mode operation

</help>
*/

#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/env.h>
#include <sys/font.h>

#define _(string) gettext(string)
#define gettext_noop(string) (string)

#define WINDOW_TITLE	_("Calendar")

static const char *weekDay[] = {
	gettext_noop("Mo"),
	gettext_noop("Tu"),
	gettext_noop("We"),
	gettext_noop("Th"),
	gettext_noop("Fr"),
	gettext_noop("Sa"),
	gettext_noop("Su")
};

static const char *monthName[] = {
	gettext_noop("January"),
	gettext_noop("February"),
	gettext_noop("March"),
	gettext_noop("April"),
	gettext_noop("May"),
	gettext_noop("June"),
	gettext_noop("July"),
	gettext_noop("August"),
	gettext_noop("September"),
	gettext_noop("October"),
	gettext_noop("November"),
	gettext_noop("December")
};

static int monthDays[12] = {
	31 /* Jan */, 00 /* Feb */, 31 /* Mar */, 30 /* Apr */, 31 /* May */,
	30 /* Jun */, 31 /* Jul */, 31 /* Aug */, 30 /* Sep */, 31 /* Oct */,
	30 /* Nov */, 31 /* Dec */
};

static int graphics = 0;
static int date = 0;
static int month = 0;
static int year = 0;
static int dayOfWeek = 0;

// For graphics mode
static objectKey window = NULL;
static objectKey monthLabel = NULL;
static objectKey yearLabel = NULL;
static objectKey calList = NULL;
static listItemParameters *calListParams = NULL;
static objectKey minusMonthButton = NULL;
static objectKey plusMonthButton  = NULL;
static objectKey minusYearButton = NULL;
static objectKey plusYearButton  = NULL;


static int leapYear(int y)
{
	// There is a leap year in every year divisible by 4 except for years
	// which are both divisible by 100 and not divisible by 400.  Got it?
	if (!(y % 4) && ((y % 100) || !(y % 400)))
		return (1);
	else
		return (0);
}


static int getDays(int m, int y)
{
	if (m == 1)
	{
		if (leapYear(y))
			return (29);
		else
			return (28);
	}

	return (monthDays[m]);
}


static void textCalendar(void)
{
	int days = getDays((month - 1), year);
	int firstDay = rtcDayOfWeek(1, month, year);
	int spaceSkip = (10 - (strlen(_(monthName[month - 1])) + 5) / 2);
	int count;

	for (count = 0; count < spaceSkip; count++)
		printf(" ");

	printf("%s %i", _(monthName[month - 1]), year);

	printf("\n");
	for (count = 0; count < 7; count++)
		printf("%s ", _(weekDay[count]));

	printf("\n");
	for (count = 0; count < firstDay; count++)
		printf("   ");

	for (count = 1; count <= days; count++)
	{
		dayOfWeek = rtcDayOfWeek(count, month, year);
		printf("%2i ", count);

		if (dayOfWeek == 6)
			printf("\n");
	}

	if (dayOfWeek != 6)
		printf("\n");
}


static void initCalListParams(void)
{
	int days = getDays((month - 1), year);
	int firstDay = rtcDayOfWeek(1, month, year);
	int count;

	for (count = 0; count < 7; count++)
		sprintf(calListParams[count].text, "%s", _(weekDay[count]));

	for (count = 7; count < 49; count++)
		sprintf(calListParams[count].text, "  ");

	for (count = 1; count <= days; count++)
		sprintf(calListParams[count + 6 + firstDay].text, "%2i", count);
}


static void getUpdate(void)
{
	char monthYearString[32];

	initCalListParams();

	// Set the month and year labels

	snprintf(monthYearString, sizeof(monthYearString), "%s",
		_(monthName[month - 1]));

	windowComponentSetData(monthLabel, monthYearString,
		strlen(monthYearString), 1 /* redraw */);

	snprintf(monthYearString, sizeof(monthYearString), "%d", year);

	windowComponentSetData(yearLabel, monthYearString,
		strlen(monthYearString), 1 /* redraw */);

	// Set the calendar contents

	windowComponentSetSelected(calList, -1);

	windowComponentSetData(calList, calListParams, 49, 1 /* redraw */);

	if ((month == rtcReadMonth()) && (year == rtcReadYear()))
	{
		windowComponentSetSelected(calList, (6 + rtcDayOfWeek(1, month,
			year) + date));
	}

	// Set the plus-minus month buttons

	if (month > 1)
	{
		snprintf(monthYearString, sizeof(monthYearString), "<< %s",
			_(monthName[month - 2]));
	}
	else
	{
		snprintf(monthYearString, sizeof(monthYearString), "<< %s",
			_(monthName[11]));
	}

	windowComponentSetData(minusMonthButton, monthYearString,
		strlen(monthYearString), 1 /* redraw */);

	if (month < 12)
	{
		snprintf(monthYearString, sizeof(monthYearString), "%s >>",
			_(monthName[month]));
	}
	else
	{
		snprintf(monthYearString, sizeof(monthYearString), "%s >>",
			_(monthName[0]));
	}

	windowComponentSetData(plusMonthButton, monthYearString,
		strlen(monthYearString), 1 /* redraw */);

	// Set the plus-minus year buttons

	sprintf(monthYearString, "<< %d", (year - 1));
	windowComponentSetData(minusYearButton, monthYearString,
		strlen(monthYearString), 1 /* redraw */);

	sprintf(monthYearString, "%d >>", (year + 1));
	windowComponentSetData(plusYearButton, monthYearString,
		strlen(monthYearString), 1 /* redraw */);
}


static void refreshWindow(void)
{
	// We got a 'window refresh' event (probably because of a language
	// switch), so we need to update things

	// Re-get the language setting
	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("cal");

	// Re-get the character set
	if (getenv(ENV_CHARSET))
		windowSetCharSet(window, getenv(ENV_CHARSET));

	// Refresh the contents
	getUpdate();

	// Refresh the window title
	windowSetTitle(window, WINDOW_TITLE);

	// Re-layout the window
	windowLayout(window);
}


static void eventHandler(objectKey key, windowEvent *event)
{
	// Check for window events
	if (key == window)
	{
		// Check for window refresh
		if (event->type == WINDOW_EVENT_WINDOW_REFRESH)
			refreshWindow();

		// Check for the window being closed
		else if (event->type == WINDOW_EVENT_WINDOW_CLOSE)
			windowGuiStop();
	}

	else if (event->type == WINDOW_EVENT_MOUSE_LEFTUP)
	{
		if (key == minusMonthButton)
		{
			month -= 1;
			if (month < 1)
			{
				month = 12;
				year -= 1;
			}
		}
		else if (key == plusMonthButton)
		{
			month += 1;
			if (month > 12)
			{
				month = 1;
				year += 1;
			}
		}
		else if (key == minusYearButton)
		{
			year = ((year >= 1900)? (year - 1) : 1900);
		}
		else if (key == plusYearButton)
		{
			year = ((year <= 3000)? (year + 1) : 3000);
		}

		if ((key == minusMonthButton) || (key == plusMonthButton) ||
			(key == minusYearButton) || (key == plusYearButton))
		{
			getUpdate();
		}
	}
}


static void constructWindow(void)
{
	objectKey container = NULL;
	componentParameters params;

	window = windowNew(multitaskerGetCurrentProcessId(), WINDOW_TITLE);
	if (!window)
		exit(ERR_NOTINITIALIZED);

	memset(&params, 0, sizeof(componentParameters));
	params.gridWidth = 1;
	params.gridHeight = 1;
	params.padTop = params.padLeft = params.padRight = 5;
	params.orientationX = orient_left;
	params.orientationY = orient_middle;
	params.font = fontGet(FONT_FAMILY_ARIAL, FONT_STYLEFLAG_BOLD, 20,
		NULL /* charSet */);
	monthLabel = windowNewTextLabel(window, "", &params);

	params.gridX += 1;
	params.orientationX = orient_right;
	yearLabel = windowNewTextLabel(window, "", &params);

	params.gridWidth = 2;
	params.gridX = 0;
	params.gridY += 1;
	params.padLeft = params.padRight = 5;
	params.orientationX = orient_center;
	params.flags = (COMP_PARAMS_FLAG_FIXEDWIDTH |
		COMP_PARAMS_FLAG_NOSCROLLBARS);
	initCalListParams();
	calList = windowNewList(window, windowlist_textonly, 7 /* rows */,
		7 /* columns */, 0 /* selectMultiple */, calListParams,
		49 /* numItems */, &params);

	params.gridY += 1;
	params.padBottom = 5;
	params.font = NULL;
	params.flags = 0;
	container = windowNewContainer(window, "buttonContainer", &params);

	params.gridWidth = 1;
	params.gridX = params.gridY = 0;
	params.padTop = params.padBottom = params.padLeft = 0;
	params.padRight = 3;
	params.orientationX = orient_right;
	params.font = fontGet(FONT_FAMILY_LIBMONO, FONT_STYLEFLAG_BOLD, 8,
		NULL /* charSet */);
	minusMonthButton = windowNewButton(container, "<< XXXXXXXXX", NULL,
		&params);
	windowRegisterEventHandler(minusMonthButton, &eventHandler);

	params.gridX += 1;
	params.padLeft = 3;
	params.padRight = 5;
	params.orientationX = orient_left;
	plusMonthButton = windowNewButton(container, "XXXXXXXXX >>", NULL,
		&params);
	windowRegisterEventHandler(plusMonthButton, &eventHandler);

	params.gridX += 1;
	params.padLeft = 5;
	params.padRight = 3;
	params.orientationX = orient_right;
	minusYearButton = windowNewButton(container, "<< XXXX", NULL, &params);
	windowRegisterEventHandler(minusYearButton, &eventHandler);

	params.gridX += 1;
	params.padLeft = 3;
	params.padRight = 0;
	params.orientationX = orient_left;
	plusYearButton = windowNewButton(container, "XXXX >>", NULL, &params);
	windowRegisterEventHandler(plusYearButton, &eventHandler);

	getUpdate();
	windowComponentSetSelected(calList, (6 + rtcDayOfWeek(1, month, year) +
		date));
	windowComponentFocus(calList);
	windowRegisterEventHandler(window, &eventHandler);

	// Make the window visible
	windowSetVisible(window, 1);
}


static void graphCalendar(void)
{
	calListParams = calloc(49, sizeof(listItemParameters));
	if (!calListParams)
		exit(ERR_MEMORY);

	constructWindow();
	windowGuiRun();
	windowDestroy(window);
	free(calListParams);
}


int main(int argc, char *argv[])
{
	int status = 0;
	char opt;

	setlocale(LC_ALL, getenv(ENV_LANG));
	textdomain("cal");

	// Are graphics enabled?
	graphics = graphicsAreEnabled();

	// Check options
	while (strchr("T?", (opt = getopt(argc, argv, "T"))))
	{
		switch (opt)
		{
			case 'T':
				// Force text mode
				graphics = 0;
				break;

			default:
				fprintf(stderr, _("Unknown option '%c'\n"), optopt);
				return (status = ERR_INVALID);
		}
	}

	date = rtcReadDayOfMonth();
	month = rtcReadMonth();
	year = rtcReadYear();

	if (graphics)
		graphCalendar();
	else
		textCalendar();

	return (status);
}

