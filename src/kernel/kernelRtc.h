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
//  kernelRtc.h
//

#if !defined(_KERNELRTC_H)

#include <time.h>

// Structures for the Real-Time Clock device

typedef struct
{
  int (*driverInitialize) (void);
  int (*driverRegisterDevice) (void *);
  int (*driverReadSeconds) (void);
  int (*driverReadMinutes) (void);
  int (*driverReadHours) (void);
  int (*driverReadDayOfWeek) (void);
  int (*driverReadDayOfMonth) (void);
  int (*driverReadMonth) (void);
  int (*driverReadYear) (void);

} kernelRtcDriver;

typedef struct 
{
  kernelRtcDriver *driver;

} kernelRtc;

// The default driver initialization
int kernelRtcDriverInitialize(void);

// Functions exported by kernelRtc.c
int kernelRtcRegisterDevice(kernelRtc *);
int kernelRtcInitialize(void);
int kernelRtcReadSeconds(void);
int kernelRtcReadMinutes(void);
int kernelRtcReadHours(void);
int kernelRtcReadDayOfWeek(void);
int kernelRtcReadDayOfMonth(void);
int kernelRtcReadMonth(void);
int kernelRtcReadYear(void);
unsigned kernelRtcUptimeSeconds(void);
unsigned kernelRtcPackedDate(void);
unsigned kernelRtcPackedTime(void);
int kernelRtcDateTime(struct tm *);

#define _KERNELRTC_H
#endif