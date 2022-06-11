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
//  imgboot.c
//

// This is a program to be launched first when booting from distribution
// images, such as CD-ROM ISOs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/api.h>

#define LOGINPROGRAM    "/programs/login"
#define INSTALLPROGRAM  "/programs/install"

static int processId = 0;
static int readOnly = 1;
static char *titleString     = "Copyright (C) 1998-2004 J. Andrew McLaughlin";
static char *gplString       =
"  This program is free software; you can redistribute it and/or modify it  \n"
"  under the terms of the GNU General Public License as published by the\n"
"  Free Software Foundation; either version 2 of the License, or (at your\n"
"  option) any later version.\n\n"
"  This program is distributed in the hope that it will be useful, but\n"
"  WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file\n"
"  COPYING.TXT for more details.";
static char *rebootQuestion  = "Would you like to reboot now?";
static char *adminString     = "Using the administrator account 'admin'.\n"
                               "There is no password set.";

// GUI stuff
static int graphics = 0;
static objectKey window = NULL;
static objectKey installButton = NULL;
static objectKey runButton = NULL;
static objectKey goAwayCheckbox = NULL;


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
  
  if (message)
    {
      va_start(list, message);
      _expandFormatString(output, message, list);
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
  char character;

  if (graphics)
    return (windowNewQueryDialog(window, "Reboot?", rebootQuestion));

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


static void eventHandler(objectKey key, windowEvent *event)
{
  // Check for the 'Install' button
  if ((key == installButton) && (event->type == EVENT_MOUSE_UP))
    {
      // Stop the GUI here and run the install program
      windowSetVisible(window, 0);
      loaderLoadAndExec(INSTALLPROGRAM, 0, 0, NULL, 1);
      if (rebootNow())
	shutdown(1, 1);
      else
	{
	  loaderLoadAndExec(LOGINPROGRAM, 0, 2, (char *[]){"-f", "admin"}, 0);
	  windowGuiStop();
	}
    }

  // Check for the 'Run' button
  else if ((key == runButton) && (event->type == EVENT_MOUSE_UP))
    {
      // Stop the GUI here and run the login program
      loaderLoadAndExec(LOGINPROGRAM, 0, 2, (char *[]){"-f", "admin"}, 0);
      windowGuiStop();
    }
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;
  objectKey font = NULL;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "Welcome to Visopsys");
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
  windowNewTextLabel(window, font, titleString, &params);

  params.gridY = 1;
  fontLoad("arial-bold-10.bmp", "arial-bold-10", &font);
  windowNewTextLabel(window, font, gplString, &params);

  params.gridY = 2;
  params.orientationX = orient_center;
  windowNewTextLabel(window, NULL, "Would you like to install "
				 "Visopsys, or just run it now?", &params);

  params.gridY = 3;
  params.gridWidth = 1;
  params.padBottom = 5;
  params.orientationX = orient_right;
  installButton = windowNewButton(window, "Install", NULL, &params);
  windowRegisterEventHandler(installButton, &eventHandler);

  params.gridX = 1;
  params.orientationX = orient_left;
  runButton = windowNewButton(window, "Run now", NULL, &params);
  windowRegisterEventHandler(runButton, &eventHandler);

  params.gridX = 0;
  params.gridY = 4;
  params.gridWidth = 2;
  params.orientationX = orient_center;
  goAwayCheckbox =
    windowNewCheckbox(window, NULL, "Don't ask me this again", &params);
  if (readOnly)
    windowComponentSetEnabled(goAwayCheckbox, 0);

  // No close button on the window
  windowSetHasCloseButton(window, 0);

  // Go
  windowSetVisible(window, 1);
}


static void changeStartProgram(void)
{
  variableList *kernelConf = configurationReader("/system/kernel.conf");
  if (kernelConf)
    {
      variableListSet(kernelConf, "start.program", "/programs/login");
      configurationWriter("/system/kernel.conf", kernelConf);
      free(kernelConf);
    }
}


int main(int argc, char *argv[])
{
  int status = 0;
  char bootDisk[DISK_MAX_NAMELENGTH];
  int foregroundColor = textGetForeground();
  int backgroundColor = textGetBackground();
  int options = 3;
  int selectedOption = 0;
  char character;
  int count;

  processId = multitaskerGetCurrentProcessId();

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  // Check privilege level
  if (multitaskerGetProcessPrivilege(processId) != 0)
    quit(ERR_PERMISSION, "This program can only be run as a privileged user."
	 "\n(Try logging in as user \"admin\").");

  // Find out whether we are currently running on a read-only filesystem
  if (!diskGetBoot(bootDisk))
    readOnly = diskGetReadOnly(bootDisk);

  if (graphics)
    {
      constructWindow();
      windowGuiRun();

      // If the user selected (or, actually, didn't deselect) the 'go away'
      // checkbox, change the start program in the kernel's config file.
      if (windowComponentGetSelected(goAwayCheckbox))
	{
	  changeStartProgram();

	  windowSetVisible(window, 0);

	  // Tell the user about the admin account
	  windowNewInfoDialog(window, "Administrator account", adminString);
	}
    }
  else
    {
      // Print title message, and ask whether to install or run
      printf("\n%s\n", gplString);

      printf("\nPlease select from the following options\n");

      int column = textGetColumn();
      int row = textGetRow();

      if (readOnly)
	options -= 1;

      while(1)
	{
	  textSetColumn(column);
	  textSetRow(row);

	  for (count = 0; count < options; count ++)
	    {
	      if (selectedOption == count)
		{
		  // Reverse the colors
		  textSetForeground(backgroundColor);
		  textSetBackground(foregroundColor);
		}

	      if (count == 0)
		printf(" o Install               \n");
	      else if (count == 1)
		printf(" o Run now               \n");
	      else if (count == 2)
		printf(" o Always run             \n"
		       "   (never ask to install) ");

	      textSetForeground(foregroundColor);
	      textSetBackground(backgroundColor);
	    }

	  textSetCursor(0);
	  textInputSetEcho(0);
	  character = getchar();
      
	  if (character == (unsigned char) 17)
	    {
	      if (selectedOption > 0)
		// Cursor up.
		selectedOption -= 1;
	    }

	  else if (character == (unsigned char) 20)
	    {
	      // Cursor down.
	      if (selectedOption < (options - 1))
		selectedOption += 1;
	    }

	  else if (character == (unsigned char) 10)
	    {
	      // Enter
	      textSetCursor(1);
	      textInputSetEcho(1);
	      printf("\n");

	      if (selectedOption == 0)
		{
		  // Install
		  loaderLoadAndExec(INSTALLPROGRAM, 0, 0, NULL, 1);
		  if (rebootNow())
		    shutdown(1, 1);
		  else
		    {
		      int pid = loaderLoadProgram(LOGINPROGRAM, 0, 2,
						  (char *[]){"-f", "admin"});
		      // Give the login program a copy of the I/O streams
		      multitaskerDuplicateIO(processId, pid, 0);
		      loaderExecProgram(pid, 0);
		      break;
		    }
		}

	      else
		{
		  if (selectedOption == 2)
		    {
		      changeStartProgram();
		      printf("\n%s\n", adminString);
		    }

		  int pid = loaderLoadProgram(LOGINPROGRAM, 0, 2,
					      (char *[]){"-f", "admin"});
		  // Give the login program a copy of the I/O streams
		  multitaskerDuplicateIO(processId, pid, 0);
		  loaderExecProgram(pid, 0);
		  break;
		}
	    }
	}
    }

  quit(0, "");

  // Make the compiler happy
  return (status);
}
