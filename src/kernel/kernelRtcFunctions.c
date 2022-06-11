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
//  kernelRtcFunctions.c
//

#include "kernelRtcFunctions.h"
#include "kernelError.h"
#include <sys/errors.h>


static kernelRtcObject *kernelRtc = NULL;
static int startSeconds, startMinutes, startHours, startDayOfMonth,
  startMonth, startYear;  // Set when the timer driver is initialized
static int initialized = 0;


static int checkObjectAndDriver(char *invokedBy)
{
  // This routine consists of a couple of things commonly done by the
  // other driver wrapper routines.  I've done this to minimize the amount
  // of duplicated code.  As its argument, it takes the name of the 
  // routine that called it so that any error messages can better reflect
  // what was really being done when the error occurred.

  int status = 0;

  // Make sure the RTC object isn't NULL (which could indicate that the
  // device has not been properly registered)
  if (kernelRtc == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The Real-Time clock object is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (kernelRtc->deviceDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // Return success
  return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelRtcRegisterDevice(kernelRtcObject *theRtc)
{
  // This routine will register a new RTC object.  It takes a 
  // kernelRtcObject structure and returns 0 if successful.  It returns 
  // negative if the device structure is NULL.

  int status = 0;

  if (theRtc == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The Real-Time clock object is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Alright.  We'll save the pointer to the device
  kernelRtc = theRtc;

  // Return success
  return (status = 0);
}


int kernelRtcInstallDriver(kernelRtcDeviceDriver *theDriver)
{
  // Attaches a driver object to an RTC object.  If the pointer to the
  // driver object is NULL, it returns negative.  Otherwise, returns zero.

  int status = 0;

  // Make sure the Rtc object isn't NULL
  if (kernelRtc == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The Real-Time clock object is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure the driver isn't NULL
  if (theDriver == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // Install the device driver
  kernelRtc->deviceDriver = theDriver;
  
  // Return success
  return (status = 0);
}


int kernelRtcInitialize(void)
{
  // This function initializes the RTC.  It pretty much just calls
  // the associated driver routines, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are
  // valid.

  int status = 0;

  // Check the RTC object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelRtc->deviceDriver->driverInitialize == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelRtc->deviceDriver->driverInitialize();

  if (status < 0)
    {
      // Make the error
      kernelError(kernel_error, "The Real-Time clock driver initialization "
		  "failed");
      return (status = ERR_NOTINITIALIZED);
    }

  // Now, register the starting time that the kernel was booted.
  startSeconds = kernelRtcReadSeconds();
  startMinutes = kernelRtcReadMinutes();
  startHours = kernelRtcReadHours();
  startDayOfMonth = kernelRtcReadDayOfMonth();
  startMonth = kernelRtcReadMonth();
  startYear = kernelRtcReadYear();

  // We are initialized
  initialized = 1;

  // Return success
  return (status = 0);
}


int kernelRtcReadSeconds(void)
{
  // This is a generic routine for invoking the corresponding routine
  // in a Real-Time Clock driver.  It takes no arguments and returns the
  // result from the device driver call

  int status = 0;

  // Check the RTC object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelRtc->deviceDriver->driverReadSeconds == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelRtc->deviceDriver->driverReadSeconds();

  return (status);
}


int kernelRtcReadMinutes(void)
{
  // This is a generic routine for invoking the corresponding routine
  // in a Real-Time Clock driver.  It takes no arguments and returns the
  // result from the device driver call

  int status = 0;

  // Check the RTC object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver initialize routine has been 
  // installed
  if (kernelRtc->deviceDriver->driverReadMinutes == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelRtc->deviceDriver->driverReadMinutes();

  return (status);
}


int kernelRtcReadHours(void)
{
  // This is a generic routine for invoking the corresponding routine
  // in a Real-Time Clock driver.  It takes no arguments and returns the
  // result from the device driver call

  int status = 0;

  // Check the RTC object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver read hours routine has been 
  // installed
  if (kernelRtc->deviceDriver->driverReadHours == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelRtc->deviceDriver->driverReadHours();

  return (status);
}


int kernelRtcReadDayOfWeek(void)
{
  // This is a generic routine for invoking the corresponding routine
  // in a Real-Time Clock driver.  It takes no arguments and returns the
  // result from the device driver call

  int status = 0;

  // Check the RTC object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver read day-of-week routine has been 
  // installed
  if (kernelRtc->deviceDriver->driverReadDayOfWeek == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelRtc->deviceDriver->driverReadDayOfWeek();

  return (status);
}


int kernelRtcReadDayOfMonth(void)
{
  // This is a generic routine for invoking the corresponding routine
  // in a Real-Time Clock driver.  It takes no arguments and returns the
  // result from the device driver call

  int status = 0;

  // Check the RTC object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver read day-of-month routine has been 
  // installed
  if (kernelRtc->deviceDriver->driverReadDayOfMonth == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelRtc->deviceDriver->driverReadDayOfMonth();

  return (status);
}


int kernelRtcReadMonth(void)
{
  // This is a generic routine for invoking the corresponding routine
  // in a Real-Time Clock driver.  It takes no arguments and returns the
  // result from the device driver call

  int status = 0;

  // Check the RTC object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver read month routine has been 
  // installed
  if (kernelRtc->deviceDriver->driverReadMonth == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelRtc->deviceDriver->driverReadMonth();

  return (status);
}


int kernelRtcReadYear(void)
{
  // This is a generic routine for invoking the corresponding routine
  // in a Real-Time Clock driver.  It takes no arguments and returns the
  // result from the device driver call

  int status = 0;

  // Check the RTC object and device driver before proceeding
  status = checkObjectAndDriver(__FUNCTION__);
  if (status < 0)
    // Something went wrong, so we can't continue
    return (status);

  // Now make sure the device driver read year routine has been 
  // installed
  if (kernelRtc->deviceDriver->driverReadYear == NULL)
    {
      // Make the error
      kernelError(kernel_error, "The device driver routine is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Ok, now we can call the routine.
  status = kernelRtc->deviceDriver->driverReadYear();

  // Y2K COMPLIANCE SECTION :-)

  // Here is where we put in the MOTHER Y2K BUG.  OK, just
  // kidding.  Here is where we avoid the problem, right at the
  // kernel level.  We have to determine what size of value we are 
  // getting from the driver.  If we are using the default driver, 
  // it returns the same two-digit number that the hardware returns.  
  // In this case (or any other where we get a value < 100) we add the 
  // century to the value.  We make an inference based on the following:
  // If the year is less than 80, i.e. 1980, we assume we are in
  // the 21st century and add 2000.  Otherwise, we add 1900.

  if (status < 100)
    {
      if (status < 80)
	return (status + 2000);

      else
	return (status + 1900);
    }

  // Must be using a different driver that returns a 4-digit year.
  // Good, I suppose.  Why would someone rewrite this driver?  Dunno.
  // Power to the people, anyway.
  else if (status >= 1980) 
    return (status);

  // We have some other gibbled value.  Return an error code.
  else 
    return (status = ERR_BADDATA);
}


unsigned kernelRtcUptimeSeconds(void)
{
  // This returns the number of seconds since the RTC driver was
  // initialized.

  unsigned upSeconds = 0;

  upSeconds += (kernelRtcReadSeconds() - startSeconds);
  upSeconds += ((kernelRtcReadMinutes() - startMinutes) * 60);
  upSeconds += ((kernelRtcReadHours() - startHours) * 60 * 60);
  upSeconds += ((kernelRtcReadDayOfMonth() - startDayOfMonth) * 24 * 60 * 60);
  upSeconds += ((kernelRtcReadMonth() - startMonth) * 31 * 24 * 60 * 60);
  upSeconds += ((kernelRtcReadYear() - startYear) * 12 * 31 * 24 * 60 * 60);

  return (upSeconds);
}


unsigned kernelRtcPackedDate(void)
{
  // This function takes a pointer to an unsigned and places the
  // current date in the variable, in a packed format.  It returns 0 on
  // success, negative on error

  // The format for dates is as follows:
  // [year (n bits)] [month (4 bits)] [day (5 bits)]

  unsigned temp = 0;
  unsigned returnedDate = 0;

  if (!initialized)
    return (returnedDate = 0);

  // The RTC function for reading the day of the month will return a value 
  // between 1 and 31 inclusive.
  temp = kernelRtcReadDayOfMonth();
  // Day is in the least-significant 5 bits.
  temp = (temp & 0x0000001F);
  returnedDate = temp;

  // The RTC function for reading the month will return a value 
  // between 1 and 12 inclusive.
  temp = kernelRtcReadMonth();
  // Month is 4 bits in places 5-8
  temp = (temp << 5);
  temp = (temp & 0x000001E0);
  returnedDate |= temp;

  // The year
  temp = kernelRtcReadYear();
  // Year is n bits in places 9->
  temp = (temp << 9);
  temp = (temp & 0xFFFFFE00);
  returnedDate |= temp;

  return (returnedDate);
}


unsigned kernelRtcPackedTime(void)
{
  // This function takes a pointer to an unsigned and places the
  // current time in the variable, in a packed format.  It returns 0 on
  // success, negative on error
  // The format for times is as follows:
  // [hours (5 bits)] [minutes (6 bits)] [seconds (6 bits)]

  unsigned temp = 0;
  unsigned returnedTime = 0;

  if (!initialized)
    return (returnedTime = 0);

  // The RTC function for reading seconds will pass us a value between
  // 0 and 59 inclusive.  
  temp = kernelRtcReadSeconds();
  // Seconds are in the least-significant 6 bits.
  temp = (temp & 0x0000003F);
  returnedTime = temp;

  // The RTC function for reading minutes will pass us a value between
  // 0 and 59 inclusive.
  temp = kernelRtcReadMinutes();
  // Minutes are six bits in places 6-11
  temp = (temp << 6);
  temp = (temp & 0x00000FC0);
  returnedTime |= temp;

  // The RTC function for reading hours will pass us a value between
  // 0 and 23 inclusive.
  temp = kernelRtcReadHours();
  // Hours are five bits in places 12-16
  temp = (temp << 12);
  temp = (temp & 0x0003F000); 
  returnedTime |= temp;

  return (returnedTime);
}


int kernelRtcDateTime(struct tm *timeStruct)
{
  // This function will fill out a 'tm' structure according to the current
  // date and time.  This function is just a convenience, as all of the
  // functionality here could be reproduced with other calls
  
  int status = 0;
  
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);

  // Make sure our time struct isn't NULL
  if (timeStruct == NULL)
    return (status = ERR_NULLPARAMETER);

  timeStruct->tm_sec = kernelRtcReadSeconds();
  timeStruct->tm_min = kernelRtcReadMinutes();
  timeStruct->tm_hour = kernelRtcReadHours();
  timeStruct->tm_mday = kernelRtcReadDayOfMonth();
  timeStruct->tm_mon = (kernelRtcReadMonth() - 1);
  timeStruct->tm_year = kernelRtcReadYear();
  timeStruct->tm_wday = (kernelRtcReadDayOfWeek() - 1);
  timeStruct->tm_yday = 0;  // unimplemented
  timeStruct->tm_isdst = 0; // We don't know anything about DST yet

  // Return success
  return (status = 0);
}
