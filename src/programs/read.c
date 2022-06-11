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
//  read.c
//

// This is for debugging; just reads a file into memory.  It's 'cat' without
// any output.

#include <stdio.h>
#include <stdlib.h>
#include <sys/vsh.h>
#include <sys/api.h>


static void usage(char *name)
{
  printf("usage:\n");
  printf("%s <file1> [file2] [...]\n", name);
  return;
}


int main(int argc, char *argv[])
{
  int status = 0;
  int argNumber = 0;
  char fileName[MAX_PATH_NAME_LENGTH];
  file theFile;
  char *fileBuffer = NULL;
  int count;

  if (argc < 2)
    {
      usage((argc > 0)? argv[0] : "read");
      return (status = ERR_ARGUMENTCOUNT);
    }

  // Loop through all of our directory name arguments
  for (argNumber = 1; argNumber < argc; argNumber ++)
    {
      // Make sure the name isn't NULL
      if (argv[argNumber] == NULL)
	return (status = ERR_NULLPARAMETER);

      // Make sure the name is complete
      vshMakeAbsolutePath(argv[argNumber], fileName);

      // Initialize the file structure
      for (count = 0; count < sizeof(file); count ++)
	((char *) &theFile)[count] = NULL;

      // Call the "find file" routine to see if we can get the file
      status = fileFind(fileName, &theFile);
  
      if (status < 0)
	{
	  errno = status;
	  perror(argv[0]);
	  return (status);
	}

      // Make sure the file isn't empty.  We don't want to try reading
      // data from a nonexistent place on the disk.
      if (theFile.size == 0)
	// It is empty, so just return
	return status;

      // The file exists and is non-empty.  That's all we care about (we
      // don't care at this point, for example, whether it's a file or a
      // directory.  Read it into memory and print it on the screen.
  
      // Allocate a buffer to store the file contents in
      fileBuffer = malloc((theFile.blocks * theFile.blockSize) + 1);

      if (fileBuffer == NULL)
	{
	  errno = ERR_MEMORY;
	  perror(argv[0]);
	  return (status = ERR_MEMORY);
	}

      status = fileOpen(fileName, OPENMODE_READ, &theFile);

      if (status < 0)
	{
	  errno = status;
	  perror(argv[0]);
	  free(fileBuffer);
	  return (status);
	}

      status = fileRead(&theFile, 0, theFile.blocks, fileBuffer);

      if (status < 0)
	{
	  errno = status;
	  perror(argv[0]);
	  free(fileBuffer);
	  return (status);
	}

      // Free the memory
      free(fileBuffer);
    }

  // Return success
  return (status = 0);
}
