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
//  screenshot.c
//

// Saves a screen shot, either with the default filename 'scrnshot.bmp' in
// the current directory, or with the supplied filename

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/api.h>


int main(int argc, char *argv[])
{
  int status = 0;
  char filename[1024];
  int count;

  // Make sure none of our args are NULL
  for (count = 0; count < argc; count ++)
    if (argv[count] == NULL)
      return (status = ERR_NULLPARAMETER);

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  // Did the user supply a filename?
  if (argc > 1)
    {
      strncpy(filename, argv[1], 1024);
      filename[1023] = '\0';
    }
  else
    {
      multitaskerGetCurrentDirectory(filename, 1024);
      if (filename[strlen(filename) - 1] != '/')
	strcat(filename, "/");
      strcat(filename, "scrnshot.bmp");
    }

  status = windowManagerSaveScreenShot(filename);
  if (status < 0)
    {
      errno = status;
      perror(argv[0]);
    }

  return (status);
}