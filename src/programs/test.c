//
//  Visopsys
//  Copyright (C) 1998-2006 J. Andrew McLaughlin
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
//  test.c
//

// This is a pointless program just for testing features.

#include <stdio.h>
#include <sys/api.h>
#include <sys/vsh.h>
#include <sys/window.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define LOGINPROGRAM    "/programs/login"
#define INSTALLPROGRAM  "/programs/install"

static int processId = 0;
static int readOnly = 1;
static char *titleString     = "Copyright (C) 1998-2006 J. Andrew McLaughlin";
static char *gplString       =
"  This program is free software; you can redistribute it and/or modify it  \n"
"  under the terms of the GNU General Public License as published by the\n"
"  Free Software Foundation; either version 2 of the License, or (at your\n"
"  option) any later version.\n\n"
"  This program is distributed in the hope that it will be useful, but\n"
"  WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See\n"
"  the file /system/COPYING.txt for more details.";

// GUI stuff
static int graphics = 0;
static objectKey window = NULL;
static objectKey installButton = NULL;
static objectKey runButton = NULL;
static objectKey goAwayCheckbox = NULL;

static void eventHandler(objectKey key, windowEvent *event)
{
  if (key && event)
    {
    }
}


static void constructWindow(void)
{
  // If we are in graphics mode, make a window rather than operating on the
  // command line.

  componentParameters params;

  // Create a new window, with small, arbitrary size and location
  window = windowNew(processId, "Welcome to Visopsys");

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
  windowNewTextLabel(window, titleString, &params);

  params.gridY = 1;
  fontLoad("/system/fonts/arial-bold-10.bmp", "arial-bold-10",
	   &(params.font), 0);
  windowNewTextLabel(window, gplString, &params);

  params.gridY = 2;
  params.orientationX = orient_center;
  params.font = NULL;
  windowNewTextLabel(window, "Would you like to install Visopsys, or just run "
  		     "it now?", &params);

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
    windowNewCheckbox(window, "Don't ask me this again", &params);
  if (readOnly)
    windowComponentSetEnabled(goAwayCheckbox, 0);

  // No minimize or close buttons on the window
  windowSetHasMinimizeButton(window, 0);
  windowSetHasCloseButton(window, 0);

  // Go
  windowSetVisible(window, 1);
}


int main(void)
{
  graphics = graphicsAreEnabled();

  printf("Hello world\n");
  vshFileList("/visopsys");

  //userLogin("admin", "");
 
  //printf("%d\n", *((int *) 0x8048a30));
  //*((int *) 0x8048a30) = 99;
  //printf("%d\n", *((int *) 0x8048a30));

  if (graphics)
    {
      //windowLogin("admin");
      constructWindow();
      windowNewInfoDialog(NULL, "Excuse me...", "OK!");
    }

  return 0;
}
