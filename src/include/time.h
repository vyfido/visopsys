// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

typedef unsigned clock_t;
typedef unsigned time_t;

char *asctime(const struct tm *timeptr);
clock_t clock(void);
double difftime(time_t, time_t);
time_t time(time_t *t);

#define _TIME_H
#endif
