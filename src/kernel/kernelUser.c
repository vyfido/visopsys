//
//  Visopsys
//  Copyright (C) 1998-2005 J. Andrew McLaughlin
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
#include "kernelFile.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelMiscFunctions.h"
#include "kernelEncrypt.h"
#include "kernelMemoryManager.h"
#include "kernelError.h"
#include <string.h>
#include <stdio.h>
#include <sys/errors.h>

static variableList *userList = NULL;
static kernelUser currentUser;
static int initialized = 0;


static void readPasswordFile(void)
{
  // Open and read the password file

  int status = 0;
  file tmpFile;
  variableList *tmpUserList = NULL;
  unsigned count;

  // Check whether the password file exists
  status = kernelFileFind(USER_PASSWORDFILE, &tmpFile);
  if (status < 0)
    {
      // Try to use the blank one instead
      status = kernelFileFind(USER_PASSWORDFILE ".blank", &tmpFile);
      if (status >= 0)
	{
	  tmpUserList = kernelConfigurationReader(USER_PASSWORDFILE ".blank");

	  if(!kernelFilesystemGet("/")->readOnly)
	    // Try to make a copy for next time
	    kernelFileCopy(USER_PASSWORDFILE ".blank", USER_PASSWORDFILE);
	}
    }
  else
    tmpUserList = kernelConfigurationReader(USER_PASSWORDFILE);

  if (tmpUserList == NULL)
    // The password file doesn't exist, and neither does the blank one.
    // Create a blank variable list.
    tmpUserList = kernelVariableListCreate(1, 1, "password data");

  // Now create a variable list big enough to hold our maximum number of
  // user entries
  userList = kernelVariableListCreate(USER_MAXUSERS, (USER_MAXUSERS * 80),
				      "password data");
  if (userList == NULL)
    {
      kernelMemoryRelease(tmpUserList);
      return;
    }

  // Now transfer all the data from the temporary list to the permanent one
  for (count = 0; count < tmpUserList->numVariables; count ++)
    {
      status = kernelVariableListSet(userList, tmpUserList->variables[count],
				     tmpUserList->values[count]);
      if (status < 0)
	break;
    }

  kernelMemoryRelease(tmpUserList);
  return;
}


static int writePasswordFile(void)
{
  // Writes the password data in memory out to the password file
  return (kernelConfigurationWriter(USER_PASSWORDFILE, userList));
}


static int hashString(const char *plain, char *hash)
{
  // Turns a plain text string into a hash

  int status = 0;
  char hashValue[16];
  char byte[3];
  int count;

  // Get the MD5 hash of the supplied string
  status = kernelEncryptMD5(plain, hashValue);
  if (status < 0)
    return (status = 0);

  // Turn it into a string
  hash[0] = '\0';
  for (count = 0; count < 16; count ++)
    {
      sprintf(byte, "%02x", (unsigned char) hashValue[count]);
      strcat(hash, byte);
    }

  return (status = 0);
}


static int authenticate(const char *userName, const char *password)
{
  int status = 0;
  char fileHash[33];
  char testHash[33];

  // Get the hash of the real password
  status = kernelVariableListGet(userList, userName, fileHash, 33);
  if (status < 0)
    return (status = 0);

  status = hashString(password, testHash);
  if (status < 0)
    return (status = 0);

  if (!strcmp(testHash, fileHash))
    return (status = 1);
  else
    return (status = 0);
}


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
  
  int status = 0;

  kernelMemClear(&currentUser, sizeof(kernelUser));

  // Try to read the password file.
  readPasswordFile();

  initialized = 1;
  return (status = 0);
}


