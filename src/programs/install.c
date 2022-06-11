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
//  install.c
//

// This is a program for installing the system on a target disk (filesystem).

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/vsh.h>

#define MOUNTPOINT               "/tmp_install"
#define BASICINSTALL             "/system/install-files.basic"
#define FULLINSTALL              "/system/install-files.full"
#define STATUSLENGTH             80
#define TEXT_PROGRESSBAR_LENGTH  20

typedef enum { install_basic, install_full } install_type;

static int processId = 0;
static char rootDisk[DISK_MAX_NAMELENGTH];
static int numberDisks = 0;
static disk diskInfo[DISK_MAXDEVICES];
static char *diskName = NULL;
static char *titleString = "Visopsys Installer\nCopyright (C) 1998-2005 "
                           "J. Andrew McLaughlin";
static char *chooseVolumeString = "Please choose the volume on which to "
  "install.  Note that the installation\nvolume MUST be of the same "
  "filesystem type as the current root filesystem!";
static char *setPasswordString = "Please choose a password for the 'admin' "
                                 "account";
static char statusLabelString[STATUSLENGTH];
static install_type installType;
static unsigned bytesToCopy = 0;
static unsigned bytesCopied = 0;
static int textProgressBarRow = 0;

// GUI stuff
static int graphics = 0;
static objectKey window = NULL;
static objectKey installTypeRadio = NULL;
static objectKey statusLabel = NULL;
static objectKey progressBar = NULL;
static objectKey installButton = NULL;
static objectKey quitButton = NULL;


static void error(const char *format, ...)
{
  // Generic error message code for either text or graphics modes
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  _expandFormatString(output, format, list);
  va_end(list);

  if (graphics)
    windowNewErrorDialog(window, "Error", output);
  else
    printf("\n\nERROR: %s\n\n", output);
}


static void quit(int status, const char *message, ...)
{
  // Shut everything down

  va_list list;
  char output[MAXSTRINGLENGTH];
  
  if (message != NULL)
    {
      va_start(list, message);
      _expandFormatString(output, message, list);
      va_end(list);
    }

  if (graphics)
    windowGuiStop();

  if (message != NULL)
    {
      if (status < 0)
	error("%s  Quitting.", output);
      else
	{
	  if (graphics)
	    windowNewInfoDialog(window, "Complete", output);
	  else
	    printf("\n\n%s\n\n", output);
	}
    }

  if (graphics && window)
    windowDestroy(window);

  errno = status;

  exit(status);
}


static void makeDiskList(void)
{
  // Make a list of disks on which we can install

  int status = 0;
  // Call the kernel to give us the number of available disks
  int tmpNumberDisks = diskGetCount();
  disk tmpDiskInfo[DISK_MAXDEVICES];
  int count;

  numberDisks = 0;
  bzero(diskInfo, (DISK_MAXDEVICES * sizeof(disk)));
  bzero(tmpDiskInfo, (DISK_MAXDEVICES * sizeof(disk)));

  status = diskGetInfo(tmpDiskInfo);
  if (status < 0)
    // Eek.  Problem getting disk info
    quit(status, "Unable to get disk information.");

  // Loop through the list we got.  Copy any valid disks (disks to which
  // we might be able to install) into the main array
  for (count = 0; count < tmpNumberDisks; count ++)
    {
      // Make sure it's not the root disk; that would be pointless and possibly
      // dangerous
      if (!strcmp(rootDisk, tmpDiskInfo[count].name))
	continue;

      // Skip CD-ROMS
      if ((tmpDiskInfo[count].type == idecdrom) ||
	  (tmpDiskInfo[count].type == scsicdrom))
	continue;

      // Otherwise, we will put this in the list
      memcpy(&(diskInfo[numberDisks]), &(tmpDiskInfo[count]), sizeof(disk));
      numberDisks += 1;
    }
}


