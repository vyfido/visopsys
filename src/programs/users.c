//
//  Visopsys
//  Copyright (C) 1998-2004 J. Andrew McLaughlin
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
//  users.c
//

// This is a program for managing users and passwords.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>

static int processId = 0;
static int privilege = 0;
static int readOnly = 1;
static char *userNames[64];
static char userBuffer[1024];
static int numUserNames = 0;
static objectKey window = NULL;
static objectKey userList = NULL;
static objectKey addUserButton = NULL;
static objectKey deleteUserButton = NULL;
static objectKey setPasswordButton = NULL;


static void error(const char *format, ...)
{
  // Generic error message code for either text or graphics modes
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  _expandFormatString(output, format, list);
  va_end(list);

  windowNewErrorDialog(window, "Error", output);
}


static int getUserNames(void)
{
  // Get the list of user names from the kernel

  int status = 0;
  char *bufferPointer = NULL;
  int count;

  bzero(userBuffer, 1024);

  status = userGetNames(userBuffer, 1024);
  if (status < 0)
    return (status);

  numUserNames = status;
  bufferPointer = userBuffer;

  for (count = 0; count < numUserNames; count ++)
    {
      userNames[count] = bufferPointer;
      bufferPointer += (strlen(userNames[count]) + 1);
    }

  return (status = 0);
}


static int setPassword(const char *userName, const char *oldPassword,
		       const char *newPassword)
{
  // Tells the kernel to set the requested password

  int status = 0;

  // Tell the kernel to add the user
  status = userSetPassword(userName, oldPassword, newPassword);
  if (status < 0)
    return (status);

  return (status = 0);
}


