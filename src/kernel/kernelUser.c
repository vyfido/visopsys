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
//  kernelUser.c
//

// This file contains the functions designed for managing user access

#include "kernelUser.h"
#include "kernelCrypt.h"
#include "kernelDisk.h"
#include "kernelEnvironment.h"
#include "kernelError.h"
#include "kernelFile.h"
#include "kernelKeyboard.h"
#include "kernelLog.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelMultitasker.h"
#include "kernelParameters.h"
#include "kernelShutdown.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/env.h>
#include <sys/kernconf.h>
#include <sys/paths.h>
#include <sys/vis.h>

static variableList systemUserList;
static userSession currentUser;
static int systemDirWritable = 0;
static int initialized = 0;


static inline int readPasswordFile(const char *fileName,
	variableList *userList)
{
	return (kernelConfigReadSystem(fileName, userList));
}


static inline int writePasswordFile(const char *fileName,
	variableList *userList)
{
	return (kernelConfigWrite(fileName, userList));
}


static int userExists(const char *userName, variableList *userList)
{
	// Returns 1 if the user exists in the supplied user list

	if (variableListGet(userList, userName))
		return (1);
	else
		return (0);
}


static int hashString(const char *plain, char *hash)
{
	// Turns a plain text string into a hash

	int status = 0;
	unsigned char hashValue[CRYPT_HASH_SHA256_BYTES];
	char byte[3];
	int count;

	// Get the SHA256 hash of the supplied string
	status = kernelCryptHashSha256((unsigned char *) plain, strlen(plain),
		hashValue, 1 /* finalize */, strlen(plain));
	if (status < 0)
		return (status = 0);

	// Turn it into a string
	hash[0] = '\0';
	for (count = 0; count < CRYPT_HASH_SHA256_BYTES; count ++)
	{
		sprintf(byte, "%02x", (unsigned char) hashValue[count]);
		strcat(hash, byte);
	}

	return (status = 0);
}


static int addUser(variableList *userList, const char *userName,
	const char *password)
{
	int status = 0;
	char hash[65];

	// Check permissions
	if ((userList == &systemUserList) &&
		(kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR))
	{
		kernelError(kernel_error, "Adding a user requires supervisor "
			"priviege");
		return (status = ERR_PERMISSION);
	}

	// Get the hash value of the supplied password
	status = hashString(password, hash);
	if (status < 0)
		return (status);

	// Add it to the variable list
	status = variableListSet(userList, userName, hash);
	if (status < 0)
		return (status);

	return (status = 0);
}


static int deleteUser(variableList *userList, const char *userName)
{
	int status = 0;

	// Check permissions
	if ((userList == &systemUserList) &&
		(kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR))
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

	status = variableListUnset(userList, userName);
	if (status < 0)
		return (status);

	return (status = 0);
}


static int authenticate(const char *userName, const char *password)
{
	int status = 0;
	const char *fileHash = NULL;
	char testHash[65];

	// Get the hash of the real password
	fileHash = variableListGet(&systemUserList, userName);
	if (!fileHash)
		return (status = 0);

	status = hashString(password, testHash);
	if (status < 0)
		return (status = 0);

	if (!strcmp(testHash, fileHash))
		return (status = 1);
	else
		return (status = 0);
}


static int setPassword(variableList *userList, const char *userName,
	const char *oldPass, const char *newPass)
{
	int status = 0;
	char newHash[65];

	// Check permissions
	if ((userList == &systemUserList) &&
		(kernelCurrentProcess->privilege != PRIVILEGE_SUPERVISOR))
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
	status = variableListSet(userList, userName, newHash);
	if (status < 0)
		return (status);

	return (status = 0);
}