static void eventHandler(objectKey key, windowEvent *event)
{
  // Check for the window being closed, or pressing of the 'Quit' button.
  if (((key == window) && (event->type == EVENT_WINDOW_CLOSE)) ||
      ((key == quitButton) && (event->type == EVENT_MOUSE_LEFTUP)))
    quit(0, NULL);

  // Check for the 'Install' button
  else if ((key == installButton) && (event->type == EVENT_MOUSE_LEFTUP))
    // Stop the GUI here and the installation will commence
    windowGuiStop();
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;
  objectKey textLabel = NULL;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "Install");
  if (window == NULL)
    quit(ERR_NOCREATE, "Can't create window!");

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 2;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_left;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;
  textLabel = windowNewTextLabel(window, titleString, &params);
  
  params.gridY = 1;
  char tmp[40];
  sprintf(tmp, "[ Installing on disk %s ]", diskName);
  windowNewTextLabel(window, tmp, &params);

  params.gridY = 2;
  installTypeRadio = windowNewRadioButton(window, 2, 1, (char *[])
      { "Basic install", "Full install" }, 2 , &params);
  windowComponentSetEnabled(installTypeRadio, 0);

  params.gridY = 3;
  params.gridWidth = 2;
  statusLabel = windowNewTextLabel(window, "", &params);
  windowComponentSetWidth(statusLabel, windowComponentGetWidth(textLabel));
  bzero(statusLabelString, STATUSLENGTH);

  params.gridY = 4;
  progressBar = windowNewProgressBar(window, &params);

  params.gridY = 5;
  params.gridWidth = 1;
  params.padBottom = 5;
  params.orientationX = orient_right;
  installButton = windowNewButton(window, "Install", NULL, &params);
  windowRegisterEventHandler(installButton, &eventHandler);
  windowComponentSetEnabled(installButton, 0);

  params.gridX = 1;
  params.orientationX = orient_left;
  quitButton = windowNewButton(window, "Quit", NULL, &params);
  windowRegisterEventHandler(quitButton, &eventHandler);
  windowComponentSetEnabled(quitButton, 0);

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  // Go
  windowSetVisible(window, 1);
}


static void printBanner(void)
{
  // Print a message
  textScreenClear();
  printf("\n%s\n\n", titleString);
}


static int yesOrNo(char *question)
{
  char character;

  if (graphics)
    return (windowNewQueryDialog(window, "Confirmation", question));

  else
    {
      printf("\n%s (y/n): ", question);
      textInputSetEcho(0);

      while(1)
	{
	  character = getchar();

	  if ((character == 'y') || (character == 'Y'))
	    {
	      printf("Yes\n");
	      textInputSetEcho(1);
	      return (1);
	    }
	  else if ((character == 'n') || (character == 'N'))
	    {
	      printf("No\n");
	      textInputSetEcho(1);
	      return (0);
	    }
	}
    }
}


