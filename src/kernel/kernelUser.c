//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
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
//  kernelUser.c
//

// This file contains the routines designed for managing user access

#include "kernelUser.h"
#include "kernelDisk.h"
#include "kernelEncrypt.h"
#include "kernelError.h"
#include "kernelFile.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelShutdown.h"
#include "kernelVariableList.h"
#include <string.h>
#include <stdio.h>

static variableList systemUserList;
static kernelUser currentUser;
static int initialized = 0;


static int readPasswordFile(const char *fileName, variableList *userList)
{
  // Open and read the password file
  return (kernelConfigRead(fileName, userList));
}


static int writePasswordFile(const char *fileName, variableList *userList)
{
  // Writes the password data in memory out to the password file
  return (kernelConfigWrite(fileName, userList));
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
  status = kernelVariableListGet(&systemUserList, userName, fileHash, 33);
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


static int addUser(variableList *userList, const char *userName,
		   const char *password)
{
  int status = 0;
  char hash[33];

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
 
  return (status = 0);
}


static int deleteUser(variableList *userList, const char *userName)
{
  int status = 0;

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

  return (status = 0);
}


static int setPassword(variableList *userList, const char *userName,
		       const char *oldPass, const char *newPass)
{
  int status = 0;
  char newHash[33];

  if (kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR)
    {
      // Authenticate with the old password
      if (!authenticate(userName, oldPass))
	{
	  kernelError(kernel_error, "Authentication of old password failed");
	  return (status = ERR_PERMISSION);
	}
    }

  // Get the hash value of the new password
  status = hashString(newPass, newHash);
  if (status < 0)
    return (status);

  // Add it to the variable list
  status = kernelVariableListSet(userList, userName, newHash);
  if (status < 0)
    return (status);
  
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
  kernelDisk *rootDisk = NULL;
  char tmp[33];

  kernelMemClear(&systemUserList, sizeof(variableList));
  kernelMemClear(&currentUser, sizeof(kernelUser));

  // Try to read the password file.

  // Does it exist?
  if (kernelFileFind(USER_PASSWORDFILE, NULL) >= 0)
    {
      status = readPasswordFile(USER_PASSWORDFILE, &systemUserList);
      if (status < 0)
	// This is bad, but we don't want to fail the whole kernel startup
	// because of it.
	kernelError(kernel_warn, "Error reading password file %s",
		    USER_PASSWORDFILE);
    }

  // Make sure there's a list, and least one user
  if ((status < 0) || (systemUserList.numVariables <= 0))
    {
      // Create a variable list
      status = kernelVariableListCreate(&systemUserList);
      if (status < 0)
	return (status);
    }

  // Make sure there's a user called 'admin'
  if (kernelVariableListGet(&systemUserList, "admin", tmp, 33) < 0)
    {
      // Create a user entry for 'admin' with a blank password.
      status = addUser(&systemUserList, "admin", "");
      if (status < 0)
	return (status);

      // If the root filesystem is not read-only, write it out, so that
      // there's a valid password file next time.
      if ((kernelDiskGetBoot(tmp) >= 0) &&
	  ((rootDisk = kernelDiskGetByName(tmp)) != NULL) &&
	  !rootDisk->filesystem.readOnly)
	writePasswordFile(USER_PASSWORDFILE, &systemUserList);
    }

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
  status = kernelVariableListGet(&systemUserList, userName, tmpHash, 256);
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

  int names = 0;
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
  for (count = 0; ((names < systemUserList.numVariables) &&
		   (strlen(buffer) < bufferSize)); count ++)
    {
      strcat(bufferPointer, systemUserList.variables[count]);
      bufferPointer += (strlen(systemUserList.variables[count]) + 1);
      names++;
    }

  return (names);
}


int kernelUserAdd(const char *userName, const char *password)
{
  // Add a user to the list, with the associated password.  This can only be
  // done by a privileged user

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

  status = addUser(&systemUserList, userName, password);
  if (status < 0)
    return (status);
  
  status = writePasswordFile(USER_PASSWORDFILE, &systemUserList);
  return (status);
}


int kernelUserDelete(const char *userName)
{
  // Remove a user from the list.  This can only be done by a privileged
  // user

  int status = 0;

  // Check initialization
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);
  
  // Check params
  if (userName == NULL)
    {
      kernelError(kernel_error, "User name is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  status = deleteUser(&systemUserList, userName);
  if (status < 0)
    return (status);

  status = writePasswordFile(USER_PASSWORDFILE, &systemUserList);
  return (status);
}


int kernelUserSetPassword(const char *userName, const char *oldPass,
			  const char *newPass)
{
  int status = 0;

  // Check initialization
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);
  
  // Check params
  if ((userName == NULL) || (oldPass == NULL) || (newPass == NULL))
    {
      kernelError(kernel_error, "User name or password is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  status = setPassword(&systemUserList, userName, oldPass, newPass);
  if (status < 0)
    return (status);

  status = writePasswordFile(USER_PASSWORDFILE, &systemUserList);
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

  #ifdef PLUS
  extern int kernelIsLicensed;
  if (!kernelIsLicensed)
    kernelPanicOutput("License key", "", 0,
		      "The license key you entered is not valid");
  #endif

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


int kernelUserFileAdd(const char *passFile, const char *userName,
		      const char *password)
{
  // Add a user to the designated password file, with the given name and
  // password.  This can only be done by a privileged user.

  int status = 0;
  variableList userList;

  // Check initialization
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);
  
  // Check params
  if ((passFile == NULL) || (userName == NULL) || (password == NULL))
    {
      kernelError(kernel_error, "Password file, user name, or password is "
		  "NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Try to read the requested password file
  status = readPasswordFile(passFile, &userList);
  if (status < 0)
    return (status);

  status = addUser(&userList, userName, password);
  if (status < 0)
    {
      kernelMemoryRelease(userList.memory);
      return (status);
    }

  status = writePasswordFile(passFile, &userList);

  kernelMemoryRelease(userList.memory);

  return (status);
}


int kernelUserFileDelete(const char *passFile, const char *userName)
{
  // Remove a user from the designated password file.  This can only be done
  // by a privileged user

  int status = 0;
  variableList userList;

  // Check initialization
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);
  
  // Check params
  if ((passFile == NULL) || (userName == NULL))
    {
      kernelError(kernel_error, "Password file or user name is NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Try to read the requested password file
  status = readPasswordFile(passFile, &userList);
  if (status < 0)
    return (status);

  status = deleteUser(&userList, userName);
  if (status < 0)
    {
      kernelMemoryRelease(userList.memory);
      return (status);
    }

  status = writePasswordFile(passFile, &userList);

  kernelMemoryRelease(userList.memory);

  return (status);
}


int kernelUserFileSetPassword(const char *passFile, const char *userName,
			      const char *oldPass, const char *newPass)
{
  int status = 0;
  variableList userList;

  // Check initialization
  if (!initialized)
    return (status = ERR_NOTINITIALIZED);
  
  // Check params
  if ((passFile == NULL) || (userName == NULL) || (oldPass == NULL) ||
      (newPass == NULL))
    {
      kernelError(kernel_error, "Password file, user name, or password is "
		  "NULL");
      return (status = ERR_NULLPARAMETER);
    }

  // Try to read the requested password file
  status = readPasswordFile(passFile, &userList);
  if (status < 0)
    return (status);

  status = setPassword(&userList, userName, oldPass, newPass);
  if (status < 0)
    {
      kernelMemoryRelease(userList.memory);
      return (status);
    }

  status = writePasswordFile(passFile, &userList);

  kernelMemoryRelease(userList.memory);

  return (status);
}
