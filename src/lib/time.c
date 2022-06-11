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
//  time.c
//

// This is the standard "time" function, as found in standard C libraries

#include <time.h>
#include <errno.h>
#include <sys/api.h>

#define SECPERMIN 60
#define SECPERHR  (SECPERMIN * 60)
#define SECPERDAY (SECPERHR * 24)
#define SECPERYR  (SECPERDAY * 365)


time_t time(time_t *t)
{
  // The time() function returns the value of time in seconds since 00:00:00
  // UTC, January 1, 1970.  If tloc is non-zero, the return value is also
  // stored in the location to which tloc points.  On error, ((time_t) -1)
  // is returned, and errno is set appropriately.  If tloc points to an
  // illegal address, time() fails and its actions are undefined.

  time_t time_simple = 0;
  struct tm time_struct;
  int count;

  int month_days[] =
  { 31, /* Jan */ 28, /* Feb */ 31, /* Mar */ 30, /* Apr */
    31, /* May */ 30, /* Jun */ 31, /* Jul */ 31, /* Aug */
    30, /* Sep */ 31, /* Aug */ 30 /* Nov */ };

  // Get the date and time according to the kernel
  errno = rtcDateTime(&time_struct);

  if (errno)
    return (time_simple = -1);

  // One last check
  if (time_struct.tm_year < 1970)
    {
      // Eek, these results would be wrong
      errno = ERR_INVALID;
      return (time_simple = -1);
    }

  // Turn the time structure into the number of seconds since 0:00:00
  // 01/01/1970.

  // Calculate seconds for all complete years
  time_simple = (((time_struct.tm_year - 1) - 1970) * SECPERYR);

  // Add 1 days's worth of seconds for every complete leap year.  There
  // is a leap year in every year divisible by 4 except for years which
  // are both divisible by 100 not by 400.  Got it?
  for (count = (time_struct.tm_year - 1); count >= 1972; count--)
    if (((count % 4) == 0) && (((count % 100) != 0) || ((count % 400) == 0)))
      time_simple += SECPERDAY;

  // Add seconds for all complete months this year
  for (count = (time_struct.tm_mon - 1); count >= 0; count --)
    time_simple += (month_days[count] * SECPERDAY);

  // Add seconds for all complete days in this month
  time_simple += ((time_struct.tm_mday - 1) * SECPERDAY);

  // Add one day's worth of seconds if THIS is a leap year, and if the
  // current month and day are greater than Feb 28
  if (((time_struct.tm_year % 4) == 0) &&
      (((time_struct.tm_year % 100) != 0) ||
       ((time_struct.tm_year % 400) == 0)))
    {
      if ((time_struct.tm_mon > 1) ||
	  ((time_struct.tm_mon == 1) &&
	   (time_struct.tm_mday > 28)))
	time_simple += SECPERDAY;
    }

  // Add seconds for all complete hours in this day
  time_simple += (time_struct.tm_hour * SECPERHR);

  // Add seconds for all complete minutes in this hour
  time_simple += (time_struct.tm_min * SECPERMIN);

  // Add the current seconds
  time_simple += time_struct.tm_sec;

  // Done.
  errno = 0;
  if (t)
    *t = time_simple;
  return (time_simple);
}


