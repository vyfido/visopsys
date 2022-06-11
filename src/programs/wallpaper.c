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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/window.h>
#include <sys/api.h>


int main(int argc, char *argv[])
{
  int status = 0;
  char tmpFilename[128];
  char filename[MAX_PATH_NAME_LENGTH];
  int processId = 0;
  objectKey window = NULL;

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  // We need our process ID to create the windows
  processId = multitaskerGetCurrentProcessId();

  if (argc < 2)
    {
      // The user did not specify a file.  We will prompt them.
      status =
	windowNewFileDialog(window, "Enter filename", "Please enter the "
			    "background image\nfile name:", tmpFilename, 128);
      if (status != 1)
	{
	  if (status == 0)
	    return (status);

	  printf("No filename specified\n");
	  errno = status;
	  perror(argv[0]);
	  return (status);
	}
    }
  
  else
    strcpy(tmpFilename, argv[1]);

  // Make sure the file name is complete
  vshMakeAbsolutePath(tmpFilename, filename);

  status = windowTileBackground(filename);

  errno = status;
  return (status);
}