static int chooseDisk(void)
{
  // This is where the user chooses the disk on which to install

  int status = 0;
  int diskNumber = -1;
  objectKey chooseWindow = NULL;
  componentParameters params;
  objectKey diskList = NULL;
  objectKey okButton = NULL;
  objectKey partButton = NULL;
  objectKey cancelButton = NULL;
  char *diskStrings[DISK_MAXDEVICES];
  windowEvent event;
  int count;

  // We jump back to this position if the user repartitions the disks
 start:

  bzero(&params, sizeof(componentParameters));
  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 3;
  params.gridHeight = 1;
  params.padTop = 5;
  params.padLeft = 5;
  params.padRight = 5;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  char *tmp = malloc(numberDisks * 80);
  for (count = 0; count < numberDisks; count ++)
    {
      diskStrings[count] = (tmp + (count * 80));
      sprintf(diskStrings[count], "%s  [ %s ]", diskInfo[count].name,
	      diskInfo[count].partType.description);
    }

  if (graphics)
    {
      chooseWindow = windowNew(processId, "Choose Installation Disk");
      windowNewTextLabel(chooseWindow, chooseVolumeString, &params);

      // Make a window list with all the disk choices
      params.gridY = 1;
      diskList = windowNewList(chooseWindow, 5, 1, 0, diskStrings,
			       numberDisks, &params);
      free(diskStrings[0]);

      // Make 'OK', 'partition', and 'cancel' buttons
      params.gridY = 2;
      params.gridWidth = 1;
      params.padBottom = 5;
      params.padRight = 0;
      params.orientationX = orient_right;
      okButton = windowNewButton(chooseWindow, "OK", NULL, &params);

      params.gridX = 1;
      params.padRight = 5;
      params.orientationX = orient_center;
      partButton = windowNewButton(chooseWindow, "Partition disks...", NULL,
				   &params);

      params.gridX = 2;
      params.padLeft = 0;
      params.orientationX = orient_left;
      cancelButton = windowNewButton(chooseWindow, "Cancel", NULL, &params);

      // Make the window visible
      windowSetHasMinimizeButton(chooseWindow, 0);
      windowSetHasCloseButton(chooseWindow, 0);
      windowSetResizable(chooseWindow, 0);
      windowSetVisible(chooseWindow, 1);

      while(1)
	{
	  // Check for our OK button
	  status = windowComponentEventGet(okButton, &event);
	  if ((status < 0) || ((status > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP)))
	    {
	      diskNumber = windowComponentGetSelected(diskList);
	      break;
	    }

	  // Check for our 'partition' button
	  status = windowComponentEventGet(partButton, &event);
	  if ((status < 0) || ((status > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP)))
	    {
	      // The user wants to repartition the disks.  Get rid of this
	      // window, run the disk manager, and start again
	      windowDestroy(chooseWindow);
	      chooseWindow = NULL;
	      // Privilege zero, no args, block
	      loaderLoadAndExec("/programs/fdisk", 0, 0, NULL, 1);
	      // Remake our disk list
	      makeDiskList();
	      goto start;
	    }

	  // Check for our Cancel button
	  status = windowComponentEventGet(cancelButton, &event);
	  if ((status < 0) || ((status > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP)))
	    break;

	  // Done
	  multitaskerYield();
	}

      windowDestroy(chooseWindow);
      chooseWindow = NULL;
    }

  else
    diskNumber =
      vshCursorMenu(chooseVolumeString, numberDisks, diskStrings, 0);

  free(diskStrings[0]);
  return (diskNumber);
}


static unsigned getInstallSize(const char *installFileName)
{
  // Given the name of an install file, calculate the number of bytes
  // of disk space the installation will require.

  #define BUFFSIZE 160

  int status = 0;
  fileStream installFile;
  file theFile;
  char buffer[BUFFSIZE];
  unsigned bytes = 0;

  // Clear stack data
  bzero(&installFile, sizeof(fileStream));
  bzero(&theFile, sizeof(file));
  bzero(buffer, BUFFSIZE);

  // Open the install file
  status = fileStreamOpen(installFileName, OPENMODE_READ, &installFile);
  if (status < 0)
    // Can't open the install file.
    return (bytes = 0);

  // Read it line by line
  while (1)
    {
      status = fileStreamReadLine(&installFile, BUFFSIZE, buffer);
      if (status < 0)
	{
	  error("Error reading from install file \"%s\"", installFileName);
	  fileStreamClose(&installFile);
	  return (bytes = 0);
	}

      else if ((buffer[0] == '\n') || (buffer[0] == '#'))
	// Ignore blank lines and comments
	continue;

      else if (status == 0)
	{
	  // End of file
	  fileStreamClose(&installFile);
	  break;
	}

      else
	{
	  // If there's a newline at the end of the line, remove it
	  if (buffer[strlen(buffer) - 1] == '\n')
	    buffer[strlen(buffer) - 1] = '\0';

	  // Use the line of data as the name of a file.  We try to find the
	  // file and add its size to the number of bytes
	  status = fileFind(buffer, &theFile);
	  if (status < 0)
	    {
	      error("Can't open source file \"%s\"", buffer);
	      continue;
	    }

	  bytes += theFile.size;
	}
    }

  // Add 1K for a little buffer space
  return (bytes + 1024);
}


