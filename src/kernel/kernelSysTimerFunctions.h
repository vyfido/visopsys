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
//  kernelSysTimerFunctions.h
//

#if !defined(_KERNELSYSTIMERFUNCTIONS_H)

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

// Functions exported by kernelSysTimerFunctions.c
int kernelSysTimerRegisterDevice(kernelSysTimerObject *);
int kernelSysTimerInstallDriver(kernelSysTimerDriver *);
int kernelSysTimerInitialize(void);
void kernelSysTimerTick(void);
unsigned kernelSysTimerRead(void);
int kernelSysTimerReadValue(int);
int kernelSysTimerSetupTimer(int, int, int);
void kernelSysTimerWaitTicks(int);


#define _KERNELSYSTIMERFUNCTIONS_H
#endif
