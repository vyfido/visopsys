//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/font.h>
#include <sys/window.h>

static char *weekDay[] = { "Mon ", "Tue ", "Wed ", "Thu ", "Fri ",  "Sat ",
			   "Sun "};
static char *month[] = { "Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ", 
			 "Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec " };
static char timeString[32];


static void makeTime(void)
{
  struct tm theTime;

  bzero(&theTime, sizeof(struct tm));

  // Get the current date and time structure
  if (rtcDateTime(&theTime) < 0)
    return;

  // Turn it into a string
  sprintf(timeString, "%s %s %d - %02d:%02d", weekDay[theTime.tm_wday],
	  month[theTime.tm_mon], theTime.tm_mday, theTime.tm_hour,
	  theTime.tm_min);

  return;
}


int main(int argc __attribute__((unused)), char *argv[])
{
  int status = 0;
  int processId = 0;
  objectKey window = NULL;
  objectKey label = NULL;
  componentParameters params;
  int width, height;
  
  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  processId = multitaskerGetCurrentProcessId();

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "Clock");
  if (window == NULL)
    return (status = ERR_NOTINITIALIZED);

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 2;
  params.padRight = 2;
  params.padTop = 2;
  params.padBottom = 2;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  if (fileFind(FONT_SYSDIR "/arial-bold-10.vbf", NULL) >= 0)
    fontLoad("arial-bold-10.vbf", "arial-bold-10", &(params.font), 0);

  makeTime();
  label = windowNewTextLabel(window, timeString, &params);
  
  // No title bar
  windowSetHasTitleBar(window, 0);

  // Put it in the bottom right corner
  windowGetSize(window, &width, &height);
  windowSetLocation(window, (graphicGetScreenWidth() - width),
   		    (graphicGetScreenHeight() - height));

  // Make it visible
  windowSetVisible(window, 1);

  while(1)
    {
      makeTime();
      windowComponentSetData(label, timeString, (strlen(timeString) + 1));
      sleep(1);
    }
}
