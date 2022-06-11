//
//  Visopsys
//  Copyright (C) 1998-2019 J. Andrew McLaughlin
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
//  user.h
//

// Definitions for user accounts

#ifndef _USER_H
#define _USER_H

#include <sys/apidefs.h>
#include <sys/network.h>
#include <sys/paths.h>

#define USER_PASSWORDFILE			PATH_SYSTEM "/password"
#define USER_PASSWORDFILE_BLANK		PATH_SYSTEM "/password.blank"
#define USER_MAX_NAMELENGTH			31
#define USER_MAX_PASSWDLENGTH		31
#define USER_ADMIN					"admin"

typedef enum {
	session_local, session_remote

} userSessionType;

typedef struct {
	userSessionType type;
	char name[USER_MAX_NAMELENGTH + 1];
	int privilege;
	int loginPid;
	union {
		struct {
			objectKey rootWindow;

		} local;

		struct {
			networkAddress address;
			int port;

		} remote;
	};

} userSession;

#endif

