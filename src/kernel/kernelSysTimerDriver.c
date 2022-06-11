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
//  kernelSysTimerDriver.c
//

// Driver for standard PC system timer chip

#include "kernelDriverManagement.h" // Contains my prototypes
#include "kernelProcessorX86.h"
#include <sys/errors.h>


int kernelSysTimerDriverRegisterDevice(void *);
void kernelSysTimerDriverTick(void);
int kernelSysTimerDriverRead(void);
int kernelSysTimerDriverSetTimer(int, int, int);
int kernelSysTimerDriverReadTimer(int);

static kernelSysTimerDriver defaultSysTimerDriver =
{
  kernelSysTimerDriverInitialize,
  kernelSysTimerDriverRegisterDevice,
  kernelSysTimerDriverTick,
  kernelSysTimerDriverRead,
  kernelSysTimerDriverReadTimer,
  kernelSysTimerDriverSetTimer
};

static int portNumber[] = { 0x40, 0x41, 0x42 };
static unsigned char latchCommand[] = { 0x00, 0x04, 0x08 };
static unsigned char dataCommand[] = { 0x03, 0x07, 0x0B };

static int timerTicks = 0;
static int initialized = 0;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelSysTimerDriverRegisterDevice(void *timer)
{
  // Initializes the system timer itself.

  // Make sure that counter 0 is set to operate in mode 3 
  // (some systems erroneously use mode 2) with an initial value of 0
  return (kernelSysTimerDriverSetTimer(0, 3, 0));
}


int kernelSysTimerDriverInitialize(void)
{
  // Just registers our device driver.  Device initialization is done in
  // the 'register device' function

  // Reset the counter we use to count the number of timer 0 (system timer)
  // interrupts we've encountered
  timerTicks = 0;

  initialized = 1;

  // Register our driver
  return (kernelDriverRegister(sysTimerDriver, &defaultSysTimerDriver));
}


void kernelSysTimerDriverTick(void)
{
  // This updates the count of the system timer

  if (!initialized)
    return;

  // Add one to the timer 0 tick counter
  timerTicks += 1;
  return;
}


int kernelSysTimerDriverRead(void)
{
  if (!initialized)
    return (ERR_NOTINITIALIZED);

  // Returns the value of the system timer tick counter.
  return (timerTicks);
}

	
int kernelSysTimerDriverSetTimer(int counter, int mode, int count)
{
  // This function is used to select, set the mode and count of one
  // of the system timer counters

  int status = 0;
  unsigned char data, commandByte;

  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure the timer number is not greater than 2.  This driver only
  // supports timers 0 through 2 (since that's all most systems will have)
  if (counter > 2)
    return (status = ERR_BOUNDS);

  // Make sure the mode is legal
  if (mode > 5)
    return (status = ERR_BOUNDS);

  // Calculate the data command to use
  commandByte = dataCommand[counter];
  // Shift the commandByte left by 4 bits
  commandByte <<= 4;

  // Or the command with the mode (shifted left by one).  The
  // result is the formatted command byte we'll send to the timer
  commandByte |= ((unsigned char) mode << 1);

  // We can send the command to the general command port
  kernelProcessorOutPort8(0x43, commandByte);

  // The timer is now expecting us to send two bytes which represent
  // the initial count of the timer.  We will get this value from
  // the parameters.

  // Send low byte first, followed by the high byte to the data
  data = (unsigned char) (count & 0xFF);
  kernelProcessorOutPort8(portNumber[counter], data);
  data = (unsigned char) ((count >> 8) & 0xFF);
  kernelProcessorOutPort8(portNumber[counter], data);

  return (status = 0);
}
	
	
int kernelSysTimerDriverReadTimer(int counter)
{
  // This function is used to select and read one of the system 
  // timer counters

  int timerValue = 0;
  unsigned char data, commandByte;

  // Make sure the timer number is not greater than 2.  This driver only
  // supports timers 0 through 2 (since that's all most systems will have)
  if (counter > 2)
    return (timerValue = ERR_BOUNDS);

  // Before we can read the timer reliably, we must send a command
  // to cause it to latch the current value.  Calculate which latch 
  // command to use
  commandByte = latchCommand[counter];
  // Shift the commandByte left by 4 bits
  commandByte <<= 4;

  // We can send the command to the general command port
  kernelProcessorOutPort8(0x43, commandByte);

  // The counter will now be expecting us to read two bytes from
  // the applicable port.

  // Read the low byte first, followed by the high byte
  kernelProcessorInPort8(portNumber[counter], data);
  timerValue = data;
  kernelProcessorInPort8(portNumber[counter], data);
  timerValue |= (data << 8);

  return (timerValue);
}
