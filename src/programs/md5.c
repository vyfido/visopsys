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
//  md5.c
//

// Uses the kernel's built-in MD5 crypto to create a digest string

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/api.h>


int main(int argc, char *argv[])
{
  int status = 0;
  unsigned outputSize = 0;
  unsigned char *output;
  int count1, count2;

  // If no parameter is supplied, use an empty string.
  if (argc < 2)
    {
      argv[1] = "";
      argc = 2;
    }

  for (count1 = 1; count1 < argc; count1 ++)
    {
      // Get a buffer to hold the digest result.  This may  be bigger than it
      // needs to be but that's okay
      outputSize = (((strlen(argv[count1]) / 56) + 1) * 4);

      // Get memory
      output = malloc(outputSize);
      if (output == NULL)
	{
	  errno = ERR_MEMORY;
	  perror(argv[0]);
	  return (status = errno);
	}

      status = encryptMD5(argv[count1], output);
      if (status < 0)
	{
	  errno = status;
	  perror(argv[0]);
	  free(output);
	  return (status);
	}

      for (count2 = 0; count2 < status; count2 ++)
	printf("%02x", output[count2]);
      printf("\n");

      free(output);
    }

  return (status = 0);
}
