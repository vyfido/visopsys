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
//  kernelRtcDriver.c
//

// Driver for standard Real-Time Clock (RTC) chips.

#include "kernelDriverManagement.h" // Contains my prototypes
#include "kernelProcessorX86.h"


int kernelRtcDriverReadSeconds(void);
int kernelRtcDriverReadMinutes(void);
int kernelRtcDriverReadHours(void);
int kernelRtcDriverReadDayOfMonth(void);
int kernelRtcDriverReadMonth(void);
int kernelRtcDriverReadYear(void);

// Register numbers
#define SECSREG 0  // Seconds register
#define MINSREG 2  // Minutes register
#define HOURREG 4  // Hours register
// #define DOTWREG 6  // (unused) Day of week register
#define DOTMREG 7  // Day of month register
#define MNTHREG 8  // Month register
#define YEARREG 9  // Year register

static kernelRtcDriver defaultRtcDriver =
{
  kernelRtcDriverInitialize,
  NULL, // driverRegisterDevice
  kernelRtcDriverReadSeconds,
  kernelRtcDriverReadMinutes,
  kernelRtcDriverReadHours,
  kernelRtcDriverReadDayOfMonth,
  kernelRtcDriverReadMonth,
  kernelRtcDriverReadYear
};


static inline void waitReady(void)
{
  // This returns when the RTC is ready to be read or written.  Make sure
  // to disable interrupts before calling this routine.

  // Read the clock's "update in progress" bit from Register A.  If it is
  // set, do a loop until the clock has finished doing the update.  This is
  // so we know the data we're getting from the clock is reasonable.

  unsigned char data = 0x80;

  while (data & 0x80)
    {
      kernelProcessorOutPort8(0x70, 0x0A);
      kernelProcessorInPort8(0x71, data);
    }
}


static int readRegister(int regNum)
{
  // This takes a register number, does the necessary probe of the RTC,
  // and returns the data in EAX

  int interrupts = 0;
  unsigned char data = 0;

  // Suspend interrupts
  kernelProcessorSuspendInts(interrupts);

  // Wait until the clock is stable
  waitReady();

  // Now we have 244 us to read the data we want.  We'd better stop
  // talking and do it.  Disable NMI at the same time.
  data = (unsigned char) (regNum | 0x80);
  kernelProcessorOutPort8(0x70, data);

  // Now read the data
  kernelProcessorInPort8(0x71, data);

  // Reenable NMI
  kernelProcessorOutPort8(0x70, 0x00);

  // Restore interrupts
  kernelProcessorRestoreInts(interrupts);

  // The data is in BCD format.  Sucks.  Convert it to binary.
  return ((int) ((((data & 0xF0) >> 4) * 10) + (data & 0x0F)));
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelRtcDriverInitialize(void)
{
  // Register our driver
  return (kernelDriverRegister(rtcDriver, &defaultRtcDriver));
}


int kernelRtcDriverReadSeconds(void)
{
  // This function returns the seconds value from the Real-Time clock.
  return (readRegister(SECSREG));
}


int kernelRtcDriverReadMinutes(void)
{
  // This function returns the minutes value from the Real-Time clock.
  return (readRegister(MINSREG));
}


int kernelRtcDriverReadHours(void)
{
  // This function returns the hours value from the Real-Time clock.
  return (readRegister(HOURREG));
}


int kernelRtcDriverReadDayOfMonth(void)
{
  // This function returns the day-of-month value from the Real-Time clock.
  return (readRegister(DOTMREG));
}


int kernelRtcDriverReadMonth(void)
{
  // This function returns the month value from the Real-Time clock.
  return (readRegister(MNTHREG));
}


int kernelRtcDriverReadYear(void)
{
  // This function returns the year value from the Real-Time clock.
  return (readRegister(YEARREG));
}
