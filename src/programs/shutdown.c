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
//  shutdown.c
//

// This is the UNIX-style command for shutting down the system

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/api.h>
#include <sys/window.h>

static objectKey window = NULL;
static objectKey rebootIcon = NULL;
static objectKey shutdownIcon = NULL;
static objectKey ejectCheckbox = NULL;
static disk sysDisk;


static void eventHandler(objectKey key, windowEvent *event)
{
  int status = 0;
  objectKey bannerDialog = NULL;

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

      if (ejectCheckbox && (windowComponentGetSelected(ejectCheckbox) == 1))
	{
	  bannerDialog = windowNewBannerDialog(window, "Ejecting",
					       "Ejecting CD, please wait...");

	  if (diskSetLockState(sysDisk.name, 0) < 0)
	    {
	      if (bannerDialog)
		windowDestroy(bannerDialog);

	      windowNewErrorDialog(window, "Error", "Unable to unlock the "
				   "CD-ROM tray");
	    }
	  else
	    {
	      status = diskSetDoorState(sysDisk.name, 1);

	      if (bannerDialog)
		windowDestroy(bannerDialog);

	      if (status < 0)
		windowNewInfoDialog(window, "Hmm", "Can't seem to eject.  Try "
				    "pushing\nthe 'eject' button now.");
	    }
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
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;

  // Create a reboot icon
  bzero(&iconImage, sizeof(image));
  if (!imageLoadBmp("/system/icons/rebticon.bmp", &iconImage))
    {
      rebootIcon = windowNewIcon(window, &iconImage, "Reboot", &params);
      windowRegisterEventHandler(rebootIcon, &eventHandler);
      memoryRelease(iconImage.data);
    }

  // Create a shut down icon
  bzero(&iconImage, sizeof(image));
  if (!imageLoadBmp("/system/icons/shuticon.bmp", &iconImage))
    {
      params.gridX = 1;
      shutdownIcon =
	windowNewIcon(window, &iconImage, "Shut down", &params);
      windowRegisterEventHandler(shutdownIcon, &eventHandler);
      memoryRelease(iconImage.data);
    }

  // Find out whether we are currently running from a CD-ROM
  bzero(&sysDisk, sizeof(disk));
  if (!fileGetDisk("/", &sysDisk))
    {
      if ((sysDisk.type == idecdrom) || (sysDisk.type == scsicdrom))
	{
	  // Yes.  Make an 'eject cd' checkbox.
	  params.gridX = 0;
	  params.gridY = 1;
	  params.gridWidth = 2;
	  params.padTop = 0;
	  params.useDefaultForeground = 0;
	  params.foreground.red = 255;
	  params.foreground.green = 255;
	  params.foreground.blue = 255;
	  params.useDefaultBackground = 0;
	  params.background.red = 40;
	  params.background.green = 93;
	  params.background.blue = 171;
	  ejectCheckbox = windowNewCheckbox(window, "Eject CD-ROM", &params);
	}
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
  int force = 0;

  // Shut down forcefully?
  if (getopt(argc, argv, "f") == 'f')
    force = 1;

  // If graphics are enabled, show a query dialog asking whether to shut
  // down or reboot
  if (graphicsAreEnabled())
    {
      constructWindow();

      // Run the GUI
      windowGuiRun();
    }
  else
    {
      // There's a nice system function for doing this.
      status = shutdown(0, force);
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
