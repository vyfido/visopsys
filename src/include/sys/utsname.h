// 
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  utsname.h
//

// This file is the Visopsys implementation of the standard <sys/utsname.h>
// file found in Unix.

#if !defined(_UTSNAME_H)

#include <sys/network.h>

#define UTSNAME_MAX_SYSNAME_LENGTH  16
#define UTSNAME_MAX_RELEASE_LENGTH  16
#define UTSNAME_MAX_VERSION_LENGTH  32
#define UTSNAME_MAX_MACHINE_LENGTH  16

struct utsname {
  char sysname[UTSNAME_MAX_SYSNAME_LENGTH];
  char nodename[NETWORK_MAX_HOSTNAMELENGTH];
  char release[UTSNAME_MAX_RELEASE_LENGTH];
  char version[UTSNAME_MAX_VERSION_LENGTH];
  char machine[UTSNAME_MAX_MACHINE_LENGTH];
  char domainname[NETWORK_MAX_DOMAINNAMELENGTH];

};

#define _UTSNAME_H
#endif
