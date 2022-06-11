//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  utsname.h
//

// This file is the Visopsys implementation of the standard <sys/utsname.h>
// file found in Unix.

#ifndef _UTSNAME_H
#define _UTSNAME_H

#include <sys/network.h>

#define UTSNAME_MAX_SYSNAME_LENGTH	15
#define UTSNAME_MAX_RELEASE_LENGTH	15
#define UTSNAME_MAX_VERSION_LENGTH	31
#define UTSNAME_MAX_MACHINE_LENGTH	15

struct utsname {
	char sysname[UTSNAME_MAX_SYSNAME_LENGTH + 1];
	char nodename[NETWORK_MAX_HOSTNAMELENGTH + 1];
	char release[UTSNAME_MAX_RELEASE_LENGTH + 1];
	char version[UTSNAME_MAX_VERSION_LENGTH + 1];
	char machine[UTSNAME_MAX_MACHINE_LENGTH + 1];
	char domainname[NETWORK_MAX_DOMAINNAMELENGTH + 1];
};

// Functions
int uname(struct utsname *);

#endif