static int setPasswordDialog(int userNumber)
{
  // Show a 'set password' dialog box

  int status = 0;
  objectKey dialogWindow = NULL;
  componentParameters params;
  objectKey oldPasswordLabel = NULL;
  objectKey oldPasswordField = NULL;
  objectKey label = NULL;
  objectKey passwordField1 = NULL;
  objectKey passwordField2 = NULL;
  objectKey noMatchLabel = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  windowEvent event;
  char confirmPassword[17];
  char oldPassword[17];
  char newPassword[17];

  bzero(&params, sizeof(componentParameters));

  // Create the dialog
  if (window)
    dialogWindow = windowNewDialog(window, "Set Password");
  else
    dialogWindow = windowNew(processId, "Set Password");
  if (dialogWindow == NULL)
    return (status = ERR_NOCREATE);

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padRight = 5;
  params.padTop = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  char labelText[64];
  sprintf(labelText, "User name: %s", userNames[userNumber]);
  params.gridY = 0;
  params.gridWidth = 2;
  label = windowNewTextLabel(dialogWindow, NULL, labelText, &params);

  // If this user is privileged, or we can authenticate with no password,
  // don't prompt for the old password
  if (privilege && userAuthenticate(userNames[userNumber], ""))
    {
      params.gridY = 1;
      params.gridWidth = 1;
      params.padRight = 0;
      params.orientationX = orient_right;
      oldPasswordLabel = windowNewTextLabel(dialogWindow, NULL, "Old "
					    "password:", &params);

      params.gridX = 1;
      params.orientationX = orient_left;
      params.padRight = 5;
      params.hasBorder = 1;
      params.useDefaultBackground = 0;
      params.background.red = 255;
      params.background.green = 255;
      params.background.blue = 255;
      oldPasswordField =
	windowNewPasswordField(dialogWindow, 17, NULL /* default font*/,
			       &params);
    }

  params.gridX = 0;
  params.gridY = 2;
  params.gridWidth = 1;
  params.padRight = 0;
  params.orientationX = orient_right;
  params.hasBorder = 0;
  params.useDefaultBackground = 1;
  label = windowNewTextLabel(dialogWindow, NULL, "New password:", &params);

  params.gridX = 1;
  params.hasBorder = 1;
  params.padRight = 5;
  params.orientationX = orient_left;
  params.useDefaultBackground = 0;
  params.background.red = 255;
  params.background.green = 255;
  params.background.blue = 255;
  passwordField1 =
    windowNewPasswordField(dialogWindow, 17, NULL /* default font*/, &params);

  params.gridX = 0;
  params.gridY = 3;
  params.padRight = 0;
  params.orientationX = orient_right;
  params.hasBorder = 0;
  params.useDefaultBackground = 1;
  label = windowNewTextLabel(dialogWindow, NULL, "Confirm password:", &params);

  params.gridX = 1;
  params.orientationX = orient_left;
  params.padRight = 5;
  params.hasBorder = 1;
  params.useDefaultBackground = 0;
  params.background.red = 255;
  params.background.green = 255;
  params.background.blue = 255;
  passwordField2 =
    windowNewPasswordField(dialogWindow, 17, NULL /* default font*/, &params);

  params.gridX = 0;
  params.gridY = 4;
  params.gridWidth = 2;
  params.orientationX = orient_center;
  params.hasBorder = 0;
  params.useDefaultBackground = 1;
  noMatchLabel = windowNewTextLabel(dialogWindow, NULL, "Passwords do not "
				    "match", &params);
  windowComponentSetVisible(noMatchLabel, 0);

  // Create the OK button
  params.gridY = 5;
  params.gridWidth = 1;
  params.padBottom = 5;
  params.padRight = 0;
  params.orientationX = orient_right;
  okButton = windowNewButton(dialogWindow, "OK", NULL, &params);

  // Create the Cancel button
  params.gridX = 1;
  params.padRight = 5;
  params.orientationX = orient_left;
  cancelButton = windowNewButton(dialogWindow, "Cancel", NULL, &params);

  windowCenterDialog(window, dialogWindow);
  windowSetVisible(dialogWindow, 1);
  
  while(1)
    {
      // Check for the OK button
      status = windowComponentEventGet(okButton, &event);
      if (status < 0)
	goto out;
      else if ((status > 0) && (event.type == EVENT_MOUSE_UP))
	break;

      // Check for the Cancel button
      status = windowComponentEventGet(cancelButton, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_MOUSE_UP)))
	{
	  windowDestroy(dialogWindow);
	  return (status = ERR_NODATA);
	}
      
      // Check for window close events
      status = windowComponentEventGet(dialogWindow, &event);
      if ((status < 0) || ((status > 0) && (event.type == EVENT_WINDOW_CLOSE)))
	{
	  windowDestroy(dialogWindow);
	  return (status = ERR_NODATA);
	}
      
      // Check for keyboard events 
      if (oldPasswordField)
	{
	  status = windowComponentEventGet(oldPasswordField, &event);
	  if ((status > 0) && (event.type == EVENT_KEY_DOWN) &&
	      (event.key == (unsigned char) 10))
	    break;
	}

      status = windowComponentEventGet(passwordField1, &event);
      if ((status > 0) && (event.type == EVENT_KEY_DOWN))
	{
	  if (event.key == (unsigned char) 10)
	    break;
	  else
	    {
	      windowComponentGetData(passwordField1, newPassword, 16);
	      windowComponentGetData(passwordField2, confirmPassword, 16);
	      if (strncmp(newPassword, confirmPassword, 16))
		windowComponentSetVisible(noMatchLabel, 1);
	      else
		windowComponentSetVisible(noMatchLabel, 0);
	    }
	}

      status = windowComponentEventGet(passwordField2, &event);
      if ((status > 0) && (event.type == EVENT_KEY_DOWN))
	{
	  if (event.key == (unsigned char) 10)
	    break;
	  else
	    {
	      windowComponentGetData(passwordField1, newPassword, 16);
	      windowComponentGetData(passwordField2, confirmPassword, 16);
	      if (strncmp(newPassword, confirmPassword, 16))
		windowComponentSetVisible(noMatchLabel, 1);
	      else
		windowComponentSetVisible(noMatchLabel, 0);
	    }
	}

      // Done
      multitaskerYield();
    }
  
  if (oldPasswordField)
    windowComponentGetData(oldPasswordField, oldPassword, 16);
  else
    oldPassword[0] = '\0';
  windowComponentGetData(passwordField1, newPassword, 16);
  windowComponentGetData(passwordField2, confirmPassword, 16);

 out:
  windowDestroy(dialogWindow);

  // Make sure the new password and confirm passwords match
  if (!strncmp(newPassword, confirmPassword, 16))
    {
      status = setPassword(userNames[userNumber], oldPassword, newPassword);
      if (status == ERR_PERMISSION)
	error("Permission denied");
      else if (status < 0)
	error("Error setting password");
    }
  else
    {
      error("Passwords do not match");
      status = ERR_INVALID;
    }

  return (status);
}


static int addUser(const char *userName, const char *password)
{
  // Tells the kernel to add the requested user name and password

  int status = 0;

  // With the user name, we try to authenticate with no password
  status = userAuthenticate(userName, "");
  if (!status || (status == ERR_PERMISSION))
    {
      error("User \"%s\" already exists.", userName);
      return (status);
    }

  // Tell the kernel to add the user
  status = userAdd(userName, password);
  if (status < 0)
    {
      error("Error adding user");
      return (status);
    }

  // Refresh our list of user names
  status = getUserNames();
  if (status < 0)
    {
      error("Error adding user");
      return (status);
    }

  // Re-populate our list component
  status = windowComponentSetData(userList, userNames, numUserNames);
  if (status < 0)
    return (status);

  return (status = 0);
}


