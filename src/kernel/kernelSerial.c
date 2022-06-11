//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelSerial.c
//

// This file contains the C functions belonging to the kernel's serial port
// services

/*
#include "kernelSerial.h"
#include "kernelParameters.h"
#include "kernelError.h"
#include <string.h>

static volatile kernelSerialPort *serialPort[MAX_SERIAL_PORTS];
static volatile int numSerialPorts = 0;
static int initialized = 0;


static inline kernelSerialPort *findPort(int portNumber)
{
  kernelSerialPort *port = NULL;
  int count;

  for (count = 0; count < numSerialPorts; count ++)
    if (serialPort[count]->portNumber == portNumber)
      break;

  return (port);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelSerialRegisterDevice(kernelSerialPort *theSerial)
{
  // This routine will register a new serial port.  It takes a 
  // kernelSerialPort structure and returns 0 if successful.

  int status = 0;

  if (theSerial == NULL)
    {
      kernelError(kernel_error, "The serial port is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Make sure that the device has a non-NULL driver
  if (theSerial->driver == NULL)
    {
      kernelError(kernel_error, "The serial driver is NULL");
      return (status = ERR_NOSUCHDRIVER);
    }

  // If the driver has a 'register device' function, call it
  if (theSerial->driver->driverRegisterDevice)
    status = theSerial->driver->driverRegisterDevice(theSerial);

  // Alright.  We'll save the pointer to the device
  serialPort[numSerialPorts++] = theSerial;

  // Return success
  return (status);
}


int kernelSerialInitialize(void)
{
  // This function initializes the serial ports.  It pretty much just calls
  // the associated driver routines, but it also does some checks and
  // whatnot to make sure that the device, driver, and driver routines are
  // valid.

  int status = 0;
  volatile kernelSerialPort *port = NULL;
  int count;

  // Are any serial ports registered?
  if (numSerialPorts == 0)
    {
      kernelError(kernel_error, "No serial ports have been registered");
      return (status = ERR_NOSUCHENTRY);
    }

  // Loop through all of the serial ports, and call the driver initialization
  // routine for each
  for (count = 0; count < numSerialPorts; count ++)
    {
      // Get a serial port
      port = serialPort[count];
      if (port == NULL)
	{
	  kernelError(kernel_error, "The serial port is NULL");
	  return (status = ERR_NULLPARAMETER);
	}
    }

  // We are initialized
  initialized = 1;

  // Return success
  return (status = 0);
}


int kernelSerialConfigurePort(int portNumber, int baudRate, int dataBits,
			      int stopBits, int parity)
{
  // Configures a serial port for use

  int status = 0;
  kernelSerialPort *port = NULL;

  // Find the requested serial port
  port = findPort(portNumber);
  if (port == NULL)
    {
      kernelError(kernel_error, "The serial port is NULL");
      return (status = ERR_NOSUCHENTRY);
    }  

  // Make sure the configuration routine exists
  if (port->deviceDriver->driverConfigurePort == NULL)
    {
      kernelError(kernel_error, "The device driver funtion is NULL");
      return (status = ERR_NOSUCHFUNCTION);
    }

  // Call the driver function
  status = port->deviceDriver
    ->driverConfigurePort(port->baseAddress, baudRate, dataBits, stopBits,
			  parity);

  if (status < 0)
    {
      kernelError(kernel_error, "Unable to configure serial port");
      return (status);
    }					   

  // Save the data we've been passed
  port->baudRate = baudRate;
  port->dataBits = dataBits;
  port->stopBits = stopBits;
  port->parity = parity;

  // Done
  return (status = 0);
}
*/
