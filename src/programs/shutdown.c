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
//  shutdown.c
//

// This is the UNIX-style command for shutting down the system

/* This is the text that appears when a user requests help about this program
<help>

 -- shutdown --

A command for shutting down (and/or rebooting) the computer.

Usage:
  shutdown [-T] [-e] [-f] [-r]

This command causes the system to shut down.  If the (optional) '-e'
parameter is supplied, then 'shutdown' will attempt to eject the boot
medium (if applicable, such as a CD-ROM).  If the (optional) '-f' parameter
is supplied, then it will attempt to ignore errors and shut down
regardless.  Use this flag with caution if filesystems do not appear to be
unmounting correctly; you may need to back up unsaved data before shutting
down.  If the (optional) '-r' parameter is supplied, then it will reboot
the computer rather than simply halting.

In graphics mode, the program prompts the user to select 'reboot' or
'shut down'.  If the system is currently booted from a CD-ROM, the dialog
box also offers a checkbox to eject the disc.  If the '-r' parameter is
used, the dialog will not appear and the computer will reboot.

Options:
-T  : Force text mode operation.
-e  : Eject the boot medium.
-f  : Force shutdown and ignore errors.
-r  : Reboot.

</help>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/window.h>

#define EJECT_MESS     "Ejecting, please wait..."
#define NOUNLOCK_MESS  "Unable to unlock the media door"
#define NOEJECT_MESS   "Can't seem to eject.  Try pushing\nthe 'eject' " \
                       "button now."

static int graphics = 0;
static int eject = 0;
static int reboot = 0;
static objectKey window = NULL;
static objectKey rebootIcon = NULL;
static objectKey shutdownIcon = NULL;
static objectKey ejectCheckbox = NULL;
static disk sysDisk;


static void doEject(void)
{
  int status = 0;
  objectKey bannerDialog = NULL;

  if (graphics)
    bannerDialog = windowNewBannerDialog(window, "Ejecting", EJECT_MESS);
  else
    printf("\n"EJECT_MESS" ");

  if (diskSetLockState(sysDisk.name, 0) < 0)
    {
      if (graphics)
	{
	  if (bannerDialog)
	    windowDestroy(bannerDialog);
      
	  windowNewErrorDialog(window, "Error", NOUNLOCK_MESS);
	}
      else
	printf("\n\n"NOUNLOCK_MESS"\n");
    }
  else
    {
      status = diskSetDoorState(sysDisk.name, 1);
      if (status < 0)
	{
	  // Try a second time.  Sometimes 2 attempts seems to help.
	  status = diskSetDoorState(sysDisk.name, 1);

	  if (status < 0)
	    {
	      if (graphics)
		{
		  if (bannerDialog)
		    windowDestroy(bannerDialog);

		  windowNewInfoDialog(window, "Hmm", NOEJECT_MESS);
		}
	      else
		printf("\n\n"NOEJECT_MESS"\n");
	    }
	}
      else
	{
	  if (graphics)
	    {
	      if (bannerDialog)
		windowDestroy(bannerDialog);
	    }
	  else
	    printf("\n");
	}
    }
}


static void eventHandler(objectKey key, windowEvent *event)
{
  int selected = 0;

  // Check for the window being closed by a GUI event.
  if ((key == window) && (event->type == EVENT_WINDOW_CLOSE))
    {
      windowGuiStop();
      windowDestroy(window);
      exit(0);
    }

  if (((key == rebootIcon) || (key == shutdownIcon)) &&
      (event->type == EVENT_MOUSE_LEFTUP))
    {
      windowGuiStop();

      if (ejectCheckbox)
	{
	  windowComponentGetSelected(ejectCheckbox, &selected);

	  if (eject || (selected == 1))
	    doEject();
	}

      windowDestroy(window);

      shutdown((key == rebootIcon), 0);
      while(1);
    }
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;
  image iconImage;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(multitaskerGetCurrentProcessId(), "Shut down");
  if (window == NULL)
    return;

  bzero(&params, sizeof(componentParameters));
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padTop = 20;
  params.padBottom = 20;
  params.padLeft = 20;
  params.padRight = 20;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;

  // Create a reboot icon
  bzero(&iconImage, sizeof(image));
  if (!imageLoad("/system/icons/rebticon.bmp", 0, 0, &iconImage))
    {
      rebootIcon = windowNewIcon(window, &iconImage, "Reboot", &params);
      windowRegisterEventHandler(rebootIcon, &eventHandler);
      memoryRelease(iconImage.data);
    }

  // Create a shut down icon
  bzero(&iconImage, sizeof(image));
  if (!imageLoad("/system/icons/shuticon.bmp", 0, 0, &iconImage))
    {
      params.gridX = 1;
      shutdownIcon =
	windowNewIcon(window, &iconImage, "Shut down", &params);
      windowRegisterEventHandler(shutdownIcon, &eventHandler);
      memoryRelease(iconImage.data);
    }

  // Find out whether we are currently running from a CD-ROM
  if (sysDisk.flags & DISKFLAG_CDROM)
    {
      // Yes.  Make an 'eject cd' checkbox.
      params.gridX = 0;
      params.gridY = 1;
      params.gridWidth = 2;
      params.padTop = 0;
      params.flags |= WINDOW_COMPFLAG_CUSTOMFOREGROUND;
      params.foreground.red = 255;
      params.foreground.green = 255;
      params.foreground.blue = 255;
      params.flags |= WINDOW_COMPFLAG_CUSTOMBACKGROUND;
      params.background.red = 40;
      params.background.green = 93;
      params.background.blue = 171;
      ejectCheckbox = windowNewCheckbox(window, "Eject CD-ROM", &params);
    }

  // Register an event handler to catch window close events
  windowRegisterEventHandler(window, &eventHandler);

  windowSetColors(window, &((color){ 171, 93, 40}));
  windowSetVisible(window, 1);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char opt;
  int force = 0;

  // Are graphics enabled?
  graphics = graphicsAreEnabled();

  while (strchr("Tefr", (opt = getopt(argc, argv, "Tefr"))))
    {
      // Force text mode?
      if (opt == 'T')
	graphics = 0;

      // Eject boot media?
      if (opt == 'e')
	eject = 1;

      // Shut down forcefully?
      if (opt == 'f')
	force = 1;

      // Reboot?
      if (opt == 'r')
	{
	  graphics = 0;
	  reboot = 1;
	}
    }

  // Get the system disk
  bzero(&sysDisk, sizeof(disk));
  fileGetDisk("/", &sysDisk);

  // If graphics are enabled, show a query dialog asking whether to shut
  // down or reboot
  if (graphics)
    {
      constructWindow();

      // Run the GUI
      windowGuiRun();
    }
  else
    {
      if (eject && (sysDisk.flags & DISKFLAG_CDROM))
	doEject();

      // There's a nice system function for doing this.
      status = shutdown(reboot, force);
      if (status < 0)
	{
	  if (!force)
	    printf("Use \"%s -f\" to force.\n", argv[0]);
	  return (status);
	}

      // Wait for death
      while(1);
    }

  return (status = 0);
}
