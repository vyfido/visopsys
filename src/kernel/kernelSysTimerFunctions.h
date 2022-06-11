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
//  kernelSysTimerFunctions.h
//

#if !defined(_KERNELSYSTIMERFUNCTIONS_H)

// Some definitions
#define NULL_TIMER_OBJECT "The system timer object passed or referenced is NULL"
#define NULL_TIMER_DRIVER_OBJECT "The system timer driver object passed or referenced is NULL"
#define NULL_TIMER_DRIVER_ROUTINE "The system timer device driver routine that corresponds to this one is NULL"
#define BAD_WAIT_TICKS "The timer ticks to wait is negative.  Not possible in this dimension.  :-)"
#define NULL_EVENT_FUNCTION "The function being scheduled as a timed event in this instance is NULL"

#define MAXTIMEDEVENTS 100


// Some system timer structures

typedef struct
{

  int (*driverInitialize) (void);
  void (*driverTimerTick) (void);
  int (*driverReadTicks) (void);
  int (*driverReadValue) (int);
  int (*driverSetupTimer) (int, int, int);

} kernelSysTimerDriver;


typedef struct
{

  kernelSysTimerDriver *deviceDriver;

} kernelSysTimerObject;


typedef struct
{

  int targetTime;
  void (*targetCode) (void);
  unsigned int eventCode;

} kernelTimedEvent;


// Functions exported by kernelSysTimerFunctions.c
int kernelSysTimerRegisterDevice(kernelSysTimerObject *);
int kernelSysTimerInstallDriver(kernelSysTimerDriver *);
int kernelSysTimerInitialize(void);
void kernelSysTimerTick(void);
unsigned int kernelSysTimerRead(void);
int kernelSysTimerReadValue(int);
int kernelSysTimerSetupTimer(int, int, int);
void kernelSysTimerWaitTicks(int);
unsigned int kernelTimedEventScheduler(void *, unsigned int);
void kernelTimedEventDispatchAll(void);
int kernelTimedEventCancel(unsigned int);


#define _KERNELSYSTIMERFUNCTIONS_H
#endif
