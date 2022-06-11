//
//  Visopsys
//  Copyright (C) 1998-2011 J. Andrew McLaughlin
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
//  imgboot.c
//

// This is a program to be launched first when booting from distribution
// images, such as CD-ROM ISOs

/* This is the text that appears when a user requests help about this program
<help>

 -- imgboot --

The program launched at first system boot.

Usage:
  imgboot [-T]

This program is the default 'first boot' program on Visopsys floppy or
CD-ROM image files that asks if you want to 'install' or 'run now'.

</help>
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/font.h>
#include <sys/vsh.h>

#define LOGINPROGRAM    "/programs/login"
#define INSTALLPROGRAM  "/programs/install"

static int processId = 0;
static int readOnly = 1;
static int haveInstall = 0;
static int passwordSet = 0;
static char *titleString     = "Copyright (C) 1998-2011 J. Andrew McLaughlin";
static char *gplString       =
"  This program is free software; you can redistribute it and/or modify it  \n"
"  under the terms of the GNU General Public License as published by the\n"
"  Free Software Foundation; either version 2 of the License, or (at your\n"
"  option) any later version.\n\n"
"  This program is distributed in the hope that it will be useful, but\n"
"  WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See\n"
"  the file /system/COPYING.txt for more details.";
static char *rebootQuestion  = "Would you like to reboot now?";
static char *adminString     = "Using the administrator account 'admin'.\n"
                               "There is no password set.";

// GUI stuff
static int graphics = 0;
static objectKey window = NULL;
static objectKey instButton = NULL;
static objectKey contButton = NULL;
static objectKey goAwayCheckbox = NULL;


__attribute__((format(printf, 1, 2)))
static void error(const char *format, ...)
{
  // Generic error message code for either text or graphics modes
  
  va_list list;
  char output[MAXSTRINGLENGTH];
  
  va_start(list, format);
  vsnprintf(output, MAXSTRINGLENGTH, format, list);
  va_end(list);

  if (graphics)
    windowNewErrorDialog(window, "Error", output);
  else
    printf("\n\nERROR: %s\n\n", output);
}


__attribute__((format(printf, 2, 3))) __attribute__((noreturn))
static void quit(int status, const char *message, ...)
{
  // Shut everything down

  va_list list;
  char output[MAXSTRINGLENGTH];
  
  if (message)
    {
      va_start(list, message);
      vsnprintf(output, MAXSTRINGLENGTH, message, list);
      va_end(list);
    }

  if (graphics)
    windowGuiStop();

  if (status < 0)
    error("%s  Quitting.", output);

  if (graphics && window)
    windowDestroy(window);

  errno = status;

  exit(status);
}


static int rebootNow(void)
{
  int response = 0;
  char character;

  if (graphics)
    {
      response =
	windowNewChoiceDialog(window, "Reboot?", rebootQuestion,
			      (char *[]) { "Reboot", "Continue" }, 2, 0);
      if (response == 0)
	return (1);
      else
	return (0);
    }

  else
    {
      printf("\n%s (y/n): ", rebootQuestion);
      textInputSetEcho(0);

      while(1)
	{
	  character = getchar();
	  if (errno)
	    {
	      // Eek.  We can't get input.  Quit.
	      textInputSetEcho(1);
	      return (0);
	    }
      
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


static void doEject(void)
{
  static disk sysDisk;

  bzero(&sysDisk, sizeof(disk));

  if (fileGetDisk("/", &sysDisk) >= 0)
    if (sysDisk.type & DISKTYPE_CDROM)
      if (diskSetLockState(sysDisk.name, 0) >= 0)
	{
	  if (diskSetDoorState(sysDisk.name, 1) < 0)
	    // Try a second time.  Sometimes 2 attempts seems to help.
	    diskSetDoorState(sysDisk.name, 1);
	}
}


static int runLogin(void)
{
  int pid = 0;

  if (!passwordSet)
    pid = loaderLoadProgram(LOGINPROGRAM " -f admin", 0);
  else
    pid = loaderLoadProgram(LOGINPROGRAM, 0);

  if (!graphics)
    // Give the login program a copy of the I/O streams
    multitaskerDuplicateIO(processId, pid, 0);

  loaderExecProgram(pid, 0);

  return (pid);
}


static void eventHandler(objectKey key, windowEvent *event)
{
  // Check for the 'Install' button
  if ((key == instButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      // Stop the GUI here and run the install program
      windowSetVisible(window, 0);
      loaderLoadAndExec(INSTALLPROGRAM, 0, 1);
      if (rebootNow())
	{
	  doEject();
	  shutdown(1, 1);
	}
      else
	{
	  if (runLogin() >= 0)
	    windowGuiStop();
	}
    }

  // Check for the 'Run' button
  else if ((key == contButton) && (event->type == EVENT_MOUSE_LEFTUP))
    {
      // Stop the GUI here and run the login program
      if (runLogin() >= 0)
	windowGuiStop();
    }
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  char versionString[32];
  char title[80];
  componentParameters params;

  getVersion(versionString, 32);
  sprintf(title, "Welcome to %s", versionString);

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, title);
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
  windowNewTextLabel(window, titleString, &params);

  params.gridY += 1;
  if (fileFind(FONT_SYSDIR "/arial-bold-10.vbf", NULL) >= 0)
    fontLoad("arial-bold-10.vbf", "arial-bold-10", &(params.font), 0);
  windowNewTextLabel(window, gplString, &params);

  params.orientationX = orient_center;
  params.font = NULL;
  if (haveInstall)
    {
      params.gridY += 1;
      windowNewTextLabel(window, "Would you like to install Visopsys?\n"
			 "(Choose continue to skip installing)", &params);
    }

  params.gridY += 1;
  params.flags |= (WINDOW_COMPFLAG_FIXEDWIDTH | WINDOW_COMPFLAG_FIXEDHEIGHT);
  if (haveInstall)
    {
      params.gridWidth = 1;
      params.orientationX = orient_right;
      instButton = windowNewButton(window, "Install", NULL, &params);
      windowRegisterEventHandler(instButton, &eventHandler);

      params.gridX += 1;
      params.orientationX = orient_left;
    }

  contButton = windowNewButton(window, "Continue", NULL, &params);
  windowRegisterEventHandler(contButton, &eventHandler);
  windowComponentFocus(contButton);

  params.gridX = 0;
  params.gridY += 1;
  params.gridWidth = 2;
  params.padBottom = 5;
  params.orientationX = orient_center;

  goAwayCheckbox =
    windowNewCheckbox(window, "Don't ask me this again", &params);
  if (readOnly)
    windowComponentSetEnabled(goAwayCheckbox, 0);

  // No minimize or close buttons on the window
  windowRemoveMinimizeButton(window);
  windowRemoveCloseButton(window);

  // Go
  windowSetVisible(window, 1);
}


static inline void changeStartProgram(void)
{
  configSet("/system/config/kernel.conf", "start.program", LOGINPROGRAM);
}


__attribute__((noreturn))
int main(int argc, char *argv[])
{
  disk sysDisk;
  int numOptions = 0;
  int defOption = 0;
  char *instOption = "o Install                    ";
  char *contOption = "o Continue                   ";
  char *naskOption = "o Always continue (never ask)";
  char *optionStrings[3] = { NULL, NULL, NULL };
  int selected = 0;

  processId = multitaskerGetCurrentProcessId();

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  // Check privilege level
  if (multitaskerGetProcessPrivilege(processId) != 0)
    quit(ERR_PERMISSION, "This program can only be run as a privileged user."
	 "\n(Try logging in as user \"admin\").");

  if (getopt(argc, argv, "T") == 'T')
    // Force text mode
    graphics = 0;

  // Find out whether we are currently running on a read-only filesystem
  bzero(&sysDisk, sizeof(disk));
  if (!fileGetDisk("/system", &sysDisk))
    readOnly = sysDisk.readOnly;

  // Find out whether we have an install program.
  if (fileFind(INSTALLPROGRAM, NULL) >= 0)
    haveInstall = 1;

  // Is there a password on the administrator account?
  if (userAuthenticate("admin", "") < 0)
    passwordSet = 1;

  if (graphics)
    {
      constructWindow();
      windowGuiRun();

      // If the user selected the 'go away' checkbox, change the start
      // program in the kernel's config file.
      windowComponentGetSelected(goAwayCheckbox, &selected);
      if (selected)
	{
	  changeStartProgram();

	  windowSetVisible(window, 0);
	  
	  if (!passwordSet)
	    // Tell the user about the admin account
	    windowNewInfoDialog(window, "Administrator account", adminString);
	}
    }
  else
    {
    restart:

      // Print title message, and ask whether to install or run
      printf("\n%s\n", gplString);

      numOptions = 0;

      if (haveInstall)
	{
	  optionStrings[numOptions] = instOption;
	  defOption = numOptions;
	  numOptions += 1;
	}

      optionStrings[numOptions] = contOption;
      defOption = numOptions;
      numOptions += 1;

      if (!readOnly)
	{
	  optionStrings[numOptions] = naskOption;
	  numOptions += 1;
	}

      if (numOptions > 1)
	selected = vshCursorMenu("\nPlease select from the following options",
				 optionStrings, numOptions, defOption);
      else
	selected = defOption;

      if (selected < 0)
	{
	  doEject();
	  shutdown(1, 1);
	}

      else if (optionStrings[selected] == instOption)
	{
	  // Install
	  loaderLoadAndExec(INSTALLPROGRAM, 0, 1);
	  if (rebootNow())
	    {
	      doEject();
	      shutdown(1, 1);
	    }
	  else
	    {
	      if (runLogin() < 0)
		goto restart;
	    }
	}
      else
	{
	  if (optionStrings[selected] == naskOption)
	    {
	      changeStartProgram();
	      if (!passwordSet)
		// Tell the user about the admin account
		printf("\n%s\n", adminString);
	    }

	  if (runLogin() < 0)
	    goto restart;
	}
    }

  quit(0, NULL);
}
