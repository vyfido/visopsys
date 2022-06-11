// 
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
//  
//  This library is free software; you can redistribute it and/or modify it
//  under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2.1 of the License, or (at
//  your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
//  General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this library; if not, write to the Free Software Foundation,
//  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  progress.h
//

// This file contains definitions and structures for using and manipulating
// progress structures in Visopsys.  Progress structures can be used in some
// places to communicate the status of longer operations, for example long
// filesystem operations, between user programs and the kernel.

#if !defined(_PROGRESS_H)

// The maximum length of status messages
#define PROGRESS_MAX_MESSAGELEN  512

// These are different kinds of possible responses when the operation
// needs user feedback
#define PROGRESS_RESPONSE_OK      0x0001
#define PROGRESS_RESPONSE_CANCEL  0x0002
#define PROGRESS_RESPONSE_YES     0x0004
#define PROGRESS_RESPONSE_NO      0x0008

typedef struct {
  unsigned total;
  unsigned finished;
  int percentFinished;
  unsigned needResponse;
  unsigned response;
  char statusMessage[PROGRESS_MAX_MESSAGELEN];
  int canInterrupt;
  int interrupt;

} progress;

#define _PROGRESS_H
#endif
