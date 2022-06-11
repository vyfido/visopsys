//
//  Visopsys
//  Copyright (C) 1998-2020 J. Andrew McLaughlin
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
//  progress.h
//

// This file contains definitions and structures for using and manipulating
// progress structures in Visopsys.  Progress structures can be used in some
// places to communicate the status of longer operations, for example long
// filesystem operations, between user programs and the kernel.

#ifndef _PROGRESS_H
#define _PROGRESS_H

#include <sys/lock.h>
#include <sys/types.h>

// The maximum length of status messages
#define PROGRESS_MAX_MESSAGELEN		239

// These are different kinds of possible responses when the operation
// needs user feedback
#define PROGRESS_RESPONSE_OK		0x0001
#define PROGRESS_RESPONSE_CANCEL	0x0002
#define PROGRESS_RESPONSE_YES		0x0004
#define PROGRESS_RESPONSE_NO		0x0008

typedef volatile struct {
	uquad_t numTotal;
	uquad_t numFinished;
	int percentFinished;
	char statusMessage[PROGRESS_MAX_MESSAGELEN + 1];
	char confirmMessage[PROGRESS_MAX_MESSAGELEN + 1];
	int needConfirm;
	int confirm;
	int error;
	int canCancel;
	int cancel;
	int complete;
	spinLock lock;

} progress;

#endif

