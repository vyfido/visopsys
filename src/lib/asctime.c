//
//  Visopsys
//  Copyright (C) 1998-2001 J. Andrew McLaughlin
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
//  asctime.c
//

// This is the standard "asctime" function, as found in standard C libraries

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


char *asctime(const struct tm *timePtr)
{
  // From the linux man page about this function:
  // The asctime() function converts the broken-down time value
  // timeptr into a string with the  same  format  as  ctime().
  // The  return  value points to a statically allocated string
  // which might be overwritten by subsequent calls to  any  of
  // the date and time functions.
  
  // ctime() time format:
  // "Wed Jun 30 21:49:08 1993\n"

  static char timeString[25];
  char *misc = NULL;


  // Make sure timePtr is not NULL
  if (timePtr == NULL)
    {
      errno = ERR_NULLPARAMETER;
      return (NULL);
    }
  
  errno = 0;

  // What's the day of the week?
  switch (timePtr->tm_wday)
    {
    case 0:
      misc = "Sun ";
      break;
    case 1:
      misc = "Mon ";
      break;
    case 2:
      misc = "Tue ";
      break;
    case 3:
      misc = "Wed ";
      break;
    case 4:
      misc = "Thu ";
      break;
    case 5:
      misc = "Fri ";
      break;
    case 6:
      misc = "Sat ";
      break;
    default:
      misc = "??? ";
      break;
    }

  // Copy it into our time string
  strncpy(timeString, misc, 4);

  // Month?
  switch (timePtr->tm_wday)
    {
    case 0:
      misc = "Jan ";
      break;
    case 1:
      misc = "Feb ";
      break;
    case 2:
      misc = "Mar ";
      break;
    case 3:
      misc = "Apr ";
      break;
    case 4:
      misc = "May ";
      break;
    case 5:
      misc = "Jun ";
      break;
    case 6:
      misc = "Jul ";
      break;
    case 7:
      misc = "Aug ";
      break;
    case 8:
      misc = "Sep ";
      break;
    case 9:
      misc = "Oct ";
      break;
    case 10:
      misc = "Nov ";
      break;
    case 11:
      misc = "Dec ";
      break;
    default:
      misc = "??? ";
      break;
    }

  // Copy it into our time string
  strncat(timeString, misc, 4);

  // Day of the month?
  itoa(timePtr->tm_mday, (timeString + strlen(timeString)));
  strncat(timeString, " ", 1);

  // Hour
  if (timePtr->tm_hour < 10)
    strncat(timeString, "0", 1);
  itoa(timePtr->tm_hour, (timeString + strlen(timeString)));
  strncat(timeString, ":", 1);

  // Minute
  if (timePtr->tm_min < 10)
    strncat(timeString, "0", 1);
  itoa(timePtr->tm_min, (timeString + strlen(timeString)));
  strncat(timeString, ":", 1);

  // Second
  if (timePtr->tm_sec < 10)
    strncat(timeString, "0", 1);
  itoa(timePtr->tm_sec, (timeString + strlen(timeString)));
  strncat(timeString, " ", 1);

  // Year
  itoa(timePtr->tm_year, (timeString + strlen(timeString)));

  // Ok, return a pointer to timeString
  return (timeString);
}
