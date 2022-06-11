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

Currently, bitmap (.bmp) and JPEG (.jpg) image formats are supported. 

</help>
*/

#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <sys/vsh.h>
#include <sys/window.h>
#include <sys/api.h>

#define _(string) gettext(string)


int main(int argc, char *argv[])
{
  int status = 0;
  char *language = "";
  int processId = 0;
  char fileName[MAX_PATH_NAME_LENGTH];

#ifdef BUILDLANG
  language=BUILDLANG;
#endif
  setlocale(LC_ALL, language);
  textdomain("wallpaper");

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf(_("\nThe \"%s\" command only works in graphics mode\n"), argv[0]);
      return (status = ERR_NOTINITIALIZED);
    }

  bzero(fileName, MAX_PATH_NAME_LENGTH);

  // We need our process ID to create the windows
  processId = multitaskerGetCurrentProcessId();

  if (argc < 2)
    {
      // The user did not specify a file.  We will prompt them.
      status =
	windowNewFileDialog(NULL, _("Enter filename"),
			    _("Please choose the background image:"),
			    "/system/wallpaper", fileName,
			    MAX_PATH_NAME_LENGTH, 1);
      if (status != 1)
	{
	  if (status == 0)
	    return (status);

	  printf("%s", _("No filename specified\n"));
	  return (status);
	}
    }

  else
    strncpy(fileName, argv[1], MAX_PATH_NAME_LENGTH);

  if (strncmp(fileName, "none", MAX_PATH_NAME_LENGTH))
    {
      status = fileFind(fileName, NULL);
      if (status < 0)
	{
	  printf("%s", _("File not found\n"));
	  return (status);
	}

      status = windowTileBackground(fileName);
    }

  else
    status = windowTileBackground(NULL);

  return (status);
}
