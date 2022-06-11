//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  kernelUser.c
//

// This file contains the routines designed for managing user access

#include "kernelUser.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelMiscFunctions.h"
#include "kernelError.h"
#include <string.h>
#include <sys/errors.h>

static kernelUser currentUser;


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
// Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelUserInitialize(void)
{
  // Takes care of stuff we need to do before we can start processing things
  // about users
  
  kernelMemClear(&currentUser, sizeof(kernelUser));

  return (0);
}


int kernelUserLogin(const char *userName, int loginPid)
{
  // Logs a user in
    
  // This is just a kludge for now.  'admin' is supervisor privilege,
  // everyone else is user privilege

  int status = 0;
  
  // Check params
  if (userName == NULL)
    return (status = ERR_NULLPARAMETER);

  strncpy(currentUser.name, userName, USER_MAX_NAMELENGTH);
  
  if (!strncmp(userName, "admin", USER_MAX_NAMELENGTH))
    {
      currentUser.id = 1;
      currentUser.privilege = PRIVILEGE_SUPERVISOR;
    }
  else
    {
      currentUser.id = 2;
      currentUser.privilege = PRIVILEGE_USER;
    }

  currentUser.loginPid = loginPid;

  return (status = 0);
}


int kernelUserLogout(const char *userName)
{
  // Logs a user out

  // This is just a kludge for now.  'admin' is supervisor privilege,
  // everyone else is user privilege

  int status = 0;
  
  // Check params.  If userName is NULL, we do the current user.
  if (userName == NULL)
    userName = currentUser.name;

  // Is this a logged-in user?
  else if (strncmp(currentUser.name, userName, USER_MAX_NAMELENGTH))
    return (status = ERR_NOSUCHENTRY);

  // Kill the user's login process.  The termination of the login process
  // is what effectively logs out the user.  This will only succeed if the
  // current process is owned by the user, or if the current process is
  // supervisor privilege
  if (kernelMultitaskerProcessIsAlive(currentUser.loginPid))
    status = kernelMultitaskerKillProcess(currentUser.loginPid, 0);

  // Clear the user structure
  kernelMemClear(&currentUser, sizeof(kernelUser));

  return (status);
}


int kernelUserGetPrivilege(const char *userName)
{
  // Returns the default privilege level for the supplied user name

  // This is just a kludge for now.  'admin' is supervisor privilege,
  // everyone else is user privilege

  if (!strcmp(userName, "admin"))
    return (PRIVILEGE_SUPERVISOR);
  else
    return (PRIVILEGE_USER);
}


int kernelUserGetPid(void)
{
  // Returns the login process id for the current user

  // This is just a kludge for now.
  return (currentUser.loginPid);
}
