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
//  cp.c
//

// Yup, it's the UNIX-style command for copying files

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/vsh.h>


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
      usage((argc > 0)? argv[0] : "cp");
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Make sure none of our filenames are NULL
  for (count = 0; count < argc; count ++)
    if (argv[count] == NULL)
      return (status = ERR_NULLPARAMETER);

  // If any of the arguments are RELATIVE pathnames, we should
  // insert the pwd before it
  vshMakeAbsolutePath(argv[1], srcFileName);
  vshMakeAbsolutePath(argv[2], destFileName);
  vshCopyFile(srcFileName, destFileName);

  // Return success
  return (status = 0);
}
