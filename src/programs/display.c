//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
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
//  display.c
//

// This command will display an image file in a new window.

#include <stdio.h>
#include <string.h>
#include <sys/api.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <image file>\n", name);
  return;
}


static void makeAbsolutePath(const char *orig, char *new)
{
  char cwd[MAX_PATH_LENGTH];

  if ((orig[0] != '/') && (orig[0] != '\\'))
    {
      multitaskerGetCurrentDirectory(cwd, MAX_PATH_LENGTH);

      strcpy(new, cwd);

      if ((new[strlen(new) - 1] != '/') &&
	  (new[strlen(new) - 1] != '\\'))
	strncat(new, "/", 1);

      strcat(new, orig);
    }
  else
    strcpy(new, orig);

  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char filename[128];
  int processId = 0;
  objectKey window = NULL;
  componentParameters params;
  image showImage;
  objectKey imageComponent = NULL;

  if (argc < 2)
    {
      usage(argv[0]);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Make sure none of our arguments are NULL
  if ((argv[0] == NULL) || (argv[1] == NULL))
    return (status = ERR_NULLPARAMETER);

  // Turn it into an absolute pathname
  makeAbsolutePath(argv[1], filename);

  // Try to load the image file
  status = imageLoadBmp(filename, &showImage);

  if (status < 0)
    {
      printf("Unable to load image \"%s\"\n", filename);
      errno = status;
      perror(argv[0]);
      return (status);
    }
  
  // We need our process ID to create the window
  processId = multitaskerGetCurrentProcessId();

  // Create a new window, with small, arbitrary size and location
  window = windowManagerNewWindow(processId, argv[1], 100, 100, 100, 100);

  imageComponent = windowNewImageComponent(window, &showImage);

  // Put it in the client area of the window
  params.gridX = 0;
  params.gridY = 0;
  params.gridWidth = 1;
  params.gridHeight = 1;
  params.padLeft = 0;
  params.padRight = 0;
  params.padTop = 0;
  params.padBottom = 0;
  params.orientationX = orient_center;
  params.orientationY = orient_middle;
  params.hasBorder = 0;
  params.useDefaultForeground = 1;
  params.useDefaultBackground = 1;
  windowAddClientComponent(window, imageComponent, &params);

  windowLayout(window);

  // Autosize the window to fit our text area
  windowAutoSize(window);

  // Go live.
  status = windowSetVisible(window, 1);

  // We don't want to exit or else our window becomes dead
  while(1)
    multitaskerWait(200);

  // Return success
  errno = 0;
  return (status);
}