static int deleteUser(const char *userName)
{
  // Tells the kernel to delete the requested user

  int status = 0;

  // Don't try to delete the last user
  if (numUserNames <= 1)
    {
      error("Can't delete the last user");
      return (status = ERR_BOUNDS);
    }

  // Tell the kernel to delete the user
  status = userDelete(userName);
  if (status < 0)
    {
      if (status == ERR_PERMISSION)
	error("Permission denied");
      else
	error("Error deleting user");
      return (status);
    }

  // Refresh our list of user names
  status = getUserNames();
  if (status < 0)
    {
      error("Error deleting user");
      return (status);
    }

  // Re-populate our list component
  status = windowComponentSetData(userList, userNames, numUserNames);
  if (status < 0)
    return (status);

  return (status = 0);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  char userName[17];
  int userNumber = 0;

  // Check for the window being closed by a GUI event.
  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    {
      windowGuiStop();
      exit(0);
    }
  if ((key == addUserButton) && (event->type == EVENT_MOUSE_UP))
    {
      if (windowNewPromptDialog(window, "Add User", "Enter the user name:",
				1, 16, userName) > 0)
	{
	  if (addUser(userName, "") < 0)
	    return;
	  setPasswordDialog(numUserNames - 1);
	  windowNewInfoDialog(window, "Success", "User added");
	}
    }
  if ((key == deleteUserButton) && (event->type == EVENT_MOUSE_UP))
    {
      userNumber = windowComponentGetSelected(userList);  
      if (userNumber < 0)
	return;
      char question[1024];
      sprintf(question, "Delete user %s?", userNames[userNumber]);
      if (windowNewQueryDialog(window, "Delete?", question))
	deleteUser(userNames[userNumber]);
    }
  if ((key == setPasswordButton) && (event->type == EVENT_MOUSE_UP))
    {
      userNumber = windowComponentGetSelected(userList);  
      if (userNumber < 0)
	return;
      if (!setPasswordDialog(userNumber))
	windowNewInfoDialog(window, "Success", "Password set");
    }
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "User Manager");
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));

  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 5;
  params.padTop = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_top;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  params.gridY = 0;
  params.gridHeight = 3;
  params.padBottom = 5;
  userList = windowNewList(window, NULL, 5, 1, 0, userNames, numUserNames,
			   &params);

  // Create an 'add user' button
  params.gridX = 1;
  params.gridHeight = 1;
  params.padRight = 5;
  params.padBottom = 0;
  addUserButton = windowNewButton(window, "Add User", NULL, &params);
  windowRegisterEventHandler(addUserButton, &eventHandler);
  if (privilege || readOnly)
    windowComponentSetEnabled(addUserButton, 0);

  // Create a 'delete user' button
  params.gridY = 1;
  deleteUserButton = windowNewButton(window, "Delete User", NULL, &params);
  windowRegisterEventHandler(deleteUserButton, &eventHandler);
  if (privilege || readOnly)
    windowComponentSetEnabled(deleteUserButton, 0);

  // Create a 'set password' button
  params.gridY = 2;
  params.padBottom = 5;
  setPasswordButton = windowNewButton(window, "Set Password", NULL, &params);
  windowRegisterEventHandler(setPasswordButton, &eventHandler);
  if (readOnly)
    windowComponentSetEnabled(setPasswordButton, 0);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetVisible(window, 1);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char opt;
  char userName[17];
  int setPassword = 0;
  char bootDisk[DISK_MAX_NAMELENGTH];
  int count;

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  // Check options
  while((opt = (char) getopt(argc, argv, "p:")) != (char) -1)
    {
      switch(opt)
	{
	case 'p':
	  strncpy(userName, optarg, 17);
	  setPassword = 1;
	  break;
	default:
	  error("Unknown option '%c'.", opt);
	  break;
	}
    }

  // Find out whether we are currently running on a read-only filesystem
  if (!diskGetBoot(bootDisk))
    readOnly = diskGetReadOnly(bootDisk);

  processId = multitaskerGetCurrentProcessId();
  privilege = multitaskerGetProcessPrivilege(processId);

  // Get the list of user names
  status = getUserNames();
  if (status < 0)
    {
      errno = status;
      perror(argv[0]);
      return (status);
    }

  if (setPassword)
    {
      int userNumber = -1;

      // We're just setting the password for the requested user name.  Find
      // the user number in our list
      for (count = 0; count < numUserNames; count ++)
	if (!strcmp(userNames[count], userName))
	  {
	    userNumber = count;
	    break;
	  }
      if (userNumber < 0)
	error("No such user \"%s\"", userName);
      else
	if (!setPasswordDialog(userNumber))
	  windowNewInfoDialog(window, "Done", "Password set");
    }

  else
    {
      // Make our window
      constructWindow();

      // Run the GUI
      windowGuiRun();
    }

  // Done
  return (errno = status);
}