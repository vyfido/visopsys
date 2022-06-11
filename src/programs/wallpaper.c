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
//  wallpaper.c
//

// Calls the kernel's window manager to change the background image

/* This is the text that appears when a user requests help about this program
<help>

 -- wallpaper --

Set the background wallpaper image.

Usage:
  wallpaper [image_file]

(Only available in graphics mode)

This command will set the background wallpaper image from the (optional)
image file name parameter or, if no image file name is supplied, the program
will prompt the user.

Currently, only (uncompressed) 8-bit and 24-bit bitmap formats are supported. 

</help>
*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/window.h>
#include <sys/api.h>


int main(int argc, char *argv[])
{
  int status = 0;
  int processId = 0;
  char rawFileName[MAX_PATH_NAME_LENGTH];
  char fullFileName[MAX_PATH_NAME_LENGTH];
  file tmpFile;

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  bzero(rawFileName, MAX_PATH_NAME_LENGTH);
  bzero(fullFileName, MAX_PATH_NAME_LENGTH);
  bzero(&tmpFile, sizeof(file));

  // We need our process ID to create the windows
  processId = multitaskerGetCurrentProcessId();

  if (argc < 2)
    {
      // The user did not specify a file.  We will prompt them.

      status =
	windowNewFileDialog(NULL, "Enter filename", "Please enter the "
			    "background image\nfile name:",
			    "/system/wallpaper", rawFileName,
			    MAX_PATH_NAME_LENGTH);
      if (status != 1)
	{
	  if (status == 0)
	    return (status);

	  printf("No filename specified\n");
	  return (errno = status);
	}
    }
  
  else
    strncpy(rawFileName, argv[1], MAX_PATH_NAME_LENGTH);

  // Make sure the file name is complete
  vshMakeAbsolutePath(rawFileName, fullFileName);

  status = fileFind(fullFileName, &tmpFile);
  if (status < 0)
    {
      printf("File not found\n");
      return (errno = status);
    }

  status = windowTileBackground(fullFileName);

  return (errno = status);
}
