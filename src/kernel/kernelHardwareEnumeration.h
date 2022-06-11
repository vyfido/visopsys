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
//  kernelHardwareEnumeration.h
//

#if !defined(_KERNELHARDWAREENUMERATION_H)

#include "loaderInfo.h"
#include "kernelDiskFunctions.h"

extern loaderInfoStruct *systemInfo;

// Functions exported by kernelHardwareEnumeration.c

// These ones can be used by external routines to get information 
// about the hardware
int kernelHardwareEnumerate(loaderInfoStruct *);
kernelDiskObject *kernelGetFloppyDiskObject(int);
int kernelGetNumberLogicalHardDisks(void);
kernelDiskObject *kernelGetHardDiskObject(int);

#define _KERNELHARDWAREENUMERATION_H
#endif
