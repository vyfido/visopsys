//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  kernelLock.h
//

// This header file contains definitions for the kernel's standard 
// locking facilities.

#if !defined(_KERNELLOCK_H)

// A lock structure
typedef volatile struct {

  int processId;
  char *filename;
  char *function;
  int line;

} kernelLock;

// Functions exported by kernelLock.c
int kernelLockGetComplex(const char *, const char *, int, kernelLock *);
#define kernelLockGet(lock) \
  kernelLockGetComplex(__FILE__, __FUNCTION__, __LINE__, lock)
int kernelLockRelease(kernelLock *);
int kernelLockVerify(kernelLock *);

#define _KERNELLOCK_H
#endif