static void updateStatus(const char *message)
{
  // Updates progress messages.  In text mode we just printf() it, but
  // in graphics mode we have to update a text label
  
  int statusLength = 0;

  if (graphics)
    {
      
      if (strlen(statusLabelString) &&
	  (statusLabelString[strlen(statusLabelString) - 1] != '\n'))
	strcat(statusLabelString, message);
      else
	strcpy(statusLabelString, message);

      statusLength = strlen(statusLabelString);
      if (statusLength >= STATUSLENGTH)
	{
	  statusLength = (STATUSLENGTH - 1);
	  statusLabelString[statusLength] = '\0';
	}
      if (statusLabelString[statusLength - 1] == '\n')
	statusLength -= 1;

      windowComponentSetData(statusLabel, statusLabelString, statusLength);
    }
  else
    printf(message);
}


static void makeTextProgressBar(void)
{
  // Make an initial text mode progress bar.

  char row[TEXT_PROGRESSBAR_LENGTH + 3];
  int count;

  // Make the top row
  row[0] = 218;
  for (count = 1; count <= TEXT_PROGRESSBAR_LENGTH; count ++)
    row[count] = 196;
  row[count++] = 191;
  row[TEXT_PROGRESSBAR_LENGTH + 2] = '\0';
  printf("\n%s\n", row);

  // Remember this row
  textProgressBarRow = textGetRow();

  // Middle row
  row[0] = 179;
  for (count = 1; count <= TEXT_PROGRESSBAR_LENGTH; count ++)
    row[count] = ' ';
  row[count++] = 179;
  row[TEXT_PROGRESSBAR_LENGTH + 2] = '\0';
  printf("%s\n", row);

  // Bottom row
  row[0] = 192;
  for (count = 1; count <= TEXT_PROGRESSBAR_LENGTH; count ++)
    row[count] = 196;
  row[count++] = 217;
  row[TEXT_PROGRESSBAR_LENGTH + 2] = '\0';
  printf("%s\n\n", row);
}


static void setTextProgressBar(int percent)
{
  // Set the value of the text mode progress bar.

  int tempColumn = textGetColumn();
  int tempRow = textGetRow();
  int progressChars = 0;
  int column = 0;
  char row[TEXT_PROGRESSBAR_LENGTH + 1];
  int count;

  progressChars = ((percent * TEXT_PROGRESSBAR_LENGTH) / 100);

  textSetColumn(1);
  textSetRow(textProgressBarRow);

  for (count = 0; count < progressChars; count ++)
    row[count] = 177;
  row[count] = '\0';
  printf("%s\n", row);

  column = (TEXT_PROGRESSBAR_LENGTH / 2);
  if (percent < 10)
    column += 1;
  else if (percent >= 100)
    column -= 1;
  textSetColumn(column);
  printf("%d%%", percent);

  // Back to where we were
  textSetColumn(tempColumn);
  textSetRow(tempRow);
}


