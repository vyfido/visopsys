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
//  mount.c
//

// This is the UNIX-style command for mounting filesystems

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/api.h>
#include <sys/vsh.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <mount point>\n", name);
  return;
}


int main(int argc, char *argv[])
{
  // Attempts to unmount the named filesystem from the named mount point

  int status = 0;
  char filesystem[MAX_PATH_LENGTH];
  
  if (argc < 2)
    {
      usage(argv[0]);
      return (status = ERR_ARGUMENTCOUNT);
    }

  // We allow relative pathnames
  vshMakeAbsolutePath(argv[1], filesystem);

  status = filesystemUnmount(filesystem);
  if (status < 0)
    {
      printf("Error unmounting %s\n", filesystem);
      errno = status;
      perror(argv[0]);
      return (status = errno);
    }
 
  // Finished
  return (status = 0);
}
