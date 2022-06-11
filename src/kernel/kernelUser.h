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
//  kernelUser.h
//

#if !defined(_KERNELUSER_H)

#define USER_PASSWORDFILE      "/system/password"
#define USER_MAX_NAMELENGTH    16
#define USER_MAX_PASSWDLENGTH  16

typedef struct
{
  char name[USER_MAX_NAMELENGTH];
  char passwd[USER_MAX_PASSWDLENGTH];
  int id;
  int privilege;
  int loginPid;

} kernelUser;

// Functions exported by kernelUser.c
int kernelUserInitialize(void);
int kernelUserAuthenticate(const char *, const char *);
int kernelUserLogin(const char *, const char *);
int kernelUserLogout(const char *);
int kernelUserGetNames(char *, unsigned);
int kernelUserAdd(const char *, const char *);
int kernelUserDelete(const char *);
int kernelUserSetPassword(const char *, const char *, const char *);
int kernelUserGetPrivilege(const char *);
int kernelUserGetPid(void);
int kernelUserSetPid(const char *, int);
int kernelUserFileAdd(const char *, const char *, const char *);
int kernelUserFileDelete(const char *, const char *);
int kernelUserFileSetPassword(const char *, const char *, const char *,
			      const char *);

#define _KERNELUSER_H
#endif
