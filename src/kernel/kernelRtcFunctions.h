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
//  kernelRtcFunctions.h
//

#if !defined(_KERNELRTCFUNCTIONS_H)

#include <time.h>

// Definitions

// Error Messages
#define NULL_RTC_OBJECT "The Real-Time clock device object that was passed or referenced is NULL"
#define NULL_RTC_DRIVER_OBJECT "The Real-Time clock device driver that was passed or referenced is NULL"
#define NULL_RTC_DRIVER_ROUTINE "The corresponding Real-Time clock device driver routine is NULL"
#define RTC_INIT_FAILED "The Real-Time clock driver initialization returned this error code"

// Structures for the Real-Time Clock device

typedef struct
{
  int (*driverInitialize) (void);
  int (*driverReadSeconds) (void);
  int (*driverReadMinutes) (void);
  int (*driverReadHours) (void);
  int (*driverReadDayOfWeek) (void);
  int (*driverReadDayOfMonth) (void);
  int (*driverReadMonth) (void);
  int (*driverReadYear) (void);

} kernelRtcDeviceDriver;

typedef struct 
{
  kernelRtcDeviceDriver *deviceDriver;

} kernelRtcObject;


// Functions exported by kernelRtcFunctions.c
int kernelRtcRegisterDevice(kernelRtcObject *);
int kernelRtcInstallDriver(kernelRtcDeviceDriver *);
int kernelRtcInitialize(void);
int kernelRtcReadSeconds(void);
int kernelRtcReadMinutes(void);
int kernelRtcReadHours(void);
int kernelRtcReadDayOfWeek(void);
int kernelRtcReadDayOfMonth(void);
int kernelRtcReadMonth(void);
int kernelRtcReadYear(void);
unsigned int kernelRtcUptimeSeconds(void);
unsigned int kernelRtcPackedDate(void);
unsigned int kernelRtcPackedTime(void);
int kernelRtcDateTime(struct tm *);

#define _KERNELRTCFUNCTIONS_H
#endif