static int isSystemPasswordFile(const char *fileName)
{
	// Returns 1 if the fileName specifies the system password file.  Does a
	// fixup first, to make sure we know the canonical pathname.

	int status = 0;
	char fixedName[MAX_PATH_NAME_LENGTH + 1];

	status = kernelFileFixupPath(fileName, fixedName);
	if (status < 0)
		return (status);

	if (!strncmp(fileName, USER_PASSWORDFILE, strlen(USER_PASSWORDFILE)))
		return (1);
	else
		return (0);
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
	kernelFileEntry *systemDir = NULL;

	memset(&systemUserList, 0, sizeof(variableList));
	memset(&currentUser, 0, sizeof(userSession));

	// Try to read the password file.

	// Does it exist?
	if (kernelFileFind(USER_PASSWORDFILE, NULL) >= 0)
	{
		status = readPasswordFile(USER_PASSWORDFILE, &systemUserList);
		if (status < 0)
		{
			// This is bad, but we don't want to fail the whole kernel startup
			// because of it.
			kernelError(kernel_warn, "Error reading password file %s",
				USER_PASSWORDFILE);
		}
	}

	// Make sure there's a list, and least one user
	if ((status < 0) || (systemUserList.numVariables <= 0))
	{
		// Create a variable list
		status = variableListCreateSystem(&systemUserList);
		if (status < 0)
			return (status);
	}

	// Figure out whether the system directory is on a writeable filesystem.
	if ((systemDir = kernelFileLookup(PATH_SYSTEM)) &&
		!systemDir->disk->filesystem.readOnly)
	{
		systemDirWritable = 1;
	}

	// Make sure there's a user called 'admin'
	if (!userExists(USER_ADMIN, &systemUserList))
	{
		// Create a user entry for 'admin' with a blank password.
		status = addUser(&systemUserList, USER_ADMIN, "");
		if (status < 0)
			return (status);

		// If the filesystem of the password file is not read-only, write it
		// out, so that there's a valid password file next time.
		if (systemDirWritable)
			writePasswordFile(USER_PASSWORDFILE, &systemUserList);
	}

	initialized = 1;
	return (status = 0);
}


int kernelUserAuthenticate(const char *userName, const char *password)
{
	// Attempt to authenticate the user name with the password supplied.

	int status = 0;

	// Check initialization
	if (!initialized)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!userName || !password)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check to make sure the user exists
	if (!userExists(userName, &systemUserList))
		return (status = ERR_NOSUCHUSER);

	// Authenticate
	if (!authenticate(userName, password))
		return (status = ERR_PERMISSION);

	// Success
	return (status = 0);
}


int kernelUserLogin(const char *userName, const char *password, int loginPid)
{
	// Logs a user in

	int status = 0;
	char homeDir[MAX_PATH_LENGTH + 1];
	char keyMapName[KEYMAP_NAMELEN + 1];
	char keyMapFile[MAX_PATH_NAME_LENGTH + 1];

	// Check initialization
	if (!initialized)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!userName || !password)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check to make sure the user exists
	if (!userExists(userName, &systemUserList))
		return (status = ERR_NOSUCHUSER);

	// Authenticate
	if (!authenticate(userName, password))
		return (status = ERR_PERMISSION);

	// Log the user in

	currentUser.type = session_local;
	strncpy(currentUser.name, userName, USER_MAX_NAMELENGTH);

	// This is just a kludge for now.  'admin' is supervisor privilege,
	// everyone else is user privilege
	if (!strncmp(userName, USER_ADMIN, USER_MAX_NAMELENGTH))
		currentUser.privilege = PRIVILEGE_SUPERVISOR;
	else
		currentUser.privilege = PRIVILEGE_USER;

	currentUser.loginPid = loginPid;

	// Set the user session for the login process
	kernelMultitaskerSetProcessUserSession(currentUser.loginPid,
		&currentUser);

	// Determine the user's home directory
	if (!strncmp(userName, USER_ADMIN, USER_MAX_NAMELENGTH))
		strcpy(homeDir, "/");
	else
		snprintf(homeDir, MAX_PATH_LENGTH, PATH_USERS_HOME, userName);

	// Set the user's home directory as the current directory
	kernelMultitaskerSetProcessCurrentDirectory(loginPid, homeDir);

	// Set the login name as an environment variable
	kernelEnvironmentProcessSet(loginPid, ENV_USER, userName);

	// Set the user home directory as an environment variable
	kernelEnvironmentProcessSet(loginPid, ENV_HOME, homeDir);

	// Load the rest of the environment variables
	kernelEnvironmentLoad(userName, loginPid);

	// If the user has the ENV_KEYMAP variable set, set the current keymap
	status = kernelEnvironmentProcessGet(loginPid, ENV_KEYMAP, keyMapName,
		KEYMAP_NAMELEN);
	if (status >= 0)
	{
		sprintf(keyMapFile, PATH_SYSTEM_KEYMAPS "/%s.map", keyMapName);
		if (kernelFileFind(keyMapFile, NULL) >= 0)
			kernelKeyboardSetMap(keyMapFile);
	}

	kernelLog("User %s logged in", userName);

	return (status = 0);
}


