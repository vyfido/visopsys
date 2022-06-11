//
//  Visopsys
//  Copyright (C) 1998-2013 J. Andrew McLaughlin
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
//  kernelSerial.h
//

#if !defined(_KERNELSERIAL_H)

// Structures for the serial port

typedef struct
{
  int (*driverConfigurePort) (int, int, int, int, int);

} kernelSerialDriver;

typedef struct {
  int portNumber;
  unsigned baseAddress;
  int baudRate;
  int dataBits;
  int stopBits;
  int parity;
  kernelSerialDriver *driver;

} kernelSerialPort;

// Functions exported from kernelSerial.c
int kernelSerialRegisterDevice(kernelSerialPort *);
int kernelSerialInitialize(void);
int kernelSerialConfigurePort(int, int, int, int, int);

#define _KERNELSERIAL_H
#endif
