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
//  loadfont.c
//

// Calls the kernel to switch to the named font.

#include <stdio.h>
#include <string.h>
#include <sys/vsh.h>
#include <sys/api.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <font file> <font name>\n", name);
  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  objectKey font;

  // Only work in graphics mode
  if (!graphicsAreEnabled())
    {
      printf("\nThe \"%s\" command only works in graphics mode\n", argv[0]);
      errno = ERR_NOTINITIALIZED;
      return (status = errno);
    }

  if (argc != 3)
    {
      usage((argc > 0)? argv[0] : "loadfont");
      errno = ERR_ARGUMENTCOUNT;
      return (status = errno);
    }

  // Make sure the file name is complete
  vshMakeAbsolutePath(argv[1], fileName);

  // Call the kernel to load the font
  status = fontLoad(fileName, argv[2], &font);

  if (status < 0)
    {
      errno = status;
      perror(argv[0]);
      return (status);
    }

  // Switch to it
  fontSetDefault(argv[2]);

  errno = status;
  return (status);
}