int kernelUserLogout(const char *userName)
{
	// Logs a user out.  Currently, only 1 user can be logged in at a time.

	int status = 0;
	char keyMapFile[MAX_PATH_NAME_LENGTH + 1];

	// Check initialization
	if (!initialized)
		return (status = ERR_NOTINITIALIZED);

	// Check params.  If userName is NULL, we do the current user.
	if (!userName)
		userName = currentUser.name;

	// Is the user logged in?
	if (!currentUser.name[0] || !currentUser.loginPid)
		return (status = ERR_NOSUCHUSER);

	// Restore keyboard mapping to the default
	if ((kernelConfigGet(KERNEL_DEFAULT_CONFIG, KERNELVAR_KEYBOARD_MAP,
			keyMapFile, MAX_PATH_NAME_LENGTH) >= 0) &&
		(kernelFileFind(keyMapFile, NULL) >= 0))
	{
		kernelKeyboardSetMap(keyMapFile);
	}
	else
	{
		kernelKeyboardSetMap(NULL);
	}

	// Kill the user's login process.  The termination of the login process
	// is what effectively logs out the user.  This will only succeed if the
	// current process is owned by the user, or if the current process is
	// supervisor privilege
	if (kernelMultitaskerProcessIsAlive(currentUser.loginPid))
	{
		// Retain this status as the return value
		status = kernelMultitaskerKillProcess(currentUser.loginPid);
	}

	// Clear the user structure
	memset(&currentUser, 0, sizeof(userSession));

	kernelLog("User %s logged out", userName);

	return (status);
}


int kernelUserExists(const char *userName)
{
	// Returns 1 if the user exists in the system user list.

	int status = 0;

	// Check initialization
	if (!initialized)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!userName)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	status = userExists(userName, &systemUserList);

	return (status);
}


int kernelUserGetNames(char *buffer, unsigned bufferSize)
{
	// Returns all the user names (up to bufferSize bytes) as NULL-separated
	// strings, and returns the number copied.

	char *bufferPointer = NULL;
	const char *user = NULL;
	int count;

	// Check initialization
	if (!initialized)
		return (ERR_NOTINITIALIZED);

	// Check params
	if (!buffer || !bufferSize)
	{
		kernelError(kernel_error, "NULL parameter");
		return (ERR_NULLPARAMETER);
	}

	bufferPointer = buffer;
	bufferPointer[0] = '\0';

	// Loop through the list, appending names and newlines
	for (count = 0; ((count < systemUserList.numVariables) &&
		(strlen(buffer) < bufferSize)); count ++)
	{
		user = variableListGetVariable(&systemUserList, count);
		strcat(bufferPointer, user);
		bufferPointer += (strlen(user) + 1);
	}

	return (count);
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
	if (!userName || !password)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check to make sure the user doesn't already exist
	if (userExists(userName, &systemUserList))
	{
		kernelError(kernel_error, "User already exists");
		return (status = ERR_ALREADY);
	}

	// Add the user
	status = addUser(&systemUserList, userName, password);
	if (status < 0)
		return (status);

	// If we can write to the password file, write it out
	if (systemDirWritable)
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
	if (!userName)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check to make sure the user exists
	if (!userExists(userName, &systemUserList))
	{
		kernelError(kernel_error, "User doesn't exist");
		return (status = ERR_NOSUCHUSER);
	}

	// Delete the user
	status = deleteUser(&systemUserList, userName);
	if (status < 0)
		return (status);

	// If we can write to the password file, write it out
	if (systemDirWritable)
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
	if (!userName || !oldPass || !newPass)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Check to make sure the user exists
	if (!userExists(userName, &systemUserList))
	{
		kernelError(kernel_error, "User doesn't exist");
		return (status = ERR_NOSUCHUSER);
	}

	status = setPassword(&systemUserList, userName, oldPass, newPass);
	if (status < 0)
		return (status);

	// If we can write to the password file, write it out
	if (systemDirWritable)
		status = writePasswordFile(USER_PASSWORDFILE, &systemUserList);

	return (status);
}


int kernelUserGetCurrentLoginPid(void)
{
	// Returns the login process ID of the currently logged-in user, if any

	// Check initialization
	if (!initialized)
		return (ERR_NOTINITIALIZED);

	// Temporary, until we have multi-user support
	return (currentUser.loginPid);
}


int kernelUserGetCurrent(char *userName, unsigned bufferLen)
{
	// Returns the name of the currently logged-in user, if any

	int status = 0;
	int processId = 0;

	// Check initialization
	if (!initialized)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!userName || !bufferLen)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	processId = kernelUserGetCurrentLoginPid();
	if (processId < 0)
	{
		kernelError(kernel_error, "Current user is unknown");
		return (status = processId);
	}

	// Temporary, until we have multi-user support
	strncpy(userName, currentUser.name, min(bufferLen, USER_MAX_NAMELENGTH));

	return (status = 0);
}


