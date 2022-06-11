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
//  clock.c
//

// This shows a little clock in the corner of the screen

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/window.h>
#include <sys/api.h>

static char *weekDay[] = { "Mon ", "Tue ", "Wed ", "Thu ", "Fri ",  "Sat ",
			   "Sun "};
static char *month[] = { "Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ", 
			 "Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec " };
static char timeString[32];


static void makeTime(void)
{
  struct tm time;

  bzero(&time, sizeof(struct tm));

  // Get the current date and time structure
  if (rtcDateTime(&time) < 0)
    return;

  // Turn it into a string
  sprintf(timeString, "%s %s %d - %02d:%02d", weekDay[time.tm_wday],
	  month[time.tm_mon], time.tm_mday, time.tm_hour, time.tm_min);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  int processId = 0;
  objectKey window = NULL;
  objectKey label = NULL;
  componentParameters params;
  unsigned width, height;
  
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
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;
  fontLoad("arial-bold-10.bmp", "arial-bold-10", &(params.font), 0);

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
      multitaskerWait(20);
    }
}
