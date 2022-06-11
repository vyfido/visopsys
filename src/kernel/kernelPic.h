//
//  Visopsys
//  Copyright (C) 1998-2007 J. Andrew McLaughlin
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
//  kernelPic.h
//

#if !defined(_KERNELPIC_H)

#include "kernelDevice.h"

typedef struct {
  int (*driverEndOfInterrupt) (int);
  int (*driverMask) (int, int);
  int (*driverGetActive) (void);

} kernelPicOps;

// Functions exported by kernelPic.c
int kernelPicInitialize(kernelDevice *);
int kernelPicEndOfInterrupt(int);
int kernelPicMask(int, int);
int kernelPicGetActive(void);

#define _KERNELPIC_H
#endif