int kernelUserGetPrivilege(const char *userName)
{
	// Returns the default privilege level for the supplied user name

	// Check initialization
	if (!initialized)
		return (ERR_NOTINITIALIZED);

	// Check params
	if (!userName)
	{
		kernelError(kernel_error, "NULL parameter");
		return (ERR_NULLPARAMETER);
	}

	// Check to make sure the user exists
	if (!userExists(userName, &systemUserList))
	{
		kernelError(kernel_error, "User doesn't exist");
		return (ERR_NOSUCHUSER);
	}

	// This is just a kludge for now.  'admin' is supervisor privilege,
	// everyone else is user privilege
	if (!strcmp(userName, USER_ADMIN))
		return (PRIVILEGE_SUPERVISOR);
	else
		return (PRIVILEGE_USER);
}


int kernelUserGetSessions(userSession *sessions, int max)
{
	// Fills the supplied array with all of the current user sessions (up to
	// 'max' entries) and returns the number copied.

	int status = 0;

	// Check initialization
	if (!initialized)
		return (status = ERR_NOTINITIALIZED);

	// Check params
	if (!sessions)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Temporary, until we have multi-user support
	if (currentUser.loginPid && (max >= 1))
	{
		memcpy(sessions, &currentUser, sizeof(userSession));
		status = 1;
	}

	return (status);
}


int kernelUserFileAdd(const char *fileName, const char *userName,
	const char *password)
{
	// Add a user to the designated (non-system) password file, with the given
	// name and password.

	int status = 0;
	variableList userList;

	// Check params
	if (!fileName || !userName || !password)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure this isn't the system password file.
	status = isSystemPasswordFile(fileName);
	if (status < 0)
		// Filename probably isn't valid
		return (status);

	if (status == 1)
	{
		kernelError(kernel_error, "Cannot write the system password file");
		return (status = ERR_PERMISSION);
	}

	// Try to read the requested password file
	status = readPasswordFile(fileName, &userList);
	if (status >= 0)
	{
		// Check to make sure the user doesn't already exist
		if (!userExists(userName, &userList))
		{
			status = addUser(&userList, userName, password);
			if (status >= 0)
				status = writePasswordFile(fileName, &userList);
		}
		else
		{
			kernelError(kernel_error, "User already exists");
			status = ERR_ALREADY;
		}

		variableListDestroy(&userList);
	}

	return (status);
}


int kernelUserFileDelete(const char *fileName, const char *userName)
{
	// Remove a user from the designated (non-system) password file.

	int status = 0;
	variableList userList;

	// Check params
	if (!fileName || !userName)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure this isn't the system password file.
	status = isSystemPasswordFile(fileName);
	if (status < 0)
		// Filename probably isn't valid
		return (status);

	if (status == 1)
	{
		kernelError(kernel_error, "Cannot write the system password file");
		return (status = ERR_PERMISSION);
	}

	// Try to read the requested password file
	status = readPasswordFile(fileName, &userList);
	if (status >= 0)
	{
		// Check to make sure the user exists
		if (userExists(userName, &userList))
		{
			status = deleteUser(&userList, userName);
			if (status >= 0)
				status = writePasswordFile(fileName, &userList);
		}
		else
		{
			kernelError(kernel_error, "User doesn't exist");
			status = ERR_NOSUCHUSER;
		}

		variableListDestroy(&userList);
	}

	return (status);
}


int kernelUserFileSetPassword(const char *fileName, const char *userName,
	const char *oldPass, const char *newPass)
{
	// Set the password in the designated (non-system) password file.

	int status = 0;
	variableList userList;

	// Check params
	if (!fileName || !userName || !oldPass || !newPass)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Make sure this isn't the system password file.
	status = isSystemPasswordFile(fileName);
	if (status < 0)
		// Filename probably isn't valid
		return (status);

	if (status == 1)
	{
		kernelError(kernel_error, "Cannot write the system password file");
		return (status = ERR_PERMISSION);
	}

	// Try to read the requested password file
	status = readPasswordFile(fileName, &userList);
	if (status >= 0)
	{
		// Check to make sure the user exists
		if (userExists(userName, &userList))
		{
			status = setPassword(&userList, userName, oldPass, newPass);
			if (status >= 0)
				status = writePasswordFile(fileName, &userList);
		}
		else
		{
			kernelError(kernel_error, "User doesn't exist");
			status = ERR_NOSUCHUSER;
		}

		variableListDestroy(&userList);
	}

	return (status);
}

