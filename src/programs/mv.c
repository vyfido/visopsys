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
//  mv.c
//

// Yup, it's the UNIX-style command for renaming files

/* This is the text that appears when a user requests help about this program
<help>

 -- mv --

Move (rename) files.

Synonym:
  move, ren, rename

Usage:
  mv <item1> [item2] [...] <new_name | detination_directory>

This command will move one or more files or directories.  If one item is
being moved, then the last argument can be either the new name, or else can
be a destination directory -- in which case the moved file will retain the
same name as before.  If multiple items are being moved, then the last
argument must be a directory name and all moved items will retain the same
names.

</help>
*/

#include <stdio.h>
#include <errno.h>
#include <sys/vsh.h>
#include <sys/api.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <source 1> [source 2] ... <destination>\n",
	 name);
  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  char srcFileName[MAX_PATH_NAME_LENGTH];
  char destFileName[MAX_PATH_NAME_LENGTH];
  int count;

  // There need to be at least a source and destination file
  if (argc < 3)
    {
      usage(argv[0]);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // If the dest filename is relative, we should fix it up
  vshMakeAbsolutePath(argv[argc - 1], destFileName);

  // Attempt to move the file(s)
  for (count = 1; count < (argc - 1); count ++)
    {
      // Likewise, fix up the src filename
      vshMakeAbsolutePath(argv[count], srcFileName);

      status = fileMove(srcFileName, destFileName);
      if (status < 0)
	{
	  errno = status;
	  perror(argv[0]);
	  return (status);
	}
    }

  // Return success
  return (status = 0);
}
