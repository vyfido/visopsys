//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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

  char *weekDay[] = { "Mon ", "Tue ", "Wed ", "Thu ", "Fri ", 
		      "Sat ", "Sun "};
  char *month[] = { "Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ", 
		    "Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec " };
  static char timeString[25];

  // Make sure timePtr is not NULL
  if (timePtr == NULL)
    {
      errno = ERR_NULLPARAMETER;
      return (NULL);
    }
  
  errno = 0;

  // Get the day of the week
  strncpy(timeString, weekDay[timePtr->tm_wday], 5);

  // Month
  strncat(timeString, month[timePtr->tm_mon], 4);

  // Day of the month
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
