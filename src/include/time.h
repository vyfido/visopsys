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
//  time.h
//

// This is the Visopsys version of the standard header file time.h

#if !defined(_TIME_H)

#include <stddef.h>

#define CLOCKS_PER_SEC

#ifndef NULL
#define NULL 0
#endif

struct tm {
  int tm_sec;    // seconds
  int tm_min;    // minutes
  int tm_hour;   // hours
  int tm_mday;   // day of the month
  int tm_mon;    // month
  int tm_year;   // year
  int tm_wday;   // day of the week
  int tm_yday;   // day in the year
  int tm_isdst;  // daylight saving time
};

typedef unsigned int clock_t;
typedef unsigned int time_t;

char *asctime(const struct tm *timeptr);
clock_t clock(void);
char *ctime(const time_t *timep);
double difftime(time_t, time_t);
struct tm *gmtime(const time_t *timep);
struct tm *localtime(const time_t *timep);
time_t mktime(struct tm *timeptr);
time_t time(time_t *t);

// For dealing with unimplemented functions
#define not_implemented_int()    \
  {                              \
    errno = ERR_NOTIMPLEMENTED;  \
    return (ERR_NOTIMPLEMENTED); \
  }
#define not_implemented_uns()    \
  {                              \
    errno = ERR_NOTIMPLEMENTED;  \
    return 0;                    \
  }

// These functions are unimplemented
#define strftime(s, n, format, tptr) not_implemented_uns()

#define _TIME_H
#endif
