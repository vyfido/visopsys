//
//  Visopsys
//  Copyright (C) 1998-2021 J. Andrew McLaughlin
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
//  kernelUser.h
//

#ifndef _KERNELUSER_H
#define _KERNELUSER_H

#include <sys/user.h>

// Functions exported by kernelUser.c
int kernelUserInitialize(void);
int kernelUserAuthenticate(const char *, const char *);
int kernelUserLogin(const char *, const char *, int);
int kernelUserLogout(const char *);
int kernelUserExists(const char *);
int kernelUserGetNames(char *, unsigned);
int kernelUserAdd(const char *, const char *);
int kernelUserDelete(const char *);
int kernelUserSetPassword(const char *, const char *, const char *);
int kernelUserGetCurrentLoginPid(void);
int kernelUserGetCurrent(char *, unsigned);
int kernelUserGetPrivilege(const char *);
int kernelUserGetSessions(userSession *, int);
int kernelUserFileAdd(const char *, const char *, const char *);
int kernelUserFileDelete(const char *, const char *);
int kernelUserFileSetPassword(const char *, const char *, const char *,
	const char *);

#endif