static int copyBootSector(const char *destDisk)
{
  // Overlay the boot sector from the root disk onto the boot sector of
  // the target disk

  int status = 0;
  file bootSectFile;
  unsigned char *bootSectData = NULL;
  unsigned char rootBootSector[512];
  unsigned char destBootSector[512];
  int count;

  updateStatus("Copying boot sector...  ");

  // Try to read a boot sector file from the system directory
  bootSectData = loaderLoad("/system/boot/bootsect.fat12", &bootSectFile);  
  
  if (bootSectData == NULL)
    {
      // Try to read the boot sector of the root disk instead
      status = diskReadSectors(rootDisk, 0, 1, rootBootSector);
      if (status < 0)
	{
	  printf("\nUnable to read the boot sector of the root disk.\n");
	  return (status);
	}

      bootSectData = rootBootSector;
    }
  
  // Read the boot sector of the target disk
  status = diskReadSectors(destDisk, 0, 1, destBootSector);
  if (status < 0)
    {
      printf("\nUnable to read the boot sector of the target disk.\n");
      return (status);
    }

  // Copy bytes 0-2 and 62-511 from the root disk boot sector to the
  // target boot sector
  for (count = 0; count < 3; count ++)
    destBootSector[count] = bootSectData[count];
  for (count = 62; count < 512; count ++)
    destBootSector[count] = bootSectData[count];

  if (bootSectData != rootBootSector)
    memoryRelease(bootSectData);

  // Write the boot sector of the target disk
  status = diskWriteSectors(destDisk, 0, 1, destBootSector);
  if (status < 0)
    {
      printf("\nUnable to write the boot sector of the target disk\n");
      return (status);
    }

  diskSync();

  updateStatus("Done\n");

  return (status = 0);
}


static int copyFiles(const char *installFileName)
{
  #define BUFFSIZE 160

  int status = 0;
  fileStream installFile;
  file theFile;
  int percent = 0;
  char buffer[BUFFSIZE];
  char tmpFileName[128];

  // Clear stack data
  bzero(&installFile, sizeof(fileStream));
  bzero(&theFile, sizeof(file));
  bzero(buffer, BUFFSIZE);

  // Open the install file
  status = fileStreamOpen(installFileName, OPENMODE_READ, &installFile);
  if (status < 0)
    {
      error("Can't open install file \"%s\"",  installFileName);
      return (status);
    }

  sprintf(buffer, "Copying %s files...  ",
	  ((installFileName == BASICINSTALL)? "basic" : "extra"));
  updateStatus(buffer);

  // Read it line by line
  while (1)
    {
      status = fileStreamReadLine(&installFile, BUFFSIZE, buffer);
      if (status < 0)
	{
	  fileStreamClose(&installFile);
	  error("Error reading from install file \"%s\"", installFileName);
	  goto done;
	}

      else if ((buffer[0] == '\n') || (buffer[0] == '#'))
	// Ignore blank lines and comments
	continue;

      else if (status == 0)
	{
	  // End of file
	  fileStreamClose(&installFile);
	  break;
	}

      else
	{
	  // Use the line of data as the name of a file.  We try to find the
	  // file and add its size to the number of bytes
	  status = fileFind(buffer, &theFile);
	  if (status < 0)
	    {
	      // Later we should do something here to make a message listing
	      // the names of any missing files
	      error("Missing file \"%s\"", buffer);
	      continue;
	    }

	  strcpy(tmpFileName, MOUNTPOINT);
	  strcat(tmpFileName, buffer);

	  if (theFile.type == dirT)
	    // It's a directory, create it in the desination
	    status = fileMakeDir(tmpFileName);

	  else
	    // It's a file.  Copy it to the destination.
	    status = fileCopy(buffer, tmpFileName);

	  if (status < 0)
	    goto done;

	  bytesCopied += theFile.size;

	  percent = ((bytesCopied * 100) / bytesToCopy);

	  // Sync periodially
	  if (!(percent % 10))
	    diskSync();

	  if (graphics)
	    windowComponentSetData(progressBar, (void *) percent, 1);
	  else
	    setTextProgressBar(percent);
	}
    }

  status = 0;

 done:
  diskSync();

  updateStatus("Done\n");

  return (status);
}


