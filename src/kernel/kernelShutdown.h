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
//  kernelShutdown.h
//

#if !defined(_KERNELSHUTDOWN_H)

#define SHUTDOWN_MSG1 "Shutting down Visopsys, please wait..."
#define SHUTDOWN_MSG2 "[ Wait for \"OK to power off\" message ]"
#define SHUTDOWN_MSG_REBOOT "Rebooting."
#define SHUTDOWN_MSG_POWER "OK to power off now."

// Functions exported by kernelShutdown.c
int kernelShutdown(int, int);
void kernelPanicOutput(const char *, const char *, int, const char *, ...);

#define kernelPanic(message, arg...) \
  kernelPanicOutput(__FILE__, __FUNCTION__, __LINE__, message, ##arg)

#define _KERNELSHUTDOWN_H
#endif