int kernelUserAuthenticate(const char *userName, const char *password)
{
  // Attempt to authenticate the user name with the password supplied.

  int status = 0;
  char tmpHash[256];

  // Check initialization
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);
  
  // Check params
  if ((userName == NULL) || (password == NULL))
    {
      kernelError(kernel_error, "User name or password is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Check to make sure the user exists
  status = kernelVariableListGet(userList, userName, tmpHash, 256);
  if (status < 0)
    return (status = ERR_NOSUCHUSER);

  // Authenticate
  if (!authenticate(userName, password))
    return (status = ERR_PERMISSION);

  // Success
  return (status = 0);
}


int kernelUserLogin(const char *userName, const char *password)
{
  // Logs a user in
    
  // This is just a kludge for now.  'admin' is supervisor privilege,
  // everyone else is user privilege

  int status = 0;

  // Check initialization
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);
  
  // Check params
  if ((userName == NULL) || (password == NULL))
    {
      kernelError(kernel_error, "User name or password is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Authenticate
  if (!authenticate(userName, password))
    return (status = ERR_PERMISSION);

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

  return (status = 0);
}


int kernelUserLogout(const char *userName)
{
  // Logs a user out

  // This is just a kludge for now.  'admin' is supervisor privilege,
  // everyone else is user privilege

  int status = 0;

  // Check initialization
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);
  
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


int kernelUserGetNames(char *buffer, unsigned bufferSize)
{
  // Returns all the user names (up to bufferSize bytes) as NULL-separated
  // strings, and returns the number copied.

  unsigned names = 0;
  char *bufferPointer = NULL;
  int count;

  // Check initialization
  if (!initialized)
    return (names = ERR_NOTINITIALIZED);
  
  // Check params
  if (buffer == NULL)
    {
      kernelError(kernel_error, "User name buffer is NULL");
      return (names = ERR_NULLPARAMETER);
    }

  bufferPointer = buffer;
  bufferPointer[0] = '\0';

  // Loop through the list, appending names and newlines
  for (count = 0; ((names < userList->numVariables) &&
		   (strlen(buffer) < bufferSize)); count ++)
    {
      strcat(bufferPointer, userList->variables[count]);
      bufferPointer += (strlen(userList->variables[count]) + 1);
      names++;
    }

  return (names);
}


int kernelUserAdd(const char *userName, const char *password)
{
  // Add a user to the list, with the associated password

  int status = 0;
  char hash[33];

  // Check initialization
  if (!initialized)
    return (ERR_NOTINITIALIZED);
  
  // Check params
  if ((userName == NULL) || (password == NULL))
    {
      kernelError(kernel_error, "User name or password is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Check permissions
  if (kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR)
    {
      kernelError(kernel_error, "Adding a user requires supervisor priviege");
      return (status = ERR_PERMISSION);
    }

  // Get the hash value of the supplied password
  status = hashString(password, hash);
  if (status < 0)
    return (status);

  // Add it to the variable list
  status = kernelVariableListSet(userList, userName, hash);
  if (status < 0)
    return (status);
  
  status = writePasswordFile();
  return (status);
}


int kernelUserDelete(const char *userName)
{
  // Remove a user from the list.  This can only be done by a privileged
  // user

  int status = 0;

  // Check initialization
  if (!initialized)
    return (ERR_NOTINITIALIZED);
  
  // Check params
  if (userName == NULL)
    {
      kernelError(kernel_error, "User name is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Check permissions
  if (kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR)
    {
      kernelError(kernel_error, "Deleting a user requires supervisor "
		  "priviege");
      return (status = ERR_PERMISSION);
    }

  // Don't allow the user to delete the last user.  This is dangerous.
  if (userList->numVariables == 1)
    {
      kernelError(kernel_error, "Can't delete the last user account");
      return (status = ERR_BOUNDS);
    }

  status = kernelVariableListUnset(userList, userName);
  if (status < 0)
    return (status);
  
  status = writePasswordFile();
  return (status);
}


int kernelUserSetPassword(const char *userName, const char *oldPass,
			  const char *newPass)
{
  int status = 0;
  char newHash[33];

  // Check initialization
  if (!initialized)
    return (ERR_NOTINITIALIZED);
  
  // Check params
  if ((userName == NULL) || (oldPass == NULL) || (newPass == NULL))
    {
      kernelError(kernel_error, "User name or password is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  if (kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR)
    {
      // Authenticate with the old password
      if (!authenticate(userName, oldPass))
	return (status = ERR_PERMISSION);
    }

  // Get the hash value of the new password
  status = hashString(newPass, newHash);
  if (status < 0)
    return (status);

  // Add it to the variable list
  status = kernelVariableListSet(userList, userName, newHash);
  if (status < 0)
    return (status);
  
  status = writePasswordFile();
  return (status);
}


int kernelUserGetPrivilege(const char *userName)
{
  // Returns the default privilege level for the supplied user name

  // This is just a kludge for now.  'admin' is supervisor privilege,
  // everyone else is user privilege

  // Check initialization
  if (!initialized)
    return (ERR_NOTINITIALIZED);
  
  // Check params
  if (userName == NULL)
    {
      kernelError(kernel_error, "User name is NULL");
      return (ERR_NULLPARAMETER);
    }

  if (!strcmp(userName, "admin"))
    return (PRIVILEGE_SUPERVISOR);
  else
    return (PRIVILEGE_USER);
}


int kernelUserGetPid(void)
{
  // Returns the login process id for the current user

  // Check initialization
  if (!initialized)
    return (ERR_NOTINITIALIZED);
  
  // This is just a kludge for now.
  return (currentUser.loginPid);
}


int kernelUserSetPid(const char *userName, int loginPid)
{
  // Set the login PID for the named user.  This is just a kludge for now.

  int status = 0;

  // Check initialization
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);
  
  // Check params
  if (userName == NULL)
    {
      kernelError(kernel_error, "Login name is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  currentUser.loginPid = loginPid;

  return (status = 0);
}