static void setAdminPassword(void)
{
  // Show a 'set password' dialog box for the admin user

  int status = 0;
  objectKey dialogWindow = NULL;
  componentParameters params;
  objectKey label = NULL;
  objectKey passwordField1 = NULL;
  objectKey passwordField2 = NULL;
  objectKey noMatchLabel = NULL;
  objectKey okButton = NULL;
  objectKey cancelButton = NULL;
  windowEvent event;
  char confirmPassword[17];
  char newPassword[17];
  fileStream passFile;
  char hashValue[16];
  char byte[3];
  int count;

  if (graphics)
    {
      // Create the dialog
      dialogWindow = windowNewDialog(window, "Set Administrator Password");

      bzero(&params, sizeof(componentParameters));
      params.gridWidth = 2;
      params.gridHeight = 1;
      params.padLeft = 5;
      params.padRight = 5;
      params.padTop = 5;
      params.orientationX = orient_center;
      params.orientationY = orient_middle;
      params.useDefaultForeground = 1;
      params.useDefaultBackground = 1;
      label = windowNewTextLabel(dialogWindow, setPasswordString, &params);
	  
      params.gridY = 1;
      params.gridWidth = 1;
      params.padRight = 0;
      params.orientationX = orient_right;
      label = windowNewTextLabel(dialogWindow, "New password:", &params);

      params.gridX = 1;
      params.hasBorder = 1;
      params.padRight = 5;
      params.orientationX = orient_left;
      params.useDefaultBackground = 0;
      params.background.red = 255;
      params.background.green = 255;
      params.background.blue = 255;
      passwordField1 = windowNewPasswordField(dialogWindow, 17, &params);
      
      params.gridX = 0;
      params.gridY = 2;
      params.padRight = 0;
      params.orientationX = orient_right;
      params.hasBorder = 0;
      params.useDefaultBackground = 1;
      label = windowNewTextLabel(dialogWindow, "Confirm password:", &params);

      params.gridX = 1;
      params.orientationX = orient_left;
      params.padRight = 5;
      params.hasBorder = 1;
      params.useDefaultBackground = 0;
      params.background.red = 255;
      params.background.green = 255;
      params.background.blue = 255;
      passwordField2 = windowNewPasswordField(dialogWindow, 17, &params);
	  
      params.gridX = 0;
      params.gridY = 3;
      params.gridWidth = 2;
      params.orientationX = orient_center;
      params.hasBorder = 0;
      params.useDefaultBackground = 1;
      noMatchLabel = windowNewTextLabel(dialogWindow, "Passwords do not "
					"match", &params);
      windowComponentSetVisible(noMatchLabel, 0);
      
      // Create the OK button
      params.gridY = 4;
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

    graphicsRestart:
      while(1)
	{
	  // Check for window close events
	  status = windowComponentEventGet(dialogWindow, &event);
	  if ((status < 0) || ((status > 0) &&
			       (event.type == EVENT_WINDOW_CLOSE)))
	    {
	      error("No password set.  It will be blank.");
	      windowDestroy(dialogWindow);
	      return;
	    }
	  
	  // Check for the OK button
	  status = windowComponentEventGet(okButton, &event);
	  if ((status > 0) && (event.type == EVENT_MOUSE_LEFTUP))
	    break;
	  
	  // Check for the Cancel button
	  status = windowComponentEventGet(cancelButton, &event);
	  if ((status < 0) || ((status > 0) &&
	      (event.type == EVENT_MOUSE_LEFTUP)))
	    {
	      error("No password set.  It will be blank.");
	      windowDestroy(dialogWindow);
	      return;
	    }
	  
	  if ((windowComponentEventGet(passwordField1, &event) &&
	       (event.type == EVENT_KEY_DOWN)) ||
	      (windowComponentEventGet(passwordField2, &event) &&
	       (event.type == EVENT_KEY_DOWN)))
	    {
	      if (event.key == (unsigned char) 10)
		break;
	      else
		{
		  windowComponentGetData(passwordField1, newPassword, 16);
		  windowComponentGetData(passwordField2, confirmPassword, 16);
		  if (strncmp(newPassword, confirmPassword, 16))
		    {
		      windowComponentSetVisible(noMatchLabel, 1);
		      windowComponentSetEnabled(okButton, 0);
		    }
		  else
		    {
		      windowComponentSetVisible(noMatchLabel, 0);
		      windowComponentSetEnabled(okButton, 1);
		    }
		}
	    }
	  
	  // Done
	  multitaskerYield();
	}
      
      windowComponentGetData(passwordField1, newPassword, 16);
      windowComponentGetData(passwordField2, confirmPassword, 16);
    }
  else
    {
    textRestart:
      printf("\n%s\n", setPasswordString);

      // Turn keyboard echo off
      textInputSetEcho(0);
  
      vshPasswordPrompt("New password: ", newPassword);
      vshPasswordPrompt("Confirm password: ", confirmPassword);
    }

  // Make sure the new password and confirm passwords match
  if (strncmp(newPassword, confirmPassword, 16))
    {
      error("Passwords do not match");
      if (graphics)
	{
	  windowComponentSetData(passwordField1, "", 0);
	  windowComponentSetData(passwordField2, "", 0);
	  goto graphicsRestart;
	}
      else
	goto textRestart;
    }

  if (graphics)
    windowDestroy(dialogWindow);
  else
    printf("\n");

  status = encryptMD5(newPassword, hashValue);
  if (status < 0)
    {
      error("Unable to encrypt password.");
      return;
    }

  // Turn it into a string
  char tmp[80];
  strcpy(tmp, "admin=");
  for (count = 0; count < 16; count ++)
    {
      sprintf(byte, "%02x", (unsigned char) hashValue[count]);
      strcat(tmp, byte);
    }

  // Create the password file stream
  status = fileStreamOpen(MOUNTPOINT "/system/password",
			  (OPENMODE_WRITE | OPENMODE_CREATE), &passFile);
  if (status < 0)
    {
      error("Unable to create the password file.");
      return;
    }
  
  fileStreamWriteLine(&passFile, tmp);
  fileStreamClose(&passFile);

  return;
}


static void changeStartProgram(void)
{
  // Change the target installation's start program to the login program

  variableList *kernelConf =
    configurationReader(MOUNTPOINT "/system/kernel.conf");
  if (kernelConf)
    {
      variableListSet(kernelConf, "start.program", "/programs/login");
      configurationWriter(MOUNTPOINT "/system/kernel.conf", kernelConf);
      free(kernelConf);
    }
}


int main(int argc, char *argv[])
{
  int status = 0;
  int diskNumber = -1;
  char tmpChar[80];
  unsigned diskSize = 0;
  unsigned basicInstallSize = 0xFFFFFFFF;
  unsigned fullInstallSize = 0xFFFFFFFF;
  int count;

  processId = multitaskerGetCurrentProcessId();

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  if (getopt(argc, argv, "T") == 'T')
    // Force text mode
    graphics = 0;

  // Check privilege level
  if (multitaskerGetProcessPrivilege(processId) != 0)
    quit(ERR_PERMISSION, "You must be a privileged user to use this command."
	 "\n(Try logging in as user \"admin\").");

  // Get the root disk
  status = diskGetBoot(rootDisk);
  if (status < 0)
    // Couldn't get the root disk name
    quit(status, "Can't determine the root disk.");

  makeDiskList();

  if (!graphics)
    printBanner();

  // The user can specify the disk name as an argument.  Try to see
  // whether they did so.
  if (argc > 1)
    {
      for (count = 0; count < numberDisks; count ++)
	if (!strcmp(diskInfo[count].name, argv[argc - 1]))
	  {
	    diskNumber = count;
	    break;
	  }
    }

  if (diskNumber < 0)
    // The user has not specified a disk number.  We need to display the
    // list of available disks and prompt them.
    diskNumber = chooseDisk();

  if (diskNumber < 0)
    quit(diskNumber, NULL);

  diskName = diskInfo[diskNumber].name;

  if (graphics)
    constructWindow();

  // Calculate the number of bytes that will be consumed by the various
  // types of install
  basicInstallSize = getInstallSize(BASICINSTALL);
  fullInstallSize = getInstallSize(FULLINSTALL);

  // How much space is available on the disk?
  diskSize =
    (diskInfo[diskNumber].numSectors * diskInfo[diskNumber].sectorSize);

  // Make sure there's at least room for a basic install
  if (diskSize < basicInstallSize)
    quit((status = ERR_NOFREE), "Disk %s is too small (%dK) to install "
	 "Visopsys\n(%dK required)", diskInfo[diskNumber].name,
	 (diskSize/ 1024), (basicInstallSize / 1024));

  // Show basic/full install choices based on whether there's enough space
  // to do both
  if (fullInstallSize && ((basicInstallSize + fullInstallSize) < diskSize))
    {
      if (graphics)
	{
	  windowComponentSetSelected(installTypeRadio, 1);
	  windowComponentSetEnabled(installTypeRadio, 1);
	}
    }

  // Wait for the user to select any options and commence the installation
  if (graphics)
    {
      // We're ready to go, enable the buttons
      windowComponentSetEnabled(installButton, 1);
      windowComponentSetEnabled(quitButton, 1);
      // Focus the 'install' button by default
      windowComponentFocus(installButton);

      windowGuiRun();

      // Disable some components which are no longer usable
      windowComponentSetEnabled(installButton, 0);
      windowComponentSetEnabled(quitButton, 0);
      windowComponentSetEnabled(installTypeRadio, 0);
    }

  // Find out what type of installation to do
  installType = install_basic;
  if (graphics)
    {
      if (windowComponentGetSelected(installTypeRadio) == 1)
	installType = install_full;
    }
  else if (fullInstallSize &&
	   ((basicInstallSize + fullInstallSize) < diskSize))
    {
      status = vshCursorMenu("Please choose the install type:", 2,
			     (char *[]) { "Basic", "Full" }, 1);
      if (status < 0)
	return (status);

      if (status == 1)
	installType = install_full;
    }

  sprintf(tmpChar, "Installing on disk %s.  Are you SURE?", diskName);
  if (!yesOrNo(tmpChar))
    quit(0, "Installation cancelled.");

  sprintf(tmpChar, "Format disk %s? (destroys all data!)", diskName);
  if (yesOrNo(tmpChar))
    {
      updateStatus("Formatting... ");
      status = filesystemFormat(diskName, "fat12", "Visopsys", 0);
      if (status < 0)
	quit(status, "Errors during format.");
      updateStatus("Done\n");
    }

  // Copy the boot sector to the destination disk
  status = copyBootSector(diskName);
  if (status < 0)
    quit(status, "Couldn't copy the boot sector.");

  // Mount the target filesystem
  updateStatus("Mounting target disk...  ");
  status = filesystemMount(diskName, MOUNTPOINT);
  if (status < 0)
    {
      quit(status, "Unable to mount the target disk.");
      return (status);
    }
  updateStatus("Done\n");

  if (!graphics)
    makeTextProgressBar();

  // Copy the files
  bytesToCopy = basicInstallSize;
  if (installType == install_full)
    bytesToCopy += fullInstallSize;
  status = copyFiles(BASICINSTALL);
  if (installType == install_full)
    status = copyFiles(FULLINSTALL);

  if (!graphics)
    // Make sure this shows 100%
    setTextProgressBar(100);

  // Set the start program of the target installation back to the login
  // program
  changeStartProgram();

  // Prompt the user to set the admin password
  setAdminPassword();

  // Unmount the target filesystem
  updateStatus("Unmounting target disk...  ");
  if (filesystemUnmount(MOUNTPOINT) < 0)
    error("Unable to unmount the target disk.");
  updateStatus("Done\n");

  if (status < 0)
    // Couldn't copy the files
    quit(status, "Unable to copy files.");
  
  else
    quit(status, "Installation successful.");

  // Make the compiler happy
  return (status);
}
